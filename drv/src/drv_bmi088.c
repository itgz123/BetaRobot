/**
 * @file drv_bmi088.c
 * @brief BMI088 六轴 IMU 驱动实现
 *
 */

#include "drv_bmi088.h"
#include <string.h>

/*============================ SPI通信宏定义 ============================*/

#define BMI088_SPI_READ_CMD 0x80   // R/W位=1（读）
#define BMI088_SPI_WRITE_CMD 0x7F  // R/W位=0（写）
#define BMI088_SPI_DUMMY_BYTE 0x55 // 哑指令（用于发送时填充）

/*============================ 私有函数声明 ============================*/

static void BMI088_AccReadReg(BMI088Instance *inst, uint8_t reg, uint8_t *data, uint16_t len);
static void BMI088_AccWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data);
static uint8_t BMI088_AccWriteRegWithCheck(BMI088Instance *inst, uint8_t reg, uint8_t data);
static void BMI088_GyroReadReg(BMI088Instance *inst, uint8_t reg, uint8_t *data, uint16_t len);
static void BMI088_GyroWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data);
static uint8_t BMI088_GyroWriteRegWithCheck(BMI088Instance *inst, uint8_t reg, uint8_t data);

/*============================ 灵敏度查找表 ============================*/

/* 加速度计灵敏度查找表：将原始值转换为 g，单位 g/LSB */
const float BMI088_AccSenTable[BMI088_ACC_RANGE_NUM] = {
    0.0008974358974f,  // ±3g 量程灵敏度
    0.00179443359375f, // ±6g 量程灵敏度
    0.0035888671875f,  // ±12g 量程灵敏度
    0.007177734375f,   // ±24g 量程灵敏度
};

/* 陀螺仪灵敏度查找表：将原始值转换为 rad/s，单位 rad/s/LSB */
const float BMI088_GyroSenTable[BMI088_GYRO_RANGE_NUM] = {
    0.00106526443603169529841533860381f,     // ±2000°/s 量程灵敏度
    0.00053263221801584764920766930190693f,  // ±1000°/s 量程灵敏度
    0.00026631610900792382460383465095346f,  // ±500°/s 量程灵敏度
    0.00013315805450396191230191732547673f,  // ±250°/s 量程灵敏度
    0.000066579027251980956150958662738366f, // ±125°/s 量程灵敏度
};

/*============================ 私有函数实现 ============================*/

/**
 * @brief 加速度计读取寄存器
 * @param inst BMI088实例指针
 * @param reg 寄存器地址
 * @param data 数据接收缓冲区
 * @param len 读取数据长度
 * @note 加速度计读取时第一个字节为虚拟字节，需跳过
 */
static void BMI088_AccReadReg(BMI088Instance *inst, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (len > 6)
    {
        LOGWARNING("BMI088 Acc read len > 6: %d", len);
        return;
    }

    inst->tx_buff[0] = reg | BMI088_SPI_READ_CMD;

    GPIOReset(&inst->cs_acc);
    SPITransmitReceive(&inst->spi_inst, inst->tx_buff, 1 + 1 + len); // 1地址 + 1虚拟 + len数据
    GPIOSet(&inst->cs_acc);

    // 跳过虚拟字节，从spi_inst.rx_buff[2]开始复制
    memcpy(data, &inst->spi_inst.rx_buff[2], len);
}

/**
 * @brief 加速度计写入寄存器（无验证）
 * @param inst BMI088实例指针
 * @param reg 寄存器地址
 * @param data 写入数据
 */
static void BMI088_AccWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data)
{
    inst->tx_buff[0] = reg & BMI088_SPI_WRITE_CMD;
    inst->tx_buff[1] = data;

    GPIOReset(&inst->cs_acc);
    SPITransmit(&inst->spi_inst, inst->tx_buff, 2);
    GPIOSet(&inst->cs_acc);
}

/**
 * @brief 加速度计写入寄存器并验证
 * @param inst BMI088实例指针
 * @param reg 寄存器地址
 * @param data 写入数据
 * @return 0-成功，非0-失败
 */
static uint8_t BMI088_AccWriteRegWithCheck(BMI088Instance *inst, uint8_t reg, uint8_t data)
{
    BMI088_AccWriteReg(inst, reg, data);
    DWT_Delay(0.001f); // 等待1ms

    uint8_t check;
    BMI088_AccReadReg(inst, reg, &check, 1);
    return (check != data);
}

/**
 * @brief 陀螺仪读取寄存器
 * @param inst BMI088实例指针
 * @param reg 寄存器地址
 * @param data 数据接收缓冲区
 * @param len 读取数据长度
 * @note 陀螺仪读取无虚拟字节
 */
static void BMI088_GyroReadReg(BMI088Instance *inst, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (len > 6)
    {
        LOGWARNING("BMI088 Gyro read len > 6: %d", len);
        return;
    }

    inst->tx_buff[0] = reg | BMI088_SPI_READ_CMD;

    GPIOReset(&inst->cs_gyro);
    SPITransmitReceive(&inst->spi_inst, inst->tx_buff, 1 + len); // 1地址 + len数据
    GPIOSet(&inst->cs_gyro);

    // 数据从spi_inst.rx_buff[1]开始
    memcpy(data, &inst->spi_inst.rx_buff[1], len);
}

/**
 * @brief 陀螺仪写入寄存器（无验证）
 * @param inst BMI088实例指针
 * @param reg 寄存器地址
 * @param data 写入数据
 */
static void BMI088_GyroWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data)
{
    inst->tx_buff[0] = reg & BMI088_SPI_WRITE_CMD;
    inst->tx_buff[1] = data;

    GPIOReset(&inst->cs_gyro);
    SPITransmit(&inst->spi_inst, inst->tx_buff, 2);
    GPIOSet(&inst->cs_gyro);
}

/**
 * @brief 陀螺仪写入寄存器并验证
 * @param inst BMI088实例指针
 * @param reg 寄存器地址
 * @param data 写入数据
 * @return 0-成功，非0-失败
 */
static uint8_t BMI088_GyroWriteRegWithCheck(BMI088Instance *inst, uint8_t reg, uint8_t data)
{
    BMI088_GyroWriteReg(inst, reg, data);
    DWT_Delay(0.001f); // 等待1ms

    uint8_t check;
    BMI088_GyroReadReg(inst, reg, &check, 1);
    return (check != data);
}

/*============================ 公开接口实现 ============================*/

/**
 * @brief 注册BMI088实例
 * @param inst BMI088实例指针
 * @return 0成功，-1失败
 */
int8_t BMI088Register(BMI088Instance *inst)
{
    // 注册 SPI 实例
    if (SPIRegister(&inst->spi_inst) != 0)
    {
        LOGERROR("BMI088 SPI register failed");
        return -1;
    }

    // 注册 GPIO 实例
    if (GPIORegister(&inst->cs_acc) != 0)
    {
        LOGERROR("BMI088 cs_acc register failed");
        return -1;
    }
    if (GPIORegister(&inst->cs_gyro) != 0)
    {
        LOGERROR("BMI088 cs_gyro register failed");
        return -1;
    }
    if (GPIORegister(&inst->int_acc) != 0)
    {
        LOGERROR("BMI088 int_acc register failed");
        return -1;
    }
    if (GPIORegister(&inst->int_gyro) != 0)
    {
        LOGERROR("BMI088 int_gyro register failed");
        return -1;
    }

    // 注册 PWM 实例（加热器可选）
    if (PWMRegister(&inst->heater_pwm) != 0)
    {
        LOGERROR("BMI088 heater_pwm register failed");
        return -1;
    }

    return 0;
}

/**
 * @brief 设置BMI088配置并完成初始化
 * @param inst       BMI088实例指针
 * @param acc_range  加速度计量程
 * @param acc_bwp    加速度计低通滤波器带宽
 * @param acc_odr    加速度计输出数据速率
 * @param gyro_range 陀螺仪量程
 * @param gyro_odr   陀螺仪输出数据速率
 * @param gyro_bw    陀螺仪滤波器带宽
 * @param work_mode  工作模式
 * @return 0成功，-1失败
 * @note 注册后调用一次完成初始化，修改配置时再次调用
 */
int8_t BMI088SetConfig(BMI088Instance *inst,
                       BMI088_AccRange_e acc_range, uint8_t acc_bwp, uint8_t acc_odr,
                       BMI088_GyroRange_e gyro_range, uint8_t gyro_odr, uint8_t gyro_bw,
                       BMI088_WorkMode_e work_mode)
{
    uint8_t chip_id = 0;

    // 保存用户配置到实例
    inst->acc_range = acc_range;
    inst->acc_bwp = acc_bwp;
    inst->acc_odr = acc_odr;
    inst->gyro_range = gyro_range;
    inst->gyro_odr = gyro_odr;
    inst->gyro_bw = gyro_bw;
    inst->work_mode = work_mode;

    /*============================ 加速度计初始化 ============================*/

    // 1. 切换到SPI模式（加速度计上电默认I2C模式，需要一次fake read切换）
    BMI088_AccReadReg(inst, BMI088_ACC_CHIP_ID_REG, &chip_id, 1);
    DWT_Delay(0.01f); // 等待10ms

    // 2. 软复位加速度计
    inst->tx_buff[0] = BMI088_ACC_SOFTRESET_REG & BMI088_SPI_WRITE_CMD;
    inst->tx_buff[1] = BMI088_ACC_SOFTRESET_CMD;
    GPIOReset(&inst->cs_acc);
    SPITransmit(&inst->spi_inst, inst->tx_buff, 2);
    GPIOSet(&inst->cs_acc);
    DWT_Delay(0.05f); // 软复位后等待50ms

    // 3. 再次切换到SPI模式（复位后回到I2C模式）
    BMI088_AccReadReg(inst, BMI088_ACC_CHIP_ID_REG, &chip_id, 1);
    DWT_Delay(0.01f);

    // 4. 检查加速度计 CHIP_ID
    BMI088_AccReadReg(inst, BMI088_ACC_CHIP_ID_REG, &chip_id, 1);
    LOGINFO("BMI088 Acc CHIP_ID: 0x%02X", chip_id);
    if (chip_id != BMI088_ACC_CHIP_ID_VAL)
    {
        LOGERROR("BMI088 Acc CHIP_ID error: 0x%02X", chip_id);
        return -1;
    }
    DWT_Delay(0.01f);

    // 5. 开启加速度计（PWR_CTRL 可在挂起模式下访问）
    BMI088_AccWriteReg(inst, BMI088_ACC_PWR_CTRL_REG, BMI088_ACC_ENABLE_ON);
    DWT_Delay(0.05f); // 等待50ms让加速度计稳定

    // 6. 退出挂起模式（PWR_CONF 可在挂起模式下访问）
    BMI088_AccWriteReg(inst, BMI088_ACC_PWR_CONF_REG, BMI088_ACC_PWR_SAVE_ACTIVE);
    DWT_Delay(0.01f);

    // 7. 写入量程配置并验证
    if (BMI088_AccWriteRegWithCheck(inst, BMI088_ACC_RANGE_REG, (uint8_t)acc_range) != 0)
    {
        LOGERROR("BMI088 acc_range write failed");
        return -1;
    }
    DWT_Delay(0.001f);

    // 8. 写入滤波器和ODR配置并验证
    uint8_t acc_conf = acc_bwp | acc_odr;
    if (BMI088_AccWriteRegWithCheck(inst, BMI088_ACC_CONF_REG, acc_conf) != 0)
    {
        LOGERROR("BMI088 acc_conf write failed");
        return -1;
    }
    DWT_Delay(0.001f);

    // 9. 配置 INT1 引脚并验证
    uint8_t int1_conf = BMI088_INT1_OUT | BMI088_INT1_LVL_HIGH;
    if (BMI088_AccWriteRegWithCheck(inst, BMI088_INT1_IO_CTRL_REG, int1_conf) != 0)
    {
        LOGERROR("BMI088 int1_io_ctrl write failed");
        return -1;
    }
    DWT_Delay(0.001f);

    // 10. 配置中断映射并验证
    if (BMI088_AccWriteRegWithCheck(inst, BMI088_INT_MAP_DATA_REG, BMI088_INT1_DRDY) != 0)
    {
        LOGERROR("BMI088 int_map_data write failed");
        return -1;
    }
    DWT_Delay(0.001f);

    /*============================ 陀螺仪初始化 ============================*/

    // 1. 软复位陀螺仪
    inst->tx_buff[0] = BMI088_GYRO_SOFTRESET_REG & BMI088_SPI_WRITE_CMD;
    inst->tx_buff[1] = BMI088_GYRO_SOFTRESET_CMD;
    GPIOReset(&inst->cs_gyro);
    SPITransmit(&inst->spi_inst, inst->tx_buff, 2);
    GPIOSet(&inst->cs_gyro);
    DWT_Delay(0.08f); // 陀螺仪软复位需要30ms，多等待一些

    // 2. 检查陀螺仪 CHIP_ID
    BMI088_GyroReadReg(inst, BMI088_GYRO_CHIP_ID_REG, &chip_id, 1);
    LOGINFO("BMI088 Gyro CHIP_ID: 0x%02X", chip_id);
    if (chip_id != BMI088_GYRO_CHIP_ID_VALUE)
    {
        LOGERROR("BMI088 Gyro CHIP_ID error: 0x%02X", chip_id);
        return -1;
    }
    DWT_Delay(0.001f);

    // 3. 写入量程配置并验证
    if (BMI088_GyroWriteRegWithCheck(inst, BMI088_GYRO_RANGE_REG, (uint8_t)gyro_range) != 0)
    {
        LOGERROR("BMI088 gyro_range write failed");
        return -1;
    }
    DWT_Delay(0.001f);

    // 4. 写入带宽配置并验证（ODR + BW + MUST_SET）
    // @note bit[7] 必须写入1
    uint8_t gyro_bw_val = gyro_odr | gyro_bw | BMI088_GYRO_BW_MUST_SET;
    if (BMI088_GyroWriteRegWithCheck(inst, BMI088_GYRO_BANDWIDTH_REG, gyro_bw_val) != 0)
    {
        LOGERROR("BMI088 gyro_bandwidth write failed");
        return -1;
    }
    DWT_Delay(0.001f);

    // 5. 写入电源模式并验证（正常模式）
    if (BMI088_GyroWriteRegWithCheck(inst, BMI088_GYRO_LPM1_REG, BMI088_GYRO_PM_NORMAL) != 0)
    {
        LOGERROR("BMI088 gyro_lpm1 write failed");
        return -1;
    }
    DWT_Delay(0.001f);

    // 6. 配置中断控制并验证（启用新数据中断）
    if (BMI088_GyroWriteRegWithCheck(inst, BMI088_GYRO_INT_CTRL_REG, BMI088_GYRO_INT_DATA_EN) != 0)
    {
        LOGERROR("BMI088 gyro_int_ctrl write failed");
        return -1;
    }
    DWT_Delay(0.001f);

    // 7. 配置 INT3 引脚并验证（推挽模式 + 高电平有效）
    uint8_t int3_conf = BMI088_INT3_LVL_HIGH;
    if (BMI088_GyroWriteRegWithCheck(inst, BMI088_GYRO_INT3_INT4_IO_CONF_REG, int3_conf) != 0)
    {
        LOGERROR("BMI088 int3_int4_io_conf write failed");
        return -1;
    }
    DWT_Delay(0.001f);

    // 8. 配置 INT3 中断映射并验证（数据就绪中断映射到 INT3）
    if (BMI088_GyroWriteRegWithCheck(inst, BMI088_GYRO_INT3_INT4_IO_MAP_REG, BMI088_INT3_DATA) != 0)
    {
        LOGERROR("BMI088 int3_int4_io_map write failed");
        return -1;
    }

    LOGINFO("BMI088 init success");
    return 0;
}
