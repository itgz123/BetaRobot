# 项目简介

test_my_frame项目是我的beta_robot框架的练手项目（也有可能直接改名），思路借鉴basic_framework和beta_flight。  
作者：TRW  
代码来源：basic_framework，stm，segger，freertos，wanghongxi

# 文档目录

**要求文档能够指导任何人通过这些不同的官方代码，不克隆这个项目复刻出这个项目，当然也怕我自己忘记**

| 文档                                                    | 说明                                   |
| ------------------------------------------------------- | -------------------------------------- |
| [工具链配置](.doc/工具链配置.md)                        | msys2、arm-none-eabi、openocd 安装配置 |
| [VSCode 开发环境](.doc/VSCode开发环境配置.md)           | 插件安装、tasks.json、launch.json      |
| [SEGGER RTT 使用指南](.doc/SEGGER_RTT使用指南.md)       | 移植、调试+日志模式、仅日志模式        |
| [CubeMX FreeRTOS 配置](.doc/CubeMX_FreeRTOS配置指南.md) | 时基源、内核、内存、任务配置           |
| [项目架构设计](.doc/项目架构设计方案.md)                | 分层架构、信息流、面向对象封装         |
| [Makefile 语法教程](.doc/Makefile语法教程.md)           | 变量、规则、函数详解                   |
| [代码规范](.doc/代码规范.md)                            | 命名、文件、注释规范                   |
| [BSP 开发指南](.doc/BSP开发指南.md)                     | 实例注册、回调机制、平台无关设计       |

### TODO:

- 冷门cubemx配置和代码库
  mpu内存保护
  ramecc内存纠错
  octospi高速spi用在外部flash
  dcmi摄像头
  computer：计算模块rcordic,crc,fmac（h7以外只有crc）
  dsp库，fpu运算库
  dwt计时器
- cmsis使用,自己封装freertos为cmsis风格
- 多种开发板同一套代码，只用修改app_cfg,bsp_cfg和makefile自动处理）

### 硬件连接

##### encoder

1. tim2:pb3,pa0
2. tim3:pb4,pb5
3. tim4:pd12,pd13
4. tim8:pc6,pc7

##### pwm_ch1234

1. tim1:pe9,pe11,pe13,pa11

##### uart

1. uart2:pa2,pa3

##### gpio

1. pb12,pb13,pb14,pb15
2. pc9,pc10,pc11,pc12

##### led

1. green:pe14,red:pa1

##### 表格：

| 电机 | encoder | pwm | gpio      |
| ---- | ------- | --- | --------- |
| 1    | tim2    | ch1 | pb12,pb13 |
| 2    | tim3    | ch2 | pb14,pb15 |
| 3    | tim4    | ch3 | pc9,pc10  |
| 4    | tim8    | ch4 | pc11,pc12 |

1. PA0-WKUP ------> TIM2_CH1
2. PB3 ------> TIM2_CH2
3. PB4 ------> TIM3_CH1
4. PB5 ------> TIM3_CH2
5. PD12 ------> TIM4_CH1
6. PD13 ------> TIM4_CH2
7. PC6 ------> TIM8_CH1
8. PC7 ------> TIM8_CH2
9. PE9 ------> TIM1_CH1
10. PE11 ------> TIM1_CH2
11. PE13 ------> TIM1_CH3
12. PA11 ------> TIM1_CH4
