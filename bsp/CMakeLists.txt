# bsp 层源文件
set(MODULE_SOURCES
    bsp_dwt/bsp_dwt.c
    bsp_gpio/bsp_gpio.c
    bsp_uart_log/bsp_uart_log.c
    bsp_math/bsp_math.c
    bsp_crc/bsp_crc.c
    bsp_spi/bsp_spi.c
    bsp_tim/bsp_tim.c
    bsp_usart/bsp_usart.c
    bsp_adc/bsp_adc.c
    bsp_can/bsp_can.c
    bsp_freertos/bsp_freertos.c
)

# bsp 层头文件路径
set(MODULE_INCLUDE_DIRS
    .
    bsp_dwt
    bsp_gpio
    bsp_uart_log
    bsp_math
    bsp_crc
    bsp_module
    bsp_spi
    bsp_tim
    bsp_usart
    bsp_adc
    bsp_can
    bsp_freertos
)
