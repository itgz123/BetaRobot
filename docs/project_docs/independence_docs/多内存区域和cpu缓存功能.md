stm32的内存区域如下：
```ld
STM32F407XX_FLASH.ld
MEMORY
{
RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 128K
CCMRAM (xrw)      : ORIGIN = 0x10000000, LENGTH = 64K
FLASH (rx)      : ORIGIN = 0x8000000, LENGTH = 1024K
}
STM32F427XX_FLASH.ld
MEMORY
{
RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 192K
CCMRAM (xrw)      : ORIGIN = 0x10000000, LENGTH = 64K
FLASH (rx)      : ORIGIN = 0x8000000, LENGTH = 2048K
}
STM32H723XG_FLASH.ld
MEMORY
{
DTCMRAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 128K
RAM_D1 (xrw)      : ORIGIN = 0x24000000, LENGTH = 320K
RAM_D2 (xrw)      : ORIGIN = 0x30000000, LENGTH = 32K
RAM_D3 (xrw)      : ORIGIN = 0x38000000, LENGTH = 16K
ITCMRAM (xrw)      : ORIGIN = 0x00000000, LENGTH = 64K
FLASH (rx)      : ORIGIN = 0x8000000, LENGTH = 1024K
}
```
`cubemx`默认`stm32f4`只用`RAM`不用`CCMRAM`
`cubemx`默认`stm32h7`只用`DTCMRAM`

关键问题：
1. `stm32h7`的`DTCMRAM`不支持`dma`，需要修改`ld`链接脚本和`s`启动文件
2. `stm32h7`的`ITCMRAM`需要修改`ld`链接脚本和`s`启动文件
3. `stm32h7`的`DCache`需要`mpu`内存保护单元，比较复杂，不开启
3. `stm32h7`的`ICache`只需要开启，其他什么都不需要管，开启

修改地方：
1. `ld`链接脚本
2. `s`启动文件

