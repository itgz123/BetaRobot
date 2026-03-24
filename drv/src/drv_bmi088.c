/**
 * @file drv_bmi088.c
 * @brief BMI088 六轴 IMU 驱动实现
 *
 * @note 使用 DMA 非阻塞模式进行 SPI 数据读取
 * @note 片选由 DRV 层通过 GPIO 接口控制
 */

#include "drv_bmi088.h"

/*============================ 私有宏定义 ============================*/

#ifndef container_of
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

/*============================ 私有变量 ============================*/

/* 加速度计初始化配置表: {寄存器地址, 配置值, 错误码} */
static const uint8_t s_acc_init_table[][3] = {
    {BMI088_ACC_PWR_CTRL, BMI088_ACC_ENABLE, BMI088_ACC_PWR_CTRL_ERROR},
    {BMI088_ACC_PWR_CONF, BMI088_ACC_PWR_ACTIVE, BMI088_ACC_PWR_CONF_ERROR},
    {BMI088_ACC_CONF, BMI088_ACC_CONF_MUST_SET | BMI088_ACC_NORMAL | BMI088_ACC_800_HZ, BMI088_ACC_CONF_ERROR},
    {BMI088_ACC_RANGE, BMI088_ACC_RANGE_6G, BMI088_ACC_RANGE_ERROR},
    {BMI088_INT1_IO_CTRL, BMI088_INT1_IO_ENABLE | BMI088_INT1_GPIO_PP | BMI088_INT1_GPIO_LOW, BMI088_INT1_IO_CTRL_ERROR},
    {BMI088_INT_MAP_DATA, BMI088_INT1_DRDY_INTERRUPT, BMI088_INT_MAP_DATA_ERROR},
};
#define ACC_INIT_TABLE_SIZE (sizeof(s_acc_init_table) / sizeof(s_acc_init_table[0]))

/* 陀螺仪初始化配置表: {寄存器地址, 配置值, 错误码} */
static const uint8_t s_gyro_init_table[][3] = {
    {BMI088_GYRO_RANGE, BMI088_GYRO_2000, BMI088_GYRO_RANGE_ERROR},
    {BMI088_GYRO_BANDWIDTH, BMI088_GYRO_BW_MUST_SET | BMI088_GYRO_2000_230HZ, BMI088_GYRO_BANDWIDTH_ERROR},
    {BMI088_GYRO_LPM1, BMI088_GYRO_NORMAL_MODE, BMI088_GYRO_LPM1_ERROR},
    {BMI088_GYRO_CTRL, BMI088_DRDY_ON, BMI088_GYRO_CTRL_ERROR},
    {BMI088_GYRO_INT3_INT4_IO_CONF, BMI088_GYRO_INT3_PP | BMI088_GYRO_INT3_LOW, BMI088_GYRO_INT_CONF_ERROR},
    {BMI088_GYRO_INT3_INT4_IO_MAP, BMI088_DRDY_IO_INT3, BMI088_GYRO_INT_MAP_ERROR},
};
#define GYRO_INIT_TABLE_SIZE (sizeof(s_gyro_init_table) / sizeof(s_gyro_init_table[0]))

/* 预标定参数（可通过宏定义配置） */
#ifndef BMI088_PRE_GYRO_OFFSET_X
#define BMI088_PRE_GYRO_OFFSET_X 0.0f
#define BMI088_PRE_GYRO_OFFSET_Y 0.0f
#define BMI088_PRE_GYRO_OFFSET_Z 0.0f
#define BMI088_PRE_G_NORM 9.81f
#endif

/* 温度读取周期（每 N 次 IMU 读取读取一次温度） */
#define TEMP_READ_PERIOD 10

/*============================ 私有函数声明 ============================*/

static void BMI088_SPI_RxCallback(SPIInstance *spi);
static void BMI088_AccINTCallback(GPIOInstance *gpio);

static void BMI088_AccelWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data);
static void BMI088_GyroWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data);
static uint8_t BMI088_AccelReadReg(BMI088Instance *inst, uint8_t reg);
static uint8_t BMI088_GyroReadReg(BMI088Instance *inst, uint8_t reg);

static uint8_t BMI088_AccelInit(BMI088Instance *inst);
static uint8_t BMI088_GyroInit(BMI088Instance *inst);

static void BMI088_StartAccelRead(BMI088Instance *inst);
static void BMI088_StartGyroRead(BMI088Instance *inst);
static void BMI088_StartTempRead(BMI088Instance *inst);

static void BMI088_ParseAccelData(BMI088Instance *inst);
static void BMI088_ParseGyroData(BMI088Instance *inst);
static void BMI088_ParseTempData(BMI088Instance *inst);

/*============================ 私有函数实现 ============================*/

/**
 * @brief SPI DMA 接收完成回调
 */
static void BMI088_SPI_RxCallback(SPIInstance *spi)
{
    BMI088Instance *inst = container_of(spi, BMI088Instance, spi_inst);

    switch (inst->read_state)
    {
    case BMI088_STATE_ACC_READ:
        /* 释放 CS */
        GPIOSet(&inst->cs_acc);
        /* 解析加速度计数据 */
        BMI088_ParseAccelData(inst);
        inst->acc_updated = 1;

        /* 判断是否需要读取温度 */
        inst->temp_read_cnt++;
        if (inst->temp_read_cnt >= TEMP_READ_PERIOD)
        {
            inst->temp_read_cnt = 0;
            inst->read_state = BMI088_STATE_TEMP_READ;
            BMI088_StartTempRead(inst);
        }
        else
        {
            /* 启动陀螺仪读取 */
            inst->read_state = BMI088_STATE_GYRO_READ;
            BMI088_StartGyroRead(inst);
        }
        break;

    case BMI088_STATE_GYRO_READ:
        /* 释放 CS */
        GPIOSet(&inst->cs_gyro);
        /* 解析陀螺仪数据 */
        BMI088_ParseGyroData(inst);
        inst->gyro_updated = 1;

        /* 数据就绪 */
        inst->imu_ready = 1;
        inst->read_state = BMI088_STATE_IDLE;

        /* 调用 APP 回调 */
        if (inst->app_callback != NULL)
        {
            inst->app_callback(inst);
        }
        break;

    case BMI088_STATE_TEMP_READ:
        /* 释放 CS */
        GPIOSet(&inst->cs_acc);
        /* 解析温度数据 */
        BMI088_ParseTempData(inst);
        /* 温度控制 */
        if (inst->heater_enabled)
        {
            BMI088TempControl(inst);
        }
        /* 继续读取陀螺仪 */
        inst->read_state = BMI088_STATE_GYRO_READ;
        BMI088_StartGyroRead(inst);
        break;

    default:
        inst->read_state = BMI088_STATE_IDLE;
        break;
    }
}

/**
 * @brief 加速度计中断回调
 */
static void BMI088_AccINTCallback(GPIOInstance *gpio)
{
    BMI088Instance *inst = container_of(gpio, BMI088Instance, int_acc);

    /* 检查状态机是否空闲 */
    if (inst->read_state != BMI088_STATE_IDLE)
    {
        return;
    }

    /* 清除标志位 */
    inst->imu_ready = 0;
    inst->acc_updated = 0;
    inst->gyro_updated = 0;

    /* 启动加速度计读取 */
    inst->read_state = BMI088_STATE_ACC_READ;
    BMI088_StartAccelRead(inst);
}

/**
 * @brief 写加速度计寄存器（阻塞模式）
 */
static void BMI088_AccelWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data)
{
    uint8_t tx_buf[2] = {reg, data};

    GPIOReset(&inst->cs_acc);
    SPITransmit(&inst->spi_inst, tx_buf, 2);
    GPIOSet(&inst->cs_acc);
}

/**
 * @brief 写陀螺仪寄存器（阻塞模式）
 */
static void BMI088_GyroWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data)
{
    uint8_t tx_buf[2] = {reg, data};

    GPIOReset(&inst->cs_gyro);
    SPITransmit(&inst->spi_inst, tx_buf, 2);
    GPIOSet(&inst->cs_gyro);
}

/**
 * @brief 读加速度计寄存器（阻塞模式）
 * @note 加速度计读取需要 dummy byte
 */
static uint8_t BMI088_AccelReadReg(BMI088Instance *inst, uint8_t reg)
{
    uint8_t tx_buf[3] = {reg | 0x80, 0x55, 0x55};

    GPIOReset(&inst->cs_acc);
    SPITransmitReceive(&inst->spi_inst, tx_buf, 3);
    GPIOSet(&inst->cs_acc);

    return inst->spi_inst.rx_buff[2];
}

/**
 * @brief 读陀螺仪寄存器（阻塞模式）
 * @note 陀螺仪读取无 dummy byte
 */
static uint8_t BMI088_GyroReadReg(BMI088Instance *inst, uint8_t reg)
{
    uint8_t tx_buf[2] = {reg | 0x80, 0x55};

    GPIOReset(&inst->cs_gyro);
    SPITransmitReceive(&inst->spi_inst, tx_buf, 2);
    GPIOSet(&inst->cs_gyro);

    return inst->spi_inst.rx_buff[1];
}

/**
 * @brief 初始化加速度计
 */
static uint8_t BMI088_AccelInit(BMI088Instance *inst)
{
    uint8_t whoami = 0;
    uint8_t error = BMI088_NO_ERROR;

    /* 切换到 SPI 模式（需要一次 fake read） */
    BMI088_AccelReadReg(inst, BMI088_ACC_CHIP_ID);
    DWT_Delay(0.001f);

    /* 软复位 */
    BMI088_AccelWriteReg(inst, BMI088_ACC_SOFTRESET, BMI088_ACC_SOFTRESET_VALUE);
    DWT_Delay(0.08f);

    /* 检查 Who Am I */
    whoami = BMI088_AccelReadReg(inst, BMI088_ACC_CHIP_ID);
    if (whoami != BMI088_ACC_CHIP_ID_VALUE)
    {
        LOGERROR("[BMI088] Accel WhoAmI failed: 0x%02X (expect 0x%02X)", whoami, BMI088_ACC_CHIP_ID_VALUE);
        return BMI088_NO_SENSOR;
    }

    /* 写入配置 */
    for (uint8_t i = 0; i < ACC_INIT_TABLE_SIZE; i++)
    {
        BMI088_AccelWriteReg(inst, s_acc_init_table[i][0], s_acc_init_table[i][1]);
        DWT_Delay(0.001f);

        uint8_t read_val = BMI088_AccelReadReg(inst, s_acc_init_table[i][0]);
        DWT_Delay(0.001f);

        if (read_val != s_acc_init_table[i][1])
        {
            error |= s_acc_init_table[i][2];
            LOGWARNING("[BMI088] Accel reg 0x%02X write failed", s_acc_init_table[i][0]);
        }
    }

    return error;
}

/**
 * @brief 初始化陀螺仪
 */
static uint8_t BMI088_GyroInit(BMI088Instance *inst)
{
    uint8_t whoami = 0;
    uint8_t error = BMI088_NO_ERROR;

    /* 软复位 */
    BMI088_GyroWriteReg(inst, BMI088_GYRO_SOFTRESET, BMI088_GYRO_SOFTRESET_VALUE);
    DWT_Delay(0.08f);

    /* 检查 Who Am I */
    whoami = BMI088_GyroReadReg(inst, BMI088_GYRO_CHIP_ID);
    if (whoami != BMI088_GYRO_CHIP_ID_VALUE)
    {
        LOGERROR("[BMI088] Gyro WhoAmI failed: 0x%02X (expect 0x%02X)", whoami, BMI088_GYRO_CHIP_ID_VALUE);
        return BMI088_NO_SENSOR;
    }

    /* 写入配置 */
    for (uint8_t i = 0; i < GYRO_INIT_TABLE_SIZE; i++)
    {
        BMI088_GyroWriteReg(inst, s_gyro_init_table[i][0], s_gyro_init_table[i][1]);
        DWT_Delay(0.001f);

        uint8_t read_val = BMI088_GyroReadReg(inst, s_gyro_init_table[i][0]);
        DWT_Delay(0.001f);

        if (read_val != s_gyro_init_table[i][1])
        {
            error |= s_gyro_init_table[i][2];
            LOGWARNING("[BMI088] Gyro reg 0x%02X write failed", s_gyro_init_table[i][0]);
        }
    }

    return error;
}

/**
 * @brief 启动加速度计 DMA 读取
 */
static void BMI088_StartAccelRead(BMI088Instance *inst)
{
    /* 构造读取命令：加速度计需要 dummy byte，读取 6 字节数据 */
    /* tx_buf[0] = 读命令, tx_buf[1] = dummy, tx_buf[2-7] = dummy */
    static uint8_t tx_buf[8] = {BMI088_ACCEL_XOUT_L | 0x80, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55};

    GPIOReset(&inst->cs_acc);
    SPITransmitReceive(&inst->spi_inst, tx_buf, 8);
    /* CS 在回调中释放 */
}

/**
 * @brief 启动陀螺仪 DMA 读取
 */
static void BMI088_StartGyroRead(BMI088Instance *inst)
{
    /* 构造读取命令：陀螺仪无 dummy byte，读取 6 字节数据 + 1 字节 CHIP_ID 校验 */
    static uint8_t tx_buf[8] = {BMI088_GYRO_CHIP_ID | 0x80, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55};

    GPIOReset(&inst->cs_gyro);
    SPITransmitReceive(&inst->spi_inst, tx_buf, 8);
    /* CS 在回调中释放 */
}

/**
 * @brief 启动温度 DMA 读取
 */
static void BMI088_StartTempRead(BMI088Instance *inst)
{
    /* 构造读取命令：温度需要 dummy byte，读取 2 字节数据 */
    static uint8_t tx_buf[4] = {BMI088_TEMP_M | 0x80, 0x55, 0x55, 0x55};

    GPIOReset(&inst->cs_acc); /* 温度传感器在加速度计上 */
    SPITransmitReceive(&inst->spi_inst, tx_buf, 4);
    /* CS 在回调中释放 */
}

/**
 * @brief 解析加速度计数据
 */
static void BMI088_ParseAccelData(BMI088Instance *inst)
{
    /* rx_buff[0-1] = dummy, rx_buff[2-7] = 数据 */
    int16_t raw_x = (int16_t)((inst->spi_inst.rx_buff[3] << 8) | inst->spi_inst.rx_buff[2]);
    int16_t raw_y = (int16_t)((inst->spi_inst.rx_buff[5] << 8) | inst->spi_inst.rx_buff[4]);
    int16_t raw_z = (int16_t)((inst->spi_inst.rx_buff[7] << 8) | inst->spi_inst.rx_buff[6]);

    inst->data.acc[0] = raw_x * inst->acc_sen * inst->acc_scale;
    inst->data.acc[1] = raw_y * inst->acc_sen * inst->acc_scale;
    inst->data.acc[2] = raw_z * inst->acc_sen * inst->acc_scale;
}

/**
 * @brief 解析陀螺仪数据
 */
static void BMI088_ParseGyroData(BMI088Instance *inst)
{
    /* rx_buff[0] = CHIP_ID, rx_buff[1-6] = 数据 */
    /* 先验证 CHIP_ID */
    if (inst->spi_inst.rx_buff[0] != BMI088_GYRO_CHIP_ID_VALUE)
    {
        LOGWARNING("[BMI088] Gyro CHIP_ID mismatch: 0x%02X", inst->spi_inst.rx_buff[0]);
        return;
    }

    int16_t raw_x = (int16_t)((inst->spi_inst.rx_buff[2] << 8) | inst->spi_inst.rx_buff[1]);
    int16_t raw_y = (int16_t)((inst->spi_inst.rx_buff[4] << 8) | inst->spi_inst.rx_buff[3]);
    int16_t raw_z = (int16_t)((inst->spi_inst.rx_buff[6] << 8) | inst->spi_inst.rx_buff[5]);

    inst->data.gyro[0] = raw_x * inst->gyro_sen - inst->gyro_offset[0];
    inst->data.gyro[1] = raw_y * inst->gyro_sen - inst->gyro_offset[1];
    inst->data.gyro[2] = raw_z * inst->gyro_sen - inst->gyro_offset[2];
}

/**
 * @brief 解析温度数据
 */
static void BMI088_ParseTempData(BMI088Instance *inst)
{
    /* rx_buff[0-1] = dummy, rx_buff[2-3] = 数据 */
    int16_t raw_temp = (int16_t)((inst->spi_inst.rx_buff[2] << 3) | (inst->spi_inst.rx_buff[3] >> 5));

    /* 处理负温度 */
    if (raw_temp > 1023)
    {
        raw_temp -= 2048;
    }

    inst->data.temp = raw_temp * BMI088_TEMP_FACTOR + BMI088_TEMP_OFFSET;
}

/*============================ 公开接口实现 ============================*/

/**
 * @brief 注册并初始化 BMI088
 */
int8_t BMI088Register(BMI088Instance *inst)
{
    uint8_t error = BMI088_NO_ERROR;

    if (inst == NULL)
    {
        LOGERROR("[BMI088] Instance is NULL");
        return -1;
    }

    /* 暂时使用阻塞模式进行初始化 */
    // SPI_Work_Mode_e saved_mode = inst->spi_inst.work_mode;
    inst->spi_inst.work_mode = SPI_BLOCK_MODE;

    /* 注册 SPI 实例 */
    if (SPIRegister(&inst->spi_inst) != 0)
    {
        LOGERROR("[BMI088] SPI register failed");
        return -1;
    }

    /* 注册 GPIO 实例 */
    if (GPIORegister(&inst->cs_acc) != 0)
    {
        LOGERROR("[BMI088] CS_ACC register failed");
        return -1;
    }
    if (GPIORegister(&inst->cs_gyro) != 0)
    {
        LOGERROR("[BMI088] CS_GYRO register failed");
        return -1;
    }

    /* 初始状态：CS 拉高 */
    GPIOSet(&inst->cs_acc);
    GPIOSet(&inst->cs_gyro);

    /* 初始化加速度计和陀螺仪 */
    error |= BMI088_AccelInit(inst);
    error |= BMI088_GyroInit(inst);

    if (error != BMI088_NO_ERROR)
    {
        LOGERROR("[BMI088] Init failed, error=0x%02X", error);
        return -1;
    }

    /* 标定 */
    if (inst->cali_mode == BMI088_CALI_ONLINE)
    {
        BMI088Calibrate(inst);
    }
    else
    {
        /* 使用预标定参数 */
        inst->gyro_offset[0] = BMI088_PRE_GYRO_OFFSET_X;
        inst->gyro_offset[1] = BMI088_PRE_GYRO_OFFSET_Y;
        inst->gyro_offset[2] = BMI088_PRE_GYRO_OFFSET_Z;
        inst->g_norm = BMI088_PRE_G_NORM;
        inst->acc_scale = 9.81f / inst->g_norm;
    }

    /* 注册加热 PWM（如果启用） */
    if (inst->heater_pwm.tim_e < TIM_NUM_MAX)
    {
        if (PWMRegister(&inst->heater_pwm) == 0)
        {
            inst->heater_enabled = 1;
            LOGINFO("[BMI088] Heater PWM registered");
        }
    }

    /* 恢复工作模式 */
    if (inst->work_mode == BMI088_DMA_MODE)
    {
        inst->spi_inst.work_mode = SPI_DMA_MODE;
        inst->spi_inst.rx_callback = BMI088_SPI_RxCallback;

        /* 注册中断 GPIO */
        inst->int_acc.callback = BMI088_AccINTCallback;
        if (GPIORegister(&inst->int_acc) != 0)
        {
            LOGWARNING("[BMI088] INT_ACC register failed, falling back to polling");
            inst->work_mode = BMI088_BLOCK_MODE;
            inst->spi_inst.work_mode = SPI_BLOCK_MODE;
        }

        /* 陀螺仪中断（可选） */
        if (inst->int_gyro.gpio_e < GPIO_NUM_MAX)
        {
            GPIORegister(&inst->int_gyro);
        }
    }

    LOGINFO("[BMI088] Registered successfully, mode=%s",
            inst->work_mode == BMI088_DMA_MODE ? "DMA" : "BLOCK");

    return 0;
}

/**
 * @brief 获取 BMI088 数据
 */
uint8_t BMI088GetData(BMI088Instance *inst, BMI088_Data_t *data)
{
    if (inst == NULL || data == NULL)
    {
        return 0;
    }

    if (inst->imu_ready)
    {
        data->gyro[0] = inst->data.gyro[0];
        data->gyro[1] = inst->data.gyro[1];
        data->gyro[2] = inst->data.gyro[2];
        data->acc[0] = inst->data.acc[0];
        data->acc[1] = inst->data.acc[1];
        data->acc[2] = inst->data.acc[2];
        data->temp = inst->data.temp;
        inst->imu_ready = 0;
        return 1;
    }

    return 0;
}

/**
 * @brief 在线标定 BMI088
 */
void BMI088Calibrate(BMI088Instance *inst)
{
    if (inst == NULL)
    {
        return;
    }

    LOGINFO("[BMI088] Starting calibration...");

    /* 暂存工作模式 */
    SPI_Work_Mode_e saved_mode = inst->spi_inst.work_mode;
    inst->spi_inst.work_mode = SPI_BLOCK_MODE;

    const uint16_t cali_times = 6000;
    float gyro_sum[3] = {0};
    float g_norm_sum = 0;
    float gyro_max[3] = {0}, gyro_min[3] = {0};
    float g_norm_max = 0, g_norm_min = 0;

    uint8_t tx_acc[8] = {BMI088_ACCEL_XOUT_L | 0x80, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55};
    uint8_t tx_gyro[8] = {BMI088_GYRO_CHIP_ID | 0x80, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55};

    for (uint16_t i = 0; i < cali_times; i++)
    {
        /* 读取加速度计 */
        GPIOReset(&inst->cs_acc);
        SPITransmitReceive(&inst->spi_inst, tx_acc, 8);
        GPIOSet(&inst->cs_acc);

        int16_t raw_ax = (int16_t)((inst->spi_inst.rx_buff[3] << 8) | inst->spi_inst.rx_buff[2]);
        int16_t raw_ay = (int16_t)((inst->spi_inst.rx_buff[5] << 8) | inst->spi_inst.rx_buff[4]);
        int16_t raw_az = (int16_t)((inst->spi_inst.rx_buff[7] << 8) | inst->spi_inst.rx_buff[6]);

        float ax = raw_ax * inst->acc_sen;
        float ay = raw_ay * inst->acc_sen;
        float az = raw_az * inst->acc_sen;

        float g_norm = sqrtf(ax * ax + ay * ay + az * az);
        g_norm_sum += g_norm;

        /* 读取陀螺仪 */
        GPIOReset(&inst->cs_gyro);
        SPITransmitReceive(&inst->spi_inst, tx_gyro, 8);
        GPIOSet(&inst->cs_gyro);

        int16_t raw_gx = (int16_t)((inst->spi_inst.rx_buff[2] << 8) | inst->spi_inst.rx_buff[1]);
        int16_t raw_gy = (int16_t)((inst->spi_inst.rx_buff[4] << 8) | inst->spi_inst.rx_buff[3]);
        int16_t raw_gz = (int16_t)((inst->spi_inst.rx_buff[6] << 8) | inst->spi_inst.rx_buff[5]);

        float gx = raw_gx * inst->gyro_sen;
        float gy = raw_gy * inst->gyro_sen;
        float gz = raw_gz * inst->gyro_sen;

        gyro_sum[0] += gx;
        gyro_sum[1] += gy;
        gyro_sum[2] += gz;

        /* 更新最大最小值 */
        if (i == 0)
        {
            g_norm_max = g_norm_min = g_norm;
            gyro_max[0] = gyro_min[0] = gx;
            gyro_max[1] = gyro_min[1] = gy;
            gyro_max[2] = gyro_min[2] = gz;
        }
        else
        {
            if (g_norm > g_norm_max)
                g_norm_max = g_norm;
            if (g_norm < g_norm_min)
                g_norm_min = g_norm;
            if (gx > gyro_max[0])
                gyro_max[0] = gx;
            if (gx < gyro_min[0])
                gyro_min[0] = gx;
            if (gy > gyro_max[1])
                gyro_max[1] = gy;
            if (gy < gyro_min[1])
                gyro_min[1] = gy;
            if (gz > gyro_max[2])
                gyro_max[2] = gz;
            if (gz < gyro_min[2])
                gyro_min[2] = gz;
        }

        /* 检查运动幅度 */
        float g_diff = g_norm_max - g_norm_min;
        float gyro_diff[3] = {gyro_max[0] - gyro_min[0],
                              gyro_max[1] - gyro_min[1],
                              gyro_max[2] - gyro_min[2]};

        if (g_diff > 0.5f || gyro_diff[0] > 0.15f || gyro_diff[1] > 0.15f || gyro_diff[2] > 0.15f)
        {
            LOGWARNING("[BMI088] Calibration interrupted by motion, retrying...");
            /* 重置 */
            i = 0;
            gyro_sum[0] = gyro_sum[1] = gyro_sum[2] = 0;
            g_norm_sum = 0;
            g_norm_max = g_norm_min = 0;
            DWT_Delay(0.1f);
            continue;
        }

        DWT_Delay(0.0005f);
    }

    /* 计算平均值 */
    inst->gyro_offset[0] = gyro_sum[0] / cali_times;
    inst->gyro_offset[1] = gyro_sum[1] / cali_times;
    inst->gyro_offset[2] = gyro_sum[2] / cali_times;
    inst->g_norm = g_norm_sum / cali_times;
    inst->acc_scale = 9.81f / inst->g_norm;

    /* 恢复工作模式 */
    inst->spi_inst.work_mode = saved_mode;

    LOGINFO("[BMI088] Calibration done: gyro_off=[%.4f,%.4f,%.4f], g_norm=%.3f",
            inst->gyro_offset[0], inst->gyro_offset[1], inst->gyro_offset[2], inst->g_norm);
}

/**
 * @brief 温度控制（简单阈值控制）
 */
void BMI088TempControl(BMI088Instance *inst)
{
    if (inst == NULL || !inst->heater_enabled)
    {
        return;
    }

    const float hysteresis = 1.0f;

    if (inst->data.temp < inst->target_temp - hysteresis)
    {
        /* 温度过低，开启加热 */
        PWMSetDutyRatio(&inst->heater_pwm, 0.5f);
    }
    else if (inst->data.temp >= inst->target_temp)
    {
        /* 温度达到目标，关闭加热 */
        PWMSetDutyRatio(&inst->heater_pwm, 0.0f);
    }
}
