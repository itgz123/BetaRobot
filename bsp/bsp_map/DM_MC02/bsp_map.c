#include "bsp_map.h"

void BSPInit()
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, GPIO_PIN_SET);
}

const GPIO_Map_t gpio_map[GPIO_NUM_MAX] = {
    [GPIO_BMI088_CS_ACCEL] = {GPIOC, GPIO_PIN_0},
    [GPIO_BMI088_CS_GYRO] = {GPIOC, GPIO_PIN_3},
    [GPIO_BMI088_INT_ACCEL] = {GPIOE, GPIO_PIN_10},
    [GPIO_BMI088_INT_GYRO] = {GPIOE, GPIO_PIN_12},
    [GPIO_POWER_24V_EN1] = {GPIOC, GPIO_PIN_14},
    [GPIO_POWER_24V_EN2] = {GPIOC, GPIO_PIN_13},
    [GPIO_POWER_5V_EN] = {GPIOC, GPIO_PIN_15},
    [GPIO_LCD_KEY1] = {GPIOA, GPIO_PIN_5},
    [GPIO_LCD_KEY2] = {GPIOD, GPIO_PIN_10},
    [GPIO_EX_KEY] = {GPIOA, GPIO_PIN_15},
    [GPIO_USER_KEY] = {GPIOE, GPIO_PIN_14},
};

const TIM_Map_t tim_map[TIM_NUM_MAX] = {
    [TIM_PWM_1] = {&htim2, TIM_CHANNEL_1},
    [TIM_PWM_2] = {&htim2, TIM_CHANNEL_3},
    [TIM_PWM_3] = {&htim1, TIM_CHANNEL_1},
    [TIM_PWM_4] = {&htim1, TIM_CHANNEL_3},
    [TIM_HEATER] = {NULL, 0}, /* 由 drv_bmi088 内部直接配置 */
    [TIM_BUZZER] = {&htim12, TIM_CHANNEL_2},
};

const UART_Map_t uart_map[UART_NUM_MAX] = {
    [UART_1] = {&huart1},
    [UART_SBUS] = {&huart5},
    [UART_RS485_2] = {&huart2},
    [UART_RS485_3] = {&huart3},
    [UART_7] = {&huart7},
    [UART_EX_8] = {&huart8},
    [UART_EX_9] = {&huart9},
    [UART_10] = {&huart10},
};

const CAN_Map_t can_map[CAN_NUM_MAX] = {
    [CAN_1] = {&hfdcan1},
    [CAN_2] = {&hfdcan2},
    [CAN_3] = {&hfdcan3},
};

/*============================================
 *              CAN 外设重配置表
 *============================================*/
/* 各 FDCAN 重配置结构体（覆盖 CubeMX 初始化）。
 * 警告：RxFifo0/1ElmtSize 与 TxElmtSize 当前为 8 字节 → 外设跑经典 CAN，
 *       无法收发 >8 字节的 FD 帧（配置了 tx_len>8 的实例会被 CANConfig 拒绝）。
 *       要实际跑 FD 帧，必须同步把 rx_fifo0_elmt_size / rx_fifo1_elmt_size /
 *       tx_elmt_size 改为 FDCAN_DATA_BYTES_64，且 FDCAN3 共三路共享 Message RAM，
 *       增大元素尺寸后需重新核算 message_ram_offset 是否溢出 2560 字。 */
static const HalCan_FDCAN_Config_s s_fdcan1_cfg = {
    .frame_format = FDCAN_FRAME_CLASSIC,
    .mode = FDCAN_MODE_NORMAL,
    .auto_retransmission = ENABLE,
    .transmit_pause = DISABLE,
    .protocol_exception = ENABLE,
    .nominal_prescaler = 12,
    .nominal_sync_jump_width = 1,
    .nominal_time_seg1 = 7,
    .nominal_time_seg2 = 2,
    .data_prescaler = 1,
    .data_sync_jump_width = 1,
    .data_time_seg1 = 1,
    .data_time_seg2 = 1,
    .message_ram_offset = 0, /* FDCAN1 */
    .std_filters_nbr = 16,
    .ext_filters_nbr = 16,
    .rx_fifo0_elmts_nbr = 16,
    .rx_fifo0_elmt_size = FDCAN_DATA_BYTES_8,
    .rx_fifo1_elmts_nbr = 16,
    .rx_fifo1_elmt_size = FDCAN_DATA_BYTES_8,
    .rx_buffers_nbr = 0,
    .rx_buffer_size = FDCAN_DATA_BYTES_8,
    .tx_events_nbr = 16,
    .tx_buffers_nbr = 16,
    .tx_fifo_queue_elmts_nbr = 16,
    .tx_fifo_queue_mode = FDCAN_TX_FIFO_OPERATION,
    .tx_elmt_size = FDCAN_DATA_BYTES_8,
};

static const HalCan_FDCAN_Config_s s_fdcan2_cfg = {
    .frame_format = FDCAN_FRAME_CLASSIC,
    .mode = FDCAN_MODE_NORMAL,
    .auto_retransmission = ENABLE,
    .transmit_pause = DISABLE,
    .protocol_exception = ENABLE,
    .nominal_prescaler = 12,
    .nominal_sync_jump_width = 1,
    .nominal_time_seg1 = 7,
    .nominal_time_seg2 = 2,
    .data_prescaler = 1,
    .data_sync_jump_width = 1,
    .data_time_seg1 = 1,
    .data_time_seg2 = 1,
    .message_ram_offset = 710, /* FDCAN2 */
    .std_filters_nbr = 16,
    .ext_filters_nbr = 16,
    .rx_fifo0_elmts_nbr = 16,
    .rx_fifo0_elmt_size = FDCAN_DATA_BYTES_8,
    .rx_fifo1_elmts_nbr = 16,
    .rx_fifo1_elmt_size = FDCAN_DATA_BYTES_8,
    .rx_buffers_nbr = 0,
    .rx_buffer_size = FDCAN_DATA_BYTES_8,
    .tx_events_nbr = 16,
    .tx_buffers_nbr = 16,
    .tx_fifo_queue_elmts_nbr = 16,
    .tx_fifo_queue_mode = FDCAN_TX_FIFO_OPERATION,
    .tx_elmt_size = FDCAN_DATA_BYTES_8,
};

static const HalCan_FDCAN_Config_s s_fdcan3_cfg = {
    .frame_format = FDCAN_FRAME_CLASSIC,
    .mode = FDCAN_MODE_NORMAL,
    .auto_retransmission = ENABLE,
    .transmit_pause = DISABLE,
    .protocol_exception = ENABLE,
    .nominal_prescaler = 12,
    .nominal_sync_jump_width = 1,
    .nominal_time_seg1 = 7,
    .nominal_time_seg2 = 2,
    .data_prescaler = 1,
    .data_sync_jump_width = 1,
    .data_time_seg1 = 1,
    .data_time_seg2 = 1,
    .message_ram_offset = 1420, /* FDCAN3 */
    .std_filters_nbr = 16,
    .ext_filters_nbr = 16,
    .rx_fifo0_elmts_nbr = 16,
    .rx_fifo0_elmt_size = FDCAN_DATA_BYTES_8,
    .rx_fifo1_elmts_nbr = 16,
    .rx_fifo1_elmt_size = FDCAN_DATA_BYTES_8,
    .rx_buffers_nbr = 0,
    .rx_buffer_size = FDCAN_DATA_BYTES_8,
    .tx_events_nbr = 16,
    .tx_buffers_nbr = 16,
    .tx_fifo_queue_elmts_nbr = 16,
    .tx_fifo_queue_mode = FDCAN_TX_FIFO_OPERATION,
    .tx_elmt_size = FDCAN_DATA_BYTES_8,
};

const HalCan_FDCAN_Config_s *can_cfg_map[CAN_NUM_MAX] = {
    [CAN_1] = &s_fdcan1_cfg,
    [CAN_2] = &s_fdcan2_cfg,
    [CAN_3] = &s_fdcan3_cfg,
};

const SPI_Map_t spi_map[SPI_NUM_MAX] = {
    [SPI_LCD_1] = {&hspi1},
    [SPI_BMI088] = {&hspi2},
};

const I2C_Map_t i2c_map[I2C_NUM_MAX] = {
    [I2C_LCD_2] = {&hi2c2},
};

const ADC_Map_t adc_map[ADC_NUM_MAX] = {
    [ADC_BAT] = {&hadc1, ADC_CHANNEL_4},
};

BSP_STATIC_ASSERT_MAP_SIZE(gpio_map, GPIO_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(tim_map, TIM_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(uart_map, UART_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(can_map, CAN_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(spi_map, SPI_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(i2c_map, I2C_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(adc_map, ADC_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(can_cfg_map, CAN_NUM_MAX);
