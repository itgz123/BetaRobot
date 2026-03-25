# BMI088 驱动使用指南

## 一、概述

BMI088 是 Bosch 公司推出的一款高性能六轴 IMU（惯性测量单元），集成了三轴加速度计和三轴陀螺仪。本驱动支持：

- SPI 通信（DMA 非阻塞模式）
- 可配置的 ODR（输出数据率）、量程、带宽
- 在线标定 / 预标定参数
- 温度控制（加热器）
- 数据就绪中断触发读取

## 二、硬件连接

### 2.1 引脚说明

| 引脚      | 功能                 | 说明                         |
| --------- | -------------------- | ---------------------------- |
| CSB1      | 加速度计片选         | 低电平有效，用于选择加速度计 |
| CSB2      | 陀螺仪片选           | 低电平有效，用于选择陀螺仪   |
| INT1      | 加速度计中断         | 数据就绪中断输出             |
| INT3      | 陀螺仪中断           | 数据就绪中断输出（可选）     |
| SCK       | SPI 时钟             | -                            |
| SDI       | SPI 数据输入（MOSI） | -                            |
| SDO1/SDO2 | SPI 数据输出（MISO） | 加速度计/陀螺仪共用          |

### 2.2 bsp_cfg.h 配置

确保 `bsp_cfg.h` 中定义了 BMI088 相关的 GPIO 和 SPI 枚举：

```c
// GPIO 枚举
GPIO_BMI088_CS1,   // 加速度计片选
GPIO_BMI088_CS2,   // 陀螺仪片选
GPIO_BMI088_INT1,  // 加速度计中断
GPIO_BMI088_INT3,  // 陀螺仪中断（可选）

// SPI 枚举
SPI_BMI088,        // SPI 总线

// TIM 枚举（可选，用于加热）
TIM_HEATER,        // 加热 PWM
```

同时更新实例数量：

```c
#define SPI_INSTANCE_NUM 1      // 至少 1 个 SPI 设备
#define GPIO_INSTANCE_NUM 11    // 至少包含 BMI088 的 4 个 GPIO
```

## 三、使用步骤

### 3.1 包含头文件

```c
#include "drv_bmi088.h"
```

### 3.2 定义实例

使用 `BMI088_INSTANCE_DEF` 宏静态定义实例：

```c
// 定义 BMI088 实例
BMI088_INSTANCE_DEF(
    imu,                    // 实例名称
    SPI_BMI088,             // SPI 枚举
    GPIO_BMI088_CS1,        // 加速度计 CS
    GPIO_BMI088_CS2,        // 陀螺仪 CS
    GPIO_BMI088_INT1,       // 加速度计 INT
    GPIO_BMI088_INT3,       // 陀螺仪 INT（不使用填 GPIO_NUM_MAX）
    TIM_HEATER,             // 加热 TIM（不使用填 TIM_NUM_MAX）
    IMU_Callback            // 数据就绪回调函数
);
```

### 3.3 定义回调函数

```c
// IMU 数据就绪回调
static void IMU_Callback(BMI088Instance *inst)
{
    // 可在此处理数据，或设置标志位通知任务
}
```

### 3.4 注册实例

在初始化函数中调用 `BMI088Register`：

```c
void AppInit(void)
{
    if (BMI088Register(&imu) != 0)
    {
        LOGERROR("BMI088 register failed");
    }
}
```

### 3.5 获取数据

在任务中调用 `BMI088GetData`：

```c
void Task(void)
{
    BMI088_Data_t data;
    if (BMI088GetData(&imu, &data))
    {
        // 使用数据
        float ax = data.acc[0];  // m/s²
        float ay = data.acc[1];
        float az = data.acc[2];
        float gx = data.gyro[0]; // rad/s
        float gy = data.gyro[1];
        float gz = data.gyro[2];
        float temp = data.temp;  // °C
    }
}
```

## 四、配置参数

### 4.1 加速度计配置

#### ODR（输出数据率）

```c
typedef enum
{
    BMI088_ACC_ODR_12_5_HZ,   // 12.5Hz
    BMI088_ACC_ODR_25_HZ,     // 25Hz
    BMI088_ACC_ODR_50_HZ,     // 50Hz
    BMI088_ACC_ODR_100_HZ,    // 100Hz
    BMI088_ACC_ODR_200_HZ,    // 200Hz
    BMI088_ACC_ODR_400_HZ,    // 400Hz
    BMI088_ACC_ODR_800_HZ,    // 800Hz（默认）
    BMI088_ACC_ODR_1600_HZ,   // 1600Hz
} BMI088_AccODR_e;
```

#### 量程

```c
typedef enum
{
    BMI088_ACC_RANGE_3G,   // ±3g,  灵敏度 0.000897 g/LSB
    BMI088_ACC_RANGE_6G,   // ±6g,  灵敏度 0.001794 g/LSB（默认）
    BMI088_ACC_RANGE_12G,  // ±12g, 灵敏度 0.003589 g/LSB
    BMI088_ACC_RANGE_24G,  // ±24g, 灵敏度 0.007178 g/LSB
} BMI088_AccRange_e;
```

### 4.2 陀螺仪配置

#### 量程

```c
typedef enum
{
    BMI088_GYRO_RANGE_2000,  // ±2000°/s, 灵敏度 0.001065 rad/s/LSB（默认）
    BMI088_GYRO_RANGE_1000,  // ±1000°/s, 灵敏度 0.000533 rad/s/LSB
    BMI088_GYRO_RANGE_500,   // ±500°/s,  灵敏度 0.000266 rad/s/LSB
    BMI088_GYRO_RANGE_250,   // ±250°/s,  灵敏度 0.000133 rad/s/LSB
    BMI088_GYRO_RANGE_125,   // ±125°/s,  灵敏度 0.000067 rad/s/LSB
} BMI088_GyroRange_e;
```

#### 带宽

```c
typedef enum
{
    BMI088_GYRO_BW_2000_532HZ,  // ODR 2000Hz, 带宽 532Hz
    BMI088_GYRO_BW_2000_230HZ,  // ODR 2000Hz, 带宽 230Hz（默认）
    BMI088_GYRO_BW_1000_116HZ,  // ODR 1000Hz, 带宽 116Hz
    BMI088_GYRO_BW_400_47HZ,    // ODR 400Hz,  带宽 47Hz
    BMI088_GYRO_BW_200_23HZ,    // ODR 200Hz,  带宽 23Hz
    BMI088_GYRO_BW_100_12HZ,    // ODR 100Hz,  带宽 12Hz
    BMI088_GYRO_BW_200_64HZ,    // ODR 200Hz,  带宽 64Hz
    BMI088_GYRO_BW_100_32HZ,    // ODR 100Hz,  带宽 32Hz
} BMI088_GyroBW_e;
```

### 4.3 自定义配置

在定义实例前，可以通过修改默认值来自定义配置：

```c
// 方式1：修改宏定义默认值
// 在 BMI088_INSTANCE_DEF 展开后修改结构体初始化值

// 方式2：注册后修改
BMI088_INSTANCE_DEF(imu, ...);

void AppInit(void)
{
    imu.acc_odr = BMI088_ACC_ODR_1600_HZ;     // 修改 ODR
    imu.acc_range = BMI088_ACC_RANGE_12G;     // 修改量程
    imu.gyro_range = BMI088_GYRO_RANGE_1000;  // 修改陀螺仪量程
    imu.target_temp = 50.0f;                  // 修改目标温度

    BMI088Register(&imu);
}
```

## 五、标定

### 5.1 在线标定

默认使用在线标定模式，注册时会自动执行标定：

```c
imu.cali_mode = BMI088_CALI_ONLINE;  // 默认值
BMI088Register(&imu);  // 会自动执行标定
```

**标定要求**：

- IMU 必须静止放置
- 标定过程约需 3 秒（6000 次采样）
- 运动会触发重新标定

### 5.2 预标定参数

如果已知标定参数，可以使用预标定模式：

```c
imu.cali_mode = BMI088_CALI_PRESET;
BMI088Register(&imu);
```

预标定参数通过宏定义配置：

```c
// 在 drv_bmi088.c 中定义
#define BMI088_PRE_GYRO_OFFSET_X 0.001f
#define BMI088_PRE_GYRO_OFFSET_Y 0.002f
#define BMI088_PRE_GYRO_OFFSET_Z -0.001f
#define BMI088_PRE_G_NORM 9.81f
```

## 六、温度控制

BMI088 支持加热器控制，用于保持 IMU 温度稳定：

```c
// 启用加热器（需在 bsp_cfg.h 中定义 TIM_HEATER）
imu.target_temp = 45.0f;  // 目标温度 45°C

// 注册后自动启用（如果定义了 TIM_HEATER）
BMI088Register(&imu);
```

## 七、完整示例

```c
#include "drv_bmi088.h"

// 定义实例
BMI088_INSTANCE_DEF(
    imu,
    SPI_BMI088,
    GPIO_BMI088_CS1,
    GPIO_BMI088_CS2,
    GPIO_BMI088_INT1,
    GPIO_BMI088_INT3,
    TIM_HEATER,
    NULL  // 不使用回调
);

// 初始化
void IMU_Init(void)
{
    // 自定义配置
    imu.acc_odr = BMI088_ACC_ODR_1600_HZ;
    imu.acc_range = BMI088_ACC_RANGE_24G;
    imu.gyro_range = BMI088_GYRO_RANGE_2000;
    imu.gyro_bw = BMI088_GYRO_BW_2000_532HZ;
    imu.target_temp = 50.0f;

    // 注册
    if (BMI088Register(&imu) != 0)
    {
        LOGERROR("BMI088 register failed");
    }
}

// 任务中获取数据
void IMU_Task(void)
{
    BMI088_Data_t data;
    if (BMI088GetData(&imu, &data))
    {
        // 使用数据...
        LOGINFO("Acc: %.2f, %.2f, %.2f m/s²",
                data.acc[0], data.acc[1], data.acc[2]);
        LOGINFO("Gyro: %.3f, %.3f, %.3f rad/s",
                data.gyro[0], data.gyro[1], data.gyro[2]);
        LOGINFO("Temp: %.1f °C", data.temp);
    }
}
```

## 八、注意事项

1. **初始化延时**：加速度计使能后需要等待至少 5ms，驱动已自动处理
2. **Dummy Byte**：加速度计 SPI 读取需要额外 1 字节 dummy，驱动已自动处理
3. **SPI 模式切换**：加速度计默认 I2C 模式，驱动会自动切换到 SPI 模式
4. **中断触发**：DMA 模式依赖加速度计中断触发读取，确保 INT 引脚正确连接
5. **标定**：首次使用建议在线标定，保存标定参数后可使用预标定模式
