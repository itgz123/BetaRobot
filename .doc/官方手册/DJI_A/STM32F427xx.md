# 32b Arm® Cortex®-M4 MCU+FPU, 225DMIPS, up to 2MB flash/256+4KB RAM, USB


---

This is information on a product in full production.
February 2026
32b Arm® Cortex®-M4 MCU+FPU, 225DMIPS, up to 2MB flash/256+4KB RAM, USB
OTG HS/FS, Ethernet, 17 TIMs, 3 ADCs, 20 com. interfaces, camera and LCD-TFT
Datasheet - production data
Features
•
Includes ST state-of-the-art patented
technology.
•
Core: Arm® 32-bit Cortex®-M4 CPU with FPU,
Adaptive real-time accelerator (ART
Accelerator™) allowing 0-wait state execution
from flash memory, frequency up to 180 MHz,
MPU, 225 DMIPS/1.25 DMIPS/MHz
(Dhrystone 2.1), and DSP instructions
•
Memories
–
512 bytes of OTP memory
–
Up to 2 MB of flash memory organized into
two banks allowing read-while-write
–
Up to 256+4 KB of SRAM including 64 KB
of CCM (core coupled memory) data RAM
–
Flexible external memory controller with up
to 32-bit data bus: SRAM, PSRAM,
SDRAM/LPSDR SDRAM, compact
flash/NOR/NAND memories
•
LCD parallel interface, 8080/6800 modes
•
LCD-TFT controller with fully programmable
resolution (total width up to 4096 pixels, total
height up to 2048 lines and pixel clock up to
83 MHz)
•
Chrom-ART Accelerator™ for enhanced
graphic content creation (DMA2D)
•
Clock, reset, and supply management
–
1.7 V to 3.6 V application supply and I/Os
–
POR, PDR, PVD, and BOR
–
4-to-26 MHz crystal oscillator
–
Internal 16 MHz factory-trimmed RC (1%
accuracy)
–
32 kHz oscillator for RTC with calibration
–
Internal 32 kHz RC with calibration
•
Low power
–
Sleep, Stop, and Standby modes
–
VBAT supply for RTC, 20×32-bit backup
registers + optional 4 KB backup SRAM
•
3×12-bit, 2.4 MSPS ADC: up to 24 channels
and 7.2 MSPS in triple interleaved mode
•
2×12-bit D/A converters
•
General-purpose DMA: 16-stream DMA
controller with FIFOs and burst support
•
Up to 17 timers: up to twelve 16-bit and two 32-
bit timers up to 180 MHz, each with up to
four IC/OC/PWM or pulse counter and
quadrature (incremental) encoder input
•
Debug mode
–
SWD & JTAG interfaces
–
Cortex®-M4 Trace Macrocell™
•
Up to 168 I/O ports with interrupt capability
–
Up to 164 fast I/Os up to 90 MHz
–
Up to 166 5 V-tolerant I/Os
•
Up to 21 communication interfaces
–
Up to 3 × I2C interfaces (SMBus/PMBus)
–
Up to four USARTs/4 UARTs (11.25 Mbit/s,
ISO7816 interface, LIN, IrDA, modem
control)
–
Up to 6 SPIs (45 Mbit/s), 2 with muxed full-
duplex I2S for audio class accuracy via
internal audio PLL or external clock
–
1 x SAI (serial audio interface)
–
2 × CAN (2.0B active) and SDIO interface
•
Advanced connectivity
–
USB 2.0 full-speed device/host/OTG
controller with on-chip PHY
–
USB 2.0 high-speed/full-speed
device/host/OTG controller with dedicated
DMA, on-chip full-speed PHY and ULPI
–
10/100 Ethernet MAC with dedicated DMA:
supports IEEE 1588v2 hardware, MII/RMII
•
8- to 14-bit parallel camera interface up to
54 Mbytes/s
•
True random number generator
•
CRC calculation unit
•
RTC: subsecond accuracy, hardware calendar
•
96-bit unique ID.
•
ECOPACK2 compliant packages.
LQFP100 (14 × 14 mm)
LQFP144 (20 × 20 mm) UFBGA176 (10 x 10 mm)
LQFP176 (24 × 24 mm)
LQFP208 (28 x 28 mm)
WLCSP143
TFBGA216 (13 x 13 mm)
UFBGA169 (7 × 7 mm)
www.st.com

---

•
Table 1. Device summary
Reference
Part number
STM32F427xx STM32F427VG, STM32F427ZG, STM32F427IG, STM32F427AG, STM32F427VI, STM32F427ZI,
STM32F427II, STM32F427AI
STM32F429xx
STM32F429VG, STM32F429ZG, STM32F429IG, STM32F429BG, STM32F429NG,
STM32F429AG, STM32F429VI, STM32F429ZI, STM32F429II,, STM32F429BI, STM32F429NI,
STM32F429AI, STM32F429VE, STM32F429ZE, STM32F429IE, STM32F429BE, STM32F429NE

---

Contents
Contents
Introduction . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 13
Description . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 14
2.1
Full compatibility throughout the family  . . . . . . . . . . . . . . . . . . . . . . . . . . 18
Functional overview  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 21
3.1
Arm® Cortex®-M4 with FPU and embedded flash and SRAM . . . . . . . . . 21
3.2
Adaptive real-time memory accelerator (ART Accelerator™)  . . . . . . . . . 21
3.3
Memory protection unit . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 21
3.4
Embedded flash memory . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 22
3.5
CRC (cyclic redundancy check) calculation unit  . . . . . . . . . . . . . . . . . . . 22
3.6
Embedded SRAM . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 22
3.7
Multi-AHB bus matrix  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 22
3.8
DMA controller (DMA)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 23
3.9
Flexible memory controller (FMC)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 24
3.10
LCD-TFT controller (available only on STM32F429xx)  . . . . . . . . . . . . . . 24
3.11
Chrom-ART Accelerator™ (DMA2D)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . 25
3.12
Nested vectored interrupt controller (NVIC) . . . . . . . . . . . . . . . . . . . . . . . 25
3.13
External interrupt/event controller (EXTI) . . . . . . . . . . . . . . . . . . . . . . . . . 25
3.14
Clocks and startup  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 25
3.15
Boot modes  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26
3.16
Power supply schemes  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26
3.17
Power supply supervisor  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26
3.17.1
Internal reset ON . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26
3.17.2
Internal reset OFF  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 27
3.18
Voltage regulator  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 28
3.18.1
Regulator ON . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 28
3.18.2
Regulator OFF . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 29
3.18.3
Regulator ON/OFF and internal reset ON/OFF availability  . . . . . . . . . . 32
3.19
Real-time clock (RTC), backup SRAM, and backup registers  . . . . . . . . . 32
3.20
Low-power modes  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 33
3.21
VBAT operation  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 34

---

Contents
3.22
Timers and watchdogs . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 34
3.22.1
Advanced-control timers (TIM1, TIM8)  . . . . . . . . . . . . . . . . . . . . . . . . . 36
3.22.2
General-purpose timers (TIMx)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 36
3.22.3
Basic timers TIM6 and TIM7  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 36
3.22.4
Independent watchdog  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 37
3.22.5
Window watchdog  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 37
3.22.6
SysTick timer . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 37
3.23
Inter-integrated circuit interface ( I2C)  . . . . . . . . . . . . . . . . . . . . . . . . . . . 37
3.24
Universal synchronous/asynchronous receiver transmitters (USART)  . . 37
3.25
Serial peripheral interface (SPI) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 38
3.26
Inter-integrated sound (I2S)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 39
3.27
Serial Audio interface (SAI1)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 39
3.28
Audio PLL (PLLI2S)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 39
3.29
Audio and LCD PLL(PLLSAI) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 39
3.30
Secure digital input/output interface (SDIO) . . . . . . . . . . . . . . . . . . . . . . . 40
3.31
Ethernet MAC interface with dedicated DMA and IEEE 1588 support . . . 40
3.32
Controller area network (bxCAN) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 40
3.33
Universal serial bus on-the-go full-speed (OTG_FS) . . . . . . . . . . . . . . . . 41
3.34
Universal serial bus on-the-go high-speed (OTG_HS) . . . . . . . . . . . . . . . 41
3.35
Digital camera interface (DCMI) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 42
3.36
True random number generator (RNG)  . . . . . . . . . . . . . . . . . . . . . . . . . . 42
3.37
General-purpose input/outputs (GPIOs) . . . . . . . . . . . . . . . . . . . . . . . . . . 42
3.38
Analog-to-digital converters (ADCs) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 42
3.39
Temperature sensor . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 43
3.40
Digital-to-analog converter (DAC)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 43
3.41
Serial wire JTAG debug port (SWJ-DP) . . . . . . . . . . . . . . . . . . . . . . . . . . 43
3.42
Embedded Trace Macrocell™  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 44
Pinouts and pin description . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 45
Memory mapping . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 86
Electrical characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 91
6.1
Parameter conditions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 91
6.1.1
Minimum and maximum values . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 91

---

Contents
6.1.2
Typical values  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 91
6.1.3
Typical curves  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 91
6.1.4
Loading capacitor  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 91
6.1.5
Pin input voltage  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 91
6.1.6
Power supply scheme  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 92
6.1.7
Current consumption measurement  . . . . . . . . . . . . . . . . . . . . . . . . . . . 93
6.2
Absolute maximum ratings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 93
6.3
Operating conditions  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 95
6.3.1
General operating conditions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 95
6.3.2
VCAP1/VCAP2 external capacitor . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 97
6.3.3
Operating conditions at power-up / power-down (regulator ON) . . . . . . 98
6.3.4
Operating conditions at power-up / power-down (regulator OFF) . . . . . 98
6.3.5
Reset and power control block characteristics  . . . . . . . . . . . . . . . . . . . 99
6.3.6
Overdrive switching characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . 100
6.3.7
Supply current characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 101
6.3.8
Wake-up time from low-power modes . . . . . . . . . . . . . . . . . . . . . . . . . 117
6.3.9
External clock source characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . 118
6.3.10
Internal clock source characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . 122
6.3.11
PLL characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 124
6.3.12
PLL spread spectrum clock generation (SSCG) characteristics  . . . . . 127
6.3.13
Memory characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 129
6.3.14
EMC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 131
6.3.15
Absolute maximum ratings (electrical sensitivity)  . . . . . . . . . . . . . . . . 133
6.3.16
I/O current injection characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . 134
6.3.17
I/O port characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 135
6.3.18
NRST pin characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 141
6.3.19
TIM timer characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 142
6.3.20
Communications interfaces . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 142
6.3.21
12-bit ADC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 158
6.3.22
Temperature sensor characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . 164
6.3.23
VBAT monitoring characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 165
6.3.24
Reference voltage  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 165
6.3.25
DAC electrical characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 166
6.3.26
FMC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 169
6.3.27
Camera interface (DCMI) timing specifications . . . . . . . . . . . . . . . . . . 193
6.3.28
LCD-TFT controller (LTDC) characteristics  . . . . . . . . . . . . . . . . . . . . . 194
6.3.29
SD/SDIO MMC card host interface (SDIO) characteristics  . . . . . . . . . 196

---

Contents
6.3.30
RTC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 197
Package information . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 198
7.1
Device marking . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 198
7.2
LQFP100 package information (1L) . . . . . . . . . . . . . . . . . . . . . . . . . . . . 199
7.3
WLCSP143 package information . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 202
7.3.1
Device marking for WLCSP143 . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 204
7.4
LQFP144 package information (1A) . . . . . . . . . . . . . . . . . . . . . . . . . . . . 205
7.5
LQFP176 package information (1T) . . . . . . . . . . . . . . . . . . . . . . . . . . . . 209
7.6
LQFP208 package information . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 213
7.7
UFBGA169 package information (A0YV) . . . . . . . . . . . . . . . . . . . . . . . . 216
7.8
UFBGA(176+25) package information (A0E7) . . . . . . . . . . . . . . . . . . . . 219
7.9
TFBGA216 package information (A0L2)  . . . . . . . . . . . . . . . . . . . . . . . . 221
7.10
Thermal characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 224
Ordering information  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 225
Appendix A
Recommendations when using internal reset OFF  . . . . . . . . . . . 226
A.1
Operating conditions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 226
Appendix B
Application block diagrams . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 227
B.1
USB OTG full speed (FS) interface solutions . . . . . . . . . . . . . . . . . . . . . 227
B.2
USB OTG high speed (HS) interface solutions . . . . . . . . . . . . . . . . . . . . 229
B.3
Ethernet interface solutions. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 230
Important security notice . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 232

---

List of tables
List of tables
Table 1.
Device summary . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 2
Table 2.
STM32F427xx and STM32F429xx features and peripheral counts . . . . . . . . . . . . . . . . . . 16
Table 3.
Voltage regulator configuration mode versus device operating mode . . . . . . . . . . . . . . . . 29
Table 4.
Regulator ON/OFF and internal reset ON/OFF availability. . . . . . . . . . . . . . . . . . . . . . . . . 32
Table 5.
Voltage regulator modes in stop mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 33
Table 6.
Timer feature comparison. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 35
Table 7.
Comparison of I2C analog and digital filters . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 37
Table 8.
USART feature comparison . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 38
Table 9.
Legend/abbreviations used in the pinout table . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 53
Table 10.
STM32F427xx and STM32F429xx pin and ball definitions  . . . . . . . . . . . . . . . . . . . . . . . . 53
Table 11.
FMC pin definition. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 72
Table 12.
STM32F427xx and STM32F429xx alternate function mapping . . . . . . . . . . . . . . . . . . . . . 75
Table 13.
STM32F427xx and STM32F429xx register boundary addresses. . . . . . . . . . . . . . . . . . . . 87
Table 14.
Voltage characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 93
Table 15.
Current characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 94
Table 16.
Thermal characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 94
Table 17.
General operating conditions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 95
Table 18.
Limitations depending on the operating power supply range . . . . . . . . . . . . . . . . . . . . . . . 97
Table 19.
VCAP1/VCAP2 operating conditions  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 97
Table 20.
Operating conditions at power-up / power-down (regulator ON)  . . . . . . . . . . . . . . . . . . . . 98
Table 21.
Operating conditions at power-up / power-down (regulator OFF). . . . . . . . . . . . . . . . . . . . 98
Table 22.
 Reset and power control block characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 99
Table 23.
Over-drive switching characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 100
Table 24.
Typical and maximum current consumption in Run mode, code with data processing
 running from Flash memory (ART accelerator enabled except prefetch) or RAM. . . . . . 102
Table 25.
Typical and maximum current consumption in Run mode, code with data processing
 running from Flash memory (ART accelerator disabled) . . . . . . . . . . . . . . . . . . . . . . . . . 103
Table 26.
Typical and maximum current consumption in Sleep mode . . . . . . . . . . . . . . . . . . . . . . . 104
Table 27.
Typical and maximum current consumptions in Stop mode . . . . . . . . . . . . . . . . . . . . . . . 105
Table 28.
Typical and maximum current consumptions in Standby mode . . . . . . . . . . . . . . . . . . . . 106
Table 29.
Typical and maximum current consumptions in VBAT mode. . . . . . . . . . . . . . . . . . . . . . . 106
Table 30.
Typical current consumption in Run mode, code with data processing running from
 Flash memory or RAM, regulator ON (ART accelerator enabled except prefetch),
VDD=1.7 V . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 108
Table 31.
Typical current consumption in Run mode, code with data processing running
 from Flash memory, regulator OFF (ART accelerator enabled except prefetch). . . . . . . 109
Table 32.
Typical current consumption in Sleep mode, regulator ON, VDD=1.7 V  . . . . . . . . . . . . . 110
Table 33.
Tyical current consumption in Sleep mode, regulator OFF. . . . . . . . . . . . . . . . . . . . . . . . 111
Table 34.
Switching output I/O current consumption  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 113
Table 35.
Peripheral current consumption . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 114
Table 36.
Low-power mode wakeup timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 117
Table 37.
High-speed external user clock characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 118
Table 38.
Low-speed external user clock characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 119
Table 39.
HSE 4-26 MHz oscillator characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 120
Table 40.
LSE oscillator characteristics (fLSE = 32.768 kHz) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 121
Table 41.
HSI oscillator characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 122
Table 42.
LSI oscillator characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 123
Table 43.
Main PLL characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 124

---

List of tables
Table 44.
PLLI2S (audio PLL) characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 125
Table 45.
PLLISAI (audio and LCD-TFT PLL) characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 126
Table 46.
SSCG parameters constraint . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 127
Table 47.
Flash memory characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 129
Table 48.
Flash memory programming. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 129
Table 49.
Flash memory programming with VPP  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .130
Table 50.
Flash memory endurance and data retention . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 131
Table 51.
EMS characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 131
Table 52.
EMI characteristics for fHSE= 25 MHz and fCPU= 168 MHz . . . . . . . . . . . . . . . . . . . . . . 132
Table 53.
EMI characteristics for HSE= 25 MHz and fCPU= 180 MHz  . . . . . . . . . . . . . . . . . . . . . . 133
Table 54.
ESD absolute maximum ratings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 133
Table 55.
Electrical sensitivities . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 134
Table 56.
I/O current injection susceptibility . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 134
Table 57.
I/O static characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 135
Table 58.
Output voltage characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 138
Table 59.
I/O AC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 139
Table 60.
NRST pin characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 141
Table 61.
TIMx characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 142
Table 62.
I2C analog filter characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 143
Table 63.
SPI dynamic characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 143
Table 64.
I2S dynamic characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 146
Table 65.
SAI characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 149
Table 66.
USB OTG full speed startup time . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 151
Table 67.
USB OTG full speed DC electrical characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 151
Table 68.
USB OTG full speed electrical characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 152
Table 69.
USB HS DC electrical characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 152
Table 70.
USB HS clock timing parameters . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 153
Table 71.
Dynamic characteristics: USB ULPI . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 154
Table 72.
Dynamics characteristics: Ethernet MAC signals for SMI. . . . . . . . . . . . . . . . . . . . . . . . . 155
Table 73.
Dynamics characteristics: Ethernet MAC signals for RMII . . . . . . . . . . . . . . . . . . . . . . . . 156
Table 74.
Dynamics characteristics: Ethernet MAC signals for MII  . . . . . . . . . . . . . . . . . . . . . . . . . 157
Table 75.
ADC characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 158
Table 76.
ADC static accuracy at fADC = 18 MHz. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 159
Table 77.
ADC static accuracy at fADC = 30 MHz. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 160
Table 78.
ADC static accuracy at fADC = 36 MHz. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 160
Table 79.
ADC dynamic accuracy at fADC = 18 MHz - limited test conditions  . . . . . . . . . . . . . . . . . 160
Table 80.
ADC dynamic accuracy at fADC = 36 MHz - limited test conditions  . . . . . . . . . . . . . . . . . 160
Table 81.
Temperature sensor characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 164
Table 82.
Temperature sensor calibration values. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 164
Table 83.
VBAT monitoring characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 165
Table 84.
 internal reference voltage  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 165
Table 85.
Internal reference voltage calibration values  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 165
Table 86.
DAC characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 166
Table 87.
Asynchronous non-multiplexed SRAM/PSRAM/NOR -
read timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 170
Table 88.
Asynchronous non-multiplexed SRAM/PSRAM/NOR read -
NWAIT timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 171
Table 89.
Asynchronous non-multiplexed SRAM/PSRAM/NOR write timings . . . . . . . . . . . . . . . . . 172
Table 90.
Asynchronous non-multiplexed SRAM/PSRAM/NOR write -
NWAIT timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 173
Table 91.
Asynchronous multiplexed PSRAM/NOR read timings. . . . . . . . . . . . . . . . . . . . . . . . . . . 174
Table 92.
Asynchronous multiplexed PSRAM/NOR read-NWAIT timings . . . . . . . . . . . . . . . . . . . . 174

---

List of tables
Table 93.
Asynchronous multiplexed PSRAM/NOR write timings  . . . . . . . . . . . . . . . . . . . . . . . . . . 175
Table 94.
Asynchronous multiplexed PSRAM/NOR write-NWAIT timings . . . . . . . . . . . . . . . . . . . . 176
Table 95.
Synchronous multiplexed NOR/PSRAM read timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . 177
Table 96.
Synchronous multiplexed PSRAM write timings. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 179
Table 97.
Synchronous non-multiplexed NOR/PSRAM read timings . . . . . . . . . . . . . . . . . . . . . . . . 180
Table 98.
Synchronous non-multiplexed PSRAM write timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . 181
Table 99.
Switching characteristics for PC Card/CF read and write cycles
 in attribute/common space. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 186
Table 100.
Switching characteristics for PC Card/CF read and write cycles
 in I/O space . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 187
Table 101.
Switching characteristics for NAND Flash read cycles . . . . . . . . . . . . . . . . . . . . . . . . . . . 188
Table 102.
Switching characteristics for NAND Flash write cycles. . . . . . . . . . . . . . . . . . . . . . . . . . . 189
Table 103.
SDRAM read timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 190
Table 104.
LPSDR SDRAM read timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 190
Table 105.
SDRAM write timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 192
Table 106.
LPSDR SDRAM write timings. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 192
Table 107.
DCMI characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 193
Table 108.
LTDC characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 194
Table 109.
Dynamic characteristics: SD / MMC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 197
Table 110.
RTC characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 197
Table 111.
LQFP100 - Mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 200
Table 112.
WLCSP143 - 143-ball, 4.521x 5.547 mm, 0.4 mm pitch wafer level chip scale
 package mechanical data  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 202
Table 113.
WLCSP143 recommended PCB design rules  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 204
Table 114.
LQFP144 - Mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 206
Table 115.
LQFP176 - Mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 210
Table 116.
LQFP208 - Mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 214
Table 117.
UFBGA169 - Mechanical data  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 217
Table 118.
UFBGA169 - Example of PCB design rules (0.5 mm pitch BGA). . . . . . . . . . . . . . . . . . . 218
Table 119.
UFBGA(176+25) - Mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 219
Table 120.
UFBGA(176+25) - Example of PCB design rules (0.65 mm pitch BGA)  . . . . . . . . . . . . . 220
Table 121.
TFBGA216 - Mechanical data  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 222
Table 122.
TFBGA216 - Example of PCB design rules (0.8 mm pitch) . . . . . . . . . . . . . . . . . . . . . . . 223
Table 123.
Package thermal characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 224
Table 124.
Limitations depending on the operating power supply range . . . . . . . . . . . . . . . . . . . . . . 226
Table 125.

---

List of figures
List of figures
Figure 1.
Compatible board design STM32F10xx/STM32F2xx/STM32F4xx
for LQFP100 package. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 18
Figure 2.
Compatible board design between STM32F10xx/STM32F2xx/STM32F4xx
for LQFP144 package. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 19
Figure 3.
Compatible board design between STM32F2xx and STM32F4xx
 for LQFP176 and UFBGA176 packages . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 19
Figure 4.
STM32F427xx and STM32F429xx block diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 20
Figure 5.
STM32F427xx and STM32F429xx Multi-AHB matrix . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 23
Figure 6.
Power supply supervisor interconnection with internal reset OFF . . . . . . . . . . . . . . . . . . . 27
Figure 7.
PDR_ON control with internal reset OFF . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 28
Figure 8.
Regulator OFF  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 30
Figure 9.
Startup in regulator OFF: slow VDD slope
- power-down reset risen after VCAP_1/VCAP_2 stabilization . . . . . . . . . . . . . . . . . . . . . . . . 31
Figure 10.
Startup in regulator OFF mode: fast VDD slope
- power-down reset risen before VCAP_1/VCAP_2 stabilization  . . . . . . . . . . . . . . . . . . . . . . 31
Figure 11.
STM32F42x LQFP100 pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 45
Figure 12.
STM32F42x WLCSP143 ballout. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 46
Figure 13.
STM32F42x LQFP144 pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 47
Figure 14.
STM32F42x LQFP176 pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 48
Figure 15.
STM32F42x LQFP208 pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 49
Figure 16.
STM32F42x UFBGA169 ballout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 50
Figure 17.
STM32F42x UFBGA176 ballout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 51
Figure 18.
STM32F42x TFBGA216 ballout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 52
Figure 19.
Memory map. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 86
Figure 20.
Pin loading conditions. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 91
Figure 21.
Pin input voltage . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 91
Figure 22.
Power supply scheme  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 92
Figure 23.
Current consumption measurement scheme . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 93
Figure 24.
External capacitor CEXT . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 97
Figure 25.
Typical VBAT current consumption (LSE and RTC ON/backup RAM OFF)  . . . . . . . . . . . 107
Figure 26.
Typical VBAT current consumption (LSE and RTC ON/backup RAM ON) . . . . . . . . . . . . 107
Figure 27.
High-speed external clock source AC timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . 119
Figure 28.
Low-speed external clock source AC timing diagram. . . . . . . . . . . . . . . . . . . . . . . . . . . . 120
Figure 29.
Typical application with an 8 MHz crystal . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 121
Figure 30.
Typical application with a 32.768 kHz crystal . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 122
Figure 31.
ACCHSI accuracy versus temperature. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 123
Figure 32.
ACCLSI versus temperature . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 124
Figure 33.
PLL output clock waveforms in center spread mode  . . . . . . . . . . . . . . . . . . . . . . . . . . . . 128
Figure 34.
PLL output clock waveforms in down spread mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 128
Figure 35.
FT I/O input characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 137
Figure 36.
I/O AC characteristics definition . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 140
Figure 37.
Recommended NRST pin protection . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 141
Figure 38.
SPI timing diagram - slave mode and CPHA = 0 . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 145
Figure 39.
SPI timing diagram - slave mode and CPHA = 1 . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 145
Figure 40.
SPI timing diagram - master mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 146
Figure 41.
I2S slave timing diagram (Philips protocol)(1) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 148
Figure 42.
I2S master timing diagram (Philips protocol)(1). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 148
Figure 43.
SAI master timing waveforms . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 150

---

List of figures
Figure 44.
SAI slave timing waveforms . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 150
Figure 45.
USB OTG full speed timings: definition of data signal rise and fall time. . . . . . . . . . . . . . 152
Figure 46.
ULPI timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 153
Figure 47.
Ethernet SMI timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 155
Figure 48.
Ethernet RMII timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 156
Figure 49.
Ethernet MII timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 157
Figure 50.
ADC accuracy characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 161
Figure 51.
Typical connection diagram when using the ADC with FT/TT pins
featuring the analog switch function . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 162
Figure 52.
Power supply and reference decoupling (VREF+ not connected to VDDA). . . . . . . . . . . . . 163
Figure 53.
Power supply and reference decoupling (VREF+ connected to VDDA). . . . . . . . . . . . . . . . 164
Figure 54.
12-bit buffered /non-buffered DAC . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 168
Figure 55.
Asynchronous non-multiplexed SRAM/PSRAM/NOR read waveforms . . . . . . . . . . . . . . 170
Figure 56.
Asynchronous non-multiplexed SRAM/PSRAM/NOR write waveforms . . . . . . . . . . . . . . 172
Figure 57.
Asynchronous multiplexed PSRAM/NOR read waveforms. . . . . . . . . . . . . . . . . . . . . . . . 173
Figure 58.
Asynchronous multiplexed PSRAM/NOR write waveforms  . . . . . . . . . . . . . . . . . . . . . . . 175
Figure 59.
Synchronous multiplexed NOR/PSRAM read timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . 177
Figure 60.
Synchronous multiplexed PSRAM write timings. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 178
Figure 61.
Synchronous non-multiplexed NOR/PSRAM read timings . . . . . . . . . . . . . . . . . . . . . . . . 180
Figure 62.
Synchronous non-multiplexed PSRAM write timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . 181
Figure 63.
PC Card/CompactFlash controller waveforms for common memory read access . . . . . . 183
Figure 64.
PC Card/CompactFlash controller waveforms for common memory write access. . . . . . 183
Figure 65.
PC Card/CompactFlash controller waveforms for attribute memory
read access  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 184
Figure 66.
PC Card/CompactFlash controller waveforms for attribute memory
 write access . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 185
Figure 67.
PC Card/CompactFlash controller waveforms for I/O space read access . . . . . . . . . . . . 185
Figure 68.
PC Card/CompactFlash controller waveforms for I/O space write access . . . . . . . . . . . . 186
Figure 69.
NAND controller waveforms for read access . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 188
Figure 70.
NAND controller waveforms for write access . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 188
Figure 71.
SDRAM read access waveforms (CL = 1) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 189
Figure 72.
SDRAM write access waveforms . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 191
Figure 73.
DCMI timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 193
Figure 74.
LCD-TFT horizontal timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 195
Figure 75.
LCD-TFT vertical timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 195
Figure 76.
SDIO high-speed mode  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 196
Figure 77.
SD default mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 196
Figure 78.
LQFP100 - Outline(15). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 199
Figure 79.
LQFP100 - Footprint example  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 201
Figure 80.
WLCSP143 - 143-ball, 4.521x 5.547 mm, 0.4 mm pitch wafer level chip scale
 package outline . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 202
Figure 81.
WLCSP143 - 143-ball, 4.521x 5.547 mm, 0.4 mm pitch wafer level chip scale
package recommended footprint  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 203
Figure 82.
WLCSP143 marking example (package top view) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 204
Figure 83.
LQFP144 - Outline(15). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 205
Figure 84.
LQFP144 - Footprint example  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 208
Figure 85.
LQFP176 - Outline(15). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 209
Figure 86.
LQFP176 - Footprint example  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 212
Figure 87.
LQFP208 - Outline(15). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 213
Figure 88.
LQFP208 - footprint example . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 215
Figure 89.
UFBGA169 - Outline. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 216
Figure 90.
UFBGA169 - Footprint example . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 218

---

List of figures
Figure 91.
UFBGA(176+25) - Outline  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 219
Figure 92.
UFBGA(176+25) - Footprint example. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 220
Figure 93.
TFBGA216 - Outline . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 221
Figure 94.
TFBGA216 - Footprint example . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 223
Figure 95.
USB controller configured as peripheral-only and used
in Full speed mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 227
Figure 96.
USB controller configured as host-only and used in full speed mode. . . . . . . . . . . . . . . . 227
Figure 97.
USB controller configured in dual mode and used in full speed mode . . . . . . . . . . . . . . . 228
Figure 98.
USB controller configured as peripheral, host, or dual-mode
and used in high speed mode. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 229
Figure 99.
MII mode using a 25 MHz crystal . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 230
Figure 100. RMII with a 50 MHz oscillator . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 230
Figure 101. RMII with a 25 MHz crystal and PHY with PLL. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 231

---

Introduction
Introduction
This datasheet provides the description of the STM32F427xx and STM32F429xx line of
microcontrollers. For more details on the whole STMicroelectronics STM32 family, refer to
Section 2.1: Full compatibility throughout the family.
The STM32F427xx and STM32F429xx datasheet should be read with the STM32F4xx
reference manual.
For information on the Cortex®-M4 core, refer to the Cortex®-M4 programming manual
(PM0214), available from www.st.com.
For information on the device errata with respect to the datasheet and reference manual,
refer to the STM32F427/437xx and STM32F429/439xx errata sheet (ES0206), available
from www.st.com.
Note:
Arm and Cortex are registered trademarks of Arm Limited (or its subsidiaries or affiliates) in
the US and/or elsewhere.
The Arm word and logo are trademarks of Arm Limited (or its subsidiaries) in the US and/or
elsewhere. All rights reserved.

---

Description
Description
The STM32F427xx and STM32F429xx devices are based on the high-performance Arm®
Cortex®-M4 32-bit RISC core operating at a frequency of up to 180 MHz. The Cortex®-M4
core features a floating-point unit (FPU) single precision, which supports all Arm® single-
precision data-processing instructions and data types. It also implements a full set of DSP
instructions and a memory protection unit (MPU) which enhances application security.
The STM32F427xx and STM32F429xx devices incorporate high-speed embedded
memories (Flash memory up to 2 Mbyte, up to 256 Kbytes of SRAM), up to 4 Kbytes of
backup SRAM, and an extensive range of enhanced I/Os and peripherals connected to two
APB buses, two AHB buses and a 32-bit multi-AHB bus matrix.
All devices offer three 12-bit ADCs, two DACs, a low-power RTC, 12 general-purpose 16-bit
timers including two PWM timers for motor control, two general-purpose 32-bit timers. They
also feature standard and advanced communication interfaces.
•
Up to three I2Cs
•
Six SPIs, two I2Ss full duplex. To achieve audio class accuracy, the I2S peripherals can
be clocked via a dedicated internal audio PLL or via an external clock to allow
synchronization.
•
Four USARTs plus four UARTs
•
An USB OTG full-speed and a USB OTG high-speed with full-speed capability (with the
ULPI),
•
Two CANs
•
One SAI serial audio interface
•
An SDIO/MMC interface
•
Ethernet and camera interface
•
LCD-TFT display controller
•
Chrom-ART Accelerator™.
Advanced peripherals include an SDIO, a flexible memory control (FMC) interface, a
camera interface for CMOS image sensors. Refer to Table 2: STM32F427xx and
STM32F429xx features and peripheral counts for the list of peripherals available on each
part number.
The STM32F427xx and STM32F429xx devices operates in the –40 to +105 °C temperature
range from a 1.7 to 3.6 V power supply.
The supply voltage can drop to 1.7 V with the use of an external power supply supervisor
(refer to Section 3.17.2: Internal reset OFF). A comprehensive set of power-saving mode
allows the design of low-power applications.
The STM32F427xx and STM32F429xx devices offer devices in 8 packages ranging from
100 pins to 216 pins. The set of included peripherals changes with the device chosen.

---

Description
These features make the STM32F427xx and STM32F429xx microcontrollers suitable for a
wide range of applications:
•
Motor drive and application control
•
Medical equipment
•
Industrial applications: PLC, inverters, circuit breakers
•
Printers, and scanners
•
Alarm systems, video intercom, and HVAC
•
Home audio appliances
Figure 4 shows the general block diagram of the device family.

---

Description
Table 2. STM32F427xx and STM32F429xx features and peripheral counts
Peripherals
STM32F427
Vx
STM32F429Vx
STM32F427
Zx
STM32F429Zx
STM32F427
Ax
STM32F429
Ax
STM32F427
Ix
STM32F429Ix
STM32F429Bx
STM32F429Nx
Flash memory in Kbytes
SRAM in
Kbytes
System
256(112+16+64+64)
Backup
FMC memory controller
Yes(1)
Ethernet
Yes
Timers
General-
purpose
Advanced
-control
Basic
Random number generator
Yes
Communication
interfaces
SPI / I2S
4/2 (full duplex)(2)
6/2 (full duplex)(2)
I2C
USART/
UART
USB OTG
FS
Yes
USB OTG
HS
Yes
CAN
SAI
SDIO
Yes
Camera interface
Yes
LCD-TFT (STM32F429xx
only)
No
Yes
No
Yes
No
Yes
No
Yes
Chrom-ART Accelerator™
Yes
GPIOs
12-bit ADC
Number of channels

---

Description
12-bit DAC
Number of channels
Yes
Maximum CPU frequency
180 MHz
Operating voltage
1.8 to 3.6 V(3)
Operating temperatures
Ambient temperatures: –40 to +85 °C /–40 to +105 °C
Junction temperature: –40 to + 125 °C
Packages
LQFP100
WLCSP143
LQFP144
UFBGA169
UFBGA176
LQFP176
LQFP208
TFBGA216
1.
For the LQFP100 package, only FMC Bank1 or Bank2 are available. Bank1 can only support a multiplexed NOR/PSRAM memory using the NE1 Chip Select. Bank2 can only support a 16- or 8-bit
NAND Flash memory using the NCE2 Chip Select. The interrupt line cannot be used since Port G is not available in this package. For UFBGA169 package, only SDRAM, NAND and multiplexed
static memories are supported.
2.
The SPI2 and SPI3 interfaces give the flexibility to work in an exclusive way in either the SPI mode or the I2S audio mode.
3.
VDD/VDDA minimum value of 1.7 V is obtained when the device operates in reduced temperature range, and with the use of an external power supply supervisor (refer to Section 3.17.2: Internal reset
OFF).
Table 2. STM32F427xx and STM32F429xx features and peripheral counts (continued)
Peripherals
STM32F427
Vx
STM32F429Vx
STM32F427
Zx
STM32F429Zx
STM32F427
Ax
STM32F429
Ax
STM32F427
Ix
STM32F429Ix
STM32F429Bx
STM32F429Nx

---

Description
2.1
Full compatibility throughout the family
The STM32F427xx and STM32F429xx devices are part of the STM32F4 family. They are
fully pin-to-pin, software and feature compatible with the STM32F2xx devices, allowing the
user to try different memory densities, peripherals, and performances (FPU, higher
frequency) for a greater degree of freedom during the development cycle.
The STM32F427xx and STM32F429xx devices maintain a close compatibility with the
whole STM32F10xx family. All functional pins are pin-to-pin compatible. The STM32F427xx
and STM32F429xx, however, are not drop-in replacements for the STM32F10xx devices:
the two families do not have the same power scheme, and so their power pins are different.
Nonetheless, the transition from the STM32F10xx to the STM32F42x family remains simple
as only a few pins are impacted.
Figure 1, Figure 2, and Figure 3, give compatible board designs between the STM32F4xx,
STM32F2xx, and STM32F10xx families.
Figure 1. Compatible board design STM32F10xx/STM32F2xx/STM32F4xx
for LQFP100 package
ai18488d
VSS
VSS
VDD
VSS
VSS
VSS
UHVLVWRURUVROGHULQJEULGJH
SUHVHQWIRUWKH67032F10xxx
FRQILJXUDWLRQQRWSUHVHQWLQWKH
670)[[FRQILJXUDWLRQ
99 (VSS)
VSS
7ZRUHVLVWRUVFRQQHFWHGWR

-966IRUWKH670)[[
-966IRUWKH670)[[
-966RU1&IRUWKH670)[[
966IRU670)[[
9''IRU670)[[

---

Description
Figure 2. Compatible board design between STM32F10xx/STM32F2xx/STM32F4xx
for LQFP144 package
Figure 3. Compatible board design between STM32F2xx and STM32F4xx
 for LQFP176 and UFBGA176 packages
ai18487d
VSS
UHVLVWRURUVROGHULQJEULGJH
SUHVHQWIRUWKH67032F10xx
FRQILJXUDWLRQQRWSUHVHQWLQWKH
670)[[FRQILJXUDWLRQ
VSS
7ZRUHVLVWRUVFRQQHFWHGWR

- VSS IRUWKH670)[[
- VSS, VDD RU1&IRUWKH670)[[
- VDD RUVLJQDOIURPH[WHUQDOSRZHUVXSSO\VXSHUYLVRUIRUWKH670)[[
VSS
VDD
VSS
VSS

3'5B21
VSS
VDD
VSS  IRU670)[[
VDD IRU670)[[
6LJQDOIURP
H[WHUQDOSRZHU
VXSSO\
VXSHUYLVRU
1RWSRSXODWHGZKHQ
UHVLVWRURUVROGHULQJ
EULGJHSUHVHQW
1RWSRSXODWHGIRU67032F10xx
MS31835V1
7ZRUHVLVWRUVFRQQHFWHGWR

- VSS, VDD RU1&IRUWKH670)[[
- VDD RUVLJQDOIURPH[WHUQDOSRZHUVXSSO\VXSHUYLVoUIRUWKH670)[[

3'5B21
VSS
VDD
6LJQDOIURPH[WHUQDO
SRZHUVXSSO\
VXSHUYLVoU
*1'IRU670)[[
%<3$66B5(*IRU670)[[

---

Description
Figure 4. STM32F427xx and STM32F429xx block diagram
1.
The timers connected to APB2 are clocked from TIMxCLK up to 180 MHz, while the timers connected to APB1 are clocked
from TIMxCLK either up to 90 MHz or 180 MHz depending on TIMPRE bit configuration in the RCC_DCKCFGR register.
2.
The LCD-TFT is available only on STM32F429xx devices.
MSv30420V5.svg
GPIO PORT A
AHB/APB2
EXT IT. WKUP
168 AF
PA[15:0]
TIM1 / PWM
4 compl. chan. (TIM1_CH1[1:4]N),
4 chan. (TIM1_CH1[1:4]ETR,
BKIN as AF
USART1
RX, TX, CK,
CTS, RTS as AF
SPI1
MOSI, MISO,
SCK, NSS as AF
A P B 2 60 MHz
A P B 1 3 0M Hz
8 analog inputs common
to the 3 ADCs
VDDREF_ADC
UART4
MOSI/SD, MISO/SD_ext, SCK/CK
NSS/WS, MCK as AF
SP3/I2S3
TX, RX
bxCAN2
DAC1_OUT
as AF
ITF
WWDG
4 KB BKPSRAM
RTC_AF1
OSC32_IN
OSC32_OUT
VDDA, VSSA
NRST
smcard
irDA
16b
SDIO / MMC
D[7:0]
CMD, CK as AF
VBAT = 1.65 to 3.6 V
DMA2
SCL, SDA, SMBA as AF
I2C3/SMBUS
JTAG & SW
Arm Cortex-M4
180 MHz
NVIC
ETM
MPU
TRACECLK
TRACED[3:0]
Ethernet MAC
DMA/
FIFO
MII or RMII as AF
MDIO as AF
USB
OTG HS
DP, DM
ULPI:CK, D[7:0], DIR, STP, NXT
ID, VBUS, SOF
DMA2
8 Streams
FIFO
ART ACCEL/
CACHE
SRAM 112 KB
CLK, NE [3:0], A[23:0],
D[31:0], NOEN, NWEN,
NBL[3:0], SDCLKE[1:0], SDNE[1:0],
SDNWE, NL
NWAIT/NIORD, NREG, CD
INTR
RNG
Camera
interface
HSYNC, VSYNC
PUIXCLK, D[13:0]
PHY
USB
OTG FS
DP
DM
ID, VBUS, SOF
FIFO
AHB1 180 MHz
PHY
FIFO
U S AR T 2 MB p s
Temperature sensor
ADC1
ADC2
ADC3
IF
IF
@ VDDA
@ VDDA
POR/PDR
BOR
Supply
supervision
@ V DDA
PVD
Int
POR
reset
XTAL 32 kHz
MAN A G T
RTC
RC
HS
RC
L S
Standby
interface
IWDG
@ V BAT
@ V DDA
@ V DD
AWU
Reset &
clock
control
PLL1,2,3
PCLKx
VDD = 1.8 to 3.6 V
VSS
VCAP1, VCAP2
Voltage
regulator
3.3 to 1.2 V
VDD
Power managmt
@ V DD
RTC_AF1
Backup register
AHB bus-matrix 8S7M
APB2 90 MHz
LS
TIM14
TIM9
2 channels as AF
DAC1
DAC2
1MB Flash
External memory controller (FMC)
SRAM, SDRAM, PSRAM,
NOR Flash, PC Card,
NAND Flash
TIM6
TIM7
TIM2
TIM3
TIM4
TIM5
TIM12
D-BUS
FIFO
FPU
APB1 45 MHz (max)
SRAM 16 KB
CCM data RAM 64 KB
AHB3
AHB2 180 MHz
NJTRST, JTDI,
JTCK/SWCLK
JTDO/SWD, JTDO
I-BUS
S-BUS
DMA/
FIFO
DMA1
8 Streams
FIFO
PB[15:0]
PC[15:0]
PD[15:0]
PE[15:0]
PF[15:0]
PG[15:0]
PH[15:0]
PI[15:0]
GPIO PORT B
GPIO PORT C
GPIO PORT D
GPIO PORT E
GPIO PORT F
GPIO PORT G
GPIO PORT H
GPIO PORT I
TIM8 / PWM
16b
16b
TIM10
16b
TIM11
16b
smcard
irDA
USART6
4 compl. chan.(TIM8_CH1[1:4]N),
4 chan. (TIM8_CH1[1:4], ETR,
BKIN as AF
1 channel as AF
1 channel as AF
RX, TX, CK,
CTS, RTS as AF
8 analog inputs common
to the ADC1 & 2
8 analog inputs for ADC3
DAC2_OUT
as AF
16b
16b
bxCAN1
I2C2/SMBUS
I2C1/SMBUS
SCL, SDA, SMBA as AF
SCL, SDA, SMBA as AF
SP2/I2S2
MOSI/SD, MISO/SD_ext, SCK/CK
NSS/WS, MCK as AF
TX, RX
RX, TX as AF
RX, TX as AF
RX, TX as AF
CTS, RTS as AF
RX, TX as AF
CTS, RTS as AF
1 channel as AF
UART5
USART3
USART2
smcard
irDA
smcard
irDA
16b
16b
16b
1 channel as AF
TIM13
2 channels as AF
32b
16b
16b
32b
4 channels
4 channels, ETR as AF
4 channels, ETR as AF
4 channels, ETR as AF
DMA1
AHB/APB1
LS
OSC_IN
OSC_OUT
HCLKx
XTAL OSC
4- 26MHz
FIFO
SPI4
SCK, NSS as AF
SPI5
SCK, NSS as AF
MOSI, MISO,
MOSI, MISO,
SPI6
SCK, NSS as AF
MOSI, MISO,
RX, TX as AF
UART7
RX, TX as AF
UART8
SRAM 64 KB
1MB Flash
FIFO
LCD-TFT
FIFO
PJ[15:0]
GPIO PORT J
PK[7:0]
GPIO PORT K
SAI1
SD, SCK, FS, MCLK as AF
FIFO
Digital filter
NRAS, NCAS, NADV
RTC_50HZ
LCD_R[7:0], LCD_G[7:0], LCD_B[7:0],
LCD_HSYNC, LCD_VSYNC, LCD_DE,
LCD_CLK
CHROM-ART
DMA2D

---

Functional overview
Functional overview
3.1
Arm® Cortex®-M4 with FPU and embedded flash and SRAM
The Arm® Cortex®-M4 with FPU processor is the latest generation of Arm® processors for
embedded systems. It was developed to provide a low-cost platform that meets the needs of
MCU implementation, with a reduced pin count and low-power consumption, while
delivering outstanding computational performance and an advanced response to interrupts.
The Arm® Cortex®-M4 with FPU core is a 32-bit RISC processor that features exceptional
code-efficiency, delivering the high-performance expected from an Arm® core in the
memory size usually associated with 8- and 16-bit devices.
The processor supports a set of DSP instructions, which allow efficient signal processing
and complex algorithm execution.
Its single-precision FPU (floating-point unit) speeds up software development by using
metalanguage development tools, while avoiding saturation.
The STM32F42x family is compatible with all Arm tools and software.
Figure 4 shows the general block diagram of the STM32F42x family.
Note:
Cortex®-M4 with FPU core is binary compatible with the Cortex®-M3 core.
3.2
Adaptive real-time memory accelerator (ART Accelerator™)
The ART Accelerator™ is a memory accelerator, which is optimized for STM32 industry-
standard Arm® Cortex®-M4 with FPU processors. It balances the inherent performance
advantage of the Arm® Cortex®-M4 with FPU over flash memory at higher frequencies.
To release the processor full 225 DMIPS performance at this frequency, the accelerator
implements an instruction prefetch queue and branch cache, which increases program
execution speed from the 128-bit flash memory. Based on the CoreMark benchmark, the
performance achieved thanks to the ART Accelerator is equivalent to 0 wait state program
execution from the flash memory at a CPU frequency up to 180 MHz.
3.3
Memory protection unit
The memory protection unit (MPU) is used to manage the CPU accesses to memory to
prevent one task to accidentally corrupt the memory or resources used by any other active
task. This memory area is organized into up to 8 protected areas that can in turn be divided
up into 8 subareas. The protection area sizes are between 32 bytes and the whole 4
gigabytes of addressable memory.
The MPU is especially helpful for applications where some critical or certified code has to be
protected against the misbehavior of other tasks. It is usually managed by an RTOS (real-
time operating system). If a program accesses a memory location that is prohibited by the
MPU, the RTOS can detect it and act. In an RTOS environment, the kernel can dynamically
update the MPU area setting, based on the process to be executed.
The MPU is optional and can be bypassed for applications that do not need it.

---

Functional overview
3.4
Embedded flash memory
The devices embed 512 bytes of OTP memory, and a flash memory of up to 2 Mbytes
available for storing programs and data.
3.5
CRC (cyclic redundancy check) calculation unit
The CRC (cyclic redundancy check) calculation unit is used to get a CRC code from a 32-bit
data word and a fixed generator polynomial.
Among other applications, CRC-based techniques are used to verify data transmission or
storage integrity. In the scope of the EN/IEC 60335-1 standard, they offer a means of
verifying the flash memory integrity. The CRC calculation unit helps compute a software
signature during runtime, to be compared with a reference signature generated at link-time
and stored at a given memory location.
3.6
Embedded SRAM
All devices embed:
•
Up to 256Kbytes of system SRAM including 64 Kbytes of CCM (core coupled memory)
data RAM
RAM memory is accessed (read/write) at CPU clock speed with 0 wait states.
•
4 Kbytes of backup SRAM
This area is accessible only from the CPU. Its content is protected against possible
unwanted write accesses, and is retained in Standby or VBAT mode.
3.7
Multi-AHB bus matrix
The 32-bit multi-AHB bus matrix interconnects all the masters (CPU, DMAs, Ethernet, USB
HS, LCD-TFT, and DMA2D) and the slaves (Flash memory, RAM, FMC, AHB, and APB
peripherals) and ensures a seamless and efficient operation even when several high-speed
peripherals work simultaneously.

---

Functional overview
Figure 5. STM32F427xx and STM32F429xx Multi-AHB matrix
3.8
DMA controller (DMA)
The devices feature two general-purpose dual-port DMAs (DMA1 and DMA2) with 8
streams each. They are able to manage memory-to-memory, peripheral-to-memory, and
memory-to-peripheral transfers. They feature dedicated FIFOs for APB/AHB peripherals,
support burst transfer and are designed to provide the maximum peripheral bandwidth
(AHB/APB).
The two DMA controllers support circular buffer management, so that no specific code is
needed when the controller reaches the end of the buffer. The two DMA controllers also
have a double buffering feature, which automates the use and switching of two memory
buffers without requiring any special code.
Each stream is connected to dedicated hardware DMA requests, with support for software
trigger on each stream. Configuration is made by software and transfer sizes between
source and destination are independent.
ARM
Cortex-M4
GP
DMA1
GP
DMA2
MAC
Ethernet
USB OTG
HS
Bus matrix-S
ICODE
DCODE
ACCEL
Flash
memory
SRAM1
112 Kbyte
SRAM2
16 Kbyte
AHB2
peripherals
AHB1
peripherals
FMC external
MemCtl
I-bus
D-bus
S-bus
DMA_PI
DMA_MEM1
DMA_MEM2
DMA_P2
ETHERNET_M
USB_HS_M
MS30421V6
CCM data RAM
64-Kbyte
APB1
APB2
SRAM3
64 Kbyte
LCD-TFT
Chrom ART Accelerator
(DMA2D)
LCD-TFT_M
DMA2D

---

Functional overview
The DMA can be used with the main peripherals:
•
SPI and I2S
•
I2C
•
USART
•
General-purpose, basic, and advanced-control timers TIMx
•
DAC
•
SDIO
•
Camera interface (DCMI)
•
ADC
•
SAI1.
3.9
Flexible memory controller (FMC)
All devices embed an FMC. It has four Chip Select outputs supporting the following modes:
PCCard/Compact flash, SDRAM/LPSDR SDRAM, SRAM, PSRAM, NOR flash and NAND
flash.
Functionality overview:
•
8-,16-, 32-bit data bus width
•
Read FIFO for SDRAM controller
•
Write FIFO
•
Maximum FMC_CLK/FMC_SDCLK frequency for synchronous accesses is 90 MHz.
LCD parallel interface
The FMC can be configured to interface seamlessly with most graphic LCD controllers. It
supports the Intel 8080 and Motorola 6800 modes, and is flexible enough to adapt to
specific LCD interfaces. This LCD parallel interface capability makes it easy to build cost-
effective graphic applications using LCD modules with embedded controllers or high-
performance solutions using external controllers with dedicated acceleration.
3.10
LCD-TFT controller (available only on STM32F429xx)
The LCD-TFT display controller provides a 24-bit parallel digital RGB (Red, Green, Blue)
and delivers all signals to interface directly to a broad range of LCD and TFT panels up to
XGA (1024x768) resolution with the following features:
•
Two display layers with dedicated FIFO (64x32-bit)
•
Color look-up table (CLUT) up to 256 colors (256x24-bit) per layer
•
Up to eight input color formats selectable per layer
•
Flexible blending between two layers using alpha value (per pixel or constant)
•
Flexible programmable parameters for each layer
•
Color keying (transparency color)
•
Up to four programmable interrupt events.

---

Functional overview
3.11
Chrom-ART Accelerator™ (DMA2D)
The Chrom-Art Accelerator™ (DMA2D) is a graphic accelerator, which offers advanced bit
blitting, row data copy and pixel format conversion. It supports the following functions:
•
Rectangle filling with a fixed color
•
Rectangle copy
•
Rectangle copy with pixel format conversion
•
Rectangle composition with blending and pixel format conversion.
Various image format coding are supported, from indirect 4 bpp color mode up to 32 bpp
direct color. It embeds dedicated memory to store color lookup tables.
An interrupt can be generated when an operation is complete or at a programmed
watermark.
All the operations are fully automatized and are running independently from the CPU or the
DMAs.
3.12
Nested vectored interrupt controller (NVIC)
The devices embed a nested vectored interrupt controller able to manage 16 priority levels,
and handle up to 91 maskable interrupt channels plus the 16 interrupt lines of the Cortex®-
M4 with FPU core.
•
Closely coupled NVIC gives low-latency interrupt processing
•
Interrupt entry vector table address passed directly to the core
•
Allows early processing of interrupts
•
Processing of late arriving, higher-priority interrupts
•
Support tail chaining
•
Processor state automatically saved
•
Interrupt entry restored on interrupt exit with no instruction overhead
This hardware block provides flexible interrupt management features with minimum interrupt
latency.
3.13
External interrupt/event controller (EXTI)
The external interrupt/event controller consists of 23 edge-detector lines used to generate
interrupt/event requests. Each line can be independently configured to select the trigger
event (rising edge, falling edge, both) and can be masked independently. A pending register
maintains the status of the interrupt requests. The EXTI can detect an external line with a
pulse width shorter than the Internal APB2 clock period. Up to 168 GPIOs can be connected
to the 16 external interrupt lines.
3.14
Clocks and startup
On reset the 16 MHz internal RC oscillator is selected as the default CPU clock. The
16 MHz internal RC oscillator is factory-trimmed to offer 1% accuracy over the full
temperature range. The application can then select as system clock either the RC oscillator
or an external 4-26 MHz clock source. This clock can be monitored for failure. If a failure is

---

Functional overview
detected, the system automatically switches back to the internal RC oscillator and a
software interrupt is generated (if enabled). This clock source is input to a PLL thus allowing
to increase the frequency up to 180 MHz. Similarly, full interrupt management of the PLL
clock entry is available when necessary (for example if an indirectly used external oscillator
fails).
Several prescalers allow the configuration of the two AHB buses, the high-speed APB
(APB2) and the low-speed APB (APB1) domains. The maximum frequency of the two AHB
buses is 180 MHz while the maximum frequency of the high-speed APB domains is
90 MHz. The maximum allowed frequency of the low-speed APB domain is 45 MHz.
The devices embed a dedicated PLL (PLLI2S) and PLLSAI, which allows to achieve audio
class performance. In this case, the I2S master clock can generate all standard sampling
frequencies from 8 kHz to 192 kHz.
3.15
Boot modes
At startup, boot pins are used to select one out of three boot options:
•
Boot from user flash
•
Boot from system memory
•
Boot from embedded SRAM
The bootloader is located in system memory. It is used to reprogram the flash memory
through a serial interface. Refer to application note AN2606 for details.
3.16
Power supply schemes
•
VDD = 1.7 to 3.6 V: external power supply for I/Os and the internal regulator (when
enabled), provided externally through VDD pins.
•
VSSA, VDDA = 1.7 to 3.6 V: external analog power supplies for ADC, DAC, reset blocks,
RCs, and PLL. VDDA and VSSA must be connected to VDD and VSS, respectively.
•
VBAT = 1.65 to 3.6 V: power supply for RTC, external clock 32 kHz oscillator and
backup registers (through power switch) when VDD is not present.
Note:
The VDD/VDDA minimum value of 1.7 V is obtained with the use of an external power supply
supervisor (refer to Section 3.17.2: Internal reset OFF). Refer to Table 3: Voltage regulator
configuration mode versus device operating mode to identify the packages supporting this
option.
3.17
Power supply supervisor
3.17.1
Internal reset ON
On packages embedding the PDR_ON pin, the power supply supervisor is enabled by
holding PDR_ON high. On the other package, the power supply supervisor is always
enabled.
The device has an integrated power-on reset (POR)/ power-down reset (PDR) circuitry
coupled with a brownout reset (BOR) circuitry. At power-on, POR/PDR is always active and
ensures proper operation starting from 1.8 V. After the 1.8 V POR threshold level is

---

Functional overview
reached, the option byte loading process starts, either to confirm or modify default BOR
thresholds, or to disable BOR permanently. Three BOR thresholds are available through
option bytes. The device remains in reset mode when VDD is below a specified threshold,
VPOR/PDR, or VBOR, without the need for an external reset circuit.
The device also features an embedded programmable voltage detector (PVD) that monitors
the VDD/VDDA power supply and compares it to the VPVD threshold. An interrupt can be
generated when VDD/VDDA drops below the VPVD threshold and/or when VDD/VDDA is
higher than the VPVD threshold. The interrupt service routine can then generate a warning
message and/or put the MCU into a safe state. The PVD is enabled by software.
3.17.2
Internal reset OFF
This feature is available only on packages featuring the PDR_ON pin. The internal power-on
reset (POR) / power-down reset (PDR) circuitry is disabled through the PDR_ON pin.
An external power supply supervisor should monitor VDD and should maintain the device in
reset mode as long as VDD is below a specified threshold. PDR_ON should be connected to
this external power supply supervisor. Refer to Figure 6: Power supply supervisor
interconnection with internal reset OFF.
Figure 6. Power supply supervisor interconnection with internal reset OFF
The VDD specified threshold, below which the device must be maintained under reset, is
1.7 V (see Figure 7).
A comprehensive set of power-saving mode allows to design low-power applications.
When the internal reset is OFF, the following integrated features are no more supported:
•
The integrated power-on reset (POR) / power-down reset (PDR) circuitry is disabled
•
The brownout reset (BOR) circuitry must be disabled
•
The embedded programmable voltage detector (PVD) is disabled
•
VBAT functionality is no more available and VBAT pin should be connected to VDD.
All packages, except for the LQFP100, allow to disable the internal reset through the
PDR_ON signal.
MS31383V3
NRST
VDD
PDR_ON
External VDD power supply supervisor
Ext. reset controller active when
VDD < 1.7 V
VDD
Application reset
signal (optional)

---

Functional overview
Figure 7. PDR_ON control with internal reset OFF
3.18
Voltage regulator
The regulator has four operating modes:
•
Regulator ON
–
Main regulator mode (MR)
–
Low-power regulator (LPR)
–
Power-down
•
Regulator OFF
3.18.1
Regulator ON
On packages embedding the BYPASS_REG pin, the regulator is enabled by holding
BYPASS_REG low. On all other packages, the regulator is always enabled.
There are three power modes configured by software when the regulator is ON:
•
MR mode used in Run/sleep modes or in Stop modes
–
In Run/Sleep mode
The MR mode is used either in the normal mode (default mode) or the over-drive
mode (enabled by software). Different voltages scaling are provided to reach the
best compromise between maximum frequency and dynamic power consumption.
MS19009V6
VDD
time
PDR = 1.7 V
time
NRST
PDR_ON
PDR_ON
Reset by other source than
power supply supervisor

---

Functional overview
The overdrive mode allows operating at a higher frequency than the normal mode
for a given voltage scaling.
–
In Stop modes
The MR can be configured in two ways during stop mode:
MR operates in normal mode (default mode of MR in stop mode)
MR operates in underdrive mode (reduced leakage mode).
•
LPR is used in the Stop modes:
The LP regulator mode is configured by software when entering Stop mode.
Like the MR mode, the LPR can be configured in two ways during stop mode:
–
LPR operates in normal mode (default mode when LPR is ON)
–
LPR operates in underdrive mode (reduced leakage mode).
•
Power-down is used in Standby mode.
The Power-down mode is activated only when entering in Standby mode. The regulator
output is in high impedance and the kernel circuitry is powered down, inducing zero
consumption. The contents of the registers and SRAM are lost.
Refer to Table 3 for a summary of voltage regulator modes versus device operating modes.
Two external ceramic capacitors should be connected on VCAP_1 and VCAP_2 pin. Refer to
Figure 22: Power supply scheme and Table 19: VCAP1/VCAP2 operating conditions.
All packages have the regulator ON feature.
3.18.2
Regulator OFF
This feature is available only on packages featuring the BYPASS_REG pin. The regulator is
disabled by holding BYPASS_REG high. The regulator OFF mode allows to supply
externally a V12 voltage source through VCAP_1 and VCAP_2 pins.
Since the internal voltage scaling is not managed internally, the external voltage value must
be aligned with the targeted maximum frequency. Refer to Table 17: General operating
conditions.The two 2.2 µF ceramic capacitors should be replaced by two 100 nF decoupling
capacitors. Refer to Figure 22: Power supply scheme.
When the regulator is OFF, there is no more internal monitoring on V12. An external power
supply supervisor should be used to monitor the V12 of the logic power domain. PA0 pin
should be used for this purpose, and act as a power-on reset on V12 power domain.
Table 3. Voltage regulator configuration mode versus device operating mode(1)
1.
‘-’ means that the corresponding configuration is not available.
Voltage regulator
configuration
Run mode
Sleep mode
Stop mode
Standby mode
Normal mode
MR
MR
MR or LPR
-
Over-drive
mode(2)
2.
The over-drive mode is not available when VDD = 1.7 to 2.1 V.
MR
MR
-
-
Under-drive mode
-
-
MR or LPR
-
Power-down
mode
-
-
-
Yes

---

Functional overview
In regulator OFF mode, the following features are no more supported:
•
PA0 cannot be used as a GPIO pin since it allows to reset a part of the V12 logic power
domain, which is not reset by the NRST pin.
•
As long as PA0 is kept low, the debug mode cannot be used under power-on reset. As
a consequence, PA0 and NRST pins must be managed separately if the debug
connection under reset or prereset is required.
•
The overdrive and underdrive modes are not available.
•
The Standby mode is not available.
Figure 8. Regulator OFF
The following conditions must be respected:
•
VDD should always be higher than VCAP_1 and VCAP_2 to avoid current injection
between power domains.
•
If the time for VCAP_1 and VCAP_2 to reach V12 minimum value is faster than the time for
VDD to reach 1.7 V, then PA0 should be kept low to cover both conditions: until VCAP_1
and VCAP_2 reach V12 minimum value and until VDD reaches 1.7 V (see Figure 9).
•
Otherwise, if the time for VCAP_1 and VCAP_2 to reach V12 minimum value is slower
than the time for VDD to reach 1.7 V, then PA0 could be asserted low externally (see
Figure 10).
•
If VCAP_1 and VCAP_2 go below V12 minimum value and VDD is higher than 1.7 V, then a
reset must be asserted on PA0 pin.
Note:
The minimum value of V12 depends on the maximum frequency targeted in the application
(see Table 17: General operating conditions).
ai18498V3
BYPASS_REG
VCAP_1
VCAP_2
PA0
V12
VDD
NRST
VDD
Application reset
signal (optional)
External VCAP_1/2 power
supply supervisor
Ext. reset controller active
when VCAP_1/2  < Min V12
V12

---

Functional overview
Figure 9. Startup in regulator OFF: slow VDD slope
- power-down reset risen after VCAP_1/VCAP_2 stabilization
1.
This figure is valid whatever the internal reset mode (ON or OFF).
Figure 10. Startup in regulator OFF mode: fast VDD slope
- power-down reset risen before VCAP_1/VCAP_2 stabilization
1.
This figure is valid whatever the internal reset mode (ON or OFF).
ai18491f
VDD
time
Min V12
PDR = 1.7 V or 1.8 V
VCAP_1 / VCAP_2
V12
NRST
time
VDD
time
Min V12
VCAP_1 / VCAP_2
V12
PA0 asserted externally
NRST
time
ai18492e
PDR = 1.7 V or 1.8 V

---

Functional overview
3.18.3
Regulator ON/OFF and internal reset ON/OFF availability
3.19
Real-time clock (RTC), backup SRAM, and backup registers
The backup domain includes:
•
The real-time clock (RTC)
•
4 Kbytes of backup SRAM
•
20 backup registers
The real-time clock (RTC) is an independent BCD timer/counter. Dedicated registers contain
the second, minute, hour (in 12/24 hour), weekday, date, month, year, in BCD (binary-coded
decimal) format. Correction for 28, 29 (leap year), 30, and 31 day of the month are
performed automatically. The RTC provides a programmable alarm and programmable
periodic interrupts with wake-up from Stop and Standby modes. The subseconds value is
also available in binary format.
It is clocked by a 32.768 kHz external crystal, resonator or oscillator, the internal low-power
RC oscillator or the high-speed external clock divided by 128. The internal low-speed RC
has a typical frequency of 32 kHz. The RTC can be calibrated using an external 512 Hz
output to compensate for any natural quartz deviation.
Two alarm registers are used to generate an alarm at a specific time and calendar fields can
be independently masked for alarm comparison. To generate a periodic interrupt, a 16-bit
programmable binary autoreload downcounter with programmable resolution is available
and allows automatic wake-up and periodic alarms from every 120 µs to every 36 hours.
A 20-bit prescaler is used for the time base clock. It is by default configured to generate a
time base of 1 second from a clock at 32.768 kHz.
The 4-Kbyte backup SRAM is an EEPROM-like memory area. It can be used to store data,
which need to be retained in VBAT and standby mode. This memory area is disabled by
default to minimize power consumption (see Section 3.20: Low-power modes). It can be
enabled by software.
The backup registers are 32-bit registers used to store 80 bytes of user application data
when VDD power is not present. Backup registers are not reset by a system, a power reset,
or when the device wakes up from the Standby mode (see Section 3.20: Low-power
modes).
Table 4. Regulator ON/OFF and internal reset ON/OFF availability
Package
Regulator ON
Regulator OFF
Internal reset ON
Internal reset OFF
LQFP100
Yes
No
Yes
No
LQFP144,
LQFP208
Yes
PDR_ON set to
VDD
Yes
PDR_ON
connected to an
external power
supply supervisor
WLCSP143,
LQFP176,
UFBGA169,
UFBGA176,
TFBGA216
Yes
BYPASS_REG set
to VSS
Yes
BYPASS_REG set
to VDD

---

Functional overview
Additional 32-bit registers contain the programmable alarm subseconds, seconds, minutes,
hours, day, and date.
Like backup SRAM, the RTC and backup registers are supplied through a switch that is
powered either from the VDD supply when present or from the VBAT pin.
3.20
Low-power modes
The devices support three low-power modes to achieve the best compromise between low-
power consumption, short startup time and available wake-up sources:
•
Sleep mode
In Sleep mode, only the CPU is stopped. All peripherals continue to operate and can
wake up the CPU when an interrupt/event occurs.
•
Stop mode
The Stop mode achieves the lowest power consumption while retaining the contents of
SRAM and registers. All clocks in the 1.2 V domain are stopped, the PLL, the HSI RC
and the HSE crystal oscillators are disabled.
The voltage regulator can be put either in main regulator mode (MR) or in low-power
mode (LPR). Both modes can be configured as follows (see Table 5: Voltage regulator
modes in stop mode):
–
Normal mode (default mode when MR or LPR is enabled)
–
Underdrive mode.
The device can be woken up from the Stop mode by any of the EXTI lines (the EXTI
line source can be one of the 16 external lines, the PVD output, the RTC alarm / wake-
up / tamper / time stamp events, the USB OTG FS/HS wake-up or the Ethernet wake-
up).
•
Standby mode
The Standby mode is used to achieve the lowest power consumption. The internal
voltage regulator is switched off so that the entire 1.2 V domain is powered off. The
PLL, the HSI RC and the HSE crystal oscillators are also switched off. After entering
Standby mode, the SRAM and register contents are lost except for registers in the
backup domain and the backup SRAM when selected.
The device exits the Standby mode when an external reset (NRST pin), an IWDG reset,
a rising edge on the WKUP pin, or an RTC alarm / wake-up / tamper /time stamp event
occurs.
The standby mode is not supported when the embedded voltage regulator is bypassed,
and an external power controls the 1.2 V domain.
Table 5. Voltage regulator modes in stop mode
Voltage regulator
configuration
Main regulator (MR)
Low-power regulator (LPR)
Normal mode
MR ON
LPR ON
Under-drive mode
MR in under-drive mode
LPR in under-drive mode

---

Functional overview
3.21
VBAT operation
The VBAT pin allows to power the device VBAT domain from an external battery, an external
supercapacitor, or from VDD when no external battery and an external supercapacitor are
present.
VBAT operation is activated when VDD is not present.
The VBAT pin supplies the RTC, the backup registers, and the backup SRAM.
Note:
When the microcontroller is supplied from VBAT, external interrupts and RTC alarm/events
do not exit it from VBAT operation.
When PDR_ON pin is not connected to VDD (Internal Reset OFF), the VBAT functionality is
no more available and VBAT pin should be connected to VDD.
3.22
Timers and watchdogs
The devices include two advanced-control timers, eight general-purpose timers, two basic
timers and two watchdog timers.
All timer counters can be frozen in debug mode.
Table 6 compares the features of the advanced-control, general-purpose and basic timers.

---

Functional overview
Table 6. Timer feature comparison
Timer
type
Timer
Counter
resolution
Counter
type
Prescaler
factor
DMA
request
generation
Capture/
compare
channels
Complementary
output
Max
interface
clock
(MHz)
Max
timer
clock
(MHz)
(1)
Advanced
-control
TIM1,
TIM8
16-bit
Up,
Down,
Up/down
Any
integer
between 1
and
65536
Yes
Yes
General
purpose
TIM2,
TIM5
32-bit
Up,
Down,
Up/down
Any
integer
between 1
and
65536
Yes
No
TIM3,
TIM4
16-bit
Up,
Down,
Up/down
Any
integer
between 1
and
65536
Yes
No
TIM9
16-bit
Up
Any
integer
between 1
and
65536
No
No
TIM10
,
TIM11
16-bit
Up
Any
integer
between 1
and
65536
No
No
TIM12
16-bit
Up
Any
integer
between 1
and
65536
No
No
TIM13
,
TIM14
16-bit
Up
Any
integer
between 1
and
65536
No
No
Basic
TIM6,
TIM7
16-bit
Up
Any
integer
between 1
and
65536
Yes
No
1.
The maximum timer clock is either 90 or 180 MHz depending on TIMPRE bit configuration in the RCC_DCKCFGR register.

---

Functional overview
3.22.1
Advanced-control timers (TIM1, TIM8)
The advanced-control timers (TIM1, TIM8) can be seen as three-phase PWM generators
multiplexed on six channels. They have complementary PWM outputs with programmable
inserted dead times. They can also be considered as complete general-purpose timers.
Their four independent channels can be used for:
•
Input capture
•
Output compare
•
PWM generation (edge- or center-aligned modes)
•
One-pulse mode output
If configured as standard 16-bit timers, they have the same features as the general-purpose
TIMx timers. If configured as 16-bit PWM generators, they have full modulation capability (0-
100%).
The advanced-control timer can work together with the TIMx timers via the Timer Link
feature for synchronization or event chaining.
TIM1 and TIM8 support independent DMA request generation.
3.22.2
General-purpose timers (TIMx)
There are ten synchronizable general-purpose timers embedded in the STM32F42x devices
(see Table 6 for differences).
•
TIM2, TIM3, TIM4, TIM5
The STM32F42x include 4 full-featured general-purpose timers: TIM2, TIM5, TIM3,
and TIM4. The TIM2 and TIM5 timers are based on a 32-bit autoreload
up/downcounter and a 16-bit prescaler. The TIM3 and TIM4 timers are based on a 16-
bit auto-reload up/downcounter and a 16-bit prescaler. They all feature 4 independent
channels for input capture/output compare, PWM, or one-pulse mode output. This
gives up to 16 input capture/output compare/PWMs on the largest packages.
The TIM2, TIM3, TIM4, TIM5 general-purpose timers can work together, or with the
other general-purpose timers and the advanced-control timers TIM1 and TIM8 via the
Timer Link feature for synchronization or event chaining.
Any of these general-purpose timers can be used to generate PWM outputs.
TIM2, TIM3, TIM4, TIM5 all have independent DMA request generation. They can
handle quadrature (incremental) encoder signals and the digital outputs from 1 to 4
hall-effect sensors.
•
TIM9, TIM10, TIM11, TIM12, TIM13, and TIM14
These timers are based on a 16-bit auto-reload upcounter and a 16-bit prescaler.
TIM10, TIM11, TIM13, and TIM14 feature one independent channel, whereas TIM9
and TIM12 have two independent channels for input capture/output compare, PWM or
one-pulse mode output. They can be synchronized with the TIM2, TIM3, TIM4, TIM5
full-featured general-purpose timers. They can also be used as simple time bases.
3.22.3
Basic timers TIM6 and TIM7
These timers are mainly used for DAC trigger and waveform generation. They can also be
used as a generic 16-bit time base.
TIM6 and TIM7 support independent DMA request generation.

---

Functional overview
3.22.4
Independent watchdog
The independent watchdog is based on a 12-bit downcounter and 8-bit prescaler. It is
clocked from an independent 32 kHz internal RC and as it operates independently from the
main clock, it can operate in Stop and Standby modes. It can be used either as a watchdog
to reset the device when a problem occurs, or as a free-running timer for application timeout
management. It is hardware- or software-configurable through the option bytes.
3.22.5
Window watchdog
The window watchdog is based on a 7-bit downcounter that can be set as free-running. It
can be used as a watchdog to reset the device when a problem occurs. It is clocked from
the main clock. It has an early warning interrupt capability and the counter can be frozen in
debug mode.
3.22.6
SysTick timer
This timer is dedicated to real-time operating systems, but could also be used as a standard
downcounter. It features:
•
A 24-bit downcounter
•
Autoreload capability
•
Maskable system interrupt generation when the counter reaches 0
•
Programmable clock source.
3.23
Inter-integrated circuit interface ( I2C)
Up to three I²C bus interfaces can operate in multimaster and slave modes. They can
support the standard (up to 100 kHz), and fast (up to 400 kHz) modes. They support the
7/10-bit addressing mode and the 7-bit dual addressing mode (as slave). A hardware CRC
generation/verification is embedded.
They can be served by DMA and they support SMBus 2.0/PMBus.
The devices also include programmable analog and digital noise filters (see Table 7).
3.24
Universal synchronous/asynchronous receiver transmitters
(USART)
The devices embed four universal synchronous/asynchronous receiver transmitters
(USART1, USART2, USART3, and USART6) and four universal asynchronous receiver
transmitters (UART4, UART5, UART7, and UART8).
These six interfaces provide asynchronous communication, IrDA SIR ENDEC support,
multiprocessor communication mode, single-wire half-duplex communication mode and
have LIN Master/Slave capability. The USART1 and USART6 interfaces are able to
Table 7. Comparison of I2C analog and digital filters
Analog filter
Digital filter
Pulse width of
suppressed spikes
≥ 50 ns
Programmable length from 1 to 15
I2C peripheral clocks

---

Functional overview
communicate at speeds of up to 11.25 Mbit/s. The other available interfaces communicate
at up to 5.62 bit/s.
USART1, USART2, USART3 and USART6 also provide hardware management of the CTS
and RTS signals, Smart Card mode (ISO 7816 compliant) and SPI-like communication
capability. All interfaces can be served by the DMA controller.
3.25
Serial peripheral interface (SPI)
The devices feature up to six SPIs in slave and master modes in full-duplex and simplex
communication modes. SPI1, SPI4, SPI5, and SPI6 can communicate at up to 45 Mbits/s,
SPI2 and SPI3 can communicate at up to 22.5 Mbit/s. The 3-bit prescaler gives 8 master
mode frequencies and the frame is configurable to 8 bits or 16 bits. The hardware CRC
generation/verification supports basic SD Card/MMC modes. All SPIs can be served by the
DMA controller.
The SPI interface can be configured to operate in TI mode for communications in master
mode and slave mode.
Table 8. USART feature comparison(1)
USART
name
Standard
features
Modem
(RTS/CTS)
LIN
SPI
master irDA
Smartcard
(ISO 7816)
Max. baud
rate in Mbit/s
(oversampling
by 16)
Max. baud
rate in Mbit/s
(oversampling
by 8)
APB
mapping
USART1
X
X
X
X
X
X
5.62
11.25
APB2
(max.
90 MHz)
USART2
X
X
X
X
X
X
2.81
5.62
APB1
(max.
45 MHz)
USART3
X
X
X
X
X
X
2.81
5.62
APB1
(max.
45 MHz)
UART4
X
-
X
-
X
-
2.81
5.62
APB1
(max.
45 MHz)
UART5
X
-
X
-
X
-
2.81
5.62
APB1
(max.
45 MHz)
USART6
X
X
X
X
X
X
5.62
11.25
APB2
(max.
90 MHz)
UART7
X
-
X
-
X
-
2.81
5.62
APB1
(max.
45 MHz)
UART8
X
-
X
-
X
-
2.81
5.62
APB1
(max.
45 MHz)
1.
X = feature supported.

---

Functional overview
3.26
Inter-integrated sound (I2S)
Two standard I2S interfaces (multiplexed with SPI2 and SPI3) are available. They can be
operated in master or slave mode, in full duplex and simplex communication modes, and
can be configured to operate with a 16-/32-bit resolution as an input or output channel.
Audio sampling frequencies from 8 kHz up to 192 kHz are supported. When either or both of
the I2S interfaces is/are configured in master mode, the master clock can be output to the
external DAC/CODEC at 256 times the sampling frequency.
All I2Sx can be served by the DMA controller.
Note:
For I2S2 full-duplex mode, I2S2_CK and I2S2_WS signals can be used only on GPIO Port
B and GPIO Port D.
3.27
Serial Audio interface (SAI1)
The serial audio interface (SAI1) is based on two independent audio sub-blocks which can
operate as transmitter or receiver with their FIFO. Many audio protocols are supported by
each block: I2S standards, LSB or MSB-justified, PCM/DSP, TDM, AC’97 and SPDIF
output, supporting audio sampling frequencies from 8 kHz up to 192 kHz. Both sub-blocks
can be configured in master or in slave mode.
In master mode, the master clock can be output to the external DAC/CODEC at 256 times of
the sampling frequency.
The two sub-blocks can be configured in synchronous mode when full-duplex mode is
required.
SAI1 can be served by the DMA controller.
3.28
Audio PLL (PLLI2S)
The devices feature an additional dedicated PLL for audio I2S and SAI applications. It allows
to achieve error-free I2S sampling clock accuracy without compromising on the CPU
performance, while using USB peripherals.
The PLLI2S configuration can be modified to manage an I2S/SAI sample rate change
without disabling the main PLL (PLL) used for CPU, USB and Ethernet interfaces.
The audio PLL can be programmed with very low error to obtain sampling rates ranging
from 8 KHz to 192 KHz.
In addition to the audio PLL, a master clock input pin can be used to synchronize the
I2S/SAI flow with an external PLL (or Codec output).
3.29
Audio and LCD PLL(PLLSAI)
An additional PLL dedicated to audio and LCD-TFT is used for SAI1 peripheral in case the
PLLI2S is programmed to achieve another audio sampling frequency (49.152 MHz or
11.2896 MHz) and the audio application requires both sampling frequencies simultaneously.
The PLLSAI is also used to generate the LCD-TFT clock.

---

Functional overview
3.30
Secure digital input/output interface (SDIO)
An SD/SDIO/MMC host interface is available, that supports MultiMediaCard System
Specification Version 4.2 in three different databus modes: 1-bit (default), 4-bit and 8-bit.
The interface allows data transfer at up to 48 MHz, and is compliant with the SD Memory
Card Specification Version 2.0.
The SDIO Card Specification Version 2.0 is also supported with two different databus
modes: 1-bit (default) and 4-bit.
The current version supports only one SD/SDIO/MMC4.2 card at any one time and a stack
of MMC4.1 or previous.
In addition to SD/SDIO/MMC, this interface is fully compliant with the CE-ATA digital
protocol Rev1.1.
3.31
Ethernet MAC interface with dedicated DMA and IEEE 1588
support
The devices provide an IEEE-802.3-2002-compliant media access controller (MAC) for
ethernet LAN communications through an industry-standard medium-independent interface
(MII) or a reduced medium-independent interface (RMII). The microcontroller requires an
external physical interface device (PHY) to connect to the physical LAN bus (twisted-pair,
fiber, etc.). The PHY is connected to the device MII port using 17 signals for MII or 9 signals
for RMII, and can be clocked using the 25 MHz (MII) from the microcontroller.
The devices include the following features:
•
Supports 10 and 100 Mbit/s rates
•
Dedicated DMA controller allowing high-speed transfers between the dedicated SRAM
and the descriptors (see the STM32F4xx reference manual for details)
•
Tagged MAC frame support (VLAN support)
•
Half-duplex (CSMA/CD) and full-duplex operation
•
MAC control sublayer (control frames) support
•
32-bit CRC generation and removal
•
Several address filtering modes for physical and multicast address (multicast and
group addresses)
•
32-bit status code for each transmitted or received frame
•
Internal FIFOs to buffer transmit and receive frames. The transmit FIFO and the
receive FIFO are both 2 Kbytes.
•
Supports hardware PTP (precision time protocol) in accordance with IEEE 1588 2008
(PTP V2) with the time stamp comparator connected to the TIM2 input
•
Triggers interrupt when system time becomes greater than target time
3.32
Controller area network (bxCAN)
The two CANs are compliant with the 2.0A and B (active) specifications with a bitrate up to 1
Mbit/s. They can receive and transmit standard frames with 11-bit identifiers as well as
extended frames with 29-bit identifiers. Each CAN has three transmit mailboxes, two receive

---

Functional overview
FIFOS with 3 stages and 28 shared scalable filter banks (all of them can be used even if one
CAN is used). 256 bytes of SRAM are allocated for each CAN.
3.33
Universal serial bus on-the-go full-speed (OTG_FS)
The devices embed an USB OTG full-speed device/host/OTG peripheral with integrated
transceivers. The USB OTG FS peripheral is compliant with the USB 2.0 specification and
with the OTG 1.0 specification. It has software-configurable endpoint setting and supports
suspend/resume. The USB OTG full-speed controller requires a dedicated 48 MHz clock
that is generated by a PLL connected to the HSE oscillator. The major features are:
•
Combined Rx and Tx FIFO size of 320 × 35 bits with dynamic FIFO sizing
•
Supports the session request protocol (SRP) and host negotiation protocol (HNP)
•
4 bidirectional endpoints
•
8 host channels with periodic OUT support
•
HNP/SNP/IP inside (no need for any external resistor)
•
For OTG/Host modes, a power switch is needed in case bus-powered devices are
connected
3.34
Universal serial bus on-the-go high-speed (OTG_HS)
The devices embed a USB OTG high-speed (up to 480 Mb/s) device/host/OTG peripheral.
The USB OTG HS supports both full-speed and high-speed operations. It integrates the
transceivers for full-speed operation (12 MB/s) and features a UTMI low-pin interface (ULPI)
for high-speed operation (480 MB/s). When using the USB OTG HS in HS mode, an
external PHY device connected to the ULPI is required.
The USB OTG HS peripheral is compliant with the USB 2.0 specification and with the OTG
1.0 specification. It has software-configurable endpoint setting and supports
suspend/resume. The USB OTG full-speed controller requires a dedicated 48 MHz clock
that is generated by a PLL connected to the HSE oscillator.
The major features are:
•
Combined Rx and Tx FIFO size of 1 Kbit × 35 with dynamic FIFO sizing
•
Supports the session request protocol (SRP) and host negotiation protocol (HNP)
•
6 bidirectional endpoints
•
12 host channels with periodic OUT support
•
Internal FS OTG PHY support
•
External HS or HS OTG operation supporting ULPI in SDR mode. The OTG PHY is
connected to the microcontroller ULPI port through 12 signals. It can be clocked using
the 60 MHz output.
•
Internal USB DMA
•
HNP/SNP/IP inside (no need for any external resistor)
•
for OTG/Host modes, a power switch is needed in case bus-powered devices are
connected

---

Functional overview
3.35
Digital camera interface (DCMI)
The devices embed a camera interface that can connect with camera modules and CMOS
sensors through an 8-bit to 14-bit parallel interface, to receive video data. The camera
interface can sustain a data transfer rate up to 54 Mbyte/s at 54 MHz. It features:
•
Programmable polarity for the input pixel clock and synchronization signals
•
Parallel data communication can be 8-, 10-, 12- or 14-bit
•
Supports 8-bit progressive video monochrome or raw bayer format, YCbCr 4:2:2
progressive video, RGB 565 progressive video or compressed data (like JPEG)
•
Supports continuous mode or snapshot (a single frame) mode
•
Capability to automatically crop the image
3.36
True random number generator (RNG)
The RNG is a true random number generator that provides full entropy outputs to the
application as 32-bit samples. It is composed of a live entropy source (analog) and an
internal conditioning component.
All devices embed an RNG that delivers 32-bit random numbers generated by an integrated
analog circuit.
3.37
General-purpose input/outputs (GPIOs)
Each of the GPIO pins can be configured by software as output (push-pull or open-drain,
with or without pull-up or pull-down), as input (floating, with or without pull-up or pull-down)
or as peripheral alternate function. Most of the GPIO pins are shared with digital or analog
alternate functions. All GPIOs are high-current-capable and have speed selection to better
manage internal noise, power consumption and electromagnetic emission.
The I/O configuration can be locked if needed by following a specific sequence in order to
avoid spurious writing to the I/Os registers.
Fast I/O handling allowing maximum I/O toggling up to 90 MHz.
3.38
Analog-to-digital converters (ADCs)
Three 12-bit analog-to-digital converters are embedded and each ADC shares up to 16
external channels, performing conversions in the single-shot or scan mode. In scan mode,
automatic conversion is performed on a selected group of analog inputs.
Additional logic functions embedded in the ADC interface allow:
•
Simultaneous sample and hold
•
Interleaved sample and hold
The ADC can be served by the DMA controller. An analog watchdog feature allows very
precise monitoring of the converted voltage of one, some or all selected channels. An
interrupt is generated when the converted voltage is outside the programmed thresholds.
To synchronize A/D conversion and timers, the ADCs could be triggered by any of TIM1,
TIM2, TIM3, TIM4, TIM5, or TIM8 timer.

---

Functional overview
3.39
Temperature sensor
The temperature sensor has to generate a voltage that varies linearly with temperature. The
conversion range is between 1.7 V and 3.6 V. The temperature sensor is internally
connected to the same input channel as VBAT, ADC1_IN18, which is used to convert the
sensor output voltage into a digital value. When the temperature sensor and VBAT
conversion are enabled at the same time, only VBAT conversion is performed.
As the offset of the temperature sensor varies from chip to chip due to process variation, the
internal temperature sensor is mainly suitable for applications that detect temperature
changes instead of absolute temperatures. If an accurate temperature reading is needed,
then an external temperature sensor part should be used.
3.40
Digital-to-analog converter (DAC)
The two 12-bit buffered DAC channels can be used to convert two digital signals into two
analog voltage signal outputs.
This dual digital Interface supports the following features:
•
two DAC converters: one for each output channel
•
8-bit or 10-bit monotonic output
•
left or right data alignment in 12-bit mode
•
synchronized update capability
•
noise-wave generation
•
triangular-wave generation
•
dual DAC channel independent or simultaneous conversions
•
DMA capability for each channel
•
external triggers for conversion
•
input voltage reference VREF+
Eight DAC trigger inputs are used in the device. The DAC channels are triggered through
the timer update outputs that are also connected to different DMA streams.
3.41
Serial wire JTAG debug port (SWJ-DP)
The Arm SWJ-DP interface is embedded, and is a combined JTAG and serial wire debug
port that enables either a serial wire debug or a JTAG probe to be connected to the target.
Debug is performed using 2 pins only instead of 5 required by the JTAG (JTAG pins could
be re-use as GPIO with alternate function): the JTAG TMS and TCK pins are shared with
SWDIO and SWCLK, respectively, and a specific sequence on the TMS pin is used to
switch between JTAG-DP and SW-DP.

---

Functional overview
3.42
Embedded Trace Macrocell™
The Arm Embedded Trace Macrocell provides a greater visibility of the instruction and data
flow inside the CPU core by streaming compressed data at a very high rate from the
STM32F42x through a small number of ETM pins to an external hardware trace port
analyzer (TPA) device. The TPA is connected to a host computer using USB, Ethernet, or
any other high-speed channel. Real-time instruction and data flow activity can be recorded
and then formatted for display on the host computer that runs the debugger software. TPA
hardware is commercially available from common development tool vendors.
The Embedded Trace Macrocell operates with third party debugger software tools.

---

Pinouts and pin description
Pinouts and pin description
Figure 11. STM32F42x LQFP100 pinout
1.
The above figure shows the package top view.
PE2
PE3
PE4
PE5
PE6
VBAT
PC14
PC15
VSS
VDD
PH0
NRST
PC0
PC1
PC2
PC3
VDD
VSSA
VREF+
VDDA
PA0
PA1
PA2
VDD
VSS
VCAP_2
PA13
PA12
PA 11
PA10
PA9
PA8
PC9
PC8
PC7
PC6
PD15
PD14
PD13
PD12
PD11
PD10
PD9
PD8
PB15
PB14
PB13
PB12
PA3
VSS
VDD
PA4
PA5
PA6
PA7
PC4
PC5
PB0
PB1
PB2
PE7
PE8
PE9
PE10
PE11
PE12
PE13
PE14
PE15
PB10
PB11
VCAP_1
VDD
VDD
VSS
PE1
PE0
PB9
PB8
BOOT0
PB7
PB6
PB5
PB4
PB3
PD7
PD6
PD5
PD4
PD3
PD2
PD1
PD0
PC12
PC11
PC10
PA15
PA14
ai18495c
LQFP100
PC13
PH1

---

Pinouts and pin description
Figure 12. STM32F42x WLCSP143 ballout
1.
The above figure shows the package bump view.
VBAT
PDR
_ON
MS31855V2
A
B
C
D
E
F
G
H
J
K
L
M
N
PE4
PC14
PC15
PF0
PF3
PF8
PH0
PC1
VREF
+
PA3
BYPASS_
REG
PE1
PE0
PE3
PC13
VDD
PF2
PF6
PH1
PC2
VSSA
VDDA
PC1
PA4
PA6
PB8
PB6
PG15
PG12
PD7
PD5
PD2
PC10
VDD
PB9
PB7
PB3
PG11
PD4
PD3
PD0
PC11
PA14
BOOT
PB5
PB4
PG10
VDD
PD1
PC12
PA15
VDD
PE5
PE2
VDD
PG13
PA10
PA11
PA13
VSS
VCAP
_2
PF1
PE6
VSS
VDD
PG13
PG9
PC8
PC9
PA9
PA12
PF4
PF5
PF7
PG14
VSS
PD6
PC7
PC6
PA8
PF10
PF9
VDD
PG5
PG4
PG6
PG3
PG8
VDD
NRST
PC0
VSS
PD12
PD13
PD10
VSS
VSS
PG7
PC3
PF13
PF14
PG1
PE11
PB14
PD11
PD15
PA0
PA1
PB1
VDD
VDD
VDD
VDD
VDD
PE10
PB15
PD14
PG2
PA2
PA7
PB2
PE7
PE12
PE15
PD8
VDD
PA5
PC4
PF11
PF15
PE8
PE14
PB10
PB12
PD9
PC5
PB0
PF12
PG0
PE9
PE13
PB11
VCAP
_1
PB13

---

Pinouts and pin description
Figure 13. STM32F42x LQFP144 pinout
1.
The above figure shows the package top view.
VDD
PDR_ON
PE1
PE0
PB9
PB8
BOOT0
PB7
PB6
PB5
PB4
PB3
PG15
VDD
VSS
PG14
PG13
PG12
PG11
PG10
PG9
PD7
PD6
VDD
VSS
PD5
PD4
PD3
PD2
PD1
PD0
PC12
PC11
PC10
PA15
PA14
PE2
VDD
PE3
VSS
PE4
PE5
PA13
PE6
PA12
VBAT
PA11
PC13
PA10
PC14
PA9
PC15
PA8
PF0
PC9
PF1
PC8
PF2
PC7
PF3
PC6
PF4
VDD
PF5
VSS
VSS
PG8
VDD
PG7
PF6
PG6
PF7
PG5
PF8
PG4
PF9
PG3
PF10
PG2
PH0
PD15
PH1
PD14
NRST
VDD
PC0
VSS
PC1
PD13
PC2
PD12
PC3
PD11
VSSA
PD10
VDD
PD9
VREF+
PD8
VDDA
PB15
PA0
PB14
PA1
PB13
PA2
PB12
PA3
VSS
VDD
PA4
PA5
PA6
PA7
PC4
PC5
PB0
PB1
PB2
PF11
PF12
VDD
PF13
PF14
PF15
PG0
PG1
PE7
PE8
PE9
VSS
VDD
PE10
PE11
PE12
PE13
PE14
PE15
PB10
PB11
VCAP_1
VDD
LQFP144
ai18496b
VCAP_2
VSS

---

Pinouts and pin description
Figure 14. STM32F42x LQFP176 pinout
1.
The above figure shows the package top view.
MS31878V1
PDR_ON
VDD
PE1
PE0
PB9
PB8
BOOT0
PB7
PB6
PB5
PB4
PB3
PG15
VDD
VSS
PG14
PG13
PG12
PG11
PG10
PG9
PD7
PD6
VDD
VSS
PD5
PD4
PD3
PD2
PD1
PD0
PC12
PC11
PC10
PI7
PI6
PE2
VDD
PE3
VSS
PE4
PE5
PA13
PE6
PA12
VBAT
PA11
PI8
PA10
PC14
PA9
PC15
PA8
PF0
PC9
PF1
PC8
PF2
PC7
PF3
PC6
PF4
VDD
PF5
VSS
PG8
PG7
PF6
PG6
PF7
PG5
PF8
PG4
PF9
PG3
PF10
PG2
PH0
PD15
PH1
PD14
NRST
VDD
PC0
VSS
PC1
PD13
PC2
PD12
PC3
PD11
PD10
PD9
VREF+
PD8
PB15
PA0
PB14
PA1
PB13
PA2
PB12
PA3
BYPASS_REG
VDD
PA4
PA5
PA6
PA7
PC4
PC5
PB0
PB1
PB2
PF11
PF12
VSS
VDD
PF13
PF14
PF15
PG0
PG1
PE7
PE8
PE9
VSS
VDD
PE10
PE11
PE12
PE13
PE14
PE15
PB10
PB11
VCAP_1
VDD
LQFP176
VCAP_2
PI4
PA15
PA14
VDD
VSS
PI3
PI2
PI5
PH4
PH5
PH6
PH7
PH8
PH9
PH10
PH11
PI1
PI0
PH15
PH14
PH13
VDD
VSS
PH12
PC13
PI9
PI10
PI11
VSS
PH2
PH3
VDD
VSS
VDD
VDD
VSSA
VDDA

---

Pinouts and pin description
Figure 15. STM32F42x LQFP208 pinout
1.
The above figure shows the package top view.
MS30422V2
PI7
PI6
PI5
PI4
VDD
PDR_ON
VSS
PE1
PE0
PB9
PB8
BOOT0
PB7
PB6
PB5
PB4
PB3
PG15
PK7
PK6
PK5
PK4
PK3
VDD
VSS
PG14
PG13
PG12
PG11
PG10
PG9
PJ15
PJ14
PJ13
PJ12
PD7
PD6
VDD
VSS
PD5
PD4
PD3
PD2
PD1
PD0
PC12
PC11
PC10
PA15
PA14
VDD
PI3
PE2
PI2
PE3
PI1
PE4
PI0
PE5
PH15
PE6
PH14
VBAT
PH13
PI8
VDD
PC13
VSS
PC14
VCAP2
PC15
PA13
PI9
PA12
PI10
PA11
PI11
PA10
VSS
PA9
VDD
PA8
PF0
PC9
PF1
PC8
PF2
PC7
PI12
PC6
PI13
VDD
PI14
VSS
PF3
PG8
PF4
PG7
PF5
PG6
VSS
LQFP208
PG5
VDD
PG4
PF6
PG3
PF7
PG2
PF8
PK2
PF9
PK1
PF10
PK0
PH0
VSS
PH1
VDD
NRST
PJ11
PC0
PJ10
PC1
PJ9
PC2
PJ8
PC3
PJ7
VDD
PJ6
VSSA
PD15
VREF+
PD14
VDDA
VDD
PA0
VSS
PA1
PD13
PA2
PD12
PH2
PD11
PH3
PD10
PH4
PD9
PH5
PD8
PA3
PB15
VSS
PB14
VDD
PB13
PA4
PA5
PA6
PA7
PC4
PC5
VDD
VSS
PB0
PB1
PB2
PI15
PJ0
PJ1
PJ2
PJ3
PJ4
PF11
PF12
VSS
VDD
PF13
PF14
PF15
PG0
PG1
PE7
PE8
PE9
VSS
VDD
PE10
PE11
PE12
PE13
PE14
PE15
PB10
PB11
VCAP1
VSS
VDD
PJ5
PH6
PH7
PH8
PH9
PH10
PH11
PH12
VDD
PB12

---

Pinouts and pin description
Figure 16. STM32F42x UFBGA169 ballout
1.
The above figure shows the package top view.
2.
The 4 corners balls, A1, A13, N1 and N13, are not bonded internally and should be left not connected on the PCB.
MS33732V1
J
K
L
B
A
C
D
E
F
G
H
M
N
PE2
PE3
PE4
PE5
PE6
VBAT
PC13
PC14
PC15
PI9
PI10
VSS
VDD
PF0
PF1
PF2
PF3
PF4
PF5
VSS
VDD
PF10
PH0
PH1
NRST
PC0
PC1
PC2
PC3
VSSA
VREF-
VREF+
VDDA
PA0
PA1
PA2
PH2
PH3
PH4
PH5
PA3
BYPASS
_REG
VDD
PA4
PA5
PA6
PA7
PC4
PC5
PB0
PB1
PB2
PF11
PF12
VSS
VDD
PF13
PF14
PF15
PG0
PG1
PE7
PE8
PE9
VSS
VDD
PE10
PE11
PE12
PE13
PE14
PE15
PB10
PB11
VCAP
_1
VDD
PH6
PH7
PH8
PH9
PH10
PH11
PH12
VSS
VDD
PB12
PB13
PB14
PB15
PD8
PD9
PD10
PD11
PD12
PD13
VDD
PD14
PD15
PG2
PG4
PG5
PG6
PG7
PG8
VSS
VDD
PC6
PC7
PC8
PC9
PA8
PA9
PA10
PA11
PA12
PA13
VCAP
_2
VSS
VDD
PH13
PH14
PH15
PI0
PI1
PI2
PI3
VSS
VDD
PA14
PC10
PC11
PC12
PD0
PD1
PD2
PD3
PD4
PD5
PD6
PD7
PG10
PG11
PG12
VSS
VDD
VDD
PG15
PB3
PB4
PB5
PB6
PB7
BOOT0
PB8
PB9
PE0
PE1
VSS
PDR
_ON
PI4
PI5
PI6
PI7
VDD
PA15

---

Pinouts and pin description
Figure 17. STM32F42x UFBGA176 ballout
1.
The above figure shows the package top view.
ai18497c
A
PE3
PE2
PE1
PE0
PB8
PB5
PG14
PG13
PB4
PB3
PD7
PC12
PA15
PA14
PA13
B
PE4
PE5
PE6
PB9
PB7
PB6
PG15
PG12
PG11
PG10
PD6
PD0
PC11
PC10
PA12
C
VBAT
PI7
PI6
PI5
PDR_ON
VDD
VDD
VDD
VDD
PG9
PD5
PD1
PI3
PI2
PA11
D
PC13
PI8
PI9
PI4
BOOT0
VSS
VSS
VSS
PD4
PD3
PD2
PH15
PI1
PA10
E
PC14
PF0
PI10
PI11
PH13
PH14
PI0
PA9
F
PC15
VSS
VDD
PH2
VSS
VSS
VSS
VSS
VSS
VSS
VCAP2
PC9
PA8
G
PH0
VSS
VDD
PH3
VSS
VSS
VSS
VSS
VSS
VSS
VDD
PC8
PC7
H
PH1
PF2
PF1
PH4
VSS
VSS
VSS
VSS
VSS
VSS
VDD
PG8
PC6
J
NRST
PF3
PH5
VSS
VSS
VSS
VSS
VSS
VDD
VDD
PG7
PG6
K
PF7
PF6
PF4
VDD
VSS
VSS
VSS
VSS
VSS
PH12
PG5
PG4
PG3
L
PF10
PF9
PF5
BYPASS_
REG
PH11
PH10
PD15
PG2
M
VSSA
PC0
PF8
PC1
PC2
PC3
PB2
PG1
VSS
VSS
VCAP_1
PH6
PH8
PH9
PD14
PD13
N
VREF-
PA0
PA4
PC4
PF13
PG0
VDD
VDD
VDD
PE13
PH7
PD12
PD11
PD10
P
VREF+
PA2
PA6
PA5
PC5
PF12
PF15
PE8
PE9
PE11
PE14
PB12
PB13
PD9
PD8
R
VDDA
PA3
PA7
PB1
PB0
PF11
PF14
PE7
PE10
PE12
PE15
PB10
PB11
PB14
PB15
VSS
PA1

---

Pinouts and pin description
Figure 18. STM32F42x TFBGA216 ballout
1.
The above figure shows the package top view.
MS30423V2
A
PE4
PE3
PE2
PG14
PE1
PE0
PB8
PB5
PB4
PB3
PD7
PC12
PA15
PA14
PA13
B
PE5
PE6
PG13
PB9
PB7
PB6
PG15
PG11
PJ13
PJ12
PD6
PD0
PC11
PC10
PA12
C
VBAT
PI8
PI4
PK7
PK6
PK5
PG12
PG10
PJ14
PD5
PD3
PD1
PI3
PI2
PA11
D
PC13
PF0
PI5
PI7
PI10
PI6
PK4
PK3
PG9
PJ15
PD4
PD2
PH15
PI1
PA10
E
PC14
PF1
PI12
PI9
PDR_
ON
BOOT0 VDD
VDD
VDD
VDD
VCAP2
PH13
PH14
PI0
PA9
F
PC15
VSS
PI11
VDD
VDD
VSS
VSS
VDD
PK1
PK2
PC9
PA8
G
PH0
PF2
PI13
PI15
VDD
VSS
VDD
PJ11
PK0
PC8
PC7
H
PH1
PI14
PH4
VDD
VSS
VSS
VDD
PJ8
PJ10
PG8
PC6
J
NRST
PF4
PH5
PH3
VDD
VSS
VSS
VDD
PJ7
PJ9
PG7
PG6
K
PF7
PF6
PF5
PH2
VDD
VSS
VSS
VSS
VSS
VSS
VDD
PJ6
PD15
PB13
PD10
L
PF10
PF9
PF8
PC3
BYPASS-
REG
VSS
VDD
VDD
VDD
VDD
VCAP1
PD14
PB12
PD9
PD8
M
VSSA
PC0
PC1
PC2
PB2
PF12
PG1
PF15
PJ4
PD12
PD13
PG3
PG2
PJ5
PH12
N
VREF-
PA1
PA0
PA4
PC4
PF13
PG0
PJ3
PE8
PD11
PG5
PG4
PH7
PH9
PH11
VREF+
PA2
PA6
PA5
PC5
PF14
PJ2
PF11
PE9
PE11
PE14
PB10
PH6
PH8
PH10
PA3
PA7
PB1
PB0
PJ0
PJ1
PE7
PE10
PE12
PE15
PE13
PB11
PB14
PB15
VSS
PF3
P
R
VDDA
VSS
VSS
VSS

---

Pinouts and pin description
Table 9. Legend/abbreviations used in the pinout table
Name
Abbreviation
Definition
Pin name
Unless otherwise specified in brackets below the pin name, the pin function during and after
reset is the same as the actual pin name
Pin type
S
Supply pin
I
Input only pin
I/O
Input / output pin
I/O structure
FT
5 V tolerant I/O
TTa
3.3 V tolerant I/O directly connected to ADC
B
Dedicated BOOT0 pin
RST
Bidirectional reset pin with  weak pull-up resistor
Notes
Unless otherwise specified by a note, all I/Os are set as floating inputs during and after reset
Alternate
functions
Functions selected through GPIOx_AFR registers
Additional
functions
Functions directly selected/enabled through peripheral registers
Table 10. STM32F427xx and STM32F429xx pin and ball definitions
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216
B2
A2
D8
A3
PE2
I/O
FT
-
TRACECLK,
SPI4_SCK,
SAI1_MCLK_A,
ETH_MII_TXD3,
FMC_A23, EVENTOUT
-
C1
A1
C10
A2
PE3
I/O
FT
-
TRACED0,
SAI1_SD_B,
FMC_A19, EVENTOUT
-
C2
B1
B11
A1
PE4
I/O
FT
-
TRACED1, SPI4_NSS,
SAI1_FS_A, FMC_A20,
DCMI_D4, LCD_B0,
EVENTOUT
-

---

Pinouts and pin description
D1
B2
D9
B1
PE5
I/O
FT
-
TRACED2, TIM9_CH1,
SPI4_MISO,
SAI1_SCK_A,
FMC_A21, DCMI_D6,
LCD_G0, EVENTOUT
-
D2
B3
E8
B2
PE6
I/O
FT
-
TRACED3, TIM9_CH2,
SPI4_MOSI,
SAI1_SD_A,
FMC_A22, DCMI_D7,
LCD_G1, EVENTOUT
-
-
-
-
-
-
-
-
G6
VSS
S
-
-
-
-
-
-
-
-
-
-
-
F5
VDD
S
-
-
-
-
E5
C1
C11
C1
VBAT
S
-
-
-
-
-
-
NC
(3)
D2
-
C2
PI8
I/O
FT
(4)
(5)
EVENTOUT
TAMP_2
E4
D1
D10
D1
PC13
I/O
FT
(4)
(5)
EVENTOUT
TAMP_1
E1
E1
D11
E1
PC14-
OSC32_IN
(PC14)
I/O
FT
(4)
(5)
EVENTOUT
OSC32_IN
(6)
F1
F1
E11
F1
PC15-
OSC32_OUT
(PC15)
I/O
FT
(4)
(5)
EVENTOUT
OSC32_
OUT(6)
-
-
-
-
-
-
-
G5
VDD
S
-
-
-
-
-
-
E2
D3
-
E4
PI9
I/O
FT
-
CAN1_RX, FMC_D30,
LCD_VSYNC,
EVENTOUT
-
-
-
E3
E3
-
D5
PI10
I/O
FT
-
ETH_MII_RX_ER,
FMC_D31,
LCD_HSYNC,
EVENTOUT
-
-
-
NC
(3)
E4
-
F3
PI11
I/O
FT
-
OTG_HS_ULPI_DIR,
EVENTOUT
-
-
-
F6
F2
E7
F2
VSS
S
-
-
-
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
-
-
F4
F3
E10
F4
VDD
S
-
-
-
-
-
F2
E2
F11
D2
PF0
I/O
FT
-
I2C2_SDA, FMC_A0,
EVENTOUT
-
-
F3
H3
E9
E2
PF1
I/O
FT
-
I2C2_SCL, FMC_A1,
EVENTOUT
-
-
G5
H2
F10
G2
PF2
I/O
FT
-
I2C2_SMBA, FMC_A2,
EVENTOUT
-
-
-
-
-
-
-
E3
PI12
I/O
FT
-
LCD_HSYNC,
EVENTOUT
-
-
-
-
-
-
-
G3
PI13
I/O
FT
-
LCD_VSYNC,
EVENTOUT
-
-
-
-
-
-
-
H3
PI14
I/O
FT
LCD_CLK, EVENTOUT
-
-
G4
J2
G11
H2
PF3
I/O
FT
(6)
FMC_A3, EVENTOUT
ADC3_IN9
-
G3
J3
F9
J2
PF4
I/O
FT
(6)
FMC_A4, EVENTOUT
ADC3_
IN14
-
H3
K3
F8
K3
PF5
I/O
FT
(6)
FMC_A5, EVENTOUT
ADC3_
IN15
G7
G2
H7
H6
VSS
S
-
-
-
-
G8
G3
-
H5
VDD
S
-
-
-
-
-
NC
(3)
K2
G10
K2
PF6
I/O
FT
(6)
TIM10_CH1,
SPI5_NSS,
SAI1_SD_B,
UART7_Rx,
FMC_NIORD,
EVENTOUT
ADC3_IN4
-
NC
(3)
K1
F7
K1
PF7
I/O
FT
(6)
TIM11_CH1,
SPI5_SCK,
SAI1_MCLK_B,
UART7_Tx,
FMC_NREG,
EVENTOUT
ADC3_IN5
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
-
NC
(3)
L3
H11
L3
PF8
I/O
FT
(6)
SPI5_MISO,
SAI1_SCK_B,
TIM13_CH1,
FMC_NIOWR,
EVENTOUT
ADC3_IN6
-
NC
(3)
L2
G8
L2
PF9
I/O
FT
(6)
SPI5_MOSI,
SAI1_FS_B,
TIM14_CH1, FMC_CD,
EVENTOUT
ADC3_IN7
-
H1
L1
G9
L1
PF10
I/O
FT
(6)
FMC_INTR,
DCMI_D11, LCD_DE,
EVENTOUT
ADC3_IN8
G2
G1
J11
G1
PH0-OSC_IN
(PH0)
I/O
FT
-
EVENTOUT
OSC_IN(6)
G1
H1
H10
H1
PH1-
OSC_OUT
(PH1)
I/O
FT
-
EVENTOUT
OSC_OUT
(6)
H2
J1
H9
J1
NRST
I/O
RS
T
-
-
 -
G6
M2
H8
M2
PC0
I/O
FT
(6)
OTG_HS_ULPI_STP,
FMC_SDNWE,
EVENTOUT
ADC123_
IN10
H5
M3
K11
M3
PC1
I/O
FT
(6)
ETH_MDC,
EVENTOUT
ADC123_
IN11
H6
M4
J10
M4
PC2
I/O
FT
(6)
SPI2_MISO,
I2S2ext_SD,
OTG_HS_ULPI_DIR,
ETH_MII_TXD2,
FMC_SDNE0,
EVENTOUT
ADC123_
IN12
H7
M5
J9
L4
PC3
I/O
FT
(6)
SPI2_MOSI/I2S2_SD,
OTG_HS_ULPI_NXT,
ETH_MII_TX_CLK,
FMC_SDCKE0,
EVENTOUT
ADC123_
IN13
-
-
G7
J5
VDD
S
-
-
-
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
-
-
-
-
-
-
-
J6
VSS
S
-
-
-
-
J1
M1
K10
M1
VSSA
S
-
-
-
-
-
-
J2
N1
-
-
-
N1
VREF–
S
-
-
-
-
J3
P1
L11
P1
VREF+
S
-
-
-
-
J4
R1
L10
R1
VDDA
S
-
-
-
-
J5
N3
K9
N3
PA0-WKUP
(PA0)
I/O
FT
(7)
TIM2_CH1/TIM2_ETR,
TIM5_CH1, TIM8_ETR,
USART2_CTS,
UART4_TX,
ETH_MII_CRS,
EVENTOUT
ADC123_
IN0/WKUP
(6)
K1
N2
K8
N2
PA1
I/O
FT
(6)
TIM2_CH2, TIM5_CH2,
USART2_RTS,
UART4_RX,
ETH_MII_RX_CLK/ET
H_RMII_REF_CLK,
EVENTOUT
ADC123_
IN1
K2
P2
L9
P2
PA2
I/O
FT
(6)
TIM2_CH3, TIM5_CH3,
TIM9_CH1,
USART2_TX,
ETH_MDIO,
EVENTOUT
ADC123_
IN2
-
-
L2
F4
-
K4
PH2
I/O
FT
-
ETH_MII_CRS,
FMC_SDCKE0,
LCD_R0, EVENTOUT
-
-
-
L1
G4
-
J4
PH3
I/O
FT
-
ETH_MII_COL,
FMC_SDNE0,
LCD_R1, EVENTOUT
-
-
-
M2
H4
-
H4
PH4
I/O
FT
-
I2C2_SCL,
OTG_HS_ULPI_NXT,
EVENTOUT
-
-
-
L3
J4
-
J3
PH5
I/O
FT
-
I2C2_SDA, SPI5_NSS,
FMC_SDNWE,
EVENTOUT
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
K3
R2
M11
R2
PA3
I/O
FT
(6)
TIM2_CH4, TIM5_CH4,
TIM9_CH2,
USART2_RX,
OTG_HS_ULPI_D0,
ETH_MII_COL,
LCD_B5, EVENTOUT
ADC123_
IN3
-
-
-
K6
VSS
S
-
-
-
-
-
-
M1
L4
N11
-
L5
BYPASS_
REG
I
FT
-
-
-
J11
K4
J8
K5
VDD
S
-
-
-
-
N2
N4
M1
N4
PA4
I/O TTa
(6)
SPI1_NSS,
SPI3_NSS/I2S3_WS,
USART2_CK,
OTG_HS_SOF,
DCMI_HSYNC,
LCD_VSYNC,
EVENTOUT
ADC12_
IN4 /DAC_
OUT1
M3
P4
M9
P4
PA5
I/O TTa
(6)
TIM2_CH1/TIM2_ETR,
TIM8_CH1N,
SPI1_SCK,
OTG_HS_ULPI_CK,
EVENTOUT
ADC12_
IN5/DAC_
OUT2
N3
P3
N10
P3
PA6
I/O
FT
(6)
TIM1_BKIN,
TIM3_CH1,
TIM8_BKIN,
SPI1_MISO,
TIM13_CH1,
DCMI_PIXCLK,
LCD_G2, EVENTOUT
ADC12_
IN6
K4
R3
L8
R3
PA7
I/O
FT
(6)
TIM1_CH1N,
TIM3_CH2,
TIM8_CH1N,
SPI1_MOSI,
TIM14_CH1,
ETH_MII_RX_DV/ETH
_RMII_CRS_DV,
EVENTOUT
ADC12_
IN7
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
L4
N5
M8
N5
PC4
I/O
FT
(6)
ETH_MII_RXD0/ETH_
RMII_RXD0,
EVENTOUT
ADC12_
IN14
M4
P5
N9
P5
PC5
I/O
FT
(6)
ETH_MII_RXD1/ETH_
RMII_RXD1,
EVENTOUT
ADC12_
IN15
-
-
-
-
-
J7
L7
VDD
S
-
-
-
-
-
-
-
-
-
-
L6
VSS
S
-
-
-
-
N4
R5
N8
R5
PB0
I/O
FT
(6)
TIM1_CH2N,
TIM3_CH3,
TIM8_CH2N, LCD_R3,
OTG_HS_ULPI_D1,
ETH_MII_RXD2,
EVENTOUT
ADC12_
IN8
K5
R4
K7
R4
PB1
I/O
FT
(6)
TIM1_CH3N,
TIM3_CH4,
TIM8_CH3N, LCD_R6,
OTG_HS_ULPI_D2,
ETH_MII_RXD3,
EVENTOUT
ADC12_
IN9
L5
M6
L7
M5
PB2-BOOT1
(PB2)
I/O
FT
-
EVENTOUT
-
-
-
-
-
-
-
G4
PI15
I/O
FT
-
LCD_R0, EVENTOUT
-
-
-
-
-
-
-
R6
PJ0
I/O
FT
-
LCD_R1, EVENTOUT
-
-
-
-
-
-
-
R7
PJ1
I/O
FT
-
LCD_R2, EVENTOUT
-
-
-
-
-
-
-
P7
PJ2
I/O
FT
-
LCD_R3, EVENTOUT
-
-
-
-
-
-
-
N8
PJ3
I/O
FT
-
LCD_R4, EVENTOUT
-
-
-
-
-
-
-
M9
PJ4
I/O
FT
-
LCD_R5, EVENTOUT
-
-
M5
R6
M7
P8
PF11
I/O
FT
-
SPI5_MOSI,
FMC_SDNRAS,
DCMI_D12,
EVENTOUT
-
-
N5
P6
N7
M6
PF12
I/O
FT
-
FMC_A6, EVENTOUT
-
-
G9
M8
-
K7
VSS
S
-
-
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
-
D10
N8
-
L8
VDD
S
-
-
-
-
M6
N6
K6
N6
PF13
I/O
FT
-
FMC_A7, EVENTOUT
-
-
K7
R7
L6
P6
PF14
I/O
FT
-
FMC_A8, EVENTOUT
-
-
L7
P7
M6
M8
PF15
I/O
FT
-
FMC_A9, EVENTOUT
-
-
N6
N7
N6
N7
PG0
I/O
FT
-
FMC_A10, EVENTOUT
-
-
M7
M7
K5
M7
PG1
I/O
FT
-
FMC_A11, EVENTOUT
-
N7
R8
L5
R8
PE7
I/O
FT
-
TIM1_ETR,
UART7_Rx, FMC_D4,
EVENTOUT
-
J8
P8
M5
N9
PE8
I/O
FT
-
TIM1_CH1N,
UART7_Tx, FMC_D5,
EVENTOUT
-
K8
P9
N5
P9
PE9
I/O
FT
-
TIM1_CH1, FMC_D6,
EVENTOUT
-
-
J6
M9
H3
K8
VSS
S
-
-
-
-
G10
N9
J5
L9
VDD
S
-
-
-
L8
R9
J4
R9
PE10
I/O
FT
-
TIM1_CH2N, FMC_D7,
EVENTOUT
-
M8
P10
K4
P10
PE11
I/O
FT
-
TIM1_CH2, SPI4_NSS,
FMC_D8, LCD_G3,
EVENTOUT
-
N8
R10
L4
R10
PE12
I/O
FT
-
TIM1_CH3N,
SPI4_SCK, FMC_D9,
LCD_B4, EVENTOUT
-
H9
N11
N4
R12
PE13
I/O
FT
-
TIM1_CH3,
SPI4_MISO,
FMC_D10, LCD_DE,
EVENTOUT
-
J9
P11
M4
P11
PE14
I/O
FT
-
TIM1_CH4,
SPI4_MOSI, FMC_D11,
LCD_CLK, EVENTOUT
-
K9
R11
L3
R11
PE15
I/O
FT
-
TIM1_BKIN, FMC_D12,
LCD_R7, EVENTOUT
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
L9
R12
M3
P12
PB10
I/O
FT
-
TIM2_CH3, I2C2_SCL,
SPI2_SCK/I2S2_CK,
USART3_TX,
OTG_HS_ULPI_D3,
ETH_MII_RX_ER,
LCD_G4, EVENTOUT
-
M9
R13
N3
R13
PB11
I/O
FT
-
TIM2_CH4, I2C2_SDA,
USART3_RX,
OTG_HS_ULPI_D4,
ETH_MII_TX_EN/ETH_
RMII_TX_EN, LCD_G5,
EVENTOUT
-
N9
M10
N2
L11
VCAP_1
S
-
-
-
-
-
-
-
-
-
H2
K9
VSS
S
-
-
-
-
F8
N10
J6
L10
VDD
S
-
-
-
-
-
-
-
-
-
-
M14
PJ5
I/O
-
-
LCD_R6, EVENTOUT
-
-
-
N10
M11
-
P13
PH6
I/O
FT
-
I2C2_SMBA,
SPI5_SCK,
TIM12_CH1,
ETH_MII_RXD2,
FMC_SDNE1,
DCMI_D8, EVENTOUT
-
-
-
M10
N12
-
N13
PH7
I/O
FT
-
I2C3_SCL,
SPI5_MISO,
ETH_MII_RXD3,
FMC_SDCKE1,
DCMI_D9, EVENTOUT
-
-
-
L10
M12
-
P14
PH8
I/O
FT
-
I2C3_SDA, FMC_D16,
DCMI_HSYNC,
LCD_R2, EVENTOUT
-
-
-
K10
M13
-
N14
PH9
I/O
FT
-
I2C3_SMBA,
TIM12_CH2,
FMC_D17, DCMI_D0,
LCD_R3, EVENTOUT
-
-
-
N11
L13
-
P15
PH10
I/O
FT
-
TIM5_CH1, FMC_D18,
DCMI_D1, LCD_R4,
EVENTOUT
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
-
-
M11
L12
-
N15
PH11
I/O
FT
-
TIM5_CH2, FMC_D19,
DCMI_D2, LCD_R5,
EVENTOUT
-
-
-
L11
K12
-
M15
PH12
I/O
FT
-
TIM5_CH3, FMC_D20,
DCMI_D3, LCD_R6,
EVENTOUT
-
-
-
E7
H12
-
-
K10
VSS
S
-
-
-
-
-
-
H8
J12
-
K11
VDD
S
-
-
-
-
N12
P12
M2
L13
PB12
I/O
FT
-
TIM1_BKIN,
I2C2_SMBA,
SPI2_NSS/I2S2_WS,
USART3_CK,
CAN2_RX,
OTG_HS_ULPI_D5,
ETH_MII_TXD0/ETH_
RMII_TXD0,
OTG_HS_ID,
EVENTOUT
-
M12
P13
N1
K14
PB13
I/O
FT
-
TIM1_CH1N,
SPI2_SCK/I2S2_CK,
USART3_CTS,
CAN2_TX,
OTG_HS_ULPI_D6,
ETH_MII_TXD1/ETH_
RMII_TXD1,
EVENTOUT
OTG_HS_
VBUS
M13
R14
K3
R14
PB14
I/O
FT
-
TIM1_CH2N,
TIM8_CH2N,
SPI2_MISO,
I2S2ext_SD,
USART3_RTS,
TIM12_CH1,
OTG_HS_DM,
EVENTOUT
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
L13
R15
J3
R15
PB15
I/O
FT
-
RTC_REFIN,
TIM1_CH3N,
TIM8_CH3N,
SPI2_MOSI/I2S2_SD,
TIM12_CH2,
OTG_HS_DP,
EVENTOUT
-
L12
P15
L2
L15
PD8
I/O
FT
-
USART3_TX,
FMC_D13, EVENTOUT
-
K13
P14
M1
L14
PD9
I/O
FT
-
USART3_RX,
FMC_D14, EVENTOUT
-
K11
N15
H4
K15
PD10
I/O
FT
-
USART3_CK,
FMC_D15, LCD_B3,
EVENTOUT
-
H10
N14
K2
N10
PD11
I/O
FT
-
USART3_CTS,
FMC_A16, EVENTOUT
-
J13
N13
H6
M10
PD12
I/O
FT
-
TIM4_CH1,
USART3_RTS,
FMC_A17, EVENTOUT
-
K12
M15
H5
M11
PD13
I/O
FT
-
TIM4_CH2, FMC_A18,
EVENTOUT
-
-
-
-
-
J10
VSS
S
-
-
-
-
F7
J13
L1
J11
VDD
S
-
-
-
H11
M14
J2
L12
PD14
I/O
FT
-
TIM4_CH3, FMC_D0,
EVENTOUT
-
J12
L14
K1
K13
PD15
I/O
FT
-
TIM4_CH4, FMC_D1,
EVENTOUT
-
-
-
-
-
-
-
K12
PJ6
I/O
FT
-
LCD_R7, EVENTOUT
-
-
-
-
-
-
-
J12
PJ7
I/O
FT
-
LCD_G0, EVENTOUT
-
-
-
-
-
-
-
H12
PJ8
I/O
FT
-
LCD_G1, EVENTOUT
-
-
-
-
-
-
-
J13
PJ9
I/O
FT
-
LCD_G2, EVENTOUT
-
-
-
-
-
-
-
H13
PJ10
I/O
FT
-
LCD_G3, EVENTOUT
-
-
-
-
-
-
-
G12
PJ11
I/O
FT
-
LCD_G4, EVENTOUT
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
-
-
-
-
-
-
H11
VDD
I/O
FT
-
-
-
-
-
-
-
-
-
H10
VSS
I/O
FT
-
-
-
-
-
-
-
-
-
G13
PK0
I/O
FT
-
LCD_G5, EVENTOUT
-
-
-
-
-
-
-
F12
PK1
I/O
FT
-
LCD_G6, EVENTOUT
-
-
-
-
-
-
-
F13
PK2
I/O
FT
-
LCD_G7, EVENTOUT
-
-
H13
L15
J1
M13
PG2
I/O
FT
-
FMC_A12, EVENTOUT
-
-
NC
(3)
K15
G3
M12
PG3
I/O
FT
-
FMC_A13, EVENTOUT
-
-
H12
K14
G5
N12
PG4
I/O
FT
-
FMC_A14/FMC_BA0,
EVENTOUT
-
-
G13
K13
G6
N11
PG5
I/O
FT
-
FMC_A15/FMC_BA1,
EVENTOUT
-
-
G11
J15
G4
J15
PG6
I/O
FT
-
FMC_INT2,
DCMI_D12, LCD_R7,
EVENTOUT
-
-
G12
J14
H1
J14
PG7
I/O
FT
-
USART6_CK,
FMC_INT3,
DCMI_D13, LCD_CLK,
EVENTOUT
-
-
F13
H14
G2
H14
PG8
I/O
FT
-
SPI6_NSS,
USART6_RTS,
ETH_PPS_OUT,
FMC_SDCLK,
EVENTOUT
-
-
J7
G12
D2
G10
VSS
S
-
-
-
-
E6
H13
G1
G11
VDD
S
-
-
-
F9
H15
F2
H15
PC6
I/O
FT
-
TIM3_CH1, TIM8_CH1,
I2S2_MCK,
USART6_TX,
SDIO_D6, DCMI_D0,
LCD_HSYNC,
EVENTOUT
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
F10
G15
F3
G15
PC7
I/O
FT
-
TIM3_CH2, TIM8_CH2,
I2S3_MCK,
USART6_RX,
SDIO_D7, DCMI_D1,
LCD_G6, EVENTOUT
-
F11
G14
E4
G14
PC8
I/O
FT
-
TIM3_CH3, TIM8_CH3,
USART6_CK,
SDIO_D0, DCMI_D2,
EVENTOUT
-
F12
F14
E3
F14
PC9
I/O
FT
-
MCO2, TIM3_CH4,
TIM8_CH4, I2C3_SDA,
I2S_CKIN, SDIO_D1,
DCMI_D3, EVENTOUT
-
E13
F15
F1
F15
PA8
I/O
FT
-
MCO1, TIM1_CH1,
I2C3_SCL,
USART1_CK,
OTG_FS_SOF,
LCD_R6, EVENTOUT
-
E8
E15
E2
E15
PA9
I/O
FT
-
TIM1_CH2,
I2C3_SMBA,
USART1_TX,
DCMI_D0, EVENTOUT
OTG_FS_
VBUS
E9
D15
D5
D15
PA10
I/O
FT
-
TIM1_CH3,
USART1_RX,
OTG_FS_ID,
DCMI_D1, EVENTOUT
-
E10
C15
D4
C15
PA11
I/O
FT
-
TIM1_CH4,
USART1_CTS,
CAN1_RX, LCD_R4,
OTG_FS_DM,
EVENTOUT
-
E11
B15
E1
B15
PA12
I/O
FT
-
TIM1_ETR,
USART1_RTS,
CAN1_TX, LCD_R5,
OTG_FS_DP,
EVENTOUT
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
E12
A15
D3
A15
PA13
(JTMS-
SWDIO)
I/O
FT
-
JTMS-SWDIO,
EVENTOUT
-
D12
F13
D1
E11
VCAP_2
S
-
-
-
J10
F12
D2
F10
VSS
S
-
-
-
H4
G13
C1 150
F11
VDD
S
-
-
-
-
-
D13
E12
-
E12
PH13
I/O
FT
-
TIM8_CH1N,
CAN1_TX, FMC_D21,
LCD_G2, EVENTOUT
-
-
-
C13
E13
-
E13
PH14
I/O
FT
-
TIM8_CH2N,
FMC_D22, DCMI_D4,
LCD_G3, EVENTOUT
-
-
-
C12
D13
-
D13
PH15
I/O
FT
-
TIM8_CH3N,
FMC_D23, DCMI_D11,
LCD_G4, EVENTOUT
-
-
-
B13
E14
-
E14
PI0
I/O
FT
-
TIM5_CH4,
SPI2_NSS/I2S2_WS(8),
FMC_D24, DCMI_D13,
LCD_G5, EVENTOUT
-
-
-
C11
D14
-
D14
PI1
I/O
FT
-
SPI2_SCK/I2S2_CK(8),
FMC_D25, DCMI_D8,
LCD_G6, EVENTOUT
-
-
-
B12
C14
-
C14
PI2
I/O
FT
-
TIM8_CH4,
SPI2_MISO,
I2S2ext_SD,
FMC_D26, DCMI_D9,
LCD_G7, EVENTOUT
-
-
-
A12
C13
-
C13
PI3
I/O
FT
-
TIM8_ETR,
SPI2_MOSI/I2S2_SD,
FMC_D27, DCMI_D10,
EVENTOUT
-
-
-
D11
D9
F5
-
F9
VSS
S
-
-
-
-
-
D3
C9
A1
E10
VDD
S
-
-
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
A11
A14
B1
A14
PA14
(JTCK-
SWCLK)
I/O
FT
-
JTCK-SWCLK/
EVENTOUT
-
B11
A13
C2
A13
PA15
(JTDI)
I/O
FT
-
JTDI,
TIM2_CH1/TIM2_ETR,
SPI1_NSS,
SPI3_NSS/I2S3_WS,
EVENTOUT
-
C10
B14
A2
B14
PC10
I/O
FT
-
SPI3_SCK/I2S3_CK,
USART3_TX,
UART4_TX, SDIO_D2,
DCMI_D8, LCD_R2,
EVENTOUT
-
B10
B13
B2
B13
PC11
I/O
FT
-
I2S3ext_SD,
SPI3_MISO,
USART3_RX,
UART4_RX, SDIO_D3,
DCMI_D4, EVENTOUT
-
A10
A12
C3
A12
PC12
I/O
FT
-
SPI3_MOSI/I2S3_SD,
USART3_CK,
UART5_TX, SDIO_CK,
DCMI_D9, EVENTOUT
-
D9
B12
B3
B12
PD0
I/O
FT
-
CAN1_RX, FMC_D2,
EVENTOUT
-
C9
C12
C4
C12
PD1
I/O
FT
-
CAN1_TX, FMC_D3,
EVENTOUT
-
B9
D12
A3
D12
PD2
I/O
FT
-
TIM3_ETR,
UART5_RX,
SDIO_CMD,
DCMI_D11,
EVENTOUT
-
A9
D11
B4
C11
PD3
I/O
FT
-
SPI2_SCK/I2S2_CK,
USART2_CTS,
FMC_CLK, DCMI_D5,
LCD_G7, EVENTOUT
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
D8
D10
B5
D11
PD4
I/O
FT
-
USART2_RTS,
FMC_NOE,
EVENTOUT
-
C8
C11
A4
C10
PD5
I/O
FT
-
USART2_TX,
FMC_NWE,
EVENTOUT
-
-
-
D8
-
F8
VSS
S
-
-
-
-
D6
C8
C5
E9
VDD
S
-
-
-
B8
B11
F4
B11
PD6
I/O
FT
-
SPI3_MOSI/I2S3_SD,
SAI1_SD_A,
USART2_RX,
FMC_NWAIT,
DCMI_D10, LCD_B2,
EVENTOUT
-
A8
A11
A5
A11
PD7
I/O
FT
-
USART2_CK,
FMC_NE1/FMC_NCE2
, EVENTOUT
-
-
-
-
-
-
-
B10
PJ12
I/O
FT
-
LCD_B0, EVENTOUT
-
-
-
-
-
-
-
B9
PJ13
I/O
FT
-
LCD_B1, EVENTOUT
-
-
-
-
-
-
-
C9
PJ14
I/O
FT
-
LCD_B2, EVENTOUT
-
-
-
-
-
-
-
D10
PJ15
I/O
FT
-
LCD_B3, EVENTOUT
-
-
NC
(3)
C10
E5
D9
PG9
I/O
FT
-
USART6_RX,
FMC_NE2/FMC_NCE3
, DCMI_VSYNC(9),
EVENTOUT
-
-
C7
B10
C6
C8
PG10
I/O
FT
-
LCD_G3,
FMC_NCE4_1/FMC_N
E3, DCMI_D2,
LCD_B2, EVENTOUT
-
-
B7
B9
B6
B8
PG11
I/O
FT
-
ETH_MII_TX_EN/ETH_
RMII_TX_EN,
FMC_NCE4_2,
DCMI_D3, LCD_B3,
EVENTOUT
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
-
A7
B8
A6
C7
PG12
I/O
FT
-
SPI6_MISO,
USART6_RTS,
LCD_B4, FMC_NE4,
LCD_B1, EVENTOUT
-
-
NC
(3)
A8
D6
B3
PG13
I/O
FT
-
SPI6_SCK,
USART6_CTS,
ETH_MII_TXD0/ETH_
RMII_TXD0, FMC_A24,
EVENTOUT
-
-
NC
(3)
A7
F6
A4
PG14
I/O
FT
-
SPI6_MOSI,
USART6_TX,
ETH_MII_TXD1/ETH_
RMII_TXD1, FMC_A25,
EVENTOUT
-
-
D7
D7
-
F7
VSS
S
-
-
-
-
L6
C7
E6
E8
VDD
S
-
-
-
-
-
-
-
-
-
D8
PK3
I/O
FT
-
LCD_B4, EVENTOUT
-
-
-
-
-
-
-
D7
PK4
I/O
FT
-
LCD_B5, EVENTOUT
-
-
-
-
-
-
-
C6
PK5
I/O
FT
-
LCD_B6, EVENTOUT
-
-
-
-
-
-
-
C5
PK6
I/O
FT
-
LCD_B7, EVENTOUT
-
-
-
-
-
-
-
C4
PK7
I/O
FT
-
LCD_DE, EVENTOUT
-
-
C6
B7
A7
B7
PG15
I/O
FT
-
USART6_CTS,
FMC_SDNCAS,
DCMI_D13,
EVENTOUT
-
B6
A10
B7
A10
PB3
(JTDO/TRACE
SWO)
I/O
FT
-
JTDO/TRACESWO,
TIM2_CH2, SPI1_SCK,
SPI3_SCK/I2S3_CK,
EVENTOUT
-
A6
A9
C7
A9
PB4
(NJTRST)
I/O
FT
-
NJTRST, TIM3_CH1,
SPI1_MISO,
SPI3_MISO,
I2S3ext_SD,
EVENTOUT
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
D5
A6
C8
A8
PB5
I/O
FT
-
TIM3_CH2,
I2C1_SMBA,
SPI1_MOSI,
SPI3_MOSI/I2S3_SD,
CAN2_RX,
OTG_HS_ULPI_D7,
ETH_PPS_OUT,
FMC_SDCKE1,
DCMI_D10,
EVENTOUT
-
C5
B6
A8
B6
PB6
I/O
FT
-
TIM4_CH1, I2C1_SCL,
USART1_TX,
CAN2_TX,
FMC_SDNE1,
DCMI_D5, EVENTOUT
-
B5
B5
B8
B5
PB7
I/O
FT
-
TIM4_CH2, I2C1_SDA,
USART1_RX,
FMC_NL,
DCMI_VSYNC,
EVENTOUT
-
A5
D6
C9
E6
BOOT0
I
B
-
-
VPP
D4
A5
A9
A7
PB8
I/O
FT
-
TIM4_CH3,
TIM10_CH1,
I2C1_SCL, CAN1_RX,
ETH_MII_TXD3,
SDIO_D4, DCMI_D6,
LCD_B6, EVENTOUT
-
C4
B4
B9
B4
PB9
I/O
FT
-
TIM4_CH4,
TIM11_CH1,
I2C1_SDA,
SPI2_NSS/I2S2_WS,
CAN1_TX, SDIO_D5,
DCMI_D7, LCD_B7,
EVENTOUT
-
B4
A4
169 B10 200
A6
PE0
I/O
FT
-
TIM4_ETR,
UART8_RX,
FMC_NBL0, DCMI_D2,
EVENTOUT
-
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
A4
A3
170 A10 201
A5
PE1
I/O
FT
-
UART8_Tx,
FMC_NBL1, DCMI_D3,
EVENTOUT
-
-
F5
D5
-
-
F6
VSS
S
-
-
-
-
C3
C6
A11
E5
PDR_ON
S
-
-
-
100 144
K6
C5
D7
E7
VDD
S
-
-
-
-
-
B3
D4
-
C3
PI4
I/O
FT
-
TIM8_BKIN,
FMC_NBL2, DCMI_D5,
LCD_B4, EVENTOUT
-
-
-
A3
C4
-
D3
PI5
I/O
FT
-
TIM8_CH1,
FMC_NBL3,
DCMI_VSYNC,
LCD_B5, EVENTOUT
-
-
-
A2
C3
-
D6
PI6
I/O
FT
-
TIM8_CH2, FMC_D28,
DCMI_D6, LCD_B6,
EVENTOUT
-
-
-
B1
C2
-
D4
PI7
I/O
FT
-
TIM8_CH3, FMC_D29,
DCMI_D7, LCD_B7,
EVENTOUT
-
1.
Function availability depends on the chosen device.
2.
On the UFBGA176 package, the balls F6, F7, F8, F9, F10, G6, G7, G8, G9, G10, H6, H7, H8, H9, H10, J6, J7, J8, J9, J10,
K6, K7, K8, K9, and K10 are connected to VSS. Their purpose is heat dissipation and package mechanical stability
3.
NC (not-connected) pins are not bonded. They must be configured by software to output push-pull and forced to 0 in the
output data register to avoid extra current consumption in low-power modes.
4.
PC13, PC14, PC15, and PI8 are supplied through the power switch. Since the switch only sinks a limited amount of current
(3 mA), the use of GPIOs PC13 to PC15 and PI8 in output mode is limited:
- The speed should not exceed 2 MHz with a maximum load of 30 pF.
- These I/Os must not be used as a current source (for example, to drive an LED).
5.
The main function after the first backup domain power-up. Later on, it depends on the contents of the RTC registers even
after reset (because these registers are not reset by the main reset). For details on how to manage these I/Os, refer to the
RTC register description sections in the STM32F4xx reference manual, available from the STMicroelectronics website:
www.st.com.
6.
FT = 5 V tolerant except when in analog mode or oscillator mode (for PC14, PC15, PH0, and PH1).
7.
If the device is delivered in a WLCSP143, UFBGA169, UFBGA176, LQFP176 or TFBGA216 package, and the
BYPASS_REG pin is set to VDD (Regulator OFF/internal reset ON mode), then PA0 is used as an internal Reset (active
low).
8.
PI0 and PI1 cannot be used for I2S2 full-duplex mode.
9.
The DCMI_VSYNC alternate function on PG9 is only available on silicon revision 3.
Table 10. STM32F427xx and STM32F429xx pin and ball definitions (continued)
Pin number
Pin name
(function
after reset)(1)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP100
LQFP144
UFBGA169
UFBGA176(2)
LQFP176
WLCSP143
LQFP208
TFBGA216

---

Pinouts and pin description
Table 11. FMC pin definition
Pin name
CF
NOR/PSRAM/
SRAM
NOR/PSRAM
Mux
NAND16
SDRAM
PF0
A0
A0
-
-
A0
PF1
A1
A1
-
-
A1
PF2
A2
A2
-
-
A2
PF3
A3
A3
-
-
A3
PF4
A4
A4
-
-
A4
PF5
A5
A5
-
-
A5
PF12
A6
A6
-
-
A6
PF13
A7
A7
-
-
A7
PF14
A8
A8
-
-
A8
PF15
A9
A9
-
-
A9
PG0
A10
A10
-
-
A10
PG1
-
A11
-
-
A11
PG2
-
A12
-
-
A12
PG3
-
A13
-
-
-
PG4
-
A14
-
-
BA0
PG5
-
A15
-
-
BA1
PD11
-
A16
A16
CLE
-
PD12
-
A17
A17
ALE
-
PD13
-
A18
A18
-
-
PE3
-
A19
A19
-
-
PE4
-
A20
A20
-
-
PE5
-
A21
A21
-
-
PE6
-
A22
A22
-
-
PE2
-
A23
A23
-
-
PG13
-
A24
A24
-
-
PG14
-
A25
A25
-
-
PD14
D0
D0
DA0
D0
D0
PD15
D1
D1
DA1
D1
D1
PD0
D2
D2
DA2
D2
D2
PD1
D3
D3
DA3
D3
D3
PE7
D4
D4
DA4
D4
D4
PE8
D5
D5
DA5
D5
D5
PE9
D6
D6
DA6
D6
D6
PE10
D7
D7
DA7
D7
D7

---

Pinouts and pin description
PE11
D8
D8
DA8
D8
D8
PE12
D9
D9
DA9
D9
D9
PE13
D10
D10
DA10
D10
D10
PE14
D11
D11
DA11
D11
D11
PE15
D12
D12
DA12
D12
D12
PD8
D13
D13
DA13
D13
D13
PD9
D14
D14
DA14
D14
D14
PD10
D15
D15
DA15
D15
D15
PH8
-
D16
-
-
D16
PH9
-
D17
-
-
D17
PH10
-
D18
-
-
D18
PH11
-
D19
-
-
D19
PH12
-
D20
-
-
D20
PH13
-
D21
-
-
D21
PH14
-
D22
-
-
D22
PH15
-
D23
-
-
D23
PI0
-
D24
-
-
D24
PI1
-
D25
-
-
D25
PI2
-
D26
-
-
D26
PI3
-
D27
-
-
D27
PI6
-
D28
-
-
D28
PI7
-
D29
-
-
D29
PI9
-
D30
-
-
D30
PI10
-
D31
-
-
D31
PD7
-
NE1
NE1
NCE2
-
PG9
-
NE2
NE2
NCE3
-
PG10
NCE4_1
NE3
NE3
-
-
PG11
NCE4_2
-
-
-
-
PG12
-
NE4
NE4
-
-
PD3
-
CLK
CLK
-
-
PD4
NOE
NOE
NOE
NOE
-
PD5
NWE
NWE
NWE
NWE
-
PD6
NWAIT
NWAIT
NWAIT
NWAIT
-
PB7
-
NL(NADV)
NL(NADV)
-
-
Table 11. FMC pin definition (continued)
Pin name
CF
NOR/PSRAM/
SRAM
NOR/PSRAM
Mux
NAND16
SDRAM

---

Pinouts and pin description
PF6
NIORD
-
-
-
-
PF7
NREG
-
-
-
-
PF8
NIOWR
-
-
-
-
PF9
CD
-
-
-
-
PF10
INTR
-
-
-
-
PG6
-
-
-
INT2
-
PG7
-
-
-
INT3
-
PE0
-
NBL0
NBL0
-
NBL0
PE1
-
NBL1
NBL1
-
NBL1
PI4
-
NBL2
-
-
NBL2
PI5
-
NBL3
-
-
NBL3
PG8
-
-
-
-
SDCLK
PC0
-
-
-
-
SDNWE
PF11
-
-
-
-
SDNRAS
PG15
-
-
-
-
SDNCAS
PH2
-
-
-
-
SDCKE0
PH3
-
-
-
-
SDNE0
PH6
-
-
-
-
SDNE1
PH7
-
-
-
-
SDCKE1
PH5
-
-
-
-
SDNWE
PC2
-
-
-
-
SDNE0
PC3
-
-
-
-
SDCKE0
PB5
-
-
-
-
SDCKE1
PB6
-
-
-
-
SDNE1
Table 11. FMC pin definition (continued)
Pin name
CF
NOR/PSRAM/
SRAM
NOR/PSRAM
Mux
NAND16
SDRAM

---

Pinouts and pin description
Table 12. STM32F427xx and STM32F429xx alternate function mapping
Port
AF0
AF1
AF2
AF3
AF4
AF5
AF6
AF7
AF8
AF9
AF10
AF11
AF12
AF13
AF14
AF15
SYS
TIM1/2
TIM3/4/5
TIM8/9/
I2C1/
SPI1/2/
3/4/5/6
SPI2/3/
SAI1
SPI3/
USART1/
USART6/
UART4/5/7
/8
CAN1/2/
TIM12/13/14
/LCD
OTG2_HS
/OTG1_
FS
ETH
FMC/SDIO
/OTG2_FS
DCMI
LCD
SYS
Port A
PA0
-
TIM2_
CH1/TIM2
_ETR
TIM5_
CH1
TIM8_
ETR
-
-
-
USART2_
CTS
UART4_TX
-
-
ETH_MII_
CRS
-
-
-
EVEN
TOUT
PA1
-
TIM2_
CH2
TIM5_
CH2
-
-
-
-
USART2_
RTS
UART4_RX
-
-
ETH_MII_
RX_CLK/E
TH_RMII_
REF_CLK
-
-
-
EVEN
TOUT
PA2
-
TIM2_
CH3
TIM5_
CH3
TIM9_
CH1
-
-
-
USART2_
TX
-
-
-
ETH_
MDIO
-
-
-
EVEN
TOUT
PA3
-
TIM2_
CH4
TIM5_
CH4
TIM9_
CH2
-
-
-
USART2_
RX
-
-
OTG_HS_
ULPI_D0
ETH_MII_
COL
-
-
LCD_B5
EVEN
TOUT
PA4
-
-
-
-
-
SPI1_
NSS
SPI3_
NSS/
I2S3_WS
USART2_
CK
-
-
-
-
OTG_HS_
SOF
DCMI_
HSYNC
LCD_
VSYNC
EVEN
TOUT
PA5
-
TIM2_
CH1/TIM2
_ETR
-
TIM8_
CH1N
-
SPI1_
SCK
-
-
-
-
OTG_HS_
ULPI_CK
-
-
-
-
EVEN
TOUT
PA6
-
TIM1_
BKIN
TIM3_
CH1
TIM8_
BKIN
-
SPI1_
MISO
-
-
-
TIM13_CH1
-
-
-
DCMI_
PIXCLK
LCD_G2
EVEN
TOUT
PA7
-
TIM1_
CH1N
TIM3_
CH2
TIM8_
CH1N
-
SPI1_
MOSI
-
-
-
TIM14_CH1
-
ETH_MII_
RX_DV/
ETH_RMII
_CRS_DV
-
-
-
EVEN
TOUT
PA8
MCO1
TIM1_
CH1
-
-
I2C3_
SCL
-
-
USART1_
CK
-
-
OTG_FS_
SOF
-
-
-
LCD_R6
EVEN
TOUT
PA9
-
TIM1_
CH2
-
-
I2C3_
SMBA
-
-
USART1_
TX
-
-
-
-
-
DCMI_
D0
-
EVEN
TOUT
PA10
-
TIM1_
CH3
-
-
-
-
-
USART1_
RX
-
-
OTG_FS_
ID
-
-
DCMI_
D1
-
EVEN
TOUT
PA11
-
TIM1_
CH4
-
-
-
-
-
USART1_
CTS
-
CAN1_RX
OTG_FS_
DM
-
-
-
LCD_R4
EVEN
TOUT
PA12
-
TIM1_
ETR
-
-
-
-
-
USART1_
RTS
-
CAN1_TX
OTG_FS_
DP
-
-
-
LCD_R5
EVEN
TOUT

---

Pinouts and pin description
Port A
PA13
JTMS-
SWDI
O
-
-
-
-
-
-
-
-
-
-
-
-
-
-
EVEN
TOUT
PA14
JTCK-
SWCL
K
-
-
-
-
-
-
-
-
-
-
-
-
-
-
EVEN
TOUT
PA15
JTDI
TIM2_
CH1/TIM2
_ETR
-
-
-
SPI1_
NSS
SPI3_
NSS/
I2S3_WS
-
-
-
-
-
-
-
-
EVEN
TOUT
Port B
PB0
-
TIM1_
CH2N
TIM3_
CH3
TIM8_
CH2N
-
-
-
-
-
LCD_R3
OTG_HS_
ULPI_D1
ETH_MII_
RXD2
-
-
-
EVEN
TOUT
PB1
-
TIM1_
CH3N
TIM3_
CH4
TIM8_
CH3N
-
-
-
-
-
LCD_R6
OTG_HS_
ULPI_D2
ETH_MII_
RXD3
-
-
-
EVEN
TOUT
PB2
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-
EVEN
TOUT
PB3
JTDO/
TRAC
ESWO
TIM2_
CH2
-
-
-
SPI1_
SCK
SPI3_
SCK/
I2S3_CK
-
-
-
-
-
-
-
-
EVEN
TOUT
PB4
NJTR
ST
-
TIM3_
CH1
-
-
SPI1_
MISO
SPI3_
MISO
I2S3ext_
SD
-
-
-
-
-
-
-
EVEN
TOUT
PB5
-
-
TIM3_
CH2
-
I2C1_
SMBA
SPI1_
MOSI
SPI3_
MOSI/
I2S3_SD
-
-
CAN2_RX
OTG_HS_
ULPI_D7
ETH_PPS
_OUT
FMC_
SDCKE1
DCMI_
D10
-
EVEN
TOUT
PB6
-
-
TIM4_
CH1
-
I2C1_
SCL
-
-
USART1_
TX
-
CAN2_TX
-
-
FMC_
SDNE1
DCMI_
D5
-
EVEN
TOUT
PB7
-
-
TIM4_
CH2
-
I2C1_
SDA
-
-
USART1_
RX
-
-
-
-
FMC_NL
DCMI_
VSYNC
-
EVEN
TOUT
PB8
-
-
TIM4_
CH3
TIM10_
CH1
I2C1_
SCL
-
-
-
-
CAN1_RX
-
ETH_MII_
TXD3
SDIO_D4
DCMI_
D6
LCD_B6
EVEN
TOUT
PB9
-
-
TIM4_
CH4
TIM11_
CH1
I2C1_
SDA
SPI2_
NSS/I2
S2_WS
-
-
-
CAN1_TX
-
-
SDIO_D5
DCMI_
D7
LCD_B7
EVEN
TOUT
PB10
-
TIM2_
CH3
-
-
I2C2_
SCL
SPI2_
SCK/I2
S2_CK
-
USART3_
TX
-
-
OTG_HS_
ULPI_D3
ETH_MII_
RX_ER
-
-
LCD_G4
EVEN
TOUT
Table 12. STM32F427xx and STM32F429xx alternate function mapping (continued)
Port
AF0
AF1
AF2
AF3
AF4
AF5
AF6
AF7
AF8
AF9
AF10
AF11
AF12
AF13
AF14
AF15
SYS
TIM1/2
TIM3/4/5
TIM8/9/
I2C1/
SPI1/2/
3/4/5/6
SPI2/3/
SAI1
SPI3/
USART1/
USART6/
UART4/5/7
/8
CAN1/2/
TIM12/13/14
/LCD
OTG2_HS
/OTG1_
FS
ETH
FMC/SDIO
/OTG2_FS
DCMI
LCD
SYS

---

Pinouts and pin description
Port B
PB11
-
TIM2_
CH4
-
-
I2C2_
SDA
-
-
USART3_
RX
-
-
OTG_HS_
ULPI_D4
ETH_MII_
TX_EN/
ETH_RMII
_TX_EN
-
-
LCD_G5
EVEN
TOUT
PB12
-
TIM1_
BKIN
-
-
I2C2_
SMBA
SPI2_
NSS/I2
S2_WS
-
USART3_
CK
-
CAN2_RX
OTG_HS_
ULPI_D5
ETH_MII_
TXD0/ETH
_RMII_
TXD0
OTG_HS_
ID
-
-
EVEN
TOUT
PB13
-
TIM1_
CH1N
-
-
-
SPI2_
SCK/I2
S2_CK
-
USART3_
CTS
-
CAN2_TX
OTG_HS_
ULPI_D6
ETH_MII_
TXD1/ETH
_RMII_TX
D1
-
-
-
EVEN
TOUT
PB14
-
TIM1_
CH2N
-
TIM8_
CH2N
-
SPI2_
MISO
I2S2ext_
SD
USART3_
RTS
-
TIM12_CH1
-
-
OTG_HS_
DM
-
-
EVEN
TOUT
PB15
RTC_
REFIN
TIM1_
CH3N
-
TIM8_
CH3N
-
SPI2_
MOSI/I2
S2_SD
-
-
-
TIM12_CH2
-
-
OTG_HS_
DP
-
-
EVEN
TOUT
Port
C
PC0
-
-
-
-
-
-
-
-
-
-
OTG_HS_
ULPI_STP
-
FMC_SDN
WE
-
-
EVEN
TOUT
PC1
-
-
-
-
-
-
-
-
-
-
-
ETH_MDC
-
-
-
EVEN
TOUT
PC2
-
-
-
-
-
SPI2_
MISO
I2S2ext_
SD
-
-
-
OTG_HS_
ULPI_DIR
ETH_MII_
TXD2
FMC_
SDNE0
-
-
EVEN
TOUT
PC3
-
-
-
-
-
SPI2_
MOSI/I2
S2_SD
-
-
-
-
OTG_HS_
ULPI_NXT
ETH_MII_
TX_CLK
FMC_
SDCKE0
-
-
EVEN
TOUT
PC4
-
-
-
-
-
-
-
-
-
-
-
ETH_MII_
RXD0/ETH
_RMII_
RXD0
-
-
-
EVEN
TOUT
PC5
-
-
-
-
-
-
-
-
-
-
-
ETH_MII_
RXD1/ETH
_RMII_
RXD1
-
-
-
EVEN
TOUT
PC6
-
-
TIM3_
CH1
TIM8_
CH1
-
I2S2_
MCK
-
-
USART6_
TX
-
-
-
SDIO_D6
DCMI_
D0
LCD_
HSYNC
EVEN
TOUT
PC7
-
-
TIM3_
CH2
TIM8_
CH2
-
-
I2S3_
MCK
-
USART6_
RX
-
-
-
SDIO_D7
DCMI_
D1
LCD_G6
EVEN
TOUT
Table 12. STM32F427xx and STM32F429xx alternate function mapping (continued)
Port
AF0
AF1
AF2
AF3
AF4
AF5
AF6
AF7
AF8
AF9
AF10
AF11
AF12
AF13
AF14
AF15
SYS
TIM1/2
TIM3/4/5
TIM8/9/
I2C1/
SPI1/2/
3/4/5/6
SPI2/3/
SAI1
SPI3/
USART1/
USART6/
UART4/5/7
/8
CAN1/2/
TIM12/13/14
/LCD
OTG2_HS
/OTG1_
FS
ETH
FMC/SDIO
/OTG2_FS
DCMI
LCD
SYS

---

Pinouts and pin description
Port
C
PC8
-
-
TIM3_
CH3
TIM8_
CH3
-
-
-
-
USART6_
CK
-
-
-
SDIO_D0
DCMI_
D2
-
EVEN
TOUT
PC9
MCO2
-
TIM3_
CH4
TIM8_
CH4
I2C3_
SDA
I2S_
CKIN
-
-
-
-
-
-
SDIO_D1
DCMI_
D3
-
EVEN
TOUT
PC10
-
-
-
-
-
-
SPI3_
SCK/I2S
3_CK
USART3_
TX
UART4_TX
-
-
-
SDIO_D2
DCMI_
D8
LCD_R2
EVEN
TOUT
PC11
-
-
-
-
-
I2S3ext
_SD
SPI3_
MISO
USART3_
RX
UART4_RX
-
-
-
SDIO_D3
DCMI_
D4
-
EVEN
TOUT
PC12
-
-
-
-
-
-
SPI3_
MOSI/I2
S3_SD
USART3_
CK
UART5_TX
-
-
-
SDIO_CK
DCMI_
D9
-
EVEN
TOUT
PC13
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-
EVEN
TOUT
PC14
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-
EVEN
TOUT
PC15
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-
EVEN
TOUT
Port
D
PD0
-
-
-
-
-
-
-
-
-
CAN1_RX
-
-
FMC_D2
-
-
EVEN
TOUT
PD1
-
-
-
-
-
-
-
-
-
CAN1_TX
-
-
FMC_D3
-
-
EVEN
TOUT
PD2
-
-
TIM3_
ETR
-
-
-
-
-
UART5_RX
-
-
-
SDIO_
CMD
DCMI_
D11
-
EVEN
TOUT
PD3
-
-
-
-
-
SPI2_S
CK/I
2S2_CK
-
USART2_
CTS
-
-
-
-
FMC_CLK
DCMI_
D5
LCD_G7
EVEN
TOUT
PD4
-
-
-
-
-
-
-
USART2_
RTS
-
-
-
-
FMC_NOE
-
-
EVEN
TOUT
PD5
-
-
-
-
-
-
-
USART2_
TX
-
-
-
-
FMC_NWE
-
-
EVEN
TOUT
PD6
-
-
-
-
-
SPI3_
MOSI/I2
S3_SD
SAI1_
SD_A
USART2_
RX
-
-
-
-
FMC_
NWAIT
DCMI_
D10
LCD_B2
EVEN
TOUT
Table 12. STM32F427xx and STM32F429xx alternate function mapping (continued)
Port
AF0
AF1
AF2
AF3
AF4
AF5
AF6
AF7
AF8
AF9
AF10
AF11
AF12
AF13
AF14
AF15
SYS
TIM1/2
TIM3/4/5
TIM8/9/
I2C1/
SPI1/2/
3/4/5/6
SPI2/3/
SAI1
SPI3/
USART1/
USART6/
UART4/5/7
/8
CAN1/2/
TIM12/13/14
/LCD
OTG2_HS
/OTG1_
FS
ETH
FMC/SDIO
/OTG2_FS
DCMI
LCD
SYS

---

Pinouts and pin description
Port
D
PD7
-
-
-
-
-
-
-
USART2_
CK
-
-
-
-
FMC_NE1/
FMC_
NCE2
-
-
EVEN
TOUT
PD8
-
-
-
-
-
-
-
USART3_
TX
-
-
-
-
FMC_D13
-
-
EVEN
TOUT
PD9
-
-
-
-
-
-
-
USART3_
RX
-
-
-
-
FMC_D14
-
-
EVEN
TOUT
PD10
-
-
-
-
-
-
-
USART3_
CK
-
-
-
-
FMC_D15
-
LCD_B3
EVEN
TOUT
PD11
-
-
-
-
-
-
-
USART3_
CTS
-
-
-
-
FMC_A16
-
-
EVEN
TOUT
PD12
-
-
TIM4_
CH1
-
-
-
-
USART3_
RTS
-
-
-
-
FMC_A17
-
-
EVEN
TOUT
PD13
-
-
TIM4_
CH2
-
-
-
-
-
-
-
-
-
FMC_A18
-
-
EVEN
TOUT
PD14
-
-
TIM4_
CH3
-
-
-
-
-
-
-
-
-
FMC_D0
-
-
EVEN
TOUT
PD15
-
-
TIM4_
CH4
-
-
-
-
-
-
-
-
-
FMC_D1
-
-
EVEN
TOUT
Port E
PE0
-
-
TIM4_
ETR
-
-
-
-
-
UART8_Rx
-
-
-
FMC_
NBL0
DCMI_
D2
-
EVEN
TOUT
PE1
-
-
-
-
-
-
-
-
UART8_Tx
-
-
-
FMC_
NBL1
DCMI_
D3
-
EVEN
TOUT
PE2
TRAC
ECLK
-
-
-
-
SPI4_
SCK
SAI1_
MCLK_A
-
-
-
-
ETH_MII_
TXD3
FMC_A23
-
-
EVEN
TOUT
PE3
TRAC
ED0
-
-
-
-
-
SAI1_
SD_B
-
-
-
-
-
FMC_A19
-
-
EVEN
TOUT
PE4
TRAC
ED1
-
-
-
-
SPI4_
NSS
SAI1_
FS_A
-
-
-
-
-
FMC_A20
DCMI_
D4
LCD_B0
EVEN
TOUT
PE5
TRAC
ED2
-
-
TIM9_
CH1
-
SPI4_M
ISO
SAI1_
SCK_A
-
-
-
-
-
FMC_A21
DCMI_
D6
LCD_G0
EVEN
TOUT
PE6
TRAC
ED3
-
-
TIM9_
CH2
-
SPI4_
MOSI
SAI1_
SD_A
-
-
-
-
-
FMC_A22
DCMI_
D7
LCD_G1
EVEN
TOUT
Table 12. STM32F427xx and STM32F429xx alternate function mapping (continued)
Port
AF0
AF1
AF2
AF3
AF4
AF5
AF6
AF7
AF8
AF9
AF10
AF11
AF12
AF13
AF14
AF15
SYS
TIM1/2
TIM3/4/5
TIM8/9/
I2C1/
SPI1/2/
3/4/5/6
SPI2/3/
SAI1
SPI3/
USART1/
USART6/
UART4/5/7
/8
CAN1/2/
TIM12/13/14
/LCD
OTG2_HS
/OTG1_
FS
ETH
FMC/SDIO
/OTG2_FS
DCMI
LCD
SYS

---

Pinouts and pin description
Port E
PE7
-
TIM1_
ETR
-
-
-
-
-
-
UART7_Rx
-
-
-
FMC_D4
-
-
EVEN
TOUT
PE8
-
TIM1_
CH1N
-
-
-
-
-
-
UART7_Tx
-
-
-
FMC_D5
-
-
EVEN
TOUT
PE9
-
TIM1_
CH1
-
-
-
-
-
-
-
-
-
-
FMC_D6
-
-
EVEN
TOUT
PE10
-
TIM1_
CH2N
-
-
-
-
-
-
-
-
-
-
FMC_D7
-
-
EVEN
TOUT
PE11
-
TIM1_
CH2
-
-
-
SPI4_
NSS
-
-
-
-
-
-
FMC_D8
-
LCD_G3
EVEN
TOUT
PE12
-
TIM1_
CH3N
-
-
-
SPI4_
SCK
-
-
-
-
-
-
FMC_D9
-
LCD_B4
EVEN
TOUT
PE13
-
TIM1_
CH3
-
-
-
SPI4_
MISO
-
-
-
-
-
-
FMC_D10
-
LCD_DE
EVEN
TOUT
PE14
-
TIM1_
CH4
-
-
-
SPI4_
MOSI
-
-
-
-
-
-
FMC_D11
-
LCD_
CLK
EVEN
TOUT
PE15
-
TIM1_
BKIN
-
-
-
-
-
-
-
-
-
FMC_D12
-
LCD_R7
EVEN
TOUT
Port F
PF0
-
-
-
-
I2C2_
SDA
-
-
-
-
-
-
-
FMC_A0
-
-
EVEN
TOUT
PF1
-
I2C2_
SCL
-
-
-
-
-
-
-
FMC_A1
-
-
EVEN
TOUT
PF2
-
-
-
-
I2C2_
SMBA
-
-
-
-
-
-
-
FMC_A2
-
-
EVEN
TOUT
PF3
-
-
-
-
-
-
-
-
-
-
-
FMC_A3
-
-
EVEN
TOUT
PF4
-
-
-
-
-
-
-
-
-
-
-
FMC_A4
-
-
EVEN
TOUT
PF5
-
-
-
-
-
-
-
-
-
-
-
FMC_A5
-
-
EVEN
TOUT
PF6
-
-
-
TIM10_
CH1
-
SPI5_
NSS
SAI1_
SD_B
-
UART7_Rx
-
-
-
FMC_
NIORD
-
-
EVEN
TOUT
PF7
-
-
-
TIM11_
CH1
-
SPI5_
SCK
SAI1_
MCLK_B
-
UART7_Tx
-
-
-
FMC_
NREG
-
-
EVEN
TOUT
Table 12. STM32F427xx and STM32F429xx alternate function mapping (continued)
Port
AF0
AF1
AF2
AF3
AF4
AF5
AF6
AF7
AF8
AF9
AF10
AF11
AF12
AF13
AF14
AF15
SYS
TIM1/2
TIM3/4/5
TIM8/9/
I2C1/
SPI1/2/
3/4/5/6
SPI2/3/
SAI1
SPI3/
USART1/
USART6/
UART4/5/7
/8
CAN1/2/
TIM12/13/14
/LCD
OTG2_HS
/OTG1_
FS
ETH
FMC/SDIO
/OTG2_FS
DCMI
LCD
SYS

---

Pinouts and pin description
Port F
PF8
-
-
-
-
-
SPI5_
MISO
SAI1_
SCK_B
-
-
TIM13_CH1
-
-
FMC_
NIOWR
-
-
EVEN
TOUT
PF9
-
-
-
-
-
SPI5_
MOSI
SAI1_
FS_B
-
-
TIM14_CH1
-
-
FMC_CD
-
-
EVEN
TOUT
PF10
-
-
-
-
-
-
-
-
-
-
-
-
FMC_INTR
DCMI_
D11
LCD_DE
EVEN
TOUT
PF11
-
-
-
-
-
SPI5_
MOSI
-
-
-
-
-
-
FMC_
SDNRAS
DCMI_
D12
-
EVEN
TOUT
PF12
-
-
-
-
-
-
-
-
-
-
-
-
FMC_A6
-
-
EVEN
TOUT
PF13
-
-
-
-
-
-
-
-
-
-
-
-
FMC_A7
-
-
EVEN
TOUT
PF14
-
-
-
-
-
-
-
-
-
-
-
-
FMC_A8
-
-
EVEN
TOUT
PF15
-
-
-
-
-
-
-
-
-
-
-
-
FMC_A9
-
-
EVEN
TOUT
Port
G
PG0
-
-
-
-
-
-
-
-
-
-
-
-
FMC_A10
-
-
EVEN
TOUT
PG1
-
-
-
-
-
-
-
-
-
-
-
-
FMC_A11
-
-
EVEN
TOUT
PG2
-
-
-
-
-
-
-
-
-
-
-
-
FMC_A12
-
-
EVEN
TOUT
PG3
-
-
-
-
-
-
-
-
-
-
-
-
FMC_A13
-
-
EVEN
TOUT
PG4
-
-
-
-
-
-
-
-
-
-
-
-
FMC_A14/
FMC_BA0
-
-
EVEN
TOUT
PG5
-
-
-
-
-
-
-
-
-
-
-
-
FMC_A15/
FMC_BA1
-
-
EVEN
TOUT
PG6
-
-
-
-
-
-
-
-
-
-
-
-
FMC_INT2
DCMI_
D12
LCD_R7
EVEN
TOUT
PG7
-
-
-
-
-
-
-
-
USART6_
CK
-
-
-
FMC_INT3
DCMI_
D13
LCD_
CLK
EVEN
TOUT
PG8
-
-
-
-
-
SPI6_
NSS
-
-
USART6_
RTS
-
-
ETH_PPS
_OUT
FMC_SDC
LK
-
-
EVEN
TOUT
Table 12. STM32F427xx and STM32F429xx alternate function mapping (continued)
Port
AF0
AF1
AF2
AF3
AF4
AF5
AF6
AF7
AF8
AF9
AF10
AF11
AF12
AF13
AF14
AF15
SYS
TIM1/2
TIM3/4/5
TIM8/9/
I2C1/
SPI1/2/
3/4/5/6
SPI2/3/
SAI1
SPI3/
USART1/
USART6/
UART4/5/7
/8
CAN1/2/
TIM12/13/14
/LCD
OTG2_HS
/OTG1_
FS
ETH
FMC/SDIO
/OTG2_FS
DCMI
LCD
SYS

---

Pinouts and pin description
Port
G
PG9
-
-
-
-
-
-
-
-
USART6_
RX
-
-
-
FMC_NE2/
FMC_
NCE3
DCMI_
VSYNC
(1)
-
EVEN
TOUT
PG10
-
-
-
-
-
-
-
-
-
LCD_G3
-
-
FMC_
NCE4_1/
FMC_NE3
DCMI_
D2
LCD_B2
EVEN
TOUT
PG11
-
-
-
-
-
-
-
-
-
-
-
ETH_MII_
TX_EN/
ETH_RMII
_TX_EN
FMC_
NCE4_2
DCMI_
D3
LCD_B3
EVEN
TOUT
PG12
-
-
-
-
-
SPI6_
MISO
-
-
USART6_
RTS
LCD_B4
-
-
FMC_NE4
-
LCD_B1
EVEN
TOUT
PG13
-
-
-
-
-
SPI6_
SCK
-
-
USART6_
CTS
-
-
ETH_MII_
TXD0/
ETH_RMII
_TXD0
FMC_A24
-
-
EVEN
TOUT
PG14
-
-
-
-
-
SPI6_
MOSI
-
-
USART6_
TX
-
-
ETH_MII_
TXD1/
ETH_RMII
_TXD1
FMC_A25
-
-
EVEN
TOUT
PG15
-
-
-
-
-
-
-
-
USART6_
CTS
-
-
-
FMC_
SDNCAS
DCMI_
D13
-
EVEN
TOUT
Port
H
PH0
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-
EVEN
TOUT
PH1
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-
EVEN
TOUT
PH2
-
-
-
-
-
-
-
-
-
-
-
ETH_MII_
CRS
FMC_
SDCKE0
-
LCD_R0
EVEN
TOUT
PH3
-
-
-
-
-
-
-
-
-
-
-
ETH_MII_
COL
FMC_SDN
E0
-
LCD_R1
EVEN
TOUT
PH4
-
-
-
-
I2C2_
SCL
-
-
-
-
-
OTG_HS_
ULPI_NXT
-
-
-
-
EVEN
TOUT
PH5
-
-
-
-
I2C2_
SDA
SPI5_N
SS
-
-
-
-
-
-
FMC_SDN
WE
-
-
EVEN
TOUT
PH6
-
-
-
-
I2C2_
SMBA
SPI5_
SCK
-
-
-
TIM12_CH1
-
ETH_MII_
RXD2
FMC_
SDNE1
DCMI_
D8
-
-
Table 12. STM32F427xx and STM32F429xx alternate function mapping (continued)
Port
AF0
AF1
AF2
AF3
AF4
AF5
AF6
AF7
AF8
AF9
AF10
AF11
AF12
AF13
AF14
AF15
SYS
TIM1/2
TIM3/4/5
TIM8/9/
I2C1/
SPI1/2/
3/4/5/6
SPI2/3/
SAI1
SPI3/
USART1/
USART6/
UART4/5/7
/8
CAN1/2/
TIM12/13/14
/LCD
OTG2_HS
/OTG1_
FS
ETH
FMC/SDIO
/OTG2_FS
DCMI
LCD
SYS

---

Pinouts and pin description
Port
H
PH7
-
-
-
-
I2C3_
SCL
SPI5_
MISO
-
-
-
-
-
ETH_MII_
RXD3
FMC_
SDCKE1
DCMI_
D9
-
-
PH8
-
-
-
-
I2C3_
SDA
-
-
-
-
-
-
-
FMC_D16
DCMI_
HSYNC
LCD_R2
EVEN
TOUT
PH9
-
-
-
-
I2C3_
SMBA
-
-
-
-
TIM12_CH2
-
-
FMC_D17
DCMI_
D0
LCD_R3
EVEN
TOUT
PH10
-
-
TIM5_
CH1
-
-
-
-
-
-
-
-
-
FMC_D18
DCMI_
D1
LCD_R4
EVEN
TOUT
PH11
-
-
TIM5_
CH2
-
-
-
-
-
-
-
-
-
FMC_D19
DCMI_
D2
LCD_R5
EVEN
TOUT
PH12
-
-
TIM5_
CH3
-
-
-
-
-
-
-
-
-
FMC_D20
DCMI_
D3
LCD_R6
EVEN
TOUT
PH13
-
-
-
TIM8_
CH1N
-
-
-
-
-
CAN1_TX
-
-
FMC_D21
-
LCD_G2
EVEN
TOUT
PH14
-
-
-
TIM8_
CH2N
-
-
-
-
-
-
-
-
FMC_D22
DCMI_
D4
LCD_G3
EVEN
TOUT
PH15
-
-
-
TIM8_
CH3N
-
-
-
-
-
-
-
-
FMC_D23
DCMI_
D11
LCD_G4
EVEN
TOUT
Port I
PI0
-
-
TIM5_
CH4
-
-
SPI2_
NSS/I2
S2_WS
-
-
-
-
-
-
FMC_D24
DCMI_
D13
LCD_G5
EVEN
TOUT
PI1
-
-
-
-
-
SPI2_
SCK/I2
S2_CK
-
-
-
-
-
-
FMC_D25
DCMI_
D8
LCD_G6
EVEN
TOUT
PI2
-
-
-
TIM8_
CH4
-
SPI2_
MISO
I2S2ext_
SD
-
-
-
-
-
FMC_D26
DCMI_
D9
LCD_G7
EVEN
TOUT
PI3
-
-
-
TIM8_
ETR
-
SPI2_M
OSI/I2S
2_SD
FMC_D27
DCMI_D
EVEN
TOUT
PI4
-
-
-
TIM8_
BKIN
-
-
-
-
-
-
-
-
FMC_
NBL2
DCMI_D
LCD_B4
EVEN
TOUT
PI5
-
-
-
TIM8_
CH1
-
-
-
-
-
-
-
-
FMC_
NBL3
DCMI_
VSYNC
LCD_B5
EVEN
TOUT
PI6
-
-
-
TIM8_
CH2
-
-
-
-
-
-
-
-
FMC_D28
DCMI_
D6
LCD_B6
EVEN
TOUT
Table 12. STM32F427xx and STM32F429xx alternate function mapping (continued)
Port
AF0
AF1
AF2
AF3
AF4
AF5
AF6
AF7
AF8
AF9
AF10
AF11
AF12
AF13
AF14
AF15
SYS
TIM1/2
TIM3/4/5
TIM8/9/
I2C1/
SPI1/2/
3/4/5/6
SPI2/3/
SAI1
SPI3/
USART1/
USART6/
UART4/5/7
/8
CAN1/2/
TIM12/13/14
/LCD
OTG2_HS
/OTG1_
FS
ETH
FMC/SDIO
/OTG2_FS
DCMI
LCD
SYS

---

Pinouts and pin description
Port I
PI7
-
-
-
TIM8_
CH3
-
-
-
-
-
-
-
-
FMC_D29
DCMI_
D7
LCD_B7
EVEN
TOUT
PI8
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-
EVEN
TOUT
PI9
-
-
-
-
-
-
-
-
-
CAN1_RX
-
-
FMC_D30
-
LCD_
VSYNC
EVEN
TOUT
PI10
-
-
-
-
-
-
-
-
-
-
-
ETH_MII_
RX_ER
FMC_D31
-
LCD_
HSYNC
EVEN
TOUT
PI11
-
-
-
-
-
-
-
-
-
-
OTG_HS_
ULPI_DIR
-
-
-
-
EVEN
TOUT
PI12
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_
HSYNC
EVEN
TOUT
PI13
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_
VSYNC
EVEN
TOUT
PI14
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_
CLK
EVEN
TOUT
PI15
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_R0
EVEN
TOUT
Port J
PJ0
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_R1
EVEN
TOUT
PJ1
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_R2
EVEN
TOUT
PJ2
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_R3
EVEN
TOUT
PJ3
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_R4
EVEN
TOUT
PJ4
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_R5
EVEN
TOUT
PJ5
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_R6
EVEN
TOUT
PJ6
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_R7
EVEN
TOUT
PJ7
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_G0
EVEN
TOUT
Table 12. STM32F427xx and STM32F429xx alternate function mapping (continued)
Port
AF0
AF1
AF2
AF3
AF4
AF5
AF6
AF7
AF8
AF9
AF10
AF11
AF12
AF13
AF14
AF15
SYS
TIM1/2
TIM3/4/5
TIM8/9/
I2C1/
SPI1/2/
3/4/5/6
SPI2/3/
SAI1
SPI3/
USART1/
USART6/
UART4/5/7
/8
CAN1/2/
TIM12/13/14
/LCD
OTG2_HS
/OTG1_
FS
ETH
FMC/SDIO
/OTG2_FS
DCMI
LCD
SYS

---

Pinouts and pin description
Port J
PJ8
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_G1
EVEN
TOUT
PJ9
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_G2
EVEN
TOUT
PJ10
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_G3
EVEN
TOUT
PJ11
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_G4
EVEN
TOUT
PJ12
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_B0
EVEN
TOUT
PJ13
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_B1
EVEN
TOUT
PJ14
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_B2
EVEN
TOUT
PJ15
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_B3
EVEN
TOUT
Port K
PK0
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_G5
EVEN
TOUT
PK1
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_G6
EVEN
TOUT
PK2
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_G7
EVEN
TOUT
PK3
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_B4
EVEN
TOUT
PK4
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_B5
EVEN
TOUT
PK5
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_B6
EVEN
TOUT
PK6
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_B7
EVEN
TOUT
PK7
-
-
-
-
-
-
-
-
-
-
-
-
-
-
LCD_DE
EVEN
TOUT
1.
The DCMI_VSYNC alternate function on PG9 is only available on silicon revision 3.
Table 12. STM32F427xx and STM32F429xx alternate function mapping (continued)
Port
AF0
AF1
AF2
AF3
AF4
AF5
AF6
AF7
AF8
AF9
AF10
AF11
AF12
AF13
AF14
AF15
SYS
TIM1/2
TIM3/4/5
TIM8/9/
I2C1/
SPI1/2/
3/4/5/6
SPI2/3/
SAI1
SPI3/
USART1/
USART6/
UART4/5/7
/8
CAN1/2/
TIM12/13/14
/LCD
OTG2_HS
/OTG1_
FS
ETH
FMC/SDIO
/OTG2_FS
DCMI
LCD
SYS

---

Memory mapping
Memory mapping
The memory map is shown in Figure 19.
Figure 19. Memory map
MS30424V5
0x5006 0C00 - 0x5FFF FFFF
AHB3
0x6000 0000 - 0xDFFF FFFF
Cortex-M4 internal
peripherals
0xE000 0000 - 0xE00F FFFF
Reserved
0xE010 0000 - 0xFFFF FFFF
512-Mbyte
Block 7
Cortex-M4
Internal
peripherals
512-Mbyte
Block 6
FMC
512-Mbyte
Block 4
FMC bank 3 to
bank 4
0x0000 0000
0x1FFF FFFF
0x2000 0000
0x3FFF FFFF
0x4000 0000
0x5FFF FFFF
0x6000 0000
0x7FFF FFFF
0x8000 0000
0x9FFF FFFF
0xA000 0000
0xCFFF FFFF
0xD000 0000
0xDFFF FFFF
0xE000 0000
0xFFFF FFFF
SRAM (16 KB aliased
By bit-banding
Reserved
0x2000 0000 - 0x2001 BFFF
0x2001 C000 - 0x2001 FFFF
0x2003 0000 - 0x3FFF FFFF
0x4000 0000
Reserved
0x4000 7FFF
0x4000 8000 - 0x4000 FFFF
0x4001 0000
Reserved
0x5006 0C00 - 0x5FFF FFFF
AHB3
0x6000 0000 - 0xDFFF FFFF
AHB2
SRAM (112 KB aliased
By bit-banding
0x5006 0BFF
0x5000 0000
SRAM (64 KB aliased
By bit-banding
0x2002 0000 - 0x2002 FFFF
APB1
APB2
0x4001 6BFF
0x4001 6C00 - 0x4001 FFFF
Reserved
0x4008 0000 - 0x4FFF FFFF
0x4007 FFFF
AHB1
Reserved
Flash memory
0x0820 0000 - 0x0FFF FFFF
0x1FFF 0000 - 0x1FFF 7A0F
0x1FFF C000 - 0x1FFF C00F
0x0800 0000 - 0x081F FFFF
0x0020 0000 - 0x07FF FFFF
0x0000 0000 - 0x001F FFFF
System memory
Reserved
Reserved
Aliased to Flash, system
memory or SRAM depending
on the BOOT pins
Option Bytes
Reserved
0x1FFF C010 - 0x1FFF FFFF
0x1FFF 7A10 - 0x1FFF 7FFF
Reserved
CCM data RAM
 (64 KB data SRAM)
0x1000 0000 - 0x1000 FFFF
Reserved
0x1001 0000 - 0x1FFE BFFF
0x1FFE C000 - 0x1FFF C00F
Option bytes
Reserved
0x1FFE C010 - 0x1FFE FFFF
0x4002 0000
Cortex-M4 internal
peripherals
0xE000 0000 - 0xE00F FFFF
Reserved
0xE010 0000 - 0xFFFF FFFF
512-Mbyte
Block 5
FMC
512-Mbyte
Block 3
FMC bank 1 to
bank 2
512-Mbyte
Block 2
Peripherals
512-Mbyte
Block 1
SRAM
512-Mbyte
Block 0
SRAM

---

Memory mapping
Table 13. STM32F427xx and STM32F429xx register boundary addresses
Bus
Boundary address
Peripheral
0xE00F FFFF - 0xFFFF FFFF
Reserved
Cortex-M4
0xE000 0000 - 0xE00F FFFF
Cortex-M4 internal peripherals
AHB3
0xD000 0000 - 0xDFFF FFFF
FMC bank 6
0xC000 0000 - 0xCFFF FFFF
FMC bank 5
0xA000 1000 - 0xBFFF FFFF
Reserved
0xA000 0000- 0xA000 0FFF
FMC control register
0x9000 0000 - 0x9FFF FFFF
FMC bank 4
0x8000 0000 - 0x8FFF FFFF
FMC bank 3
0x7000 0000 - 0x7FFF FFFF
FMC bank 2
0x6000 0000 - 0x6FFF FFFF
FMC bank 1
0x5006 0C00- 0x5FFF FFFF
Reserved
AHB2
0x5006 0800 - 0X5006 0BFF
RNG
0x5005 0400 - X5006 07FF
Reserved
0x5005 0000 - 0X5005 03FF
DCMI
0x5004 0000- 0x5004 FFFF
Reserved
0x5000 0000 - 0X5003 FFFF
USB OTG FS

---

Memory mapping
0x4008 0000- 0x4FFF FFFF
Reserved
AHB1
0x4004 0000 - 0x4007 FFFF
USB OTG HS
0x4002 BC00- 0x4003 FFFF
Reserved
0x4002 B000 - 0x4002 BBFF
DMA2D
0x4002 9400 - 0x4002 AFFF
Reserved
0x4002 9000 - 0x4002 93FF
ETHERNET MAC
0x4002 8C00 - 0x4002 8FFF
0x4002 8800 - 0x4002 8BFF
0x4002 8400 - 0x4002 87FF
0x4002 8000 - 0x4002 83FF
0x4002 6800 - 0x4002 7FFF
Reserved
0x4002 6400 - 0x4002 67FF
DMA2
0x4002 6000 - 0x4002 63FF
DMA1
0X4002 5000 - 0X4002 5FFF
Reserved
0x4002 4000 - 0x4002 4FFF
BKPSRAM
0x4002 3C00 - 0x4002 3FFF
Flash interface register
0x4002 3800 - 0x4002 3BFF
RCC
0X4002 3400 - 0X4002 37FF
Reserved
0x4002 3000 - 0x4002 33FF
CRC
0x4002 2C00 - 0x4002 2FFF
Reserved
0x4002 2800 - 0x4002 2BFF
GPIOK
0x4002 2400 - 0x4002 27FF
GPIOJ
0x4002 2000 - 0x4002 23FF
GPIOI
0x4002 1C00 - 0x4002 1FFF
GPIOH
0x4002 1800 - 0x4002 1BFF
GPIOG
0x4002 1400 - 0x4002 17FF
GPIOF
0x4002 1000 - 0x4002 13FF
GPIOE
0X4002 0C00 - 0x4002 0FFF
GPIOD
0x4002 0800 - 0x4002 0BFF
GPIOC
0x4002 0400 - 0x4002 07FF
GPIOB
0x4002 0000 - 0x4002 03FF
GPIOA
Table 13. STM32F427xx and STM32F429xx register boundary addresses (continued)
Bus
Boundary address
Peripheral

---

Memory mapping
0x4001 6C00- 0x4001 FFFF
Reserved
APB2
0x4001 6800 - 0x4001 6BFF
LCD-TFT
0x4001 5C00 - 0x4001 67FF
Reserved
0x4001 5800 - 0x4001 5BFF
SAI1
0x4001 5400 - 0x4001 57FF
SPI6
0x4001 5000 - 0x4001 53FF
SPI5
0x4001 5400 - 0x4001 57FF
SPI6
0x4001 5000 - 0x4001 53FF
SPI5
0x4001 4C00 - 0x4001 4FFF
Reserved
0x4001 4800 - 0x4001 4BFF
TIM11
0x4001 4400 - 0x4001 47FF
TIM10
0x4001 4000 - 0x4001 43FF
TIM9
0x4001 3C00 - 0x4001 3FFF
EXTI
0x4001 3800 - 0x4001 3BFF
SYSCFG
0x4001 3400 - 0x4001 37FF
SPI4
0x4001 3000 - 0x4001 33FF
SPI1
0x4001 2C00 - 0x4001 2FFF
SDIO
0x4001 2400 - 0x4001 2BFF
Reserved
0x4001 2000 - 0x4001 23FF
ADC1 - ADC2 - ADC3
0x4001 1800 - 0x4001 1FFF
Reserved
0x4001 1400 - 0x4001 17FF
USART6
0x4001 1000 - 0x4001 13FF
USART1
0x4001 0800 - 0x4001 0FFF
Reserved
0x4001 0400 - 0x4001 07FF
TIM8
0x4001 0000 - 0x4001 03FF
TIM1
Table 13. STM32F427xx and STM32F429xx register boundary addresses (continued)
Bus
Boundary address
Peripheral

---

Memory mapping
0x4000 8000- 0x4000 FFFF
Reserved
APB1
0x4000 7C00 - 0x4000 7FFF
UART8
0x4000 7800 - 0x4000 7BFF
UART7
0x4000 7400 - 0x4000 77FF
DAC
0x4000 7000 - 0x4000 73FF
PWR
0x4000 6C00 - 0x4000 6FFF
Reserved
0x4000 6800 - 0x4000 6BFF
CAN2
0x4000 6400 - 0x4000 67FF
CAN1
0x4000 6000 - 0x4000 63FF
Reserved
0x4000 5C00 - 0x4000 5FFF
I2C3
0x4000 5800 - 0x4000 5BFF
I2C2
0x4000 5400 - 0x4000 57FF
I2C1
0x4000 5000 - 0x4000 53FF
UART5
0x4000 4C00 - 0x4000 4FFF
UART4
0x4000 4800 - 0x4000 4BFF
USART3
0x4000 4400 - 0x4000 47FF
USART2
0x4000 4000 - 0x4000 43FF
I2S3ext
0x4000 3C00 - 0x4000 3FFF
SPI3 / I2S3
0x4000 3800 - 0x4000 3BFF
SPI2 / I2S2
0x4000 3400 - 0x4000 37FF
I2S2ext
0x4000 3000 - 0x4000 33FF
IWDG
0x4000 2C00 - 0x4000 2FFF
WWDG
0x4000 2800 - 0x4000 2BFF
RTC & BKP Registers
0x4000 2400 - 0x4000 27FF
Reserved
0x4000 2000 - 0x4000 23FF
TIM14
0x4000 1C00 - 0x4000 1FFF
TIM13
0x4000 1800 - 0x4000 1BFF
TIM12
0x4000 1400 - 0x4000 17FF
TIM7
0x4000 1000 - 0x4000 13FF
TIM6
0x4000 0C00 - 0x4000 0FFF
TIM5
0x4000 0800 - 0x4000 0BFF
TIM4
0x4000 0400 - 0x4000 07FF
TIM3
0x4000 0000 - 0x4000 03FF
TIM2
Table 13. STM32F427xx and STM32F429xx register boundary addresses (continued)
Bus
Boundary address
Peripheral

---

Electrical characteristics
Electrical characteristics
6.1
Parameter conditions
Unless otherwise specified, all voltages are referenced to VSS.
6.1.1
Minimum and maximum values
Unless otherwise specified the minimum and maximum values are guaranteed in the worst
conditions of ambient temperature, supply voltage and frequencies by tests in production on
100% of the devices with an ambient temperature at TA = 25 °C and TA = TAmax (given by
the selected temperature range).
Data based on characterization results, design simulation and/or technology characteristics
are indicated in the table footnotes and are not tested in production. Based on
characterization, the minimum and maximum values refer to sample tests and represent the
mean value plus or minus three times the standard deviation (mean±3σ).
6.1.2
Typical values
Unless otherwise specified, typical data are based on TA = 25 °C, VDD = 3.3 V (for the
1.7 V ≤VDD ≤ 3.6 V voltage range). They are given only as design guidelines and are not
tested.
Typical ADC accuracy values are determined by characterization of a batch of samples from
a standard diffusion lot over the full temperature range, where 95% of the devices have an
error less than or equal to the value indicated (mean±2σ).
6.1.3
Typical curves
Unless otherwise specified, all typical curves are given only as design guidelines and are
not tested.
6.1.4
Loading capacitor
The loading conditions used for pin parameter measurement are shown in Figure 20.
6.1.5
Pin input voltage
The input voltage measurement on a pin of the device is described in Figure 21.
Figure 20. Pin loading conditions
Figure 21. Pin input voltage
MS19011V2
C = 50 pF
MCU pin
MS19010V2
MCU pin
VIN

---

Electrical characteristics
6.1.6
Power supply scheme
Figure 22. Power supply scheme
1.
To connect BYPASS_REG and PDR_ON pins, refer to Section 3.17: Power supply supervisor and Section 3.18: Voltage
regulator
2.
The two 2.2 µF ceramic capacitors should be replaced by two 100 nF decoupling capacitors when the voltage regulator is
OFF.
3.
The 4.7 µF ceramic capacitor must be connected to one of the VDD pins.
4.
VDDA=VDD and VSSA=VSS.
Caution:
Each power supply pair (VDD/VSS, VDDA/VSSA ...) must be decoupled with filtering ceramic
capacitors as shown above. These capacitors must be placed as close as possible to, or
below, the appropriate pins on the underside of the PCB to ensure good operation of the
device. It is not recommended to remove filtering capacitors to reduce PCB size or cost.
This might cause incorrect operation of the device.
MS19911V3
Backup circuitry
(OSC32K,RTC,
Wakeup logic
Backup registers,
backup RAM)
Kernel logic
(CPU, digital
& RAM)
Analog:
RCs,
PLL,..
Power
switch
VBAT
GPIOs
OUT
IN
15 × 100 nF
+ 1 × 4.7 μF
VBAT =
1.65 to 3.6V
Voltage
regulator
VDDA
ADC
Level shifter
IO
Logic
VDD
100 nF
+ 1 μF
Flash memory
VCAP_1
VCAP_2
2 × 2.2 μF
BYPASS_REG
PDR_ON
Reset
controller
VDD
1/2/...14/15
VSS
1/2/...14/15
VDD
VREF+
VREF-
VSSA
VREF
100 nF
+ 1 μF

---

Electrical characteristics
6.1.7
Current consumption measurement
Figure 23. Current consumption measurement scheme
6.2
Absolute maximum ratings
Stresses above the absolute maximum ratings listed in Table 14: Voltage characteristics,
Table 15: Current characteristics, and Table 16: Thermal characteristics may cause
permanent damage to the device. These are stress ratings only and functional operation of
the device at these conditions is not implied. Exposure to maximum rating conditions for
extended periods may affect device reliability.
Device mission profile (application conditions) is compliant with JEDEC JESD47
Qualification Standard, extended mission profiles are available on demand.
ai14126
VBAT
VDD
VDDA
IDD_VBAT
IDD
Table 14. Voltage characteristics
Symbol
Ratings
Min
Max
Unit
VDD–VSS
External main supply voltage (including VDDA, VDD
and VBAT)(1)
1.
All main power (VDD, VDDA) and ground (VSS, VSSA) pins must always be connected to the external
power supply, in the permitted range.
−0.3
4.0
V
VIN
Input voltage on FT pins(2)
2.
VIN maximum value must always be respected. Refer to Table 15 for the values of the maximum allowed
injected current.
VSS −0.3
VDD + 4.0
Input voltage on TTa pins
VSS −0.3
4.0
Input voltage on any other pin
VSS −0.3
4.0
Input voltage on BOOT0 pin
VSS
9.0
|∆VDDx|
Variations between different VDD power pins
-
mV
|VSSX -VSS|
Variations between all the different ground pins
including VREF-
-
VESD(HBM)
Electrostatic discharge voltage (human body model)
see Section 6.3.15:
Absolute maximum
ratings (electrical
sensitivity)

---

Electrical characteristics
Table 15. Current characteristics
Symbol
Ratings
 Max.
Unit
∑IVDD
Total current into sum of all VDD_x power lines (source)(1)
mA
∑IVSS
Total current out of sum of all VSS_x ground lines (sink)(1)
−270
IVDD
Maximum current into each VDD_x power line (source)(1)
IVSS
Maximum current out of each VSS_x ground line (sink)(1)
−100
IIO
Output current sunk by any I/O and control pin
Output current sourced by any I/Os and control pin
−25
∑IIO
Total output current sunk by sum of all I/O and control pins (2)
Total output current sourced by sum of all I/Os and control pins(2)
−120
IINJ(PIN)
 (3)
Injected current on FT pins (4)
−5/+0
Injected current on NRST and BOOT0 pins (4)
Injected current on TTa pins(5)
±5
∑IINJ(PIN)
(5)
Total injected current (sum of all I/O and control pins)(6)
±25
1.
All main power (VDD, VDDA) and ground (VSS, VSSA) pins must always be connected to the external power supply, in the
permitted range.
2.
This current consumption must be correctly distributed over all I/Os and control pins. The total output current must not be
sunk/sourced between two consecutive power supply pins referring to high pin count LQFP packages.
3.
Negative injection disturbs the analog performance of the device. See note in Section 6.3.21: 12-bit ADC characteristics.
4.
Positive injection is not possible on these I/Os and does not occur for input voltages lower than the specified maximum
value.
5.
A positive injection is induced by VIN>VDDA while a negative injection is induced by VIN<VSS. IINJ(PIN) must never be
exceeded. Refer to Table 14 for the values of the maximum allowed input voltage.
6.
When several inputs are submitted to a current injection, the maximum ΣIINJ(PIN) is the absolute sum of the positive and
negative injected currents (instantaneous values).
Table 16. Thermal characteristics
Symbol
Ratings
 Value
Unit
TSTG
Storage temperature range
−65 to +150
°C
TJ
Maximum junction temperature
°C

---

Electrical characteristics
6.3
Operating conditions
6.3.1
General operating conditions
Table 17. General operating conditions
Symbol
Parameter
 Conditions(1)
Min
Typ
Max
Unit
fHCLK
Internal AHB clock frequency
Power Scale 3 (VOS[1:0] bits in
PWR_CR register = 0x01), Regulator
ON, over-drive OFF
-
MHz
Power Scale 2 (VOS[1:0] bits
in PWR_CR register = 0x10),
Regulator ON
Over-
drive OFF
-
Over-
drive ON
-
Power Scale 1 (VOS[1:0] bits
in PWR_CR register= 0x11),
Regulator ON
Over-
drive OFF
-
Over-
drive ON
-
fPCLK1
Internal APB1 clock frequency
Over-drive OFF
-
Over-drive ON
-
fPCLK2
Internal APB2 clock frequency
Over-drive OFF
-
Over-drive ON
-
VDD
Standard operating voltage
1.7(2)
-
3.6
V
VDDA
(3)(4)
Analog operating voltage
(ADC limited to 1.2 M samples)
Must be the same potential as VDD
(5)
1.7(2)
-
2.4
Analog operating voltage
(ADC limited to 2.4 M samples)
2.4
-
3.6
VBAT
Backup operating voltage
1.65
-
3.6
V12
Regulator ON: 1.2 V internal
voltage on VCAP_1/VCAP_2 pins
Power Scale 3 ((VOS[1:0] bits in
PWR_CR register = 0x01), 120 MHz
HCLK max frequency
1.08
1.14
1.20
V
Power Scale 2 ((VOS[1:0] bits in
PWR_CR register = 0x10), 144 MHz
HCLK max frequency with over-drive
OFF or 168 MHz with over-drive ON
1.20
1.26
1.32
Power Scale 1 ((VOS[1:0] bits in
PWR_CR register = 0x11), 168 MHz
HCLK max frequency with over-drive
OFF or 180 MHz with over-drive ON
1.26
1.32
1.40
Regulator OFF: 1.2 V external
voltage must be supplied from
external regulator on
VCAP_1/VCAP_2 pins(6)
Max frequency 120 MHz
1.10
1.14
1.20
Max frequency 144 MHz
1.20
1.26
1.32
Max frequency 168 MHz
1.26
1.32
1.38

---

Electrical characteristics
VIN
Input voltage on RST and FT
pins(7)
2 V ≤ VDD ≤ 3.6 V
−0.3
-
5.5
V
VDD ≤ 2 V
−0.3
-
5.2
Input voltage on TTa pins
−0.3
-
VDDA+
0.3
Input voltage on BOOT0 pin
-
PD
Power dissipation at TA = 85 °C
for suffix 6 or TA = 105 °C for
suffix 7(8)
LQFP100
-
-
mW
WLCSP143
-
-
LQFP144
-
-
UFBGA169
-
-
LQFP176
-
-
UFBGA176
-
-
LQFP208
-
-
TFBGA216
-
-
TA
Ambient temperature for 6 suffix
version
Maximum power dissipation
−40
-
°C
Low power dissipation(9)
−40
-
Ambient temperature for 7 suffix
version
Maximum power dissipation
−40
-
°C
Low power dissipation(9)
−40
-
TJ
Junction temperature range
6 suffix version
−40
-
°C
7 suffix version
−40
-
1.
The overdrive mode is not supported at the voltage ranges from 1.7 to 2.1 V.
2.
VDD/VDDA minimum value of 1.7 V is obtained with the use of an external power supply supervisor (refer to Section 3.17.2:
Internal reset OFF).
3.
When the ADC is used, refer to Table 75: ADC characteristics.
4.
If a VREF+ pin is present, it must respect the following condition: VDDA-VREF+ < 1.2 V.
5.
It is recommended to power VDD and VDDA from the same source. A maximum difference of 300 mV between VDD and VDDA
can be tolerated during power-up and power-down operation.
6.
The overdrive mode is not supported when the internal regulator is OFF.
7.
To sustain a voltage higher than VDD+0.3, the internal pull-up and pull-down resistors must be disabled
8.
If TA is lower, higher PD values are allowed as long as TJ does not exceed TJmax.
9.
In low-power dissipation state, TA can be extended to this range as long as TJ does not exceed TJmax.
Table 17. General operating conditions (continued)
Symbol
Parameter
 Conditions(1)
Min
Typ
Max
Unit

---

Electrical characteristics
6.3.2
VCAP1/VCAP2 external capacitor
Stabilization for the main regulator is achieved by connecting an external capacitor CEXT to
the VCAP1/VCAP2 pins. CEXT is specified in Table 19.
Figure 24. External capacitor CEXT
1.
Legend: ESR is the equivalent series resistance.
Table 18. Limitations depending on the operating power supply range
Operating
power supply
range
ADC operation
Maximum Flash
memory access
frequency with
no wait states
(fFlashmax)
Maximum HCLK
frequency vs Flash
memory wait states
(1)(2)
I/O operation
Possible Flash
memory
operations
VDD =1.7 to
2.1 V(3)
Conversion time
up to 1.2 Msps
20 MHz(4)
168 MHz with 8 wait
states and over-drive
OFF
No I/O
compensation
8-bit erase and
program
operations only
VDD = 2.1 to
2.4 V
Conversion time
up to 1.2 Msps
22 MHz
180 MHz with 8 wait
states and over-drive
ON
No I/O
compensation
16-bit erase and
program
operations
VDD = 2.4 to
2.7 V
Conversion time
up to 2.4 Msps
24 MHz
180 MHz with 7 wait
states and over-drive
ON
I/O compensation
works
16-bit erase and
program
operations
VDD = 2.7 to
3.6 V(5)
Conversion time
up to 2.4 Msps
30 MHz
180 MHz with 5 wait
states and over-drive
ON
I/O compensation
works
32-bit erase and
program
operations
1.
Applicable only when the code is executed from flash memory. When the code is executed from RAM, no wait state is
required.
2.
Thanks to the ART accelerator and the 128-bit flash memory, the number of wait states given here does not impact the
execution speed from flash memory since the ART accelerator allows to achieve a performance equivalent to 0 wait state
program execution.
3.
The VDD/VDDA minimum value of 1.7 V is obtained with the use of an external power supply supervisor (refer to
Section 3.17.2: Internal reset OFF).
4.
Prefetch is not available.
5.
The voltage range for USB full speed  PHYs can drop down to 2.7 V. However, the electrical characteristics of D- and D+
pins are degraded between 2.7 and 3 V.
Table 19. VCAP1/VCAP2 operating conditions(1)
1.
When bypassing the voltage regulator, the two 2.2 µF VCAP capacitors are not required and should be
replaced by two 100 nF decoupling capacitors.
Symbol
Parameter
Conditions
CEXT
Capacitance of external capacitor
2.2 µF
ESR
ESR of external capacitor
< 2 Ω
MS19044V2
ESR
R Leak
C

---

Electrical characteristics
6.3.3
Operating conditions at power-up / power-down (regulator ON)
Subject to general operating conditions for TA.
Table 20. Operating conditions at power-up / power-down (regulator ON)
6.3.4
Operating conditions at power-up / power-down (regulator OFF)
Subject to general operating conditions for TA.
Symbol
Parameter
Min
Max
Unit
tVDD
VDD rise time rate
∞
µs/V
VDD fall time rate
∞
Table 21. Operating conditions at power-up / power-down (regulator OFF)(1)
1.
To reset the internal logic at power-down, a reset must be applied on pin PA0 when VDD reaches below
1.08 V.
Symbol
Parameter
Conditions
Min
Max
Unit
tVDD
VDD rise time rate
Power-up
∞
µs/V
VDD fall time rate
Power-down
∞
tVCAP
VCAP_1 and VCAP_2 rise time rate
Power-up
∞
VCAP_1 and VCAP_2 fall time rate
Power-down
∞

---

Electrical characteristics
6.3.5
Reset and power control block characteristics
The parameters given in Table 22 are derived from tests performed under ambient
temperature and VDD supply voltage conditions summarized in Table 17.
Table 22.  Reset and power control block characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
VPVD
Programmable voltage
detector level selection
PLS[2:0]=000 (rising edge)
2.09
2.14
2.19
V
PLS[2:0]=000 (falling edge)
1.98
2.04
2.08
V
PLS[2:0]=001 (rising edge)
2.23
2.30
2.37
V
PLS[2:0]=001 (falling edge)
2.13
2.19
2.25
V
PLS[2:0]=010 (rising edge)
2.39
2.45
2.51
V
PLS[2:0]=010 (falling edge)
2.29
2.35
2.39
V
PLS[2:0]=011 (rising edge)
2.54
2.60
2.65
V
PLS[2:0]=011 (falling edge)
2.44
2.51
2.56
V
PLS[2:0]=100 (rising edge)
2.70
2.76
2.82
V
PLS[2:0]=100 (falling edge)
2.59
2.66
2.71
V
PLS[2:0]=101 (rising edge)
2.86
2.93
2.99
V
PLS[2:0]=101 (falling edge)
2.75
2.84
2.92
V
PLS[2:0]=110 (rising edge)
2.96
3.03
3.10
V
PLS[2:0]=110 (falling edge)
2.85
2.93
2.99
V
PLS[2:0]=111 (rising edge)
3.07
3.14
3.21
V
PLS[2:0]=111 (falling edge)
2.95
3.03
3.09
V
VPVDhyst
(1)
PVD hysteresis
-
-
-
mV
VPOR/PDR
Power-on/power-down
reset threshold
Falling edge
1.60
1.68
1.76
V
Rising edge
1.64
1.72
1.80
V
VPDRhyst
(1)
PDR hysteresis
-
-
-
mV
VBOR1
Brownout level 1
threshold
Falling edge
2.13
2.19
2.24
V
Rising edge
2.23
2.29
2.33
V
VBOR2
Brownout level 2
threshold
Falling edge
2.44
2.50
2.56
V
Rising edge
2.53
2.59
2.63
V
VBOR3
Brownout level 3
threshold
Falling edge
2.75
2.83
2.88
V
Rising edge
2.85
2.92
2.97
V
VBORhyst
(1)
BOR hysteresis
-
-
-
mV
TRSTTEMPO
(1)(2)
POR reset temporization -
0.5
1.5
3.0
ms

---

Electrical characteristics
6.3.6
Overdrive switching characteristics
When the overdrive mode switches from enabled to disabled or disabled to enabled, the
system clock is stalled during the internal voltage set-up.
The overdrive switching characteristics are given in Table 23. They are subject to general
operating conditions for TA.
IRUSH
(1)
InRush current on
voltage regulator power-
on (POR or wakeup
from Standby)
-
-
mA
ERUSH
(1)
InRush energy on
voltage regulator power-
on (POR or wakeup
from Standby)
VDD = 1.7 V, TA = 105 °C,
IRUSH = 171 mA for 31 µs
-
-
5.4
µC
1.
Specified by design.
2.
The reset temporization is measured from the power-on (POR reset or wake-up from VBAT) to the instant
when the first instruction is read by the user application code.
Table 22.  Reset and power control block characteristics (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Table 23. Over-drive switching characteristics(1)
1.
Specified by design.
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Tod_swen
Over_drive switch
enable time
HSI
-
-
µs
HSE max for 4 MHz
and min for 26 MHz
-
External HSE
50 MHz
-
-
Tod_swdis
Over_drive switch
disable time
HSI
-
-
HSE max for 4 MHz
and min for 26 MHz.
-
External HSE
50 MHz
-
-

---

Electrical characteristics
6.3.7
Supply current characteristics
The current consumption is a function of several parameters and factors such as the
operating voltage, ambient temperature, I/O pin loading, device software configuration,
operating frequencies, I/O pin switching rate, program location in memory and executed
binary code.
The current consumption is measured as described in Figure 23: Current consumption
measurement scheme.
All the run-mode current consumption measurements given in this section are performed
with a reduced code that gives a consumption equivalent to CoreMark code.
Typical and maximum current consumption
The MCU is placed under the following conditions:
•
All I/O pins are in input mode with a static value at VDD or VSS (no load).
•
All peripherals are disabled except if it is explicitly mentioned.
•
The flash memory access time is adjusted both to fHCLK frequency and VDD range (see
Table 18: Limitations depending on the operating power supply range).
•
Regulator ON
•
The voltage scaling and overdrive mode are adjusted to fHCLK frequency as follows:
–
Scale 3 for fHCLK ≤ 120 MHz
–
Scale 2 for 120 MHz < fHCLK ≤ 144 MHz
–
Scale 1 for 144 MHz < fHCLK ≤ 180 MHz. The overdrive is only ON at 180 MHz.
•
The system clock is HCLK, fPCLK1 = fHCLK/4, and fPCLK2 = fHCLK/2.
•
The external clock frequency is 4 MHz and PLL is ON when fHCLK is higher than
25 MHz.
•
The maximum values are obtained for VDD = 3.6 V and a maximum ambient
temperature (TA), and the typical values for TA= 25 °C and VDD = 3.3 V unless
otherwise specified.

---

Electrical characteristics
Table 24. Typical and maximum current consumption in Run mode, code with data processing
 running from Flash memory (ART accelerator enabled except prefetch) or RAM(1)
Symbol
Parameter
Conditions
fHCLK (MHz)
Typ
Max(2)
Unit
TA =
25 °C
TA =
85 °C
TA =
105 °C
IDD
Supply
current in
RUN mode
All
Peripherals
enabled(3)(4)
104(5)
141(5)
mA
98(5)
133(5)
All
Peripherals
disabled(3)
47(5)
87(5)
45(5)
83(5)
1.
Code and data processing running from SRAM1 using boot pins.
2.
Evaluated by characterization.
3.
When analog peripheral blocks such as ADCs, DACs, HSE, LSE, HSI, or LSI are ON, an additional power consumption
should be considered.
4.
When the ADC is ON (ADON bit set in the ADC_CR2 register), add an additional power consumption of 1.6 mA per ADC
for the analog part.
5.
Evaluated by test in production.

---

Electrical characteristics
Table 25. Typical and maximum current consumption in Run mode, code with data processing
 running from Flash memory (ART accelerator disabled)
Symbol
Parameter
Conditions
fHCLK (MHz)
Typ
Max(1)
Unit
TA=
25 °C
TA=85 °C
TA=105 °C
IDD
Supply
current in
RUN mode
All Peripherals
enabled(2)(3)
mA
All Peripherals
disabled(3)
6.5
1.
Evaluated by characterization unless otherwise specified.
2.
When analog peripheral blocks such as ADCs, DACs, HSE, LSE, HSI, or LSI are ON, an additional power consumption
should be considered.
3.
When the ADC is ON (ADON bit set in the ADC_CR2 register), add an additional power consumption of 1.6 mA per ADC for
the analog part.

---

Electrical characteristics
Table 26. Typical and maximum current consumption in Sleep mode
Symbol
Parameter
Conditions
fHCLK (MHz)
Typ
Max(1)
Unit
TA =
25 °C
TA =
85 °C
TA =
105 °C
IDD
Supply
current in
Sleep mode
All
Peripherals
enabled(2)
89(3)
130(3)
mA
75(3)
110(3)
6.5
All
Peripherals
disabled
26(3)
76(3)
20(3)
58(3)
16.5
1.
Evaluated by characterization unless otherwise specified.
2.
When analog peripheral blocks such as ADCs, DACs, HSE, LSE, HSI, or LSI are ON, an additional power consumption
should be considered.
3.
Based on characterization, tested in production.

---

Electrical characteristics
Table 27. Typical and maximum current consumptions in Stop mode
Symbol
Parameter
Conditions
Typ
Max(1)
Unit
VDD = 3.6 V
TA =
25 °C
TA =
25 °C
TA =
85 °C
TA =
105 °C
IDD_STOP_NM
(normal mode)
Supply current in Stop
mode with voltage
regulator in main
regulator mode
Flash memory in Stop mode, all
oscillators OFF, no independent
watchdog
0.40
1.50
14.00
25.00
mA
Flash memory in Deep power
down mode, all oscillators OFF, no
independent watchdog
0.35
1.50
14.00
25.00
Supply current in Stop
mode with voltage
regulator in Low Power
regulator mode
Flash memory in Stop mode, all
oscillators OFF, no independent
watchdog
0.29
1.10
10.00
18.00
Flash memory in Deep power
down mode, all oscillators OFF, no
independent watchdog
0.23
1.10
10.00
18.00
IDD_STOP_UDM
(under-drive
mode)
Supply current in Stop
mode with voltage
regulator in main
regulator and under-
drive mode
Flash memory in Deep power
down mode, main regulator in
under-drive mode, all oscillators
OFF, no independent  watchdog
0.19
0.50
6.00
9.00
Supply current in Stop
mode with voltage
regulator in Low Power
regulator and under-
drive mode
Flash memory in Deep power
down mode, Low Power regulator
in under-drive mode, all oscillators
OFF, no independent  watchdog
0.10
0.40
4.00
7.00
1.
Data based on characterization, tested in production.

---

Electrical characteristics
Table 28. Typical and maximum current consumptions in Standby mode
Symbol
Parameter
Conditions
Typ(1)
Max(2)
Unit
TA = 25 °C
TA =
25 °C
TA =
85 °C
TA =
105 °C
VDD =
1.7 V
VDD=
2.4 V
VDD =
3.3 V
VDD = 3.6 V
IDD_STBY
Supply current
in Standby
mode
Backup SRAM ON, low-speed
oscillator (LSE) and RTC ON
2.80
3.00
3.60
7.00
19.00
36.00
µA
Backup SRAM OFF, low-
speed oscillator (LSE) and
RTC ON
2.30
2.60
3.10
6.00
16.00
31.00
Backup SRAM ON, RTC and
LSE OFF
2.30
2.50
2.90
6.00(3) 18.00(3) 35.00(3)
Backup SRAM OFF, RTC and
LSE OFF
1.70
1.90
2.20
5.00(3) 15.00(3) 30.00(3)
1.
The typical current consumption values are given with PDR OFF (internal reset OFF). When the PDR is OFF (internal reset
OFF), the typical current consumption is reduced by an additional 1.2 µA.
2.
Evaluated by characterization, not tested in production unless otherwise specified.
3.
Based on characterization, tested in production.
Table 29. Typical and maximum current consumptions in VBAT mode
Symbol
Parameter
Conditions(1)
Typ
Max(2)
Unit
TA = 25 °C
TA = 85 °C
TA =
105 °C
VBAT =
1.7 V
VBAT=
2.4 V
VBAT =
3.3 V
VBAT = 3.6 V
IDD_VBAT
Backup
domain supply
current
Backup SRAM ON, low-speed
oscillator (LSE) and RTC ON
1.28
1.40
1.62
µA
Backup SRAM OFF, low-speed
oscillator (LSE) and RTC ON
0.66
0.76
0.97
Backup SRAM ON, RTC and
LSE OFF
0.70
0.72
0.74
Backup SRAM OFF, RTC and
LSE OFF
0.10
0.10
0.10
1.
Crystal used: Abracon ABS07-120-32.768 kHz-T with a CL of 6 pF for typical values.
2.
Evaluated by characterization.

---

Electrical characteristics
Figure 25. Typical VBAT current consumption (LSE and RTC ON/backup RAM OFF)
Figure 26. Typical VBAT current consumption (LSE and RTC ON/backup RAM ON)
MS30490V1
0.5
1.5
2.5
0°C
25°C
55°C
85°C
105°C
IDD_VBAT (μA)
Temperature
1.65V
1.7V
1.8V
2V
2.4V
2.7V
3V
3.3V
3.6V
MS30491V1
IDD_VBAT (μA)
Temperature
0°C
25°C
55°C
85°C
105°C
1.65V
1.7V
1.8V
2V
2.4V
2.7V
3V
3.3V
3.6V

---

Electrical characteristics
Additional current consumption
The MCU is placed under the following conditions:
•
All I/O pins are configured in analog mode.
•
The flash memory access time is adjusted to fHCLK frequency.
•
The voltage scaling is adjusted to fHCLK frequency as follows:
–
Scale 3 for fHCLK ≤ 120 MHz,
–
Scale 2 for 120 MHz < fHCLK ≤ 144 MHz
–
Scale 1 for 144 MHz < fHCLK ≤ 180 MHz. The overdrive is only ON at 180 MHz.
•
The system clock is HCLK, fPCLK1 = fHCLK/4, and fPCLK2 = fHCLK/2.
•
HSE crystal clock frequency is 25 MHz.
•
When the regulator is OFF, V12 is provided externally as described in Table 17:
General operating conditions
•
TA= 25 °C.
Table 30. Typical current consumption in Run mode, code with data processing running from
 Flash memory or RAM, regulator ON (ART accelerator enabled except prefetch),
VDD=1.7 V(1)
Symbol
Parameter
Conditions
fHCLK (MHz)
Typ
Unit
IDD
Supply current in
RUN mode from
VDD supply
All Peripheral
enabled
88.2
mA
74.3
71.3
52.9
42.6
28.6
15.7
12.3
All Peripheral
disabled
40.6
30.6
32.6
24.7
19.7
13.6
7.7
6.7
1.
When peripherals are enabled, the power consumption corresponding to the analog part of the peripherals (such as ADC,
or DAC) is not included.

---

Electrical characteristics
Table 31.
Typical current consumption in Run mode, code with data processing running
 from Flash memory, regulator OFF (ART accelerator enabled except prefetch)(1)
Symbol
Parameter
Conditions
fHCLK
(MHz)
 VDD=3.3 V
 VDD=1.7 V
Unit
IDD12
IDD
IDD12
IDD
IDD12 / IDD
Supply current in
RUN mode from
V12 and VDD
supply
All Peripherals
enabled
77.8
1.3
76.8
1.0
mA
70.8
1.3
69.8
1.0
64.5
1.3
63.6
1.0
49.9
1.2
49.3
0.9
39.2
1.3
38.7
1.0
27.2
1.2
26.8
0.9
15.6
1.2
15.4
0.9
13.6
1.2
13.5
0.9
All Peripherals
disabled
38.2
1.3
37.0
1.0
34.6
1.3
33.4
1.0
31.3
1.3
30.3
1.0
24.0
1.2
23.2
0.9
18.1
1.4
18.0
1.0
12.9
1.2
12.5
0.9
7.2
1.2
6.9
0.9
6.3
1.2
6.1
0.9
1.
When peripherals are enabled, the power consumption corresponding to the analog part of the peripherals (such as ADC,
or DAC) is not included.

---

Electrical characteristics
Table 32. Typical current consumption in Sleep mode, regulator ON, VDD=1.7 V(1)
Symbol
Parameter
Conditions
fHCLK (MHz)
Typ
Unit
IDD
Supply current in Sleep
mode from VDD supply
All Peripherals enabled
65.5
mA
55.5
53.5
39.0
31.6
21.7
9.8
8.8
All Peripherals disabled
15.7
13.7
12.7
9.7
7.7
5.7
4.7
2.8
1.
When peripherals are enabled, the power consumption corresponding to the analog part of the peripherals (such as ADC,
or DAC) is not included.

---

Electrical characteristics
Table 33. Tyical current consumption in Sleep mode, regulator OFF(1)
Symbol
Parameter
Conditions
fHCLK (MHz)
 VDD=3.3 V
 VDD=1.7 V
Unit
IDD12
IDD
IDD12
IDD
IDD12/IDD
Supply current
in Sleep mode
from V12 and
VDD supply
All Peripherals
enabled
61.5
1.4
  -
-
mA
59.4
1.3
59.4
1.0
53.9
1.3
53.9
1.0
49.0
1.3
49.0
1.0
38.0
1.2
38.0
0.9
29.3
1.4
29.3
1.1
20.2
1.2
20.2
0.9
11.9
1.2
11.9
0.9
10.4
1.2
10.4
0.9
All Peripherals
disabled
14.9
1.4
  -
-
14.0
1.3
14.0
1.0
12.6
1.3
12.6
1.0
11.5
1.3
11.5
1.0
8.7
1.2
8.7
0.9
7.1
1.4
7.1
1.1
5.0
1.2
5.0
0.9
3.1
1.2
3.1
0.9
2.8
1.2
2.8
0.9
1.
When peripherals are enabled, the power consumption corresponding to the analog part of the peripherals (such as ADC,
or DAC) is not included.

---

Electrical characteristics
I/O system current consumption
The current consumption of the I/O system has two components: static and dynamic.
I/O static current consumption
All the I/Os used as inputs with pull resistors generate current consumption when the pin is
externally held to the opposite level. The value of this current consumption can be simply
computed by using the pull-up/pull-down resistor values given in Table 57: I/O static
characteristics.
For the output pins, any internal or external pull-up or pull-down and external load must also
be considered to estimate the current consumption.
Additional I/O current consumption is due to I/Os configured as inputs if an intermediate
voltage level is externally applied. This current consumption is caused by the input Schmitt
trigger circuits used to discriminate the input value. Unless this specific configuration is
required by the application, this supply current consumption can be avoided by configuring
these I/Os in analog mode. This is notably the case of ADC input pins, which should be
configured as analog inputs.
Caution:
Any floating input pin can also settle to an intermediate voltage level or switch inadvertently,
as a result of external electromagnetic noise. To avoid current consumption related to
floating pins, they must either be configured in analog mode, or forced internally to a definite
digital value. This can be done either by using pull-up/down resistors or by configuring the
pins in output mode.
I/O dynamic current consumption
In addition to the internal peripheral current consumption (see Table 35: Peripheral current
consumption), the I/Os used by an application also contribute to the current consumption.
When an I/O pin switches, it uses the current from the MCU supply voltage to supply the I/O
pin circuitry and to charge/discharge the capacitive load internal or external connected to
the pin:
where
ISW is the current sunk by a switching I/O to charge/discharge the capacitive load
VDD is the MCU supply voltage
fSW is the I/O switching frequency
C is the total capacitance seen by the I/O pin: C = CINT+ CEXT
The test pin is configured in push-pull output mode and is toggled by software at a fixed
frequency.
ISW
VDD
fSW
C
×
×
=

---

Electrical characteristics
Table 34. Switching output I/O current consumption(1)
1.
CS is the PCB board capacitance including the pad pin. CS = 7 pF (estimated value).
Symbol
Parameter
Conditions
I/O toggling
 frequency
(fsw)
Typ
Unit
IDDIO
I/O switching
Current
VDD = 3.3 V
C= CINT
(2)
2.
This test is performed by cutting the LQFP176 package pin (pad removal).
2 MHz
0.0
mA
8 MHz
0.2
25 MHz
0.6
50 MHz
1.1
60 MHz
1.3
84 MHz
1.8
90 MHz
1.9
VDD = 3.3 V
CEXT = 0 pF
C = CINT + CEXT
+ CS
2 MHz
0.1
8 MHz
0.4
25 MHz
1.23
50 MHz
2.43
60 MHz
2.93
84 MHz
3.86
90 MHz
4.07
IDDIO
I/O switching
Current
VDD = 3.3 V
CEXT = 10 pF
C = CINT + CEXT
+ CS
2 MHz
0.18
mA
8 MHz
0.67
25 MHz
2.09
50 MHz
3.6
60 MHz
4.5
84 MHz
7.8
90 MHz
9.8
VDD = 3.3 V
CEXT = 22 pF
C = CINT + CEXT
+ CS
2 MHz
0.26
8 MHz
1.01
25 MHz
3.14
50 MHz
6.39
60 MHz
10.68
VDD = 3.3 V
CEXT = 33 pF
C = CINT + Cext
+ CS
2 MHz
0.33
8 MHz
1.29
25 MHz
4.23
50 MHz
11.02

---

Electrical characteristics
On-chip peripheral current consumption
The MCU is placed under the following conditions:
•
At startup, all I/O pins are in analog input configuration.
•
All peripherals are disabled unless otherwise mentioned.
•
I/O compensation cell enabled.
•
The ART accelerator is ON.
•
Scale 1 mode selected, internal digital voltage V12 = 1.32 V.
•
HCLK is the system clock. fPCLK1 = fHCLK/4, and fPCLK2 = fHCLK/2.
The given value is calculated by measuring the difference of current consumption
–
with all the peripherals clocked off
–
with only one peripheral clocked on
–
fHCLK = 180 MHz (scale1 + overdrive ON), fHCLK = 144 MHz (scale 2),
fHCLK = 120 MHz (scale 3)"
•
Ambient operating temperature is 25 °C and VDD=3.3 V.
Table 35. Peripheral current consumption
Peripheral
IDD( Typ)(1)
Unit
Scale 1
 Scale 2
 Scale 3
AHB1
(up to
180 MHz)
GPIOA
2.50
2.36
2.08
µA/MHz
GPIOB
2.56
2.36
2.08
GPIOC
2.44
2.29
2.00
GPIOD
2.50
2.36
2.08
GPIOE
2.44
2.29
2.00
GPIOF
2.44
2.29
2.00
GPIOG
2.39
2.22
2.00
GPIOH
2.33
2.15
1.92
GPIOI
2.39
2.22
2.00
GPIOJ
2.33
2.15
1.92
GPIOK
2.33
2.15
1.92
OTG_HS+ULPI
27.00
24.86
21.92
CRC
0.44
0.42
0.33
BKPSRAM
0.78
0.69
0.58
DMA1
25.33
23.26
20.50
DMA2
24.72
22.71
20.00
DMA2D
28.50
26.32
23.33
ETH_MAC
ETH_MAC_TX
ETH_MAC_RX
ETH_MAC_PTP
21.56
20.07
17.75

---

Electrical characteristics
AHB2
(up to
180 MHz)
OTG_FS
25.67
26.67
23.58
µA/MHz
DCMI
3.72
3.40
3.00
RNG
2.28
2.36
2.17
AHB3
(up to
180 MHz)
FMC
21.39
19.79
17.50
µA/MHz
 Bus matrix(2)
14.06
13.19
11.75
µA/MHz
APB1
(up to
45 MHz)
TIM2
17.56
16.42
14.47
µA/MHz
TIM3
14.22
13.36
11.80
TIM4
14.89
13.64
12.13
TIM5
17.33
16.42
14.47
TIM6
2.89
2.53
2.47
TIM7
3.11
2.81
2.47
TIM12
7.33
6.97
6.13
TIM13
4.89
4.47
4.13
TIM14
5.56
5.31
4.80
PWR
11.11
10.31
9.13
USART2
4.22
3.92
3.47
USART3
4.44
4.19
3.80
UART4
4.00
3.92
3.47
UART5
4.00
3.92
3.47
UART7
4.00
3.92
3.47
UART8
3.78
3.92
3.47
I2C1
4.00
3.92
3.47
I2C2
4.00
3.92
3.47
I2C3
4.00
3.92
3.47
SPI2(3)
3.11
3.08
2.80
SPI3(3)
3.56
3.36
3.13
I2S2
2.89
2.81
2.47
I2S3
3.33
3.08
2.80
CAN1
6.89
6.42
5.80
CAN2
6.67
6.14
5.47
DAC(4)
2.89
2.25
2.13
WWDG
0.89
0.86
0.80
Table 35. Peripheral current consumption (continued)
Peripheral
IDD( Typ)(1)
Unit
Scale 1
 Scale 2
 Scale 3

---

Electrical characteristics
APB2
(up to
90 MHz)
SDIO
8.11
8.75
7.83
µA/MHz
TIM1
17.11
15.97
14.17
TIM8
17.33
16.11
14.33
TIM9
7.22
6.67
6.00
TIM10
4.56
4.31
3.83
TIM11
4.78
4.44
4.00
ADC1(5)
4.67
4.31
3.83
ADC2(5)
4.78
4.44
4.00
ADC3(5)
4.56
4.17
3.67
SPI1
1.44
1.39
1.17
USART1
4.00
3.75
3.33
USART6
4.00
3.75
3.33
SPI4
1.44
1.39
1.17
SPI5
1.44
1.39
1.17
SPI6
1.44
1.39
1.17
SYSCFG
0.78
0.69
0.67
LCD_TFT
39.89
37.22
33.17
SAI1
3.78
3.47
3.17
1.
When the I/O compensation cell is ON, IDD typical value increases by 0.22 mA.
2.
The BusMatrix is automatically active when at least one master is ON.
3.
To enable an I2S peripheral, first set the I2SMOD bit and then the I2SE bit in the SPI_I2SCFGR register.
4.
When the DAC is ON and EN1/2 bits are set in the DAC_CR register, add an additional power
consumption of 0.8 mA per DAC channel for the analog part.
5.
When the ADC is ON (ADON bit set in the ADC_CR2 register), add an additional power consumption of
1.6 mA per ADC for the analog part.
Table 35. Peripheral current consumption (continued)
Peripheral
IDD( Typ)(1)
Unit
Scale 1
 Scale 2
 Scale 3

---

Electrical characteristics
6.3.8
Wake-up time from low-power modes
The wake-up times given in Table 36 are measured starting from the wake-up event trigger
up to the first instruction executed by the CPU:
•
For Stop or Sleep modes: the wake-up event is WFE.
•
WKUP (PA0) pin is used to wake up from Standby, Stop, and Sleep modes.
All timings are derived from tests performed under ambient temperature and VDD=3.3 V.
Table 36. Low-power mode wakeup timings
Symbol
Parameter
Conditions
Typ(1)
Max(1)
Unit
tWUSLEEP
(2)
Wakeup from Sleep
-
-
CPU
clock
cycle
tWUSTOP
(2)
Wakeup from Stop mode
with MR/LP regulator in
normal mode
Main regulator is ON
13.6
-
µs
Main regulator is ON and Flash
memory in Deep power down mode
Low power regulator is ON
Low power regulator is ON and Flash
memory in Deep power down mode
tWUSTOP
(2)
Wakeup from Stop mode
with MR/LP regulator in
Under-drive mode
Main regulator in under-drive mode
(Flash memory in Deep power-down
mode)
Low power regulator in under-drive
mode
(Flash memory in Deep power-down
mode )
tWUSTDBY
(2)(3)
Wakeup from Standby
mode
-
1.
Evaluated by characterization.
2.
The wake-up times are measured from the wake-up event to the point in which the application code reads the first
3.
tWUSTDBY maximum value is given at –40 °C.

---

Electrical characteristics
6.3.9
External clock source characteristics
High-speed external user clock generated from an external source
In bypass mode the HSE oscillator is switched off and the input pin is a standard I/O. The
external clock signal has to respect the Table 57: I/O static characteristics. However, the
recommended clock input waveform is shown in Figure 27.
The characteristics given in Table 37 result from tests performed using a high-speed
external clock source, and under ambient temperature and supply voltage conditions
summarized in Table 17.
Table 37. High-speed external user clock characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fHSE_ext
External user clock source
frequency(1)
-
-
MHz
VHSEH
OSC_IN input pin high level voltage
0.7VDD
-
VDD
V
VHSEL
OSC_IN input pin low level voltage
VSS
-
0.3VDD
tw(HSE)
tw(HSE)
OSC_IN high or low time(1)
1.
Specified by design.
-
-
ns
tr(HSE)
tf(HSE)
OSC_IN rise or fall time(1)
-
-
Cin(HSE)
OSC_IN input capacitance(1)
-
-
-
pF
DuCy(HSE) Duty cycle
-
-
%
IL
OSC_IN Input leakage current
VSS ≤ VIN ≤ VDD
-
-
±1
µA

---

Electrical characteristics
Low-speed external user clock generated from an external source
In bypass mode the LSE oscillator is switched off and the input pin is a standard I/O. The
external clock signal has to respect the Table 57: I/O static characteristics. However, the
recommended clock input waveform is shown in Figure 28.
The characteristics given in Table 38 result from tests performed using a low-speed external
clock source, and under ambient temperature and supply voltage conditions summarized in
Table 17.
Figure 27. High-speed external clock source AC timing diagram
Table 38. Low-speed external user clock characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fLSE_ext
User External clock source
frequency(1)
-
-
32.768
kHz
VLSEH
OSC32_IN input pin high level
voltage
0.7VDD
-
VDD
V
VLSEL
OSC32_IN input pin low level voltage
VSS
-
0.3VDD
tw(LSE)
tf(LSE)
OSC32_IN high or low time(1)
-
-
ns
tr(LSE)
tf(LSE)
OSC32_IN rise or fall time(1)
-
-
Cin(LSE)
OSC32_IN input capacitance(1)
-
-
-
pF
DuCy(LSE)
Duty cycle
-
-
%
IL
OSC32_IN Input leakage current
VSS ≤ VIN ≤ VDD
-
-
±1
µA
1.
Specified by design.
ai17528
OSC_IN
External
STM32F
clock source
VHSEH
tf(HSE)
tW(HSE)
IL
90%
10 %
THSE
t
tr(HSE)
tW(HSE)
fHSE_ext
VHSEL

---

Electrical characteristics
Figure 28. Low-speed external clock source AC timing diagram
High-speed external clock generated from a crystal/ceramic resonator
The high-speed external (HSE) clock can be supplied with a 4 to 26 MHz crystal/ceramic
resonator oscillator. All the information in this paragraph is based on the characterization
results obtained with typical external components specified in Table 39. In the application,
the resonator and the load capacitors have to be placed as close as possible to the
oscillator pins to minimize output distortion and startup stabilization time. Refer to the crystal
resonator manufacturer for more details on the resonator characteristics (frequency,
package, accuracy).
Table 39. HSE 4-26 MHz oscillator characteristics (1)
1.
Specified by design.
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fOSC_IN
Oscillator frequency
-
-
MHz
RF
Feedback resistor
-
-
-
kΩ
IDD
HSE current consumption
VDD=3.3 V,
ESR= 30 Ω,
CL=5 pF@25 MHz
-
-
µA
VDD=3.3 V,
ESR= 30 Ω,
CL=10 pF@25 MHz
-
-
ACCHSE
(2)
2.
This parameter depends on the crystal used in the application. The minimum and maximum values must
be respected to comply with USB standard specifications.
HSE accuracy
-
−500
-
ppm
Gm_crit_max Maximum critical crystal gm
Startup
-
-
mA/V
tSU(HSE
(3)
3.
tSU(HSE) is the startup time measured from the moment that it is enabled (by software) until a stabilized 8
MHz oscillation is reached. This value is based on characterization and not tested in production. It is
measured for a standard crystal resonator and it can vary significantly with the crystal manufacturer.
Startup time
 VDD is stabilized
-
-
ms
ai17529
OSC32_IN
External
STM32F
clock source
VLSEH
tf(LSE)
tW(LSE)
IL
90%
10%
TLSE
t
tr(LSE)
tW(LSE)
fLSE_ext
VLSEL

---

Electrical characteristics
For CL1 and CL2, it is recommended to use high-quality external ceramic capacitors in the
5 pF to 25 pF range (typ.), designed for high-frequency applications, and selected to match
the requirements of the crystal or resonator (see Figure 29). CL1 and CL2 are usually the
same size. The crystal manufacturer typically specifies a load capacitance, which is the
series combination of CL1 and CL2. PCB and MCU pin capacitance must be included (10 pF
can be used as a rough estimate of the combined pin and board capacitance) when sizing
CL1 and CL2.
Note:
For information on selecting the crystal, refer to the application note AN2867 “Oscillator
design guide for ST microcontrollers” available from the ST website www.st.com.
Figure 29. Typical application with an 8 MHz crystal
1.
REXT value depends on the crystal characteristics.
Low-speed external clock generated from a crystal/ceramic resonator
The low-speed external (LSE) clock can be supplied with a 32.768 kHz crystal/ceramic
resonator oscillator. All the information given in this paragraph are based on
characterization results obtained with typical external components specified in Table 40. In
the application, the resonator and the load capacitors have to be placed as close as
possible to the oscillator pins to minimize output distortion and startup stabilization time.
Refer to the crystal resonator manufacturer for more details on the resonator characteristics
(frequency, package, accuracy).
Note:
For information on selecting the crystal, refer to the application note AN2867 “Oscillator
design guide for ST microcontrollers” available from the ST website www.st.com.
Table 40. LSE oscillator characteristics (fLSE = 32.768 kHz) (1)
1.
Specified by design.
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
RF
Feedback resistor
-
-
18.4
-
MΩ
IDD
LSE current consumption
-
-
-
µA
ACCLSE
(2)
2.
This parameter depends on the crystal used in the application. Refer to application note AN2867.
LSE accuracy
-
−500
-
ppm
Gm_crit_max Maximum critical crystal gm
Startup
-
-
0.56
µA/V
tSU(LSE)
(3)
3.
tSU(LSE) is the startup time measured from the moment that it is enabled (by software) to a stabilized
32.768 kHz oscillation is reached. This value is based on characterization and not tested in production. It is
measured for a standard crystal resonator and it can vary significantly with the crystal manufacturer.
startup time
 VDD is stabilized
-
-
s
ai17530
OSC_OUT
OSC_IN
fHSE
CL1
RF
STM32F
8 MHz
resonator
Resonator with
integrated capacitors
Bias
controlled
gain
REXT(1)
CL2

---

Electrical characteristics
Figure 30. Typical application with a 32.768 kHz crystal
6.3.10
Internal clock source characteristics
The parameters given in Table 41 and Table 42 are derived from tests performed under
ambient temperature and VDD supply voltage conditions summarized in Table 17.
High-speed internal (HSI) RC oscillator
ai17531
OSC32_OUT
OSC32_IN
fLSE
CL1
RF
STM32F
32.768 kHz
resonator
Resonator with
integrated capacitors
Bias
controlled
gain
CL2
Table 41. HSI oscillator characteristics (1)
1.
VDD = 3.3 V, PLL OFF, TA = –40 to 125 °C unless otherwise specified.
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fHSI
Frequency
-
-
-
MHz
ACCHSI
HSI user-trimming step (2)
2.
Specified by design.
-
-
-
%
Accuracy of the HSI oscillator
TA = –40 to 105 °C(3)
3.
Evaluated by characterization results.
−8
-
4.5
%
TA = –10 to 85 °C(3)
−4
-
%
TA = 25 °C(4)
4.
Factory calibrated, parts not soldered.
−1
-
%
tsu(HSI)
(2)
HSI oscillator startup time
-
-
2.2
µs
IDD(HSI)
(2)
HSI oscillator power
consumption
-
-
µA

---

Electrical characteristics
Figure 31. ACCHSI accuracy versus temperature
1.
Evaluated by characterization results.
Low-speed internal (LSI) RC oscillator
Table 42. LSI oscillator characteristics (1)
1.
VDD = 3 V, TA = –40 to 105 °C unless otherwise specified.
Symbol
Parameter
Min
Typ
Max
Unit
fLSI
(2)
2.
Evaluated by characterization results.
Frequency
kHz
tsu(LSI)
(3)
3.
Specified by design.
LSI oscillator startup time
-
µs
IDD(LSI)
(3)
LSI oscillator power consumption
-
0.4
0.6
µA
MSv41925V1
-8
-6
-4
-2
-40
ACCHSI (%)
TA (°C)
Min
Max
Typical

---

Electrical characteristics
Figure 32. ACCLSI versus temperature
6.3.11
PLL characteristics
The parameters given in Table 43 and Table 44 are derived from tests performed under
temperature and VDD supply voltage conditions summarized in Table 17.
MS19013V1
-40
-30
-20
-10
-45
-35
-25
-15
-5
Normalized deviation (%)
Temperature (°C)
max
avg
min
Table 43. Main PLL characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fPLL_IN
PLL input clock(1)
-
0.95(2)
2.10
MHz
fPLL_OUT
PLL multiplier output clock
-
-
MHz
fPLL48_OUT
48 MHz PLL multiplier output
clock
-
-
MHz
fVCO_OUT
PLL VCO output
-
-
MHz
tLOCK
PLL lock time
VCO freq = 100 MHz
-
µs
VCO freq = 432 MHz
-

---

Electrical characteristics
Jitter(3)
Cycle-to-cycle jitter
System clock
120 MHz
RMS
-
-
ps
peak
to
peak
-
±150
-
Period Jitter
RMS
-
-
peak
to
peak
-
±200
-
Main clock output (MCO) for
RMII Ethernet
Cycle to cycle at 50 MHz
on 1000 samples
-
-
Main clock output (MCO) for MII
Ethernet
Cycle to cycle at 25 MHz
on 1000 samples
-
-
Bit Time CAN jitter
Cycle to cycle at 1 MHz
on 1000 samples
-
-
IDD(PLL)
(4)
PLL power consumption on VDD
VCO freq = 100 MHz
VCO freq = 432 MHz
0.15
0.45
-
0.40
0.75
mA
IDDA(PLL)
(4)
PLL power consumption on
VDDA
VCO freq = 100 MHz
VCO freq = 432 MHz
0.30
0.55
-
0.40
0.85
mA
1.
Use the appropriate division factor M to obtain the specified PLL input clock values. The M factor is shared between PLL
and PLLI2S.
2.
Specified by design.
3.
The use of 2 PLLs in parallel could degrade the Jitter up to +30%.
4.
Evaluated by characterization.
Table 43. Main PLL characteristics (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Table 44. PLLI2S (audio PLL) characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fPLLI2S_IN
PLLI2S input clock(1)
-
0.95(2)
2.10
MHz
fPLLI2S_OUT
PLLI2S multiplier output clock
-
-
-
MHz
fVCO_OUT
PLLI2S VCO output
-
-
MHz
tLOCK
PLLI2S lock time
VCO freq = 100 MHz
-
µs
VCO freq = 432 MHz
-
Jitter(3)
Master I2S clock jitter
Cycle to cycle at
12.288 MHz on
48KHz period,
N=432, R=5
RMS
-
-
-
 peak
to
peak
-
 ±280
-
ps
Average frequency of
12.288 MHz
N = 432, R = 5
on 1000 samples
-
-
ps
WS I2S clock jitter
Cycle to cycle at 48 KHz
on 1000 samples
-
-
ps

---

Electrical characteristics
IDD(PLLI2S)
(4)
PLLI2S power consumption on
VDD
VCO freq = 100 MHz
VCO freq = 432 MHz
0.15
0.45
-
0.40
0.75
mA
IDDA(PLLI2S)
(4)
PLLI2S power consumption on
VDDA
VCO freq = 100 MHz
VCO freq = 432 MHz
0.30
0.55
-
0.40
0.85
mA
1.
Use the appropriate division factor M to have the specified PLL input clock values.
2.
Specified by design.
3.
Value given with the main PLL running.
4.
Evaluated by characterization.
Table 44. PLLI2S (audio PLL) characteristics (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Table 45. PLLISAI (audio and LCD-TFT PLL) characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fPLLSAI_IN
PLLSAI input clock(1)
-
0.95(2)
2.10
MHz
fPLLSAI_OUT
PLLSAI multiplier output clock
-
-
-
MHz
fVCO_OUT
PLLSAI VCO output
-
-
MHz
tLOCK
PLLSAI lock time
VCO freq = 100 MHz
-
µs
VCO freq = 432 MHz
-
Jitter(3)
Main SAI clock jitter
Cycle to cycle at
12.288 MHz on
48KHz period,
N=432, R=5
RMS
-
-
-
 peak
to
peak
-
 ±280
-
ps
Average frequency of
12.288 MHz
N = 432, R = 5
on 1000 samples
-
-
ps
FS clock jitter
Cycle to cycle at 48 KHz
on 1000 samples
-
-
ps
IDD(PLLSAI)
(4)
PLLSAI power consumption on
VDD
VCO freq = 100 MHz
VCO freq = 432 MHz
0.15
0.45
-
0.40
0.75
mA
IDDA(PLLSAI)
(4) PLLSAI power consumption on
VDDA
VCO freq = 100 MHz
VCO freq = 432 MHz
0.30
0.55
-
0.40
0.85
mA
1.
Use the appropriate division factor M to have the specified PLL input clock values.
2.
Specified by design.
3.
Value given with the main PLL running.
4.
Evaluated by characterization.

---

Electrical characteristics
6.3.12
PLL spread spectrum clock generation (SSCG) characteristics
The spread spectrum clock generation (SSCG) feature allows the decrease of
electromagnetic interferences (see Table 52: EMI characteristics for fHSE= 25 MHz and
fCPU= 168 MHz). It is available only on the main PLL.
Equation 1
The frequency modulation period (MODEPER) is given by the equation below:
fPLL_IN and fMod must be expressed in Hz.
As an example:
If fPLL_IN = 1 MHz, and fMOD = 1 kHz, the modulation depth (MODEPER) is given by
equation 1:
Equation 2
Equation 2 allows to calculate the increment step (INCSTEP):
fVCO_OUT must be expressed in MHz.
With a modulation depth (md) = ±2 % (4 % peak to peak), and PLLN = 240 (in MHz):
An amplitude quantization error may be generated because the linear modulation profile is
obtained by taking the quantized values (rounded to the nearest integer) of MODPER and
INCSTEP. As a result, the achieved modulation depth is quantized. The percentage
quantized modulation depth is given by the following formula:
As a result:
Table 46. SSCG parameters constraint
Symbol
Parameter
Min
Typ
Max(1)
Unit
fMod
Modulation frequency
-
-
KHz
md
Peak modulation depth
0.25
-
%
MODEPER * INCSTEP
-
-
-
215 −1
-
1.
Specified by design.
MODEPER
round fPLL_IN
fMod
×
(
)
⁄
[
]
=
MODEPER
round 106
×
(
)
⁄
[
]
=
=
INCSTEP
round
–
(
)
md
PLLN
×
×
(
)
×
MODEPER
×
(
)
⁄
[
]
=
INCSTEP
round
–
(
)
×
×
(
)
×
×
(
)
⁄
[
]
126md(quantitazed)%
=
=
mdquantized%
MODEPER
INCSTEP
×
×
×
(
)
–
(
)
PLLN
×
(
)
⁄
=
mdquantized%
×
×
×
(
)
–
(
)
×
(
)
⁄
2.002%(peak)
=
=

---

Electrical characteristics
Figure 33 and Figure 34 show the main PLL output clock waveforms in center spread and
down spread modes, where:
F0 is fPLL_OUT nominal.
Tmode is the modulation period.
md is the modulation depth.
Figure 33. PLL output clock waveforms in center spread mode
Figure 34. PLL output clock waveforms in down spread mode
Frequency (PLL_OUT)
Time
F0
tmode
2xtmode
md
ai17291
md
Frequency (PLL_OUT)
Time
F0
tmode
2xtmode
2xmd
ai17292b

---

Electrical characteristics
6.3.13
Memory characteristics
Flash memory
The characteristics are given at TA = –40 to 105 °C unless otherwise specified.
The devices are shipped to customers with the flash memory erased.
Table 47. Flash memory characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
IDD
Supply current
Write / Erase 8-bit mode, VDD = 1.7 V
-
-
mA
Write / Erase 16-bit mode, VDD = 2.1 V
-
-
Write / Erase 32-bit mode, VDD = 3.3 V
-
-
Table 48. Flash memory programming
Symbol
Parameter
 Conditions
Min(1)
Typ
Max(1)
Unit
tprog
Word programming time
Program/erase parallelism
(PSIZE) = x 8/16/32
-
100(2)
µs
tERASE16KB
Sector (16 KB) erase time
Program/erase parallelism
(PSIZE) = x 8
-
ms
Program/erase parallelism
(PSIZE) = x 16
-
Program/erase parallelism
(PSIZE) = x 32
-
tERASE64KB
Sector (64 KB) erase time
Program/erase parallelism
(PSIZE) = x 8
-
ms
Program/erase parallelism
(PSIZE) = x 16
-
Program/erase parallelism
(PSIZE) = x 32
-
tERASE128KB Sector (128 KB) erase time
Program/erase parallelism
(PSIZE) = x 8
-
s
Program/erase parallelism
(PSIZE) = x 16
-
1.3
2.6
Program/erase parallelism
(PSIZE) = x 32
-
tME
Mass erase time
Program/erase parallelism
(PSIZE) = x 8
-
s
Program/erase parallelism
(PSIZE) = x 16
-
Program/erase parallelism
(PSIZE) = x 32
-

---

Electrical characteristics
tBE
Bank erase time
Program/erase parallelism
(PSIZE) = x 8
-
s
Program/erase parallelism
(PSIZE) = x 16
-
Program/erase parallelism
(PSIZE) = x 32
-
Vprog
Programming voltage
32-bit program operation
2.7
-
3.6
V
16-bit program operation
2.1
-
3.6
V
8-bit program operation
1.7
-
3.6
V
1.
Evaluated by characterization.
2.
The maximum programming time is measured after 100 K erase operations.
Table 49. Flash memory programming with VPP
Symbol
Parameter
 Conditions
Min(1)
Typ
Max(1)
1.
Specified by design.
Unit
tprog
Double word programming
TA = 0 to +40 °C
VDD = 3.3 V
VPP = 8.5 V
-
100(2)
2.
The maximum programming time is measured after 100 K erase operations.
µs
tERASE16KB
Sector (16 KB) erase time
-
-
ms
tERASE64KB
Sector (64 KB) erase time
-
-
tERASE128KB
Sector (128 KB) erase time
-
-
tME
Mass erase time
-
6.9
-
s
tBE
Bank erase time
-
-
6.9
-
s
Vprog
Programming voltage
-
2.7
-
3.6
V
VPP
VPP voltage range
-
-
V
IPP
Minimum current sunk on
the VPP pin
-
-
-
mA
tVPP
(3)
3.
VPP should only be connected during programming/erasing.
Cumulative time during
which VPP is applied
-
-
-
hour
Table 48. Flash memory programming (continued)
Symbol
Parameter
 Conditions
Min(1)
Typ
Max(1)
Unit

---

Electrical characteristics
Table 50. Flash memory endurance and data retention
6.3.14
EMC characteristics
Susceptibility tests are performed on a sample basis during device characterization.
Functional EMS (electromagnetic susceptibility)
While a simple application is executed on the device (toggling 2 LEDs through I/O ports).
Two electromagnetic events stress the device until a failure occurs. The LEDs indicate the
failure:
•
Electrostatic discharge (ESD) (positive and negative) is applied to all device pins until
a functional disturbance occurs. This test is compliant with the IEC 61000-4-2 standard.
•
FTB: A burst of fast transient voltage (positive and negative) is applied to VDD and VSS
through a 100 pF capacitor, until a functional disturbance occurs. This test is compliant
with the IEC 61000-4-4 standard.
A device reset allows normal operations to be resumed.
The test results are given in Table 51. They are based on the EMS levels and classes
defined in application note AN1709.
When the application is exposed to a noisy environment, it is recommended to avoid pin
exposition to disturbances. The pins showing a middle range robustness are: PA0, PA1,
PA2, PH2, PH3, PH4, PH5, PA3, PA4, PA5, PA6, PA7, PC4, and PC5.
As a consequence, it is recommended to add a serial resistor (1 kΏ) located as close as
possible to the MCU to the pins exposed to noise (connected to tracks longer than 50 mm
on PCB).
Symbol
Parameter
 Conditions
Value
Unit
Min(1)
1.
Evaluated by characterization results.
NEND
Endurance
TA = –40 to +85 °C (6 suffix versions)
TA = –40 to +105 °C (7 suffix versions)
kcycles
tRET
Data retention
1 kcycle(2) at TA = 85 °C
2.
Cycling performed over the whole temperature range.
Years
1 kcycle(2) at TA = 105 °C
10 kcycles(2) at TA = 55 °C
Table 51. EMS characteristics
Symbol
Parameter
Conditions
Level/
Class
VFESD
Voltage limits to be applied on any I/O pin to
induce a functional disturbance
VDD = 3.3 V, LQFP176, TA =
+25 °C, fHCLK = 168 MHz, conforms
to IEC 61000-4-2
2B
VEFTB
Fast transient voltage burst limits to be
applied through 100 pF on VDD and VSS
pins to induce a functional disturbance
VDD = 3.3 V, LQFP176, TA =+25 °C,
fHCLK = 168 MHz, conforms to
IEC 61000-4-2
4A

---

Electrical characteristics
Designing hardened software to avoid noise problems
EMC characterization and optimization are performed at component level with a typical
application environment and simplified MCU software. It should be noted that good EMC
performance is highly dependent on the user application and the software in particular.
Therefore, it is recommended that the user applies EMC software optimization and
prequalification tests in relation with the EMC level requested for his application.
Software recommendations
The software flowchart must include the management of runaway conditions such as:
•
Corrupted program counter
•
Unexpected reset
•
Critical data corruption (control registers...)
Prequalification trials
Most of the common failures (unexpected reset and program counter corruption) can be
reproduced by manually forcing a low state on the NRST pin or the oscillator pins for 1
second.
To complete these trials, ESD stress can be applied directly on the device, over the range of
specification values. When unexpected behavior is detected, the software can be hardened
to prevent unrecoverable errors occurring (see application note AN1015).
Electromagnetic Interference (EMI)
The electromagnetic field emitted by the device is monitored while a simple application,
executing EEMBC? code, is running. This emission test is compliant with SAE IEC61967-2
standard, which specifies the test board and the pin loading.
Table 52. EMI characteristics for fHSE= 25 MHz and fCPU= 168 MHz
Symbol
Parameter
Conditions
Monitored
frequency band
Max vs.
[fHSE/fCPU]
Unit
25/168 MHz
SEMI
Peak(1)
VDD = 3.3 V, TA = 25 °C, LQFP176
package, conforming to SAE J1752/3
EEMBC, ART ON, all peripheral
clocks enabled, clock dithering
disabled.
0.1 to 30 MHz
dBµV
30 to 130 MHz
130 MHz to
1GHz
Level(2)
0.1 MHz to
1GHz
-
Peak(1)
VDD = 3.3 V, TA = 25 °C, LQFP176
package, conforming to SAE J1752/3
EEMBC, ART ON, all peripheral
clocks enabled, clock dithering
enabled
0.1 to 30 MHz
dBµV
30 to 130 MHz
130 MHz to
1GHz
Level(2)
0.1 MHz to
1GHz
3.5
-
1.
Refer to chapter “EMI radiated test” in AN1709.
2.
Refer to chapter “EMI level classification” in AN1709.

---

Electrical characteristics
Table 53. EMI characteristics for HSE= 25 MHz and fCPU= 180 MHz
6.3.15
Absolute maximum ratings (electrical sensitivity)
Based on three different tests (ESD, LU) using specific measurement methods, the device is
stressed to determine its performance in terms of electrical sensitivity.
Electrostatic discharge (ESD)
Electrostatic discharges (a positive then a negative pulse separated by 1 second) are
applied to the pins of each sample according to each pin combination. The sample size
depends on the number of supply pins in the device (3 parts × (n+1) supply pins). This test
conforms to the ANSI/ESDA/JEDEC JS-001 and ANSI/ESD S5.3.1 standards.
Symbol
Parameter
Conditions
Monitored
frequency band
Max vs.
[fHSE/fCPU]
Unit
25/180 MHz
SEMI
Peak(1)
VDD = 3.3 V, TA = 25 °C, LQFP176
package, conforming to SAE J1752/3
EEMBC, ART ON, all peripheral
clocks enabled, clock dithering
disabled.
0.1 to 30 MHz
dBµV
30 to 130 MHz
130 MHz to
1GHz
Level(2)
0.1 MHz to
1GHz
-
Peak(1)
VDD = 3.3 V, TA = 25 °C, LQFP176
package, conforming to SAE J1752/3
EEMBC, ART ON, all peripheral
clocks enabled, clock dithering
enabled
0.1 to 30 MHz
dBµV
30 to 130 MHz
130 MHz to
1GHz
Level(2)
0.1 MHz to
1GHz
3.5
-
1.
Refer to chapter “EMI radiated test” in AN1709.
2.
Refer to chapter “EMI level classification” in AN1709.
Table 54. ESD absolute maximum ratings
Symbol
Ratings
Conditions
Class
Maximum
value(1)
Unit
VESD(HBM)
Electrostatic discharge
voltage (human body
model)
TA = +25 °C conforming to
ANSI/ESDA/JEDEC JS-001
V
VESD(CDM)
Electrostatic discharge
voltage (charge device
model)
TA = +25 °C conforming to ANSI/ESD S5.3.1,
LQFP100/144/176, UFBGA169/176,
TFBGA176 and WLCSP143 packages
C3
TA = +25 °C conforming to ANSI/ESD S5.3.1,
LQFP208 package
C3
1.
Evaluated by characterization.

---

Electrical characteristics
Static latchup
Two complementary static tests are required on six parts to assess the latchup
performance:
•
A supply overvoltage is applied to each power supply pin
•
A current injection is applied to each input, output, and configurable I/O pin
These tests are compliant with the EIA/JESD 78A IC latchup standard.
6.3.16
I/O current injection characteristics
As a general rule, current injection to the I/O pins, due to external voltage below VSS or
above VDD (for standard, 3 V-capable I/O pins) should be avoided during normal product
operation. However, to give an indication of the robustness of the microcontroller in cases
when abnormal injection accidentally happens, susceptibility tests are performed on a
sample basis during device characterization.
Functional susceptibility to I/O current injection
While a simple application is executed on the device, the device is stressed by injecting
current into the I/O pins programmed in floating input mode. While current is injected into
the I/O pin, one at a time, the device is checked for functional failures.
An out of range parameter indicates the failure: ADC error above a certain limit (>5 LSB
TUE), out of conventional limits of induced leakage current on adjacent pins (out of –
5 µA/+0 µA range), or other functional failure (for example reset, oscillator frequency
deviation).
Negative induced leakage current is caused by negative injection and positive induced
leakage current by positive injection.
The test results are given in Table 56.
Table 55. Electrical sensitivities
Symbol
Parameter
Conditions
Class
LU
Static latch-up class
TA = +105 °C conforming to JESD78A
II level A
Table 56. I/O current injection susceptibility(1)
Symbol
Description
Functional susceptibility
Unit
Negative
injection
Positive
injection
IINJ
Injected current on BOOT0 pin
−0
NA
mA
Injected current on NRST pin
−0
NA
Injected current on PA0, PA1, PA2, PA3, PA6, PA7, PB0,
PC0, PC1, PC2, PC3, PC4, PC5, PH1, PH2, PH3, PH4, PH5
−0
NA
Injected current on TTa pins: PA4 and PA5
−0
+5
Injected current on any other FT pin
−5
NA
1.
NA = not applicable.

---

Electrical characteristics
Note:
It is recommended to add a Schottky diode (pin to ground) to analog pins, which may
potentially inject negative currents.
6.3.17
I/O port characteristics
General input/output characteristics
Unless otherwise specified, the parameters given in Table 57: I/O static characteristics are
derived from tests performed under the conditions summarized in Table 17. All I/Os are
CMOS and TTL compliant.
Note:
For information on GPIO configuration, refer to the application note AN4899 “STM32 GPIO
configuration for hardware settings and low-power consumption” available from
www.st.com.
Table 57. I/O static characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
VIL
FT, TTa and NRST I/O input
low level voltage
1.7 V ≤VDD ≤3.6 V
-
-
0.35VDD−0.04
(1)
V
0.3VDD
(2)
BOOT0 I/O input low level
voltage
1.75 VDD ≤3.6 V,
–40 °C ≤TA ≤105 °C
-
-
0.1VDD+0.1(1)
1.7 V ≤VDD ≤3.6 V,
0 °C ≤TA ≤105 °C
-
-
VIH
FT, TTa and NRST I/O input
high level voltage(5)
1.7 V ≤VDD ≤3.6 V
0.45VDD+0.3(1)
-
-
V
0.7VDD
(2)
BOOT0 I/O input high level
voltage
1.75 V ≤VDD ≤3.6 V,
–40 °C ≤TA ≤105 °C
0.17VDD+0.7(1)
-
-
1.7 V ≤VDD ≤3.6 V,
0 °C ≤TA ≤105 °C
VHYS
FT, TTa and NRST I/O input
hysteresis
1.7 V ≤VDD ≤3.6 V
10%VDD
(3)
-
-
V
BOOT0 I/O input hysteresis
1.75 V ≤VDD ≤3.6 V,
–40 °C ≤TA ≤105 °C
0.1
-
-
1.7 V ≤VDD ≤3.6 V,
0 °C ≤TA ≤105 °C
Ilkg
I/O input leakage current (4)
VSS ≤VIN ≤VDD
-
-
±1
µA
I/O FT input leakage current (5)
VIN = 5 V
-
-

---

Electrical characteristics
All I/Os are CMOS and TTL compliant (no software configuration required). Their
characteristics cover more than the strict CMOS-technology or TTL parameters. The
coverage of these requirements for FT I/Os is shown in Figure 35.
RPU
Weak pull-up
equivalent
resistor(6)
All pins
except for
PA10/PB12
(OTG_FS_ID,
OTG_HS_ID)
VIN = VSS
kΩ
PA10/PB12
(OTG_FS_ID,
OTG_HS_ID)
RPD
Weak pull-
down
equivalent
resistor(7)
All pins
except for
PA10/PB12
(OTG_FS_ID,
OTG_HS_ID)
VIN = VDD
 PA10/PB12
(OTG_FS_ID,
OTG_HS_ID)
CIO
(8)
I/O pin capacitance
-
-
-
pF
1.
Specified by design.
2.
Tested in production.
3.
With a minimum of 200 mV.
4.
Leakage could be higher than the maximum value, if negative current is injected on adjacent pins, refer to Table 56: I/O
current injection susceptibility
5.
To sustain a voltage higher than VDD +0.3 V, the internal pull-up/pull-down resistors must be disabled. Leakage could be
higher than the maximum value, if a negative current is injected on adjacent pins. Refer to Table 56: I/O current injection
susceptibility
6.
Pull-up resistors are designed with a true resistance in series with a switchable PMOS. This PMOS contribution to the series
resistance is minimum (~10% order).
7.
Pull-down resistors are designed with a true resistance in series with a switchable NMOS. This NMOS contribution to the
series resistance is minimum (~10% order).
8.
 Hysteresis voltage between Schmitt trigger switching levels. Evaluated by characterization.
Table 57. I/O static characteristics (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit

---

Electrical characteristics
Figure 35. FT I/O input characteristics
Output driving current
The GPIOs (general-purpose input/outputs) can sink or source up to ±8 mA, and sink or
source up to ±20 mA (with a relaxed VOL/VOH) except PC13, PC14, PC15, and PI8, which
can sink or source up to ±3mA. When using the PC13 to PC15 and PI8 GPIOs in output
mode, the speed should not exceed 2 MHz with a maximum load of 30 pF.
In the user application, the number of I/O pins, which can drive current must be limited to
respect the absolute maximum rating specified in Section 6.2. In particular:
•
The sum of the currents sourced by all the I/Os on VDD, plus the maximum Run
consumption of the MCU sourced on VDD, cannot exceed the absolute maximum rating
∑IVDD (see Table 15).
•
The sum of the currents sunk by all the I/Os on VSS plus the maximum Run
consumption of the MCU sunk on VSS cannot exceed the absolute maximum rating
∑IVSS (see Table 15).
MS33746V1
1.92
1.065
1.22
1.7
2.0
2.4
2.7
3.3
3.6
2.0
0.55
0.8
VDD (V)
VIL/VIH (V)
Tested in production  - CMOS requirement VIHmin = 0.7VDD
Tested in production  - CMOS requirement VILmax = 0.3VDD
Based on Design simulations, VILmax= 0.35VDD-0.04
TTL requirement
VIHmin = 2V
TTL requirement VILmax
= 0.8V
0.51
2.52
Area not
determined
1.19
1.7
Based on Design simulations, VIHmin= 0.45VDD+0.3

---

Electrical characteristics
Output voltage levels
Unless otherwise specified, the parameters given in Table 58 are derived from tests
performed under ambient temperature and VDD supply voltage conditions summarized in
Table 17. All I/Os are CMOS and TTL compliant.
Table 58. Output voltage characteristics
Symbol
Parameter
Conditions
Min
Max
Unit
VOL
(1)
1.
The IIO current sunk by the device must always respect the absolute maximum rating specified in Table 15.
and the sum of IIO (I/O ports and control pins) must not exceed IVSS.
Output low level voltage for an I/O pin
CMOS port(2)
IIO = +8 mA
2.7 V ≤VDD ≤ 3.6 V
2.
TTL and CMOS outputs are compatible with JEDEC standards JESD36 and JESD52.
-
0.4
V
VOH
(3)
3.
The IIO current sourced by the device must always respect the absolute maximum rating specified in
Table 15 and the sum of IIO (I/O ports and control pins) must not exceed IVDD.
Output high level voltage for an I/O pin
VDD −0.4
-
VOL
(1)
Output low level voltage for an I/O pin
TTL port(2)
IIO =+ 8mA
2.7 V ≤VDD ≤ 3.6 V
-
0.4
V
VOH
(3)
Output high level voltage for an I/O pin
2.4
-
VOL
(1)
Output low level voltage for an I/O pin
IIO = +20 mA
2.7 V ≤ VDD ≤ 3.6 V
-
1.3(4)
4.
Based on characterization data.
V
VOH
(3)
Output high level voltage for an I/O pin
VDD−1.3(4)
-
VOL
(1)
Output low level voltage for an I/O pin
IIO = +6 mA
1.8 V ≤ VDD ≤ 3.6 V
-
0.4(4)
V
VOH
(3)
Output high level voltage for an I/O pin
VDD−0.4(4)
-
VOL
(1)
Output low level voltage for an I/O pin
IIO = +4 mA
1.7 V ≤ VDD ≤ 3.6V
-
0.4(5)
5.
Specified by design.
V
VOH
(3)
Output high level voltage for an I/O pin
VDD−0.4(5)
-

---

Electrical characteristics
Input/output AC characteristics
The definition and values of input/output AC characteristics are given in Figure 36 and
Table 59, respectively.
Unless otherwise specified, the parameters given in Table 59 are derived from tests
performed under the ambient temperature and VDD supply voltage conditions summarized
in Table 17.
Table 59. I/O AC characteristics(1)(2)
OSPEEDRy
[1:0] bit
value(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fmax(IO)out
Maximum frequency(3)
CL = 50 pF, VDD ≥ 2.7 V
-
-
MHz
CL = 50 pF, VDD ≥ 1.7 V
-
-
CL = 10 pF, VDD ≥ 2.7 V
-
-
CL = 10 pF, VDD ≥ 1.8 V
-
-
CL = 10 pF, VDD ≥ 1.7 V
-
-
tf(IO)out/
tr(IO)out
Output high to low level fall
time and output low to high
level rise time
CL = 50 pF, VDD = 1.7 V to
3.6 V
-
-
ns
fmax(IO)out
Maximum frequency(3)
CL = 50 pF, VDD≥ 2.7 V
-
-
MHz
CL = 50 pF, VDD≥ 1.8 V
-
-
12.5
CL = 50 pF, VDD≥ 1.7 V
-
-
CL = 10 pF, VDD ≥ 2.7 V
-
-
CL = 10 pF, VDD≥ 1.8 V
-
-
CL = 10 pF, VDD≥ 1.7 V
-
-
12.5
tf(IO)out/
tr(IO)out
Output high to low level fall
time and output low to high
level rise time
CL = 50 pF, VDD ≥ 2.7 V
-
-
ns
CL = 10 pF, VDD ≥ 2.7 V
-
-
CL = 50 pF, VDD ≥ 1.7 V
-
-
CL = 10 pF, VDD ≥ 1.7 V
-
-
fmax(IO)out
Maximum frequency(3)
CL = 40 pF, VDD ≥ 2.7 V
-
-
50(4)
MHz
CL = 10 pF, VDD ≥ 2.7 V
-
-
100(4)
CL = 40 pF, VDD ≥ 1.7 V
-
-
CL = 10 pF, VDD ≥ 1.8 V
-
-
CL = 10 pF, VDD ≥ 1.7 V
-
-
42.5
tf(IO)out/
tr(IO)out
Output high to low level fall
time and output low to high
level rise time
CL = 40 pF, VDD ≥2.7 V
-
-
ns
CL = 10 pF, VDD ≥ 2.7 V
-
-
CL = 40 pF, VDD ≥ 1.7 V
-
-
CL = 10 pF, VDD ≥ 1.7 V
-
-

---

Electrical characteristics
Figure 36. I/O AC characteristics definition
fmax(IO)out
Maximum frequency(3)
CL = 30 pF, VDD ≥ 2.7 V
-
-
100(4)
MHz
CL = 30 pF, VDD ≥ 1.8 V
-
-
CL = 30 pF, VDD ≥ 1.7 V
-
-
42.5
CL = 10 pF, VDD≥ 2.7 V
-
-
180(4)
CL = 10 pF, VDD ≥ 1.8 V
-
-
CL = 10 pF, VDD ≥ 1.7 V
-
-
72.5
tf(IO)out/
tr(IO)out
Output high to low level fall
time and output low to high
level rise time
CL = 30 pF, VDD ≥ 2.7 V
-
-
ns
CL = 30 pF, VDD ≥1.8 V
-
-
CL = 30 pF, VDD ≥1.7 V
-
-
CL = 10 pF, VDD ≥ 2.7 V
-
-
2.5
CL = 10 pF, VDD ≥1.8 V
-
-
3.5
CL = 10 pF, VDD ≥1.7 V
-
-
-
tEXTIpw
Pulse width of external signals
detected by the EXTI
controller
-
-
-
ns
1.
Specified by design.
2.
The I/O speed is configured using the OSPEEDRy[1:0] bits. Refer to the STM32F4xx reference manual for a description of
the GPIOx_SPEEDR GPIO port output speed register.
3.
The maximum frequency is defined in Figure 36.
4.
For maximum frequencies above 50 MHz and VDD > 2.4 V, the compensation cell should be used.
Table 59. I/O AC characteristics(1)(2) (continued)
OSPEEDRy
[1:0] bit
value(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
MS32132V4
Maximum frequency is achieved with a duty cycle at (45 - 55%) when loaded by the
specified capacitance.
T
10%
50%
90%
10%
50%
90%
r(IO)out
t
f(IO)out
t

---

Electrical characteristics
6.3.18
NRST pin characteristics
The NRST pin input driver uses CMOS technology. It is connected to a permanent pull-up
resistor, RPU (see Table 57: I/O static characteristics).
Unless otherwise specified, the parameters given in Table 60 are derived from tests
performed under the ambient temperature and VDD supply voltage conditions summarized
in Table 17.
Figure 37. Recommended NRST pin protection
1.
The reset network protects the device against parasitic resets.
2.
The external capacitor must be placed as close as possible to the device.
3.
The user must ensure that the level on the NRST pin can go below the VIL(NRST) max level specified in
Table 60. Otherwise, the reset is not considered by the device.
Table 60. NRST pin characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
RPU
Weak pull-up equivalent resistor(1)
VIN = VSS
kΩ
TF(NRST)
(2)
NRST Input filtered pulse
-
-
-
ns
TNF(NRST)
(2)
NRST Input not filtered pulse
VDD > 2.7 V
-
-
ns
TNRST_OUT
Generated reset pulse duration
Internal Reset source
-
-
µs
1.
The pull-up is designed with a true resistance in series with a switchable PMOS. This PMOS contribution to the series
resistance must be minimum (~10% order).
2.
Specified by design.
ai14132c
STM32F
RPU
NRST(2)
VDD
Filter
Internal Reset
0.1 μF
External
reset circuit(1)

---

Electrical characteristics
6.3.19
TIM timer characteristics
The parameters given in Table 61 are specified by design.
Refer to Section 6.3.17: I/O port characteristics for details on the input/output alternate
function characteristics (output compare, input capture, external clock, PWM output).
6.3.20
Communications interfaces
I2C interface characteristics
The I2C interface meets the timing requirements of the I2C-bus specification and user
manual rev. 03 for:
•
Standard mode (Sm): with a bit rate up to 100 kbit/s
•
Fast mode (Fm): with a bit rate up to 400 kbit/s.
The I2C timings requirements are specified by design when the I2C peripheral is properly
configured (refer to RM0090 reference manual).
The SDA and SCL I/O requirements are met with the following restrictions: the SDA and
SCL I/O pins are not “true” open-drain. When configured as open-drain, the PMOS
connected between the I/O pin and VDD is disabled, but is still present. Refer to
Section 6.3.17: I/O port characteristics for more details on the I2C I/O characteristics.
All I2C SDA and SCL I/Os embed an analog filter. Refer to the table below for the analog
filter characteristics:
Table 61. TIMx characteristics(1)(2)
1.
TIMx is used as a general term to refer to the TIM1 to TIM12 timers.
2.
Specified by design.
Symbol
Parameter
Conditions(3)
3.
The maximum timer frequency on APB1 or APB2 is up to 180 MHz, by setting the TIMPRE bit in the
RCC_DCKCFGR register, if APBx prescaler is 1 or 2 or 4, then TIMxCLK = HCKL, otherwise TIMxCLK =
4x PCLKx.
Min
Max
Unit
tres(TIM)
Timer resolution time
AHB/APBx prescaler=1
or 2 or 4, fTIMxCLK =
180 MHz
-
tTIMxCLK
AHB/APBx prescaler>4,
fTIMxCLK = 90 MHz
-
tTIMxCLK
fEXT
Timer external clock
frequency on CH1 to CH4  fTIMxCLK = 180 MHz
fTIMxCLK/2
MHz
ResTIM
Timer resolution
-
bit
tMAX_COUNT
Maximum possible count
with 32-bit counter
-
-
65536 ×
65536
tTIMxCLK

---

Electrical characteristics
SPI interface characteristics
Unless otherwise specified, the parameters given in Table 63 for the SPI interface are
derived from tests performed under the ambient temperature, fPCLKx frequency, and VDD
supply voltage conditions summarized in Table 17, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load C = 30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
Refer to Section 6.3.17: I/O port characteristics for more details on the input/output alternate
function characteristics (NSS, SCK, MOSI, MISO for SPI).
Table 62. I2C analog filter characteristics(1)
1.
Specified by design.
Symbol
Parameter
Min
Max
Unit
tAF
Maximum pulse width of spikes that
are suppressed by the analog filter
50(2)
2.
Spikes with widths below tAF(min) are filtered.
260(3)
3.
Spikes with widths above tAF(max) are not filtered
ns
Table 63. SPI dynamic characteristics(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fSCK
1/tc(SCK)
SPI clock frequency
Master mode, SPI1/4/5/6,
2.7 V≤VDD≤3.6 V
-
-
MHz
Slave mode,
SPI1/4/5/6,
2.7 V≤VDD≤3.6 V
Receiver
Transmitter/
full-duplex
38(2)
Master mode, SPI1/2/3/4/5/6,
1.7 V≤VDD≤3.6 V
-
-
22.5
Slave mode, SPI1/2/3/4/5/6,
1.7 V≤VDD≤3.6 V
22.5
Duty(SCK)
Duty cycle of SPI clock
frequency
Slave mode
%

---

Electrical characteristics
tw(SCKH)
SCK high and low time
Master mode, SPI presc = 2,
2.7 V≤VDD≤3.6 V
TPCLK −0.5
TPCLK
TPCLK+0.5
ns
tw(SCKL)
Master mode, SPI presc = 2,
1.7 V≤VDD≤3.6 V
TPCLK −2
TPCLK
TPCLK+2
tsu(NSS)
NSS setup time
Slave mode, SPI presc = 2
4TPCLK
-
-
th(NSS)
NSS hold time
Slave mode, SPI presc = 2
2TPCLK
tsu(MI)
Data input setup time
Master mode
-
-
tsu(SI)
Slave mode
-
-
th(MI)
Data input hold time
Master mode
0.5
-
-
th(SI)
Slave mode
-
-
ta(SO)
Data output access time Slave mode, SPI presc = 2
-
4TPCLK
tdis(SO)
Data output disable time
Slave mode, SPI1/4/5/6,
2.7 V≤VDD≤3.6 V
-
8.5
Slave mode, SPI1/2/3/4/5/6 and
1.7 V≤VDD≤3.6 V
-
16.5
tv(SO)
th(SO)
Data output valid/hold
time
Slave mode (after enable edge),
SPI1/4/5/6 and 2.7V ≤ VDD ≤ 3.6V
-
ns
Slave mode (after enable edge),
SPI2/3, 2.7 V≤VDD≤3.6 V
-
Slave mode (after enable edge),
SPI1/4/5/6, 1.7 V≤VDD≤3.6 V
-
15.5
Slave mode (after enable edge),
SPI2/3, 1.7 V≤VDD≤3.6 V
-
15.5
17.5
tv(MO)
Data output valid time
Master mode (after enable edge),
SPI1/4/5/6, 2.7 V≤VDD≤3.6 V
-
-
2.5
Master mode (after enable edge),
SPI1/2/3/4/5/6, 1.7 V≤VDD≤3.6 V
-
-
4.5
th(MO)
Data output hold time
Master mode (after enable edge)
-
-
1.
Evaluated by characterization.
2.
The maximum frequency in Slave transmitter mode is determined by the sum of tv(SO) and tsu(MI), which has to fit into SCK
low or high phase preceding the SCK sampling edge. This value can be achieved when the SPI communicates with a
master having tsu(MI) = 0 while Duty(SCK) = 50%.
Table 63. SPI dynamic characteristics(1) (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit

---

Electrical characteristics
Figure 38. SPI timing diagram - slave mode and CPHA = 0
Figure 39. SPI timing diagram - slave mode and CPHA = 1
MSv41658V2
NSS input
CPHA=0
CPOL=0
SCK input
CPHA=0
CPOL=1
MISO output
MOSI input
tsu(SI)
th(SI)
tw(SCKL)
tw(SCKH)
tc(SCK)
th(NSS)
tdis(SO)
tsu(NSS)
ta(SO)
tv(SO)
Next bits IN
Last bit OUT
First bit IN
First bit OUT
Next bits OUT
th(SO)
Last bit IN
MSv41659V2
th(SI)
tc(SCK)
tw(SCKH)
tw(SCKL)
tsu(NSS)
ta(SO)
tv(SO)
th(NSS)
tdis(SO)
tsu(SI)
NSS input
CPHA=1
CPOL=0
CPHA=1
CPOL=1
MISO output
MOSI input
SCK input
First bit OUT
Next bits OUT
Last bit OUT
First bit IN
Last bit IN
Next bits IN
th(SO)

---

Electrical characteristics
Figure 40. SPI timing diagram - master mode
I2S interface characteristics
Unless otherwise specified, the parameters given in Table 64 for the I2S interface are
derived from tests performed under the ambient temperature, fPCLKx frequency, and VDD
supply voltage conditions summarized in Table 17, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load C = 30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
Refer to Section 6.3.17: I/O port characteristics for more details on the input/output alternate
function characteristics (CK, SD, WS).
MSv72626V1
th(MI)
tc(SCK)
tw(SCKH)
tw(SCKL)
tsu(MI)
NSS input
CPHA=0
CPOL=0
CPHA=0
CPOL=1
MOSI output
MISO input
SCK output
First bit OUT
Last bit OUT
Next bits OUT
First bit IN
Last bit IN
Next bits IN
CPHA=1
CPOL=0
CPHA=1
CPOL=1
SCK output
High
tv(MO)
th(MO)
Table 64. I2S dynamic characteristics(1)
Symbol
Parameter
Conditions
Min
Max
Unit
fMCK
I2S Main clock output
-
256x8K
256xFs(2)
MHz
fCK
I2S clock frequency
Master data: 32 bits
-
64xFs
MHz
Slave data: 32 bits
-
64xFs
DCK
I2S clock frequency duty cycle Slave receiver
%

---

Electrical characteristics
Note:
Refer to the I2S section of the RM0090 reference manual for more details on the sampling
frequency (FS).
fMCK, fCK, and DCK values reflect only the digital peripheral behavior. The values of these
parameters might be slightly impacted by the source clock precision. DCK depends mainly
on the value of ODD bit. The digital contribution leads to a minimum value of
(I2SDIV/(2*I2SDIV+ODD) and a maximum value of (I2SDIV+ODD)/(2*I2SDIV+ODD). FS
maximum value is supported for each mode/condition.
tv(WS)
WS valid time
Master mode
ns
th(WS)
WS hold time
Master mode
-
tsu(WS)
WS setup time
Slave mode
-
th(WS)
WS hold time
Slave mode
-
tsu(SD_MR)
Data input setup time
Master receiver
7.5
-
tsu(SD_SR)
Slave receiver
-
th(SD_MR)
Data input hold time
Master receiver
-
th(SD_SR)
Slave receiver
-
tv(SD_ST)
th(SD_ST)
Data output valid time
Slave transmitter (after enable edge)
-
tv(SD_MT)
Master transmitter (after enable edge)
-
th(SD_MT)
Data output hold time
Master transmitter (after enable edge)
2.5
-
1.
Evaluated by characterization.
2.
The maximum value of 256xFs is 45 MHz (APB1 maximum frequency).
Table 64. I2S dynamic characteristics(1) (continued)
Symbol
Parameter
Conditions
Min
Max
Unit

---

Electrical characteristics
Figure 41. I2S slave timing diagram (Philips protocol)(1)
1.
.LSB transmit/receive of the previously transmitted byte. No LSB transmit/receive is sent before the first
byte.
Figure 42. I2S master timing diagram (Philips protocol)(1)
1.
LSB transmit/receive of the previously transmitted byte. No LSB transmit/receive is sent before the first
byte.

---

Electrical characteristics
SAI characteristics
Unless otherwise specified, the parameters given in Table 65 for SAI are derived from tests
performed under the ambient temperature, fPCLKx frequency, and VDD supply voltage
conditions summarized in Table 17, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load C=30 pF
•
Measurement points are performed at CMOS levels: 0.5VDD
Refer to Section 6.3.17: I/O port characteristics for more details on the input/output alternate
function characteristics (SCK,SD,WS).
Table 65. SAI characteristics(1)
Symbol
Parameter
Conditions
Min
Max
Unit
fMCKL
SAI Main clock output
 -
256 x 8K
256xFs(2)
MHz
FSCK
SAI clock frequency
Master data: 32 bits
-
64xFs
MHz
Slave  data: 32 bits
-
64xFs
DSCK
SAI clock frequency duty
cycle
Slave receiver
%
tv(FS)
FS valid time
Master mode
ns
tsu(FS)
FS setup time
Slave mode
-
th(FS)
FS hold time
Master mode
-
Slave mode
-
tsu(SD_MR)
Data input setup time
Master receiver
-
tsu(SD_SR)
Slave receiver
-
th(SD_MR)
Data input hold time
Master receiver
-
th(SD_SR)
Slave receiver
-
tv(SD_ST)
th(SD_ST)
Data output valid time
Slave transmitter (after enable
edge)
-
tv(SD_MT)
Master transmitter (after enable
edge)
-
th(SD_MT)
Data output hold time
Master transmitter (after enable
edge)
-
1.
Evaluated by characterization.
2.
256xFs maximum corresponds to 45 MHz (APB2 maximum frequency)

---

Electrical characteristics
Figure 43. SAI master timing waveforms
Figure 44. SAI slave timing waveforms
MS32771V1
SAI_SCK_X
SAI_FS_X
(output)
1/fSCK
SAI_SD_X
(transmit)
tv(FS)
Slot n
SAI_SD_X
(receive)
th(FS)
Slot n+2
tv(SD_MT)
th(SD_MT)
Slot n
tsu(SD_MR)
th(SD_MR)
MS32772V1
SAI_SCK_X
SAI_FS_X
(input)
SAI_SD_X
(transmit)
tsu(FS)
Slot n
SAI_SD_X
(receive)
tw(CKH_X)
th(FS)
Slot n+2
tv(SD_ST)
th(SD_ST)
Slot n
tsu(SD_SR)
tw(CKL_X)
th(SD_SR)
1/fSCK

---

Electrical characteristics
USB OTG full speed (FS) characteristics
This interface is present in both the USB OTG HS and USB OTG FS controllers.
Note:
When the VBUS sensing feature is enabled, PA9 and PB13 should be left at their default
state (floating input), not as alternate function. A typical 200 µA current consumption of the
sensing block (current-to-voltage conversion to determine the different sessions) can be
observed on PA9 and PB13 when the feature is enabled.
Table 66. USB OTG full speed startup time
Symbol
Parameter
 Max
 Unit
tSTARTUP
(1)
1.
Specified by design.
USB OTG full speed transceiver startup time
µs
Table 67. USB OTG full speed DC electrical characteristics
Symbol
Parameter
Conditions
Min.(1)
1.
All the voltages are measured from the local ground potential.
Typ. Max.(1) Unit
Input
levels
VDD
USB OTG full speed
transceiver operating
voltage
3.0(2)
2.
The USB OTG full speed transceiver functionality is ensured down to 2.7 V but not the full USB full speed
electrical characteristics, which are degraded in the 2.7-to-3.0 V VDD voltage range.
-
3.6
V
VDI
(3)
3.
Specified by design.
Differential input sensitivity
I(USB_FS_DP/DM,
USB_HS_DP/DM)
0.2
-
-
V
VCM
(3) Differential common mode
range
Includes VDI range
0.8
-
2.5
VSE
(3)
Single ended receiver
threshold
1.3
-
2.0
Output
levels
VOL
Static output level low
RL of 1.5 kΩ to 3.6 V(4)
4.
RL is the load connected on the USB OTG full speed drivers.
-
-
0.3
V
VOH
Static output level high
RL of 15 kΩ to VSS
(4)
2.8
-
3.6
RPD
PA11, PA12, PB14, PB15
(USB_FS_DP/DM,
USB_HS_DP/DM)
VIN = VDD
kΩ
PA9, PB13
(OTG_FS_VBUS,
OTG_HS_VBUS)
0.65
1.1
2.0
RPU
PA12, PB15 (USB_FS_DP,
USB_HS_DP)
VIN = VSS
1.5
1.8
2.1
PA9, PB13
(OTG_FS_VBUS,
OTG_HS_VBUS)
VIN = VSS
0.25
0.37
0.55

---

Electrical characteristics
Figure 45. USB OTG full speed timings: definition of data signal rise and fall time
USB high speed (HS) characteristics
Unless otherwise specified, the parameters given in Table 71 for ULPI are derived from
tests performed under the ambient temperature, fHCLK frequency summarized in Table 70
and VDD supply voltage conditions summarized in Table 69, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10, unless otherwise specified
•
Capacitive load C = 30 pF, unless otherwise specified
•
Measurement points are done at CMOS levels: 0.5VDD.
Refer to Section 6.3.17: I/O port characteristics for more details on the input/output
characteristics.
Table 68. USB OTG full speed electrical characteristics(1)
1.
Specified by design.
Driver characteristics
Symbol
Parameter
Conditions
Min
Max
Unit
tr
Rise time(2)
2.
Measured from 10% to 90% of the data signal. For more detailed information, refer to USB Specification -
Chapter 7 (version 2.0).
CL = 50 pF
ns
tf
Fall time(2)
CL = 50 pF
ns
trfm
Rise/ fall time matching
tr/tf
%
VCRS
Output signal crossover voltage
-
1.3
2.0
V
ZDRV
Output driver impedance(3)
3.
No external termination series resistors are required on DP (D+) and DM (D-) pins since the matching
impedance is included in the embedded driver.
Driving high or
low
Ω
Table 69. USB HS DC electrical characteristics
Symbol
Parameter
Min.(1)
1.
All the voltages are measured from the local ground potential.
Max.(1)
Unit
Input level
VDD
USB OTG HS operating voltage
1.7
3.6
V
ai14137b
Cross over
points
Differential
data lines
VCRS
VSS
tf
tr

---

Electrical characteristics
Figure 46. ULPI timing diagram
Table 70. USB HS clock timing parameters(1)
1.
Specified by design.
Symbol
Parameter
Min
Typ
Max
Unit
fHCLK value to guarantee proper operation of
USB HS interface
-
-
MHz
FSTART_8BIT
Frequency (first transition)
8-bit ±10%
MHz
FSTEADY
Frequency (steady state) ±500 ppm
59.97
60.03
MHz
DSTART_8BIT
Duty cycle (first transition)
8-bit ±10%
%
DSTEADY
Duty cycle (steady state) ±500 ppm
49.975
50.025
%
tSTEADY
Time to reach the steady state frequency and
duty cycle after the first transition
-
-
1.4
ms
tSTART_DEV
Clock startup time after the
de-assertion of SuspendM
Peripheral
-
-
5.6
ms
tSTART_HOST
Host
-
-
-
tPREP
PHY preparation time after the first transition
of the input clock
-
-
-
µs
Clock
Control In
(ULPI_DIR,
ULPI_NXT)
data In
(8-bit)
Control out
(ULPI_STP)
data out
(8-bit)
tDD
tDC
tHD
tSD
tHC
tSC
ai17361c
tDC

---

Electrical characteristics
Table 71. Dynamic characteristics: USB ULPI(1)
Symbol
Parameter
Conditions
Min.
Typ.
Max.
Unit
tSC
Control in (ULPI_DIR, ULPI_NXT) setup time
-
-
-
ns
tHC
Control in (ULPI_DIR, ULPI_NXT) hold time
-
0.5
-
-
tSD
Data in setup time
-
1.5
-
-
tHD
Data in hold time
-
-
-
tDC/tDD
Data/control output delay
2.7 V < VDD < 3.6 V,
CL = 15 pF and
OSPEEDRy[1:0] = 11
-
9.5
2.7 V < VDD < 3.6 V,
CL = 20 pF and
OSPEEDRy[1:0] = 10
-
1.7 V < VDD < 3.6 V,
CL = 15 pF and
OSPEEDRy[1:0] = 11
-
1.
Specified by characterization.

---

Electrical characteristics
Ethernet characteristics
Unless otherwise specified, the parameters given in Table 72, Table 73 and Table 74 for
SMI, RMII and MII are derived from tests performed under the ambient temperature, fHCLK
frequency summarized in Table 17 with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load C = 30 pF for 2.7 V < VDD < 3.6 V
•
Capacitive load C = 20 pF for 1.71 V < VDD < 3.6 V
•
Measurement points are done at CMOS levels: 0.5VDD.
Refer to Section 6.3.17: I/O port characteristics for more details on the input/output
characteristics.
Table 72 gives the list of Ethernet MAC signals for the SMI (station management interface)
and Figure 47 shows the corresponding timing diagram.
Figure 47. Ethernet SMI timing diagram
Table 72. Dynamics characteristics: Ethernet MAC signals for SMI(1)
1.
Evaluated by characterization.
Symbol
Parameter
Min
Typ
Max
Unit
tMDC
MDC cycle time(2.38 MHz)
ns
Td(MDIO)
Write data valid time
tsu(MDIO)
Read data setup time
-
-
th(MDIO)
Read data hold time
-
-
MS31384V1
ETH_MDC
ETH_MDIO(O)
ETH_MDIO(I)
tMDC
td(MDIO)
tsu(MDIO)
th(MDIO)

---

Electrical characteristics
Table 73 gives the list of Ethernet MAC signals for the RMII and Figure 48 shows the
corresponding timing diagram.
Figure 48. Ethernet RMII timing diagram
Table 74 gives the list of Ethernet MAC signals for MII and Figure 48 shows the
corresponding timing diagram.
ai15667b
RMII_REF_CLK
RMII_TX_EN
RMII_TXD[1:0]
RMII_RXD[1:0]
RMII_CRS_DV
td(TXEN)
td(TXD)
tsu(RXD)
tsu(CRS)
tih(RXD)
tih(CRS)
Table 73. Dynamics characteristics: Ethernet MAC signals for RMII(1)
Symbol
Parameter
Condition
Min
Typ
Max
Unit
tsu(RXD)
Receive data setup time
1.71 V < VDD < 3.6 V
1.5
-
-
ns
tih(RXD)
Receive data hold time
-
-
tsu(CRS)
Carrier sense setup time
-
-
tih(CRS)
Carrier sense hold time
-
-
td(TXEN)
Transmit enable valid delay
time
2.7 V < VDD < 3.6 V
10.5
1.71 V < VDD < 3.6 V
10.5
td(TXD)
Transmit data valid delay time
2.7 V < VDD < 3.6 V
12.5
1.71 V < VDD < 3.6 V
14.5
1.
Evaluated by characterization results.

---

Electrical characteristics
Figure 49. Ethernet MII timing diagram
CAN (controller area network) interface
Refer to Section 6.3.17: I/O port characteristics for more details on the input/output alternate
function characteristics (CANx_TX and CANx_RX).
ai15668b
MII_RX_CLK
MII_RXD[3:0]
MII_RX_DV
MII_RX_ER
td(TXEN)
td(TXD)
tsu(RXD)
tsu(ER)
tsu(DV)
tih(RXD)
tih(ER)
tih(DV)
MII_TX_CLK
MII_TX_EN
MII_TXD[3:0]
Table 74. Dynamics characteristics: Ethernet MAC signals for MII(1)
Symbol
Parameter
Condition
Min
Typ
Max
Unit
tsu(RXD)
Receive data setup time
1.71 V < VDD < 3.6 V
-
-
ns
tih(RXD)
Receive data hold time
-
-
tsu(DV)
Data valid setup time
-
-
tih(DV)
Data valid hold time
-
-
tsu(ER)
Error setup time
-
-
tih(ER)
Error hold time
-
-
td(TXEN)
Transmit enable valid delay time
2.7 V < VDD < 3.6 V
1.71 V < VDD < 3.6 V
td(TXD)
Transmit data valid delay time
2.7 V < VDD < 3.6 V
7.5
1.71 V < VDD < 3.6 V
7.5
1.
Evaluated by characterization.

---

Electrical characteristics
6.3.21
12-bit ADC characteristics
Unless otherwise specified, the parameters given in Table 75 are derived from tests
performed under the ambient temperature, fPCLK2 frequency and VDDA supply voltage
conditions summarized in Table 17.
Table 75. ADC characteristics
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit
VDDA
Power supply
 VDDA − VREF+ < 1.2 V
1.7(1)
-
3.6
V
VREF+
Positive reference voltage
1.7(1)
-
VDDA
VREF-
Negative reference voltage
-
-
-
fADC
ADC clock frequency
VDDA = 1.7(1) to 2.4 V
0.6
MHz
VDDA = 2.4 to 3.6 V
0.6
MHz
fTRIG
(2)
External trigger frequency
fADC = 30 MHz,
12-bit resolution
-
-
kHz
-
-
-
1/fADC
VAIN
Conversion voltage range(3)
-
(VSSA or VREF-
tied to ground)
-
VREF+
V
RAIN
(2)
External input impedance
See Equation 1 for
details
-
-
kΩ
RADC
(2)(4) Sampling switch resistance
-
1.5
-
kΩ
CADC
(2)
Internal sample and hold
capacitor
-
-
pF
tlat
(2)
Injection trigger conversion
latency
fADC = 30 MHz
-
-
0.100
µs
-
-
-
3(5)
1/fADC
tlatr
(2)
Regular trigger conversion
latency
fADC = 30 MHz
-
-
0.067
µs
-
-
-
2(5)
1/fADC
tS
(2)
Sampling time
fADC = 30 MHz
0.100
-
µs
-
-
1/fADC
tSTAB
(2)
Power-up time
-
-
µs
tCONV
(2)
Total conversion time (including
sampling time)
fADC = 30 MHz
12-bit resolution
0.50
-
16.40
µs
fADC = 30 MHz
10-bit resolution
0.43
-
16.34
µs
fADC = 30 MHz
8-bit resolution
0.37
-
16.27
µs
fADC = 30 MHz
6-bit resolution
0.30
-
16.20
µs
9 to 492 (tS for sampling +n-bit resolution for successive
approximation)
1/fADC

---

Electrical characteristics
Equation 1: RAIN max formula
The formula above (Equation 1) is used to determine the maximum external impedance
allowed for an error below 1/4 of LSB. N = 12 (from 12-bit resolution) and k is the number of
sampling periods defined in the ADC_SMPR1 register.
fS
(2)
Sampling rate
(fADC = 30 MHz, and
tS = 3 ADC cycles)
12-bit resolution
Single ADC
-
-
Msps
12-bit resolution
Interleave Dual ADC
mode
-
-
3.75
Msps
12-bit resolution
Interleave Triple ADC
mode
-
-
Msps
IVREF+
(2)
ADC VREF DC current
consumption in conversion
mode
-
-
µA
IVDDA
(2)
ADC VDDA DC current
consumption in conversion
mode
-
-
1.6
1.8
mA
1.
VDDA minimum value of 1.7 V is obtained with the use of an external power supply supervisor (refer to Section 3.17.2:
Internal reset OFF).
2.
Evaluated by characterization results.
3.
VREF+ is internally connected to VDDA and VREF- is internally connected to VSSA.
4.
RADC maximum value is given for VDD=1.7 V, and a minimum value for VDD=3.3 V.
5.
For external triggers, a delay of 1/fPCLK2 must be added to the latency specified in Table 75.
Table 75. ADC characteristics (continued)
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit
Table 76. ADC static accuracy at fADC = 18 MHz
Symbol
Parameter
Test conditions
Typ
Max(1)
1.
Evaluated by characterization - not tested in production results.
Unit
ET
Total unadjusted error
fADC =18 MHz
VDDA = 1.7 to 3.6 V
VREF = 1.7 to 3.6 V
VDDA − VREF < 1.2 V
±3
±4
LSB
EO
Offset error
±2
±3
EG
Gain error
±1
±3
ED
Differential linearity error
±1
±2
EL
Integral linearity error
±2
±3
RAIN
k
0.5
–
(
)
fADC
CADC
N
+
(
)
ln
×
×
RADC
–
=

---

Electrical characteristics
         a
Table 77. ADC static accuracy at fADC = 30 MHz
Symbol
Parameter
Test conditions
Typ
Max(1)
Unit
ET
Total unadjusted error
fADC = 30 MHz,
RAIN < 10 kΩ
VDDA = 2.4 to 3.6 V,
VREF = 1.7 to 3.6 V,
VDDA − VREF < 1.2 V
±2
±5
LSB
EO
Offset error
±1.5
±2.5
EG
Gain error
±1.5
±3
ED
Differential linearity error
±1
±2
EL
Integral linearity error
±1.5
±3
1.
Evaluated by characterization.
Table 78. ADC static accuracy at fADC = 36 MHz
Symbol
Parameter
Test conditions
Typ
Max(1)
Unit
ET
Total unadjusted error
fADC =36 MHz,
VDDA = 2.4 to 3.6 V,
VREF = 1.7 to 3.6 V
VDDA − VREF < 1.2 V
±4
±7
LSB
EO
Offset error
±2
±3
EG
Gain error
±3
±6
ED
Differential linearity error
±2
±3
EL
Integral linearity error
±3
±6
1.
Evaluated by characterization.
Table 79. ADC dynamic accuracy at fADC = 18 MHz - limited test conditions(1)
Symbol
Parameter
Test conditions
Min
Typ
Max
Unit
ENOB
Effective number of bits
fADC =18 MHz
VDDA = VREF+= 1.7 V
Input Frequency = 20 KHz
Temperature = 25 °C
10.3
10.4
-
bits
SINAD
Signal-to-noise and distortion ratio
64.2
-
dB
SNR
Signal-to-noise ratio
-
THD
Total harmonic distortion
−67
−72
-
1.
Evaluated by characterization.
Table 80. ADC dynamic accuracy at fADC = 36 MHz - limited test conditions(1)
Symbol
Parameter
Test conditions
Min
Typ
Max
Unit
ENOB
Effective number of bits
fADC =36 MHz
VDDA = VREF+ = 3.3 V
Input Frequency = 20 KHz
Temperature = 25 °C
10.6
10.8
-
bits
SINAD
Signal-to noise and distortion ratio
-
dB
SNR
Signal-to noise ratio
-
THD
Total harmonic distortion
−70
−72
-
1.
Evaluated by characterization.

---

Electrical characteristics
Note:
ADC accuracy vs. negative injection current: injecting a negative current on any analog
input pins should be avoided as this significantly reduces the accuracy of the conversion
being performed on another analog input. It is recommended to add a Schottky diode (pin to
ground) to analog pins, which may potentially inject negative currents.
Any positive injection current within the limits specified for IINJ(PIN) and ∑IINJ(PIN) in
Section 6.3.17 does not affect the ADC accuracy.
Figure 50. ADC accuracy characteristics
1.
See also Table 77.
2.
Example of an actual transfer curve.
3.
Ideal transfer curve.
4.
End-point correlation line.
5.
ET = Total Unadjusted Error: maximum deviation between the actual and the ideal transfer curves.
EO = Offset Error: deviation between the first actual transition and the first ideal one.
EG = Gain Error: deviation between the last ideal transition and the last actual one.
ED = Differential Linearity Error: maximum deviation between actual steps and the ideal one.
EL = Integral Linearity Error: maximum deviation between any actual transition and the end-point
correlation line.
ai14395c
EO
EG
1L SBIDEAL
4093 4094 4095 4096
(1)
(2)
ET
ED
EL
(3)
VDDA
VSSA
VREF+
(or              depending on package)]
VDDA
[1LSB IDEAL =

---

Electrical characteristics
Figure 51. Typical connection diagram when using the ADC with FT/TT pins
featuring the analog switch function
1.
Refer to Table 75: ADC characteristics for the values of RAIN, RADC, and CADC.
2.
Cparasitic represents the capacitance of the PCB (dependent on soldering and PCB layout quality) plus the
pad capacitance (refer to Table 57: I/O static characteristics). A high Cparasitic value downgrades
conversion accuracy. To remedy this, fADC should be reduced.
3.
Refer to Table 57: I/O static characteristics for the value of IIkg.
4.
Refer to Figure 22: Power supply scheme.
MSv67871V3
Sample-and-hold ADC converter
Converter
Cparasitic
(2)
Ilkg
(3)
CADC
VDDA
(4)
RAIN
(1)
VAIN
RADC
VREF+
(4)
VSSA
I/O
analog
switch
Sampling
switch with
multiplexing
VSS
VSS

---

Electrical characteristics
General PCB design guidelines
Power supply decoupling should be performed as shown in Figure 52 or Figure 53,
depending on whether VREF+ is connected to VDDA or not. The 10 nF capacitors should be
ceramic (good quality). They should be placed them as close as possible to the chip.
Figure 52. Power supply and reference decoupling (VREF+ not connected to VDDA)
1.
VREF+ and VREF– inputs are both available on UFBGA176. VREF+ is also available on LQFP100, LQFP144,
and LQFP176. When VREF+ and VREF– are not available, they are internally connected to VDDA and VSSA.
STM32F
1 μF // 10 nF
1 μF // 10 nF
VREF+ (1)
VDDA
VSSA/VREF- (1)
ai17535b

---

Electrical characteristics
Figure 53. Power supply and reference decoupling (VREF+ connected to VDDA)
1.
VREF+ and VREF– inputs are both available on UFBGA176. VREF+ is also available on LQFP100, LQFP144,
and LQFP176. When VREF+ and VREF– are not available, they are internally connected to VDDA and VSSA.
6.3.22
Temperature sensor characteristics
STM32F
1 μF // 10 nF
ai17536c
VREF+/VDDA
VREF-/VSSA (1)
(1)
Table 81. Temperature sensor characteristics
Symbol
Parameter
Min
Typ
Max
Unit
TL
(1)
VSENSE linearity with temperature
-
±1
±2
°C
Avg_Slope(1)
Average slope
-
2.5
-
mV/°C
V25
(1)
Voltage at 25 °C
-
0.76
-
V
tSTART
(2)
Startup time
-
µs
TS_temp
(2)
ADC sampling time when reading the temperature (1 °C accuracy)
-
-
µs
1.
Evaluated by characterization.
2.
Specified by design.
Table 82. Temperature sensor calibration values
Symbol
Parameter
Memory address
TS_CAL1
TS ADC raw data acquired at temperature of 30 °C, VDDA= 3.3 V
0x1FFF 7A2C - 0x1FFF 7A2D
TS_CAL2
TS ADC raw data acquired at temperature of 110 °C, VDDA= 3.3 V
0x1FFF 7A2E - 0x1FFF 7A2F

---

Electrical characteristics
6.3.23
VBAT monitoring characteristics
6.3.24
Reference voltage
The parameters given in Table 84 are derived from tests performed under ambient
temperature and VDD supply voltage conditions summarized in Table 17.
Table 83. VBAT monitoring characteristics
Symbol
Parameter
Min
Typ
Max
Unit
R
Resistor bridge for VBAT
-
-
KΩ
Q
Ratio on VBAT measurement
-
-
-
Er(1)
Error on Q
–1
-
+1
%
TS_vbat
(2)(2)
ADC sampling time when reading the VBAT
1 mV accuracy
-
-
µs
1.
Specified by design.
2.
The shortest sampling time can be determined in the application by multiple iterations.
Table 84.  internal reference voltage
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
VREFINT
Internal reference voltage
–40 °C < TA < +105 °C
1.18
1.21
1.24
V
TS_vrefint
(1)
ADC sampling time when reading the
internal reference voltage
-
-
-
µs
VRERINT_s
(2)
Internal reference voltage spread over the
temperature range
VDD = 3V ± 10mV
-
mV
TCoeff
(2)
Temperature coefficient
-
-
ppm/°C
tSTART
(2)
Startup time
-
-
µs
1.
The shortest sampling time can be determined in the application by multiple iterations.
2.
Specified by design, not tested in production
Table 85. Internal reference voltage calibration values
Symbol
Parameter
Memory address
VREFIN_CAL
Raw data acquired at temperature of 30 °C VDDA = 3.3 V
0x1FFF 7A2A - 0x1FFF 7A2B

---

Electrical characteristics
6.3.25
DAC electrical characteristics
Table 86. DAC characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Comments
VDDA
Analog supply voltage
-
1.7(1)
-
3.6
V
-
VREF+
Reference supply
voltage
-
1.7(1)
-
3.6
V
VREF+ ≤ VDDA
VSSA
Ground
-
-
V
-
RLOAD
(2) Resistive load
DAC output
buffer ON
RLOAD
connected
to VSSA
-
-
kΩ
-
RLOAD
connected
to VDDA
-
RO
(2)
Impedance output with
buffer OFF
-
-
-
kΩ
When the buffer is OFF, the
Minimum resistive load
between DAC_OUT and VSS
to have a 1% accuracy is
1.5 MΩ
CLOAD
(2) Capacitive load
-
-
-
pF
Maximum capacitive load at
DAC_OUT pin (when the
buffer is ON).
DAC_O
UT
min(2)
Lower DAC_OUT
voltage with buffer ON
-
0.2
-
-
V
It gives the maximum output
excursion of the DAC.
It corresponds to 12-bit input
code (0x0E0) to (0xF1C) at
VREF+ = 3.6 V and (0x1C7) to
(0xE38) at VREF+ = 1.7 V
DAC_O
UT
max(2)
Higher DAC_OUT
voltage with buffer ON
-
-
-
VDDA
− 0.2
V
DAC_O
UT
min(2)
Lower DAC_OUT
voltage with buffer
OFF
-
-
0.5
-
mV
It gives the maximum output
excursion of the DAC.
DAC_O
UT
max(2)
Higher DAC_OUT
voltage with buffer
OFF
-
-
-
VREF+
−
1LSB
V
IVREF+
(4)
DAC DC VREF current
consumption in
quiescent mode
(Standby mode)
-
-
µA
With no load, worst code
(0x800) at VREF+ = 3.6 V in
terms of DC consumption on
the inputs
-
-
With no load, worst code
(0xF1C) at VREF+ = 3.6 V in
terms of DC consumption on
the inputs

---

Electrical characteristics
IDDA
(4)
DAC DC VDDA
current consumption in
quiescent mode(3)
-
-
µA With no load, middle code
(0x800) on the inputs
-
-
µA
With no load, worst code
(0xF1C) at VREF+ = 3.6 V in
terms of DC consumption on
the inputs
DNL(4)
Differential non
linearity Difference
between two
consecutive code-
1LSB)
-
-
-
±0.5 LSB Given for the DAC in 10-bit
configuration.
-
-
-
±2
LSB Given for the DAC in 12-bit
configuration.
INL(4)
Integral non linearity
(difference between
measured value at
Code i and the value
at Code i on a line
drawn between Code
0 and last Code 1023)
-
-
-
±1
LSB Given for the DAC in 10-bit
configuration.
-
-
-
±4
LSB Given for the DAC in 12-bit
configuration.
Offset(4)
Offset error
(difference between
measured value at
Code (0x800) and the
ideal value = VREF+/2)
-
-
-
±10
mV Given for the DAC in 12-bit
configuration
-
-
-
±3
LSB Given for the DAC in 10-bit at
VREF+ = 3.6 V
-
-
-
±12
LSB Given for the DAC in 12-bit at
VREF+ = 3.6 V
Gain
error(4)
Gain error
-
-
-
±0.5
%
Given for the DAC in 12-bit
configuration
tSETTLIN
G
(4)
Settling time (full
scale: for a 10-bit input
code transition
between the lowest
and the highest input
codes when
DAC_OUT reaches
final value ±4LSB
-
-
µs
CLOAD ≤ 50 pF,
RLOAD ≥ 5 kΩ
THD(4)
Total Harmonic
Distortion
Buffer ON
-
-
-
-
dB CLOAD ≤ 50 pF,
RLOAD ≥ 5 kΩ
Update
rate(2)
Max frequency for a
correct DAC_OUT
change when small
variation in the input
code (from code i to
i+1LSB)
-
-
-
MS/
s
CLOAD ≤ 50 pF,
RLOAD ≥ 5 kΩ
Table 86. DAC characteristics (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Comments

---

Electrical characteristics
Figure 54. 12-bit buffered /non-buffered DAC
1.
The DAC integrates an output buffer that can be used to reduce the output impedance and to drive external loads directly
without the use of an external operational amplifier. The buffer can be bypassed by configuring the BOFFx bit in the
DAC_CR register.
tWAKEUP
(
4)
Wakeup time from off
state (Setting the ENx
bit in the DAC Control
register)
-
-
6.5
µs
CLOAD ≤ 50 pF, RLOAD ≥ 5 kΩ
input code between lowest and
highest possible ones.
PSRR+
(2)
Power supply rejection
ratio (to VDDA) (static
DC measurement)
-
-
–67
–40
dB No RLOAD, CLOAD = 50 pF
1.
VDDA minimum value of 1.7 V is obtained with the use of an external power supply supervisor (refer to Section 3.17.2:
Internal reset OFF).
2.
Specified by design.
3.
The quiescent mode corresponds to a state where the DAC maintains a stable output level to ensure that no dynamic
consumption occurs.
4.
Evaluated by characterization - not tested in production.
Table 86. DAC characteristics (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Comments
ai17157a
(1)
Buffer
12-bit
digital to
analog
converter
Buffered/non-buffered DAC
DACx_OUT
RLOAD
CLOAD

---

Electrical characteristics
6.3.26
FMC characteristics
Unless otherwise specified, the parameters given in Table 87 to Table 102 for the FMC
interface are derived from tests performed under the ambient temperature, fHCLK frequency,
and VDD supply voltage conditions summarized in Table 17, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10 except at VDD range 1.7 to 2.1 V where
OSPEEDRy[1:0] = 11
•
Measurement points are done at CMOS levels: 0.5VDD
Refer to Section 6.3.17: I/O port characteristics for more details on the input/output
characteristics.
Asynchronous waveforms and timings
Figure 55 through Figure 58 represent asynchronous waveforms and Table 87 through
Table 94 provide the corresponding timings. The results shown in these tables are obtained
with the following FMC configuration:
•
AddressSetupTime = 0x1
•
AddressHoldTime = 0x1
•
DataSetupTime = 0x1 (except for asynchronous NWAIT mode, DataSetupTime = 0x5)
•
BusTurnAroundDuration = 0x0
•
For SDRAM memories, VDD ranges from 2.7 to 3.6 V and maximum frequency
FMC_SDCLK = 90 MHz
•
For Mobile LPSDR SDRAM memories, VDD ranges from 1.7 to 1.95 V and maximum
frequency FMC_SDCLK = 84 MHz

---

Electrical characteristics
Figure 55. Asynchronous non-multiplexed SRAM/PSRAM/NOR read waveforms
1.
Mode 2/B, C, and D only. In Mode 1, FMC_NADV is not used.
Table 87. Asynchronous non-multiplexed SRAM/PSRAM/NOR -
read timings(1)(2)
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
2THCLK − 0.5
2 THCLK+0.5
ns
tv(NOE_NE)
FMC_NEx low to FMC_NOE low
ns
tw(NOE)
FMC_NOE low time
2THCLK
2THCLK+ 0.5
ns
th(NE_NOE)
FMC_NOE high to FMC_NE high hold time
-
ns
tv(A_NE)
FMC_NEx low to FMC_A valid
-
ns
th(A_NOE)
Address hold time after FMC_NOE high
-
ns
tv(BL_NE)
FMC_NEx low to FMC_BL valid
-
ns
th(BL_NOE)
FMC_BL hold time after FMC_NOE high
-
ns
tsu(Data_NE)
Data to FMC_NEx high setup time
THCLK + 2.5
-
ns
tsu(Data_NOE)
Data to FMC_NOEx high setup time
THCLK +2
-
ns
Data
FMC_NE
FMC_NBL[1:0]
FMC_D[15:0]
tv(BL_NE)
t h(Data_NE)
FMC_NOE
Address
FMC_A[25:0]
tv(A_NE)
FMC_NWE
tsu(Data_NE)
tw(NE)
MS32753V1
w(NOE)
t
tv(NOE_NE)
t h(NE_NOE)
th(Data_NOE)
t h(A_NOE)
t h(BL_NOE)
tsu(Data_NOE)
FMC_NADV
(1)
t v(NADV_NE)
tw(NADV)
FMC_NWAIT
tsu(NWAIT_NE)
th(NE_NWAIT)

---

Electrical characteristics
th(Data_NOE)
Data hold time after FMC_NOE high
-
ns
th(Data_NE)
Data hold time after FMC_NEx high
-
ns
tv(NADV_NE)
FMC_NEx low to FMC_NADV low
-
ns
tw(NADV)
FMC_NADV low time
-
THCLK +1
ns
1.
CL = 30 pF.
2.
Evaluated by characterization.
Table 88. Asynchronous non-multiplexed SRAM/PSRAM/NOR read -
NWAIT timings(1)(2)
1.
CL = 30 pF.
2.
Evaluated by characterization.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
7THCLK+0.5
7THCLK+1
ns
tw(NOE)
FMC_NWE low time
5THCLK −1.5
5THCLK +2
tsu(NWAIT_NE)
FMC_NWAIT valid before FMC_NEx high
5THCLK+1.5
-
th(NE_NWAIT)
FMC_NEx hold time after FMC_NWAIT
invalid
4THCLK+1
-
Table 87. Asynchronous non-multiplexed SRAM/PSRAM/NOR -
read timings(1)(2) (continued)
Symbol
Parameter
Min
Max
Unit

---

Electrical characteristics
Figure 56. Asynchronous non-multiplexed SRAM/PSRAM/NOR write waveforms
1.
Mode 2/B, C, and D only. In Mode 1, FMC_NADV is not used.
         Table 89. Asynchronous non-multiplexed SRAM/PSRAM/NOR write timings(1)(2)
1.
CL = 30 pF.
2.
Evaluated by characterization.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
3THCLK
3THCLK+1
ns
tv(NWE_NE)
FMC_NEx low to FMC_NWE low
THCLK −0.5
THCLK+ 0.5
ns
tw(NWE)
FMC_NWE low time
THCLK
THCLK+ 0.5
ns
th(NE_NWE)
FMC_NWE high to FMC_NE high hold time
THCLK +1.5
 -
ns
tv(A_NE)
FMC_NEx low to FMC_A valid
 -
ns
th(A_NWE)
Address hold time after FMC_NWE high
THCLK+0.5
 -
ns
tv(BL_NE)
FMC_NEx low to FMC_BL valid
 -
1.5
ns
th(BL_NWE)
FMC_BL hold time after FMC_NWE high
THCLK+0.5
 -
ns
tv(Data_NE)
Data to FMC_NEx low to Data valid
 -
THCLK+ 2
ns
th(Data_NWE)
Data hold time after FMC_NWE high
THCLK+0.5
 -
ns
tv(NADV_NE)
FMC_NEx low to FMC_NADV low
 -
0.5
ns
tw(NADV)
FMC_NADV low time
 -
THCLK+ 0.5
ns
NBL
Data
FMC_NEx
FMC_NBL[1:0]
FMC_D[15:0]
tv(BL_NE)
th(Data_NWE)
FMC_NOE
Address
FMC_A[25:0]
tv(A_NE)
tw(NWE)
FMC_NWE
tv(NWE_NE)
th(NE_NWE)
th(A_NWE)
th(BL_NWE)
tv(Data_NE)
tw(NE)
MS32754V1
FMC_NADV (1)
tv(NADV_NE)
tw(NADV)
FMC_NWAIT
tsu(NWAIT_NE)
th(NE_NWAIT)

---

Electrical characteristics
Figure 57. Asynchronous multiplexed PSRAM/NOR read waveforms
Table 90. Asynchronous non-multiplexed SRAM/PSRAM/NOR write -
NWAIT timings(1)(2)
1.
CL = 30 pF.
2.
Evaluated by characterization.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
8THCLK+1
 8THCLK+2
ns
tw(NWE)
FMC_NWE low time
6THCLK −1
6THCLK+2
ns
tsu(NWAIT_NE)
FMC_NWAIT valid before FMC_NEx high
6THCLK+1.5
-
ns
th(NE_NWAIT)
FMC_NEx hold time after FMC_NWAIT
invalid
4THCLK+1
-
ns
NBL
Data
FMC_ NBL[1:0]
FMC_ AD[15:0]
tv(BL_NE)
th(Data_NE)
Address
FMC_ A[25:16]
tv(A_NE)
FMC_NWE
tv(A_NE)
MS32755V1
Address
FMC_NADV
tv(NADV_NE)
tw(NADV)
tsu(Data_NE)
th(AD_NADV)
FMC_ NE
FMC_NOE
tw(NE)
tw(NOE)
tv(NOE_NE)
th(NE_NOE)
th(A_NOE)
th(BL_NOE)
tsu(Data_NOE)
th(Data_NOE)
FMC_NWAIT
tsu(NWAIT_NE)
th(NE_NWAIT)

---

Electrical characteristics
Table 91. Asynchronous multiplexed PSRAM/NOR read timings(1)(2)
1.
CL = 30 pF.
2.
Evaluated by characterization.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
3THCLK −1
3THCLK+0.5
ns
tv(NOE_NE)
FMC_NEx low to FMC_NOE low
2THCLK −0.5
2THCLK
ns
ttw(NOE)
FMC_NOE low time
THCLK −1
THCLK+1
ns
th(NE_NOE)
FMC_NOE high to FMC_NE high hold time
 -
ns
tv(A_NE)
FMC_NEx low to FMC_A valid
 -
ns
tv(NADV_NE)
FMC_NEx low to FMC_NADV low
ns
tw(NADV)
FMC_NADV low time
THCLK −0.5
THCLK+0.5
ns
th(AD_NADV)
FMC_AD(address) valid hold time after
FMC_NADV high)
 -
ns
th(A_NOE)
Address hold time after FMC_NOE high
THCLK −0.5
 -
ns
th(BL_NOE)
FMC_BL time after FMC_NOE high
 -
ns
tv(BL_NE)
FMC_NEx low to FMC_BL valid
-
ns
tsu(Data_NE)
Data to FMC_NEx high setup time
THCLK+1.5
 -
ns
tsu(Data_NOE)
Data to FMC_NOE high setup time
THCLK+1
 -
ns
th(Data_NE)
Data hold time after FMC_NEx high
-
ns
th(Data_NOE)
Data hold time after FMC_NOE high
-
ns
Table 92. Asynchronous multiplexed PSRAM/NOR read-NWAIT timings(1)(2)
1.
CL = 30 pF.
2.
Evaluated by characterization.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
8THCLK+0.5
8THCLK+2
ns
tw(NOE)
FMC_NWE low time
5THCLK −1
5THCLK +1.5
ns
tsu(NWAIT_NE)
FMC_NWAIT valid before FMC_NEx high
5THCLK +1.5
-
ns
th(NE_NWAIT)
FMC_NEx hold time after FMC_NWAIT
invalid
4THCLK+1
-
ns

---

Electrical characteristics
Figure 58. Asynchronous multiplexed PSRAM/NOR write waveforms
NBL
Data
FMC_ NEx
FMC_ NBL[1:0]
FMC_ AD[15:0]
tv(BL_NE)
th(Data_NWE)
FMC_NOE
Address
FMC_ A[25:16]
tv(A_NE)
tw(NWE)
FMC_NWE
tv(NWE_NE)
th(NE_NWE)
th(A_NWE)
th(BL_NWE)
tv(A_NE)
tw(NE)
MS32756V1
Address
FMC_NADV
tv(NADV_NE)
tw(NADV)
tv(Data_NADV)
th(AD_NADV)
FMC_NWAIT
tsu(NWAIT_NE)
th(NE_NWAIT)
Table 93. Asynchronous multiplexed PSRAM/NOR write timings(1)(2)
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
4THCLK
4THCLK+0.5
ns
tv(NWE_NE)
FMC_NEx low to FMC_NWE low
THCLK −1
THCLK+0.5
ns
tw(NWE)
FMC_NWE low time
 2THCLK
2THCLK+0.5
ns
th(NE_NWE)
FMC_NWE high to FMC_NE high hold time
THCLK
-
ns
tv(A_NE)
FMC_NEx low to FMC_A valid
-
ns
tv(NADV_NE)
FMC_NEx low to FMC_NADV low
0.5
ns
tw(NADV)
FMC_NADV low time
THCLK −0.5
THCLK+ 0.5
ns
th(AD_NADV)
FMC_AD(adress) valid hold time after FMC_NADV high)
THCLK −2
-
ns
th(A_NWE)
Address hold time after FMC_NWE high
 THCLK
-
ns
th(BL_NWE)
FMC_BL hold time after FMC_NWE high
THCLK −2
-
ns
tv(BL_NE)
FMC_NEx low to FMC_BL valid
-
ns
tv(Data_NADV)
FMC_NADV high to Data valid
-
 THCLK +1.5
ns
th(Data_NWE)
Data hold time after FMC_NWE high
 THCLK +0.5
-
ns
1.
CL = 30 pF.
2.
Evaluated by characterization.

---

Electrical characteristics
Synchronous waveforms and timings
Figure 59 through Figure 62 represent synchronous waveforms and Table 95 through
Table 98 provide the corresponding timings. The results shown in these tables are obtained
with the following FMC configuration:
•
BurstAccessMode = FMC_BurstAccessMode_Enable;
•
MemoryType = FMC_MemoryType_CRAM;
•
WriteBurst = FMC_WriteBurst_Enable;
•
CLKDivision = 1; (0 is not supported. See the STM32F4xx reference manual: RM0090)
•
DataLatency = 1 for NOR flash; DataLatency = 0 for PSRAM
In all timing tables, the THCLK is the HCLK clock period (with maximum
FMC_CLK = 90 MHz).
Table 94. Asynchronous multiplexed PSRAM/NOR write-NWAIT timings(1)(2)
1.
CL = 30 pF.
2.
Evaluated by characterization.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
9THCLK
 9THCLK+0.5
ns
tw(NWE)
FMC_NWE low time
7THCLK
7THCLK+2
ns
tsu(NWAIT_NE)
FMC_NWAIT valid before FMC_NEx high
6THCLK+1.5
-
ns
th(NE_NWAIT)
FMC_NEx hold time after FMC_NWAIT
invalid
4THCLK–1
-
ns

---

Electrical characteristics
Figure 59. Synchronous multiplexed NOR/PSRAM read timings
Table 95. Synchronous multiplexed NOR/PSRAM read timings(1)(2)
Symbol
Parameter
Min
Max
Unit
tw(CLK)
FMC_CLK period
2THCLK −1
 -
ns
td(CLKL-NExL)
FMC_CLK low to FMC_NEx low (x=0..2)
 -
ns
td(CLKH_NExH)
FMC_CLK high to FMC_NEx high (x= 0…2)
THCLK
 -
ns
td(CLKL-NADVL)
FMC_CLK low to FMC_NADV low
 -
ns
td(CLKL-NADVH)
FMC_CLK low to FMC_NADV high
 -
ns
td(CLKL-AV)
FMC_CLK low to FMC_Ax valid (x=16…25)
 -
ns
td(CLKH-AIV)
FMC_CLK high to FMC_Ax invalid (x=16…25)
 -
ns
td(CLKL-NOEL)
FMC_CLK low to FMC_NOE low
 -
THCLK+0.5
ns
td(CLKH-NOEH)
FMC_CLK high to FMC_NOE high
THCLK−0.5
 -
ns
td(CLKL-ADV)
FMC_CLK low to FMC_AD[15:0] valid
 -
0.5
ns
td(CLKL-ADIV)
FMC_CLK low to FMC_AD[15:0] invalid
 -
ns
FMC_CLK
FMC_NEx
FMC_NADV
FMC_A[25:16]
FMC_NOE
FMC_AD[15:0]
AD[15:0]
D1
D2
FMC_NWAIT
(WAITCFG = 1b,
WAITPOL + 0b)
FMC_NWAIT
(WAITCFG = 0b,
WAITPOL + 0b)
tw(CLK)
tw(CLK)
Data latency = 0
BUSTURN = 0
td(CLKL-NExL)
td(CLKH-NExH)
td(CLKL-NADVL)
td(CLKL-AV)
td(CLKL-NADVH)
td(CLKH-AIV)
td(CLKL-NOEL)
td(CLKH-NOEH)
td(CLKL-ADV)
td(CLKL-ADIV)
tsu(ADV-CLKH)
th(CLKH-ADV)
tsu(ADV-CLKH)
th(CLKH-ADV)
tsu(NWAITV-CLKH)
th(CLKH-NWAITV)
tsu(NWAITV-CLKH)
th(CLKH-NWAITV)
tsu(NWAITV-CLKH)
th(CLKH-NWAITV)
MS32757V1

---

Electrical characteristics
Figure 60. Synchronous multiplexed PSRAM write timings
tsu(ADV-CLKH)
FMC_A/D[15:0] valid data before FMC_CLK
high
 -
ns
th(CLKH-ADV)
FMC_A/D[15:0] valid data after FMC_CLK high
 -
ns
tsu(NWAIT-CLKH)
FMC_NWAIT valid before FMC_CLK high
-
ns
th(CLKH-NWAIT)
FMC_NWAIT valid after FMC_CLK high
-
ns
1.
CL = 30 pF.
2.
Evaluated by characterization.
Table 95. Synchronous multiplexed NOR/PSRAM read timings(1)(2) (continued)
Symbol
Parameter
Min
Max
Unit
FMC_CLK
FMC_NEx
FMC_NADV
FMC_A[25:16]
FMC_NWE
FMC_AD[15:0]
AD[15:0]
D1
D2
FMC_NWAIT
(WAITCFG = 0b,
 WAITPOL + 0b)
tw(CLK)
tw(CLK)
Data latency = 0
BUSTURN = 0
td(CLKL-NExL)
td(CLKH-NExH)
td(CLKL-NADVL)
td(CLKL-AV)
td(CLKL-NADVH)
td(CLKH-AIV)
td(CLKH-NWEH)
td(CLKL-NWEL)
td(CLKH-NBLH)
td(CLKL-ADV)
td(CLKL-ADIV)
td(CLKL-Data)
tsu(NWAITV-CLKH)
th(CLKH-NWAITV)
MS32758V1
td(CLKL-Data)
FMC_NBL

---

Electrical characteristics
Table 96. Synchronous multiplexed PSRAM write timings(1)(2)
1.
CL = 30 pF.
2.
Evaluated by characterization.
Symbol
Parameter
Min
Max
Unit
tw(CLK)
FMC_CLK period,  VDD range= 2.7 to 3.6 V
2THCLK −1
 -
ns
td(CLKL-NExL)
FMC_CLK low to FMC_NEx low (x=0..2)
 -
1.5
ns
td(CLKH-NExH)
FMC_CLK high to FMC_NEx high (x= 0…2)
THCLK
 -
ns
td(CLKL-NADVL)
FMC_CLK low to FMC_NADV low
 -
ns
td(CLKL-NADVH)
FMC_CLK low to FMC_NADV high
 -
ns
td(CLKL-AV)
FMC_CLK low to FMC_Ax valid (x=16…25)
 -
ns
td(CLKH-AIV)
FMC_CLK high to FMC_Ax invalid (x=16…25)
THCLK
 -
ns
td(CLKL-NWEL)
FMC_CLK low to FMC_NWE low
 -
ns
t(CLKH-NWEH)
FMC_CLK high to FMC_NWE high
THCLK−0.5
 -
ns
td(CLKL-ADV)
FMC_CLK low to FMC_AD[15:0] valid
-
ns
td(CLKL-ADIV)
FMC_CLK low to FMC_AD[15:0] invalid
 -
ns
td(CLKL-DATA)
FMC_A/D[15:0] valid data after FMC_CLK low
 -
ns
td(CLKL-NBLL)
FMC_CLK low to FMC_NBL low
 -
ns
td(CLKH-NBLH)
FMC_CLK high to FMC_NBL high
THCLK−0.5
-
ns
tsu(NWAIT-CLKH) FMC_NWAIT valid before FMC_CLK high
-
ns
th(CLKH-NWAIT)
FMC_NWAIT valid after FMC_CLK high
-
ns

---

Electrical characteristics
Figure 61. Synchronous non-multiplexed NOR/PSRAM read timings
Table 97. Synchronous non-multiplexed NOR/PSRAM read timings(1)(2)
Symbol
Parameter
Min
Max
Unit
tw(CLK)
FMC_CLK period
2THCLK −1
 -
ns
t(CLKL-NExL)
FMC_CLK low to FMC_NEx low (x=0..2)
 -
0.5
ns
td(CLKH-
NExH)
FMC_CLK high to FMC_NEx high (x= 0…2)
THCLK
 -
ns
td(CLKL-
NADVL)
FMC_CLK low to FMC_NADV low
 -
ns
td(CLKL-
NADVH)
FMC_CLK low to FMC_NADV high
 -
ns
td(CLKL-AV)
FMC_CLK low to FMC_Ax valid (x=16…25)
 -
ns
td(CLKH-AIV)
FMC_CLK high to FMC_Ax invalid (x=16…25)
THCLK −0.5
 -
ns
td(CLKL-NOEL) FMC_CLK low to FMC_NOE low
 -
THCLK+2
ns
td(CLKH-
NOEH)
FMC_CLK high to FMC_NOE high
THCLK −0.5
 -
ns
tsu(DV-CLKH)
FMC_D[15:0] valid data before FMC_CLK high
 -
ns
FMC_CLK
FMC_NEx
FMC_A[25:0]
FMC_NOE
FMC_D[15:0]
D1
D2
FMC_NWAIT
(WAITCFG = 1b,
WAITPOL + 0b)
FMC_NWAIT
(WAITCFG = 0b,
WAITPOL + 0b)
tw(CLK)
tw(CLK)
Data latency = 0
td(CLKL-NExL)
td(CLKH-NExH)
td(CLKL-AV)
td(CLKH-AIV)
td(CLKL-NOEL)
td(CLKH-NOEH)
tsu(DV-CLKH)
th(CLKH-DV)
tsu(DV-CLKH)
th(CLKH-DV)
tsu(NWAITV-CLKH)
th(CLKH-NWAITV)
tsu(NWAITV-CLKH)
t h(CLKH-NWAITV)
tsu(NWAITV-CLKH)
th(CLKH-NWAITV)
MS32759V1
FMC_NADV
td(CLKL-NADVL)
td(CLKL-NADVH)

---

Electrical characteristics
Figure 62. Synchronous non-multiplexed PSRAM write timings
th(CLKH-DV)
FMC_D[15:0] valid data after FMC_CLK high
 -
ns
t(NWAIT-CLKH) FMC_NWAIT valid before FMC_CLK high
-
-
th(CLKH-
NWAIT)
FMC_NWAIT valid after FMC_CLK high
-
-
1.
CL = 30 pF.
2.
Guaranteed by characterization results.
Table 98. Synchronous non-multiplexed PSRAM write timings(1)(2)
Symbol
Parameter
Min
Max
Unit
t(CLK)
FMC_CLK period
2THCLK −1
 -
ns
td(CLKL-NExL)
FMC_CLK low to FMC_NEx low (x=0..2)
 -
0.5
ns
t(CLKH-NExH)
FMC_CLK high to FMC_NEx high (x= 0…2)
THCLK
 -
ns
td(CLKL-NADVL)
FMC_CLK low to FMC_NADV low
 -
ns
td(CLKL-NADVH)
FMC_CLK low to FMC_NADV high
 -
ns
td(CLKL-AV)
FMC_CLK low to FMC_Ax valid (x=16…25)
 -
ns
Table 97. Synchronous non-multiplexed NOR/PSRAM read timings(1)(2) (continued)
Symbol
Parameter
Min
Max
Unit
MS32760V1
FMC_CLK
FMC_NEx
FMC_A[25:0]
FMC_NWE
FMC_D[15:0]
D1
D2
FMC_NWAIT
(WAITCFG = 0b, WAITPOL + 0b)
tw(CLK)
tw(CLK)
Data latency = 0
td(CLKL-NExL)
td(CLKH-NExH)
td(CLKL-AV)
td(CLKH-AIV)
td(CLKH-NWEH)
td(CLKL-NWEL)
td(CLKL-Data)
tsu(NWAITV-CLKH)
th(CLKH-NWAITV)
FMC_NADV
td(CLKL-NADVL)
td(CLKL-NADVH)
td(CLKL-Data)
FMC_NBL
td(CLKH-NBLH)

---

Electrical characteristics
PC Card/CompactFlash controller waveforms and timings
Figure 63 through Figure 68 represent synchronous waveforms, and Table 99 and
Table 100 provide the corresponding timings. The results shown in this table are obtained
with the following FMC configuration:
•
COM.FMC_SetupTime = 0x04;
•
COM.FMC_WaitSetupTime = 0x07;
•
COM.FMC_HoldSetupTime = 0x04;
•
COM.FMC_HiZSetupTime = 0x00;
•
ATT.FMC_SetupTime = 0x04;
•
ATT.FMC_WaitSetupTime = 0x07;
•
ATT.FMC_HoldSetupTime = 0x04;
•
ATT.FMC_HiZSetupTime = 0x00;
•
IO.FMC_SetupTime = 0x04;
•
IO.FMC_WaitSetupTime = 0x07;
•
IO.FMC_HoldSetupTime = 0x04;
•
IO.FMC_HiZSetupTime = 0x00;
•
TCLRSetupTime = 0;
•
TARSetupTime = 0.
In all timing tables, the THCLK is the HCLK clock period.
td(CLKH-AIV)
FMC_CLK high to FMC_Ax invalid (x=16…25)
 -
ns
td(CLKL-NWEL)
FMC_CLK low to FMC_NWE low
 -
ns
td(CLKH-NWEH)
FMC_CLK high to FMC_NWE high
THCLK−0.5
 -
ns
td(CLKL-Data)
FMC_D[15:0] valid data after FMC_CLK low
 -
2.5
ns
td(CLKL-NBLL)
FMC_CLK low to FMC_NBL low
-
ns
td(CLKH-NBLH)
FMC_CLK high to FMC_NBL high
THCLK−0.5
 -
ns
tsu(NWAIT-CLKH) FMC_NWAIT valid before FMC_CLK high
-
-
th(CLKH-NWAIT)
FMC_NWAIT valid after FMC_CLK high
-
-
1.
CL = 30 pF.
2.
Evaluated by characterization.
Table 98. Synchronous non-multiplexed PSRAM write timings(1)(2) (continued)
Symbol
Parameter
Min
Max
Unit

---

Electrical characteristics
Figure 63. PC Card/CompactFlash controller waveforms for common memory read
access
1.
FMC_NCE4_2 remains high (inactive during 8-bit access.
Figure 64. PC Card/CompactFlash controller waveforms for common memory write
access
FMC_NWE
tw(NOE)
FMC_NOE
FMC_D[15:0]
FMC_A[10:0]
FMC_NCE4_2(1)
FMC_NCE4_1
FMC_NREG
FMC_NIOWR
FMC_NIORD
td(NCE4_1-NOE)
tsu(D-NOE)
th(NOE-D)
tv(NCEx-A)
td(NREG-NCEx)
td(NIORD-NCEx)
th(NCEx-AI)
th(NCEx-NREG)
th(NCEx-NIORD)
th(NCEx-NIOWR)
MS32761V1
MS32762V1
td(NCE4_1-NWE)
tw(NWE)
th(NWE-D)
tv(NCE4_1-A)
td(NREG-NCE4_1)
td(NIORD-NCE4_1)
th(NCE4_1-AI)
MEMxHIZ =1
tv(NWE-D)
th(NCE4_1-NREG)
th(NCE4_1-NIORD)
th(NCE4_1-NIOWR)
FMC_NWE
FMC_NOE
FMC_D[15:0]
FMC_A[10:0]
FMC_NCE4_1
FMC_NREG
FMC_NIOWR
FMC_NIORD
td(NWE-NCE4_1)
td(D-NWE)
FMC_NCE4_2
High

---

Electrical characteristics
Figure 65. PC Card/CompactFlash controller waveforms for attribute memory
read access
1.
Only data bits 0...7 are read (bits 8...15 are disregarded).
MS32763V1
td(NCE4_1-NOE)
tw(NOE)
tsu(D-NOE)
th(NOE-D)
tv(NCE4_1-A)
th(NCE4_1-AI)
td(NREG-NCE4_1)
th(NCE4_1-NREG)
FMC_NWE
FMC_NOE
FMC_D[15:0](1)
FMC_A[10:0]
FMC_NCE4_2
FMC_NCE4_1
FMC_NREG
FMC_NIOWR
FMC_NIORD
td(NOE-NCE4_1)
High

---

Electrical characteristics
Figure 66. PC Card/CompactFlash controller waveforms for attribute memory
 write access
1.
Only data bits 0...7 are driven (bits 8...15 remains Hi-Z).
Figure 67. PC Card/CompactFlash controller waveforms for I/O space read access
MS32764V1
tw(NWE)
tv(NCE4_1-A)
td(NREG-NCE4_1)
th(NCE4_1-AI)
th(NCE4_1-NREG)
tv(NWE-D)
FMC_NWE
FMC_NOE
FMC_D[7:0](1)
FMC_A[10:0]
FMC_NCE4_2
FMC_NCE4_1
FMC_NREG
FMC_NIOWR
FMC_NIORD
td(NWE-NCE4_1)
High
td(NCE4_1-NWE)
MS32765V1
td(NIORD-NCE4_1)
tw(NIORD)
tsu(D-NIORD)
td(NIORD-D)
tv(NCEx-A)
th(NCE4_1-AI)
FMC_NWE
FMC_NOE
FMC_D[15:0]
FMC_A[10:0]
FMC_NCE4_2
FMC_NCE4_1
FMC_NREG
FMC_NIOWR
FMC_NIORD

---

Electrical characteristics
Figure 68. PC Card/CompactFlash controller waveforms for I/O space write access
t d(NCE4_1-NIOWR)
tw(NIOWR)
tv(NCEx-A)
th(NCE4_1-AI)
th(NIOWR-D)
ATTxHIZ =1
tv(NIOWR-D)
MS32766V1
FMC_NWE
FMC_NOE
FMC_D[15:0]
FMC_A[10:0]
FMC_NCE4_2
FMC_NCE4_1
FMC_NREG
FMC_NIOWR
FMC_NIORD
Table 99. Switching characteristics for PC Card/CF read and write cycles
 in attribute/common space(1)(2)
Symbol
Parameter
Min
Max
Unit
tv(NCEx-A)
FMC_Ncex low to FMC_Ay valid
-
ns
th(NCEx_AI)
FMC_NCEx high to FMC_Ax invalid
-
ns
td(NREG-NCEx)
FMC_NCEx low to FMC_NREG valid
-
ns
th(NCEx-NREG)
FMC_NCEx high to FMC_NREG invalid
THCLK −2
-
ns
td(NCEx-NWE)
FMC_NCEx low to FMC_NWE low
-
5THCLK
ns
tw(NWE)
FMC_NWE low width
8THCLK −0.5
8THCLK+0.5
ns
td(NWE_NCEx)
FMC_NWE high to FMC_NCEx high
5THCLK+1
-
ns
tV(NWE-D)
FMC_NWE low to FMC_D[15:0] valid
-
ns
th(NWE-D)
FMC_NWE high to FMC_D[15:0] invalid
9THCLK −0.5
-
ns
td(D-NWE)
FMC_D[15:0] valid before FMC_NWE high
13THCLK −3
-
ns
td(NCEx-NOE)
FMC_NCEx low to FMC_NOE low
-
5THCLK
ns
tw(NOE)
FMC_NOE low width
8 THCLK −0.5
8 THCLK+0.5
ns
td(NOE_NCEx)
FMC_NOE high to FMC_NCEx high
5THCLK −1
-
ns
tsu (D-NOE)
FMC_D[15:0] valid data before FMC_NOE high
THCLK
-
ns
th(NOE-D)
FMC_NOE high to FMC_D[15:0] invalid
-
ns
1.
CL = 30 pF.
2.
Guaranteed by characterization results.

---

Electrical characteristics
NAND controller waveforms and timings
Figure 69 and Figure 70 represent synchronous waveforms, and Table 101 and Table 102
provide the corresponding timings. The results shown in this table are obtained with the
following FMC configuration:
•
COM.FMC_SetupTime = 0x01;
•
COM.FMC_WaitSetupTime = 0x03;
•
COM.FMC_HoldSetupTime = 0x02;
•
COM.FMC_HiZSetupTime = 0x01;
•
ATT.FMC_SetupTime = 0x01;
•
ATT.FMC_WaitSetupTime = 0x03;
•
ATT.FMC_HoldSetupTime = 0x02;
•
ATT.FMC_HiZSetupTime = 0x01;
•
Bank = FMC_Bank_NAND;
•
MemoryDataWidth = FMC_MemoryDataWidth_16b;
•
ECC = FMC_ECC_Enable;
•
ECCPageSize = FMC_ECCPageSize_512Bytes;
•
TCLRSetupTime = 0;
•
TARSetupTime = 0.
In all timing tables, the THCLK is the HCLK clock period.
Table 100. Switching characteristics for PC Card/CF read and write cycles
 in I/O space(1)(2)
Symbol
Parameter
Min
Max
Unit
tw(NIOWR)
FMC_NIOWR low width
8THCLK −0.5
-
ns
tv(NIOWR-D)
FMC_NIOWR low to FMC_D[15:0] valid
-
ns
th(NIOWR-D)
FMC_NIOWR high to FMC_D[15:0] invalid
9THCLK −2
-
ns
td(NCE4_1-NIOWR) FMC_NCE4_1 low to FMC_NIOWR valid
-
5THCLK
ns
th(NCEx-NIOWR)
FMC_NCEx high to FMC_NIOWR invalid
5THCLK
-
ns
td(NIORD-NCEx)
FMC_NCEx low to FMC_NIORD valid
-
5THCLK
ns
th(NCEx-NIORD)
FMC_NCEx high to FMC_NIORD) valid
6THCLK+2
-
ns
tw(NIORD)
FMC_NIORD low width
8THCLK −0.5
8THCLK+0.5
ns
tsu(D-NIORD)
FMC_D[15:0] valid before FMC_NIORD high
THCLK
-
ns
td(NIORD-D)
FMC_D[15:0] valid after FMC_NIORD high
-
ns
1.
CL = 30 pF.
2.
Evaluated by characterization.

---

Electrical characteristics
Figure 69. NAND controller waveforms for read access
1.
y= 7 or 15 depending on the NAND flash memory interface.
Figure 70. NAND controller waveforms for write access
2.
y= 7 or 15 depending on the NAND flash memory interface.
         N
Table 101. Switching characteristics for NAND Flash read cycles(1)
1.
CL = 30 pF.
Symbol
Parameter
Min
Max
Unit
tw(N0E)
FMC_NOE low width
4THCLK −0.5
4THCLK+0.5
ns
tsu(D-NOE)
FMC_D[15-0] valid data before FMC_NOE high
-
ns
th(NOE-D)
FMC_D[15-0] valid data after FMC_NOE high
-
ns
td(ALE-NOE)
FMC_ALE valid before FMC_NOE low
-
3THCLK −0.5
ns
th(NOE-ALE)
FMC_NWE high to FMC_ALE invalid
3THCLK −2
-
ns
MSv73150V1
FMC_NWE
FMC_NOE (NRE)
FMC_D[y:0]
tw(NOE)
tsu(D-NOE)
th(NOE-D)
ALE (FMC_A17)
CLE (FMC_A16)
FMC_NCEx
td(ALE-NOE)
th(NOE-ALE)
MSv73151V1
tw(NWE)
th(NWE-D)
tv(NWE-D)
FMC_NWE
FMC_NOE (NRE)
FMC_D[y:0]
td(D-NWE)
ALE (FMC_A17)
CLE (FMC_A16)
FMC_NCEx
td(ALE-NWE)
th(NWE-ALE)

---

Electrical characteristics
SDRAM waveforms and timings
Figure 71. SDRAM read access waveforms (CL = 1)
Table 102. Switching characteristics for NAND Flash write cycles(1)
1.
CL = 30 pF.
Symbol
Parameter
Min
Max
Unit
tw(NWE)
FMC_NWE low width
4THCLK
4THCLK+1
ns
tv(NWE-D)
FMC_NWE low to FMC_D[15-0] valid
-
ns
th(NWE-D)
FMC_NWE high to FMC_D[15-0] invalid
3THCLK −1
-
ns
td(D-NWE)
FMC_D[15-0] valid before FMC_NWE high
5THCLK −3
-
ns
td(ALE-NWE)
FMC_ALE valid before FMC_NWE low
-
3THCLK−0.5
ns
th(NWE-ALE)
FMC_NWE high to FMC_ALE invalid
3THCLK −1
-
ns
MS32751V2
Row n
Col1
FMC_SDCLK
FMC_A[12:0]
FMC_SDNRAS
FMC_SDNCAS
FMC_SDNWE
FMC_D[31:0]
FMC_SDNE[1:0]
td(SDCLKL_AddR)
td(SDCLKL_AddC)
th(SDCLKL_AddR)
th(SDCLKL_AddC)
td(SDCLKL_SNDE)
tsu(SDCLKH_Data)
th(SDCLKH_Data)
Col2
Coli
Coln
Data2
Datai
Datan
Data1
th(SDCLKL_SNDE)
td(SDCLKL_NRAS)
td(SDCLKL_NCAS)
th(SDCLKL_NCAS)
th(SDCLKL_NRAS)

---

Electrical characteristics
Table 103. SDRAM read timings(1)(2)
1.
CL = 30 pF on data and address lines. CL=15pF on FMC_SDCLK.
2.
Evaluated by characterization.
Symbol
Parameter
Min
Max
Unit
tw(SDCLK)
FMC_SDCLK period
2THCLK −0.5
2THCLK+0.5
ns
tsu(SDCLKH _Data)
Data input setup time
-
th(SDCLKH_Data)
Data input hold time
-
td(SDCLKL_Add)
Address valid time
-
1.5
td(SDCLKL- SDNE)
Chip select valid time
-
0.5
th(SDCLKL_SDNE)
Chip select hold time
-
td(SDCLKL_SDNRAS)
SDNRAS valid time
-
0.5
th(SDCLKL_SDNRAS)
SDNRAS hold time
-
td(SDCLKL_SDNCAS)
SDNCAS valid time
-
0.5
th(SDCLKL_SDNCAS)
SDNCAS hold time
-
Table 104. LPSDR SDRAM read timings(1)(2)
1.
CL = 10 pF.
2.
Evaluated by characterization.
Symbol
Parameter
Min
Max
Unit
tW(SDCLK)
FMC_SDCLK period
2THCLK −0.5
2THCLK+0.5
ns
tsu(SDCLKH_Data)
Data input setup time
2.5
-
th(SDCLKH_Data)
Data input hold time
-
td(SDCLKL_Add)
Address valid time
-
td(SDCLKL_SDNE)
Chip select valid time
-
th(SDCLKL_SDNE)
Chip select hold time
-
td(SDCLKL_SDNRAS
SDNRAS valid time
-
th(SDCLKL_SDNRAS)
SDNRAS hold time
-
td(SDCLKL_SDNCAS)
SDNCAS valid time
-
th(SDCLKL_SDNCAS)
SDNCAS hold time
-

---

Electrical characteristics
Figure 72. SDRAM write access waveforms
MS32752V2
Row n
Col1
FMC_SDCLK
FMC_A[12:0]
FMC_SDNRAS
FMC_SDNCAS
FMC_SDNWE
FMC_D[31:0]
FMC_SDNE[1:0]
td(SDCLKL_AddR)
td(SDCLKL_AddC)
th(SDCLKL_AddR)
th(SDCLKL_AddC)
td(SDCLKL_SNDE)
td(SDCLKL_Data)
th(SDCLKL_Data)
Col2
Coli
Coln
Data2
Datai
Datan
Data1
th(SDCLKL_SNDE)
td(SDCLKL_NRAS)
td(SDCLKL_NCAS)
th(SDCLKL_NCAS)
th(SDCLKL_NRAS)
td(SDCLKL_NWE)
th(SDCLKL_NWE)
FMC_NBL[3:0]
td(SDCLKL_NBL)

---

Electrical characteristics
Table 105. SDRAM write timings(1)(2)
1.
CL = 30 pF on data and address lines. CL=15 pF on FMC_SDCLK.
2.
Evaluated by characterization.
Symbol
Parameter
Min
Max
Unit
tw(SDCLK)
FMC_SDCLK  period
2THCLK −0.5
2THCLK+0.5
ns
td(SDCLKL _Data)
Data output valid time
-
3.5
th(SDCLKL _Data)
Data output hold time
-
td(SDCLKL_Add)
Address valid time
-
1.5
td(SDCLKL_SDNWE)
SDNWE valid time
-
th(SDCLKL_SDNWE)
SDNWE hold time
-
td(SDCLKL_ SDNE)
Chip  select valid time
-
0.5
th(SDCLKL-_SDNE)
Chip  select hold time
-
td(SDCLKL_SDNRAS)
SDNRAS valid time
-
th(SDCLKL_SDNRAS)
SDNRAS hold time
-
td(SDCLKL_SDNCAS)
SDNCAS valid time
-
0.5
td(SDCLKL_SDNCAS)
SDNCAS hold time
-
td(SDCLKL_NBL)
NBL valid time
-
0.5
th(SDCLKL_NBL)
NBLoutput time
-
Table 106. LPSDR SDRAM write timings(1)(2)
1.
CL = 10 pF.
2.
Evaluated by characterization.
Symbol
Parameter
Min
Max
Unit
tw(SDCLK)
FMC_SDCLK  period
2THCLK −0.5
2THCLK+0.5
ns
td(SDCLKL _Data)
Data output valid time
-
th(SDCLKL _Data)
Data output hold time
-
td(SDCLKL_Add)
Address valid time
-
2.8
td(SDCLKL-SDNWE)
SDNWE valid time
-
th(SDCLKL-SDNWE)
SDNWE hold time
-
td(SDCLKL- SDNE)
Chip  select valid time
-
1.5
th(SDCLKL- SDNE)
Chip  select hold time
-
td(SDCLKL-SDNRAS)
SDNRAS valid time
-
1.5
th(SDCLKL-SDNRAS)
SDNRAS hold time
1.5
-
td(SDCLKL-SDNCAS)
SDNCAS valid time
-
1.5
td(SDCLKL-SDNCAS)
SDNCAS hold time
1.5
-
td(SDCLKL_NBL)
NBL valid time
-
1.5
th(SDCLKL-NBL)
NBL output time
1.5
-

---

Electrical characteristics
6.3.27
Camera interface (DCMI) timing specifications
Unless otherwise specified, the parameters given in Table 107 for DCMI are derived
from tests performed under the ambient temperature, fHCLK frequency, and VDD supply
voltage summarized in Table 17, with the following configuration:
•
DCMI_PIXCLK polarity: falling
•
DCMI_VSYNC and DCMI_HSYNC polarity: high
•
Data formats: 14 bits
Figure 73. DCMI timing diagram
Table 107. DCMI characteristics
Symbol
Parameter
Min
Max
Unit
Frequency ratio DCMI_PIXCLK/fHCLK
-
0.4
DCMI_PIXCLK
Pixel clock input
-
MHz
DPixel
Pixel clock input duty cycle
%
tsu(DATA)
Data input setup time
-
ns
th(DATA)
Data input hold time
2.5
-
tsu(HSYNC)
tsu(VSYNC)
DCMI_HSYNC/DCMI_VSYNC input setup time
0.5
-
th(HSYNC)
th(VSYNC)
DCMI_HSYNC/DCMI_VSYNC input hold time
-
MS32414V2
DCMI_PIXCLK
tsu(VSYNC)
tsu(HSYNC)
DCMI_HSYNC
DCMI_VSYNC
DATA[0:13]
1/DCMI_PIXCLK
th(HSYNC)
th(HSYNC)
tsu(DATA)
th(DATA)

---

Electrical characteristics
6.3.28
LCD-TFT controller (LTDC) characteristics
Unless otherwise specified, the parameters given in Table 108 for LCD-TFT are derived
from tests performed under the ambient temperature, fHCLK frequency, and VDD supply
voltage summarized in Table 17, with the following configuration:
•
LCD_CLK polarity: high
•
LCD_DE polarity: low
•
LCD_VSYNC and LCD_HSYNC polarity: high
•
Pixel formats: 24 bits
Table 108. LTDC characteristics
Symbol
Parameter
Min
Max
Unit
fCLK
LTDC clock output frequency
-
MHz
DCLK
LTDC clock output duty cycle
%
tw(CLKH)
tw(CLKL)
Clock High time, low time
tw(CLK)/2 −0.5 tw(CLK)/2+0.5
ns
tv(DATA)
Data output valid time
-
3.5
th(DATA)
Data output hold time
1.5
-
tv(HSYNC)
HSYNC/VSYNC/DE output valid
time
-
2.5
tv(VSYNC)
tv(DE)
th(HSYNC)
HSYNC/VSYNC/DE output hold
time
-
th(VSYNC)
th(DE)

---

Electrical characteristics
Figure 74. LCD-TFT horizontal timing diagram
Figure 75. LCD-TFT vertical timing diagram
MS32749V1
LCD_CLK
tv(HSYNC)
LCD_HSYNC
LCD_DE
LCD_R[0:7]
LCD_G[0:7]
LCD_B[0:7]
tCLK
LCD_VSYNC
tv(HSYNC)
tv(DE)
th(DE)
Pixel
Pixel
tv(DATA)
th(DATA)
Pixel
N
HSYNC
width
Horizontal
back porch
Active width
Horizontal
back porch
One line
MS32750V1
LCD_CLK
tv(VSYNC)
LCD_R[0:7]
LCD_G[0:7]
LCD_B[0:7]
tCLK
LCD_VSYNC
tv(VSYNC)
M lines data
VSYNC
width
Vertical
back porch
Active width
Vertical
back porch
One frame

---

Electrical characteristics
6.3.29
SD/SDIO MMC card host interface (SDIO) characteristics
Unless otherwise specified, the parameters given in Table 109 for the SDIO/MMC interface
are derived from tests performed under the ambient temperature, fPCLK2 frequency and VDD
supply voltage conditions summarized in Table 17, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load C = 30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
Refer to Section 6.3.17: I/O port characteristics for more details on the input/output
characteristics.
Figure 76. SDIO high-speed mode
Figure 77. SD default mode
ai14888
CK
D, CMD
(output)
tOVD
tOHD

---

Electrical characteristics
6.3.30
RTC characteristics
Table 109. Dynamic characteristics: SD / MMC characteristics(1)(2)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fPP
Clock frequency in data transfer mode
-
-
MHz
-
SDIO_CK/fPCLK2 frequency ratio
-
-
-
-
tW(CKL)
Clock low time
fpp =48 MHz
8.5
-
ns
tW(CKH)
Clock high time
fpp =48 MHz
8.3
-
CMD, D inputs (referenced to CK) in MMC and SD HS mode
tISU
Input setup time HS
fpp =48 MHz
3.5
-
-
ns
tIH
Input hold time HS
fpp =48 MHz
-
-
CMD, D outputs (referenced to CK) in MMC and SD HS mode
tOV
Output valid time HS
fpp =48 MHz
-
4.5
ns
tOH
Output hold time HS
fpp =48 MHz
-
-
CMD, D inputs (referenced to CK) in SD default mode
tISUD
Input setup time SD
fpp =24 MHz
1.5
-
-
ns
tIHD
Input hold time SD
fpp =24 MHz
0.5
-
-
CMD, D outputs (referenced to CK) in SD default mode
tOVD
Output valid default time SD
fpp =24 MHz
-
4.5
6.5
ns
tOHD
Output hold default time SD
fpp =24 MHz
3.5
-
-
1.
Evaluated by characterization.
2.
VDD = 2.7 to 3.6 V.
Table 110. RTC characteristics
Symbol
Parameter
Conditions
Min
Max
-
fPCLK1/RTCCLK frequency ratio
Any read/write operation
from/to an RTC register
-

---

Package information
Package information
In order to meet environmental requirements, ST offers these devices in different grades of
ECOPACK® packages, depending on their level of environmental compliance. ECOPACK®
specifications, grade definitions and product status are available at: www.st.com.
ECOPACK® is an ST trademark.
7.1
Device marking
Refer to technical note “Reference device marking schematics for STM32 microcontrollers
and microprocessors” (TN1433 ) available on www.st.com, for the location of pin 1 / ball A1
as well as the location and orientation of the marking areas versus pin 1 / ball A1.
Parts marked as “ES”, “E” or accompanied by an engineering sample notification letter, are
not yet qualified and therefore not approved for use in production. ST is not responsible for
any consequences resulting from such use. In no event will ST be liable for the customer
using any of these engineering samples in production. ST’s Quality department must be
contacted prior to any decision to use these engineering samples to run a qualification
activity.
A WLCSP simplified marking example (if any) is provided in the corresponding package
information subsection.

---

Package information
7.2
LQFP100 package information (1L)
This LQFP is 100 lead, 14 x 14 mm low-profile quad flat package.
Note:
See list of notes in the notes section.
Figure 78. LQFP100 - Outline(15)
D1/4
E1/4
4x N/4 TIPS
aaaC A-BD
bbb H A-B D
4x
(N-4) x e
A
0.05 A2 A1
b
aaa
C A-BD
ccc C
C
D
D1
D
N
A
SECTION A-A
A
A
B
E
E1
SECTION A-A
GAUGE PLANE
B
B
SECTION B-B
H
E1/4
D1/4
L
S
R1
R2
SECTION B-B
b
b1
c
c1
WITH PLATING
BASE METAL
TOP VIEW
SIDE VIEW
BOTTOM VIEW
1L_LQFP100_ME_V3
(6)
(6)
(10)
ș2
ș
ș
ș
(13)
(12)
(5)
(2)
(4)
(4)
(5)
(2)
(3)
(L1)
(9) (11)
(11)
(11)
(11)
(2)
(11)
(1)

---

Package information
Table 111. LQFP100 - Mechanical data
Symbol
millimeters
inches(14)
Min
Typ
Max
Min
Typ
Max
A
-
1.50
1.60
-
0.0590
0.0630
A1(12)
0.05
-
0.15
0.0019
-
0.0059
A2
1.35
1.40
1.45
0.0531
0.0551
0.0570
b(9)(11)
0.17
0.22
0.27
0.0067
0.0087
0.0106
b1(11)
0.17
0.20
0.23
0.0067
0.0079
0.0090
c(11)
0.09
-
0.20
0.0035
-
0.0079
c1(11)
0.09
-
0.16
0.0035
-
0.0063
D(4)
16.00 BSC
0.6299 BSC
D1(2)(5)
14.00 BSC
0.5512 BSC
E(4)
16.00 BSC
0.6299 BSC
E1(2)(5)
14.00 BSC
0.5512 BSC
e
0.50 BSC
0.0197 BSC
L
0.45
0.60
0.75
0.177
0.0236
0.0295
L1(1)(11)
1.00
-
0.0394
-
N(13)
θ
0°
3.5°
7°
0°
3.5°
7°
θ1
0°
-
-
0°
-
-
θ2
10°
12°
14°
10°
12°
14°
θ3
10°
12°
14°
10°
12°
14°
R1
0.08
-
-
0.0031
-
-
R2
0.08
-
0.20
0.0031
-
0.0079
S
0.20
-
-
0.0079
-
-
aaa(1)
0.20
0.0079
bbb(1)
0.20
0.0079
ccc(1)
0.08
0.0031
ddd(1)
0.08
0.0031

---

Package information
Notes:
1.
Dimensioning and tolerancing schemes conform to ASME Y14.5M-1994.
2.
The Top package body size may be smaller than the bottom package size by as much
as 0.15 mm.
3.
Datums A-B and D to be determined at datum plane H.
4.
To be determined at seating datum plane C.
5.
Dimensions D1 and E1 do not include mold flash or protrusions. Allowable mold flash
or protrusions is “0.25 mm” per side. D1 and E1 are Maximum plastic body size
dimensions including mold mismatch.
6.
Details of pin 1 identifier are optional but must be located within the zone indicated.
7.
All Dimensions are in millimeters.
8.
No intrusion allowed inwards the leads.
9.
Dimension “b” does not include dambar protrusion. Allowable dambar protrusion shall
not cause the lead width to exceed the maximum “b” dimension by more than 0.08 mm.
Dambar cannot be located on the lower radius or the foot. Minimum space between
protrusion and an adjacent lead is 0.07 mm for 0.4 mm and 0.5 mm pitch packages.
10. Exact shape of each corner is optional.
11. These dimensions apply to the flat section of the lead between 0.10 mm and 0.25 mm
from the lead tip.
12. A1 is defined as the distance from the seating plane to the lowest point on the package
body.
13. “N” is the number of terminal positions for the specified body size.
14. Values in inches are converted from mm and rounded to 4 decimal digits.
15. Drawing is not to scale.
Figure 79. LQFP100 - Footprint example
1.
Dimensions are expressed in millimeters.
0.5
0.3
16.7
14.3
12.3
1.2
16.7
1L_LQFP100_FP_V1

---

Package information
7.3
WLCSP143 package information
Figure 80. WLCSP143 - 143-ball, 4.521x 5.547 mm, 0.4 mm pitch wafer level chip scale
 package outline
1.
Drawing is not to scale.
         Table 112. WLCSP143 - 143-ball, 4.521x 5.547 mm, 0.4 mm pitch wafer level chip scale
 package mechanical data
Symbol
millimeters
inches(1)
Min
Typ
Max
Min
Typ
Max
A
0.525
0.555
0.585
0.0207
0.0219
0.0230
A1
-
0.175
-
-
0.0069
-
A2
-
0.380
-
-
0.0150
-
A0WE_ME_V2
e1
F
G
e
e
Bottom view
Bump side
e2
A1 ball location
D
A1 orientation
reference
Top view
Wafer back side
Detail A
A3
A2
Side view
A
E
Detail A
Rotated 90°
Bump
Seating
plane
b
A1
A3
aaa
ccc
ddd
Z
Z
X Y
bbb
eee

---

Package information
Figure 81. WLCSP143 - 143-ball, 4.521x 5.547 mm, 0.4 mm pitch wafer level chip scale
package recommended footprint
A3(2)
-
0.025
-
-
0.0010
-
b(3)
0.220
0.250
0.280
0.0087
0.0098
0.0110
D
4.486
4.521
4.556
0.1766
0.1780
0.1794
E
5.512
5.547
5.582
0.2170
0.2184
0.2198
e
-
0.400
-
-
0.0157
-
e1
-
4.000
-
-
0.1575
-
e2
-
4.800
-
-
0.1890
-
F
-
0.2605
-
-
0.0103
-
G
-
0.3735
-
-
0.0147
-
aaa
-
-
0.100
-
-
0.0039
bbb
-
-
0.100
-
-
0.0039
ccc
-
-
0.100
-
-
0.0039
ddd
-
-
0.050
-
-
0.0020
eee
-
-
0.050
-
-
0.0020
1.
Values in inches are converted from mm and rounded to 4 decimal digits.
2.
Back side coating.
3.
Dimension is measured at the maximum bump diameter parallel to primary datum Z.
Table 112. WLCSP143 - 143-ball, 4.521x 5.547 mm, 0.4 mm pitch wafer level chip scale
 package mechanical data (continued)
Symbol
millimeters
inches(1)
Min
Typ
Max
Min
Typ
Max
A0WE_FP_V1
Dpad
Dsm

---

Package information
7.3.1
Device marking for WLCSP143
The following figure gives an example of topside marking orientation versus ball A 1
identifier location.
Other optional marking or inset/upset marks, which depend on assembly location, are not
indicated below.
Figure 82. WLCSP143 marking example (package top view)
Table 113. WLCSP143 recommended PCB design rules
Dimension
Recommended values
Pitch
0.4
Dpad
0.225 mm
Dsm
0.290 mm typ. (depends on the soldermask
registration tolerance)
Stencil opening
0.250 mm
Stencil thickness
0.100 mm
MSv37234V3
Date code = Year+Week
ball A1
Product
identification(1)
Y WW

---

Package information
7.4
LQFP144 package information (1A)
This LQFP is a 144-pin, 20 x 20 mm low-profile quad flat package.
Note:
See list of notes in the notes section.
Figure 83. LQFP144 - Outline(15)
SECTION A-A
GAUGE PLANE
B
B
SECTION B-B
H
L
(L1)
S
R1
R2
SECTION B-B
b
b1
c
c1
WITH PLATING
BASE METAL
(6)
D 1/4
E 1/4
BOTTOM VIEW
ddd
C A-B D
0.05
(12)
A1
A2
A
aaa C A-B D
(N-4)x e
1A_LQFP144_ME_V2
4x
H A-B D
bbb
b
C
ccc C
D 1/4
E 1/4
D
D1
(3)
N
(10)
(6)
(3)
A
(3)
B
(2)
(5)
E1
E
(2)(5)
(4)
(4)
A
A
(Section A-A)
D
TOP VIEW
(2)
0.25
(11)
4x N/4 TIPS
(9)(11)
(11)
(11)
(11)
(1)

---

Package information
Table 114. LQFP144 - Mechanical data
Symbol
millimeters
inches(14)
Min
Typ
Max
Min
Typ
Max
A
-
-
1.60
-
-
0.0630
A1(12)
0.05
-
0.15
0.0020
-
0.0059
A2
1.35
1.40
1.45
0.0531
0.0551
0.0571
b(9)(11)
0.17
0.22
0.27
0.0067
0.0087
0.0106
b1(11)
0.17
0.20
0.23
0.0067
0.0079
0.0090
c(11)
0.09
-
0.20
0.0035
-
0.0079
c1(11)
0.09
-
0.16
0.0035
-
0.0063
D(4)
22.00 BSC
0.8661 BSC
D1(2)(5)
20.00 BSC
0.7874 BSC
E(4)
22.00 BSC
0.8661 BSC
E1(2)(5)
20.00 BSC
0.7874 BSC
e
0.50 BSC
0.0197 BSC
L
0.45
0.60
0.75
0.0177
0.0236
0.0295
L1
1.00 REF
0.0394 REF
N(13)
θ
0°
3.5°
7°
0°
3.5°
7°
θ1
0°
-
-
0°
-
-
θ2
10°
12°
14°
10°
12°
14°
θ3
10°
12°
14°
10°
12°
14°
R1
0.08
-
-
0.0031
-
-
R2
0.08
-
0.20
0.0031
-
0.0079
S
0.20
-
-
0.0079
-
-
aaa
0.20
0.0079
bbb
0.20
0.0079
ccc
0.08
0.0031
ddd
0.08
0.0031

---

Package information
Notes:
1.
Dimensioning and tolerancing schemes conform to ASME Y14.5M-1994.
2.
The Top package body size may be smaller than the bottom package size by as much
as 0.15 mm.
3.
Datums A-B and D to be determined at datum plane H.
4.
To be determined at seating datum plane C.
5.
Dimensions D1 and E1 do not include mold flash or protrusions. Allowable mold flash
or protrusions is “0.25 mm” per side. D1 and E1 are Maximum plastic body size
dimensions including mold mismatch.
6.
Details of pin 1 identifier are optional but must be located within the zone indicated.
7.
All Dimensions are in millimeters.
8.
No intrusion allowed inwards the leads.
9.
Dimension “b” does not include dambar protrusion. Allowable dambar protrusion shall
not cause the lead width to exceed the maximum “b” dimension by more than 0.08 mm.
Dambar cannot be located on the lower radius or the foot. Minimum space between
protrusion and an adjacent lead is 0.07 mm for 0.4 mm and 0.5 mm pitch packages.
10. Exact shape of each corner is optional.
11. These dimensions apply to the flat section of the lead between 0.10 mm and 0.25 mm
from the lead tip.
12. A1 is defined as the distance from the seating plane to the lowest point on the package
body.
13. “N” is the number of terminal positions for the specified body size.
14. Values in inches are converted from mm and rounded to 4 decimal digits.
15. Drawing is not to scale.

---

Package information
Figure 84. LQFP144 - Footprint example
1.
Dimensions are expressed in millimeters.
0.50
0.35
19.90
17.85
22.60
1.35
22.60
19.90
1A_LQFP144_FP

---

Package information
7.5
LQFP176 package information (1T)
This LQFP is a 176-pin, 24 x 24 mm, 0.5 mm pitch, low profile quad flat package.
Note:
See list of notes in the notes section.
Figure 85. LQFP176 - Outline(15)
SECTION A-A
BOTTOM VIEW
SIDE VIEW
TOP VIEW
SECTION B-B
ș1
ș2
S
(L1)
L
B
ș
0.25
B
R2
R1
c
b
WITH PLATING
BASE METAL
b1
c1
GAUGE PLANE
H
bbb H A-B D
4x
E1/4
D1/4
4x N/4 TIPS
aaaC A-BD
D
D1
N
D
SECTION A-A
(See SECTION B-B)
A
A
E1/4
D1/4
E1 E
A
B
ddd
C A-BD
b
ccc C
C
A
(N-4) x e
A1
A2
0.05
1T_LQFP176_ME_V2
ș
(2)
(11)
(1)
(2)
(9) (11)
(11)
(11)
(11)

(4)
(6)

(4)
(5)
(5)
(2)
(10)
(12)

(6)

---

Package information
Table 115. LQFP176 - Mechanical data
Symbol
millimeters
inches(14)
Min
Typ
Max
Min
Typ
Max
A
-
-
1.600
-
-
0.0630
A1(12)
0.050
-
0.150
0.0020
-
0.0059
A2
1.350
1.400
1.450
0.0531
0.0551
0.0571
b(9)(11)
0.170
0.220
0.270
0.0067
0.0087
0.0106
b1(11)
0.170
0.200
0.230
0.0067
0.0079
0.0091
c(11)
0.090
-
0.200
0.0035
-
0.0079
c1(11)
0.090
-
0.160
0.0035
-
0.063
D(4)
26.000
1.0236
D1(2)(5)
24.000
0.9449
E(4)
26.000
0.0197
E1(2)(5)
24.000
0.9449
e
0.500
0.1970
L
0.450
0.600
0.750
0.0177
0.0236
0.0295
L1(1)(11)
0.0394 REF
N(13)
θ
0°
3.5°
7°
0°
3.5°
7°
θ1
0°
-
-
0°
-
-
θ2
10°
12°
14°
10°
12°
14°
θ3
10°
12°
14°
10°
12°
14°
R1
0.080
-
-
0.0031
-
-
R2
0.080
-
0.200
0.0031
-
0.0079
S
0.200
-
-
0.0079
-
-
aaa(1)
0.200
0.0079
bbb(1)
0.200
0.0079
ccc(1)
0.080
0.0031
ddd(1)
0.080
0.0031

---

Package information
Notes:
1.
Dimensioning and tolerancing schemes conform to ASME Y14.5M-1994.
2.
The Top package body size may be smaller than the bottom package size by as much
as 0.15 mm.
3.
Datums A-B and D to be determined at datum plane H.
4.
To be determined at seating datum plane C.
5.
Dimensions D1 and E1 do not include mold flash or protrusions. Allowable mold flash
or protrusions is “0.25 mm” per side. D1 and E1 are Maximum plastic body size
dimensions including mold mismatch.
6.
Details of pin 1 identifier are optional but must be located within the zone indicated.
7.
All Dimensions are in millimeters.
8.
No intrusion allowed inwards the leads.
9.
Dimension “b” does not include dambar protrusion. Allowable dambar protrusion shall
not cause the lead width to exceed the maximum “b” dimension by more than 0.08 mm.
Dambar cannot be located on the lower radius or the foot. Minimum space between
protrusion and an adjacent lead is 0.07 mm for 0.4 mm and 0.5 mm pitch packages.
10. Exact shape of each corner is optional.
11. These dimensions apply to the flat section of the lead between 0.10 mm and 0.25 mm
from the lead tip.
12. A1 is defined as the distance from the seating plane to the lowest point on the package
body.
13. “N” is the number of terminal positions for the specified body size.
14. Values in inches are converted from mm and rounded to 4 decimal digits.
15. Drawing is not to scale.

---

Package information
Figure 86. LQFP176 - Footprint example
1.
Dimensions are expressed in millimeters.
1T_FP_V1
1.2
0.3
0.5
1.2
21.8
26.7
1 176
26.7
21.8

---

Package information
7.6
LQFP208 package information
This LQFP is a 208-pin, 28 x 28 mm low-profile quad flat package.
Note:
See list of notes in the notes section.
Figure 87. LQFP208 - Outline(15)
SECTION A-A
GAUGE PLANE
B
B
SECTION B-B
H
L
S
R1
R2
SECTION B-B
b
b1
c
c1
WITH
PLATING
BASE METAL
UH_LQFP208_ME_V2
(L1)
(2)
0.25
(11)
(9) (11)
(11)
(11)
(11)
(1)
D 1/4
E 1/4
(6)
4x
bbb H A-B D
aaa C A-B D
4x N/4 TIPS
(12)
A2
(13)
(N – 4)xe
A1
A
ddd
C A-B D
0.05
b
C
ccc C
(10)
(2)(5)
N
D1
D
D (3)
(4)
D 1/4
(3) A
(6)
E 1/4
(3)
E1
(4)
E
B
(5)
(2)
A
(Section A-A)
A
TOP VIEW
BOTTOM VIEW

---

Package information
Table 116. LQFP208 - Mechanical data
Symbol
millimeters
inches(15)
Min
Typ
Max
Min
Typ
Max
A
-
-
1.60
-
-
0.0630
A1(12)
0.05
-
0.15
0.0020
-
0.0059
A2
1.35
1.40
1.45
0.0531
0.0551
0.0571
b(9)(11)
0.17
0.22
0.27
0.0067
0.0087
0.0106
b1(11)
0.17
0.20
0.23
0.0067
0.0079
0.0091
c(11)
0.09
-
0.20
0.0035
-
0.0079
c1(11)
0.09
-
0.16
0.0035
-
0.0063
D(4)
30.00 BSC
1.1732 BSC
D1(2)(5)
28.00 BSC
1.0945 BSC
E(4)
30.00 BSC
1.1732 BSC
E1(2)(5)
28.00 BSC
1.0945 BSC
e
0.50 BSC
0.0197 BSC
L
0.45
0.60
0.75
0.0177
0.0236
0.0295
L1
1.00 REF
0.0394 REF
N(13)
θ
0°
3.5°
7°
0°
3.5°
7°
θ1
0°
-
-
0°
-
-
θ2
10°
12°
14°
10°
12°
14°
θ3
10°
12°
14°
10°
12°
14°
R1
0.08
-
-
0.0031
-
-
R2
0.08
-
0.20
0.0031
-
0.0079
S
0.20
-
-
0.0079
-
-
aaa(1)(7)
0.20
0.0079
bbb(1)(7)
0.20
0.0079
ccc(1)(7)
0.08
0.0031
ddd(1)(7)
0.08
0.0031

---

Package information
Notes:
1.
Dimensioning and tolerancing schemes conform to ASME Y14.5M-1994.
2.
The Top package body size may be smaller than the bottom package size by as much
as 0.15 mm.
3.
Datums A-B and D to be determined at datum plane H.
4.
To be determined at seating datum plane C.
5.
Dimensions D1 and E1 do not include mold flash or protrusions. Allowable mold flash
or protrusions is “0.25 mm” per side. D1 and E1 are Maximum plastic body size
dimensions including mold mismatch.
6.
Details of pin 1 identifier are optional but must be located within the zone indicated.
7.
All Dimensions are in millimeters.
8.
No intrusion allowed inwards the leads.
9.
Dimension “b” does not include dambar protrusion. Allowable dambar protrusion shall
not cause the lead width to exceed the maximum “b” dimension by more than 0.08 mm.
Dambar cannot be located on the lower radius or the foot. Minimum space between
protrusion and an adjacent lead is 0.07 mm for 0.4 mm and 0.5 mm pitch packages.
10. Exact shape of each corner is optional.
11. These dimensions apply to the flat section of the lead between 0.10 mm and 0.25 mm
from the lead tip.
12. A1 is defined as the distance from the seating plane to the lowest point on the package
body.
13. “N” is the number of terminal positions for the specified body size.
14. Values in inches are converted from mm and rounded to 4 decimal digits.
15. Drawing is not to scale.
Figure 88. LQFP208 - footprint example
1.
Dimensions are expressed in millimeters.
UH_LQFP208_FP_V3
30.7
25.8
1.2
30.7
28.3
0.50
0.30
1.25

---

Package information
7.7
UFBGA169 package information (A0YV)
This UFBGA is a 169-ball, 7 x 7 mm, 0.50 mm pitch, ultra fine pitch ball grid array package.
Figure 89. UFBGA169 - Outline
1.
Drawing is not to scale.
2.
Primary datum C is defined by the plane established by the contact points of three or more solder balls that
support the device when it is placed on top of a planar surface.
3.
The terminal (ball) A1 corner must be identified on the top surface of the package by using a corner
chamfer, ink or metallized markings, or other feature of package body or integral heat slug. A distinguish
feature is allowable on the bottom surface of the package to identify the terminal A1 corner. Exact shape of
each corner is optional.
A0YV_UFBGA169_ME_V2
E
TOP VIEW
A1 ball pad
corner
A
SIDE VIEW
Detail A
C
e
N
BOTTOM VIEW
Øb (169 balls)
B
eee
Ø
M
fff
Ø
M
C
C
A
D1
E1
SE
e
2 3
4 5 6 7 8 9 1011 12
A1 ball pad
corner
M
L
K
J
H
G
F
E
D
C
B
A
SD
B
A
D
aaa C
(4x)
Seating plane
A1
A3
C
ddd
Substrate
Solder balls
A5
A2
Mold resin
C
ccc
(DATUM B)
(DATUM A)
DETAIL A
C

---

Package information
Table 117. UFBGA169 - Mechanical data
Symbol
millimeters
inches(1)
1.
Values in inches are converted from mm and rounded to 4 decimal digits.
Min.
Typ.
Max.
Min.
Typ.
Max.
A(2)
2.
The profile height, A, is the distance from the seating plane to the highest point on the package. It is
measured perpendicular to the seating plane.
-
-
0.60
-
-
0.0236
A1(3)
3.
A1 is defined as the distance from the seating plane to the lowest point on the package body.
0.05
-
-
0.0020
-
-
A2
-
0.43
-
-
0.0169
-
b(4)
4.
Dimension b is measured at the maximum diameter of the terminal (ball) in a plane parallel to primary
datum C.
0.23
0.28
0.33
0.0091
0.0110
0.0130
D(5)
5.
BSC stands for BASIC dimensions. It corresponds to the nominal value and has no tolerance. For
tolerances refer to form and position table.
 7.00 BSC
0.2756 BSC
D1(5)
6.00 BSC
0.2362 BSC
E(5)
7.00 BSC
0.2756 BSC
E1(5)
6.00 BSC
0.2362 BSC
e(5)(6)
6.
e represents the solder ball grid pitch.
0.50 BSC
0.0197 BSC
N(7)
7.
N represents the total number of balls on the BGA.
SD(5)(8)
8.
Basic dimensions SD and SE are defined with respect to datums A and B. It defines the position of the
centre ball(s) in the outer row or column of a fully populated matrix.
0.50 BSC
0.0197 BSC
SE(5)(8)
0.50 BSC
0.0197 BSC
aaa(9)
9.
Tolerance of form and position drawing
0.15
0.0059
ccc(9)
0.20
0.0079
ddd(9)
0.08
0.0031
eee(9)
0.15
0.0059
fff(9)
0.05
0.0020

---

Package information
Figure 90. UFBGA169 - Footprint example
Note:
Non-solder mask defined (NSMD) pads are recommended.
Note:
4 to 6 mils solder paste screen printing process.
Table 118. UFBGA169 - Example of PCB design rules (0.5 mm pitch BGA)
Dimension
Values
Pitch
0.5 mm
Dpad
0.27 mm
Dsm
0.35 mm typ. (depends on the soldermask
registration tolerance)
Solder paste
0.27 mm aperture diameter.
BGA_WLCSP_FT_V1
Dsm
Dpad

---

Package information
7.8
UFBGA(176+25) package information (A0E7)
This UFBGA is a 176+25-ball, 10 x 10 mm, 0.65 mm pitch, ultra fine pitch ball grid array
package.
Figure 91. UFBGA(176+25) - Outline
1.
Drawing is not to scale.
Table 119. UFBGA(176+25) - Mechanical data
Symbol
millimeters
inches(1)
Min.
Typ.
Max.
Min.
Typ.
Max.
A
-
-
0.600
-
-
0.0236
A1
0.050
0.080
0.110
0.0020
0.0031
0.0043
A2
-
0.450
-
-
0.0177
-
A3
-
0.130
-
-
0.0051
-
A4
-
0.320
-
-
0.0126
-
b
0.240
0.290
0.340
0.0094
0.0114
0.0134
D
9.850
10.000
10.150
0.3878
0.3937
0.3996
D1
-
9.100
-
-
0.3583
-
E
9.850
10.000
10.150
0.3878
0.3937
0.3996
E1
-
9.100
-
-
0.3583
-
e
 -
0.650
-
-
0.0256
-
F
-
0.450
-
-
0.0177
-
ddd
-
-
0.080
-
-
0.0031
A0E7_ME_V10
Seating plane
A2
C
ddd
A1A
e
F
F
e
R
A
BOTTOM VIEW
E
D
TOP VIEW
Øb (176 + 25  balls)
B
A
B
eee
Ø
M
fff
Ø
M
C
C
A
C
A1 ball
identifier
A1 ball
index
area
b
A4
E1
A3
D1

---

Package information
Figure 92. UFBGA(176+25) - Footprint example
eee
-
-
0.150
-
-
0.0059
fff
-
-
0.050
-
-
0.0020
1.
Values in inches are converted from mm and rounded to 4 decimal digits.
Table 120. UFBGA(176+25) - Example of PCB design rules (0.65 mm pitch BGA)
Dimension
Values
Pitch
0.65 mm
Dpad
0.300 mm
Dsm
0.400 mm typ. (depends on the soldermask
registration tolerance)
Stencil opening
0.300 mm
Stencil thickness
Between 0.100 mm and 0.125 mm
Pad trace width
0.100 mm
Table 119. UFBGA(176+25) - Mechanical data (continued)
Symbol
millimeters
inches(1)
Min.
Typ.
Max.
Min.
Typ.
Max.
BGA_WLCSP_FT_V1
Dsm
Dpad

---

Package information
7.9
TFBGA216 package information (A0L2)
This TFBGA is a 216-ball, 13 x 13 mm, 0.8 mm pitch, fine pitch ball grid array package.
Figure 93. TFBGA216 - Outline
1.
Drawing is not to scale.
2.
• The terminal A1 corner must be identified on the top surface by using a corner chamfer, ink or metalized
markings, or other feature of package body or integral heat slug.
• A distinguishing feature is allowable on the bottom surface of the package to identify the terminal A1
corner. Exact shape of each corner is optional
A0L2_ME_V3
Seating plane
A1
e
F
G
D
R
Øb (216 balls)
A
E
TOP VIEW
BOTTOM VIEW
e
A
A2
Y
X
Z
ddd Z
D1
E1
eee
Z Y X
fff
Ø
Ø
M
M Z
A1 ball
identifier
A1 ball
index area

---

Package information
Table 121. TFBGA216 - Mechanical data
Symbol
millimeters
inches(1)
Min
Typ
Max
Min
Typ
Max
A
-
-
1.200
-
-
0.0472
A1(2)
0.150
-
-
0.0059
-
-
A2
-
0.760
-
-
0.0299
-
b(3)
0.350
0.400
0.450
0.0138
0.0157
0.0177
D
12.850
13.000
13.150
0.5059
0.5118
0.5177
D1
-
11.200
-
-
0.4409
-
E
12.850
13.000
13.150
0.5059
0.5118
0.5177
E1
-
11.200
-
-
0.4409
-
e
-
0.800
-
-
0.0315
-
F
-
0.900
-
-
0.0354
-
G
-
0.900
-
-
0.0354
-
ddd
-
-
0.100
-
-
0.0039
eee(4)
-
-
0.150
-
-
0.0059
fff(5)
-
-
0.080
-
-
0.0031
1.
Values in inches are converted from mm and rounded to four decimal digits.
2.
• The terminal A1 corner must be identified on the top surface by using a corner chamfer, ink or metallized
markings, or other feature of package body or integral heat slug.
• A distinguishing feature is allowable on the bottom surface of the package to identify the terminal A1
corner. Exact shape of each corner is optional.
3.
Initial ball equal 0.350 mm.
4.
The tolerance of position that controls the location of the pattern of balls with respect to datums A and B.
For each ball there is a cylindrical tolerance zone eee perpendicular to datum C and located on true
position with respect to datums A and B as defined by e. The axis perpendicular to datum C of each ball
must lie within this tolerance zone.
5.
The tolerance of position that controls the location of the balls within the matrix with respect to each other.
For each ball there is a cylindrical tolerance zone fff perpendicular to datum C and located on true position
as defined by e. The axis perpendicular to datum C of each ball must lie within this tolerance zone. Each
tolerance zone fff in the array is contained entirely in the respective zone eee above The axis of each ball
must lie simultaneously in both tolerance zones.

---

Package information
Figure 94. TFBGA216 - Footprint example
Table 122. TFBGA216 - Example of PCB design rules (0.8 mm pitch)
Dimension
Values
Pitch
0.8 mm
Dpad
0.400 mm
Dsm
0.470 mm typ. (depends on the soldermask
registration tolerance)
Stencil opening
0.400 mm
Stencil thickness
Between 0.100 mm and 0.125 mm
Pad trace width
0.120 mm
BGA_WLCSP_FT_V1
Dsm
Dpad

---

Package information
7.10
Thermal characteristics
The maximum chip-junction temperature, TJ max, in degrees Celsius, may be calculated
using the following equation:
TJ max = TA max + (PD max x θJA)
Where:
•
TA max is the maximum ambient temperature in °C,
•
θJA is the package junction-to-ambient thermal resistance, in °C/W,
•
PD max is the sum of PINT max and PI/O max (PD max = PINT max + PI/Omax),
•
PINT max is the product of IDD and VDD, expressed in Watts. This is the maximum chip
internal power.
PI/O max represents the maximum power dissipation on output pins where:
PI/O max = ∑ (VOL × IOL) + ∑((VDD – VOH) × IOH),
taking into account the actual VOL / IOL and VOH / IOH of the I/Os at low and high level in the
application.
Reference document
JESD51-2 Integrated Circuits Thermal Test Method Environment Conditions - Natural
Convection (Still Air). Available from www.jedec.org.
Table 123. Package thermal characteristics
Symbol
Parameter
Value
Unit
ΘJA
Thermal resistance junction-ambient
LQFP100 - 14 × 14 mm / 0.5 mm pitch
°C/W
Thermal resistance junction-ambient
WLCSP143
31.2
Thermal resistance junction-ambient
LQFP144 - 20 × 20 mm / 0.5 mm pitch
Thermal resistance junction-ambient
LQFP176 - 24 × 24 mm / 0.5 mm pitch
Thermal resistance junction-ambient
LQFP208 - 28 × 28 mm / 0.5 mm pitch
Thermal resistance junction-ambient
UFBGA169 - 7 × 7mm / 0.5 mm pitch
Thermal resistance junction-ambient
UFBGA176 - 10× 10 mm / 0.5 mm pitch
Thermal resistance junction-ambient
TFBGA216 - 13 × 13 mm / 0.8 mm pitch

---

Ordering information
Ordering information
For a list of available options (speed, package, etc.) or for further information on any aspect
of this device, please contact your nearest ST sales office.
Example:
STM32
F
429 V
I
T
xxx
Device family
STM32 = Arm-based 32-bit microcontroller
Product type
F = general-purpose
Device subfamily
427= STM32F427xx, USB OTG FS/HS, camera interface,
Ethernet
429= STM32F429xx, USB OTG FS/HS, camera interface,
Ethernet, LCD-TFT
Pin count
V = 100 pins
Z = 143 and 144 pins
A = 169 pins
I = 176 pins
B = 208 pins
N = 216 pins
Flash memory size
E = 512 Kbytes of Flash memory
G = 1024 Kbytes of Flash memory
I = 2048 Kbytes of Flash memory
Package
T = LQFP
H = BGA
Y = WLCSP
Temperature range
6 = Industrial temperature range, –40 to 85 °C.
7 = Industrial temperature range, –40 to 105 °C.
Options
xxx = programmed parts
TR = tape and reel

---

Recommendations when using internal reset OFF
Appendix A
Recommendations when using internal reset
OFF
When the internal reset is OFF, the following integrated features are no longer supported:
•
The integrated power-on reset (POR) / power-down reset (PDR) circuitry is disabled.
•
The brownout reset (BOR) circuitry must be disabled.
•
The embedded programmable voltage detector (PVD) is disabled.
•
VBAT functionality is no more available and VBAT pin should be connected to VDD.
•
The over-drive mode is not supported.
A.1
Operating conditions
Table 124. Limitations depending on the operating power supply range
Operating
power
supply
range
ADC
operation
Maximum
Flash
memory
access
frequency
with no wait
states
(fFlashmax)
Maximum Flash
memory access
frequency with
wait states (1)(2)
1.
Applicable only when the code is executed from Flash memory. When the code is executed from RAM, no
wait state is required.
2.
Thanks to the ART accelerator and the 128-bit Flash memory, the number of wait states given here does
not impact the execution speed from Flash memory since the ART accelerator allows to achieve a
performance equivalent to 0 wait state program execution.
I/O operation
Possible Flash
memory
operations
VDD =1.7 to
2.1 V(3)
3.
VDD/VDDA minimum value of 1.7 V, with the use of an external power supply supervisor (refer to
Section 3.17.1: Internal reset ON).
Conversion
time up to
1.2 Msps
20 MHz(4)
4.
Prefetch is not available. Refer to AN3430 application note for details on how to adjust performance and
power.
168 MHz with 8
wait states and
over-drive OFF
– No I/O
compensation
8-bit erase and
program
operations only

---

Application block diagrams
Appendix B
Application block diagrams
B.1
USB OTG full speed (FS) interface solutions
Figure 95. USB controller configured as peripheral-only and used
in Full speed mode
1.
External voltage regulator only needed when building a VBUS powered device.
2.
The same application can be developed using the OTG HS in FS mode to achieve enhanced performance
thanks to the large Rx/Tx FIFO and to a dedicated DMA controller.
Figure 96. USB controller configured as host-only and used in full speed mode
1.
The current limiter is required only if the application has to support a VBUS powered device. A basic power
switch can be used if 5 V are available on the application board.
2.
The same application can be developed using the OTG HS in FS mode to achieve enhanced performance
thanks to the large Rx/Tx FIFO and to a dedicated DMA controller.
STM32F4xx
5V to VDD
Volatge regulator (1)
VDD
VBUS
DP
VSS
PA12/PB15
PA11//PB14
USB Std-B connector
DM
OSC_IN
OSC_OUT
MS19000V5
STM32F4xx
VDD
VBUS
DP
VSS
USB Std-A connector
DM
GPIO+IRQ
GPIO
EN
Overcurrent
5 V Pwr
OSC_IN
OSC_OUT
MS19001V4
Current limiter
power switch(1)
PA12/PB15
PA11//PB14

---

Application block diagrams
Figure 97. USB controller configured in dual mode and used in full speed mode
1.
External voltage regulator only needed when building a VBUS powered device.
2.
The current limiter is required only if the application has to support a VBUS powered device. A basic power
switch can be used if 5 V are available on the application board.
3.
The ID pin is required in dual role only.
4.
The same application can be developed using the OTG HS in FS mode to achieve enhanced performance
thanks to the large Rx/Tx FIFO and to a dedicated DMA controller.
STM32F4xx
VDD
VBUS
DP
VSS
PA9/PB13
PA12/PB15
PA11/PB14
USB micro-AB connector
DM
GPIO+IRQ
GPIO
EN
Overcurrent
5 V Pwr
5 V to VDD
voltage regulator (1)
VDD
ID(3)
PA10/PB12
OSC_IN
OSC_OUT
MS19002V3
Current limiter
power switch(2)

---

Application block diagrams
B.2
USB OTG high speed (HS) interface solutions
Figure 98. USB controller configured as peripheral, host, or dual-mode
and used in high speed mode
1.
It is possible to use MCO1 or MCO2 to save a crystal. It is however not mandatory to clock the STM32F42x
with a 24 or 26 MHz crystal when using USB HS. The above figure only shows an example of a possible
connection.
2.
The ID pin is required in dual role only.
DP
STM32F4xx
DM
VBUS
VSS
DM
DP
ID(2)
USB
USB HS
OTG Ctrl
FS PHY
ULPI
High speed
OTG PHY
ULPI_CLK
ULPI_D[7:0]
ULPI_DIR
ULPI_STP
ULPI_NXT
not connected
connector
MCO1 or MCO2
24 or 26 MHz XT(1)
PLL
XT1
XI
MS19005V2

---

Application block diagrams
B.3
Ethernet interface solutions
Figure 99. MII mode using a 25 MHz crystal
1.
fHCLK must be greater than 25 MHz.
2.
Pulse per second when using IEEE1588 PTP optional signal.
Figure 100. RMII with a 50 MHz oscillator
1.
fHCLK must be greater than 25 MHz.
MCU
Ethernet
MAC 10/100
Ethernet
PHY 10/100
PLL
HCLK
XT1
PHY_CLK 25 MHz
MII_RX_CLK
MII_RXD[3:0]
MII_RX_DV
MII_RX_ER
MII_TX_CLK
MII_TX_EN
MII_TXD[3:0]
MII_CRS
MII_COL
MDIO
MDC
HCLK(1)
PPS_OUT(2)
XTAL
25 MHz
STM32
OSC
TIM2
Timestamp
comparator
Timer
input
trigger
IEEE1588 PTP
MII
= 15 pins
MII + MDC
= 17 pins
MS19968V1
MCO1/MCO2
MCU
Ethernet
MAC 10/100
Ethernet
PHY 10/100
PLL
HCLK
XT1
PHY_CLK    50 MHz
RMII_RXD[1:0]
RMII_CRX_DV
RMII_REF_CLK
RMII_TX_EN
RMII_TXD[1:0]
MDIO
MDC
HCLK(1)
STM32
OSC
50 MHz
TIM2
Timestamp
comparator
Timer
input
trigger
IEEE1588 PTP
RMII
= 7 pins
RMII + MDC
= 9 pins
MS19969V1
/2 or /20
synchronous
2.5 or 25 MHz
50 MHz
50 MHz

---

Application block diagrams
Figure 101. RMII with a 25 MHz crystal and PHY with PLL
1.
fHCLK must be greater than 25 MHz.
2.
The 25 MHz (PHY_CLK) must be derived directly from the HSE oscillator, before the PLL block.
MCU
Ethernet
MAC 10/100
Ethernet
PHY 10/100
PLL
HCLK
XT1
PHY_CLK   25 MHz
RMII_RXD[1:0]
RMII_CRX_DV
RMII_REF_CLK
RMII_TX_EN
RMII_TXD[1:0]
MDIO
MDC
HCLK(1)
STM32F
TIM2
Timestamp
comparator
Timer
input
trigger
IEEE1588 PTP
RMII
= 7 pins
RMII + MDC
= 9 pins
MS19970V1
/2 or /20
synchronous
2.5 or 25 MHz
50 MHz
XTAL
25 MHz
OSC
PLL
REF_CLK
MCO1/MCO2

---

Important security notice
Important security notice
The STMicroelectronics group of companies (ST) places a high value on product security,
which is why the ST product(s) identified in this documentation may be certified by various
security certification bodies and/or may implement our own security measures as set forth
herein. However, no level of security certification and/or built-in security measures can
guarantee that ST products are resistant to all forms of attacks. As such, it is the
responsibility of each of ST's customers to determine if the level of security provided in an
ST product meets the customer needs both in relation to the ST product alone, as well as
when combined with other components and/or software for the customer end product or
application. In particular, take note that:
•
ST products may have been certified by one or more security certification bodies, such
as Platform Security Architecture (www.psacertified.org) and/or Security Evaluation
standard for IoT Platforms (www.trustcb.com). For details concerning whether the ST
product(s) referenced herein have received security certification along with the level
and current status of such certification, either visit the relevant certification standards
website or go to the relevant product page on www.st.com for the most up to date
information. As the status and/or level of security certification for an ST product can
change from time to time, customers should re-check security certification status/level
as needed. If an ST product is not shown to be certified under a particular security
standard, customers should not assume it is certified.
•
Certification bodies have the right to evaluate, grant and revoke security certification in
relation to ST products. These certification bodies are therefore independently
responsible for granting or revoking security certification for an ST product, and ST
does not take any responsibility for mistakes, evaluations, assessments, testing, or
other activity carried out by the certification body with respect to any ST product.
•
Industry-based cryptographic algorithms (such as AES, DES, or MD5) and other open
standard technologies which may be used in conjunction with an ST product are based
on standards which were not developed by ST. ST does not take responsibility for any
flaws in such cryptographic algorithms or open technologies or for any methods which
have been or may be developed to bypass, decrypt or crack such algorithms or
technologies.
•
While robust security testing may be done, no level of certification can absolutely
guarantee protections against all attacks, including, for example, against advanced
attacks which have not been tested for, against new or unidentified forms of attack, or
against any form of attack when using an ST product outside of its specification or
intended use, or in conjunction with other components or software which are used by
customer to create their end product or application. ST is not responsible for resistance
against such attacks. As such, regardless of the incorporated security features and/or
any information or support that may be provided by ST, each customer is solely
responsible for determining if the level of attacks tested for meets their needs, both in
relation to the ST product alone and when incorporated into a customer end product or
application.
•
All security features of ST products (inclusive of any hardware, software,
documentation, and the like), including but not limited to any enhanced security
features added by ST, are provided on an "AS IS" BASIS. AS SUCH, TO THE EXTENT
PERMITTED BY APPLICABLE LAW, ST DISCLAIMS ALL WARRANTIES, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, unless the
applicable written and signed contract terms specifically provide otherwise.

---

Table 125. Document revision history
Date
Changes
19-Mar-2013
Initial release.
10-Sep-2013
Added STM32F429xx part numbers and related informations.
STM32F427xx part numbers:
Replaced FSMC by FMC added Chrom-ART Accelerator and SAI
interface.
Increased core, timer, GPIOs, SPI maximum frequencies
Updated Figure 8.Updated Figure 9.
Removed note in Section ·: Standby mode.
Updated Figure 18.
Updated Table 10: STM32F437xx and STM32F439xx pin and ball
definitions and Table 12: STM32F437xx and STM32F439xx alternate
function mapping..
Modified Figure 19: Memory map.
Updated Table 17: General operating conditions, Table 18: Limitations
depending on the operating power supply range. Removed note 1 in
Table 22: reset and power control block characteristics. Added Table
23: Over-drive switching characteristics.
Updated Section : Typical and maximum current consumption, Table
34: Switching output I/O current consumption, Table 35: Peripheral
current consumption and Section : On-chip peripheral current
consumption.
Updated Table 36: Low-power mode wakeup timings.
Modified Section : High-speed external user clock generated from an
external source, Section : Low-speed external user clock generated
from an external source, and Section 6.3.10: Internal clock source
characteristics.
Updated Table 43: Main PLL characteristics and Table 45: PLLISAI
(audio and LCD-TFT PLL) characteristics.
Updated Table 52: EMI characteristics.
Updated Table 57: Output voltage characteristics and Table 58: I/O AC
characteristics.
Updated Table 60: TIMx characteristics, Table 61: I2C characteristics,
Table 62: SPI dynamic characteristics, Section : SAI characteristics.
Updated Table 102: SDRAM read timings and Table 104: SDRAM write
timings.

---

24-Jan-2014
.Added STM32F429xE part numbers featuring 512 Mbytes of Flash
memory and UFBGA169 package.
Added LPSDR SDRAM.
Changed INTN into INTR in Figure 4: STM32F437xx and
STM32F439xx block diagram.
Added note 4 in Table 2: STM32F427xx and STM32F429xx features
and peripheral counts.
Updated Section 3.15: Boot modes.
Updated for PA4 and PA5 in Table 10: STM32F437xx and
STM32F439xx pin and ball definitions.
Added VIN for BOOT0 pins in Table 14: Voltage characteristics.
Updated Note 6., added Note 1.,and updated maximum VIN for B pins
in Table 17: General operating conditions.
Updated maximum Flash memory access frequency with wait states
for VDD =1.8 to 2.1 V in Table 18: Limitations depending on the
operating power supply range.
Updated Table 24: Typical and maximum current consumption in Run
mode, code with data processing   running from Flash memory (ART
accelerator enabled except prefetch) or RAM and Table 25: Typical and
maximum current consumption in Run mode, code with data
processing   running from Flash memory (ART accelerator disabled).
Updated Table 30: Typical current consumption in Run mode, code
with data processing running from   Flash memory or RAM, regulator
ON (ART accelerator enabled except prefetch),  VDD=1.7 V, Table 31:
Typical current consumption in Run mode, code with data processing
running   from Flash memory, regulator OFF (ART accelerator enabled
except prefetch), and Table 32: Typical current consumption in Sleep
mode, regulator ON, VDD=1.7 V.
Updated Table 57: Output voltage characteristics.
Updated Table 58: I/O AC characteristics. Added Figure 35.
Updated th(SDA), tr(SDA) and tr(SCL) and added tSP in Table 61: I2C
characteristics.
Updated fSCK in Table 62: SPI dynamic characteristics.
Updated Table 70: Dynamic characteristics: USB ULPI.
Updated Section 6.3.26: FMC characteristics conditions. Updated
Figure 73: SDRAM read access waveforms (CL = 1) and Figure 74:
SDRAM write access waveforms. Added Table 103: LPSDR SDRAM
read timings and Table 105: LPSDR SDRAM write timings. Updated
Table 102: SDRAM read timings and Table 104: SDRAM write timings
and added note 2.Table 108: Dynamic characteristics: SD / MMC
characteristics
Table 125. Document revision history
Date
Changes

---

24-Apr-2014
In the whole document, minimum supply voltage changed to 1.7 V
when external power supply supervisor is used.
Added DCMI_VSYNC alternate function on PG9 and updated note 6.
in Table 10: STM32F437xx and STM32F439xx pin and ball definitions
and Table 12: STM32F437xx and STM32F439xx alternate function
mapping. Added note 2.belowFigure 16: STM32F43x UFBGA169
ballout.
Changed SVGA (800x600) into XGA1024x768) on cover page and in
Section 3.10: LCD-TFT controller (available only on STM32F439xx).
Updated Section 3.18.2: Regulator OFF.
Updated signal corresponding to pin L5 in Figure 12: STM32F43x
WLCSP143 ballout.
Added ACCHSE in Table 39: HSE 4-26 MHz oscillator characteristics
and ACCLSE in Table 40: LSE oscillator characteristics (fLSE = 32.768
kHz).
Updated Table 53: ESD absolute maximum ratings.
Updated VIH in Table 56: I/O static characteristics. Added condition
VDD>1.7 V in Table 58: I/O AC characteristics.
Updated conditions in Table 62: SPI dynamic characteristics.
Added ZDRV in Table 67: USB OTG full speed electrical characteristics
Removed note 3 in Table 80: Temperature sensor characteristics.
Added Figure 82: LQFP100 marking example (package top view),
Figure 85: WLCSP143 marking example (package top view), Figure
88: LQFP144 marking example (package top view), Figure 91:
LQFP176 marking (package top view), Figure 94: LQFP208 marking
example (package top view), Figure 97: UFBGA169 marking example
(package top view) and Figure 100: UFBGA176+25 marking example
(package top view).
Added Appendix A: Recommendations when using internal reset OFF.
Removed Internal reset OFF hardware connection appendix.
Table 125. Document revision history
Date
Changes

---

19-Feb-2015
Update SPI/IS2 in Table 2: STM32F427xx and STM32F429xx features
and peripheral counts.
Updated LQFP208 in Table 4: Regulator ON/OFF and internal reset
ON/OFF availability.
Updated Figure 19: Memory map.
Changed PLS[2:0]=101 (falling edge) maximum value in Table 22:
reset and power control block characteristics.
Updated current consumption with all peripherals disabled in Table 24:
Typical and maximum current consumption in Run mode, code with
data processing   running from Flash memory (ART accelerator
enabled except prefetch) or RAM. Updated note 1. in Table 28: Typical
and maximum current consumptions in Standby mode.
Updated tWUSTOP in Table 36: Low-power mode wakeup timings.
Updated ESD standards and Table 53: ESD absolute maximum
ratings.
Updated Table 56: I/O static characteristics.
Section : I2C interface characteristics: updated section introduction,
removed Table I2C characteristics, Figure I2C bus AC waveforms and
measurement circuit and Table SCL frequency; added Table 61: I2C
analog filter characteristics.
Updated measurement conditions in Table 62: SPI dynamic
characteristics.
Updated Figure 51: Typical connection diagram using the ADC.
Updated Section : Device marking for LQFP100.
Updated Figure 83: WLCSP143 - 143-ball, 4.521x 5.547 mm, 0.4 mm
pitch wafer level chip scale package outline and Table 111: WLCSP143
- 143-ball, 4.521x 5.547 mm, 0.4 mm pitch wafer level chip scale
package mechanical data; added Figure 84: WLCSP143 - 143-ball,
4.521x 5.547 mm, 0.4 mm pitch wafer level chip scale recommended
footprint and Table 112: WLCSP143 recommended PCB design rules
(0.4 mm pitch). Updated Figure 85: WLCSP143 marking example
(package top view) and related note. Updated Section : Device
marking for WLCSP143.
Updated Section : Device marking for LQFP144.
Updated Section : Device marking for LQFP176.
Updated Figure 92: LQFP208 - 208-pin, 28 x 28 mm low-profile quad
flat package outline; Updated Section : Device marking for LQFP208.
Modified UFBGA169 pitch, updated Figure 95: UFBGA169 - 169-ball 7
x 7 mm 0.50 mm pitch, ultra fine pitch ball grid array package outline
and Table 116: UFBGA169 - 169-ball 7 x 7 mm 0.50 mm pitch, ultra
fine pitch ball grid array package mechanical data; updated Section :
Device marking for LQFP208.
updated Section : Device marking for UFBGA169, Section : Device
marking for UFBGA176+25 and Section : Device marking for
TFBGA176.
Updated Z pin count in Table  : .
?ǣ
Table 125. Document revision history
Date
Changes

---

17-Sep-2015
Updated notes related to the minimum and maximum values
guaranteed by design, characterization or test in production.
Updated IDD_STOP_UDM in Table 27: Typical and maximum current
consumptions in Stop mode.
Removed note related to tests in production in Table 24: Typical and
maximum current consumption in Run mode, code with data
processing   running from Flash memory (ART accelerator enabled
except prefetch) or RAM and Table 26: Typical and maximum current
consumption in Sleep mode.
Updated Table 41: HSI oscillator characteristics. Figure 31 renamed
ACCHSI accuracy versus temperature and updated.
Updated Figure 38: SPI timing diagram - slave mode and CPHA = 0.
Updated Section : Ethernet characteristics.
Updated Table 43: Main PLL characteristics, Table 44: PLLI2S (audio
PLL) characteristics and Table 45: PLLISAI (audio and LCD-TFT PLL)
characteristics.
Removed note 1 in Table 75: ADC static accuracy at fADC = 18 MHz,
Table 76: ADC static accuracy at fADC = 30 MHz and Table 77: ADC
static accuracy at fADC = 36 MHz.
Updated td(SDCLKL _Data) and th(SDCLKL _Data) in Table 104:
SDRAM write timings.
Added Figure 96: UFBGA169 - 169-ball, 7 x 7 mm, 0.50 mm pitch, ultra
fine pitch ball grid array recommended footprint and Table 117:
UFBGA169 recommended PCB design rules (0.5 mm pitch BGA).
Added Figure 99: UFBGA176+25-ball, 10 x 10 mm, 0.65 mm pitch,
ultra fine pitch ball grid array package recommended footprint and
Table 119: UFBGA176+25 recommended PCB design rules (0.65 mm
pitch BGA).
30-Nov-2015
Updated |VSSX -VSS| in Table 14: Voltage characteristics to add
VREF-.
Updated td(TXEN) and td(TXD) minimum value in Table 72: Dynamics
characteristics: Ethernet MAC signals for RMII and Table 73: Dynamics
characteristics: Ethernet MAC signals for MII.
Added VREF- in Table 74: ADC characteristics.
Added A1 minimum and maximum values in Table 111: WLCSP143 -
143-ball, 4.521x 5.547 mm, 0.4 mm pitch wafer level chip scale
package mechanical data. Updated Figure 86: LQFP144-144-pin, 20 x
20 mm low-profile quad flat package outline.
Updated Figure 98: UFBGA176+25 - ball 10 x 10 mm, 0.65 mm pitch
ultra thin fine pitch  ball grid array package outline and Table 118:
UFBGA176+25 - ball, 10 x 10 mm, 0.65 mm pitch, ultra fine pitch ball
grid array package mechanical data. Updated Figure 101: TFBGA216 -
216 ball 13 × 13 mm 0.8 mm pitch thin fine pitch ball grid array package
outline and Table 120: TFBGA216 - 216 ball 13 × 13 mm 0.8 mm pitch
thin fine pitch ball grid array package mechanical data.
21-Jan-2016
Updated Figure 22: Power supply scheme.
Added td(TXD) values corresponding to 1.71 V < VDD < 3.6 V in Table
72: Dynamics characteristics: Ethernet MAC signals for RMII.
Table 125. Document revision history
Date
Changes

---

18-Jul-2016
Updated Figure 1: Compatible board design
STM32F10xx/STM32F2xx/STM32F4xx for LQFP100 package.
Added mission profile compliance with JEDEC JESD47 in Section 6.2:
Absolute maximum ratings.
Changed Figure 31 HSI deviation versus temperature to ACCHSI
versus temperature.
Updated RLOAD in Table 85: DAC characteristics.
Added note 2. related to the position of the 0.1 µF capacitor below
Figure 37: Recommended NRST pin protection.
Updated Figure 40: SPI timing diagram - master mode.
Added reference to optional marking or inset/upset marks in all
package device marking sections. Updated Figure 85: WLCSP143
marking example (package top view), Figure 88: LQFP144 marking
example (package top view), Figure 91: LQFP176 marking (package
top view), Figure 94: LQFP208 marking example (package top view).
Updated Figure 98: UFBGA176+25 - ball 10 x 10 mm, 0.65 mm pitch
ultra thin fine pitch  ball grid array package outline and Table 118:
UFBGA176+25 - ball, 10 x 10 mm, 0.65 mm pitch, ultra fine pitch ball
grid array package mechanical data.
19-Jan-2018
Updated Arm wordmark and added Arm logo in Section 2: Description.
Updated LDC-TFT feature on cover page.
Updated Table 24: Typical and maximum current consumption in Run
mode, code with data processing   running from Flash memory (ART
accelerator enabled except prefetch) or RAM and Table 26: Typical and
maximum current consumption in Sleep mode.
RADC minimum value added in Table 74: ADC characteristics.
LTDC clock output frequency changed to 83 MHz in Table 107: LTDC
characteristics.
Table 125. Document revision history
Date
Changes

---

24-Oct-2024
General datasheet update to include minor terminology updates.
Updated the datasheet cover page.
General update of the Section 7: Package information.
Updated the figure Figure 19: Memory map.
Edited the footnote in Table 41: HSI oscillator characteristics.
Updated the Table 22: Reset and power control block characteristics.
Updated the Section 6.2: Absolute maximum ratings.
Updated the Section : I/O system current consumption.
Updated the Section 3.36: True random number generator (RNG).
Updated the Figure 36: I/O AC characteristics definition.
Updated the Figure 51: Typical connection diagram when using the
ADC with FT/TT pins featuring the analog switch function.
Updated the Section : Electromagnetic Interference (EMI).
Updated the Figure 38: SPI timing diagram - slave mode and CPHA =
0, the Figure 39: SPI timing diagram - slave mode and CPHA = 1, and
the Figure 40: SPI timing diagram - master mode.
Updated the Figure 69: NAND controller waveforms for read access
and Figure 70: NAND controller waveforms for write access.
Updated the Table 14: Voltage characteristics.
05-May-2025
Corrected Table 57: I/O static characteristics.
23-Feb-2026
Updated Arm legal notice in Section 1: Introduction.
Updated VIN max. in Table 14: Voltage characteristics.
Updated terminology in Table 60: NRST pin characteristics.
Table 125. Document revision history
Date
Changes

---

IMPORTANT NOTICE – READ CAREFULLY
STMicroelectronics NV and its subsidiaries (“ST”) reserve the right to make changes, corrections, enhancements, modifications, and
improvements to ST products and/or to this document at any time without notice.
In the event of any conflict between the provisions of this document and the provisions of any contractual arrangement in force between the
purchasers and ST, the provisions of such contractual arrangement shall prevail.
The purchasers should obtain the latest relevant information on ST products before placing orders. ST products are sold pursuant to ST’s
terms and conditions of sale in place at the time of order acknowledgment.
The purchasers are solely responsible for the choice, selection, and use of ST products and ST assumes no liability for application
assistance or the design of the purchasers’ products.
No license, express or implied, to any intellectual property right is granted by ST herein.
Resale of ST products with provisions different from the information set forth herein shall void any warranty granted by ST for such product.
If the purchasers identify an ST product that meets their functional and performance requirements but that is not designated for the
purchasers' market segment, the purchasers shall contact ST for more information.
ST and the ST logo are trademarks of ST. For additional information about ST trademarks, refer to www.st.com/trademarks. All other product
or service names are the property of their respective owners.
Information in this document supersedes and replaces information previously supplied in any prior versions of this document.
© 2026 STMicroelectronics – All rights reserved
