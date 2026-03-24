/**
 * @file bsp_module.c
 * @brief 板载模块硬件资源索引实现
 *
 * @note 不同开发板需要修改本文件中的资源索引
 *       CubeMX重新生成后需确认引脚是否正确
 */

#include "bsp_module.h"

#if DEVELOPMENT_BOARD == STM32F407VET6

/*============================================
 *              BMI088 资源索引
 *============================================*/
const BMI088_Resource_t bmi088_resource[BMI088_INSTANCE_NUM] = {
    // STM32F407VET6 未配置 BMI088
};

/*============================================
 *              QFlash 资源索引
 *============================================*/
const QFlash_Resource_t qflash_resource[QFLASH_INSTANCE_NUM] = {
    // STM32F407VET6 未配置 QFlash
};

#endif // STM32F407VET6

#if DEVELOPMENT_BOARD == DM_MC02

/*============================================
 *              BMI088 资源索引
 *============================================*/
/**
 * @brief BMI088 硬件资源索引
 * @note BMI088 使用 SPI2 总线
 *       - 加速度计: CS1(PC0), INT1(PE10)
 *       - 陀螺仪:   CS2(PC3), INT3(PE12)
 */
const BMI088_Resource_t bmi088_resource[BMI088_INSTANCE_NUM] = {
    [0] = {
        .spi_accel = SPI_BMI088,       // 加速度计 SPI
        .spi_gyro = SPI_BMI088,        // 陀螺仪 SPI（共用）
        .cs_accel = GPIO_BMI088_CS1,   // 加速度计片选 PC0
        .cs_gyro = GPIO_BMI088_CS2,    // 陀螺仪片选 PC3
        .int_accel = GPIO_BMI088_INT1, // 加速度计中断 PE10
        .int_gyro = GPIO_BMI088_INT3,  // 陀螺仪中断 PE12
    },
};

/*============================================
 *              QFlash 资源索引
 *============================================*/
/**
 * @brief QFlash 硬件资源索引
 * @note QFlash 使用 OCTOSPI1 接口
 *       - CLK:  PB2
 *       - NCS:  PE11
 *       - IO0:  PB0
 *       - IO1:  PA3
 *       - IO2:  PA1
 *       - IO3:  PD11
 */
const QFlash_Resource_t qflash_resource[QFLASH_INSTANCE_NUM] = {
    [0] = {
        .octospi_instance = 1, // OCTOSPI1
        // 注意: OCTOSPI 引脚暂无对应 GPIO 枚举，需要在 CubeMX 配置
        // 这里预留索引，待后续扩展
        .cs_pin = GPIO_NUM_MAX,  // PE11（暂无枚举）
        .clk_pin = GPIO_NUM_MAX, // PB2（暂无枚举）
        .io0_pin = GPIO_NUM_MAX, // PB0（暂无枚举）
        .io1_pin = GPIO_NUM_MAX, // PA3（暂无枚举）
        .io2_pin = GPIO_NUM_MAX, // PA1（暂无枚举）
        .io3_pin = GPIO_NUM_MAX, // PD11（暂无枚举）
    },
};

#endif // DM_MC02

#if DEVELOPMENT_BOARD == DJI_A

/*============================================
 *              BMI088 资源索引
 *============================================*/
const BMI088_Resource_t bmi088_resource[BMI088_INSTANCE_NUM] = {
    // DJI_A 待配置
};

/*============================================
 *              QFlash 资源索引
 *============================================*/
const QFlash_Resource_t qflash_resource[QFLASH_INSTANCE_NUM] = {
    // DJI_A 待配置
};

#endif // DJI_A

#if DEVELOPMENT_BOARD == DJI_C

/*============================================
 *              BMI088 资源索引
 *============================================*/
const BMI088_Resource_t bmi088_resource[BMI088_INSTANCE_NUM] = {
    // DJI_C 待配置
};

/*============================================
 *              QFlash 资源索引
 *============================================*/
const QFlash_Resource_t qflash_resource[QFLASH_INSTANCE_NUM] = {
    // DJI_C 待配置
};

#endif // DJI_C
