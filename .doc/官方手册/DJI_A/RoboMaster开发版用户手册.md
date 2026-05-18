# RoboMaster 开发板


---

V1.0
2018.04
用户手册

---

免责声明
产品使用注意事项
RoboMaster 开发板A 型
简  介
接口及外部丝印
特征参数
功能说明
RoboMaster 开发板B 型
简  介
接口及外部丝印
特征参数
功能说明
RoboMaster 开发板OLED
简  介
特征参数
功能说明
参考初始化代码
RoboMaster 开发板A 型 丝印及引脚定义图
RoboMaster 开发板B 型 丝印及引脚定义图
目  录

---

免责声明
感谢您购买RoboMaster
TM 开发板A 型（以下简称开发板A 型）、RoboMaster 开发板B 型（以
下简称开发板B 型）、RoboMaster 开发板OLED（以下简称OLED）。在使用之前，请仔细
阅读本声明，一旦使用，即被视为对本声明全部内容的认可和接受。请严格遵守手册、产品说
明和相关的法律法规、政策、准则安装和使用该产品。在使用产品过程中，用户承诺对自己的
行为及因此而产生的所有后果负责。因用户不当使用、安装、改装造成的任何损失，DJI
TM 将
不承担法律责任。
DJI 是深圳市大疆
TM 创新科技有限公司及其关联公司的商标。本文出现的产品名称、品牌等，
均为其所属公司的商标。本产品及手册为大疆创新版权所有。未经许可，不得以任何形式复制
翻印。
关于免责声明的最终解释权，归大疆创新所有。
产品使用注意事项
1. 请按照说明书正确连接线材，以免损坏接口以及开发板。
2. 使用前请检查线材有无老化、短路。老化或短路的线材不适合继续使用。
3. 请按照本文规定的工作环境（如电压、电流、温度等参数）使用，否则将会影响产品寿命
或造成永久性损坏。
4. 安装时注意做好保护，防止静电、物理损坏。
5. 请保持开发板的干净整洁，避免由于异物造成短路或性能下降。
6. 请不要用手直接接触开发板上的芯片，避免由于静电放电造成开发板损坏或性能下降。
7. 开发板上电后如发现有火花、冒烟，焦糊味或其它异常，请立即关掉电源。

---

23 1
2 23
1 22 21 201916
3 4 5
RoboMaster 开发板A 型
简  介
RoboMaster 开发板 A 型是一款面向机器人 DIY 的开源主控。开发板主控芯片为
STM32F427IIH6，拥有丰富的扩展接口和通信接口，板载 IMU，可配合 RoboMaster 出品的
M3508 直流无刷减速电机、UWB 模块以及妙算等产品使用，亦可配合 DJI 飞控 SDK 使用，
配件丰富。开发板具有防反接和缓启动等多重保护。经过 RoboMaster 竞赛的打磨和改进，开
发板不仅满足比赛机器人的控制需求，也非常适合用户 DIY。
接口及外部丝印
开发板A 型正面丝印及接口如下图所示：
序号
名称
丝印说明
备注
CAN1
H：CAN-H( 高位数据线)
L：CAN-L( 低位数据线)
可控电源输出接口
+：电源正极
未标注的一端为电源负极, 此
电源可以通过程序进行控制。
TF 卡槽
电压调节拨码
6.4V 7.4V 8.4V：
仅有对应位置ON 时的电压值
此拨码用于调节(8)PWM 的
电压，更多详细设置请见反
面丝印图。
SDK CAN2

---

CAN2
H：CAN-H( 高位数据线)
L：CAN-L( 低位数据线)
同步信号
G：GND、S：同步信号
PWM×8
G：GND 、+：VCC
：PWM 输出，A – H S-Z：IO
对应表索引
其中VCC 电压可以通过电压
调节拨码设置，IO 对应表见
反面丝印图。
USB
用户自定义LED×8
OLED 接口
DBUS
DBUS：DBUS 信号、+：VCC
G：GND
在连接设备时，请注意DBUS
端子上的突出部分方向与丝
印标注一致。
用户自定义按键
SWD
+：VCC(3.3V)、G：GND、
SWCLK：SWD 时钟、SIDIO：
SWD 数据
3.3V 电源输出接口
3V3：VCC(3.3V)、PGND：GND
UART
+：VCC、G：GND、T：数据发送
(Tx)、R：数据接收(Rx)
复位按键
用户自定义LED×2
SDK UART
G：GND、T：数据发送(Tx)
R：数据接收(Rx)、*：无连接
5V 电源输出接口
+：VCC
未标注引脚为GND。
12V 电源指示灯
蓝牙串口
T：数据发送(Tx)、R：数据接收(Rx)、
G：GND、+：VCC
PWM
G：GND 、*：无连接
：PWM 输出、△：1 号引脚
部分接口未完全标注，只标
注了1 号引脚。
电源输入接口
+：VCC
未标注引脚为GND。
 12V 电源输出接口×3
GPIO X 18 & 5V 电源
1，2，I-R：IO 对应表索引
IO 对应表见反面丝印图。

---

电源框图如下图所示：
2*8 路PWM供电
CAN通讯接口
串口通讯接口
XT30 供电
3.3 V 对外供电
电源输出
可调电源输出
D-Bus + Buzzer
3.3 V@250 mA
5 V@500 mA
IMU
MCU
LM25116
12 V@10 A
MP2233
3.3 V@3 A
TPS54540
TPS54540
MP2456GJ
LP5907MFX
24 V Power output @ Max 20 A
3.3 V@250 mA
LP5907MFX
5~12 V@5 A
5 V@2 A 5 V
PowerTree
24 V电源输入
防反接& 缓启动
通讯接口电源
特征参数
项目
参数
最大电压
26 V
支持电池
4~6S LiPo
最大允许输入电流 *（持续）
20 A
电源输出接口最大单路电流**
10A
重量
48 g
尺寸（长宽）
85×58 mm
工作温度范围
0 ~ 55 ℃
* 室温25℃、通风良好的实验环境下测得。
** 电源输出接口单路最大电流指每一路电源输出接口可承受的最大电流，但所有电源输出接
口的电流总和不得超过最大允许输入电流。
开发板A 型背面丝印如下图所示：
5.50
3.50
对应单片机IO
对应输出电压
正面丝印标注索引
电压调节拨码状态
对应单片机IO
正面丝印标注索引

---

1.2  DBUS* 接口
开发板A 型配备有1 路DBUS 接口, 下图是其接口原理图。
DBUS 是UART 信号的反相形式，因此从接插件的3 脚进入后，经过Q14 反相之后再送入单
片机的UART1。波特率一般设置为100kbps。
* DBUS 为DJI 遥控器通用协议
最大输出电压28V左右
电流20A
NC D14 开启过压防护
NC
BAT-
BAT-
PGND
PGND
VCC_INPUT
PGND
PGND
CAN1_L
[6,8]
CAN1_H
[6,8]
Q18
PMBT3906
J10
0.0
XT30PW-M
D17
BZX584C9V1
R87
1K
1%
R171
10K
1%
D13
SMAJ28CA
R82
33R
5%
Q1
TPCA8122
R169
100K
1%
D94
28V~32V
A
C
R172
1K
1%
D14
BZX584C9V1
R92
33R
5%
R173
1.5M
1%
C194
10nF
25V
C199
10nF
25V
R170
100K
1%
J9
1.0A
11257W90-2P-S
C41
100nF
50V
Q2
PSMN1R4-40YLD
电源和CAN输入
防反接，上电缓启动，过压保护
DBUS
VCC_5V_S
VCC_3V3_S
PGND
PGND
USART1_RX(PB7)
[3]
Q14
PMBT3904-215
R174
4.7K
J43
0.0A
R89
4.7K
2.54mm 3PIN单排弯头插针（90°）
功能说明
1.1  电源过压、防反接、缓启动电路
电源输入接口采用标准的XT30 接口，电源输入首先并联28V TVS 管，防止瞬态高电压烧坏开
发板，然后通过PMOS Q1 和NMOS Q2 组成缓启动与防反接电路，有效防止电源接头接触瞬
间打火，造成接头损坏，并且防止电源正负极反接对开发板造成损害。其中图中D19 为齐纳
二极管，当输入电压超过其击穿电压（30±2V）时，三极管Q3 导通，PMOS Q1 关断，该电
路起到过压保护的作用。

---

1.4  USART 接口
开发板A 型配备有四路USART 接口，分别连接到USART3、USART6、USART7 和
USART8。其中USART3 是配合DJI ON BOARD SDK 使用，线序与其它三个串口不同，在使
用串口时请注意TX 和RX 需要交叉连接。本接口只支持3.3V 和5V 电平，若需与RS485 或
RS232 接口通信，请外置电平转换芯片。
通讯接口
立式
PGND
USART3_RX(PD9)
[4]
USART3_TX(PD8)
[4]
J2
1.0A
11257W00-4P-S
C38
47pF
50V
L4
150mA
L3
150mA
D12
A
C
C37
47pF
50V
D11
A
C
On Board SDK
立式
PGND
VCC_5V_U
PGND
USART6_TX(PG14)
[4]
USART6_RX(PG9)
[4]
J36
1.0A
11257W00-4P-S
D76
A
C
D77
A
C
L63
150mA
L62
150mA
C104
47pF
50V
C105
47pF
50V
立式
PGND
VCC_5V_U
PGND
UART8_RX(PE0)
[4]
UART8_TX(PE1)
[4]
J37
1.0A
11257W00-4P-S
L64
150mA
C108
47pF
50V
L65
150mA
C109
47pF
50V
D79
A
C
D80
A
C
立式
PGND
VCC_5V_U
PGND
UART7_RX(PE7)
[4]
UART7_TX(PE8)
[4]
D83
A
C
J38
1.0A
11257W00-4P-S
L67
150mA
C123
47pF
50V
L68
150mA
C124
47pF
50V
D82
A
C
1.3  SWD 调试接口
开发板A 型配备一个SWD 调试接口，用于单片机程序的下载和调试。SWD 调试接口位于主
控板的右侧下方。SWCLK 和SWDIO 在开发板中串联了100Ω 的电阻，起到保护单片机的作用。
RFID
GND
VCC_3V3_S
GND
SWDIO
[3]
SWCLK
[3]
R4
100R
1%
R3
100R
1%
L1
1500mA
J1
卧式53261 接口，1脚在左侧

---

1.5  用户自定义LED×2
开发板A 型配备有2 颗用户自定义LED，分别为绿色和红色。每一个指示灯都由单片机的一
个引脚直接驱动，其配置低电平则点亮指示灯，配置高电平则指示灯熄灭。下表为指示灯的引
脚配置和参数。
用户自定义LED
颜色
单片机引脚
点亮电流
绿色
PF14
约4mA
红色
PE11
约4mA
1.6  用户自定义LED X 8
开发板A 型配备有8 颗绿色LED 灯珠，对应IO 为PG1-PG8，单颗点亮电流约4mA。
1.7  按键
开发板A 型配备两个按键，其中黑色按键为单片机复位按键，白色按键为用户自定义按键。
其中白色按键直接连到单片机的PB2 管脚，该按键按下为高电平。
1.8  BOOT 启动设置
开发板A 型上的STM32 芯片上有两个管脚BOOT0 和BOOT1，这两个管脚在芯片复位时的
电平状态决定了芯片复位后的启动方式。开发板A 型的BOOT 管脚配置如下图。默认情况下
BOOT 管脚均被拉低，上电从User Flash 启动。其中R1、R2、R17 和R18 焊盘位于两个按
键中间，R1 和R17 是空贴，用户可以使用镊子将其短接，使得单片机以不同的方式启动。当
BOOT0 = 1 BOOT1 = 0 时，单片机将从System memory 启动，进入DFU 模式。
NRST
VCC_3V3_S
50mA
SW1
R19
10K
1%
C13
1uF
6.3V
VCC_3V3_S
KEY(PB2)
[4]
S1
用户自定义按键
BOOT1
VCC_3V3_MCU
R2
10K
5%
R1
10K
5% NC
BOOT0
VCC_3V3_MCU
R18
10K
5%
R17
10K
5% NC

---

1.9  USB 接口
开发板A 型配备一个USB 全速接口，该端口完全符合USB2.0 规范的On-The-Go 补充标准，
在主机模式下。OTG_FS 支持全速（FS，12Mbps）和低速（LS，1.5Mbps）收发器，而从机
模式下仅支持全速（FS，12Mbps）收发器。
VCC_5V_S
USB_FS_DP(PA12)
[3]
USB_FS_DM(PA11) [3]
USB_FS_ID(PA10) [3]
C40
1uF
16V
D15
PESD12VV1BL
A
C
D18
PESD5V0F1BL
C
A
R38
22R
5%
L5INDUCTOR
1.4A
J14
1.0A
VBUS
DM
DP
ID
GND
SHELL1
SHELL2
SHELL3
SHELL4
SHELL5
SHELL6
U2
DLP11SN900HL2
C39
100nF
25V
D19
PESD5V0F1BL
C
A
R41
22R
5%
D20
PESD5V0F1BL
C
A
R42
22R
5%
USB接口
Boot mood selection pins
Boot mode
Aliasing
BOOT1
BOOT0
X
Main Flash Memory
Main Flash memory is selected as
the boot space
System memory
System memory is selected as the
boot space
Embedded SRAM
Embedded SRAM is selected as
the boot space
3V3
BOOT0
BOOT0
NRST
NRST
GND
GND
GND
3V3
BOOT1
BOOT1
GND
KEY(PB2)
VCC_3V3_S
VCC_3V3_S
KEY(PB2)

---

1.10  蜂鸣器
开发板A 型板载一个贴片式蜂鸣器，该蜂鸣器需要使用PWM 驱动，额定频率2700Hz。
1.11  5V 电源接口
开发板A 型集成一个可控的5V 电源接口，最大电流400mA，用户可以外接RoboMaster 红
点激光器，也可使用线材包里提供的线材连接其他设备。
Buzzer
VCC_5V_S
PGND
TIM12_CH1(PH6)
[4]
C50
10uF
25V
LS1
KLJ-8530A-5027
5V
A
C
B
D
R48
10K
Q4
NPN-1
1A
R47
510R
5%
D28
DIODE
A
C
PGND
VCC_5V_U
PGND
LASER(PG13)
[4]
R44
10K
Q3
NPN-1
1A
J15
1A
53398-0271
R43
510R
5%
R37
0R
5%
5V电源接口

---

1.13  IMU 模块
开发板A 型集成一个IMU 模块，其IMU 由MPU6500 陀螺仪和IST8310 地磁传感器组成。为
了解决陀螺仪温飘的问题，开发板在MPU6500 四周增加10 颗加热电阻，用户可以通过PB5
加热电阻控制管脚和MPU6500 内部的温度传感器做恒温处理，加热温度一般控制在比电路板
正常工作温度高15~20℃为宜。10 个加热电阻工作电压为24V，该电阻可以在1S 内将IMU
模块的温度从25℃加热到50℃。板载IST8310 的地址为：0x0E。为减少电源噪声对IMU 模
块的影响，板载IMU 模块采用独立的LDO 供电。
VCC_3V3_IMU
VCC_5V_S
R168
4.7K
5%
C196
100nF
25V 0402
U14
LP5907SNX-3.3/NOPB
OUT
GND
EN
IN
PAD
C195
4.7uF
6.3V
C197
4.7uF
6.3V
1.12  CAN2 通讯接口
开发板A 型为满足用户外接设备的需求，一共引出4 个CAN2 接口，其中一个（J3）线序与
另外三个（J11、J8 和J4）不相同。J3 是专门为DJI OnboardSDK 使用的通讯接口，其余三
个接口可以接RoboMaster UWB 定位系统以及其他CAN 通讯的模块。
立式
VCC_5V_U
PGND
PGND
CAN2_H
[6,8]
CAN2_L
[6,8]
J4
1.0A
11257W00-4P-S
CAN 接口
立式
VCC_5V_U
PGND
PGND
CAN2_H [6,8]
CAN2_L
[6,8]
J8
1.0A
11257W00-4P-S
CAN2 接口
立式
VCC_5V_U
PGND
PGND
CAN2_H
[6,8]
CAN2_L
[6,8]
J11
1.0A
11257W00-4P-S
CAN2 接口
立式
PGND
PGND
CAN2_H
[6,8]
CAN2_L
[6,8]
J3
1.0A
11257W00-4P-S

---

泄放电阻
AUX_DA
AUX_CL
AUX_DA
AUX_CL
VCC_3V3_IMU
VCC_3V3_IMU
IMU_INT(PB8)
[3]
SPI5_NSS(PF6)
[4]
SPI5_SCK(PF7)
[4]
SPI5_MISO(PF8)
[4]
SPI5_MOSI(PF9)
[4]
C51
10nF
10V
C52
100nF
25V
L16
150mA
C53
1uF
16V
U3
MPU6600
NC1
NC2
NC3
NC4
NC5
NC6
AUX_CL
VDDIO
AD0/SDO
REGOUT
FSYNC
INT
VDD
NC7
NC8
NC9
NC10
GND
RESV-FLOAT
RESV-GND
AUX_DA
CSn
SCL/SCLK
SDA/SDI
ePAD
C54
100nF
25V
R46
4.7K
L14
150mA
L13
150mA
L17
150mA
R49
4.7K
5%
L15
150mA
R50
4.7K
5%
IIC Address :0X0E
AUX_DA
AUX_CL
VCC_3V3_IMU
VCC_3V3_IMU
INT(PE3)
[4]
Set/Reset(PE2)
[4]
C57
100nF
25V
C56
4.7uF
10V
U4
IST8310
SCL
AVDD
NC1
NC2
CAD0
CAD1
VPP
NC3
GND1
C1
GND2
NC
DVDD
RSTN
DRDY
SDA
C55
100nF
25V
VCC_INPUT
PGND
Heat_PWM(PB5)
[3]
R52
10K
5%
R55
10K
5%
R59
10K
5%
R61
100R
1%
R57
10K
5%
R54
10K
5%
R51
10K
5%
R58
10K
5%
Q5
BSS138LT1G
0.5~1.5V
R56
10K
5%
R53
10K
5%
R60
10K
5%
R62
10K
XXX
5%

---

1.15  用户自定义接口
为增强开发板A 型的适用性，板载18 个用户自定义接口，以2.54mm 排针的形式引出，其原
理图如下图所示。
PGND
VCC_5V_U
DAC_OUT2(PA5)
[3]
ADC1_IN14(PC4)
[3]
ADC1_IN15(PC5)
[3]
I2C2_SCL(PF1)
[4]
ADC1_IN13(PC3)
[3]
ADC1_IN12(PC2)
[3]
SPI4_MISO(PE5)
[4]
SPI4_MOSI(PE6)
[4]
PF10
[4]
I2C2_SDA(PF0)
[4]
SPI4_SCK(PE12)
[4]
DAC_OUT1(PA4)
[3]
SPI4_NSS(PE4)
[4]
ADC1_IN9(PB1)
[3]
ADC1_IN8(PB0)
[3]
ADC1_IN10(PC0)
[3]
ADC1_IN11(PC1)
[3]
DAC_EXTI9(PI9)
[5]
C95
47pF
50V
150mA
D67
A
C
C85
47pF
50V
150mA
C91
47pF
50V
150mA
D65
A
C
C96
47pF
50V
150mA
D68
A
C
C90
47pF
50V
D58
A
C
150mA
150mA
D64
A
C
C97
47pF
50V
D69
A
C
C89
47pF
50V
D63
A
C
C98
47pF
50V
D70
A
C
150mA
C88
47pF
50V
C93
47pF
50V
150mA
D62
A
C
C99
47pF
50V
150mA
C87
47pF
50V
D71
A
C
150mA
150mA
D61
A
C
C100
47pF
50V
D66
A
C
D72
A
C
C102
47pF
50V
C86
47pF
50V
150mA
D60
A
C
C101
47pF
50V
J34
3A
2*10pin 180°直排针
D73
A
C
150mA
L56
L58
L50
L52
L44
L46
L48
L54
L60
150mA
D75
A
C
D59
A
C
C94
47pF
50V
D74
A
C
C92
47pF
50V
150mA
用户自定义接口
150mA
150mA
150mA
L55
L57
L49
L61
L59
L51
L53
L45
L47
1.14  TF 卡接口
开发板A 型配备一个TF 卡接口，用户可以根据自己的需求存储一些调试数据，其原理图如下
图所示。
Micro_SD
VCC_3V3_S
PGND
PGND
PGND
SDIO_CMD(PD2)
[4]
SDIO_CK(PC12)
[3]
SDIO_D2(PC10)
[3]
SDIO_D3(PC11)
[3]
SDIO_D0(PC8)
[3]
SDIO_D1(PC9)
[3]
SD_EXTI(PE15)
[4]
R29
22R
0.05
R22
10K
R30
22R
0.05
R23
10K
R31
22R
0.05
R28
22R
0.05
R24
10K
R32
22R
0.05
R25
10K
J6
MicroSD 47352-1001
DAT2
DAT3
CMD
VSS1
VDD
CLK
DAT0
DAT1
CDSW
D1
GND1
G1
GND3
G3
GND2
G2
GND4
G4
R26
10K
R27
22R
0.05
C36
100nF
25V
SD卡接口

---

1.16  PWM 接口
开发板A 型为方便用户使用PWM 驱动的舵机等执行器，引出16 路PWM，并为这16 路
PWM 提供5A 驱动能力的电源。PWM 供电电压默认输出5V，用户可以根据实际需求调整3
位拨码配置不同的输出电压。PWM 原理图和电源配置表如下图所示。
串联电阻
22.75
13.3
9.4
拨码序号
输出电压
OFF
OFF
OFF
4.9875
ON
OFF
OFF
6.401126
OFF
ON
OFF
7.405545
OFF
OFF
ON
8.408777
ON
ON
ON
12.24
ON
ON
OFF
8.819
OFF
ON
ON
10.85
ON
OFF
ON
9.82
5V@5A
环路补偿
(12~24)V->5V
0.8V
Imax =5A
fclk = 400kHz
NC
DC-DC BUCK
FB=0.8V
FB_1
FB_1
VCC_INPUT
VCC_5V_ADJ
PGND
PGND
PGND
PGND
PGND
PGND
PGND
R120
68K
0.01
R119
17.8K
0.01
TP_3
C122
4.7nF
50V
C117
22uF
25V
R124
13.3K
1%
5A
U7
BOOT
VIN
EN
RT/CLK
FB
COMP
GND
SW
EP
C113
4.7uF
50V
R167
1.2K
1%
C184
22uF
25V
TP_4
R116
0R 0402
XXX
0.05
C118
22uF
25V
R165
750R
1%
C116
22uF
25V
C110
100nF
50V
C114
4.7uF
50V
R118
100R
1%
C120
100pF
D81
SVM860VB
8A
C112
4.7uF
50V
C115
100nF
25V
R121
243K
0.01
R117
40.2K
XXX
1%
C111
4.7uF
50V
1 2 3
ON
KE
U8
DSHP03TSGER
25mA
C119
470pF
50V
C185
22uF
25V
R115
309K
0.01
R166
8.2K
1%
R123
22K
1%
L66
6.8uH 4.5A
C121
47pF
50V
R122
7.68K
1%
并排放置
PGND
VCC_5V_ADJ
TIM8_CH4(PI2)
[5]
TIM8_CH3(PI7)
[5]
TIM8_CH2(PI6)
[5]
TIM2_CH1(PA0)
[3]
TIM8_CH1(PI5)
[5]
TIM2_CH2(PA1)
[3]
TIM2_CH3(PA2)
[3]
TIM2_CH4(PA3)
[3]
J31
3A
8pin 180°直排针蓝色
C79
47pF
50V
R99
100R
1%
L34
150mA
D56
A
C
C78
47pF
50V
R95
100R
1%
L40
150mA
D57
A
C
D55
A
C
L36
150mA
R101
100R
1%
D47
A
C
R98
100R
1%
L41
150mA
C82
47pF
50V
C81
47pF
50V
J30
3A
8pin 180°直排针
L37
150mA
D51
A
C
R103
100R
1%
C80
47pF
50V
L42
150mA
D50
A
C
R97
100R
1%
C75
47pF
50V
J29
3A
8pin 180°直排针红色
R102
100R
1%
D49
A
C
L38
150mA
C74
47pF
50V
R96
100R
1%
D48
A
C
L35
150mA
C76
47pF
50V
8路PWM 输出

---

1.18  同步信号接口
为了同步外接模块时序，开发板A 型配备一个5V 同步信号接口，该接口可以根据PB14 和
PB15 的相关配置，来决定单发同步信号、接收同步信号以及收发同步信号。其电路图如下所示。
1.17  OLED 接口
为方便用户调试和显示部分参数，开发板A 型配备OLED 接口，用户可以购买OLED 模块使用，
详细使用方法可以参见开发板OLED 功能说明。
卧式
VCC_3V3_S
PGND
PGND
BUTTON_AD(PA6) [3]
OLED_RST(PB10)
[3]
OLED_DC(PB9)
[3]
OLED_SCLK(PB3)
[3]
OLED_MOSI(PA7) [3]
C46
47pF
50V
D23
A
C
J17
1.0A
11257W90-7P-S
C47
47pF
50V
L6
150mA
L9
150mA
D24
A
C
C43
47pF
50V
D25
A
C
D21
A
C
L10
150mA
L7
150mA
C44
47pF
50V
L8
150mA
C45
47pF
50V
D22
A
C
OLED&按键
PGND
PGND
VCC_5V_U
VCC_3V3_S
SNYCHOR(PB14)
[3]
SNYCHOR(PB15)
[3]
R107
10K
0402 5%
C106
47pF
50V
R105
22R
0.05
R106
10K
0402 5%
R111
0R
5%
Q15
BSS138LT1G
0.5~1.5V
D78
A
C
J42
1.0A
11257W90-2P-S
5V 同步信号

---

1.19  四路可控电源输出接口
开发板A 型有四个可控电源输出接口，每一个电源接口都由一个PMOS 来控制，4 路总电流
不能超过20A，且单路电流不能超过10A。其原理图如下图所示：
20A走线
PGND
VCC_OUT1
VCC_OUT1
VCC_INPUT
PGND
POWER1_CTRL(PH2)
[4]
C59
47pF
50V
Q6
TPCA8122
?2V~?3V
+
-
J20
15.0A
XT30UPB-F
C
A
D30
BZX584C9V1
R68
1K
1%
L19
150mA
R70
10K
1%
R66
10K
XXX
5%
Q9
PMBT3904-215
R64
10K
XXX
5%
20A走线
VCC_OUT2
VCC_INPUT
VCC_OUT2
PGND
PGND
POWER2_CTRL(PH3)
[4]
Q10
TPCA8122
?2V~?3V
Q13
PMBT3904-215
R78
10K
1%
L21
150mA
R76
1K
1%
R74
10K
XXX
5%
D32
BZX584C9V1
R72
10K
XXX
5%
+
-
J22
15.0A
XT30UPB-F
C
A
C60
47pF
50V
20A走线
VCC_INPUT
VCC_OUT3
PGND
VCC_OUT3
PGND
POWER3_CTRL(PH4)
[4]
R69
10K
1%
+
-
J19
15.0A
XT30UPB-F
C
A
Q7
TPCA8122
?2V~?3V
L18
150mA
R65
10K
XXX
5%
R67
1K
1%
Q8
PMBT3904-215
D29
BZX584C9V1
R63
10K
XXX
5%
C58
47pF
50V
20A走线
VCC_OUT4
VCC_INPUT
PGND
VCC_OUT4
PGND
POWER4_CTRL(PH5)
[4]
R77
10K
1%
D31
BZX584C9V1
Q12
PMBT3904-215
R71
10K
XXX
5%
R75
1K
1%
+
-
J21
15.0A
XT30UPB-F
C
A
C61
47pF
50V
Q11
TPCA8122
?2V~?3V
L20
150mA
R73
10K
XXX
5%

---

1.20  12V 电源输出接口
开发板A 型配备一个输出电压12V，最大输出电流10A* 的DC-DC 电源, 当负载电流达到
10A 后，若继续加重负载，会导致电压下降甚至造成永久损坏。该电源具备过流保护功能，
保护动作电流为22A，如果用户用来给电机供电，请注意电机如进行急加速急减速等操作时，
会导致短时的电流增大超过保护电路动作电流值，故请缓慢启动电机，以免造成电机无法正
常启动。
* 室温25℃、通风良好的实验环境下测得。
f=250KHz
Vin>12.5V
开始转换
(15~24)V->12V
VCC
VCC
PGND
PGND
PGNDPGND
PGND
VCC_12V
PGND
VCC_12V
PGND
PGND
PGND
PGND
PGND
VCC_INPUT
VCC_INPUT
PGND
VCC_12V
VCC_12V
PGND
PGND
D88
DIODE TVS BI-DIR
A
C
R139
0R
R142
0R
XXX
R134
4.7R
1%
R143
4.7K
R129
4.7R
C134
10nF
C143
3.3nF
C141
1uF
50V
C191
22uF
25V
LM25116MHX/NOPB
U9
VIN
UVLO
RT/SYNC
EN
RAMP
AGND
SS
FB
COMP
VOUT
DEMB
CS
CSG
PGND
LO
VCC
VCCX
HB
HO
SW
EP
D84
DIODE
A
C
R138
10K
5%
C142
100nF
50V
R130
10K
5%
C139
22uF
25V
TP_5
R133
17.8K
C130
1uF
50V
C137
22uF
25V
R137
0R
C147
330pF
R128
100K
1%
R141
4mR
1206 1%
Q16
BSC067N06LS3
1.7V
C144
100pF
R132
511K
C132
1uF
16V
R131
12.4K
C193
22uF
25V
D87
DIODE
A
C
TP_6
C145
1uF
R135
1.21K
C140
22uF
25V
R140
10.7K
D85
DIODE
A
C
R136
10R
5%
L69
10uH
13.8*12.6*6.0mm
10A
C126
4.7uF
50V
C129
100nF
50V
+
C125
22uF
35V
C
A
+
C135
22uF
35V C
A
C133
270pF
C138
22uF
25V
C128
1uF
50V
C131
1uF
50V
C192
22uF
25V
D89
30mA
Q17
PSMN1R8-40YLC
D86
DIODE
A
C
C127
4.7uF
50V
R127
1M
1%
C146
470pF
C136
22uF
25V
1.21  预留蓝牙模块接口
为方便用户调试，开发板A 型含有一个蓝牙接口，用户可以根据实际的需求自行使用蓝牙模块。
该接口使用UART2，其原理图如下所示。
VCC_5V_U
PGND
USART2_RX(PD6)
[4]
USART2_TX(PD5)
[4]
C49
47pF
50V
C48
47pF
50V
L12
150mA
D26
A
C
D27
A
C
J18
SIP4-2P54MM
4Pin排针
L11
150mA
预留蓝牙串口

---

RoboMaster 开发板B 型
简  介
RoboMaster 开发板 B 型专为传感器和执行部件设计，可配合RoboMaster 开发板 A 型使用，
完成复杂机器人的开发。该开发板主控芯片为 STM32F105R8T6，接口丰富、结构紧凑，支持
电磁阀等控制。拥有四路光耦隔离保护接口，可搭配 RoboMaster OLED 模块使用。
接口及外部丝印
12 11 10

---

序号名称
丝印说明
备注
电源输出接口
+：VCC
未标注一端为GND。
复位按键
OLED 接口
用户自定义LED X 2
CAN2
H：CAN-H( 高位数据线)
L：CAN-L( 低位数据线)
G：GND、+:VCC
传感器指示灯
S1-S4：对应4 个传感器指示灯
UART
+：VCC、G：GND
T：数据发送(Tx)
R：数据接收(Rx)
传感器接口
+：VCC、G：GND、
：传感器信号
PWM X 4
G：GND、+：VCC
：PWM 信号
DBUS
CAN1
H：CAN-H( 高位数据线)
L：CAN-L( 低位数据线)
电源输入接口
+：VCC
未标注一端为GND。
SWD
+：VCC(3.3V)、G：GND、
SWCLK：SWD 时钟、SIDIO：
SWD 数据
GPIO X 4
可控电源输出接口
+：VCC、G：GND
电源框图如下图所示：
3.3 V@250 mA
TPS54540
5 V@5 A
LP5907MFX
MCU
PWM&5 V POWER SUPPLY
24 V PowerOutput MAX@16 A
PowerTree
电源输出
防反接
24 V电源输入

---

特征参数
项目
参数
最大电压
26 V
支持电池
3-6S LiPo
最大工作电流 *（持续）
20 A
电源输出接口最大单路电流**
10A
重量
24 g
尺寸（长宽）
60*46 mm
工作温度范围
0 ~ 55 ℃
* 室温25℃、通风良好的实验环境下测得。
** 电源输出接口单路最大电流指每一路电源输出接口可承受的最大电流，但所有电源输出接
口的电流总和不得超过最大允许输入电流。
功能说明
2.1  电源防反接电路
电源输入接口采用标准的XT30 接口，在正极线路上串有一个PMOS，该MOS 可以实现防反
接的作用，其原理图如下所示。
VCC_INPUT
R68
10K
5%
Q10
TPCA8122
J30
0.0
XT30PW-M
D20
R67
10K
5%

---

2.2  DBUS 接口
开发板B 型配备有1 路DBUS 接口, 下图是其接口原理图。
DBUS 是UART 信号的反相形式，因此从接插件的3 脚进入后，经过Q9 反相之后再送入单片
机的UART4。波特率一般设置为100kbps。
DBUS
2.54mm间距插针
VCC_3V3
VCC_5V
DBUS_RX
[2]
R54
4.7K
5%
J9
2.54MM 3PIN
R45
4.7K
5%
Q9
PMBT3904-215
2.3  SWD 调试接口
开发板B 型配备一个SWD 调试接口，用于单片机程序的下载和调试。SWD 调试接口位于主
控板的右侧下方。SWCLK 和SWDIO 在开发板中串联了100Ω 的电阻，起到保护单片机的作用。
立式式53261 接口，1脚在左侧
GND
VCC_3V3
GND
SWDIO
[2]
SWCLK
[2]
R1
100R
1%
J1
1A
533980471
L1
1500mA
R2
100R
1%

---

2.4  USART 接口
开发板B 型配备有两路USART 接口，分别连接到USART2、USART3。在使用串口时请注意
TX 和RX 的交叉连接，以免造成无法通讯。本接口只支持3.3V 和5V 电平，若需与RS485 或
RS232 接口通信，请外置电平转换芯片。
 卧式
VCC_5V
VCC_5V
USART2_RX
[2]
USART2_TX
[2]
USART3_RX
[2]
USART3_TX
[2]
R66
0R
5%
NC
J20
11257W90-4P-S
11257W90-4P-S
R65
0R
5%
NC
J16
11257W90-4P-S
11257W90-4P-S
2.5  用户自定义LED
开发板B 型配备有2 颗LED 指示灯。每一个LED 都由单片机的一个引脚直接驱动，其配置低
电平则点亮LED，配置高电平则LED 熄灭。给出了LED 的引脚配置和参数。
LED 引脚配置
颜色
单片机引脚
IO 类型
点亮电流
绿色
PB0
5V 容忍
约4mA
红色
PB1
5V 容忍
约4mA

---

2.6  按键
复位按键
NRST
VCC_3V3
R5
10K
1%
50mA
SW1
C5
1uF
10V
2.7  BOOT 启动设置
开发板B 型上的STM32 芯片上有两个管脚BOOT0 和BOOT1，这两个管脚在芯片复位时的
电平状态决定了芯片复位后的启动方式。开发板B 型的BOOT 管脚配置如下图所示。默认情
况下BOOT 管脚均被拉低，上电从User Flash 启动。其中R11 和R14 是空贴器件，用户可以
使用镊子将其短接，使得单片机以不同的方式启动。
Boot mood selection pins
Boot mode
Aliasing
BOOT1
BOOT0
X
Main Flash Memory
Main Flash memory is selected as
the boot space
System memory
System memory is selected as the
boot space
Embedded SRAM
Embedded SRAM is selected as
the boot space
BOOT1
BOOT0
VCC_3V3
VCC_3V3
R11
10K
5%
NC
R13
10K
5%
R14
10K
5% NC
R12
10K
5%

---

2.7  CAN 接口
开发板B 型为满足用户外接设备的需求，引出两路CAN，CAN1 配置为PB8 和PB9 管脚，
CAN2 配置为PB12 和PB13 管脚。
2.8  用户自定义接口
为增强开发板的适用性，板载4 个用户自定义接口，以2.54mm 排针的形式引出，其原理图
如下图所示。
PC0
PC1
PC2
PC3
J33
0.0A
2.54mm 4PIN 单排直插针
2.9  PWM 接口
开发板B 型为方便用户使用PWM 驱动的舵机等执行器，引出4 路PWM。
YC.DZ.S00743
TIM1_CH1(PA8)
2.54mm间距插针
2.54mm间距插针
TIM1_CH2(PA9)
2.54mm间距插针
TIM1_CH3(PA10)
TIM1_CH4(PA11)
2.54mm间距插针
4路PWM 输出
VCC_5V
VCC_5V
VCC_5V
VCC_5V
TIM1_CH1
[2]
TIM1_CH2
[2]
TIM1_CH3
[2]
TIM1_CH4
[2]
J27
2.54MM 3PIN
J28
2.54MM 3PIN
J29
2.54MM 3PIN
J26
2.54MM 3PIN

---

2.10  OLED 接口
为方便用户调试和显示部分参数，开发板B 型配备OLED 接口，用户可以购买OLED 模块使用，
详细使用方法可以参见开发板OLED 功能说明。
OLED 接口
VCC_3V3
OLED_DC
[2]
OLED_RST
[2]
SPI1_SCK
[2]
SPI1_MOSI
[2]
Button_AD
[2]
C27
47pF
50V
D11
A
C
L19
150mA
L18
150mA
J31
1.0A
11257W90-7P-S
D9
A
C
C25
47pF
50V
C28
47pF
50V
L20
150mA
D7
A
C
D10
A
C
C26
47pF
50V
L21
150mA
D8
A
C
C29
47pF
50V
L22
150mA
2.11  传感器接口
为满足用户需要接高压信号（大于5V）开关量传感器的需求，本开发板集成了四路带光耦隔
离的接口，其中有两路是高电平有效，另外两路是低电平有效，用户可以根据选用的传感器自
行安装。另外也可以根据原理图修改0ohm 电阻自行配置高低电平。其原理图如下所示。每一
路传感器接口型号为XH2.54 插座，每个接口最大可提供2A 电流。
默认低电平有效，实际使用可以自行配置电阻。
S_INPUT1_L
S_INPUT2_L
S_INPUT1_H
S_INPUT2_H
S_INPUT3_H
S_INPUT4_H
S_INPUT4_L
S_INPUT3_L
S_INPUT1_H
S_INPUT1_L
S_INPUT2_H
S_INPUT2_L
S_INPUT3_H
S_INPUT3_L
S_INPUT4_H
S_INPUT4_L
VCC_3V3
VCC_3V3
VCC_3V3
VCC_3V3
VCC_INPUT
VCC_INPUT
VCC_INPUT
VCC_INPUT
VCC_INPUT
VCC_INPUT
VCC_INPUT
VCC_INPUT
S_OUT1
[2]
S_OUT2
[2]
S_OUT3
[2]
S_OUT4
[2]
J8
3.0A
R57
1K
04020.05
R42
0R
5%
R53
10K
5%
R63
0R
NC
R64
0R
D18
30mA
红
R52
1K
04020.05
R56
0R
NC
R59
0R
5%
R47
10K
5%
R39
0R
NC
R44
0R
NC
R41
0R
5%
D19
30mA
红
R40
0R
5%
R46
1K
0.05
D16
30mA
红
R51
10K
5%
R49
0R
5%
R58
10K
5%
R62
0R
5%
NC
R60
0R
NC
R55
0R
5%
U4
TLP291-4
J10
3.0A
R61
0R
0603 5%
R48
0R
NC
R43
0R
NC
R50
1K
04020.05
D17
30mA
红
J7
3.0A
WAFER2.5-1X3P
J11
3.0A
四路传感器输入接口

---

2.12  四路可控电源输出接口
开发板B 型有四个电源输出接口，每一个电源接口都由一个PMOS 来控制是否输出电源。其
原理图如下图所示。每一路可输出最大电流为2A，插座型号为XH2.54。
VCC_INPUT
VCC_OUT1
VCC_INPUT
VCC_OUT2
VCC_INPUT
VCC_OUT3
VCC_INPUT
VCC_OUT4
VCC_OUT1
VCC_OUT2
VCC_OUT3
VCC_OUT4
POWER1_CTRL
[2]
POWER2_CTRL
[2]
POWER3_CTRL
[2]
POWER4_CTRL
[2]
J3
3.0A
WAFER2.5-1X2P
R35
10K
0402 5%
L24
150mA
Q6
PMBT3904-215
R37
1K
0.05
J5
3.0A
WAFER2.5-1X2P
L26
150mA
R21
10K
0402 5%
Q1
TPCA8122
C35
47pF
50V
R25
1K
0.05
R34
10K
0402 5%
R30
10K
0402 5%
R19
1K
0.05
L25
150mA
R18
10K
0402 5%
J4
3.0A
WAFER2.5-1X2P
Q3
TPCA8122
R20
10K
1%
R29
10K
0402 5%
Q7
TPCA8122
C30
47pF
50V
R27
10K
1%
R17
10K
0402 5%
Q4
PMBT3904-215
R31
1K
0.05
J6
3.0A
WAFER2.5-1X2P
L23
150mA
Q8
PMBT3904-215
R33
10K
1%
Q2
PMBT3904-215
C31
47pF
50V
C37
47pF
50V
Q5
TPCA8122
R38
10K
1%
R22
10K
0402 5%
4路可控电源输出

---

RoboMaster 开发板OLED
简  介
RoboMaster 开发板 OLED 专 为 RoboMaster 开发板 A 型 和RoboMaster 开发板 B 型设计。
OLED 带有一个 0.96 英寸，分辨率为 128 × 64 的 OLED 屏幕以及一个五维按键。OLED 采
用SPI 通讯的方式，其驱动芯片为 SH1106G。
接口示意图
1. 7-Pin 接口，如图所示从左到右线序分别为：
BUTTON AD、SPI2 MOSI、SPI2-SCK、OLED-DC、OLED-RST、GND、VCC-3V3。
2. OLED 屏幕。
3. 五维按键安装孔：将包装内的五维按键按压至该孔进行固定。安装后，五维按键支持上、下、
左、右及中间五个方向的操作，查看五维按键说明章节了解详细信息。
特征参数
项目
参数
额定电压
3.3V
最大工作电流*
10mA
OLED 通信接口
SPI
按键
五维键
重量
9 g
尺寸（长宽）
37×45 mm
屏幕分辨率
128×64
屏幕尺寸
0.96 英寸
工作温度范围
0 ~ 50 ℃
* 室温25℃、通风良好的实验环境下测得。

---

功能说明
OLED 模块采用SPI 通讯的方式，其驱动芯片为SH1106G。扩展OLED 模块的通用性，该模
块集成一个五向按键，为节省信号线，五个按键共用一个管脚，采用AD 采集键值，以分压的
方式，将五个按键五等分，其参考分压表如下表所示。
按键
阻值
电压
AD 参考值(12bit)
中间
左
26.1
0.683029
847.7843
右
73.2
1.394688
1731.104
上
1.98
2457.6
下
2.642629
3280.064
100K
Button_AD
Button_AD
Button_AD
Button_AD
VCC_3V3
R3
309K
1%
R5
10K
1%
R2
33K
1%
R4
0R
5%
R6
10M
1%
R7
100K
1%
C7
100nF
25V
R8
0R
5%
J3
0.0
A
CEN
C
B
COM
D
MT1
MT2
MT3
MT4

---

参考初始化代码
void InitOLED_MASTER_SH1106G(void)
{
Write_Command(0xAE); //DOT MARTIX DISPLAY OFF
Write_Command(0x32); //SET PUMP VOLTAGE 8v
Write_Command(0x40); //SET DISPLAY START LINE(40H-7FH)
Write_Command(0x81); //CONTARST CONTROL(00H-0FFH)
Write_Command(CONTRAST);
Write_Command(0xA1); //SET SEGMENT RE-MAP(0A0H-0A1H)
Write_Command(0xA4); //ENTIRE DISPLAY OFF(0A4H-0A5H)
Write_Command(0xA6); //SET NORMAL DISPLAY(0A6H-0A7H)
Write_Command(0xA8); //SET MULTIPLEX RATIO 64
Write_Command(0x3F);
Write_Command(0xAD); //SET DC/DC BOOSTER(8AH=OFF,8BH=ON)
Write_Command(0x8B);
Write_Command(0xC8); //COM SCAN COM1-COM64(0C8H,0C0H)
Write_Command(0xD3); //SET DISPLAY OFFSET(OOH-3FH)
Write_Command(0x00);
Write_Command(0xD5); //SET FRAME FREQUENCY
Write_Command(0x80);
Write_Command(0xD9); //SET PRE_CHARGE PERIOD
Write_Command(0x1F);
Write_Command(0xDA); //COM PIN CONFIGURATION(02H,12H)
Write_Command(0x12);
Write_Command(0xDB); //SET VCOM DESELECT LEVEL(35H)
Write_Command(0x40);
Write_Command(0xAF); //DSPLAY ON
}

---

RoboMaster 开发板A 型 丝印及引脚定义图

---

RoboMaster 开发板B 型 丝印及引脚定义图
（正面）

---

（背面）

---

Copyright © 2018 大疆创新 版权所有
WWW.ROBOMASTER.COM
