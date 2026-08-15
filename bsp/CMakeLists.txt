# bsp 层源文件
set(MODULE_SOURCES
    bsp_assert/bsp_assert.c
    bsp_dwt/bsp_dwt.c
    bsp_gpio/bsp_gpio.c
    bsp_uart_log/bsp_uart_log.c
    bsp_math/bsp_math.c
    bsp_math/bsp_math_trig_lut.c
    bsp_crc/bsp_crc.c
    bsp_crc/bsp_crc_tables.c
    bsp_spi/bsp_spi.c
    bsp_tim/bsp_tim.c
    bsp_usart/bsp_usart.c
    bsp_adc/bsp_adc.c
    bsp_can/bsp_can.c
    bsp_freertos/bsp_freertos.c
    # usb开始
    bsp_usb/bsp_usb.c
    bsp_usb/core/bsp_usb_core.c
    bsp_usb/core/bsp_usb_ep.c
    bsp_usb/core/bsp_usb_ring.c
    bsp_usb/desc/bsp_usb_desc_dev.c
    bsp_usb/desc/bsp_usb_desc_cfg.c
    bsp_usb/desc/bsp_usb_desc_cdc.c
    bsp_usb/desc/bsp_usb_desc_str.c
    bsp_usb/cdc/bsp_usb_cdc.c
    # bsp_usb/hid/bsp_usb_hid_kbd.c  # TODO[HID停用]：Windows 复合设备 HID 枚举问题暂缓，恢复时取消注释
    # usb结束
)

# bsp 层头文件路径
set(MODULE_INCLUDE_DIRS
    .
    bsp_assert
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
    # usb开始
    bsp_usb
    bsp_usb/core
    bsp_usb/desc
    bsp_usb/cdc
    # bsp_usb/hid  # TODO[HID停用]：恢复时取消注释
    # usb结束
)
