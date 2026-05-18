# STM32F405xx STM32F407xx


---

This is information on a product in full production.
February 2026
STM32F405xx STM32F407xx
Arm® Cortex®-M4 32b MCU+FPU, 210DMIPS, up to 1MB flash/192+4KB RAM, USB
OTG HS/FS, Ethernet, 17 TIMs, 3 ADCs, 15 comm. interfaces, and camera
Datasheet - production data
Features
•
Includes ST state-of-the-art patented
technology
•
Core: Arm® 32-bit Cortex®-M4 CPU with FPU,
Adaptive real-time accelerator (ART
Accelerator) allowing 0-wait state execution
from flash memory, frequency up to 168 MHz,
memory protection unit, 210 DMIPS/
1.25 DMIPS/MHz (Dhrystone 2.1), and DSP
instructions
•
Memories
–
Up to 1 Mbyte of flash memory
–
Up to 192+4 Kbytes of SRAM including 64-
Kbyte of CCM (core coupled memory) data
RAM
–
512 bytes of OTP memory
–
Flexible static memory controller
supporting Compact Flash, SRAM,
PSRAM, NOR and NAND memories
•
LCD parallel interface, 8080/6800 modes
•
Clock, reset, and supply management
–
1.8 V to 3.6 V application supply and I/Os
–
POR, PDR, PVD and BOR
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
Low-power operation
–
Sleep, Stop, and Standby modes
–
VBAT supply for RTC, 20×32-bit backup
registers + optional 4 KB backup SRAM
•
3×12-bit, 2.4 MSPS A/D converters: up to 24
channels and 7.2 MSPS in triple interleaved
mode
•
2×12-bit D/A converters
•
General-purpose DMA: 16-stream DMA
controller with FIFOs and burst support
•
Up to 17 timers: up to twelve 16-bit and two 32-
bit timers up to 168 MHz, each with up to 4
IC/OC/PWM or pulse counter and quadrature
(incremental) encoder input
•
Debug mode
–
Serial wire debug (SWD) & JTAG
interfaces
–
Cortex-M4 Embedded Trace Macrocell™
•
Up to 140 I/O ports with interrupt capability
–
Up to 136 fast I/Os up to 84 MHz
–
Up to 138 5 V-tolerant I/Os
•
Up to 15 communication interfaces
–
Up to 3 × I2C interfaces (SMBus/PMBus)
–
Up to 4 USARTs/2 UARTs (10.5 Mbit/s, ISO
7816 interface, LIN, IrDA, modem control)
–
Up to 3 SPIs (42 Mbits/s), 2 with muxed
full-duplex I2S to achieve audio class
accuracy via internal audio PLL or external
clock
–
2 × CAN interfaces (2.0B Active)
–
SDIO interface
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
LQFP64 (10 × 10 mm)
LQFP100 (14 × 14 mm)
LQFP144 (20 × 20 mm)
UFBGA176
(10 × 10 mm)
LQFP176 (24 × 24 mm)
WLCSP90
(4.223x3.969 mm)
www.st.com

---

•
8- to 14-bit parallel camera interface up to
54 Mbytes/s
•
True random number generator
•
CRC calculation unit
•
96-bit unique ID
•
RTC: subsecond accuracy, hardware calendar
•
All packages are ECOPACK2 compliant
Table 1. Device summary
Reference
Part number
STM32F405xx
STM32F405RG, STM32F405VG, STM32F405ZG, STM32F405OG, STM32F405OE
STM32F407xx
STM32F407VG, STM32F407IG, STM32F407ZG, STM32F407VE, STM32F407ZE,
STM32F407IE

---

Contents
Contents
Introduction . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 13
Description . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 14
2.1
Full compatibility throughout the family  . . . . . . . . . . . . . . . . . . . . . . . . . . 17
Functional overview  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 20
3.0.1
Arm® Cortex®-M4 core with FPU and embedded Flash and SRAM . . . 21
3.0.2
Adaptive real-time memory accelerator (ART Accelerator)  . . . . . . . . . . 21
3.0.3
Memory protection unit  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 21
3.0.4
Embedded flash memory . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 21
3.0.5
CRC (cyclic redundancy check) calculation unit  . . . . . . . . . . . . . . . . . . 22
3.0.6
Embedded SRAM  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 22
3.0.7
Multi-AHB bus matrix . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 22
3.0.8
DMA controller (DMA)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 23
3.0.9
Flexible static memory controller (FSMC)  . . . . . . . . . . . . . . . . . . . . . . . 24
3.0.10
Nested vectored interrupt controller (NVIC) . . . . . . . . . . . . . . . . . . . . . . 24
3.0.11
External interrupt/event controller (EXTI)  . . . . . . . . . . . . . . . . . . . . . . . 24
3.0.12
Clocks and startup . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 24
3.0.13
Boot modes  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 25
3.0.14
Power supply schemes  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 25
3.0.15
Power supply supervisor  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 25
3.0.16
Voltage regulator  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 27
3.0.17
Regulator ON/OFF and internal reset ON/OFF availability  . . . . . . . . . . 30
3.0.18
Real-time clock (RTC), backup SRAM and backup registers  . . . . . . . . 30
3.0.19
Low-power modes . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 31
3.0.20
VBAT operation . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 32
3.0.21
Timers and watchdogs . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 32
3.0.22
Inter-integrated circuit interface (I²C) . . . . . . . . . . . . . . . . . . . . . . . . . . . 35
3.0.23
Universal synchronous/asynchronous receiver transmitters (USART)  . 35
3.0.24
Serial peripheral interface (SPI) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 36
3.0.25
Inter-integrated sound (I2S) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 36
3.0.26
Audio PLL (PLLI2S) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 37
3.0.27
Secure digital input/output interface (SDIO)  . . . . . . . . . . . . . . . . . . . . . 37
3.0.28
Ethernet MAC interface with dedicated DMA and IEEE 1588 support  . 37

---

Contents
3.0.29
Controller area network (bxCAN) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 38
3.0.30
Universal serial bus on-the-go full-speed (OTG_FS) . . . . . . . . . . . . . . . 38
3.0.31
Universal serial bus on-the-go high-speed (OTG_HS)  . . . . . . . . . . . . . 39
3.0.32
Digital camera interface (DCMI)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 39
3.0.33
True random number generator (RNG) . . . . . . . . . . . . . . . . . . . . . . . . . 39
3.0.34
General-purpose input/outputs (GPIOs)  . . . . . . . . . . . . . . . . . . . . . . . . 39
3.0.35
Analog-to-digital converters (ADCs)  . . . . . . . . . . . . . . . . . . . . . . . . . . . 40
3.0.36
Temperature sensor . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 40
3.0.37
Digital-to-analog converter (DAC)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 40
3.0.38
Serial wire JTAG debug port (SWJ-DP) . . . . . . . . . . . . . . . . . . . . . . . . . 41
3.0.39
Embedded Trace Macrocell™ . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 41
Pinouts and pin description . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 42
Memory mapping . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 72
Electrical characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 77
6.1
Parameter conditions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 77
6.1.1
Minimum and maximum values . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 77
6.1.2
Typical values  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 77
6.1.3
Typical curves  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 77
6.1.4
Loading capacitor  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 77
6.1.5
Pin input voltage  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 77
6.1.6
Power supply scheme  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 78
6.1.7
Current consumption measurement  . . . . . . . . . . . . . . . . . . . . . . . . . . . 79
6.2
Absolute maximum ratings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 79
6.3
Operating conditions  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 80
6.3.1
General operating conditions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 80
6.3.2
VCAP_1/VCAP_2 external capacitor . . . . . . . . . . . . . . . . . . . . . . . . . . . 83
6.3.3
Operating conditions at power-up / power-down (regulator ON) . . . . . . 83
6.3.4
Operating conditions at power-up / power-down (regulator OFF) . . . . . 83
6.3.5
Embedded reset and power control block characteristics . . . . . . . . . . . 84
6.3.6
Supply current characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 85
6.3.7
Wakeup time from low-power mode  . . . . . . . . . . . . . . . . . . . . . . . . . . . 99
6.3.8
External clock source characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . 100
6.3.9
Internal clock source characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . 104
6.3.10
PLL characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 105

---

Contents
6.3.11
PLL spread spectrum clock generation (SSCG) characteristics  . . . . . 107
6.3.12
Memory characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 109
6.3.13
EMC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 111
6.3.14
Absolute maximum ratings (electrical sensitivity)  . . . . . . . . . . . . . . . . 113
6.3.15
I/O current injection characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . 114
6.3.16
I/O port characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 115
6.3.17
NRST pin characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 120
6.3.18
TIM timer characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 120
6.3.19
Communications interfaces . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 122
6.3.20
CAN (controller area network) interface  . . . . . . . . . . . . . . . . . . . . . . . 134
6.3.21
12-bit ADC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 134
6.3.22
Temperature sensor characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . 139
6.3.23
VBAT monitoring characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 140
6.3.24
Embedded reference voltage . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 140
6.3.25
DAC electrical characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 140
6.3.26
FSMC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 143
6.3.27
Camera interface (DCMI) timing specifications . . . . . . . . . . . . . . . . . . 161
6.3.28
SD/SDIO MMC card host interface (SDIO) characteristics  . . . . . . . . . 162
6.3.29
RTC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 163
Package information . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 164
7.1
Device marking . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 164
7.2
WLCSP90 package information . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 165
7.3
LQFP64 package information (5W)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . 168
7.4
LQFP100 package information (1L) . . . . . . . . . . . . . . . . . . . . . . . . . . . . 171
7.5
LQFP144 package information (1A) . . . . . . . . . . . . . . . . . . . . . . . . . . . . 174
7.6
UFBGA(176+25) package information (A0E7) . . . . . . . . . . . . . . . . . . . . 178
7.7
LQFP176 package information (1T) . . . . . . . . . . . . . . . . . . . . . . . . . . . . 180
7.8
Thermal characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 184
Ordering information  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 185
Appendix A
Application block diagrams . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 186
A.1
USB OTG full speed (FS) interface solutions . . . . . . . . . . . . . . . . . . . . . 186
A.2
USB OTG high speed (HS) interface solutions . . . . . . . . . . . . . . . . . . . . 188
A.3
Ethernet interface solutions. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 189

---

Contents
Important security notice . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 191

---

List of tables
List of tables
Table 1.
Device summary . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 2
Table 2.
STM32F405xx and STM32F407xx: features and peripheral counts. . . . . . . . . . . . . . . . . . 15
Table 3.
Regulator ON/OFF and internal reset ON/OFF availability. . . . . . . . . . . . . . . . . . . . . . . . . 30
Table 4.
Timer feature comparison. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 32
Table 5.
USART feature comparison . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 36
Table 6.
Legend/abbreviations used in the pinout table . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 47
Table 7.
STM32F40xxx pin and ball definitions  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 48
Table 8.
FSMC pin definition  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 60
Table 9.
Alternate function mapping . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 63
Table 10.
Register boundary addresses. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 73
Table 11.
Voltage characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 79
Table 12.
Current characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 80
Table 13.
Thermal characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 80
Table 14.
General operating conditions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 80
Table 15.
Limitations depending on the operating power supply range . . . . . . . . . . . . . . . . . . . . . . . 82
Table 16.
VCAP_1/VCAP_2 operating conditions  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 83
Table 17.
Operating conditions at power-up / power-down (regulator ON)  . . . . . . . . . . . . . . . . . . . . 83
Table 18.
Operating conditions at power-up / power-down (regulator OFF). . . . . . . . . . . . . . . . . . . . 83
Table 19.
Embedded reset and power control block characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . 84
Table 20.
Typical and maximum current consumption in Run mode, code with data processing
 running from flash memory (ART accelerator enabled) or RAM . . . . . . . . . . . . . . . . . . . . 86
Table 21.
Typical and maximum current consumption in Run mode, code with data processing
 running from flash memory (ART accelerator disabled)  . . . . . . . . . . . . . . . . . . . . . . . . . . 87
Table 22.
Typical and maximum current consumption in Sleep mode . . . . . . . . . . . . . . . . . . . . . . . . 90
Table 23.
Typical and maximum current consumptions in Stop mode . . . . . . . . . . . . . . . . . . . . . . . . 91
Table 24.
Typical and maximum current consumptions in Standby mode . . . . . . . . . . . . . . . . . . . . . 91
Table 25.
Typical and maximum current consumptions in VBAT mode. . . . . . . . . . . . . . . . . . . . . . . . 92
Table 26.
Typical current consumption in Run mode, code with data processing
 running from flash memory, regulator ON (ART accelerator enabled
except prefetch), VDD = 1.8 V. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 94
Table 27.
Switching output I/O current consumption  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 96
Table 28.
Peripheral current consumption . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 97
Table 29.
Low-power mode wakeup timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 100
Table 30.
High-speed external user clock characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 100
Table 31.
Low-speed external user clock characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 101
Table 32.
HSE 4-26 MHz oscillator characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 102
Table 33.
LSE oscillator characteristics (fLSE = 32.768 kHz) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 103
Table 34.
HSI oscillator characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 104
Table 35.
LSI oscillator characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 104
Table 36.
Main PLL characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 105
Table 37.
PLLI2S (audio PLL) characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 106
Table 38.
SSCG parameters constraint . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 107
Table 39.
Flash memory characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 109
Table 40.
Flash memory programming. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 109
Table 41.
Flash memory programming with VPP . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 111
Table 42.
Flash memory endurance and data retention . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 111
Table 43.
EMS characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 112
Table 44.
EMI characteristics for fHSE = 25 MH and fCPU = 168 MHz . . . . . . . . . . . . . . . . . . . . . . 113

---

List of tables
Table 45.
ESD absolute maximum ratings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 113
Table 46.
Electrical sensitivities . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 114
Table 47.
I/O current injection susceptibility . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 115
Table 48.
I/O static characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 115
Table 49.
Output voltage characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 117
Table 50.
I/O AC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 118
Table 51.
NRST pin characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 120
Table 52.
Characteristics of TIMx connected to the APB1 domain  . . . . . . . . . . . . . . . . . . . . . . . . . 121
Table 53.
Characteristics of TIMx connected to the APB2 domain  . . . . . . . . . . . . . . . . . . . . . . . . . 122
Table 54.
I2C analog filter characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 122
Table 55.
SPI dynamic characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 123
Table 56.
I2S dynamic characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 127
Table 57.
USB OTG FS startup time  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 129
Table 58.
USB OTG FS DC electrical characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 129
Table 59.
USB OTG FS electrical characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 130
Table 60.
USB HS DC electrical characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 130
Table 61.
USB HS clock timing parameters . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 130
Table 62.
ULPI timing . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 131
Table 63.
Ethernet DC electrical characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 132
Table 64.
Dynamic characteristics: Ethernet MAC signals for SMI. . . . . . . . . . . . . . . . . . . . . . . . . . 132
Table 65.
Dynamic characteristics: Ethernet MAC signals for RMII . . . . . . . . . . . . . . . . . . . . . . . . . 133
Table 66.
Dynamic characteristics: Ethernet MAC signals for MII . . . . . . . . . . . . . . . . . . . . . . . . . . 134
Table 67.
ADC characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 134
Table 68.
ADC accuracy at fADC = 30 MHz  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 136
Table 69.
Temperature sensor characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 139
Table 70.
Temperature sensor calibration values. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 139
Table 71.
VBAT monitoring characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 140
Table 72.
Embedded internal reference voltage. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 140
Table 73.
Internal reference voltage calibration values  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 140
Table 74.
DAC characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 140
Table 75.
Asynchronous non-multiplexed SRAM/PSRAM/NOR read timings . . . . . . . . . . . . . . . . . 144
Table 76.
Asynchronous non-multiplexed SRAM/PSRAM/NOR write timings . . . . . . . . . . . . . . . . . 145
Table 77.
Asynchronous multiplexed PSRAM/NOR read timings. . . . . . . . . . . . . . . . . . . . . . . . . . . 146
Table 78.
Asynchronous multiplexed PSRAM/NOR write timings  . . . . . . . . . . . . . . . . . . . . . . . . . . 147
Table 79.
Synchronous multiplexed NOR/PSRAM read timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . 149
Table 80.
Synchronous multiplexed PSRAM write timings. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 150
Table 81.
Synchronous non-multiplexed NOR/PSRAM read timings . . . . . . . . . . . . . . . . . . . . . . . . 152
Table 82.
Synchronous non-multiplexed PSRAM write timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . 153
Table 83.
Switching characteristics for PC Card/CF read and write cycles
 in attribute/common space. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 158
Table 84.
Switching characteristics for PC Card/CF read and write cycles
 in I/O space . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 159
Table 85.
Switching characteristics for NAND Flash read cycles . . . . . . . . . . . . . . . . . . . . . . . . . . . 160
Table 86.
Switching characteristics for NAND Flash write cycles. . . . . . . . . . . . . . . . . . . . . . . . . . . 161
Table 87.
DCMI characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 161
Table 88.
Dynamic characteristics: SD/MMC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 163
Table 89.
RTC characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 163
Table 90.
WLCSP90 - 4.223 x 3.969 mm, 0.400 mm pitch wafer level chip scale
package mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 166
Table 91.
WLCSP90 recommended PCB design rules  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 167
Table 92.
LQFP64 - Mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 169
Table 93.
LQFP100 - Mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 172

---

List of tables
Table 94.
LQFP144 - Mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 175
Table 95.
UFBGA(176+25) - Mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 178
Table 96.
UFBGA(176+25) - Example of PCB design rules (0.65 mm pitch BGA)  . . . . . . . . . . . . . 179
Table 97.
LQFP176 - Mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 181
Table 98.
Package thermal characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 184
Table 99.

---

List of figures
List of figures
Figure 1.
Compatible board design between STM32F10xx/STM32F40xxx for LQFP64 . . . . . . . . . . 17
Figure 2.
Compatible board design STM32F10xx/STM32F2/STM32F40xxx
for LQFP100 package. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 18
Figure 3.
Compatible board design between STM32F10xx/STM32F2/STM32F40xxx
for LQFP144 package. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 18
Figure 4.
Compatible board design between STM32F2 and STM32F40xxx
 for LQFP176 and BGA176 packages  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 19
Figure 5.
STM32F40xxx block diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 20
Figure 6.
Multi-AHB matrix. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 23
Figure 7.
Power supply supervisor interconnection with internal reset OFF . . . . . . . . . . . . . . . . . . . 26
Figure 8.
PDR_ON and NRST control with internal reset OFF . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 27
Figure 9.
Regulator OFF  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 28
Figure 10.
Startup in regulator OFF mode: slow VDD slope
- power-down reset risen after VCAP_1/VCAP_2 stabilization . . . . . . . . . . . . . . . . . . . . . . . . 29
Figure 11.
Startup in regulator OFF mode: fast VDD slope
- power-down reset risen before VCAP_1/VCAP_2 stabilization  . . . . . . . . . . . . . . . . . . . . . . 30
Figure 12.
STM32F40xxx LQFP64 pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 42
Figure 13.
STM32F40xxx LQFP100 pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 43
Figure 14.
STM32F40xxx LQFP144 pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 44
Figure 15.
STM32F40xxx LQFP176 pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 45
Figure 16.
STM32F40xxx UFBGA176 ballout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 46
Figure 17.
STM32F40xxx WLCSP90 ballout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 47
Figure 18.
STM32F40xxx memory map. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 72
Figure 19.
Pin loading conditions. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 77
Figure 20.
Pin input voltage . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 77
Figure 21.
Power supply scheme  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 78
Figure 22.
Current consumption measurement scheme . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 79
Figure 23.
External capacitor CEXT . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 83
Figure 24.
Typical current consumption versus temperature, Run mode, code with data
processing running from Flash (ART accelerator ON) or RAM, and peripherals OFF . . . . 88
Figure 25.
Typical current consumption versus temperature, Run mode, code with data
processing running from Flash (ART accelerator ON) or RAM, and peripherals ON . . . . . 88
Figure 26.
Typical current consumption versus temperature, Run mode, code with data
processing running from Flash (ART accelerator OFF) or RAM, and peripherals OFF . . . 89
Figure 27.
Typical current consumption versus temperature, Run mode, code with data
processing running from Flash (ART accelerator OFF) or RAM, and peripherals ON . . . . 89
Figure 28.
Typical VBAT current consumption (LSE and RTC ON/backup RAM OFF)  . . . . . . . . . . . . 92
Figure 29.
Typical VBAT current consumption (LSE and RTC ON/backup RAM ON) . . . . . . . . . . . . . 93
Figure 30.
High-speed external clock source AC timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . 101
Figure 31.
Low-speed external clock source AC timing diagram. . . . . . . . . . . . . . . . . . . . . . . . . . . . 102
Figure 32.
Typical application with an 8 MHz crystal . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 103
Figure 33.
Typical application with a 32.768 kHz crystal . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 104
Figure 34.
ACCLSI versus temperature . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 105
Figure 35.
PLL output clock waveforms in center spread mode  . . . . . . . . . . . . . . . . . . . . . . . . . . . . 108
Figure 36.
PLL output clock waveforms in down spread mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 109
Figure 37.
I/O AC characteristics definition . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 119
Figure 38.
Recommended NRST pin protection  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 120
Figure 39.
SPI timing diagram - slave mode and CPHA = 0 . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 125

---

List of figures
Figure 40.
SPI timing diagram - slave mode and CPHA = 1 . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 125
Figure 41.
SPI timing diagram - master mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 126
Figure 42.
I2S slave timing diagram (Philips protocol)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 128
Figure 43.
I2S master timing diagram (Philips protocol)(1). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 128
Figure 44.
USB OTG FS timings: definition of data signal rise and fall time . . . . . . . . . . . . . . . . . . . 130
Figure 45.
ULPI timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 131
Figure 46.
Ethernet SMI timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 132
Figure 47.
Ethernet RMII timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 133
Figure 48.
Ethernet MII timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 133
Figure 49.
ADC accuracy characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 136
Figure 50.
Typical connection diagram when using the ADC with FT/TT pins featuring
analog switch function  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 137
Figure 51.
Power supply and reference decoupling (VREF+ not connected to VDDA). . . . . . . . . . . . . 138
Figure 52.
Power supply and reference decoupling (VREF+ connected to VDDA). . . . . . . . . . . . . . . . 139
Figure 53.
12-bit buffered /non-buffered DAC . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 143
Figure 54.
Asynchronous non-multiplexed SRAM/PSRAM/NOR read waveforms . . . . . . . . . . . . . . 144
Figure 55.
Asynchronous non-multiplexed SRAM/PSRAM/NOR write waveforms . . . . . . . . . . . . . . 145
Figure 56.
Asynchronous multiplexed PSRAM/NOR read waveforms. . . . . . . . . . . . . . . . . . . . . . . . 146
Figure 57.
Asynchronous multiplexed PSRAM/NOR write waveforms  . . . . . . . . . . . . . . . . . . . . . . . 147
Figure 58.
Synchronous multiplexed NOR/PSRAM read timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . 148
Figure 59.
Synchronous multiplexed PSRAM write timings. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 150
Figure 60.
Synchronous non-multiplexed NOR/PSRAM read timings . . . . . . . . . . . . . . . . . . . . . . . . 151
Figure 61.
Synchronous non-multiplexed PSRAM write timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . 153
Figure 62.
PC Card/CompactFlash controller waveforms for common memory read access . . . . . . 154
Figure 63.
PC Card/CompactFlash controller waveforms for common memory write access. . . . . . 155
Figure 64.
PC Card/CompactFlash controller waveforms for attribute memory read
access. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 156
Figure 65.
PC Card/CompactFlash controller waveforms for attribute memory write
access. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 157
Figure 66.
PC Card/CompactFlash controller waveforms for I/O space read access . . . . . . . . . . . . 157
Figure 67.
PC Card/CompactFlash controller waveforms for I/O space write access . . . . . . . . . . . . 158
Figure 68.
NAND controller waveforms for read access . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 160
Figure 69.
NAND controller waveforms for write access . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 160
Figure 70.
DCMI timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 161
Figure 71.
SDIO high-speed mode  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 162
Figure 72.
SD default mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 163
Figure 73.
WLCSP90 - 4.223 x 3.969 mm, 0.400 mm pitch wafer level chip scale
package outline. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 165
Figure 74.
WLCSP90 - 4.223 x 3.969 mm, 0.400 mm pitch wafer level chip scale
package recommended footprint  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 166
Figure 75.
WLCSP90 marking example (package top view) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 167
Figure 76.
LQFP64 - Outline(15). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 168
Figure 77.
LQFP100 - Outline(15). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 171
Figure 78.
LQFP100 - Footprint example  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 173
Figure 79.
LQFP144 - Outline(15). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 174
Figure 80.
LQFP144 - Footprint example  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 177
Figure 81.
UFBGA(176+25) - Outline  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 178
Figure 82.
UFBGA(176+25) - Footprint example. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 179
Figure 83.
LQFP176 - Outline(15). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 180
Figure 84.
LQFP176 - Footprint example  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 183
Figure 85.
USB controller configured as peripheral-only and used
in Full speed mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 186

---

List of figures
Figure 86.
USB controller configured as host-only and used in full speed mode. . . . . . . . . . . . . . . . 186
Figure 87.
USB controller configured in dual mode and used in full speed mode . . . . . . . . . . . . . . . 187
Figure 88.
USB controller configured as peripheral, host, or dual-mode
and used in high speed mode. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 188
Figure 89.
MII mode using a 25 MHz crystal . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 189
Figure 90.
RMII with a 50 MHz oscillator . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 189
Figure 91.
RMII with a 25 MHz crystal and PHY with PLL. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 190

---

Introduction
Introduction
This datasheet provides the description of the STM32F405xx and STM32F407xx lines of
microcontrollers. For more details on the whole STMicroelectronics STM32™ family, refer to
Section 2.1: Full compatibility throughout the family.
The STM32F405xx and STM32F407xx datasheet should be read in conjunction with the
STM32F4xx reference manual which is available from the STMicroelectronics website
www.st.com.
For information on the device errata with respect to the datasheet and reference manual,
refer to the STM32F405xx and STM32F407xx errata sheet (ES0182), which is available
from the STMicroelectronics website www.st.com.
For information on the Arm® Cortex®-M4 core, refer to the Cortex®-M4 programming
manual (PM0214) available from www.st.com.
Note:
Arm and Cortex are registered trademarks of Arm Limited (or its subsidiaries or affiliates) in
the US and/or elsewhere.
The Arm word and logo are trademarks of Arm Limited (or its subsidiaries) in the US and/or
elsewhere. All rights reserved.

---

Description
Description
The STM32F405xx and STM32F407xx family is based on the high-performance Arm®
Cortex®-M4 32-bit RISC core operating at a frequency of up to 168 MHz. The Cortex®-M4
core features a floating-point unit (FPU) single precision which supports all Arm single-
precision data-processing instructions and data types. It also implements a full set of DSP
instructions and a memory protection unit (MPU) which enhances application security.
The STM32F405xx and STM32F407xx family incorporates high-speed embedded
memories (flash memory up to 1 Mbyte, up to 192 Kbytes of SRAM), up to 4 Kbytes of
backup SRAM, and an extensive range of enhanced I/Os and peripherals connected to two
APB buses, three AHB buses and a 32-bit multi-AHB bus matrix.
All devices offer three 12-bit ADCs, two DACs, a low-power RTC, 12 general-purpose 16-bit
timers including two PWM timers for motor control, two general-purpose 32-bit timers. a true
random number generator (RNG). They also feature standard and advanced
communication interfaces.
•
Up to three I2Cs
•
Three SPIs, two I2Ss full duplex. To achieve audio class accuracy, the I2S peripherals
can be clocked via a dedicated internal audio PLL or via an external clock to allow
synchronization
•
Four USARTs plus two UARTs
•
A USB OTG full speed and a USB OTG high speed with full-speed capability (with the
ULPI)
•
Two CANs
•
An SDIO/MMC interface
•
Ethernet and the camera interface available on STM32F407xx devices only
New advanced peripherals include an SDIO, an enhanced flexible static memory control
(FSMC) interface (for devices offered in packages of 100 pins and more), a camera
interface for CMOS sensors. Refer to Table 2: STM32F405xx and STM32F407xx: features
and peripheral counts for the list of peripherals available on each part number.
The STM32F405xx and STM32F407xx family operates in the –40 to +105 °C temperature
range from a 1.8 to 3.6 V power supply. The supply voltage can drop to 1.7 V when the
device operates in the 0 to 70 °C temperature range using an external power supply
supervisor: refer to Section : Internal reset OFF. A comprehensive set of power-saving
mode allows the design of low-power applications.
The STM32F405xx and STM32F407xx family offers devices in various packages ranging
from 64 pins to 176 pins. The set of included peripherals changes with the device chosen.
These features make the STM32F405xx and STM32F407xx microcontroller family suitable
for a wide range of applications:
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

---

Description
Figure 5 shows the general block diagram of the device family.
Table 2. STM32F405xx and STM32F407xx: features and peripheral counts
Peripherals
STM32F405RG STM32F405OG STM32F405VG STM32F405ZG STM32F405OE STM32F407Vx STM32F407Zx STM32F407Ix
Flash memory in
Kbytes
SRAM in
Kbytes
System
192(112+16+64)
Backup
FSMC memory
controller
No
Yes(1)
Ethernet
No
Yes
Timers
General-
purpose
Advanced
-control
Basic
IWDG
Yes
WWDG
Yes
RTC
Yes
True random number
generator
Yes

---

Description
Communi
cation
interfaces
SPI / I2S
3/2 (full duplex)(2)
I2C
USART/
UART
USB
OTG FS
Yes
USB
OTG HS
Yes
CAN
SDIO
Yes
Camera interface
No
Yes
GPIOs
12-bit ADC
Number of channels
12-bit DAC
Number of channels
Yes
Maximum CPU
frequency
 168 MHz
Operating voltage
1.8 to 3.6 V(3)
Operating
temperatures
Ambient temperatures: –40 to +85 °C /–40 to +105 °C
Junction temperature: –40 to + 125 °C
Package
LQFP64
WLCSP90
LQFP100
LQFP144
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176
1.
For the LQFP100 and WLCSP90 packages, only FSMC Bank1 or Bank2 are available. Bank1 can only support a multiplexed NOR/PSRAM memory using the NE1 Chip
Select. Bank2 can only support a 16- or 8-bit NAND flash memory using the NCE2 Chip Select. The interrupt line cannot be used since Port G is not available in this
package.
2.
The SPI2 and SPI3 interfaces give the flexibility to work in an exclusive way in either the SPI mode or the I2S audio mode.
3.
VDD/VDDA minimum value of 1.7 V is obtained when the device operates in reduced temperature range, and with the use of an external power supply supervisor (refer to
Section : Internal reset OFF).
Table 2. STM32F405xx and STM32F407xx: features and peripheral counts (continued)
Peripherals
STM32F405RG STM32F405OG STM32F405VG STM32F405ZG STM32F405OE STM32F407Vx STM32F407Zx STM32F407Ix

---

Description
2.1
Full compatibility throughout the family
The STM32F405xx and STM32F407xx are part of the STM32F4 family. They are fully pin-
to-pin, software and feature compatible with the STM32F2xx devices, allowing the user to
try different memory densities, peripherals, and performances (FPU, higher frequency) for a
greater degree of freedom during the development cycle.
The STM32F405xx and STM32F407xx devices maintain a close compatibility with the
whole STM32F10xxx family. All functional pins are pin-to-pin compatible. The
STM32F405xx and STM32F407xx, however, are not drop-in replacements for the
STM32F10xxx devices: the two families do not have the same power scheme, and so their
power pins are different. Nonetheless, transition from the STM32F10xxx to the
STM32F40xxx family remains simple as only a few pins are impacted.
Figure 4, Figure 3, Figure 2, and Figure 1 give compatible board designs between the
STM32F40xxx, STM32F2, and STM32F10xxx families.
Figure 1. Compatible board design between STM32F10xx/STM32F40xxx for LQFP64
VSS
VSS
VSS
VSS
0 Ω resistor or soldering bridge
present for the STM32F10xx
configuration, not present in the
STM32F4xx configuration
ai18489

---

Description
Figure 2. Compatible board design STM32F10xx/STM32F2/STM32F40xxx
for LQFP100 package
Figure 3. Compatible board design between STM32F10xx/STM32F2/STM32F40xxx
for LQFP144 package
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

---

Description
Figure 4. Compatible board design between STM32F2 and STM32F40xxx
 for LQFP176 and BGA176 packages
MS19919V3
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

---

Functional overview
Functional overview
Figure 5. STM32F40xxx block diagram
1.
The camera interface and ethernet are available only on STM32F407xx devices.
MS19920V5
GPIO PORT A
AHB/APB2
140 AF
PA[15:0]
TIM1 / PWM
4 compl. channels (TIM1_CH1[1:4]N,
4 channels (TIM1_CH1[1:4]ETR,
BKIN as AF
RX, TX, CK,
CTS, RTS as AF
MOSI, MISO,
SCK, NSS as AF
A P B 1 3 0 M Hz
8 analog inputs common
to the 3 ADCs
VDDREF_ADC
MOSI/SD, MISO/SD_ext, SCK/CK
NSS/WS, MCK as AF
TX, RX
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
16b
SDIO / MMC
D[7:0]
CMD, CK as AF
VBAT = 1.65 to 3.6 V
DMA2
SCL, SDA, SMBA as AF
JTAG & SW
Arm Cortex-M4
168 MHz
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
D[31:0], OEN, WEN,
NBL[3:0], NL, NREG,
NWAIT/IORDY, CD
INTN, NIIS16 as AF
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
AHB1 168 MHz
PHY
FIFO
@VDDA
@VDDA
POR/PDR
BOR
Supply
supervision
@VDDA
PVD
Int
POR
reset
XTAL 32 kHz
M AN A G T
RTC
RC
HS
FCLK
RC
L S
PWR
interface
IWDG
@VBAT
AWU
Reset &
clock
control
P L L1&2
PCLKx
VDD = 1.8 to 3.6 V
VSS
VCAP1, VCPA2
Voltage
regulator
3.3 to 1.2 V
VDD
Power managmt
RTC_AF1
Backup register
AHB bus-matrix 8S7M
LS
2 channels as AF
DAC1
DAC2
Flash
up to
1 MB
SRAM, PSRAM, NOR Flash,
PC Card (ATA), NAND Flash
External memory
controller (FSMC)
TIM6
TIM7
TIM2
TIM3
TIM4
TIM5
TIM12
TIM13
TIM14
USART2
USART3
UART4
UART5
SP3/I2S3
I2C1/SMBUS
I2C2/SMBUS
I2C3/SMBUS
bxCAN1
bxCAN2
SPI1
EXT IT. WKUP
D-BUS
FIFO
FPU
APB1 42  MHz (max)
SRAM 16 KB
CCM data RAM 64 KB
AHB3
AHB2 168 MHz
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
PI[11:0]
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
4 compl. channels (TIM1_CH1[1:4]N,
4 channels (TIM1_CH1[1:4]ETR,
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
SCL, SDA, SMBA as AF
SCL, SDA, SMBA as AF
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
smcard
irDA
smcard
irDA
16b
16b
16b
1 channel as AF
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
4- 16MHz
FIFO
SP2/I2S2
NIORD, IOWR, INT[2:3]
ADC3
ADC2
ADC1
Temperature sensor
IF
TIM9
16b
TIM10
16b
TIM11
16b
smcard
irDA
USART1
irDA
USART6
smcard
APB2 84 MHz (max)
@VDD
@VDD
@VDDA

---

Functional overview
3.0.1
Arm® Cortex®-M4 core with FPU and embedded Flash and SRAM
The Arm Cortex-M4 processor with FPU is the latest generation of Arm processors for
embedded systems. It was developed to provide a low-cost platform that meets the needs of
MCU implementation, with a reduced pin count and low-power consumption, while
delivering outstanding computational performance and an advanced response to interrupts.
The Arm Cortex-M4 32-bit RISC processor with FPU features exceptional code-efficiency,
delivering the high-performance expected from an Arm core in the memory size usually
associated with 8- and 16-bit devices.
The processor supports a set of DSP instructions which allow efficient signal processing and
complex algorithm execution.
Its single precision FPU (floating point unit) speeds up software development by using
metalanguage development tools, while avoiding saturation.
The STM32F405xx and STM32F407xx family is compatible with all Arm tools and software.
Figure 5 shows the general block diagram of the STM32F40xxx family.
Note:
Cortex-M4 with FPU is binary compatible with Cortex-M3.
3.0.2
Adaptive real-time memory accelerator (ART Accelerator)
The ART Accelerator is a memory accelerator which is optimized for STM32 industry-
standard Arm® Cortex®-M4 with FPU processors. It balances the inherent performance
advantage of the Arm Cortex-M4 with FPU over flash memory technologies, which normally
requires the processor to wait for the flash memory at higher frequencies.
To release the processor full 210 DMIPS performance at this frequency, the accelerator
implements an instruction prefetch queue and branch cache, which increases program
execution speed from the 128-bit flash memory. Based on CoreMark benchmark, the
performance achieved thanks to the ART accelerator is equivalent to 0 wait state program
execution from flash memory at a CPU frequency up to 168 MHz.
3.0.3
Memory protection unit
The memory protection unit (MPU) is used to manage the CPU accesses to memory to
prevent one task to accidentally corrupt the memory or resources used by any other active
task. This memory area is organized into up to 8 protected areas that can in turn be divided
up into 8 subareas. The protection area sizes are between 32 bytes and the whole 4
gigabytes of addressable memory.
The MPU is especially helpful for applications where some critical or certified code has to be
protected against the misbehavior of other tasks. It is usually managed by an RTOS (real-
time operating system). If a program accesses a memory location that is prohibited by the
MPU, the RTOS can detect it and take action. In an RTOS environment, the kernel can
dynamically update the MPU area setting, based on the process to be executed.
The MPU is optional and can be bypassed for applications that do not need it.
3.0.4
Embedded flash memory
The STM32F40xxx devices embed a flash memory of 512 Kbytes or 1 Mbytes available for
storing programs and data, plus 512 bytes of OTP memory.

---

Functional overview
3.0.5
CRC (cyclic redundancy check) calculation unit
The CRC (cyclic redundancy check) calculation unit is used to get a CRC code from a 32-bit
data word and a fixed generator polynomial.
Among other applications, CRC-based techniques are used to verify data transmission or
storage integrity. In the scope of the EN/IEC 60335-1 standard, they offer a means of
verifying the flash memory integrity. The CRC calculation unit helps compute a software
signature during runtime, to be compared with a reference signature generated at link-time
and stored at a given memory location.
3.0.6
Embedded SRAM
All STM32F40xxx products embed:
•
Up to 192 Kbytes of system SRAM including 64 Kbytes of CCM (core coupled memory)
data RAM
RAM memory is accessed (read/write) at CPU clock speed with 0 wait states.
•
4 Kbytes of backup SRAM
This area is accessible only from the CPU. Its content is protected against possible
unwanted write accesses, and is retained in Standby or VBAT mode.
3.0.7
Multi-AHB bus matrix
The 32-bit multi-AHB bus matrix interconnects all the masters (CPU, DMAs, Ethernet, USB
HS) and the slaves (flash memory, RAM, FSMC, AHB, and APB peripherals) and ensures a
seamless and efficient operation even when several high-speed peripherals work
simultaneously.

---

Functional overview
Figure 6. Multi-AHB matrix
3.0.8
DMA controller (DMA)
The devices feature two general-purpose dual-port DMAs (DMA1 and DMA2) with 8
streams each. They are able to manage memory-to-memory, peripheral-to-memory and
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
The DMA can be used with the main peripherals:
•
SPI and I2S
•
I2C
•
USART
•
General-purpose, basic and advanced-control timers TIMx
•
DAC
•
SDIO
•
Camera interface (DCMI)
•
ADC.
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
AHB1
peripherals
AHB2
FSMC
Static MemCtl
I-bus
D-bus
S-bus
DMA_P1
DMA_MEM1
DMA_MEM2
DMA_P2
ETHERNET_M
USB_HS_M
ai18490e
CCM data RAM
64-Kbyte
APB1
APB2
peripherals

---

Functional overview
3.0.9
Flexible static memory controller (FSMC)
The FSMC is embedded in the STM32F405xx and STM32F407xx family. It has four Chip
Select outputs supporting the following modes: PCCard/Compact Flash, SRAM, PSRAM,
NOR Flash and NAND Flash.
Functionality overview:
•
Write FIFO
•
Maximum FSMC_CLK frequency for synchronous accesses is 60 MHz.
LCD parallel interface
The FSMC can be configured to interface seamlessly with most graphic LCD controllers. It
supports the Intel 8080 and Motorola 6800 modes, and is flexible enough to adapt to
specific LCD interfaces. This LCD parallel interface capability makes it easy to build cost-
effective graphic applications using LCD modules with embedded controllers or high
performance solutions using external controllers with dedicated acceleration.
3.0.10
Nested vectored interrupt controller (NVIC)
The STM32F405xx and STM32F407xx embed a nested vectored interrupt controller able to
manage 16 priority levels, and handle up to 82 maskable interrupt channels plus the 16
interrupt lines of the Cortex®-M4 with FPU core.
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
3.0.11
External interrupt/event controller (EXTI)
The external interrupt/event controller consists of 23 edge-detector lines used to generate
interrupt/event requests. Each line can be independently configured to select the trigger
event (rising edge, falling edge, both) and can be masked independently. A pending register
maintains the status of the interrupt requests. The EXTI can detect an external line with a
pulse width shorter than the Internal APB2 clock period. Up to 140 GPIOs can be connected
to the 16 external interrupt lines.
3.0.12
Clocks and startup
On reset the 16 MHz internal RC oscillator is selected as the default CPU clock. The
16 MHz internal RC oscillator is factory-trimmed to offer 1% accuracy over the full
temperature range. The application can then select as system clock either the RC oscillator
or an external 4-26 MHz clock source. This clock can be monitored for failure. If a failure is
detected, the system automatically switches back to the internal RC oscillator and a
software interrupt is generated (if enabled). This clock source is input to a PLL thus allowing
to increase the frequency up to 168 MHz. Similarly, full interrupt management of the PLL

---

Functional overview
clock entry is available when necessary (for example if an indirectly used external oscillator
fails).
Several prescalers allow the configuration of the three AHB buses, the high-speed APB
(APB2) and the low-speed APB (APB1) domains. The maximum frequency of the three AHB
buses is 168 MHz while the maximum frequency of the high-speed APB domains is
84 MHz. The maximum allowed frequency of the low-speed APB domain is 42 MHz.
The devices embed a dedicated PLL (PLLI2S) which allows to achieve audio class
performance. In this case, the I2S master clock can generate all standard sampling
frequencies from 8 kHz to 192 kHz.
3.0.13
Boot modes
At startup, boot pins are used to select one out of three boot options:
•
Boot from user Flash
•
Boot from system memory
•
Boot from embedded SRAM
The boot loader is located in system memory. It is used to reprogram the flash memory by
using USART1 (PA9/PA10), USART3 (PC10/PC11 or PB10/PB11), CAN2 (PB5/PB13), USB
OTG FS in Device mode (PA11/PA12) through DFU (device firmware upgrade).
3.0.14
Power supply schemes
•
VDD = 1.8 to 3.6 V: external power supply for I/Os and the internal regulator (when
enabled), provided externally through VDD pins.
•
VSSA, VDDA = 1.8 to 3.6 V: external analog power supplies for ADC, DAC, Reset
blocks, RCs and PLL. VDDA and VSSA must be connected to VDD and VSS, respectively.
•
VBAT = 1.65 to 3.6 V: power supply for RTC, external clock 32 kHz oscillator and
backup registers (through power switch) when VDD is not present.
Refer to Figure 21: Power supply scheme for more details.
Note:
VDD/VDDA minimum value of 1.7 V is obtained when the device operates in reduced
temperature range, and with the use of an external power supply supervisor (refer to
Section : Internal reset OFF).
Refer to Table 2 in order to identify the packages supporting this option.
3.0.15
Power supply supervisor
Internal reset ON
On packages embedding the PDR_ON pin, the power supply supervisor is enabled by
holding PDR_ON high. On all other packages, the power supply supervisor is always
enabled.
The device has an integrated power-on reset (POR) / power-down reset (PDR) circuitry
coupled with a Brownout reset (BOR) circuitry. At power-on, POR/PDR is always active and
ensures proper operation starting from 1.8 V. After the 1.8 V POR threshold level is
reached, the option byte loading process starts, either to confirm or modify default BOR
threshold levels, or to disable BOR permanently. Three BOR thresholds are available
through option bytes. The device remains in reset mode when VDD is below a specified
threshold, VPOR/PDR or VBOR, without the need for an external reset circuit.

---

Functional overview
The device also features an embedded programmable voltage detector (PVD) that monitors
the VDD/VDDA power supply and compares it to the VPVD threshold. An interrupt can be
generated when VDD/VDDA drops below the VPVD threshold and/or when VDD/VDDA is
higher than the VPVD threshold. The interrupt service routine can then generate a warning
message and/or put the MCU into a safe state. The PVD is enabled by software.
Internal reset OFF
This feature is available only on packages featuring the PDR_ON pin. The internal power-on
reset (POR) / power-down reset (PDR) circuitry is disabled with the PDR_ON pin.
An external power supply supervisor should monitor VDD and should maintain the device in
reset mode as long as VDD is below a specified threshold. PDR_ON should be connected to
this external power supply supervisor. Refer to Figure 7: Power supply supervisor
interconnection with internal reset OFF.
Figure 7. Power supply supervisor interconnection with internal reset OFF
1.
PDR = 1.7 V for reduce temperature range; PDR = 1.8 V for all temperature range.
The VDD specified threshold, below which the device must be maintained under reset, is
1.8 V (see Figure 7). This supply voltage can drop to 1.7 V when the device operates in the
0 to 70 °C temperature range.
A comprehensive set of power-saving mode allows to design low-power applications.
When the internal reset is OFF, the following integrated features are no more supported:
•
The integrated power-on reset (POR) / power-down reset (PDR) circuitry is disabled
•
The brownout reset (BOR) circuitry is disabled
•
The embedded programmable voltage detector (PVD) is disabled
•
VBAT functionality is no more available and VBAT pin should be connected to VDD
All packages, except for the LQFP64 and LQFP100, allow to disable the internal reset
through the PDR_ON signal.
MS31383V4
NRST
VDD
PDR_ON
External VDD power supply supervisor
Ext. reset controller active when
VDD < 1.7 V
VDD
Application reset
signal
VSS

---

Functional overview
Figure 8. PDR_ON and NRST control with internal reset OFF
1.
PDR = 1.7 V for reduce temperature range; PDR = 1.8 V for all temperature range.
3.0.16
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
Regulator ON
On packages embedding the BYPASS_REG pin, the regulator is enabled by holding
BYPASS_REG low. On all other packages, the regulator is always enabled.
There are three power modes configured by software when regulator is ON:
•
MR is used in the nominal regulation mode (With different voltage scaling in Run)
In Main regulator mode (MR mode), different voltage scaling are provided to reach the
best compromise between maximum frequency and dynamic power consumption.
Refer to Table 14: General operating conditions.
•
LPR is used in the Stop modes
The LP regulator mode is configured by software when entering Stop mode.
•
Power-down is used in Standby mode.
The Power-down mode is activated only when entering in Standby mode. The regulator
output is in high impedance and the kernel circuitry is powered down, inducing zero
consumption. The contents of the registers and SRAM are lost)
MS19009V8
VDD
time
PDR = 1.7 V
time
NRST
PDR_ON
PDR_ON
Reset by a source different from
power supply supervisor

---

Functional overview
Two external ceramic capacitors should be connected on VCAP_1 & VCAP_2 pin. Refer to
Figure 21: Power supply scheme and Figure 16: VCAP_1/VCAP_2 operating conditions.
All packages have regulator ON feature.
Regulator OFF
This feature is available only on packages featuring the BYPASS_REG pin. The regulator is
disabled by holding BYPASS_REG high. The regulator OFF mode allows to supply
externally a V12 voltage source through VCAP_1 and VCAP_2 pins.
Since the internal voltage scaling is not manage internally, the external voltage value must
be aligned with the targeted maximum frequency. Refer to Table 14: General operating
conditions.
The two 2.2 µF ceramic capacitors should be replaced by two 100 nF decoupling
capacitors.
Refer to Figure 21: Power supply scheme
When the regulator is OFF, there is no more internal monitoring on V12. An external power
supply supervisor should be used to monitor the V12 of the logic power domain. PA0 pin
should be used for this purpose, and act as power-on reset on V12 power domain.
In regulator OFF mode the following features are no more supported:
•
PA0 cannot be used as a GPIO pin since it allows to reset a part of the V12 logic power
domain which is not reset by the NRST pin.
•
As long as PA0 is kept low, the debug mode cannot be used under power-on reset. As
a consequence, PA0 and NRST pins must be managed separately if the debug
connection under reset or pre-reset is required.
•
The standby mode is not available
Figure 9. Regulator OFF
External VCAP_1/2 power
supply supervisor
Ext. reset controller active
when VCAP_1/2 < Min V12
ai18498V5
VSS
1/2/...N
2 x 100 nF
(1 x 100 nF)
(note 1)
PA0
VCAP1
VCAP2
V12
V12
BYPASS_REG
VDD
1/2/...N
N ×  100 nF
+ 1 × 4.7 μF
VDD
NRST
Application reset
signal (optional)

---

Functional overview
The following conditions must be respected:
•
VDD should always be higher than VCAP_1 and VCAP_2 to avoid current injection
between power domains.
•
If the time for VCAP_1 and VCAP_2 to reach V12 minimum value is faster than the time for
VDD to reach 1.8 V, then PA0 should be kept low to cover both conditions: until VCAP_1
and VCAP_2 reach V12 minimum value and until VDD reaches 1.8 V (see Figure 10).
•
Otherwise, if the time for VCAP_1 and VCAP_2 to reach V12 minimum value is slower
than the time for VDD to reach 1.8 V, then PA0 could be asserted low externally (see
Figure 11).
•
If VCAP_1 and VCAP_2 go below V12 minimum value and VDD is higher than 1.8 V, then
a reset must be asserted on PA0 pin.
Note:
The minimum value of V12 depends on the maximum frequency targeted in the application
(see Table 14: General operating conditions).
Figure 10. Startup in regulator OFF mode: slow VDD slope
- power-down reset risen after VCAP_1/VCAP_2 stabilization
1.
This figure is valid both whatever the internal reset mode (ON or OFF).
2.
PDR = 1.7 V for reduced temperature range; PDR = 1.8 V for all temperature ranges.
ai18491e
VDD
time
Min V12
PDR = 1.7 V or 1.8 V (2)
VCAP_1/VCAP_2
V12
NRST
time

---

Functional overview
Figure 11. Startup in regulator OFF mode: fast VDD slope
- power-down reset risen before VCAP_1/VCAP_2 stabilization
1.
This figure is valid both whatever the internal reset mode (ON or OFF).
2.
PDR = 1.7 V for a reduced temperature range; PDR = 1.8 V for all temperature ranges.
3.0.17
Regulator ON/OFF and internal reset ON/OFF availability
3.0.18
Real-time clock (RTC), backup SRAM and backup registers
The backup domain of the STM32F405xx and STM32F407xx includes:
•
The real-time clock (RTC)
•
4 Kbytes of backup SRAM
•
20 backup registers
The real-time clock (RTC) is an independent BCD timer/counter. Dedicated registers contain
the second, minute, hour (in 12/24 hour), week day, date, month, year, in BCD (binary-
coded decimal) format. Correction for 28, 29 (leap year), 30, and 31 day of the month are
performed automatically. The RTC provides a programmable alarm and programmable
periodic interrupts with wakeup from Stop and Standby modes. The sub-seconds value is
also available in binary format.
It is clocked by a 32.768 kHz external crystal, resonator or oscillator, the internal low-power
RC oscillator or the high-speed external clock divided by 2 to 31. The internal low-speed RC
VDD
time
Min V12
VCAP_1/VCAP_2
V12
PA0 asserted externally
NRST
time
ai18492d
PDR = 1.7 V or 1.8 V (2)
Table 3. Regulator ON/OFF and internal reset ON/OFF availability
Regulator ON
Regulator OFF
Internal reset ON
Internal reset
OFF
LQFP64
LQFP100
Yes
No
Yes
No
LQFP144
Yes
PDR_ON set to
VDD
Yes
PDR_ON
connected to an
external power
supply supervisor
WLCSP90
UFBGA176
LQFP176
Yes
BYPASS_REG set
to VSS
Yes
BYPASS_REG set
to VDD

---

Functional overview
has a typical frequency of 32 kHz. The RTC can be calibrated using an external 512 Hz
output to compensate for any natural quartz deviation.
Two alarm registers are used to generate an alarm at a specific time and calendar fields can
be independently masked for alarm comparison. To generate a periodic interrupt, a 16-bit
programmable binary auto-reload downcounter with programmable resolution is available
and allows automatic wakeup and periodic alarms from every 120 µs to every 36 hours.
A 20-bit prescaler is used for the time base clock. It is by default configured to generate a
time base of 1 second from a clock at 32.768 kHz.
The 4-Kbyte backup SRAM is an EEPROM-like memory area. It can be used to store data
which need to be retained in VBAT and standby mode. This memory area is disabled by
default to minimize power consumption (see Section 3.0.19: Low-power modes). It can be
enabled by software.
The backup registers are 32-bit registers used to store 80 bytes of user application data
when VDD power is not present. Backup registers are not reset by a system, a power reset,
or when the device wakes up from the Standby mode (see Section 3.0.19: Low-power
modes).
Additional 32-bit registers contain the programmable alarm subseconds, seconds, minutes,
hours, day, and date.
Like backup SRAM, the RTC and backup registers are supplied through a switch that is
powered either from the VDD supply when present or from the VBAT pin.
3.0.19
Low-power modes
The STM32F405xx and STM32F407xx support three low-power modes to achieve the best
compromise between low-power consumption, short startup time and available wakeup
sources:
•
Sleep mode
In Sleep mode, only the CPU is stopped. All peripherals continue to operate and can
wake up the CPU when an interrupt/event occurs.
•
Stop mode
The Stop mode achieves the lowest power consumption while retaining the contents of
SRAM and registers. All clocks in the V12 domain are stopped, the PLL, the HSI RC
and the HSE crystal oscillators are disabled. The voltage regulator can also be put
either in normal or in low-power mode.
The device can be woken up from the Stop mode by any of the EXTI line (the EXTI line
source can be one of the 16 external lines, the PVD output, the RTC alarm / wakeup /
tamper / time stamp events, the USB OTG FS/HS wakeup or the Ethernet wakeup).
•
Standby mode
The Standby mode is used to achieve the lowest power consumption. The internal
voltage regulator is switched off so that the entire V12 domain is powered off. The PLL,
the HSI RC and the HSE crystal oscillators are also switched off. After entering

---

Functional overview
Standby mode, the SRAM and register contents are lost except for registers in the
backup domain and the backup SRAM when selected.
The device exits the Standby mode when an external reset (NRST pin), an IWDG reset,
a rising edge on the WKUP pin, or an RTC alarm / wakeup / tamper /time stamp event
occurs.
The standby mode is not supported when the embedded voltage regulator is bypassed
and the V12 domain is controlled by an external power.
3.0.20
VBAT operation
The VBAT pin allows to power the device VBAT domain from an external battery, an external
supercapacitor, or from VDD when no external battery and an external supercapacitor are
present.
VBAT operation is activated when VDD is not present.
The VBAT pin supplies the RTC, the backup registers and the backup SRAM.
Note:
When the microcontroller is supplied from VBAT, external interrupts and RTC alarm/events
do not exit it from VBAT operation.
When PDR_ON pin is not connected to VDD (internal reset OFF), the VBAT functionality is no
more available and VBAT pin should be connected to VDD.
3.0.21
Timers and watchdogs
The STM32F405xx and STM32F407xx devices include two advanced-control timers, eight
general-purpose timers, two basic timers and two watchdog timers.
All timer counters can be frozen in debug mode.
Table 4 compares the features of the advanced-control, general-purpose and basic timers.
Table 4. Timer feature comparison
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
Complemen-
tary output
Max
interface
clock
(MHz)
Max
timer
clock
(MHz)
Advanced
-control
TIM1,
TIM8
16-bit
Up,
Down,
Up/down
Any integer
between 1
and 65536
Yes
Yes

---

Functional overview
Advanced-control timers (TIM1, TIM8)
The advanced-control timers (TIM1, TIM8) can be seen as three-phase PWM generators
multiplexed on 6 channels. They have complementary PWM outputs with programmable
inserted dead times. They can also be considered as complete general-purpose timers.
Their 4 independent channels can be used for:
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
General
purpose
TIM2,
TIM5
32-bit
Up,
Down,
Up/down
Any integer
between 1
and 65536
Yes
No
TIM3,
TIM4
16-bit
Up,
Down,
Up/down
Any integer
between 1
and 65536
Yes
No
TIM9
16-bit
Up
Any integer
between 1
and 65536
No
No
TIM10
,
TIM11
16-bit
Up
Any integer
between 1
and 65536
No
No
TIM12
16-bit
Up
Any integer
between 1
and 65536
No
No
TIM13
,
TIM14
16-bit
Up
Any integer
between 1
and 65536
No
No
Basic
TIM6,
TIM7
16-bit
Up
Any integer
between 1
and 65536
Yes
No
Table 4. Timer feature comparison (continued)
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
Complemen-
tary output
Max
interface
clock
(MHz)
Max
timer
clock
(MHz)

---

Functional overview
General-purpose timers (TIMx)
There are ten synchronizable general-purpose timers embedded in the STM32F40xxx
devices (see Table 4 for differences).
•
TIM2, TIM3, TIM4, TIM5
The STM32F40xxx include 4 full-featured general-purpose timers: TIM2, TIM5, TIM3,
and TIM4.The TIM2 and TIM5 timers are based on a 32-bit auto-reload
up/downcounter and a 16-bit prescaler. The TIM3 and TIM4 timers are based on a 16-
bit auto-reload up/downcounter and a 16-bit prescaler. They all feature 4 independent
channels for input capture/output compare, PWM or one-pulse mode output. This gives
up to 16 input capture/output compare/PWMs on the largest packages.
The TIM2, TIM3, TIM4, TIM5 general-purpose timers can work together, or with the
other general-purpose timers and the advanced-control timers TIM1 and TIM8 via the
Timer Link feature for synchronization or event chaining.
Any of these general-purpose timers can be used to generate PWM outputs.
TIM2, TIM3, TIM4, TIM5 all have independent DMA request generation. They are
capable of handling quadrature (incremental) encoder signals and the digital outputs
from 1 to 4 hall-effect sensors.
•
TIM9, TIM10, TIM11, TIM12, TIM13, and TIM14
These timers are based on a 16-bit auto-reload upcounter and a 16-bit prescaler.
TIM10, TIM11, TIM13, and TIM14 feature one independent channel, whereas TIM9
and TIM12 have two independent channels for input capture/output compare, PWM or
one-pulse mode output. They can be synchronized with the TIM2, TIM3, TIM4, TIM5
full-featured general-purpose timers. They can also be used as simple time bases.
Basic timers TIM6 and TIM7
These timers are mainly used for DAC trigger and waveform generation. They can also be
used as a generic 16-bit time base.
TIM6 and TIM7 support independent DMA request generation.
Independent watchdog
The independent watchdog is based on a 12-bit downcounter and 8-bit prescaler. It is
clocked from an independent 32 kHz internal RC and as it operates independently from the
main clock, it can operate in Stop and Standby modes. It can be used either as a watchdog
to reset the device when a problem occurs, or as a free-running timer for application timeout
management. It is hardware- or software-configurable through the option bytes.
Window watchdog
The window watchdog is based on a 7-bit downcounter that can be set as free-running. It
can be used as a watchdog to reset the device when a problem occurs. It is clocked from
the main clock. It has an early warning interrupt capability and the counter can be frozen in
debug mode.

---

Functional overview
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
3.0.22
Inter-integrated circuit interface (I²C)
Up to three I²C bus interfaces can operate in multimaster and slave modes. They can
support the Standard-mode (up to 100 kHz) and Fast-mode (up to 400 kHz). They support
the 7/10-bit addressing mode and the 7-bit dual addressing mode (as slave). A hardware
CRC generation/verification is embedded.
They can be served by DMA and they support SMBus 2.0/PMBus.
3.0.23
Universal synchronous/asynchronous receiver transmitters (USART)
The STM32F405xx and STM32F407xx embed four universal synchronous/asynchronous
receiver transmitters (USART1, USART2, USART3 and USART6) and two universal
asynchronous receiver transmitters (UART4 and UART5).
These six interfaces provide asynchronous communication, IrDA SIR ENDEC support,
multiprocessor communication mode, single-wire half-duplex communication mode and
have LIN Master/Slave capability. The USART1 and USART6 interfaces are able to
communicate at speeds of up to 10.5 Mbit/s. The other available interfaces communicate at
up to 5.25 Mbit/s.
USART1, USART2, USART3 and USART6 also provide hardware management of the CTS
and RTS signals, Smart Card mode (ISO 7816 compliant) and SPI-like communication
capability. All interfaces can be served by the DMA controller.

---

Functional overview
3.0.24
Serial peripheral interface (SPI)
The STM32F40xxx feature up to three SPIs in slave and master modes in full-duplex and
simplex communication modes. SPI1 can communicate at up to 42 Mbits/s, SPI2 and SPI3
can communicate at up to 21 Mbit/s. The 3-bit prescaler gives 8 master mode frequencies
and the frame is configurable to 8 bits or 16 bits. The hardware CRC generation/verification
supports basic SD Card/MMC modes. All SPIs can be served by the DMA controller.
The SPI interface can be configured to operate in TI mode for communications in master
mode and slave mode.
3.0.25
Inter-integrated sound (I2S)
Two standard I2S interfaces (multiplexed with SPI2 and SPI3) are available. They can be
operated in master or slave mode, in full duplex and half-duplex communication modes, and
can be configured to operate with a 16-/32-bit resolution as an input or output channel.
Audio sampling frequencies from 8 kHz up to 192 kHz are supported. When either or both of
the I2S interfaces is/are configured in master mode, the master clock can be output to the
external DAC/CODEC at 256 times the sampling frequency.
All I2Sx can be served by the DMA controller.
Table 5. USART feature comparison
USART
name
Standard
features
Modem
(RTS/
CTS)
LIN
SPI
master
irDA
Smartcard
(ISO 7816)
Max. baud rate
in Mbit/s
(oversampling
by 16)
Max. baud rate
in Mbit/s
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
5.25
10.5
APB2
(max.
84 MHz)
USART2
X
X
X
X
X
X
2.62
5.25
APB1
(max.
42 MHz)
USART3
X
X
X
X
X
X
2.62
5.25
APB1
(max.
42 MHz)
UART4
X
-
X
-
X
-
2.62
5.25
APB1
(max.
42 MHz)
UART5
X
-
X
-
X
-
2.62
5.25
APB1
(max.
42 MHz)
USART6
X
X
X
X
X
X
5.25
10.5
APB2
(max.
84 MHz)

---

Functional overview
3.0.26
Audio PLL (PLLI2S)
The devices feature an additional dedicated PLL for audio I2S application. It allows to
achieve error-free I2S sampling clock accuracy without compromising on the CPU
performance, while using USB peripherals.
The PLLI2S configuration can be modified to manage an I2S sample rate change without
disabling the main PLL (PLL) used for CPU, USB and Ethernet interfaces.
The audio PLL can be programmed with very low error to obtain sampling rates ranging
from 8 KHz to 192 KHz.
In addition to the audio PLL, a master clock input pin can be used to synchronize the I2S
flow with an external PLL (or Codec output).
3.0.27
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
3.0.28
Ethernet MAC interface with dedicated DMA and IEEE 1588 support
Peripheral available only on the STM32F407xx devices.
The STM32F407xx devices provide an IEEE-802.3-2002-compliant media access controller
(MAC) for ethernet LAN communications through an industry-standard medium-
independent interface (MII) or a reduced medium-independent interface (RMII). The
STM32F407xx requires an external physical interface device (PHY) to connect to the
physical LAN bus (twisted-pair, fiber, etc.). the PHY is connected to the STM32F407xx MII
port using 17 signals for MII or 9 signals for RMII, and can be clocked using the 25 MHz
(MII) from the STM32F407xx.

---

Functional overview
The STM32F407xx includes the following features:
•
Supports 10 and 100 Mbit/s rates
•
Dedicated DMA controller allowing high-speed transfers between the dedicated SRAM
and the descriptors (see the STM32F40xxx/41xxx reference manual for details)
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
3.0.29
Controller area network (bxCAN)
The two CANs are compliant with the 2.0A and B (active) specifications with a bitrate up to 1
Mbit/s. They can receive and transmit standard frames with 11-bit identifiers as well as
extended frames with 29-bit identifiers. Each CAN has three transmit mailboxes, two receive
FIFOS with 3 stages and 28 shared scalable filter banks (all of them can be used even if one
CAN is used). 256 bytes of SRAM are allocated for each CAN.
3.0.30
Universal serial bus on-the-go full-speed (OTG_FS)
The STM32F405xx and STM32F407xx embed an USB OTG full-speed device/host/OTG
peripheral with integrated transceivers. The USB OTG FS peripheral is compliant with the
USB 2.0 specification and with the OTG 1.0 specification. It has software-configurable
endpoint setting and supports suspend/resume. The USB OTG full-speed controller
requires a dedicated 48 MHz clock that is generated by a PLL connected to the HSE
oscillator. The major features are:
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

---

Functional overview
3.0.31
Universal serial bus on-the-go high-speed (OTG_HS)
The STM32F405xx and STM32F407xx devices embed a USB OTG high-speed (up to
480 Mb/s) device/host/OTG peripheral. The USB OTG HS supports both full-speed and
high-speed operations. It integrates the transceivers for full-speed operation (12 MB/s) and
features a UTMI low-pin interface (ULPI) for high-speed operation (480 MB/s). When using
the USB OTG HS in HS mode, an external PHY device connected to the ULPI is required.
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
3.0.32
Digital camera interface (DCMI)
The camera interface is not available in STM32F405xx devices.
STM32F407xx products embed a camera interface that can connect with camera modules
and CMOS sensors through an 8-bit to 14-bit parallel interface, to receive video data. The
camera interface can sustain a data transfer rate up to 54 Mbyte/s at 54 MHz. It features:
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
3.0.33
True random number generator (RNG)
All STM32F405xx and STM32F407xx products embed a true random number generator
(RNG) that provides full entropy outputs to the application as 32-bit samples. It is composed
of a live entropy source (analog) and an internal conditioning component.
3.0.34
General-purpose input/outputs (GPIOs)
Each of the GPIO pins can be configured by software as output (push-pull or open-drain,
with or without pull-up or pull-down), as input (floating, with or without pull-up or pull-down)

---

Functional overview
or as peripheral alternate function. Most of the GPIO pins are shared with digital or analog
alternate functions. All GPIOs are high-current-capable and have speed selection to better
manage internal noise, power consumption and electromagnetic emission.
The I/O configuration can be locked if needed by following a specific sequence in order to
avoid spurious writing to the I/Os registers.
Fast I/O handling allowing maximum I/O toggling up to 84 MHz.
3.0.35
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
3.0.36
Temperature sensor
The temperature sensor has to generate a voltage that varies linearly with temperature. The
conversion range is between 1.8 V and 3.6 V. The temperature sensor is internally
connected to the ADC1_IN16 input channel which is used to convert the sensor output
voltage into a digital value.
As the offset of the temperature sensor varies from chip to chip due to process variation, the
internal temperature sensor is mainly suitable for applications that detect temperature
changes instead of absolute temperatures. If an accurate temperature reading is needed,
then an external temperature sensor part should be used.
3.0.37
Digital-to-analog converter (DAC)
The two 12-bit buffered DAC channels can be used to convert two digital signals into two
analog voltage signal outputs.

---

Functional overview
This dual digital Interface supports the following features:
•
two DAC converters: one for each output channel
•
8-bit or 12-bit monotonic output
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
3.0.38
Serial wire JTAG debug port (SWJ-DP)
The Arm SWJ-DP interface is embedded, and is a combined JTAG and serial wire debug
port that enables either a serial wire debug or a JTAG probe to be connected to the target.
Debug is performed using 2 pins only instead of 5 required by the JTAG (JTAG pins could
be re-use as GPIO with alternate function): the JTAG TMS and TCK pins are shared with
SWDIO and SWCLK, respectively, and a specific sequence on the TMS pin is used to
switch between JTAG-DP and SW-DP.
3.0.39
Embedded Trace Macrocell™
The Arm Embedded Trace Macrocell provides a greater visibility of the instruction and data
flow inside the CPU core by streaming compressed data at a very high rate from the
STM32F40xxx through a small number of ETM pins to an external hardware trace port
analyser (TPA) device. The TPA is connected to a host computer using USB, Ethernet, or
any other high-speed channel. Real-time instruction and data flow activity can be recorded
and then formatted for display on the host computer that runs the debugger software. TPA
hardware is commercially available from common development tool vendors.
The Embedded Trace Macrocell operates with third party debugger software tools.

---

Pinouts and pin description
Pinouts and pin description
Figure 12. STM32F40xxx LQFP64 pinout
1.
The above figure shows the package top view.
64 63 62 61 60 59 58 57 56 55 54 53 52 51 50 49
17 18 19 20 21 22 23 24
29 30 31 32
25 26 27 28
VBAT
PC14
PC15
NRST
PC0
PC1
PC2
PC3
VSSA
VDDA
PA0_WKUP
PA1
PA2
VDD
PB9
PB8
BOOT0
PB7
PB6
PB5
PB4
PB3
PD2
PC12
PC11
PC10
PA15
PA14
VDD
VCAP_2
PA13
PA12
PA11
PA10
PA9
PA8
PC9
PC8
PC7
PC6
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
PB10
PB11
VCAP_1
VDD
LQFP64
ai18493b
PC13
PH0
PH1
VSS

---

Pinouts and pin description
Figure 13. STM32F40xxx LQFP100 pinout
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
Figure 14. STM32F40xxx LQFP144 pinout
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
Figure 15. STM32F40xxx LQFP176 pinout
1.
The above figure shows the package top view.
MS19916V4
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
PG14
PG13
PG12
PG11
PG10
PG9
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
PI7
PI6
PE2
PE3
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
PF5
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
V
PC0
V
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
PF13
PF14
PF15
PG0
PG1
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
LQFP176
PI4
PA15
PA14
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
BYPASS_REG
VDD
VDD
VSS
VDD
VCAP_1
VDD
VSS
VDD
VCAP_2
VSS
VDD
VSS
VDD
VSS
VDD
VSS
VDD
VDD
VSS
VDD
VSS
VDD

---

Pinouts and pin description
Figure 16. STM32F40xxx UFBGA176 ballout
1.
This figure shows the package top view.
ai18497b
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
VCAP_2
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
PF4
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
PF5
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
PF8
BYPASS_
REG
PH11
PH10
PD15
PG2
M
VSSA
PC0
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
PA1
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

---

Pinouts and pin description
Figure 17. STM32F40xxx WLCSP90 ballout
1.
This figure shows the package bump view.
A
VBAT
PC13
PDR_ON
PB4
PD7
PD4
PC12
B
PC15
VDD
PB7
PB3
PD6
PD2
PA15
C
PA0
VSS
PC11
PI0
PB6
PD5
PD1
D
PC2
PB8
PA13
E
PC3
VSS
F
PH1
PA1
G
NRST
H
VSSA
J
PA2
PA4
PA7
PB2
PE11
PB11
PB12
MS30402V1
PA14
PI1
PA12
PA10
PA9
PC0
PC9
PC8
PH0
PB13
PC6
PD14
PD12
PE8
PE12
BYPASS_
REG
PD9
PD8
PE9
PB14
VDD
PC14
VCAP_2
PA11
PB5
PD0
PC10
PA8
VSS
VDD
VSS
VDD
PC7
VDD
PE10
PE14
VCAP_1
PD15
PE13
PE15
PD10
PD11
PA3
PA6
PB1
PB10
PB15
PB9
BOOT0
VDDA
PB0
PE7
PA5
Table 6. Legend/abbreviations used in the pinout table
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
Bidirectional reset pin with embedded weak pull-up resistor
Notes
Unless otherwise specified by a note, all I/Os are set as floating inputs during and after reset
Alternate
functions
Functions selected through GPIOx_AFR registers
Additional
functions
Functions directly selected/enabled through peripheral registers

---

Pinouts and pin description
Table 7. STM32F40xxx pin and ball definitions(1)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176
-
-
A2
PE2
I/O
FT
-
TRACECLK/ FSMC_A23 /
ETH_MII_TXD3 /
EVENTOUT
-
-
-
A1
PE3
I/O
FT
-
TRACED0/FSMC_A19 /
EVENTOUT
-
-
-
B1
PE4
I/O
FT
-
TRACED1/FSMC_A20 /
DCMI_D4/ EVENTOUT
-
-
-
B2
PE5
I/O
FT
-
TRACED2 / FSMC_A21 /
TIM9_CH1 / DCMI_D6 /
EVENTOUT
-
-
-
B3
PE6
I/O
FT
-
TRACED3 / FSMC_A22 /
TIM9_CH2 / DCMI_D7 /
EVENTOUT
-
A10
C1
VBAT
S
-
-
-
-
-
-
-
-
D2
PI8
I/O
FT
(3)(
4)
-
RTC_TAMP1,
RTC_TAMP2,
RTC_TS
A9
D1
PC13
I/O
FT
(3)
(4)
-
RTC_OUT,
RTC_TAMP1,
RTC_TS
B10
E1
PC14/OSC32_IN
(PC14)
I/O
FT
(3)(
4)
-
OSC32_IN(5)
B9
F1
PC15/
OSC32_OUT
(PC15)
I/O
FT
(3)(
4)
-
OSC32_OUT(5)
-
-
-
-
D3
PI9
I/O
FT
-
CAN1_RX  / EVENTOUT
-
-
-
-
-
E3
PI10
I/O
FT
-
ETH_MII_RX_ER /
EVENTOUT
-
-
-
-
-
E4
PI11
I/O
FT
-
OTG_HS_ULPI_DIR /
EVENTOUT
-
-
-
-
-
F2
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
F3
VDD
S
-
-
-
-
-
-
-
E2
PF0
I/O
FT
-
FSMC_A0 / I2C2_SDA /
EVENTOUT
-

---

Pinouts and pin description
-
-
-
H3
PF1
I/O
FT
-
FSMC_A1 / I2C2_SCL /
EVENTOUT
-
-
-
-
H2
PF2
I/O
FT
-
FSMC_A2 / I2C2_SMBA /
EVENTOUT
-
-
-
-
J2
PF3
I/O
FT
(5)
FSMC_A3/EVENTOUT
ADC3_IN9
-
-
-
J3
PF4
I/O
FT
(5)
FSMC_A4/EVENTOUT
ADC3_IN14
-
-
-
K3
PF5
I/O
FT
(5)
FSMC_A5/EVENTOUT
ADC3_IN15
-
C9
G2
VSS
S
-
-
-
-
-
B8
G3
VDD
S
-
-
-
-
-
-
-
K2
PF6
I/O
FT
(5)
TIM10_CH1 /
FSMC_NIORD/
EVENTOUT
ADC3_IN4
-
-
-
K1
PF7
I/O
FT
(5)
TIM11_CH1/FSMC_NREG/
EVENTOUT
ADC3_IN5
-
-
-
L3
PF8
I/O
FT
(5)
TIM13_CH1 /
FSMC_NIOWR/
EVENTOUT
ADC3_IN6
-
-
-
L2
PF9
I/O
FT
(5)
TIM14_CH1 / FSMC_CD/
EVENTOUT
ADC3_IN7
-
-
-
L1
PF10
I/O
FT
(5)
FSMC_INTR/ EVENTOUT
ADC3_IN8
F10
G1
PH0/OSC_IN
(PH0)
I/O
FT
-
 EVENTOUT
OSC_IN(5)
F9
H1
PH1/OSC_OUT
(PH1)
I/O
FT
-
EVENTOUT
OSC_OUT(5)
G10 14
J1
NRST
I/O
RST
-
-
-
E10
M2
PC0
I/O
FT
(5)
OTG_HS_ULPI_STP/
EVENTOUT
ADC123_IN10
-
M3
PC1
I/O
FT
(5)
ETH_MDC/ EVENTOUT
ADC123_IN11
10 D10 17
M4
PC2
I/O
FT
(5)
SPI2_MISO /
OTG_HS_ULPI_DIR /
ETH_MII_TXD2
/I2S2ext_SD/ EVENTOUT
ADC123_IN12
Table 7. STM32F40xxx pin and ball definitions(1) (continued)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176

---

Pinouts and pin description
E9
M5
PC3
I/O
FT
(5)
SPI2_MOSI / I2S2_SD /
OTG_HS_ULPI_NXT /
ETH_MII_TX_CLK/
EVENTOUT
ADC123_IN13
-
-
-
VDD
S
-
-
-
-
12 H10 20
M1
VSSA
S
-
-
-
-
-
-
-
-
N1
-
VREF–
S
-
-
-
-
-
-
P1
VREF+
S
-
-
-
-
G9
R1
VDDA
S
-
-
-
-
14 C10 23
N3
PA0/WKUP
(PA0)
I/O
FT
(6)
USART2_CTS/
UART4_TX/
ETH_MII_CRS /
TIM2_CH1_ETR/
TIM5_CH1 / TIM8_ETR/
EVENTOUT
ADC123_IN0/WK
UP(5)
F8
N2
PA1
I/O
FT
(5)
USART2_RTS /
UART4_RX/
ETH_RMII_REF_CLK /
ETH_MII_RX_CLK /
TIM5_CH2 / TIM2_CH2/
EVENTOUT
ADC123_IN1
J10
P2
PA2
I/O
FT
(5)
USART2_TX/TIM5_CH3 /
TIM9_CH1 / TIM2_CH3 /
ETH_MDIO/ EVENTOUT
ADC123_IN2
-
-
-
-
F4
PH2
I/O
FT
-
ETH_MII_CRS/EVENTOUT
-
-
-
-
-
G4
PH3
I/O
FT
-
ETH_MII_COL/EVENTOUT
-
-
-
-
-
H4
PH4
I/O
FT
-
I2C2_SCL /
OTG_HS_ULPI_NXT/
EVENTOUT
-
-
-
-
-
J4
PH5
I/O
FT
-
I2C2_SDA/ EVENTOUT
-
H9
R2
PA3
I/O
FT
(5)
USART2_RX/TIM5_CH4 /
TIM9_CH2 / TIM2_CH4 /
OTG_HS_ULPI_D0 /
ETH_MII_COL/
EVENTOUT
ADC123_IN3
E5
-
-
VSS
S
-
-
-
-
Table 7. STM32F40xxx pin and ball definitions(1) (continued)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176

---

Pinouts and pin description
-
D9
-
-
L4
BYPASS_REG
I
FT
-
-
-
E4
K4
VDD
S
-
-
-
-
J9
N4
PA4
I/O
TTa
(5)
SPI1_NSS / SPI3_NSS /
USART2_CK /
DCMI_HSYNC /
OTG_HS_SOF/ I2S3_WS/
EVENTOUT
ADC12_IN4
/DAC_OUT1
G8
P4
PA5
I/O
TTa
(5)
SPI1_SCK/
OTG_HS_ULPI_CK /
TIM2_CH1_ETR/
TIM8_CH1N/ EVENTOUT
ADC12_IN5/DAC
_OUT2
H8
P3
PA6
I/O
FT
(5)
SPI1_MISO /
TIM8_BKIN/TIM13_CH1 /
DCMI_PIXCLK / TIM3_CH1
/ TIM1_BKIN/ EVENTOUT
ADC12_IN6
J8
R3
PA7
I/O
FT
(5)
SPI1_MOSI/ TIM8_CH1N /
TIM14_CH1/TIM3_CH2/
ETH_MII_RX_DV /
TIM1_CH1N /
ETH_RMII_CRS_DV/
EVENTOUT
ADC12_IN7
-
N5
PC4
I/O
FT
(5)
ETH_RMII_RX_D0 /
ETH_MII_RX_D0/
EVENTOUT
ADC12_IN14
-
P5
PC5
I/O
FT
(5)
ETH_RMII_RX_D1 /
ETH_MII_RX_D1/
EVENTOUT
ADC12_IN15
G7
R5
PB0
I/O
FT
(5)
TIM3_CH3 / TIM8_CH2N/
OTG_HS_ULPI_D1/
ETH_MII_RXD2 /
TIM1_CH2N/ EVENTOUT
ADC12_IN8
H7
R4
PB1
I/O
FT
(5)
TIM3_CH4 / TIM8_CH3N/
OTG_HS_ULPI_D2/
ETH_MII_RXD3 /
TIM1_CH3N/ EVENTOUT
ADC12_IN9
J7
M6
PB2/BOOT1
(PB2)
I/O
FT
-
EVENTOUT
-
Table 7. STM32F40xxx pin and ball definitions(1) (continued)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176

---

Pinouts and pin description
-
-
-
R6
PF11
I/O
FT
-
DCMI_D12/ EVENTOUT
-
-
-
-
P6
PF12
I/O
FT
-
FSMC_A6/ EVENTOUT
-
-
-
-
M8
VSS
S
-
-
-
-
-
-
-
N8
VDD
S
-
-
-
-
-
-
-
N6
PF13
I/O
FT
-
FSMC_A7/ EVENTOUT
-
-
-
-
R7
PF14
I/O
FT
-
FSMC_A8/ EVENTOUT
-
-
-
-
P7
PF15
I/O
FT
-
FSMC_A9/ EVENTOUT
-
-
-
-
N7
PG0
I/O
FT
-
FSMC_A10/ EVENTOUT
-
-
-
-
M7
PG1
I/O
FT
-
FSMC_A11/ EVENTOUT
-
-
G6
R8
PE7
I/O
FT
-
FSMC_D4/TIM1_ETR/
EVENTOUT
-
-
H6
P8
PE8
I/O
FT
-
FSMC_D5/ TIM1_CH1N/
EVENTOUT
-
-
J6
P9
PE9
I/O
FT
-
FSMC_D6/TIM1_CH1/
EVENTOUT
-
-
-
-
M9
VSS
S
-
-
-
-
-
-
-
N9
VDD
S
-
-
-
-
-
F6
R9
PE10
I/O
FT
-
FSMC_D7/TIM1_CH2N/
EVENTOUT
-
-
J5
P10
PE11
I/O
FT
-
FSMC_D8/TIM1_CH2/
EVENTOUT
-
-
H5
R10
PE12
I/O
FT
-
FSMC_D9/TIM1_CH3N/
EVENTOUT
-
-
G5
N11
PE13
I/O
FT
-
FSMC_D10/TIM1_CH3/
EVENTOUT
-
-
F5
P11
PE14
I/O
FT
-
FSMC_D11/TIM1_CH4/
EVENTOUT
-
-
G4
R11
PE15
I/O
FT
-
FSMC_D12/TIM1_BKIN/
EVENTOUT
-
Table 7. STM32F40xxx pin and ball definitions(1) (continued)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176

---

Pinouts and pin description
H4
R12
PB10
I/O
FT
-
SPI2_SCK / I2S2_CK /
I2C2_SCL/ USART3_TX /
OTG_HS_ULPI_D3 /
ETH_MII_RX_ER /
TIM2_CH3/ EVENTOUT
-
J4
R13
PB11
I/O
FT
-
I2C2_SDA/USART3_RX/
OTG_HS_ULPI_D4 /
ETH_RMII_TX_EN/
ETH_MII_TX_EN /
TIM2_CH4/ EVENTOUT
-
F4
M10
VCAP_1
S
-
-
-
-
-
N10
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
M11
PH6
I/O
FT
-
I2C2_SMBA / TIM12_CH1 /
ETH_MII_RXD2/
EVENTOUT
-
-
-
-
-
N12
PH7
I/O
FT
-
I2C3_SCL /
ETH_MII_RXD3/
EVENTOUT
-
-
-
-
-
M12
PH8
I/O
FT
-
I2C3_SDA /
DCMI_HSYNC/
EVENTOUT
-
-
-
-
-
M13
PH9
I/O
FT
-
I2C3_SMBA / TIM12_CH2/
DCMI_D0/ EVENTOUT
-
-
-
-
-
L13
PH10
I/O
FT
-
TIM5_CH1 / DCMI_D1/
EVENTOUT
-
-
-
-
-
L12
PH11
I/O
FT
-
TIM5_CH2 / DCMI_D2/
EVENTOUT
-
-
-
-
-
K12
PH12
I/O
FT
-
TIM5_CH3 / DCMI_D3/
EVENTOUT
-
-
-
-
-
H12
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
J12
VDD
S
-
-
-
-
Table 7. STM32F40xxx pin and ball definitions(1) (continued)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176

---

Pinouts and pin description
J3
P12
PB12
I/O
FT
-
SPI2_NSS / I2S2_WS /
I2C2_SMBA/
USART3_CK/ TIM1_BKIN /
CAN2_RX /
OTG_HS_ULPI_D5/
ETH_RMII_TXD0 /
ETH_MII_TXD0/
OTG_HS_ID/ EVENTOUT
-
J1
P13
PB13
I/O
FT
-
SPI2_SCK / I2S2_CK /
USART3_CTS/
TIM1_CH1N /CAN2_TX /
OTG_HS_ULPI_D6 /
ETH_RMII_TXD1 /
ETH_MII_TXD1/
EVENTOUT
OTG_HS_VBUS
J2
R14
PB14
I/O
FT
-
SPI2_MISO/ TIM1_CH2N /
TIM12_CH1 /
OTG_HS_DM/
USART3_RTS /
TIM8_CH2N/I2S2ext_SD/
EVENTOUT
-
H1
R15
PB15
I/O
FT
-
SPI2_MOSI / I2S2_SD/
TIM1_CH3N / TIM8_CH3N
/ TIM12_CH2 /
OTG_HS_DP/ EVENTOUT
RTC_REFIN
-
H2
P15
PD8
I/O
FT
-
FSMC_D13 / USART3_TX/
EVENTOUT
-
-
H3
P14
PD9
I/O
FT
-
FSMC_D14 / USART3_RX/
EVENTOUT
-
-
G3
N15
PD10
I/O
FT
-
FSMC_D15 / USART3_CK/
EVENTOUT
-
-
G1
N14
PD11
I/O
FT
-
FSMC_CLE /
FSMC_A16/USART3_CTS/
EVENTOUT
-
-
G2
N13
PD12
I/O
FT
-
FSMC_ALE/
FSMC_A17/TIM4_CH1 /
USART3_RTS/
EVENTOUT
-
Table 7. STM32F40xxx pin and ball definitions(1) (continued)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176

---

Pinouts and pin description
-
-
M15 101
PD13
I/O
FT
-
FSMC_A18/TIM4_CH2/
EVENTOUT
-
-
-
-
-
VSS
S
-
-
-
-
-
-
-
J13
VDD
S
-
-
-
-
-
F2
M14 104
PD14
I/O
FT
-
FSMC_D0/TIM4_CH3/
EVENTOUT/ EVENTOUT
-
-
F1
L14
PD15
I/O
FT
-
FSMC_D1/TIM4_CH4/
EVENTOUT
-
-
-
-
L15
PG2
I/O
FT
-
FSMC_A12/ EVENTOUT
-
-
-
-
K15
PG3
I/O
FT
-
FSMC_A13/ EVENTOUT
-
-
-
-
K14
PG4
I/O
FT
-
FSMC_A14/ EVENTOUT
-
-
-
-
K13
PG5
I/O
FT
-
FSMC_A15/ EVENTOUT
-
-
-
-
J15
PG6
I/O
FT
-
FSMC_INT2/ EVENTOUT
-
-
-
-
J14
PG7
I/O
FT
-
FSMC_INT3 /USART6_CK/
EVENTOUT
-
-
-
-
H14
PG8
I/O
FT
-
USART6_RTS /
ETH_PPS_OUT/
EVENTOUT
-
-
-
-
G12
VSS
S
-
-
-
-
-
-
H13
VDD
S
-
-
-
F3
H15
PC6
I/O
FT
-
I2S2_MCK /
TIM8_CH1/SDIO_D6 /
USART6_TX /
DCMI_D0/TIM3_CH1/
EVENTOUT
-
E1
G15
PC7
I/O
FT
-
I2S3_MCK /
TIM8_CH2/SDIO_D7 /
USART6_RX /
DCMI_D1/TIM3_CH2/
EVENTOUT
-
E2
G14
PC8
I/O
FT
-
TIM8_CH3/SDIO_D0
/TIM3_CH3/ USART6_CK /
DCMI_D2/ EVENTOUT
-
Table 7. STM32F40xxx pin and ball definitions(1) (continued)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176

---

Pinouts and pin description
E3
F14
PC9
I/O
FT
-
I2S_CKIN/ MCO2 /
TIM8_CH4/SDIO_D1 /
/I2C3_SDA / DCMI_D3 /
TIM3_CH4/ EVENTOUT
-
D1
67 100
F15
PA8
I/O
FT
-
MCO1 / USART1_CK/
TIM1_CH1/ I2C3_SCL/
OTG_FS_SOF/
EVENTOUT
-
D2
68 101
E15
PA9
I/O
FT
-
USART1_TX/ TIM1_CH2 /
I2C3_SMBA / DCMI_D0/
EVENTOUT
OTG_FS_VBUS
D3
69 102 D15
PA10
I/O
FT
-
USART1_RX/ TIM1_CH3/
OTG_FS_ID/DCMI_D1/
EVENTOUT
-
C1
70 103 C15
PA11
I/O
FT
-
USART1_CTS / CAN1_RX
/ TIM1_CH4 /
OTG_FS_DM/ EVENTOUT
-
C2
71 104
B15
PA12
I/O
FT
-
USART1_RTS / CAN1_TX/
TIM1_ETR/ OTG_FS_DP/
EVENTOUT
-
D4
72 105
A15
PA13
(JTMS-SWDIO)
I/O
FT
-
JTMS-SWDIO/ EVENTOUT
-
B1
73 106
F13
VCAP_2
S
-
-
-
-
-
E7
74 107
F12
VSS
S
-
-
-
-
E6
75 108 G13
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
E12
PH13
I/O
FT
-
TIM8_CH1N / CAN1_TX/
EVENTOUT
-
-
-
-
-
E13
PH14
I/O
FT
-
TIM8_CH2N / DCMI_D4/
EVENTOUT
-
-
-
-
-
D13
PH15
I/O
FT
-
TIM8_CH3N / DCMI_D11/
EVENTOUT
-
-
C3
-
-
E14
PI0
I/O
FT
-
TIM5_CH4 / SPI2_NSS /
I2S2_WS / DCMI_D13/
EVENTOUT
-
-
B2
-
-
D14
PI1
I/O
FT
-
SPI2_SCK / I2S2_CK /
DCMI_D8/ EVENTOUT
-
Table 7. STM32F40xxx pin and ball definitions(1) (continued)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176

---

Pinouts and pin description
-
-
-
-
C14
PI2
I/O
FT
-
TIM8_CH4 /SPI2_MISO /
DCMI_D9 / I2S2ext_SD/
EVENTOUT
-
-
-
-
-
C13
PI3
I/O
FT
-
TIM8_ETR / SPI2_MOSI /
I2S2_SD / DCMI_D10/
EVENTOUT
-
-
-
-
-
D9
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
C9
VDD
S
-
-
-
-
A2
76 109
A14
PA14
(JTCK/SWCLK)
I/O
FT
-
JTCK-SWCLK/ EVENTOUT
-
B3
77 110
A13
PA15
(JTDI)
I/O
FT
-
JTDI/ SPI3_NSS/
I2S3_WS/TIM2_CH1_ETR
/ SPI1_NSS / EVENTOUT
-
D5
B14
PC10
I/O
FT
-
SPI3_SCK / I2S3_CK/
UART4_TX/SDIO_D2 /
DCMI_D8 / USART3_TX/
EVENTOUT
-
C4
79 112
B13
PC11
I/O
FT
-
UART4_RX/ SPI3_MISO /
SDIO_D3 /
DCMI_D4/USART3_RX /
I2S3ext_SD/ EVENTOUT
-
A3
80 113
A12
PC12
I/O
FT
-
UART5_TX/SDIO_CK /
DCMI_D9 / SPI3_MOSI
/I2S3_SD / USART3_CK/
EVENTOUT
-
-
D6
81 114
B12
PD0
I/O
FT
-
FSMC_D2/CAN1_RX/
EVENTOUT
-
-
C5
82 115
C12
PD1
I/O
FT
-
FSMC_D3 / CAN1_TX/
EVENTOUT
-
B4
83 116
D12
PD2
I/O
FT
-
TIM3_ETR/UART5_RX/
SDIO_CMD / DCMI_D11/
EVENTOUT
-
-
-
84 117
D11
PD3
I/O
FT
-
FSMC_CLK/
USART2_CTS/
EVENTOUT
-
Table 7. STM32F40xxx pin and ball definitions(1) (continued)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176

---

Pinouts and pin description
-
A4
85 118
D10
PD4
I/O
FT
-
FSMC_NOE/
USART2_RTS/
EVENTOUT
-
-
C6
86 119
C11
PD5
I/O
FT
-
FSMC_NWE/USART2_TX/
EVENTOUT
-
-
-
-
D8
VSS
S
-
-
-
-
-
-
-
C8
VDD
S
-
-
-
-
-
B5
87 122
B11
PD6
I/O
FT
-
FSMC_NWAIT/
USART2_RX/ EVENTOUT
-
-
A5
88 123
A11
PD7
I/O
FT
-
USART2_CK/FSMC_NE1/
FSMC_NCE2/ EVENTOUT
-
-
-
-
124 C10
PG9
I/O
FT
-
USART6_RX /
FSMC_NE2/FSMC_NCE3/
EVENTOUT
-
-
-
-
B10
PG10
I/O
FT
-
FSMC_NCE4_1/
FSMC_NE3/ EVENTOUT
-
-
-
-
B9
PG11
I/O
FT
-
FSMC_NCE4_2 /
ETH_MII_TX_EN/
ETH _RMII_TX_EN/
EVENTOUT
-
-
-
-
B8
PG12
I/O
FT
-
FSMC_NE4 /
USART6_RTS/
EVENTOUT
-
-
-
-
A8
PG13
I/O
FT
-
FSMC_A24 /
USART6_CTS
/ETH_MII_TXD0/
ETH_RMII_TXD0/
EVENTOUT
-
-
-
-
A7
PG14
I/O
FT
-
FSMC_A25 / USART6_TX
/ETH_MII_TXD1/
ETH_RMII_TXD1/
EVENTOUT
-
-
E8
-
D7
VSS
S
-
-
-
-
-
F7
-
C7
VDD
S
-
-
-
-
-
-
-
B7
PG15
I/O
FT
-
USART6_CTS /
DCMI_D13/ EVENTOUT
-
Table 7. STM32F40xxx pin and ball definitions(1) (continued)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176

---

Pinouts and pin description
B6
89 133
A10
PB3
(JTDO/
TRACESWO)
I/O
FT
-
JTDO/ TRACESWO/
SPI3_SCK / I2S3_CK /
TIM2_CH2 / SPI1_SCK/
EVENTOUT
-
A6
90 134
A9
PB4
(NJTRST)
I/O
FT
-
NJTRST/ SPI3_MISO /
TIM3_CH1 / SPI1_MISO /
I2S3ext_SD/ EVENTOUT
-
D7
91 135
A6
PB5
I/O
FT
-
I2C1_SMBA/ CAN2_RX /
OTG_HS_ULPI_D7 /
ETH_PPS_OUT/TIM3_CH2
/ SPI1_MOSI/ SPI3_MOSI /
DCMI_D10 / I2S3_SD/
EVENTOUT
-
C7
92 136
B6
PB6
I/O
FT
-
I2C1_SCL/ TIM4_CH1 /
CAN2_TX /
DCMI_D5/USART1_TX/
EVENTOUT
-
B7
93 137
B5
PB7
I/O
FT
-
I2C1_SDA / FSMC_NL /
DCMI_VSYNC /
USART1_RX/ TIM4_CH2/
EVENTOUT
-
A7
94 138
D6
BOOT0
I
B
-
-
VPP
D8
95 139
A5
PB8
I/O
FT
-
TIM4_CH3/SDIO_D4/
TIM10_CH1 / DCMI_D6 /
ETH_MII_TXD3 /
I2C1_SCL/ CAN1_RX/
EVENTOUT
-
C8
96 140
B4
PB9
I/O
FT
-
SPI2_NSS/ I2S2_WS /
TIM4_CH4/ TIM11_CH1/
SDIO_D5 / DCMI_D7 /
I2C1_SDA / CAN1_TX/
EVENTOUT
-
-
-
97 141
A4
PE0
I/O
FT
-
TIM4_ETR / FSMC_NBL0 /
DCMI_D2/ EVENTOUT
-
-
-
98 142
A3
PE1
I/O
FT
-
FSMC_NBL1 / DCMI_D3/
EVENTOUT
-
-
-
D5
-
VSS
S
-
-
-
-
Table 7. STM32F40xxx pin and ball definitions(1) (continued)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176

---

Pinouts and pin description
-
A8
-
C6
PDR_ON
I
FT
-
-
-
A1
C5
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
D4
PI4
I/O
FT
-
TIM8_BKIN / DCMI_D5/
EVENTOUT
-
-
-
-
-
C4
PI5
I/O
FT
-
TIM8_CH1 /
DCMI_VSYNC/
EVENTOUT
-
-
-
-
-
C3
PI6
I/O
FT
-
TIM8_CH2 / DCMI_D6/
EVENTOUT
-
-
-
-
-
C2
PI7
I/O
FT
-
TIM8_CH3 / DCMI_D7/
EVENTOUT
-
1.
UFBGA176 F6, F7, F8, F9, F10, G6, G7, G8, G9, G10, H6, H7, H8, H9, H10, J6, J7, J8, J9, J10, K6, K7, K8, K9 and K10
balls are connected to VSS for heat dissipation and package mechanical stability.
2.
Function availability depends on the chosen device.
3.
PC13, PC14, PC15 and PI8 are supplied through the power switch. Since the switch only sinks a limited amount of current
(3 mA), the use of GPIOs PC13 to PC15 and PI8 in output mode is limited:
- The speed should not exceed 2 MHz with a maximum load of 30 pF.
- These I/Os must not be used as a current source (e.g. to drive an LED).
4.
Main function after the first backup domain power-up. Later on, it depends on the contents of the RTC registers even after
reset (because these registers are not reset by the main reset). For details on how to manage these I/Os, refer to the RTC
register description sections in the STM32F4xx reference manual, available from the STMicroelectronics website:
www.st.com.
5.
FT = 5 V tolerant except when in analog mode or oscillator mode (for PC14, PC15, PH0 and PH1).
6.
If the device is delivered in an UFBGA176 or WLCSP90 and the BYPASS_REG pin is set to VDD (Regulator off/internal
reset ON mode), then PA0 is used as an internal Reset (active low).
Table 7. STM32F40xxx pin and ball definitions(1) (continued)
Pin number
Pin name
(function after
reset)(2)
Pin type
I / O structure
Notes
Alternate functions
Additional
functions
LQFP64
WLCSP90
LQFP100
LQFP144
UFBGA176
LQFP176
Table 8. FSMC pin definition
Pins(1)
FSMC
LQFP100(2)
WLCSP90
(2)
CF
NOR/PSRAM/
SRAM
NOR/PSRAM Mux
NAND 16 bit
PE2
-
A23
A23
-
Yes
-
PE3
-
A19
A19
-
Yes
-
PE4
-
A20
A20
-
Yes
-
PE5
-
A21
A21
-
Yes
-
PE6
-
A22
A22
-
Yes
-

---

Pinouts and pin description
PF0
A0
A0
-
-
-
-
PF1
A1
A1
-
-
-
-
PF2
A2
A2
-
-
-
-
PF3
A3
A3
-
-
-
-
PF4
A4
A4
-
-
-
-
PF5
A5
A5
-
-
-
-
PF6
NIORD
-
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
-
PF8
NIOWR
-
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
-
PF10
INTR
-
-
-
-
-
PF12
A6
A6
-
-
-
-
PF13
A7
A7
-
-
-
-
PF14
A8
A8
-
-
-
-
PF15
A9
A9
-
-
-
-
PG0
A10
A10
-
-
-
-
PG1
-
A11
-
-
-
-
PE7
D4
D4
DA4
D4
Yes
Yes
PE8
D5
D5
DA5
D5
Yes
Yes
PE9
D6
D6
DA6
D6
Yes
Yes
PE10
D7
D7
DA7
D7
Yes
Yes
PE11
D8
D8
DA8
D8
Yes
Yes
PE12
D9
D9
DA9
D9
Yes
Yes
PE13
D10
D10
DA10
D10
Yes
Yes
PE14
D11
D11
DA11
D11
Yes
Yes
PE15
D12
D12
DA12
D12
Yes
Yes
PD8
D13
D13
DA13
D13
Yes
Yes
PD9
D14
D14
DA14
D14
Yes
Yes
PD10
D15
D15
DA15
D15
Yes
Yes
PD11
-
A16
A16
CLE
Yes
Yes
PD12
-
A17
A17
ALE
Yes
Yes
PD13
-
A18
A18
-
Yes
-
PD14
D0
D0
DA0
D0
Yes
Yes
Table 8. FSMC pin definition (continued)
Pins(1)
FSMC
LQFP100(2)
WLCSP90
(2)
CF
NOR/PSRAM/
SRAM
NOR/PSRAM Mux
NAND 16 bit

---

Pinouts and pin description
PD15
D1
D1
DA1
D1
Yes
Yes
PG2
-
A12
-
-
-
-
PG3
-
A13
-
-
-
-
PG4
-
A14
-
-
-
-
PG5
-
A15
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
-
PG7
-
-
-
INT3
-
-
PD0
D2
D2
DA2
D2
Yes
Yes
PD1
D3
D3
DA3
D3
Yes
Yes
PD3
-
CLK
CLK
-
Yes
-
PD4
NOE
NOE
NOE
NOE
Yes
Yes
PD5
NWE
NWE
NWE
NWE
Yes
Yes
PD6
NWAIT
NWAIT
NWAIT
NWAIT
Yes
Yes
PD7
-
NE1
NE1
NCE2
Yes
Yes
PG9
-
NE2
NE2
NCE3
-
-
PG10
NCE4_1
NE3
NE3
-
-
-
PG11
NCE4_2
-
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
-
PG13
-
A24
A24
-
-
-
PG14
-
A25
A25
-
-
-
PB7
-
NADV
NADV
-
Yes
Yes
PE0
-
NBL0
NBL0
-
Yes
-
PE1
-
NBL1
NBL1
-
Yes
-
1.
Full FSMC features are available on LQFP144, LQFP176, and UFBGA176. The features available on
smaller packages are given in the dedicated package column.
2.
Ports F and G are not available in devices delivered in 100-pin packages.
Table 8. FSMC pin definition (continued)
Pins(1)
FSMC
LQFP100(2)
WLCSP90
(2)
CF
NOR/PSRAM/
SRAM
NOR/PSRAM Mux
NAND 16 bit

---

Pinouts and pin description
Table 9. Alternate function mapping
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
TIM8/9/10
/11
I2C1/2/3
SPI1/SPI2/
I2S2/I2S2e
xt
SPI3/I2Sext
/I2S3
USART1/2/3/
I2S3ext
UART4/5/
USART6
CAN1/2
TIM12/13/
OTG_FS/
OTG_HS
ETH
FSMC/SDIO
/OTG_FS
DCMI
Port A
PA0
-
TIM2_CH1_
ETR
TIM 5_CH1
TIM8_ETR
-
-
-
USART2_CTS
UART4_TX
-
-
ETH_MII_CRS
-
-
-
EVENTOUT
PA1
-
TIM2_CH2
TIM5_CH2
-
-
-
-
USART2_RTS
UART4_RX
-
-
ETH_MII
_RX_CLK
ETH_RMII__REF
_CLK
-
-
-
EVENTOUT
PA2
-
TIM2_CH3
TIM5_CH3
TIM9_CH1
-
-
-
USART2_TX
-
-
-
ETH_MDIO
-
-
-
EVENTOUT
PA3
-
TIM2_CH4
TIM5_CH4
TIM9_CH2
-
-
-
USART2_RX
-
-
OTG_HS_ULPI_
D0
ETH _MII_COL
-
-
-
EVENTOUT
PA4
-
-
-
-
-
SPI1_NSS
SPI3_NSS
I2S3_WS
USART2_CK
-
-
-
-
OTG_HS_SOF
DCMI_
HSYNC
-
EVENTOUT
PA5
-
TIM2_CH1_
ETR
-
TIM8_CH1N
-
SPI1_SCK
-
-
-
-
OTG_HS_ULPI_
CK
-
-
-
-
EVENTOUT
PA6
-
TIM1_BKIN
TIM3_CH1
TIM8_BKIN
-
SPI1_MISO
-
-
-
TIM13_CH1
-
-
-
DCMI_
PIXCLK
-
EVENTOUT
PA7
-
TIM1_CH1N
TIM3_CH2
TIM8_CH1N
-
SPI1_MOSI
-
-
-
TIM14_CH1
-
ETH_MII _RX_DV
ETH_RMII
_CRS_DV
-
-
-
EVENTOUT
PA8
MCO1
TIM1_CH1
-
-
I2C3_SCL
-
-
USART1_CK
-
-
OTG_FS_SOF
-
-
-
-
EVENTOUT
PA9
-
TIM1_CH2
-
-
I2C3_
SMBA
-
-
USART1_TX
-
-
-
-
-
DCMI_D0
-
EVENTOUT
PA10
-
TIM1_CH3
-
-
-
-
-
USART1_RX
-
-
OTG_FS_ID
-
-
DCMI_D1
-
EVENTOUT
PA11
-
TIM1_CH4
-
-
-
-
-
USART1_CTS
-
CAN1_RX
OTG_FS_DM
-
-
-
-
EVENTOUT
PA12
-
TIM1_ETR
-
-
-
-
-
USART1_RTS
-
CAN1_TX
OTG_FS_DP
-
-
-
-
EVENTOUT
PA13
JTMS-
SWDIO
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
EVENTOUT
PA14
JTCK-
SWCLK
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
EVENTOUT
PA15
JTDI
TIM 2_CH1
TIM 2_ETR
-
-
-
SPI1_NSS
SPI3_NSS/
I2S3_WS
-
-
-
-
-
-
-
-
EVENTOUT

---

Pinouts and pin description
Port B
PB0
-
TIM1_CH2N
TIM3_CH3
TIM8_CH2N
-
-
-
-
-
-
OTG_HS_ULPI_
D1
ETH _MII_RXD2
-
-
-
EVENTOUT
PB1
-
TIM1_CH3N
TIM3_CH4
TIM8_CH3N
-
-
-
-
-
OTG_HS_ULPI_
D2
ETH _MII_RXD3
-
-
-
EVENTOUT
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
EVENTOUT
PB3
JTDO/
TRACES
WO
TIM2_CH2
-
-
-
SPI1_SCK
SPI3_SCK
I2S3_CK
-
-
-
-
-
-
-
-
EVENTOUT
PB4
NJTRST
-
TIM3_CH1
-
SPI1_MISO
SPI3_MISO
I2S3ext_SD
-
-
-
-
-
-
-
EVENTOUT
PB5
-
-
TIM3_CH2
I2C1_SMB
A
SPI1_MOSI
SPI3_MOSI
I2S3_SD
-
-
CAN2_RX
OTG_HS_ULPI_
D7
ETH _PPS_OUT
-
DCMI_D10
-
EVENTOUT
PB6
-
-
TIM4_CH1
I2C1_SCL
-
-
USART1_TX
-
CAN2_TX
-
-
-
DCMI_D5
-
EVENTOUT
PB7
-
-
TIM4_CH2
I2C1_SDA
-
-
USART1_RX
-
-
-
-
FSMC_NL
DCMI_VSYN
C
-
EVENTOUT
PB8
-
-
TIM4_CH3
TIM10_CH1
I2C1_SCL
-
-
-
-
CAN1_RX
-
ETH _MII_TXD3
SDIO_D4
DCMI_D6
-
EVENTOUT
PB9
-
-
TIM4_CH4
TIM11_CH1
I2C1_SDA
SPI2_NSS
I2S2_WS
-
-
-
CAN1_TX
-
-
SDIO_D5
DCMI_D7
-
EVENTOUT
PB10
-
TIM2_CH3
-
-
I2C2_SCL
SPI2_SCK
I2S2_CK
-
USART3_TX
-
-
OTG_HS_ULPI_
D3
ETH_ MII_RX_ER
-
-
-
EVENTOUT
PB11
-
TIM2_CH4
-
-
I2C2_SDA
-
-
USART3_RX
-
-
OTG_HS_ULPI_
D4
ETH _MII_TX_EN
ETH
_RMII_TX_EN
-
-
-
EVENTOUT
PB12
-
TIM1_BKIN
-
-
I2C2_
SMBA
SPI2_NSS
I2S2_WS
-
USART3_CK
-
CAN2_RX
OTG_HS_ULPI_
D5
ETH _MII_TXD0
ETH _RMII_TXD0
OTG_HS_ID
-
-
EVENTOUT
PB13
-
TIM1_CH1N
-
-
-
SPI2_SCK
I2S2_CK
-
USART3_CTS
-
CAN2_TX
OTG_HS_ULPI_
D6
ETH _MII_TXD1
ETH _RMII_TXD1
 -
-
-
EVENTOUT
PB14
-
TIM1_CH2N
-
TIM8_CH2N
-
SPI2_MISO
I2S2ext_SD
USART3_RTS
-
TIM12_CH1
-
-
OTG_HS_DM
-
-
EVENTOUT
PB15
RTC_
REFIN
TIM1_CH3N
-
TIM8_CH3N
-
SPI2_MOSI
I2S2_SD
-
-
-
TIM12_CH2
-
-
OTG_HS_DP
-
-
EVENTOUT
Table 9. Alternate function mapping (continued)
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
TIM8/9/10
/11
I2C1/2/3
SPI1/SPI2/
I2S2/I2S2e
xt
SPI3/I2Sext
/I2S3
USART1/2/3/
I2S3ext
UART4/5/
USART6
CAN1/2
TIM12/13/
OTG_FS/
OTG_HS
ETH
FSMC/SDIO
/OTG_FS
DCMI

---

Pinouts and pin description
Port C
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
OTG_HS_ULPI_
STP
-
-
-
-
EVENTOUT
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
EVENTOUT
PC2
-
-
-
-
-
SPI2_MISO
I2S2ext_SD
-
-
-
OTG_HS_ULPI_
DIR
ETH _MII_TXD2
-
-
-
EVENTOUT
PC3
-
-
-
-
-
SPI2_MOSI
I2S2_SD
-
-
-
-
OTG_HS_ULPI_
NXT
ETH
_MII_TX_CLK
-
-
-
EVENTOUT
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
ETH_MII_RXD0
ETH_RMII_RXD0
-
-
-
EVENTOUT
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
ETH _MII_RXD1
ETH _RMII_RXD1
-
-
-
EVENTOUT
PC6
-
-
TIM3_CH1
TIM8_CH1
I2S2_MCK
-
USART6_TX
-
-
-
SDIO_D6
DCMI_D0
-
EVENTOUT
PC7
-
-
TIM3_CH2
TIM8_CH2
-
-
I2S3_MCK
-
USART6_RX
-
-
-
SDIO_D7
DCMI_D1
-
EVENTOUT
PC8
-
-
TIM3_CH3
TIM8_CH3
-
-
-
-
USART6_CK
-
-
-
SDIO_D0
DCMI_D2
-
EVENTOUT
PC9
MCO2
-
TIM3_CH4
TIM8_CH4
I2C3_SDA
I2S_CKIN
-
-
-
-
-
-
SDIO_D1
DCMI_D3
-
EVENTOUT
PC10
-
-
-
-
-
-
SPI3_SCK/
I2S3_CK
USART3_TX/
UART4_TX
-
-
-
SDIO_D2
DCMI_D8
-
EVENTOUT
PC11
-
-
-
-
-
I2S3ext_SD
SPI3_MISO/
USART3_RX
UART4_RX
-
-
-
SDIO_D3
DCMI_D4
-
EVENTOUT
PC12
-
-
-
-
-
-
SPI3_MOSI
I2S3_SD
USART3_CK
UART5_TX
-
-
-
SDIO_CK
DCMI_D9
-
EVENTOUT
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
-
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
-
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
-
Table 9. Alternate function mapping (continued)
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
TIM8/9/10
/11
I2C1/2/3
SPI1/SPI2/
I2S2/I2S2e
xt
SPI3/I2Sext
/I2S3
USART1/2/3/
I2S3ext
UART4/5/
USART6
CAN1/2
TIM12/13/
OTG_FS/
OTG_HS
ETH
FSMC/SDIO
/OTG_FS
DCMI

---

Pinouts and pin description
Port D
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
FSMC_D2
-
-
EVENTOUT
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
FSMC_D3
-
-
EVENTOUT
PD2
-
-
TIM3_ETR
-
-
-
-
-
UART5_RX
-
-
-
SDIO_CMD
DCMI_D11
-
EVENTOUT
PD3
-
-
-
-
-
-
-
USART2_CTS
-
-
-
-
FSMC_CLK
-
-
EVENTOUT
PD4
-
-
-
-
-
-
-
USART2_RTS
-
-
-
-
FSMC_NOE
-
-
EVENTOUT
PD5
-
-
-
-
-
-
-
USART2_TX
-
-
-
-
FSMC_NWE
-
-
EVENTOUT
PD6
-
-
-
-
-
-
-
USART2_RX
-
-
-
-
FSMC_NWAIT
-
-
EVENTOUT
PD7
-
-
-
-
-
-
-
USART2_CK
-
-
-
-
FSMC_NE1/
FSMC_NCE2
-
-
EVENTOUT
PD8
-
-
-
-
-
-
-
USART3_TX
-
-
-
-
FSMC_D13
-
-
EVENTOUT
PD9
-
-
-
-
-
-
-
USART3_RX
-
-
-
-
FSMC_D14
-
-
EVENTOUT
PD10
-
-
-
-
-
-
-
USART3_CK
-
-
-
-
FSMC_D15
-
-
EVENTOUT
PD11
-
-
-
-
-
-
-
USART3_CTS
-
-
-
-
FSMC_A16
-
-
EVENTOUT
PD12
-
-
TIM4_CH1
-
-
-
-
USART3_RTS
-
-
-
-
FSMC_A17
-
-
EVENTOUT
PD13
-
-
TIM4_CH2
-
-
-
-
-
-
-
-
-
FSMC_A18
-
-
EVENTOUT
PD14
-
-
TIM4_CH3
-
-
-
-
-
-
-
-
-
FSMC_D0
-
-
EVENTOUT
PD15
-
-
TIM4_CH4
-
-
-
-
-
-
-
-
-
FSMC_D1
-
-
EVENTOUT
Table 9. Alternate function mapping (continued)
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
TIM8/9/10
/11
I2C1/2/3
SPI1/SPI2/
I2S2/I2S2e
xt
SPI3/I2Sext
/I2S3
USART1/2/3/
I2S3ext
UART4/5/
USART6
CAN1/2
TIM12/13/
OTG_FS/
OTG_HS
ETH
FSMC/SDIO
/OTG_FS
DCMI

---

Pinouts and pin description
Port E
PE0
-
-
TIM4_ETR
-
-
-
-
-
-
-
-
-
FSMC_NBL0
DCMI_D2
-
EVENTOUT
PE1
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
FSMC_NBL1
DCMI_D3
-
EVENTOUT
PE2
TRACECL
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
ETH _MII_TXD3
FSMC_A23
-
-
EVENTOUT
PE3
TRACED0
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
FSMC_A19
-
-
EVENTOUT
PE4
TRACED1
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
FSMC_A20
DCMI_D4
-
EVENTOUT
PE5
TRACED2
-
-
TIM9_CH1
-
-
-
-
-
-
-
-
FSMC_A21
DCMI_D6
-
EVENTOUT
PE6
TRACED3
-
-
TIM9_CH2
-
-
-
-
-
-
-
-
FSMC_A22
DCMI_D7
-
EVENTOUT
PE7
-
TIM1_ETR
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
FSMC_D4
-
-
EVENTOUT
PE8
-
TIM1_CH1N
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
FSMC_D5
-
-
EVENTOUT
PE9
-
TIM1_CH1
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
FSMC_D6
-
-
EVENTOUT
PE10
-
TIM1_CH2N
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
FSMC_D7
-
-
EVENTOUT
PE11
-
TIM1_CH2
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
FSMC_D8
-
-
EVENTOUT
PE12
-
TIM1_CH3N
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
FSMC_D9
-
-
EVENTOUT
PE13
-
TIM1_CH3
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
FSMC_D10
-
-
EVENTOUT
PE14
-
TIM1_CH4
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
FSMC_D11
-
-
EVENTOUT
PE15
-
TIM1_BKIN
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
FSMC_D12
-
-
EVENTOUT
Table 9. Alternate function mapping (continued)
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
TIM8/9/10
/11
I2C1/2/3
SPI1/SPI2/
I2S2/I2S2e
xt
SPI3/I2Sext
/I2S3
USART1/2/3/
I2S3ext
UART4/5/
USART6
CAN1/2
TIM12/13/
OTG_FS/
OTG_HS
ETH
FSMC/SDIO
/OTG_FS
DCMI

---

Pinouts and pin description
Port F
PF0
-
-
-
-
I2C2_SDA
-
-
-
-
-
-
-
FSMC_A0
-
-
EVENTOUT
PF1
-
-
-
-
I2C2_SCL
-
-
-
-
-
-
-
FSMC_A1
-
-
EVENTOUT
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
FSMC_A2
-
-
EVENTOUT
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
-
FSMC_A3
-
-
EVENTOUT
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
-
FSMC_A4
-
-
EVENTOUT
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
-
FSMC_A5
-
-
EVENTOUT
PF6
-
-
-
TIM10_CH1
-
-
-
-
-
-
-
-
FSMC_NIORD
-
-
EVENTOUT
PF7
-
-
-
TIM11_CH1
-
-
-
-
-
-
-
-
FSMC_NREG
-
-
EVENTOUT
PF8
-
-
-
-
-
-
-
-
-
TIM13_CH1
-
-
FSMC_
NIOWR
-
-
EVENTOUT
PF9
-
-
-
-
-
-
-
-
-
TIM14_CH1
-
-
FSMC_CD
-
-
EVENTOUT
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
FSMC_INTR
-
-
EVENTOUT
PF11
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
DCMI_D12
-
EVENTOUT
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
FSMC_A6
-
-
EVENTOUT
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
FSMC_A7
-
-
EVENTOUT
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
FSMC_A8
-
-
EVENTOUT
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
FSMC_A9
-
-
EVENTOUT
Table 9. Alternate function mapping (continued)
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
TIM8/9/10
/11
I2C1/2/3
SPI1/SPI2/
I2S2/I2S2e
xt
SPI3/I2Sext
/I2S3
USART1/2/3/
I2S3ext
UART4/5/
USART6
CAN1/2
TIM12/13/
OTG_FS/
OTG_HS
ETH
FSMC/SDIO
/OTG_FS
DCMI

---

Pinouts and pin description
Port G
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
FSMC_A10
-
-
EVENTOUT
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
FSMC_A11
-
-
EVENTOUT
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
FSMC_A12
-
-
EVENTOUT
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
FSMC_A13
-
-
EVENTOUT
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
FSMC_A14
-
-
EVENTOUT
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
FSMC_A15
-
-
EVENTOUT
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
FSMC_INT2
-
-
EVENTOUT
PG7
-
-
-
-
-
-
-
-
USART6_CK
-
-
-
FSMC_INT3
-
-
EVENTOUT
PG8
-
-
-
-
-
-
-
-
USART6_
RTS
-
-
ETH _PPS_OUT
-
-
-
EVENTOUT
PG9
-
-
-
-
-
-
-
-
USART6_RX
-
-
-
FSMC_NE2/
FSMC_NCE3
-
-
EVENTOUT
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
-
-
-
FSMC_
NCE4_1/
FSMC_NE3
-
-
EVENTOUT
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
ETH _MII_TX_EN
ETH _RMII_
TX_EN
FSMC_NCE4_
-
-
EVENTOUT
PG12
-
-
-
-
-
-
-
-
USART6_
RTS
-
-
-
FSMC_NE4
-
-
EVENTOUT
PG13
-
-
-
-
-
-
-
-
UART6_CTS
-
-
ETH _MII_TXD0
ETH _RMII_TXD0
FSMC_A24
-
-
EVENTOUT
PG14
-
-
-
-
-
-
-
-
USART6_TX
-
-
ETH _MII_TXD1
ETH _RMII_TXD1
FSMC_A25
-
-
EVENTOUT
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
-
DCMI_D13
-
EVENTOUT
Table 9. Alternate function mapping (continued)
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
TIM8/9/10
/11
I2C1/2/3
SPI1/SPI2/
I2S2/I2S2e
xt
SPI3/I2Sext
/I2S3
USART1/2/3/
I2S3ext
UART4/5/
USART6
CAN1/2
TIM12/13/
OTG_FS/
OTG_HS
ETH
FSMC/SDIO
/OTG_FS
DCMI

---

Pinouts and pin description
Port H
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
EVENTOUT
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
EVENTOUT
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
ETH _MII_CRS
-
-
-
EVENTOUT
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
ETH _MII_COL
-
-
-
EVENTOUT
PH4
-
-
-
-
I2C2_SCL
-
-
-
-
-
OTG_HS_ULPI_
NXT
-
-
-
-
EVENTOUT
PH5
-
-
-
-
I2C2_SDA
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
EVENTOUT
PH6
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
TIM12_CH1
-
ETH _MII_RXD2
-
-
-
EVENTOUT
PH7
-
-
-
-
I2C3_SCL
-
-
-
-
-
-
ETH _MII_RXD3
-
-
-
EVENTOUT
PH8
-
-
-
-
I2C3_SDA
-
-
-
-
-
-
-
-
DCMI_
HSYNC
-
EVENTOUT
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
-
DCMI_D0
-
EVENTOUT
PH10
-
-
TIM5_CH1
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
DCMI_D1
-
EVENTOUT
PH11
-
-
TIM5_CH2
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
DCMI_D2
-
EVENTOUT
PH12
-
-
TIM5_CH3
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
DCMI_D3
-
EVENTOUT
PH13
-
-
-
TIM8_CH1N
-
-
-
-
-
CAN1_TX
-
-
-
-
-
EVENTOUT
PH14
-
-
-
TIM8_CH2N
-
-
-
-
-
-
-
-
-
DCMI_D4
-
EVENTOUT
PH15
-
-
-
TIM8_CH3N
-
-
-
-
-
-
-
-
-
DCMI_D11
-
EVENTOUT
Table 9. Alternate function mapping (continued)
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
TIM8/9/10
/11
I2C1/2/3
SPI1/SPI2/
I2S2/I2S2e
xt
SPI3/I2Sext
/I2S3
USART1/2/3/
I2S3ext
UART4/5/
USART6
CAN1/2
TIM12/13/
OTG_FS/
OTG_HS
ETH
FSMC/SDIO
/OTG_FS
DCMI

---

Pinouts and pin description
Port I
PI0
-
-
TIM5_CH4
-
-
SPI2_NSS
I2S2_WS
-
-
-
-
-
-
-
DCMI_D13
-
EVENTOUT
PI1
-
-
-
-
-
SPI2_SCK
I2S2_CK
-
-
-
-
-
-
-
DCMI_D8
-
EVENTOUT
PI2
-
-
-
TIM8_CH4
-
SPI2_MISO
I2S2ext_SD
-
-
-
-
-
-
DCMI_D9
-
EVENTOUT
PI3
-
-
-
TIM8_ETR
-
SPI2_MOSI
I2S2_SD
-
-
-
-
-
-
-
DCMI_D10
-
EVENTOUT
PI4
-
-
-
TIM8_BKIN
-
-
-
-
-
-
-
-
-
DCMI_D5
-
EVENTOUT
PI5
-
-
-
TIM8_CH1
-
-
-
-
-
-
-
-
-
DCMI_
VSYNC
-
EVENTOUT
PI6
-
-
-
TIM8_CH2
-
-
-
-
-
-
-
-
-
DCMI_D6
-
EVENTOUT
PI7
-
-
-
TIM8_CH3
-
-
-
-
-
-
-
-
-
DCMI_D7
-
EVENTOUT
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
-
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
-
-
-
EVENTOUT
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
ETH _MII_RX_ER
-
-
-
EVENTOUT
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
OTG_HS_ULPI_
DIR
-
-
-
-
EVENTOUT
Table 9. Alternate function mapping (continued)
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
TIM8/9/10
/11
I2C1/2/3
SPI1/SPI2/
I2S2/I2S2e
xt
SPI3/I2Sext
/I2S3
USART1/2/3/
I2S3ext
UART4/5/
USART6
CAN1/2
TIM12/13/
OTG_FS/
OTG_HS
ETH
FSMC/SDIO
/OTG_FS
DCMI

---

Memory mapping
Memory mapping
The memory map is shown in Figure 18.
Figure 18. STM32F40xxx memory map
512-Mbyte
block 7
Cortex-M4's
internal
peripherals
512-Mbyte
block 6
Not used
512-Mbyte
block 5
FSMC registers
512-Mbyte
block 4
FSMC bank 3
& bank4
512-Mbyte
block 3
FSMC bank1
& bank2
512-Mbyte
block 2
Peripherals
512-Mbyte
block 1
SRAM
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
0xBFFF FFFF
0xC000 0000
0xDFFF FFFF
0xE000 0000
0xFFFF FFFF
512-Mbyte
block 0
Code
Flash
0x0810 0000 - 0x0FFF FFFF
0x1FFF 0000 - 0x1FFF 7A0F
0x1FFF C000 - 0x1FFF C007
0x0800 0000  - 0x080F FFFF
0x0010 0000 - 0x07FF FFFF
0x0000 0000 - 0x000F FFFF
System memory + OTP
Reserved
Reserved
Aliased to Flash, system
memory or SRAM depending
on the BOOT pins
SRAM (16 KB aliased
by bit-banding)
Reserved
0x2000 0000 - 0x2001 BFFF
0x2001 C000 - 0x2001 FFFF
0x2002 0000 - 0x3FFF FFFF
0x4000 0000
Reserved
0x4000 7FFF
0x4000 7800 - 0x4000 FFFF
0x4001 0000
0x4001 57FF
0x4002 0000
Reserved
0x5006 0C00 - 0x5FFF FFFF
0x6000 0000
AHB3
0xA000 0FFF
0xA000 1000 - 0xDFFF FFFF
ai18513g
Option Bytes
Reserved
0x4001 5800 - 0x4001 FFFF
0x5006 0BFF
AHB2
0x5000 0000
0x4008 0000 - 0x4FFF FFFF
Reserved
AHB1
SRAM (112 KB aliased
by bit-banding)
Reserved
0x1FFF C008 - 0x1FFF FFFF
0x1FFF 7A10 - 0x1FFF 7FFF
Reserved
CCM data RAM
(64 KB data SRAM)
0x1000 0000 - 0x1000 FFFF
Reserved
0x1001 0000 - 0x1FFE FFFF
Reserved
APB2
0x4007 FFFF
APB1
CORTEX-M4 internal peripherals
0xE000 0000 - 0xE00F FFFF
Reserved
0xE010 0000 - 0xFFFF FFFF

---

Memory mapping
Table 10. Register boundary addresses
Bus
Boundary address
Peripheral
0xE00F FFFF - 0xFFFF FFFF
Reserved
Cortex-M4
0xE000 0000 - 0xE00F FFFF
Cortex-M4 internal peripherals
0xA000 1000 - 0xDFFF FFFF
Reserved
AHB3
0xA000 0000 - 0xA000 0FFF
FSMC control register
0x9000 0000 - 0x9FFF FFFF
FSMC bank 4
0x8000 0000 - 0x8FFF FFFF
FSMC bank 3
0x7000 0000 - 0x7FFF FFFF
FSMC bank 2
0x6000 0000 - 0x6FFF FFFF
FSMC bank 1
0x5006 0C00- 0x5FFF FFFF
Reserved
AHB2
0x5006 0800 - 0x5006 0BFF
RNG
0x5005 0400 - 0x5006 07FF
Reserved
0x5005 0000 - 0x5005 03FF
DCMI
0x5004 0000- 0x5004 FFFF
Reserved
0x5000 0000 - 0x5003 FFFF
USB OTG FS
0x4008 0000- 0x4FFF FFFF
Reserved

---

Memory mapping
AHB1
0x4004 0000 - 0x4007 FFFF
USB OTG HS
0x4002 9400 - 0x4003 FFFF
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
0x4002 5000 - 0x4002 5FFF
Reserved
0x4002 4000 - 0x4002 4FFF
BKPSRAM
0x4002 3C00 - 0x4002 3FFF
Flash interface register
0x4002 3800 - 0x4002 3BFF
RCC
0x4002 3400 - 0x4002 37FF
Reserved
0x4002 3000 - 0x4002 33FF
CRC
0x4002 2400 - 0x4002 2FFF
Reserved
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
0x4002 0C00 - 0x4002 0FFF
GPIOD
0x4002 0800 - 0x4002 0BFF
GPIOC
0x4002 0400 - 0x4002 07FF
GPIOB
0x4002 0000 - 0x4002 03FF
GPIOA
0x4001 5800- 0x4001 FFFF
Reserved
Table 10. Register boundary addresses (continued)
Bus
Boundary address
Peripheral

---

Memory mapping
APB2
0x4001 4C00 - 0x4001 57FF
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
Reserved
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
0x4000 7800- 0x4000 FFFF
Reserved
Table 10. Register boundary addresses (continued)
Bus
Boundary address
Peripheral

---

Memory mapping
APB1
0x4000 7800 - 0x4000 7FFF
Reserved
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
Table 10. Register boundary addresses (continued)
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
Unless otherwise specified the minimum and maximum values are evaluated in the worst
conditions of ambient temperature, supply voltage, and frequencies by tests in production
on 100% of the devices with an ambient temperature at TA = 25 °C and TA = TAmax (given
by the selected temperature range).
Data based on characterization results, design simulation and/or technology characteristics
are indicated in the table footnotes and are not tested in production. Based on
characterization, the minimum and maximum values refer to sample tests and represent the
mean value plus or minus three times the standard deviation (mean±3Σ).
6.1.2
Typical values
Unless otherwise specified, typical data are based on TA = 25 °C, VDD = 3.3 V (for the
1.8 V ≤ VDD ≤ 3.6 V voltage range). They are given only as design guidelines and are not
tested.
Typical ADC accuracy values are determined by characterization of a batch of samples from
a standard diffusion lot over the full temperature range, where 95% of the devices have an
error less than or equal to the value indicated (mean±2Σ).
6.1.3
Typical curves
Unless otherwise specified, all typical curves are given only as design guidelines and are
not tested.
6.1.4
Loading capacitor
The loading conditions used for pin parameter measurement are shown in Figure 19.
6.1.5
Pin input voltage
The input voltage measurement on a pin of the device is described in Figure 20.
Figure 19. Pin loading conditions
Figure 20. Pin input voltage
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
Figure 21. Power supply scheme
1.
Each power supply pair must be decoupled with filtering ceramic capacitors as shown above. These capacitors must be
placed as close as possible to, or below, the appropriate pins on the underside of the PCB to ensure the good functionality
of the device.
2.
To connect BYPASS_REG and PDR_ON pins, refer to Section 3.0.16: Voltage regulator and Table 3.0.15: Power supply
supervisor.
3.
The two 2.2 µF ceramic capacitors should be replaced by two 100 nF decoupling capacitors when the voltage regulator is
OFF.
4.
The 4.7 µF ceramic capacitor must be connected to one of the VDD pin.
5.
VDDA=VDD and VSSA=VSS.
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
Figure 22. Current consumption measurement scheme
6.2
Absolute maximum ratings
Stresses above the absolute maximum ratings listed in Table 11: Voltage characteristics,
Table 12: Current characteristics, and Table 13: Thermal characteristics may cause
permanent damage to the device. These are stress ratings only and functional operation of
the device at these conditions is not implied. Exposure to maximum rating conditions for
extended periods may affect device reliability. Device mission profile (application conditions)
is compliant with JEDEC JESD47 Qualification Standard, extended mission profiles are
available on demand.
ai14126
VBAT
VDD
VDDA
IDD_VBAT
IDD
Table 11. Voltage characteristics
Symbol
Ratings
Min
Max
Unit
VDD–VSS
External main supply voltage (including VDDA, VDD)(1)
1.
All main power (VDD, VDDA) and ground (VSS, VSSA) pins must always be connected to the external power
supply, in the permitted range.
–0.3
4.0
V
VIN
Input voltage on five-volt tolerant pin(2)
2.
VIN maximum value must always be respected. Refer to Table 12 for the values of the maximum allowed
injected current.
VSS–0.3
VDD+4
Input voltage on any other pin
VSS–0.3
4.0
|ΔVDDx|
Variations between different VDD power pins
-
mV
|VSSX − VSS|
Variations between all the different ground pins
including VREF−
-
VESD(HBM)
Electrostatic discharge voltage (human body model)
see Section 6.3.14:
Absolute maximum
ratings (electrical
sensitivity)

---

Electrical characteristics
6.3
Operating conditions
6.3.1
General operating conditions
Table 12. Current characteristics
Symbol
Ratings
 Max.
Unit
IVDD
Total current into VDD power lines (source)(1)
1.
All main power (VDD, VDDA) and ground (VSS, VSSA) pins must always be connected to the external power
supply, in the permitted range.
mA
IVSS
Total current out of VSS ground lines (sink)(1)
IIO
Output current sunk by any I/O and control pin
Output current source by any I/Os and control pin
IINJ(PIN)
 (2)
2.
Negative injection disturbs the analog performance of the device. See note in Section 6.3.21: 12-bit ADC
characteristics.
Injected current on five-volt tolerant I/O(3)
3.
Positive injection is not possible on these I/Os. A negative injection is induced by VIN<VSS. IINJ(PIN) must
never be exceeded. Refer to Table 11 for the values of the maximum allowed input voltage.
–5/+0
Injected current on any other pin(4)
4.
A positive injection is induced by VIN>VDD while a negative injection is induced by VIN<VSS. IINJ(PIN) must
never be exceeded. Refer to Table 11 for the values of the maximum allowed input voltage.
±5
ΣIINJ(PIN)
(4)
Total injected current (sum of all I/O and control pins)(5)
5.
When several inputs are submitted to a current injection, the maximum ΣIINJ(PIN) is the absolute sum of the
positive and negative injected currents (instantaneous values).
±25
Table 13. Thermal characteristics
Symbol
Ratings
 Value
Unit
TSTG
Storage temperature range
–65 to +150
°C
TJ
Maximum junction temperature
°C
Table 14. General operating conditions
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit
fHCLK
Internal AHB clock frequency
VOS bit in PWR_CR register = 0(1)
-
MHz
VOS bit in PWR_CR register= 1
-
fPCLK1
Internal APB1 clock frequency
-
-
fPCLK2
Internal APB2 clock frequency
-
-
VDD
Standard operating voltage
-
1.8(2)
-
3.6
V
VDDA
(3)(4)
Analog operating voltage
(ADC limited to 1.2 M samples)
Must be the same potential as
VDD
(5)
1.8(2)
-
2.4
V
Analog operating voltage
(ADC limited to 1.4 M samples)
2.4
-
3.6
VBAT
Backup operating voltage
-
1.65
-
3.6
V

---

Electrical characteristics
V12
Regulator ON:
1.2 V internal voltage on
VCAP_1/VCAP_2 pins
VOS bit in PWR_CR register = 0(1)
Max frequency 144MHz
1.08
1.14
1.20
V
VOS bit in PWR_CR register= 1
Max frequency 168MHz
1.20
1.26
1.32
V
Regulator OFF:
1.2 V external voltage must be
supplied from external regulator
on VCAP_1/VCAP_2 pins
Max frequency 144MHz
1.10
1.14
1.20
V
Max frequency 168MHz
1.20
1.26
1.30
V
VIN
Input voltage on RST and FT
pins(6)
2 V ≤ VDD ≤ 3.6 V
–0.3
-
5.5
V
VDD ≤ 2 V
–0.3
-
5.2
Input voltage on TTa pins
-
–0.3
-
VDDA+
0.3
Input voltage on B pin
-
-
-
5.5
PD
Power dissipation at TA = 85 °C
for suffix 6 or TA = 105 °C for
suffix 7(7)
LQFP64
-
-
mW
LQFP100
-
-
LQFP144
-
-
LQFP176
-
-
UFBGA176
-
-
WLCSP90
-
-
TA
Ambient temperature for 6 suffix
version
Maximum power dissipation
–40
-
°C
Low-power dissipation(8)
–40
-
Ambient temperature for 7 suffix
version
Maximum power dissipation
–40
-
°C
Low-power dissipation(8)
–40
-
TJ
Junction temperature range
6 suffix version
–40
-
°C
7 suffix version
–40
-
1.
The average expected gain in power consumption when VOS = 0 compared to VOS = 1 is around 10% for the whole
temperature range, when the system clock frequency is between 30 and 144 MHz.
2.
VDD/VDDA minimum value of 1.7 V is obtained when the device operates in reduced temperature range, and with the use of
an external power supply supervisor (refer to Section : Internal reset OFF).
3.
When the ADC is used, refer to Table 67: ADC characteristics.
4.
If VREF+ pin is present, it must respect the following condition: VDDA-VREF+ < 1.2 V.
5.
It is recommended to power VDD and VDDA from the same source. A maximum difference of 300 mV between VDD and
VDDA can be tolerated during power-up and power-down operation.
6.
To sustain a voltage higher than VDD+0.3, the internal pull-up and pull-down resistors must be disabled.
7.
If TA is lower, higher PD values are allowed as long as TJ does not exceed TJmax.
8.
In low-power dissipation state, TA can be extended to this range as long as TJ does not exceed TJmax.
Table 14. General operating conditions (continued)
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit

---

Electrical characteristics
          Table 15. Limitations depending on the operating power supply range
Operating
power
supply
range
ADC
operation
Maximum
flash memory
access
frequency
with no wait
state
(fFlashmax)
 Maximum flash
memory access
frequency
with wait
states(1) (2)
I/O operation
Clock output
Frequency on
I/O pins
Possible
flash
memory
operations
VDD =1.8 to
2.1 V(3)
Conversion
time up to
1.2 Msps
20 MHz(4)
160 MHz with 7
wait states
– Degraded
speed
performance
– No I/O
compensation
up to 30 MHz
8-bit erase
and program
operations
only
VDD = 2.1 to
2.4 V
Conversion
time up to
1.2 Msps
22 MHz
168 MHz with 7
wait states
– Degraded
speed
performance
– No I/O
compensation
up to 30 MHz
16-bit erase
and program
operations
VDD = 2.4 to
2.7 V
Conversion
time up to
2.4 Msps
24 MHz
168 MHz with 6
wait states
– Degraded
speed
performance
– I/O
compensation
works
up to 48 MHz
16-bit erase
and program
operations
VDD = 2.7 to
3.6 V(5)
Conversion
time up to
2.4 Msps
30 MHz
168 MHz with 5
wait states
– Full-speed
operation
– I/O
compensation
works
– up to
60 MHz
when VDD =
3.0 to 3.6 V
– up to
48 MHz
when VDD =
2.7 to 3.0 V
32-bit erase
and program
operations
1.
It applies only when code executed from flash memory access, when code executed from RAM, no wait state is required.
2.
Thanks to the ART accelerator and the 128-bit flash memory, the number of wait states given here does not impact the
execution speed from flash memory since the ART accelerator allows to achieve a performance equivalent to 0 wait state
program execution.
3.
VDD/VDDA minimum value of 1.7 V is obtained when the device operates in reduced temperature range, and with the use
of an external power supply supervisor (refer to Section : Internal reset OFF).
4.
Prefetch is not available. Refer to AN3430 application note for details on how to adjust performance and power.
5.
The voltage range for OTG USB FS can drop down to 2.7 V. However it is degraded between 2.7 and 3 V.

---

Electrical characteristics
6.3.2
VCAP_1/VCAP_2 external capacitor
Stabilization for the main regulator is achieved by connecting an external capacitor CEXT to
the VCAP_1/VCAP_2 pins. CEXT is specified in Table 16.
Figure 23. External capacitor CEXT
1.
Legend: ESR is the equivalent series resistance.
6.3.3
Operating conditions at power-up / power-down (regulator ON)
Subject to general operating conditions for TA.
6.3.4
Operating conditions at power-up / power-down (regulator OFF)
Subject to general operating conditions for TA.
Table 16. VCAP_1/VCAP_2 operating conditions(1)
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
< 2 Ω
MS19044V2
ESR
R Leak
C
Table 17. Operating conditions at power-up / power-down (regulator ON)
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
Table 18. Operating conditions at power-up / power-down (regulator OFF)(1)
1.
To reset the internal logic at power-down, a reset must be applied on pin PA0 when VDD reach below
minimum value of V12.
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
VCAP_1 and VCAP_2 rise time
rate
Power-up
∞
VCAP_1 and VCAP_2 fall time
rate
Power-down
∞

---

Electrical characteristics
6.3.5
Embedded reset and power control block characteristics
The parameters given in Table 19 are derived from tests performed under ambient
temperature and VDD supply voltage conditions summarized in Table 14.
Table 19. Embedded reset and power control block characteristics
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
PLS[2:0]=000 (rising
edge)
2.09
2.14
2.19
V
PLS[2:0]=000 (falling
edge)
1.98
2.04
2.08
V
PLS[2:0]=001 (rising
edge)
2.23
2.30
2.37
V
PLS[2:0]=001 (falling
edge)
2.13
2.19
2.25
V
PLS[2:0]=010 (rising
edge)
2.39
2.45
2.51
V
PLS[2:0]=010 (falling
edge)
2.29
2.35
2.39
V
PLS[2:0]=011 (rising edge)
2.54
2.60
2.65
V
PLS[2:0]=011 (falling
edge)
2.44
2.51
2.56
V
PLS[2:0]=100 (rising
edge)
2.70
2.76
2.82
V
PLS[2:0]=100 (falling
edge)
2.59
2.66
2.71
V
PLS[2:0]=101 (rising
edge)
2.86
2.93
2.99
V
PLS[2:0]=101 (falling
edge)
2.75
2.84
2.92
V
PLS[2:0]=110 (rising edge)
2.96
3.03
3.10
V
PLS[2:0]=110 (falling
edge)
2.85
2.93
2.99
V
PLS[2:0]=111 (rising edge)
3.07
3.14
3.21
V
PLS[2:0]=111 (falling
edge)
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

---

Electrical characteristics
6.3.6
Supply current characteristics
The current consumption is a function of several parameters and factors such as the
operating voltage, ambient temperature, I/O pin loading, device software configuration,
operating frequencies, I/O pin switching rate, program location in memory and executed
binary code.
The current consumption is measured as described in Figure 22: Current consumption
measurement scheme.
All Run mode current consumption measurements given in this section are performed using
a CoreMark-compliant code.
Typical and maximum current consumption
The MCU is placed under the following conditions:
•
At startup, all I/O pins are configured as analog inputs by firmware.
•
All peripherals are disabled except if it is explicitly mentioned.
•
The flash memory access time is adjusted to fHCLK frequency (0 wait state from 0 to
30 MHz, 1 wait state from 30 to 60 MHz, 2 wait states from 60 to 90 MHz, 3 wait states
from 90 to 120 MHz, 4 wait states from 120 to 150 MHz, and 5 wait states from 150 to
168 MHz).
•
When the peripherals are enabled HCLK is the system clock, fPCLK1 = fHCLK/4, and
fPCLK2 = fHCLK/2, except is explicitly mentioned.
•
The maximum values are obtained for VDD = 3.6 V and maximum ambient temperature
(TA), and the typical values for TA= 25 °C and VDD = 3.3 V unless otherwise specified.
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
(1)(2) Reset temporization
-
0.5
1.5
3.0
ms
IRUSH
(1)
InRush current on
voltage regulator
power-on (POR or
wakeup from Standby)
-
-
mA
ERUSH
(1)
InRush energy on
voltage regulator
power-on (POR or
wakeup from Standby)
VDD = 1.8 V, TA = 105 °C,
IRUSH = 171 mA for 31 µs
-
-
5.4
µC
1.
Specified by design.
2.
The reset temporization is measured from the power-on (POR reset or wakeup from VBAT) to the instant
when first instruction is read by the user application code.
Table 19. Embedded reset and power control block characteristics (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit

---

Electrical characteristics
Table 20. Typical and maximum current consumption in Run mode, code with data processing
 running from flash memory (ART accelerator enabled) or RAM (1)
Symbol
Parameter
Conditions
fHCLK
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
Supply current in
Run mode
External clock(3), all
peripherals enabled(4)(5)
168 MHz
mA
144 MHz
120 MHz
90 MHz
60 MHz
30 MHz
25 MHz
16 MHz(6)
8 MHz
4 MHz
2 MHz
External clock(3), all
peripherals disabled(4)(5)
168 MHz
144 MHz
120 MHz
90 MHz
60 MHz
30 MHz
25 MHz
16 MHz(6)
8 MHz
4 MHz
2 MHz
1.
Code and data processing running from SRAM1 using boot pins.
2.
Evaluated by characterization, tested in production at VDD max and fHCLK max with peripherals enabled.
3.
External clock is 4 MHz and PLL is on when fHCLK > 25 MHz.
4.
When the ADC is ON (ADON bit set in the ADC_CR2 register), add an additional power consumption of 1.6 mA per ADC for
the analog part.
5.
When analog peripheral blocks such as ADCs, DACs, HSE, LSE, HSI, or LSI are ON, an additional power consumption
should be considered.
6.
In this case HCLK = system clock/2.

---

Electrical characteristics
Table 21. Typical and maximum current consumption in Run mode, code with data processing
 running from flash memory (ART accelerator disabled)
Symbol
Parameter
Conditions
fHCLK
Typ
Max(1)
Unit
TA = 25 °C TA = 85 °C TA = 105 °C
IDD
Supply current
in Run mode
External clock(2),
all peripherals
enabled(3)(4)
168 MHz
mA
144 MHz
120 MHz
90 MHz
60 MHz
30 MHz
25 MHz
16 MHz
8 MHz
4 MHz
2 MHz
External clock(2),
all peripherals
disabled(3)(4)
168 MHz
144 MHz
120 MHz
90 MHz
60 MHz
30 MHz
25 MHz
16 MHz
8 MHz
4 MHz
2 MHz
1.
Evaluated by characterization, tested in production at VDD max and fHCLK max with peripherals enabled.
2.
External clock is 4 MHz and PLL is on when fHCLK > 25 MHz.
3.
When analog peripheral blocks such as (ADCs, DACs, HSE, LSE, HSI,LSI) are on, an additional power consumption
should be considered.
4.
 When the ADC is ON (ADON bit set in the ADC_CR2 register), add an additional power consumption of 1.6 mA per ADC
for the analog part.

---

Electrical characteristics
Figure 24. Typical current consumption versus temperature, Run mode, code with data
processing running from Flash (ART accelerator ON) or RAM, and peripherals OFF
Figure 25. Typical current consumption versus temperature, Run mode, code with data
processing running from Flash (ART accelerator ON) or RAM, and peripherals ON
MS19974V1
IDD RUN (mA)
CPU Frequency (MHz
-45 °C
0 °C
25 °C
55 °C
85 °C
105 °C
MS19975V1
IDD RUN (mA)
CPU Frequency (MHz
-45°C
0°C
25°C
55°C
85°C
105°C

---

Electrical characteristics
Figure 26. Typical current consumption versus temperature, Run mode, code with data
processing running from Flash (ART accelerator OFF) or RAM, and peripherals OFF
Figure 27. Typical current consumption versus temperature, Run mode, code with data
processing running from Flash (ART accelerator OFF) or RAM, and peripherals ON
MS19976V1
IDD RUN (mA)
CPU Frequency (MHz
-45°C
0°C
25°C
55°C
85°C
105°C
MS19977V1
IDD RUN (mA)
CPU Frequency (MHz
-45°C
0°C
25°C
55°C
85°C
105°C

---

Electrical characteristics
          Table 22. Typical and maximum current consumption in Sleep mode
Symbol
Parameter
Conditions
fHCLK
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
Supply current in
Sleep mode
External clock(2),
all peripherals enabled(3)
168 MHz
mA
144 MHz
120 MHz
90 MHz
60 MHz
30 MHz
25 MHz
16 MHz
8 MHz
4 MHz
2 MHz
External clock(2), all
peripherals disabled
168 MHz
144 MHz
120 MHz
90 MHz
60 MHz
30 MHz
25 MHz
16 MHz
8 MHz
4 MHz
2 MHz
1.
Evaluated by characterization, tested in production at VDD max and fHCLK max with peripherals enabled.
2.
External clock is 4 MHz and PLL is on when fHCLK > 25 MHz.
3.
Add an additional power consumption of 1.6 mA per ADC for the analog part. In applications, this consumption occurs only
while the ADC is ON (ADON bit is set in the ADC_CR2 register).

---

Electrical characteristics
Table 23. Typical and maximum current consumptions in Stop mode
Symbol
Parameter
Conditions
Typ
Max
Unit
TA =
25 °C
TA =
25 °C
TA =
85 °C
TA =
105 °C
IDD_STOP
Supply
current in
Stop mode
with main
regulator in
Run mode
Flash in Stop mode, low-speed and high-
speed internal RC oscillators and high-speed
oscillator OFF (no independent watchdog)
0.45
1.5
11.00
20.00
mA
Flash in Deep power-down mode, low-speed
and high-speed internal RC oscillators and
high-speed oscillator OFF (no independent
watchdog)
0.40
1.5
11.00
20.00
Supply
current in
Stop mode
with main
regulator in
Low-power
mode
Flash in Stop mode, low-speed and high-
speed internal RC oscillators and high-speed
oscillator OFF (no independent watchdog)
0.31
1.1
8.00
15.00
Flash in Deep power-down mode, low-speed
and high-speed internal RC oscillators and
high-speed oscillator OFF (no independent
watchdog)
0.28
1.1
8.00
15.00
Table 24. Typical and maximum current consumptions in Standby mode
Symbol
Parameter
Conditions
Typ
Max(1)
Unit
TA = 25 °C
TA =
85 °C
TA =
105 °C
VDD =
1.8 V
VDD=
2.4 V
VDD =
3.3 V
VDD = 3.6 V
IDD_STBY
Supply current
in Standby
mode
Backup SRAM ON, low-
speed oscillator and RTC ON
3.0
3.4
4.0
µA
Backup SRAM OFF, low-
speed oscillator and RTC ON
2.4
2.7
3.3
Backup SRAM ON, RTC
OFF
2.4
2.6
3.0
12.5
24.8
Backup SRAM OFF, RTC
OFF
1.7
1.9
2.2
9.8
19.2
1.
Evaluated by characterization - not tested in production.

---

Electrical characteristics
Figure 28. Typical VBAT current consumption (LSE and RTC ON/backup RAM OFF)
Table 25. Typical and maximum current consumptions in VBAT mode
Symbol
Parameter
Conditions
Typ
Max(1)
Unit
TA = 25 °C
TA =
85 °C
TA =
105 °C
VBAT
=
1.8 V
VBAT=
2.4 V
VBAT
=
3.3 V
VBAT = 3.6 V
IDD_VBA
T
Backup
domain
supply
current
Backup SRAM ON, low-speed
oscillator and RTC ON
1.29
1.42
1.68
µA
Backup SRAM OFF, low-speed
oscillator and RTC ON
0.62
0.73
0.96
Backup SRAM ON, RTC OFF
0.79
0.81
0.86
Backup SRAM OFF, RTC OFF
0.10
0.10
0.10
1.
Evaluated by characterization - not tested in production.
MS19990V1
0.5
1.5
2.5
3.5
IVBAT in (μA)
Temperature in (°C)
1.65V
1.8V
2V
2.4V
2.7V
3V
3.3V
3.6V

---

Electrical characteristics
Figure 29. Typical VBAT current consumption (LSE and RTC ON/backup RAM ON)
Additional current consumption
The MCU is placed under the following conditions:
•
All I/O pins are configured in analog mode.
•
The flash memory access time is adjusted to fHCLK frequency.
•
The voltage scaling is adjusted to fHCLK frequency as follows:
–
Scale 2 for fHCLK ≤ 144 MHz
–
Scale 1 for 144 MHz < fHCLK ≤ 168 MHz.
•
The system clock is HCLK, fPCLK1 = fHCLK/4, and fPCLK2 = fHCLK/2.
•
The HSE crystal clock frequency is 25 MHz.
•
TA= 25 °C.
MS19991V1
IVBAT in (μA)
Temperature in (°C)
1.65V
1.8V
2V
2.4V
2.7V
3V
3.3V
3.6V

---

Electrical characteristics
I/O system current consumption
The current consumption of the I/O system has two components: static and dynamic.
I/O static current consumption
All the I/Os used as input with pull-up or pull-down generate current consumption when the
pin is externally held to the opposite level. The value of this current consumption can be
simply computed by using the pull-up/pull-down resistors values given in Table 48: I/O static
characteristics.
For the output pins, any internal or external pull-up or pull-down and external load must also
be considered to estimate the current consumption.
Additional I/O current consumption is due to I/Os configured as inputs if an intermediate
voltage level is externally applied. This current consumption is caused by the input Schmitt
trigger circuits used to discriminate the input value. Unless this specific configuration is
required by the application, this supply current consumption can be avoided by configuring
these I/Os in analog mode. This is notably the case of ADC input pins which should be
configured as analog inputs.
Caution:
Any floating input pin can also settle to an intermediate voltage level or switch inadvertently,
as a result of external electromagnetic noise. To avoid current consumption related to
floating pins, they must either be configured in analog mode, or forced internally to a definite
digital value. This can be done either by using pull-up/down resistors or by configuring the
pins in output mode.
I/O dynamic current consumption
In addition to the internal peripheral current consumption measured previously (see
Table 28: Peripheral current consumption), the I/Os used by an application also contribute
to the current consumption. When an I/O pin switches, it uses the current from the MCU
supply voltage to supply the I/O pin circuitry and to charge/discharge the capacitive load
internal and external connected to the pin:
Table 26. Typical current consumption in Run mode, code with data processing
 running from flash memory, regulator ON (ART accelerator enabled
except prefetch), VDD = 1.8 V(1)
1.
When peripherals are enabled, the power consumption corresponding to the analog part of the peripherals
(such as ADC or DAC) is not included.
Symbol
Parameter
Conditions
fHCLK (MHz)
Typ. at TA =
25 °C
Unit
IDD
Supply current in
Run mode
All peripheral
disabled
36.2
mA
29.3
24.7
19.3
13.4
7.7
6.0
ISW
VDD
fSW
C
×
×
=

---

Electrical characteristics
where
ISW is the current sunk by a switching I/O to charge/discharge the capacitive load
VDD is the MCU supply voltage
fSW is the I/O switching frequency
C is the total capacitance seen by the I/O pin: C = CINT+ CEXT
The test pin is configured in push-pull output mode and is toggled by software at a fixed
frequency.

---

Electrical characteristics
Table 27. Switching output I/O current consumption
Symbol
Parameter
Conditions(1)
I/O toggling
frequency (fSW)
Typ
Unit
IDDIO
I/O switching
current
VDD = 3.3 V(2)
C = CINT
2 MHz
0.02
mA
8 MHz
0.14
25 MHz
0.51
50 MHz
0.86
60 MHz
1.30
VDD = 3.3 V
CEXT = 0 pF
C = CINT + CEXT+ CS
2 MHz
0.10
8 MHz
0.38
25 MHz
1.18
50 MHz
2.47
60 MHz
2.86
VDD = 3.3 V
CEXT = 10 pF
C = CINT + CEXT+ CS
2 MHz
0.17
8 MHz
0.66
25 MHz
1.70
50 MHz
2.65
60 MHz
3.48
VDD = 3.3 V
CEXT = 22 pF
C = CINT + CEXT+ CS
2 MHz
0.23
8 MHz
0.95
25 MHz
3.20
50 MHz
4.69
60 MHz
8.06
VDD = 3.3 V
CEXT = 33 pF
C = CINT + CEXT+ CS
2 MHz
0.30
8 MHz
1.22
25 MHz
3.90
50 MHz
8.82
60 MHz
-(3)
1.
CS is the PCB board capacitance including the pad pin. CS = 7 pF (estimated value).
2.
This test is performed by cutting the LQFP package pin (pad removal).
3.
At 60 MHz, C maximum load is specified 30 pF.

---

Electrical characteristics
On-chip peripheral current consumption
The current consumption of the on-chip peripherals is given in Table 28. The MCU is placed
under the following conditions:
•
At startup, all I/O pins are configured as analog pins by firmware.
•
All peripherals are disabled unless otherwise mentioned
•
The code is running from flash memory and the flash memory access time is equal to 5
wait states at 168 MHz.
•
The code is running from flash memory and the flash memory access time is equal to 4
wait states at 144 MHz, and the power scale mode is set to 2.
•
The ART accelerator is ON.
•
The given value is calculated by measuring the difference of current consumption
–
with all peripherals clocked off
–
with one peripheral clocked on (with only the clock applied)
•
When the peripherals are enabled: HCLK is the system clock, fPCLK1 = fHCLK/4, and
fPCLK2 = fHCLK/2.
•
The typical values are obtained for VDD = 3.3 V and TA= 25 °C, unless otherwise
specified.
Table 28. Peripheral current consumption
Peripheral
IDD(Typ)(1)
Unit
Scale1
(up t 168 MHz)
Scale2
(up to 144 MHz)
AHB1
(up to 168 MHz)
GPIOA
2.70
2.40
µA/MHz
GPIOB
2.50
2.22
GPIOC
2.54
2.28
GPIOD
2.55
2.28
GPIOE
2.68
2.40
GPIOF
2.53
2.28
GPIOG
2.51
2.22
GPIOH
2.51
2.22
GPIOI
2.50
2.22
OTG_HS+ULPI
28.33
25.38
CRC
0.41
0.40
BKPSRAM
0.63
0.58
DMA1
37.44
33.58
DMA2
37.69
33.93
ETH_MAC
ETH_MAC_TX
ETH_MAC_RX
ETH_MAC_PTP
20.43
18.39

---

Electrical characteristics
AHB2
(up to 168 MHz)
OTG_FS
26.45
26.67
µA/MHz
DCMI
5.87
5.35
RNG
1.50
1.67
AHB3
(up to 168 MHz)
FSMC
12.46
11.31
µA/MHz
Bus matrix(2)
13.10
11.81
µA/MHz
APB1
(up to 42 MHz)
TIM2
16.71
16.50
µA/MHz
TIM3
12.33
11.94
TIM4
13.45
12.92
TIM5
17.14
16.58
TIM6
2.43
3.06
TIM7
2.43
2.22
TIM12
6.62
6.83
TIM13
5.05
5.47
TIM14
5.26
5.61
PWR
1.00
0.56
USART2
2.69
2.78
USART3
2.74
2.78
UART4
3.24
3.33
UART5
2.69
2.78
I2C1
2.67
2.50
I2C2
2.83
2.78
I2C3
2.81
2.78
SPI2
2.43
2.22
SPI3
2.43
2.22
I2S2(3)
2.43
2.22
I2S3(3)
2.26
2.22
CAN1
5.12
5.56
CAN2
4.81
5.28
DAC(4)
1.67
1.67
WWDG
1.00
0.83
Table 28. Peripheral current consumption (continued)
Peripheral
IDD(Typ)(1)
Unit
Scale1
(up t 168 MHz)
Scale2
(up to 144 MHz)

---

Electrical characteristics
6.3.7
Wakeup time from low-power mode
The wakeup times given in Table 29 is measured on a wakeup phase with a 16 MHz HSI
RC oscillator. The clock source used to wake up the device depends from the current
operating mode:
•
Stop or Standby mode: the clock source is the RC oscillator
•
Sleep mode: the clock source is the clock that was set before entering Sleep mode.
All timings are derived from tests performed under ambient temperature and VDD supply
voltage conditions summarized in Table 14.
APB2
(up to 84 MHz)
SDIO
7.08
7.92
µA/MHz
TIM1
16.79
15.51
TIM8
17.88
16.53
TIM9
7.64
7.28
TIM10
4.89
4.82
TIM11
5.19
4.82
ADC1(5)
4.67
4.58
ADC2(5)
4.67
4.58
ADC3(5)
4.43
4.44
SPI1
1.32
1.39
USART1
3.51
3.72
USART6
3.55
3.75
SYSCFG
0.74
0.56
1.
When the I/O compensation cell is ON, IDD typical value increases by 0.22 mA.
2.
The BusMatrix is automatically active when at least one master is ON.
3.
To enable an I2S peripheral, first set the I2SMOD bit and then the I2SE bit in the SPI_I2SCFGR register.
4.
When the DAC is ON and EN1/2 bits are set in DAC_CR register, add an additional power consumption of
0.8 mA per DAC channel for the analog part.
5.
When the ADC is ON (ADON bit set in the ADC_CR2 register), add an additional power consumption of
1.6 mA per ADC for the analog part.
Table 28. Peripheral current consumption (continued)
Peripheral
IDD(Typ)(1)
Unit
Scale1
(up t 168 MHz)
Scale2
(up to 144 MHz)

---

Electrical characteristics
6.3.8
External clock source characteristics
High-speed external user clock generated from an external source
The characteristics given in Table 30 result from tests performed using an high-speed
external clock source, and under ambient temperature and supply voltage conditions
summarized in Table 14.
Table 29. Low-power mode wakeup timings
Symbol
Parameter
Min(1)
Typ(1)
Max(1)
Unit
tWUSLEEP
(2)
Wakeup from Sleep mode
-
-
CPU
clock
cycle
tWUSTOP
(2)
Wakeup from Stop mode (regulator in Run mode and
flash memory in Stop mode)
-
-
µs
Wakeup from Stop mode (regulator in low-power mode
and flash memory in Stop mode)
-
Wakeup from Stop mode (regulator in Run mode and
flash memory in Deep power-down mode)
-
-
Wakeup from Stop mode (regulator in low-power mode
and flash memory in Deep power-down mode)
-
-
tWUSTDBY
(2)(3)
Wakeup from Standby mode
µs
1.
Evaluated by characterization - not tested in production.
2.
The wakeup times are measured from the wakeup event to the point in which the application code reads the first instruction.
3.
tWUSTDBY minimum and maximum values are given at 105 °C and –45 °C, respectively.
Table 30. High-speed external user clock characteristics
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
The characteristics given in Table 31 result from tests performed using an low-speed
external clock source, and under ambient temperature and supply voltage conditions
summarized in Table 14.
Figure 30. High-speed external clock source AC timing diagram
Table 31. Low-speed external user clock characteristics
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
Figure 31. Low-speed external clock source AC timing diagram
High-speed external clock generated from a crystal/ceramic resonator
The high-speed external (HSE) clock can be supplied with a 4 to 26 MHz crystal/ceramic
resonator oscillator. All the information given in this paragraph are based on
characterization results obtained with typical external components specified in Table 32. In
the application, the resonator and the load capacitors have to be placed as close as
possible to the oscillator pins in order to minimize output distortion and startup stabilization
time. Refer to the crystal resonator manufacturer for more details on the resonator
characteristics (frequency, package, accuracy).
For CL1 and CL2, it is recommended to use high-quality external ceramic capacitors in the
5 pF to 25 pF range (typ.), designed for high-frequency applications, and selected to match
the requirements of the crystal or resonator (see Figure 32). CL1 and CL2 are usually the
same size. The crystal manufacturer typically specifies a load capacitance which is the
series combination of CL1 and CL2. PCB and MCU pin capacitance must be included (10 pF
can be used as a rough estimate of the combined pin and board capacitance) when sizing
CL1 and CL2.
Table 32. HSE 4-26 MHz oscillator characteristics (1)
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
kΩ
Gm
Oscillator transconductance
Startup
-
-
mA/V
Gmcritmax
Maximum critical crystal Gm
-
-
tSU(HSE)
(2)
2.
Evaluated by characterization - not tested in production. tSU(HSE) is the startup time measured from the
moment it is enabled (by software) to a stabilized 8 MHz oscillation is reached. This value is measured for
a standard crystal resonator and can vary significantly with the crystal manufacturer
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
Note:
For information on electing the crystal, refer to the application note AN2867 “Oscillator
design guide for ST microcontrollers” available from the ST website www.st.com.
Figure 32. Typical application with an 8 MHz crystal
1.
REXT value depends on the crystal characteristics.
Low-speed external clock generated from a crystal/ceramic resonator
The low-speed external (LSE) clock can be supplied with a 32.768 kHz crystal/ceramic
resonator oscillator. All the information given in this paragraph are based on
characterization results obtained with typical external components specified in Table 33. In
the application, the resonator and the load capacitors have to be placed as close as
possible to the oscillator pins in order to minimize output distortion and startup stabilization
time. Refer to the crystal resonator manufacturer for more details on the resonator
characteristics (frequency, package, accuracy).
Note:
For information on electing the crystal, refer to the application note AN2867 “Oscillator
design guide for ST microcontrollers” available from the ST website www.st.com.
Table 33. LSE oscillator characteristics (fLSE = 32.768 kHz) (1)
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
32.768
-
kHz
RF
Feedback resistor
-
-
18.4
-
MΩ
IDD
LSE current consumption
-
-
-
µA
Gm
Oscillator transconductance
Startup
2.8
-
-
µA/V
Gmcritmax
Maximum critical crystal Gm
-
-
0.56
tSU(LSE)
(2)
2.
Evaluated by characterization - not tested in production. tSU(LSE) is the startup time measured from the
moment it is enabled (by software) to a stabilized 32.768 kHz oscillation is reached. This value is
measured for a standard crystal resonator and it can vary significantly with the crystal manufacturer
Startup time
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
Figure 33. Typical application with a 32.768 kHz crystal
6.3.9
Internal clock source characteristics
The parameters given in Table 34 and Table 35 are derived from tests performed under
ambient temperature and VDD supply voltage conditions summarized in Table 14.
High-speed internal (HSI) RC oscillator
Low-speed internal (LSI) RC oscillator
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
Table 34. HSI oscillator characteristics (1)
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
HSI user trimming step(2)
2.
Specified by design.
-
-
-
%
Accuracy of the HSI oscillator
TA = –40 to 105 °C(3)
3.
Evaluated by characterization - not tested in production.
–8
-
4.5
%
TA = –10 to 85 °C(3)
–4
-
%
TA = 25 °C(4)
4.
Factory calibrated, parts not soldered.
–1
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
Table 35. LSI oscillator characteristics (1)
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
Evaluated by characterization - not tested in production.
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

---

Electrical characteristics
Figure 34. ACCLSI versus temperature
6.3.10
PLL characteristics
The parameters given in Table 36 and Table 37 are derived from tests performed under
temperature and VDD supply voltage conditions summarized in Table 14.
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
Table 36. Main PLL characteristics
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
Take care of using the appropriate division factor M to obtain the specified PLL input clock values. The M factor is shared
between PLL and PLLI2S.
2.
Specified by design.
3.
The use of 2 PLLs in parallel could degraded the Jitter up to +30%.
4.
Evaluated by characterization - not tested in production.
Table 36. Main PLL characteristics (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Table 37. PLLI2S (audio PLL) characteristics
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
6.3.11
PLL spread spectrum clock generation (SSCG) characteristics
The spread spectrum clock generation (SSCG) feature allows to reduce electromagnetic
interferences (see Table 44: EMI characteristics for fHSE = 25 MH and fCPU = 168 MHz). It
is available only on the main PLL.
Equation 1
The frequency modulation period (MODEPER) is given by the equation below:
fPLL_IN and fMod must be expressed in Hz.
As an example:
If fPLL_IN = 1 MHz, and fMOD = 1 kHz, the modulation depth (MODEPER) is given by
equation 1:
Equation 2
Equation 2 allows to calculate the increment step (INCSTEP):
fVCO_OUT must be expressed in MHz.
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
(4) PLLI2S power consumption on
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
Take care of using the appropriate division factor M to have the specified PLL input clock values.
2.
Specified by design.
3.
Value given with main PLL running.
4.
Evaluated by characterization - not tested in production.
Table 37. PLLI2S (audio PLL) characteristics (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Table 38. SSCG parameters constraint
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
215−1
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

---

Electrical characteristics
With a modulation depth (md) = ±2 % (4 % peak to peak), and PLLN = 240 (in MHz):
An amplitude quantization error may be generated because the linear modulation profile is
obtained by taking the quantized values (rounded to the nearest integer) of MODPER and
INCSTEP. As a result, the achieved modulation depth is quantized. The percentage
quantized modulation depth is given by the following formula:
As a result:
Figure 35 and Figure 36 show the main PLL output clock waveforms in center spread and
down spread modes, where:
F0 is fPLL_OUT nominal.
Tmode is the modulation period.
md is the modulation depth.
Figure 35. PLL output clock waveforms in center spread mode
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
Frequency (PLL_OUT)
Time
F0
tmode
2xtmode
md
ai17291
md

---

Electrical characteristics
Figure 36. PLL output clock waveforms in down spread mode
6.3.12
Memory characteristics
Flash memory
The characteristics are given at TA = –40 to 105 °C unless otherwise specified.
The devices are shipped to customers with the flash memory erased.
Frequency (PLL_OUT)
Time
F0
tmode
2xtmode
2xmd
ai17292b
Table 39. Flash memory characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
IDD
Supply current
Write / Erase 8-bit mode, VDD = 1.8 V
-
-
mA
Write / Erase 16-bit mode, VDD = 2.1 V
-
-
Write / Erase 32-bit mode, VDD = 3.3 V
-
-
Table 40. Flash memory programming
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

---

Electrical characteristics
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
1.8
-
3.6
V
1.
Evaluated by characterization - not tested in production.
2.
The maximum programming time is measured after 100K erase operations.
Table 40. Flash memory programming (continued)
Symbol
Parameter
 Conditions
Min(1)
Typ
Max(1)
Unit

---

Electrical characteristics
6.3.13
EMC characteristics
Susceptibility tests are performed on a sample basis during device characterization.
Functional EMS (electromagnetic susceptibility)
While a simple application is executed on the device (toggling 2 LEDs through I/O ports).
the device is stressed by two electromagnetic events until a failure occurs. The failure is
indicated by the LEDs:
•
Electrostatic discharge (ESD) (positive and negative) is applied to all device pins until
a functional disturbance occurs. This test is compliant with the IEC 61000-4-2 standard.
•
FTB: A burst of fast transient voltage (positive and negative) is applied to VDD and VSS
through a 100 pF capacitor, until a functional disturbance occurs. This test is compliant
with the IEC 61000-4-4 standard.
Table 41. Flash memory programming with VPP
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
The maximum programming time is measured after 100K erase operations.
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
Table 42. Flash memory endurance and data retention
Symbol
Parameter
 Conditions
Value
Unit
Min(1)
1.
Evaluated by characterization - not tested in production.
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

---

Electrical characteristics
A device reset allows normal operations to be resumed.
The test results are given in Table 43. They are based on the EMS levels and classes
defined in application note AN1709.
Designing hardened software to avoid noise problems
EMC characterization and optimization are performed at component level with a typical
application environment and simplified MCU software. It should be noted that good EMC
performance is highly dependent on the user application and the software in particular.
Therefore it is recommended that the user applies EMC software optimization and
prequalification tests in relation with the EMC level requested for his application.
Software recommendations
The software flowchart must include the management of runaway conditions such as:
•
Corrupted program counter
•
Unexpected reset
•
Critical Data corruption (control registers...)
Prequalification trials
Most of the common failures (unexpected reset and program counter corruption) can be
reproduced by manually forcing a low state on the NRST pin or the Oscillator pins for 1
second.
To complete these trials, ESD stress can be applied directly on the device, over the range of
specification values. When unexpected behavior is detected, the software can be hardened
to prevent unrecoverable errors occurring (see application note AN1015).
Table 43. EMS characteristics
Symbol
Parameter
Conditions
Level/
Class
VFESD
Voltage limits to be applied on any I/O pin to
induce a functional disturbance
VDD = 3.3 V, LQFP176, TA = +25 °C,
fHCLK = 168 MHz, conforms to
IEC 61000-4-2
2B
VEFTB
Fast transient voltage burst limits to be
applied through 100 pF on VDD and VSS
pins to induce a functional disturbance
VDD = 3.3 V, LQFP176, TA = +25 °C,
fHCLK = 168 MHz, conforms to
IEC 61000-4-2
4A

---

Electrical characteristics
Electromagnetic Interference (EMI)
The electromagnetic field emitted by the device are monitored while a simple application,
executing EEMBC code, is running. This emission test is compliant with SAE IEC61967-2
standard which specifies the test board and the pin loading.
6.3.14
Absolute maximum ratings (electrical sensitivity)
Stresses above the absolute maximum ratings listed in Table 11: Voltage characteristics,
Table 12: Current characteristics, and Table 13: Thermal characteristics may cause
permanent damage to the device. These are stress ratings only and the functional operation
of the device at these conditions is not implied. Exposure to maximum rating conditions for
extended periods may affect device reliability. Device mission profile (application conditions)
is compliant with JEDEC JESD47 Qualification Standard, extended mission profiles are
available on demand.
Electrostatic discharge (ESD)
Electrostatic discharges (a positive then a negative pulse separated by 1 second) are
applied to the pins of each sample according to each pin combination. The sample size
depends on the number of supply pins in the device (3 parts × (n+1) supply pins). This test
conforms to the JESD22-A114/C101 standard.
Table 44. EMI characteristics for fHSE = 25 MH and fCPU = 168 MHz
Symbol
Parameter
Conditions
Monitored
frequency band
Value
Unit
SEMI
Peak(1)
VDD = 3.3 V, TA = 25 °C, LQFP176
package, conforming to SAE J1752/3
EEMBC, code running from flash
memory with ART accelerator enabled
0.1 to 30 MHz
dBµV
30 to 130 MHz
130 MHz to 1GHz
Level(2)
0.1 MHz to 2 GHz
-
Peak(1)
VDD = 3.3 V, TA = 25 °C, LQFP176
package, conforming to SAE J1752/3
EEMBC, code running from flash
memory with ART accelerator and PLL
spread spectrum enabled
0.1 to 30 MHz
dBµV
30 to 130 MHz
130 MHz to 1GHz
Level(2)
0.1 MHz to 2 GHz
3.5
-
1.
Refer to AN1709 "EMI radiated test" chapter.
2.
Refer to AN1709 "EMI level classification" chapter.
Table 45. ESD absolute maximum ratings
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
TA = +25 °C conforming to JESD22-A114
2000(2)
V
VESD(CDM)
Electrostatic discharge
voltage (charge device
model)
TA = +25 °C conforming to
ANSI/ESDA/JEDEC JS-002
II
1.
Evaluated by characterization - not tested in production.

---

Electrical characteristics
Static latchup
Two complementary static tests are required on six parts to assess the latchup
performance:
•
A supply overvoltage is applied to each power supply pin
•
A current injection is applied to each input, output and configurable I/O pin
These tests are compliant with EIA/JESD 78A IC latchup standard.
6.3.15
I/O current injection characteristics
As a general rule, current injection to the I/O pins, due to external voltage below VSS or
above VDD (for standard, 3 V-capable I/O pins) should be avoided during normal product
operation. However, in order to give an indication of the robustness of the microcontroller in
cases when abnormal injection accidentally happens, susceptibility tests are performed on a
sample basis during device characterization.
Functional susceptibilty to I/O current injection
While a simple application is executed on the device, the device is stressed by injecting
current into the I/O pins programmed in floating input mode. While current is injected into
the I/O pin, one at a time, the device is checked for functional failures.
The failure is indicated by an out of range parameter: ADC error above a certain limit (>5
LSB TUE), out of conventional limits of induced leakage current on adjacent pins (out of
5 μA/+0 μA range), or other functional failure (for example reset, oscillator frequency
deviation).
Negative induced leakage current is caused by negative injection and positive induced
leakage current by positive injection.
The test results are given in Table 47.
2.
On VBAT pin, VESD(HBM) is limited to 1000 V.
Table 46. Electrical sensitivities
Symbol
Parameter
Conditions
Class
LU
Static latch-up class
TA = +105 °C conforming to JESD78A
II level A

---

Electrical characteristics
6.3.16
I/O port characteristics
General input/output characteristics
Unless otherwise specified, the parameters given in Table 48 are derived from tests
performed under the conditions summarized in Table 14. All I/Os are CMOS and TTL
compliant.
Note:
For information on GPIO configuration, refer to application note AN4899 “STM32 GPIO
configuration for hardware settings and low-power consumption” available from the ST
website www.st.com.
Table 47. I/O current injection susceptibility
Symbol
Description
Functional susceptibility
Unit
Negative
injection
Positive
injection
IINJ
(1)
Injected current on BOOT0 pin
−0
NA
mA
Injected current on NRST pin
−0
NA
Injected current on PE2, PE3, PE4, PE5, PE6,
PI8, PC13, PC14, PC15, PI9, PI10, PI11, PF0,
PF1, PF2, PF3, PF4, PF5, PF10, PH0/OSC_IN,
PH1/OSC_OUT, PC0, PC1, PC2, PC3, PB6,
PB7, PB8, PB9, PE0, PE1, PI4, PI5, PI6, PI7,
PDR_ON, BYPASS_REG
−0
NA
Injected current on all FT pins
−5
NA
Injected current on any other pin
−5
+5
1.
It is recommended to add a Schottky diode (pin to ground) to analog pins which may potentially inject negative currents.
Table 48. I/O static characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
VIL
FT, TTa and NRST I/O input low
level voltage
1.7 V ≤ VDD ≤ 3.6 V
-
-
0.3VDD-0.04(1)
V
-
-
0.3VDD
(2)
BOOT0 I/O input low level
voltage
1.75 V ≤ VDD ≤ 3.6 V
-40 °C≤ TA ≤ 105 °C
-
-
0.1VDD-+0.1(1)
1.7 V ≤ VDD ≤ 3.6 V
0 °C≤ TA ≤ 105 °C
-
-
VIH
FT, TTa and NRST I/O input low
level voltage
1.7 V ≤ VDD ≤ 3.6 V
0.45VDD+0.3(1)
-
-
0.7VDD
(2)
-
-
BOOT0 I/O input low level
voltage
1.75 V ≤ VDD ≤ 3.6 V
-40 °C≤ TA ≤ 105 °C
0.17VDD+0.7(1)
-
-
1.7 V ≤ VDD ≤ 3.6 V
0 °C≤ TA ≤ 105 °C
-
-

---

Electrical characteristics
All I/Os are CMOS and TTL compliant (no software configuration required). Their
characteristics cover more than the strict CMOS-technology or TTL parameters.
VHYS
FT, TTa and NRST I/O input
hysteresis
1.7 V ≤ VDD ≤ 3.6 V
10%VDD
(3)
-
-
V
BOOT0 I/O input hysteresis
1.75 V ≤ VDD ≤ 3.6 V
-40 °C≤ TA ≤ 105 °C
0.1
-
-
1.7 V ≤ VDD ≤ 3.6 V
0 °C≤ TA ≤ 105 °C
Ilkg
I/O input leakage current (4)
VSS ≤ VIN ≤ VDD
-
-
±1
µA
I/O FT input leakage current (5)
VIN = 5 V
-
-
RPU
Weak pull-up
equivalent
resistor(6)
All pins
except for
PA10 and
PB12
(OTG_FS_ID,
OTG_HS_ID)
VIN = VSS
kΩ
PA10 and
PB12
(OTG_FS_ID,
OTG_HS_ID)
-
RPD
Weak pull-down
equivalent
resistor(7)
All pins
except for
PA10 and
PB12
VIN = VDD
PA10 and
PB12
-
CIO
(8)
I/O pin
capacitance
-
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
Leakage could be higher than the maximum value, if negative current is injected on adjacent pins.Refer to Table 47: I/O
current injection susceptibility
5.
To sustain a voltage higher than VDD + 0.3 V, the internal pull-up/pull-down resistors must be disabled. Leakage could be
higher than the maximum value, if negative current is injected on adjacent pins. Refer to Table 47: I/O current injection
susceptibility.
6.
Pull-up and pull-down resistors are designed with a true resistance in series with a switchable PMOS. This PMOS
contribution to the series resistance is minimum (~10% order).
7.
Pull-up and pull-down resistors are designed with a true resistance in series with a switchable NMOS. This NMOS
contribution to the series resistance is minimum (~10% order).
8.
Hysteresis voltage between Schmitt trigger switching levels. Evaluated by characterization - not tested in production.
Table 48. I/O static characteristics (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit

---

Electrical characteristics
Output driving current
The GPIOs (general purpose input/outputs) can sink or source up to ±8 mA, and sink or
source up to ±20 mA (with a relaxed VOL/VOH) except PC13, PC14 and PC15 which can
sink or source up to ±3mA. When using the PC13 to PC15 GPIOs in output mode, the speed
should not exceed 2 MHz with a maximum load of 30 pF.
In the user application, the number of I/O pins which can drive current must be limited to
respect the absolute maximum rating specified in Section 6.2. In particular:
•
The sum of the currents sourced by all the I/Os on VDD, plus the maximum Run
consumption of the MCU sourced on VDD, cannot exceed the absolute maximum rating
IVDD (see Table 12).
•
The sum of the currents sunk by all the I/Os on VSS plus the maximum Run
consumption of the MCU sunk on VSS cannot exceed the absolute maximum rating
IVSS (see Table 12).
Output voltage levels
Unless otherwise specified, the parameters given in Table 49 are derived from tests
performed under ambient temperature and VDD supply voltage conditions summarized in
Table 14. All I/Os are CMOS and TTL compliant.
Table 49. Output voltage characteristics(1)
1.
PC13, PC14, PC15 and PI8 are supplied through the power switch. Since the switch only sinks a limited
amount of current (3 mA), the use of GPIOs PC13 to PC15 and PI8 in output mode is limited: the speed
should not exceed 2 MHz with a maximum load of 30 pF and these I/Os must not be used as a current
source (e.g. to drive an LED).
Symbol
Parameter
Conditions
Min
Max
Unit
VOL
(2)
2.
The IIO current sunk by the device must always respect the absolute maximum rating specified in Table 12
and the sum of IIO (I/O ports and control pins) must not exceed IVSS.
Output low level voltage
CMOS port
IIO = +8 mA
2.7 V < VDD < 3.6 V
-
0.4
V
VOH
(3)
3.
The IIO current sourced by the device must always respect the absolute maximum rating specified in
Table 12 and the sum of IIO (I/O ports and control pins) must not exceed IVDD.
Output high level voltage
VDD–0.4
-
VOL
(2)
Output low level voltage
TTL port
IIO =+ 8mA
2.7 V < VDD < 3.6 V
-
0.4
V
VOH
(3)
Output high level voltage
2.4
-
VOL
(2)(4)
4.
Evaluated by characterization - not tested in production.
Output low level voltage
IIO = +20 mA
2.7 V < VDD < 3.6 V
-
1.3
V
VOH
(3)(4) Output high level voltage
VDD–1.3
-
VOL
(2)(4)
Output low level voltage
IIO = +6 mA
2 V < VDD < 2.7 V
-
0.4
V
VOH
(3)(4) Output high level voltage
VDD–0.4
-

---

Electrical characteristics
Input/output AC characteristics
The definition and values of input/output AC characteristics are given in Figure 37 and
Table 50, respectively.
Unless otherwise specified, the parameters given in Table 50 are derived from tests
performed under the ambient temperature and VDD supply voltage conditions summarized
in Table 14.
Table 50. I/O AC characteristics(1)(2)
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
fmax(IO)out Maximum frequency(3)
CL = 50 pF, VDD > 2.70 V
-
-
MHz
CL = 50 pF, VDD > 1.8 V
-
-
CL = 10 pF, VDD > 2.70 V
-
-
CL = 10 pF, VDD > 1.8 V
-
-
tf(IO)out/
tr(IO)out
Output high to low level fall
time and output low to high
level rise time
CL = 50 pF, VDD = 1.8 V to
3.6 V
-
-
ns
fmax(IO)out Maximum frequency(3)
CL = 50 pF, VDD > 2.70 V
-
-
MHz
CL = 50 pF, VDD > 1.8 V
-
-
12.5
CL = 10 pF, VDD > 2.70 V
-
-
50(4)
CL = 10 pF, VDD > 1.8 V
-
-
tf(IO)out/
tr(IO)out
Output high to low level fall
time and output low to high
level rise time
CL = 50 pF, VDD >2.7 V
-
-
ns
CL = 50 pF, VDD > 1.8 V
-
-
CL = 10 pF, VDD > 2.70 V
-
-
CL = 10 pF, VDD > 1.8 V
-
-
fmax(IO)out Maximum frequency(3)
CL = 40 pF, VDD > 2.70 V
-
-
50(4)
MHz
CL = 40 pF, VDD > 1.8 V
-
-
CL = 10 pF, VDD > 2.70 V
-
-
100(4)
CL = 10 pF, VDD > 1.8 V
-
-
50(4)
tf(IO)out/
tr(IO)out
Output high to low level fall
time and output low to high
level rise time
CL = 40 pF, VDD > 2.70 V
-
-
ns
CL = 40 pF, VDD > 1.8 V
-
-
CL = 10 pF, VDD > 2.70 V
-
-
CL = 10 pF, VDD > 1.8 V
-
-

---

Electrical characteristics
Figure 37. I/O AC characteristics definition
Fmax(IO)out Maximum frequency(3)
CL = 30 pF, VDD > 2.70 V
-
-
100(4)
MHz
CL = 30 pF, VDD > 1.8 V
-
-
50(4)
CL = 10 pF, VDD > 2.70 V
-
-
180(4)
CL = 10 pF, VDD > 1.8 V
-
-
100(4)
tf(IO)out/
tr(IO)out
Output high to low level fall
time and output low to high
level rise time
CL = 30 pF, VDD > 2.70 V
-
-
ns
CL = 30 pF, VDD > 1.8 V
-
-
CL = 10 pF, VDD > 2.70 V
-
-
2.5
CL = 10 pF, VDD > 1.8 V
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
Evaluated by characterization - not tested in production.
2.
The I/O speed is configured using the OSPEEDRy[1:0] bits. Refer to the STM32F4xx reference manual for a description of
the GPIOx_SPEEDR GPIO port output speed register.
3.
The maximum frequency is defined in Figure 37.
4.
For maximum frequencies above 50 MHz, the compensation cell should be used.
Table 50. I/O AC characteristics(1)(2) (continued)
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
MSv75936V1
Maximum frequency is achieved with a duty cycle at
45-55% when loaded by the specified capacitance
10%
90%
50%
tr(IO)out
OUTPUT
EXTERNAL
ON CL
10%
50%
90%
T
tf(IO)out

---

Electrical characteristics
6.3.17
NRST pin characteristics
The NRST pin input driver uses CMOS technology. It is connected to a permanent pull-up
resistor, RPU (see Table 48).
Unless otherwise specified, the parameters given in Table 51 are derived from tests
performed under the ambient temperature and VDD supply voltage conditions summarized
in Table 14.
Figure 38. Recommended NRST pin protection
1.
The reset network protects the device against parasitic resets.
2.
The user must ensure that the level on the NRST pin can go below the VIL(NRST) max level specified in
Table 51. Otherwise the reset is not taken into account by the device.
6.3.18
TIM timer characteristics
The parameters given in Table 52 and Table 53 are specified by design.
Refer to Section 6.3.16: I/O port characteristics for details on the input/output alternate
function characteristics (output compare, input capture, external clock, PWM output).
Table 51. NRST pin characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
VIL(NRST)
(1)
1.
Specified by design.
NRST Input low level voltage
TTL ports
2.7 V ≤ VDD ≤ 3.6 V
-
-
0.8
V
VIH(NRST)
(1)
NRST Input high level voltage
-
-
VIL(NRST)
(1)
NRST Input low level voltage
CMOS ports
1.8 V ≤ VDD ≤ 3.6 V
-
-
0.3VDD
VIH(NRST)
(1)
NRST Input high level voltage
0.7VDD
-
-
Vhys(NRST)
NRST Schmitt trigger voltage
hysteresis
-
-
-
mV
RPU
Weak pull-up equivalent resistor(2)
2.
The pull-up is designed with a true resistance in series with a switchable PMOS. This PMOS contribution to
the series resistance must be minimum (~10% order).
VIN = VSS
kΩ
VF(NRST)
(1)
NRST Input filtered pulse
-
-
-
ns
VNF(NRST)
(1) NRST Input not filtered pulse
VDD > 2.7 V
-
-
ns
TNRST_OUT
Generated reset pulse duration
Internal reset
source
-
-
µs
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
Table 52. Characteristics of TIMx connected to the APB1 domain(1)
1.
TIMx is used as a general term to refer to the TIM2, TIM3, TIM4, TIM5, TIM6, TIM7, and TIM12 timers.
Symbol
Parameter
Conditions
Min
Max
Unit
tres(TIM)
Timer resolution time
AHB/APB1
prescaler distinct
from 1, fTIMxCLK =
84 MHz
-
tTIMxCLK
11.9
-
ns
AHB/APB1
prescaler = 1,
fTIMxCLK = 42 MHz
-
tTIMxCLK
23.8
-
ns
fEXT
Timer external clock
frequency on CH1 to CH4
 fTIMxCLK = 84 MHz
APB1= 42 MHz
fTIMxCLK/2
MHz
MHz
ResTIM
Timer resolution
-
bit
tCOUNTER
16-bit counter clock
period when internal clock
is selected
65536
tTIMxCLK
0.0119
µs
32-bit counter clock
period when internal clock
is selected
-
tTIMxCLK
0.0119
51130563
µs
tMAX_COUNT
Maximum possible count
-
65536 × 65536
tTIMxCLK
-
51.1
s

---

Electrical characteristics
6.3.19
Communications interfaces
I2C interface characteristics
The I2C interface meets the timings requirements of the I2C-bus specification and user
manual rev. 03 for:
•
Standard-mode (Sm): with a bit rate up to 100 kbit/s
•
Fast-mode (Fm): with a bit rate up to 400 kbit/s.
The I2C timings requirements are specified by design when the I2C peripheral is properly
configured (refer to RM0090 reference manual).
The SDA and SCL I/O requirements are met with the following restrictions: the SDA and
SCL I/O pins are not “true” open-drain. When configured as open-drain, the PMOS
connected between the I/O pin and VDD is disabled, but is still present. Refer to
Section 6.3.16: I/O port characteristics for more details on the I2C I/O characteristics.
All I2C SDA and SCL I/Os embed an analog filter. Refer to the table below for the analog
filter characteristics:
Table 53. Characteristics of TIMx connected to the APB2 domain(1)
1.
TIMx is used as a general term to refer to the TIM1, TIM8, TIM9, TIM10, and TIM11 timers.
Symbol
Parameter
Conditions
Min
Max
Unit
tres(TIM)
Timer resolution time
AHB/APB2
prescaler distinct
from 1, fTIMxCLK =
168 MHz
-
tTIMxCLK
5.95
-
ns
AHB/APB2
prescaler = 1,
fTIMxCLK = 84 MHz
-
tTIMxCLK
11.9
-
ns
fEXT
Timer external clock
frequency on CH1 to
CH4
 fTIMxCLK =
168 MHz
APB2 = 84 MHz
fTIMxCLK/2
MHz
MHz
ResTIM
Timer resolution
-
bit
tCOUNTER
16-bit counter clock
period when internal
clock is selected
65536
tTIMxCLK
tMAX_COUNT
Maximum possible count
-
32768
tTIMxCLK
Table 54. I2C analog filter characteristics(1)
1.
Specified by design.
Symbol
Parameter
Min
Max
Unit
tAF
Maximum pulse width of spikes
that are suppressed by the analog
filter
50(2)
2.
Spikes with widths below tAF(min) are filtered.
260(3)
3.
Spikes with widths above tAF(max) are not filtered
ns

---

Electrical characteristics
SPI interface characteristics
Unless otherwise specified, the parameters given in Table 55 for SPI are derived from tests
performed under the ambient temperature, fPCLKx frequency and VDD supply voltage
conditions summarized in Table 14 with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load C = 30 pF
•
Measurement points are done at CMOS levels: 0.5 VDD
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output alternate
function characteristics (NSS, SCK, MOSI, MISO).
Table 55. SPI dynamic characteristics(1)
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit
fSCK
SPI clock frequency
Master mode, SPI1,
2.7V < VDD < 3.6V
-
-
MHz
Slave mode, SPI1,
2.7V < VDD < 3.6V
1/tc(SCK)
Master mode, SPI1/2/3,
1.7V < VDD < 3.6V
-
-
Slave mode, SPI1/2/3,
1.7V < VDD < 3.6V
Duty(SCK) Duty cycle of SPI clock
frequency
Slave mode
%

---

Electrical characteristics
tw(SCKH)
SCK high and low time
Master mode, SPI presc = 2,
2.7V < VDD < 3.6V
TPCLK-0.5
TPCLK
TPCLK+0.5
ns
tw(SCKL)
Master mode, SPI presc = 2,
1.7V < VDD < 3.6V
TPCLK-2
TPCLK
TPCLK+2
tsu(NSS)
NSS setup time
Slave mode, SPI presc = 2
4 x TPCLK
-
-
th(NSS)
NSS hold time
Slave mode, SPI presc = 2
2 x TPCLK
tsu(MI)
Data input setup time
Master mode
6.5
-
-
tsu(SI)
Slave mode
2.5
-
-
th(MI)
Data input hold time
Master mode
2.5
-
-
th(SI)
Slave mode
-
-
ta(SO)
(2)
Data output access time
Slave mode, SPI presc = 2
-
4 x TPCLK
tdis(SO)
(3)
Data output disable time
Slave mode, SPI1,
2.7V < VDD < 3.6V
-
7.5
Slave mode, SPI1/2/3
1.7V < VDD < 3.6V
-
16.5
tv(SO)
th(SO)
Data output valid/hold time
Slave mode (after enable edge),
SPI1, 2.7V < VDD < 3.6V
-
Slave mode (after enable edge),
SPI2/3, 2.7V < VDD < 3.6V
-
16.5
Slave mode (after enable edge),
SPI1, 1.7V < VDD < 3.6V
-
15.5
Slave mode (after enable edge),
SPI2/3, 1.7V < VDD < 3.6V
-
20.5
tv(MO)
Data output valid time
Master mode (after enable edge),
SPI1, 2.7V < VDD < 3.6V
-
-
2.5
Master mode (after enable edge),
SPI1/2/3, 1.7V < VDD < 3.6V
-
-
4.5
th(MO)
Data output hold time
Master mode (after enable edge)
-
-
1.
Evaluated by characterization - not tested in production.
2.
Min time is for the minimum time to drive the output and the max time is for the maximum time to validate the data.
3.
Min time is for the minimum time to invalidate the output and the max time is for the maximum time to put the data in Hi-Z.
Table 55. SPI dynamic characteristics(1) (continued)
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit

---

Electrical characteristics
Figure 39. SPI timing diagram - slave mode and CPHA = 0
Figure 40. SPI timing diagram - slave mode and CPHA = 1
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
Figure 41. SPI timing diagram - master mode
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

---

Electrical characteristics
I2S interface characteristics
Unless otherwise specified, the parameters given in Table 56 for the i2S interface are
derived from tests performed under the ambient temperature, fPCLKx frequency and VDD
supply voltage conditions summarized in Table 14, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load C = 30 pF
•
Measurement points are done at CMOS levels: 0.5 VDD
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output alternate
function characteristics (CK, SD, WS).
Note:
Refer to the I2S section of RM0090 reference manual for more details on the sampling
frequency (FS). fMCK, fCK, and DCK values reflect only the digital peripheral behavior. The
value of these parameters might be slightly impacted by the source clock accuracy. DCK
depends mainly on the value of ODD bit. The digital contribution leads to a minimum value
of I2SDIV / (2 x I2SDIV + ODD) and a maximum value of (I2SDIV + ODD) / (2 x I2SDIV +
ODD). FS maximum value is supported for each mode/condition.
Table 56. I2S dynamic characteristics(1)
Symbol
Parameter
Conditions
Min
Max
Unit
fMCK
I2S main clock output
-
256 x
8K
256 x FS
(2)
MHz
fCK
I2S clock frequency
Master data: 32 bits
-
64 x FS
MHz
Slave data: 32 bits
-
64 x FS
DCK
I2S clock frequency duty cycle Slave receiver
%
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
Evaluated by characterization - not tested in production.
2.
The maximum value of 256 x FS is 42 MHz (APB1 maximum frequency).

---

Electrical characteristics
Figure 42. I2S slave timing diagram (Philips protocol)
1.
LSB transmit/receive of the previously transmitted byte. No LSB transmit/receive is sent before the first
byte.
Figure 43. I2S master timing diagram (Philips protocol)(1)
1.
Evaluated by characterization - not tested in production.
2.
LSB transmit/receive of the previously transmitted byte. No LSB transmit/receive is sent before the first
byte.

---

Electrical characteristics
USB OTG FS characteristics
This interface is present in both the USB OTG HS and USB OTG FS controllers.
Table 57. USB OTG FS startup time
Symbol
Parameter
 Max
 Unit
tSTARTUP
(1)
1.
Specified by design.
USB OTG FS transceiver startup time
µs
Table 58. USB OTG FS DC electrical characteristics
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
USB OTG FS operating
voltage
-
3.0(2)
2.
The STM32F405xx and STM32F407xx USB OTG FS functionality is ensured down to 2.7 V but not the full
USB OTG FS electrical characteristics which are degraded in the 2.7-to-3.0 V VDD voltage range.
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
-
1.3
-
2.0
Output
levels
VOL
Static output level low
RL of 1.5 kΩ to 3.6 V(4)
4.
RL is the load connected on the USB OTG FS drivers
-
-
0.3
V
VOH
Static output level high
RL of 15 kΩ to VSS
(4)
2.8
-
3.6
RPD
PA11, PA12, PB14, PB15
(USB_FS_DP/DM,
USB_HS_DP/DM)
VIN = VDD
kΩ
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
Figure 44. USB OTG FS timings: definition of data signal rise and fall time
USB HS characteristics
Unless otherwise specified, the parameters given in Table 62 for ULPI are derived from
tests performed under the ambient temperature, fHCLK frequency summarized in Table 61
and VDD supply voltage conditions summarized in Table 60, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load C = 30 pF
•
Measurement points are done at CMOS levels: 0.5VDD.
Refer to Section Section 6.3.16: I/O port characteristics for more details on the input/output
characteristics.
Table 59. USB OTG FS electrical characteristics(1)
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
Measured from 10% to 90% of the data signal. For more detailed informations, please refer to USB
Specification - Chapter 7 (version 2.0).
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
Table 60. USB HS DC electrical characteristics
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
2.7
3.6
V
Table 61. USB HS clock timing parameters(1)
Parameter
Symbol
Min
Nominal
Max
Unit
fHCLK value to guarantee proper operation of
USB HS interface
-
-
-
MHz
Frequency (first transition)
8-bit ±10%
FSTART_8BIT
MHz
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
Figure 45. ULPI timing diagram
Frequency (steady state) ±500 ppm
FSTEADY
59.97
60.03
MHz
Duty cycle (first transition)
8-bit ±10%
DSTART_8BIT
%
Duty cycle (steady state) ±500 ppm
DSTEADY
49.975
50.025
%
Time to reach the steady state frequency and
duty cycle after the first transition
TSTEADY
-
-
1.4
ms
Clock startup time after the
de-assertion of SuspendM
Peripheral
TSTART_DEV
-
-
5.6
ms
Host
TSTART_HOST
-
-
-
PHY preparation time after the first transition
of the input clock
TPREP
-
-
-
µs
1.
Specified by design.
Table 62. ULPI timing
Parameter
Symbol
Value(1)
1.
VDD = 2.7 V to 3.6 V and TA = –40 to 85 °C.
Unit
Min.
Max.
Control in (ULPI_DIR) setup time
tSC
-
2.0
ns
Control in (ULPI_NXT) setup time
-
1.5
Control in (ULPI_DIR, ULPI_NXT) hold time
 tHC
-
Data in setup time
tSD
-
2.0
Data in hold time
tHD
-
Control out (ULPI_STP) setup time and hold time
tDC
-
9.2
Data out available from clock rising edge
tDD
-
10.7
Table 61. USB HS clock timing parameters(1)
Parameter
Symbol
Min
Nominal
Max
Unit
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
Ethernet characteristics
Unless otherwise specified, the parameters given in Table 64, Table 65 and Table 66 for
SMI, RMII and MII are derived from tests performed under the ambient temperature, fHCLK
frequency summarized in Table 14 and VDD supply voltage conditions summarized in
Table 63, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load C = 30 pF
•
Measurement points are done at CMOS levels: 0.5VDD.
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output
characteristics.
Table 64 gives the list of Ethernet MAC signals for the SMI (station management interface)
and Figure 46 shows the corresponding timing diagram.
Figure 46. Ethernet SMI timing diagram
Table 65 gives the list of Ethernet MAC signals for the RMII and Figure 47 shows the
corresponding timing diagram.
Table 63. Ethernet DC electrical characteristics
Symbol
Parameter
Min.(1)
1.
All the voltages are measured from the local ground potential.
Max.(1)
Unit
Input level
VDD
Ethernet operating voltage
2.7
3.6
V
Table 64. Dynamic characteristics: Ethernet MAC signals for SMI(1)
1.
Evaluated by characterization - not tested in production.
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
Figure 47. Ethernet RMII timing diagram
Table 66 gives the list of Ethernet MAC signals for MII and Figure 47 shows the
corresponding timing diagram.
Figure 48. Ethernet MII timing diagram
Table 65. Dynamic characteristics: Ethernet MAC signals for RMII
Symbol
Rating
Min
Typ
Max
Unit
tsu(RXD)
Receive data setup time
-
-
ns
tih(RXD)
Receive data hold time
-
-
ns
tsu(CRS)
Carrier sense set-up time
0.5
-
-
ns
tih(CRS)
Carrier sense hold time
-
-
ns
td(TXEN)
Transmit enable valid delay time
9.5
ns
td(TXD)
Transmit data valid delay time
8.5
11.5
ns
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
ai15667

---

Electrical characteristics
6.3.20
CAN (controller area network) interface
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output alternate
function characteristics (CANTX and CANRX).
6.3.21
12-bit ADC characteristics
Unless otherwise specified, the parameters given in Table 67 are derived from tests
performed under the ambient temperature, fPCLK2 frequency and VDDA supply voltage
conditions summarized in Table 14.
Table 66. Dynamic characteristics: Ethernet MAC signals for MII(1)
1.
Evaluated by characterization - not tested in production.
Symbol
Parameter
Min
Typ
Max
Unit
tsu(RXD)
Receive data setup time
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
td(TXD)
Transmit data valid delay time
Table 67. ADC characteristics
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit
VDDA
Power supply
-
1.8(1)
-
3.6
V
VREF+
Positive reference voltage
-
1.8(1)(2)(3)
-
VDDA
VREF−
Negative reference voltage
-
-
-
fADC
ADC clock frequency
VDDA = 1.8(1)(3) to
2.4 V
0.6
MHz
VDDA = 2.4 to 3.6 V(3)
0.6
MHz
fTRIG
(4)
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
Conversion voltage range(5)
-
0 (VSSA or VREF-
tied to ground)
-
VREF+
V
RAIN
(4)
External input impedance
See Equation 1 for
details
-
-
κΩ
RADC
(4)(6) Sampling switch resistance
-
-
-
κΩ
CADC
(4)
Internal sample and hold
capacitor
 -
-
-
pF

---

Electrical characteristics
tlat
(4)
Injection trigger conversion
latency
fADC = 30 MHz
-
-
0.100
µs
-
-
3(7)
1/fADC
tlatr
(4)
Regular trigger conversion
latency
fADC = 30 MHz
-
-
0.067
µs
-
-
2(7)
1/fADC
tS
(4)
Sampling time
fADC = 30 MHz
0.100
-
µs
-
-
1/fADC
tSTAB
(4)
Power-up time
-
-
µs
tCONV
(4)
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
fS
(4)
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
(4)
ADC VREF DC current
consumption in conversion
mode
-
-
µA
IVDDA
(4)
ADC VDDA DC current
consumption in conversion
mode
-
-
1.6
1.8
mA
1.
VDD/VDDA minimum value of 1.7 V is obtained when the device operates in reduced temperature range, and with the use of
an external power supply supervisor (refer to Section : Internal reset OFF).
2.
It is recommended to maintain the voltage difference between VREF+ and VDDA below 1.8 V.
3.
VDDA -VREF+ < 1.2 V.
4.
Evaluated by characterization - not tested in production.
5.
VREF+ is internally connected to VDDA and VREF- is internally connected to VSSA.
6.
RADC maximum value is given for VDD=1.8 V, and minimum value for VDD=3.3 V.
7.
For external triggers, a delay of 1/fPCLK2 must be added to the latency specified in Table 67.
Table 67. ADC characteristics (continued)
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit

---

Electrical characteristics
Equation 1: RAIN max formula
The formula above (Equation 1) is used to determine the maximum external impedance
allowed for an error below 1/4 of LSB. N = 12 (from 12-bit resolution) and k is the number of
sampling periods defined in the ADC_SMPR1 register.
          a
Note:
ADC accuracy vs. negative injection current: injecting a negative current on any analog
input pins should be avoided as this significantly reduces the accuracy of the conversion
being performed on another analog input. It is recommended to add a Schottky diode (pin to
ground) to analog pins which may potentially inject negative currents.
Any positive injection current within the limits specified for IINJ(PIN) and SIINJ(PIN) in
Section 6.3.16 does not affect the ADC accuracy.
Figure 49. ADC accuracy characteristics
Table 68. ADC accuracy at fADC = 30 MHz
Symbol
Parameter
Test conditions
Typ
Max(1)
1.
Evaluated by characterization - not tested in production.
Unit
ET
Total unadjusted error
fPCLK2 = 60 MHz,
fADC = 30 MHz, RAIN < 10 kΩ,
VDDA = 1.8(2) to 3.6 V
2.
VDD/VDDA minimum value of 1.7 V is obtained when the device operates in reduced temperature range,
and with the use of an external power supply supervisor (refer to Section : Internal reset OFF).
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
MSv19880V6
(1) Example of an actual transfer curve
(2) Ideal transfer curve
(3) End-point correlation line
n = ADC resolution
ET = total unadjusted error: maximum deviation
between the actual and ideal transfer curves
EO = offset error: maximum deviation between the first
actual transition and the first ideal one
EG = gain error: deviation between the last ideal
transition and the last actual one
ED = differential linearity error: maximum deviation
between actual steps and the ideal one
EL = integral linearity error: maximum deviation between
any actual transition and the end point correlation line
2n-1
(1/2n)*VREF+
Output code
EO
VSSA
2n-2
2n-3
VREF+ (VDDA)
ET
EL
ED
1 LSB ideal
EG
(1)
(2)
(3)
(or                 )]
[1LSB =
VDDA
2n
VREF+
2n
(2/2n)*VREF+
(3/2n)*VREF+
(4/2n)*VREF+
(5/2n)*VREF+
(6/2n)*VREF+
(7/2n)*VREF+
(2n-3/2n)*VREF+
(2n-2/2n)*VREF+
(2n-1/2n)*VREF+
(2n/2n)*VREF+

---

Electrical characteristics
Figure 50. Typical connection diagram when using the ADC with FT/TT pins featuring
analog switch function
1.
Refer to Table 67: ADC characteristics for the values of RAIN, RADC and CADC.
2.
Cparasitic represents the capacitance of the PCB (dependent on soldering and PCB layout quality) plus the
pad capacitance (refer to Table 48: I/O static characteristics) A high Cparasitic value downgrades conversion
accuracy. To remedy this, fADC should be reduced.
3.
Refer to Table 48: I/O static characteristics.
4.
Refer to Figure 21: Power supply scheme.
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
Power supply decoupling should be performed as shown in Figure 51 or Figure 52,
depending on whether VREF+ is connected to VDDA or not. The 10 nF capacitors should be
ceramic (good quality). They should be placed them as close as possible to the chip.
Figure 51. Power supply and reference decoupling (VREF+ not connected to VDDA)
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
Figure 52. Power supply and reference decoupling (VREF+ connected to VDDA)
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
Table 69. Temperature sensor characteristics
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
Evaluated by characterization - not tested in production.
2.
Specified by design.
Table 70. Temperature sensor calibration values
Symbol
Parameter
Memory address
TS_CAL1
TS ADC raw data acquired at temperature of 30 °C, VDDA=3.3 V
0x1FFF 7A2C - 0x1FFF 7A2D
TS_CAL2
TS ADC raw data acquired at temperature of 110 °C, VDDA=3.3 V
0x1FFF 7A2E - 0x1FFF 7A2F

---

Electrical characteristics
6.3.23
VBAT monitoring characteristics
6.3.24
Embedded reference voltage
The parameters given in Table 72 are derived from tests performed under ambient
temperature and VDD supply voltage conditions summarized in Table 14.
6.3.25
DAC electrical characteristics
Table 71. VBAT monitoring characteristics
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
KΩ
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
Shortest sampling time can be determined in the application by multiple iterations.
Table 72. Embedded internal reference voltage
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
VDD = 3 V
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
Shortest sampling time can be determined in the application by multiple iterations.
2.
Specified by design.
Table 73. Internal reference voltage calibration values
Symbol
Parameter
Memory address
VREFIN_CAL
Raw data acquired at temperature of 30 °C, VDDA=3.3 V
0x1FFF 7A2A - 0x1FFF 7A2B
Table 74. DAC characteristics
Symbol
Parameter
Min
Typ
Max
Unit
Comments
VDDA
Analog supply voltage
1.8(1)
-
3.6
V
-
VREF+
Reference supply voltage
1.8(1)
-
3.6
V
VREF+ ≤ VDDA
VSSA
Ground
-
V
-

---

Electrical characteristics
RLOAD
(2)
Resistive load with buffer
ON
-
-
kΩ
-
RO
(2)
Impedance output with
buffer OFF
-
-
kΩ
When the buffer is OFF, the
Minimum resistive load between
DAC_OUT and VSS to have a 1%
accuracy is 1.5 MΩ
CLOAD
(2)
Capacitive load
-
-
pF
Maximum capacitive load at
DAC_OUT pin (when the buffer is
ON).
DAC_OUT
min(2)
Lower DAC_OUT voltage
with buffer ON
0.2
-
-
V
It gives the maximum output
excursion of the DAC.
It corresponds to 12-bit input code
(0x0E0) to (0xF1C) at VREF+ =
3.6 V and (0x1C7) to (0xE38) at
VREF+ = 1.8 V
DAC_OUT
max(2)
Higher DAC_OUT voltage
with buffer ON
-
-
VDDA – 0.2
V
DAC_OUT
min(2)
Lower DAC_OUT voltage
with buffer OFF
-
0.5
-
mV
It gives the maximum output
excursion of the DAC.
DAC_OUT
max(2)
Higher DAC_OUT voltage
with buffer OFF
-
-
VREF+ – 1LSB
V
IVREF+
(4)
DAC DC VREF current
consumption in quiescent
mode (Standby mode)
-
µA
With no load, worst code (0x800)
at VREF+ = 3.6 V in terms of DC
consumption on the inputs
-
With no load, worst code (0xF1C)
at VREF+ = 3.6 V in terms of DC
consumption on the inputs
IDDA
(4)
DAC DC VDDA current
consumption in quiescent
mode(3)
-
µA
With no load, middle code (0x800)
on the inputs
-
µA
With no load, worst code (0xF1C)
at VREF+ = 3.6 V in terms of DC
consumption on the inputs
DNL(4)
Differential non linearity
Difference between two
consecutive code-1LSB)
-
-
±0.5
LSB
Given for the DAC in 10-bit
configuration.
-
-
±2
LSB
Given for the DAC in 12-bit
configuration.
INL(4)
Integral non linearity
(difference between
measured value at Code i
and the value at Code i on a
line drawn between Code 0
and last Code 1023)
-
-
±1
LSB
Given for the DAC in 10-bit
configuration.
-
-
±4
LSB
Given for the DAC in 12-bit
configuration.
Table 74. DAC characteristics (continued)
Symbol
Parameter
Min
Typ
Max
Unit
Comments

---

Electrical characteristics
Offset(4)
Offset error
(difference between
measured value at Code
(0x800) and the ideal value
= VREF+/2)
-
-
±10
mV
Given for the DAC in 12-bit
configuration
-
-
±3
LSB
Given for the DAC in 10-bit at
VREF+ = 3.6 V
-
-
±12
LSB
Given for the DAC in 12-bit at
VREF+ = 3.6 V
Gain
error(4)
Gain error
-
-
±0.5
%
Given for the DAC in 12-bit
configuration
tSETTLING
(4)
Settling time (full scale: for a
10-bit input code transition
between the lowest and the
highest input codes when
DAC_OUT reaches final
value ±4LSB
-
µs
CLOAD ≤  50 pF,
RLOAD ≥ 5 kΩ
THD(4)
Total Harmonic Distortion
Buffer ON
-
-
-
dB
CLOAD ≤  50 pF,
RLOAD ≥ 5 kΩ
Update
rate(2)
Max frequency for a correct
DAC_OUT change when
small variation in the input
code (from code i to i+1LSB)
-
-
MS/s CLOAD ≤  50 pF,
RLOAD ≥ 5 kΩ
tWAKEUP
(4)
Wakeup time from off state
(Setting the ENx bit in the
DAC Control register)
-
6.5
µs
CLOAD ≤  50 pF, RLOAD ≥ 5 kΩ
input code between lowest and
highest possible ones.
PSRR+ (2)
Power supply rejection ratio
(to VDDA) (static DC
measurement)
-
–67
–40
dB
No RLOAD, CLOAD = 50 pF
1.
VDD/VDDA minimum value of 1.7 V is obtained when the device operates in reduced temperature range, and with the use of
an external power supply supervisor (refer to Section : Internal reset OFF).
2.
Specified by design.
3.
The quiescent mode corresponds to a state where the DAC maintains a stable output level to ensure that no dynamic
consumption occurs.
4.
Evaluated by characterization - not tested in production.
Table 74. DAC characteristics (continued)
Symbol
Parameter
Min
Typ
Max
Unit
Comments

---

Electrical characteristics
Figure 53. 12-bit buffered /non-buffered DAC
1.
The DAC integrates an output buffer that can be used to reduce the output impedance and to drive external
loads directly without the use of an external operational amplifier. The buffer can be bypassed by
configuring the BOFFx bit in the DAC_CR register.
6.3.26
FSMC characteristics
Unless otherwise specified, the parameters given in Table 75 to Table 86 for the FSMC
interface are derived from tests performed under the ambient temperature, fHCLK frequency
and VDD supply voltage conditions summarized in Table 14, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load C = 30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
Refer to Section Section 6.3.16: I/O port characteristics for more details on the input/output
characteristics.
Asynchronous waveforms and timings
Figure 54 through Figure 57 represent asynchronous waveforms and Table 75 through
Table 78 provide the corresponding timings. The results shown in these tables are obtained
with the following FSMC configuration:
•
AddressSetupTime = 1
•
AddressHoldTime = 0x1
•
DataSetupTime = 0x1
•
BusTurnAroundDuration = 0x0
In all timing tables, the THCLK is the HCLK clock period.
R L
C L
Buffered/Non-buffered DAC
DAC_OUTx
Buffer(1)
12-bit
digital to
analog
converter
ai17157V3

---

Electrical characteristics
Figure 54. Asynchronous non-multiplexed SRAM/PSRAM/NOR read waveforms
1.
Mode 2/B, C and D only. In Mode 1, FSMC_NADV is not used.
Table 75. Asynchronous non-multiplexed SRAM/PSRAM/NOR read timings(1)(2)
1.
CL = 30 pF.
2.
Evaluated by characterization - not tested in production.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FSMC_NE low time
2THCLK–0.5 2 THCLK+1
ns
tv(NOE_NE)
FSMC_NEx low to FSMC_NOE low
 0.5
ns
tw(NOE)
FSMC_NOE low time
2THCLK–2
2THCLK+ 2
ns
th(NE_NOE)
FSMC_NOE high to FSMC_NE high hold time
-
ns
tv(A_NE)
FSMC_NEx low to FSMC_A valid
-
4.5
ns
th(A_NOE)
Address hold time after FSMC_NOE high
-
ns
tv(BL_NE)
FSMC_NEx low to FSMC_BL valid
-
1.5
ns
th(BL_NOE)
FSMC_BL hold time after FSMC_NOE high
-
ns
tsu(Data_NE)
Data to FSMC_NEx high setup time
THCLK+4
-
ns
tsu(Data_NOE) Data to FSMC_NOEx high setup time
THCLK+4
-
ns
th(Data_NOE)
Data hold time after FSMC_NOE high
-
ns
th(Data_NE)
Data hold time after FSMC_NEx high
-
ns
tv(NADV_NE)
FSMC_NEx low to FSMC_NADV low
-
ns
tw(NADV)
FSMC_NADV low time
-
THCLK
ns
Data
FSMC_NE
FSMC_NBL[1:0]
FSMC_D[15:0]
tv(BL_NE)
t h(Data_NE)
FSMC_NOE
Address
FSMC_A[25:0]
tv(A_NE)
FSMC_NWE
tsu(Data_NE)
tw(NE)
ai14991c
w(NOE)
t
tv(NOE_NE)
t h(NE_NOE)
th(Data_NOE)
t h(A_NOE)
t h(BL_NOE)
tsu(Data_NOE)
FSMC_NADV(1)
t v(NADV_NE)
tw(NADV)

---

Electrical characteristics
Figure 55. Asynchronous non-multiplexed SRAM/PSRAM/NOR write waveforms
1.
Mode 2/B, C and D only. In Mode 1, FSMC_NADV is not used.
Table 76. Asynchronous non-multiplexed SRAM/PSRAM/NOR write timings(1)(2)
1.
CL = 30 pF.
2.
Evaluated by characterization - not tested in production.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FSMC_NE low time
3THCLK
3THCLK+ 4
ns
tv(NWE_NE)
FSMC_NEx low to FSMC_NWE low
THCLK–0.5
THCLK+0.5
ns
tw(NWE)
FSMC_NWE low time
THCLK–1
THCLK+2
ns
th(NE_NWE)
FSMC_NWE high to FSMC_NE high hold time
THCLK–1
 -
ns
tv(A_NE)
FSMC_NEx low to FSMC_A valid
 -
ns
th(A_NWE)
Address hold time after FSMC_NWE high
THCLK– 2
 -
ns
tv(BL_NE)
FSMC_NEx low to FSMC_BL valid
 -
1.5
ns
th(BL_NWE)
FSMC_BL hold time after FSMC_NWE high
THCLK– 1
 -
ns
tv(Data_NE)
Data to FSMC_NEx low to Data valid
 -
THCLK+3
ns
th(Data_NWE)
Data hold time after FSMC_NWE high
THCLK–1
 -
ns
tv(NADV_NE)
FSMC_NEx low to FSMC_NADV low
 -
ns
tw(NADV)
FSMC_NADV low time
 -
THCLK+0.5
ns
NBL
Data
FSMC_NEx
FSMC_NBL[1:0]
FSMC_D[15:0]
tv(BL_NE)
th(Data_NWE)
FSMC_NOE
Address
FSMC_A[25:0]
tv(A_NE)
tw(NWE)
FSMC_NWE
tv(NWE_NE)
th(NE_NWE)
th(A_NWE)
th(BL_NWE)
tv(Data_NE)
tw(NE)
ai14990
FSMC_NADV(1)
tv(NADV_NE)
tw(NADV)

---

Electrical characteristics
Figure 56. Asynchronous multiplexed PSRAM/NOR read waveforms
NBL
Data
FSMC_NBL[1:0]
FSMC_AD[15:0]
tv(BL_NE)
th(Data_NE)
Address
FSMC_A[25:16]
tv(A_NE)
FSMC_NWE
t v(A_NE)
ai14892b
Address
FSMC_NADV
t v(NADV_NE)
tw(NADV)
tsu(Data_NE)
th(AD_NADV)
FSMC_NE
FSMC_NOE
tw(NE)
t w(NOE)
tv(NOE_NE)
t h(NE_NOE)
th(A_NOE)
th(BL_NOE)
tsu(Data_NOE)
th(Data_NOE)
Table 77. Asynchronous multiplexed PSRAM/NOR read timings(1)(2)
Symbol
Parameter
Min
Max
Unit
tw(NE)
FSMC_NE low time
3THCLK–1
3THCLK+1
ns
tv(NOE_NE)
FSMC_NEx low to FSMC_NOE low
2THCLK–0.5
2THCLK+0.5
ns
tw(NOE)
FSMC_NOE low time
 THCLK–1
THCLK+1
ns
th(NE_NOE)
FSMC_NOE high to FSMC_NE high hold time
 -
ns
tv(A_NE)
FSMC_NEx low to FSMC_A valid
 -
ns
tv(NADV_NE)
FSMC_NEx low to FSMC_NADV low
ns
tw(NADV)
FSMC_NADV low time
 THCLK– 2
THCLK+1
ns
th(AD_NADV)
FSMC_AD(adress) valid hold time after FSMC_NADV high)
THCLK
 -
ns
th(A_NOE)
Address hold time after FSMC_NOE high
THCLK–1
 -
ns
th(BL_NOE)
FSMC_BL time after FSMC_NOE high
 -
ns
tv(BL_NE)
FSMC_NEx low to FSMC_BL valid
-
ns
tsu(Data_NE)
Data to FSMC_NEx high setup time
THCLK+4
 -
ns
tsu(Data_NOE)
Data to FSMC_NOE high setup time
THCLK+4
 -
ns
th(Data_NE)
Data hold time after FSMC_NEx high
-
ns
th(Data_NOE)
Data hold time after FSMC_NOE high
-
ns
1.
CL = 30 pF.
2.
Evaluated by characterization - not tested in production.

---

Electrical characteristics
Figure 57. Asynchronous multiplexed PSRAM/NOR write waveforms
Table 78. Asynchronous multiplexed PSRAM/NOR write timings(1)(2)
1.
CL = 30 pF.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FSMC_NE low time
  4THCLK–0.5
 4THCLK+3
ns
tv(NWE_NE)
FSMC_NEx low to FSMC_NWE low
THCLK–0.5
THCLK -0.5
ns
tw(NWE)
FSMC_NWE low tim e
 2THCLK–0.5
2THCLK+3
ns
th(NE_NWE)
FSMC_NWE high to FSMC_NE high hold time
THCLK
-
ns
tv(A_NE)
FSMC_NEx low to FSMC_A valid
-
ns
tv(NADV_NE)
FSMC_NEx low to FSMC_NADV low
ns
tw(NADV)
FSMC_NADV low time
THCLK– 2
THCLK+ 1
ns
th(AD_NADV)
FSMC_AD(address) valid hold time after
FSMC_NADV high)
THCLK–2
-
ns
th(A_NWE)
Address hold time after FSMC_NWE high
 THCLK
-
ns
th(BL_NWE)
FSMC_BL hold time after FSMC_NWE high
THCLK–2
-
ns
tv(BL_NE)
FSMC_NEx low to FSMC_BL valid
-
1.5
ns
tv(Data_NADV)
FSMC_NADV high to Data valid
-
 THCLK–0.5
ns
th(Data_NWE)
Data hold time after FSMC_NWE high
 THCLK
-
ns
NBL
Data
FSMC_NEx
FSMC_NBL[1:0]
FSMC_AD[15:0]
tv(BL_NE)
th(Data_NWE)
FSMC_NOE
Address
FSMC_A[25:16]
tv(A_NE)
tw(NWE)
FSMC_NWE
tv(NWE_NE)
th(NE_NWE)
th(A_NWE)
th(BL_NWE)
tv(A_NE)
tw(NE)
ai14891B
Address
FSMC_NADV
tv(NADV_NE)
tw(NADV)
tv(Data_NADV)
th(AD_NADV)

---

Electrical characteristics
Synchronous waveforms and timings
Figure 58 through Figure 61 represent synchronous waveforms and Table 80 through
Table 82 provide the corresponding timings. The results shown in these tables are obtained
with the following FSMC configuration:
•
BurstAccessMode = FSMC_BurstAccessMode_Enable;
•
MemoryType = FSMC_MemoryType_CRAM;
•
WriteBurst = FSMC_WriteBurst_Enable;
•
CLKDivision = 1; (0 is not supported, see the STM32F40xxx/41xxx reference manual)
•
DataLatency = 1 for NOR Flash; DataLatency = 0 for PSRAM
In all timing tables, the THCLK is the HCLK clock period (with maximum
FSMC_CLK = 60 MHz).
Figure 58. Synchronous multiplexed NOR/PSRAM read timings
2.
Evaluated by characterization - not tested in production.
FSMC_CLK
FSMC_NEx
FSMC_NADV
FSMC_A[25:16]
FSMC_NOE
FSMC_AD[15:0]
AD[15:0]
D1
D2
FSMC_NWAIT
(WAITCFG = 1b, WAITPOL + 0b)
FSMC_NWAIT
(WAITCFG = 0b, WAITPOL + 0b)
tw(CLK)
tw(CLK)
Data latency = 0
BUSTURN = 0
td(CLKL-NExL)
td(CLKL-NExH)
td(CLKL-NADVL)
td(CLKL-AV)
td(CLKL-NADVH)
td(CLKL-AIV)
td(CLKL-NOEL)
td(CLKL-NOEH)
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
ai14893g

---

Electrical characteristics
Table 79. Synchronous multiplexed NOR/PSRAM read timings(1)(2)
1.
CL = 30 pF.
2.
Evaluated by characterization - not tested in production.
Symbol
Parameter
Min
Max
Unit
tw(CLK)
FSMC_CLK period
2THCLK
 -
ns
td(CLKL-NExL)
FSMC_CLK low to FSMC_NEx low (x=0..2)
 -
ns
td(CLKL-NExH)
FSMC_CLK low to FSMC_NEx high (x= 0…2)
 -
ns
td(CLKL-NADVL)
FSMC_CLK low to FSMC_NADV low
 -
ns
td(CLKL-NADVH)
FSMC_CLK low to FSMC_NADV high
 -
ns
td(CLKL-AV)
FSMC_CLK low to FSMC_Ax valid (x=16…25)
 -
ns
td(CLKL-AIV)
FSMC_CLK low to FSMC_Ax invalid (x=16…25)
 -
ns
td(CLKL-NOEL)
FSMC_CLK low to FSMC_NOE low
 -
ns
td(CLKL-NOEH)
FSMC_CLK low to FSMC_NOE high
 -
ns
td(CLKL-ADV)
FSMC_CLK low to FSMC_AD[15:0] valid
 -
4.5
ns
td(CLKL-ADIV)
FSMC_CLK low to FSMC_AD[15:0] invalid
-
ns
tsu(ADV-CLKH)
FSMC_A/D[15:0] valid data before FSMC_CLK high
 -
ns
th(CLKH-ADV)
FSMC_A/D[15:0] valid data after FSMC_CLK high
 -
ns
tsu(NWAIT-CLKH)
FSMC_NWAIT valid before FSMC_CLK high
-
ns
th(CLKH-NWAIT)
FSMC_NWAIT valid after FSMC_CLK high
-
ns

---

Electrical characteristics
Figure 59. Synchronous multiplexed PSRAM write timings
Table 80. Synchronous multiplexed PSRAM write timings(1)(2)
Symbol
Parameter
Min
Max
Unit
tw(CLK)
FSMC_CLK period
2THCLK
 -
ns
td(CLKL-NExL)
FSMC_CLK low to FSMC_NEx low (x=0..2)
 -
ns
td(CLKL-NExH)
FSMC_CLK low to FSMC_NEx high (x= 0…2)
 -
ns
td(CLKL-NADVL) FSMC_CLK low to FSMC_NADV low
 -
ns
td(CLKL-
NADVH)
FSMC_CLK low to FSMC_NADV high
 -
ns
td(CLKL-AV)
FSMC_CLK low to FSMC_Ax valid (x=16…25)
 -
ns
td(CLKL-AIV)
FSMC_CLK low to FSMC_Ax invalid (x=16…25)
 -
ns
td(CLKL-NWEL)
FSMC_CLK low to FSMC_NWE low
 -
0.5
ns
td(CLKL-NWEH)
FSMC_CLK low to FSMC_NWE high
 -
ns
td(CLKL-ADIV)
FSMC_CLK low to FSMC_AD[15:0] invalid
 -
ns
td(CLKL-DATA)
FSMC_A/D[15:0] valid data after FSMC_CLK
low
 -
ns
FSMC_CLK
FSMC_NEx
FSMC_NADV
FSMC_A[25:16]
FSMC_NWE
FSMC_AD[15:0]
AD[15:0]
D1
D2
FSMC_NWAIT
(WAITCFG = 0b, WAITPOL + 0b)
tw(CLK)
tw(CLK)
Data latency = 0
BUSTURN = 0
td(CLKL-NExL)
td(CLKL-NExH)
td(CLKL-NADVL)
td(CLKL-AV)
td(CLKL-NADVH)
td(CLKL-AIV)
td(CLKL-NWEH)
td(CLKL-NWEL)
td(CLKL-NBLH)
td(CLKL-ADV)
td(CLKL-ADIV)
td(CLKL-Data)
tsu(NWAITV-CLKH)
th(CLKH-NWAITV)
ai14992g
td(CLKL-Data)
FSMC_NBL

---

Electrical characteristics
Figure 60. Synchronous non-multiplexed NOR/PSRAM read timings
td(CLKL-NBLH)
FSMC_CLK low to FSMC_NBL high
 -
ns
tsu(NWAIT-
CLKH)
FSMC_NWAIT valid before FSMC_CLK high
-
ns
th(CLKH-NWAIT) FSMC_NWAIT valid after FSMC_CLK high
-
ns
1.
CL = 30 pF.
2.
Evaluated by characterization - not tested in production.
Table 80. Synchronous multiplexed PSRAM write timings(1)(2) (continued)
Symbol
Parameter
Min
Max
Unit
FSMC_CLK
FSMC_NEx
FSMC_A[25:0]
FSMC_NOE
FSMC_D[15:0]
D1
D2
FSMC_NWAIT
(WAITCFG = 1b, WAITPOL + 0b)
FSMC_NWAIT
(WAITCFG = 0b, WAITPOL + 0b)
tw(CLK)
tw(CLK)
Data latency = 0
BUSTURN = 0
td(CLKL-NExL)
td(CLKL-NExH)
td(CLKL-AV)
td(CLKL-AIV)
td(CLKL-NOEL)
td(CLKL-NOEH)
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
ai14894f
FSMC_NADV
td(CLKL-NADVL)
td(CLKL-NADVH)

---

Electrical characteristics
Table 81. Synchronous non-multiplexed NOR/PSRAM read timings(1)(2)
1.
CL = 30 pF.
2.
Evaluated by characterization - not tested in production.
Symbol
Parameter
Min
Max
Unit
tw(CLK)
FSMC_CLK period
2THCLK –0.5
 -
ns
td(CLKL-NExL)
FSMC_CLK low to FSMC_NEx low (x=0..2)
 -
0.5
ns
td(CLKL-NExH)
FSMC_CLK low to FSMC_NEx high (x= 0…2)
 -
ns
td(CLKL-NADVL)
FSMC_CLK low to FSMC_NADV low
 -
ns
td(CLKL-NADVH)
FSMC_CLK low to FSMC_NADV high
 -
ns
td(CLKL-AV)
FSMC_CLK low to FSMC_Ax valid (x=16…25)
 -
ns
td(CLKL-AIV)
FSMC_CLK low to FSMC_Ax invalid (x=16…25)
 -
ns
td(CLKL-NOEL)
FSMC_CLK low to FSMC_NOE low
 -
0.5
ns
td(CLKL-NOEH)
FSMC_CLK low to FSMC_NOE high
1.5
 -
ns
tsu(DV-CLKH)
FSMC_D[15:0] valid data before FSMC_CLK high
 -
ns
th(CLKH-DV)
FSMC_D[15:0] valid data after FSMC_CLK high
 -
ns
tsu(NWAIT-CLKH)
FSMC_NWAIT valid before FSMC_CLK high
-
ns
th(CLKH-NWAIT)
FSMC_NWAIT valid after FSMC_CLK high
-
ns

---

Electrical characteristics
Figure 61. Synchronous non-multiplexed PSRAM write timings
Table 82. Synchronous non-multiplexed PSRAM write timings(1)(2)
1.
CL = 30 pF.
2.
Evaluated by characterization - not tested in production.
Symbol
Parameter
Min
Max
Unit
tw(CLK)
FSMC_CLK period
2THCLK
 -
ns
td(CLKL-NExL)
FSMC_CLK low to FSMC_NEx low (x=0..2)
 -
ns
td(CLKL-NExH)
FSMC_CLK low to FSMC_NEx high (x= 0…2)
 -
ns
td(CLKL-NADVL)
FSMC_CLK low to FSMC_NADV low
 -
ns
td(CLKL-NADVH)
FSMC_CLK low to FSMC_NADV high
 -
ns
td(CLKL-AV)
FSMC_CLK low to FSMC_Ax valid (x=16…25)
 -
ns
td(CLKL-AIV)
FSMC_CLK low to FSMC_Ax invalid (x=16…25)
 -
ns
td(CLKL-NWEL)
FSMC_CLK low to FSMC_NWE low
 -
ns
td(CLKL-NWEH)
FSMC_CLK low to FSMC_NWE high
 -
ns
td(CLKL-Data)
FSMC_D[15:0] valid data after FSMC_CLK low
 -
ns
td(CLKL-NBLH)
FSMC_CLK low to FSMC_NBL high
 -
ns
tsu(NWAIT-CLKH)
FSMC_NWAIT valid before FSMC_CLK high
-
ns
th(CLKH-NWAIT)
FSMC_NWAIT valid after FSMC_CLK high
-
ns
FSMC_CLK
FSMC_NEx
FSMC_A[25:0]
FSMC_NWE
FSMC_D[15:0]
D1
D2
FSMC_NWAIT
(WAITCFG = 0b, WAITPOL + 0b)
tw(CLK)
tw(CLK)
Data latency = 0
BUSTURN = 0
td(CLKL-NExL)
td(CLKL-NExH)
td(CLKL-AV)
td(CLKL-AIV)
td(CLKL-NWEH)
td(CLKL-NWEL)
td(CLKL-Data)
tsu(NWAITV-CLKH)
th(CLKH-NWAITV)
ai14993g
FSMC_NADV
td(CLKL-NADVL)
td(CLKL-NADVH)
td(CLKL-Data)
FSMC_NBL
td(CLKL-NBLH)

---

Electrical characteristics
PC Card/CompactFlash controller waveforms and timings
Figure 62 through Figure 67 represent synchronous waveforms, and Table 83 and Table 84
provide the corresponding timings. The results shown in this table are obtained with the
following FSMC configuration:
•
COM.FSMC_SetupTime = 0x04;
•
COM.FSMC_WaitSetupTime = 0x07;
•
COM.FSMC_HoldSetupTime = 0x04;
•
COM.FSMC_HiZSetupTime = 0x00;
•
ATT.FSMC_SetupTime = 0x04;
•
ATT.FSMC_WaitSetupTime = 0x07;
•
ATT.FSMC_HoldSetupTime = 0x04;
•
ATT.FSMC_HiZSetupTime = 0x00;
•
IO.FSMC_SetupTime = 0x04;
•
IO.FSMC_WaitSetupTime = 0x07;
•
IO.FSMC_HoldSetupTime = 0x04;
•
IO.FSMC_HiZSetupTime = 0x00;
•
TCLRSetupTime = 0;
•
TARSetupTime = 0.
In all timing tables, the THCLK is the HCLK clock period.
Figure 62. PC Card/CompactFlash controller waveforms for common memory read
access
1.
FSMC_NCE4_2 remains high (inactive during 8-bit access.
FSMC_NWE
tw(NOE)
FSMC_NOE
FSMC_D[15:0]
FSMC_A[10:0]
FSMC_NCE4_2(1)
FSMC_NCE4_1
FSMC_NREG
FSMC_NIOWR
FSMC_NIORD
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
ai14895b

---

Electrical characteristics
Figure 63. PC Card/CompactFlash controller waveforms for common memory write
access
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
ai14896
FSMC_NWE
FSMC_NOE
FSMC_D[15:0]
FSMC_A[10:0]
FSMC_NCE4_1
FSMC_NREG
FSMC_NIOWR
FSMC_NIORD
td(NWE-NCE4_1)
td(D-NWE)
FSMC_NCE4_2 High

---

Electrical characteristics
Figure 64. PC Card/CompactFlash controller waveforms for attribute memory read
access
1.
Only data bits 0...7 are read (bits 8...15 are disregarded).
td(NCE4_1-NOE)
tw(NOE)
tsu(D-NOE)
th(NOE-D)
tv(NCE4_1-A)
th(NCE4_1-AI)
td(NREG-NCE4_1)
th(NCE4_1-NREG)
ai14897b
FSMC_NWE
FSMC_NOE
FSMC_D[15:0](1)
FSMC_A[10:0]
FSMC_NCE4_2
FSMC_NCE4_1
FSMC_NREG
FSMC_NIOWR
FSMC_NIORD
td(NOE-NCE4_1)
High

---

Electrical characteristics
Figure 65. PC Card/CompactFlash controller waveforms for attribute memory write
access
1.
Only data bits 0...7 are driven (bits 8...15 remains Hi-Z).
Figure 66. PC Card/CompactFlash controller waveforms for I/O space read access
tw(NWE)
tv(NCE4_1-A)
td(NREG-NCE4_1)
th(NCE4_1-AI)
th(NCE4_1-NREG)
tv(NWE-D)
ai14898b
FSMC_NWE
FSMC_NOE
FSMC_D[7:0](1)
FSMC_A[10:0]
FSMC_NCE4_2
FSMC_NCE4_1
FSMC_NREG
FSMC_NIOWR
FSMC_NIORD
td(NWE-NCE4_1)
High
td(NCE4_1-NWE)
td(NIORD-NCE4_1)
tw(NIORD)
tsu(D-NIORD)
td(NIORD-D)
tv(NCEx-A)
th(NCE4_1-AI)
ai14899B
FSMC_NWE
FSMC_NOE
FSMC_D[15:0]
FSMC_A[10:0]
FSMC_NCE4_2
FSMC_NCE4_1
FSMC_NREG
FSMC_NIOWR
FSMC_NIORD

---

Electrical characteristics
Figure 67. PC Card/CompactFlash controller waveforms for I/O space write access
td(NCE4_1-NIOWR)
tw(NIOWR)
tv(NCEx-A)
th(NCE4_1-AI)
th(NIOWR-D)
ATTxHIZ =1
tv(NIOWR-D)
ai14900c
FSMC_NWE
FSMC_NOE
FSMC_D[15:0]
FSMC_A[10:0]
FSMC_NCE4_2
FSMC_NCE4_1
FSMC_NREG
FSMC_NIOWR
FSMC_NIORD
Table 83. Switching characteristics for PC Card/CF read and write cycles
 in attribute/common space(1)(2)
Symbol
Parameter
Min
Max
Unit
tv(NCEx-A)
FSMC_Ncex low to FSMC_Ay valid
-
ns
th(NCEx_AI)
FSMC_NCEx high to FSMC_Ax invalid
-
ns
td(NREG-NCEx) FSMC_NCEx low to FSMC_NREG valid
-
3.5
ns
th(NCEx-NREG) FSMC_NCEx high to FSMC_NREG invalid
THCLK+4
-
ns
td(NCEx-NWE)
FSMC_NCEx low to FSMC_NWE low
-
5THCLK+0.5
ns
td(NCEx-NOE)
FSMC_NCEx low to FSMC_NOE low
-
5THCLK +0.5
ns
tw(NOE)
FSMC_NOE low width
8THCLK–1
8THCLK+1
ns
td(NOE_NCEx)
FSMC_NOE high to FSMC_NCEx high
5THCLK+2.5
-
ns
tsu (D-NOE)
FSMC_D[15:0] valid data before FSMC_NOE high
4.5
-
ns
th(N0E-D)
FSMC_N0E high to FSMC_D[15:0] invalid
-
ns
tw(NWE)
FSMC_NWE low width
8THCLK–0.5
8THCLK+ 3
ns
td(NWE_NCEx)
FSMC_NWE high to FSMC_NCEx high
5THCLK–1
-
ns
td(NCEx-NWE)
FSMC_NCEx low to FSMC_NWE low
-
5THCLK+ 1
ns
tv(NWE-D)
FSMC_NWE low to FSMC_D[15:0] valid
-
ns
th (NWE-D)
FSMC_NWE high to FSMC_D[15:0] invalid
8THCLK –1
-
ns
td (D-NWE)
FSMC_D[15:0] valid before FSMC_NWE high
13THCLK –1
-
ns
1.
CL = 30 pF.
2.
Evaluated by characterization - not tested in production.

---

Electrical characteristics
NAND controller waveforms and timings
Figure 68 and Figure 69 represent synchronous waveforms, and Table 85 and Table 86
provide the corresponding timings. The results shown in this table are obtained with the
following FSMC configuration:
•
COM.FSMC_SetupTime = 0x01;
•
COM.FSMC_WaitSetupTime = 0x03;
•
COM.FSMC_HoldSetupTime = 0x02;
•
COM.FSMC_HiZSetupTime = 0x01;
•
ATT.FSMC_SetupTime = 0x01;
•
ATT.FSMC_WaitSetupTime = 0x03;
•
ATT.FSMC_HoldSetupTime = 0x02;
•
ATT.FSMC_HiZSetupTime = 0x01;
•
Bank = FSMC_Bank_NAND;
•
MemoryDataWidth = FSMC_MemoryDataWidth_16b;
•
ECC = FSMC_ECC_Enable;
•
ECCPageSize = FSMC_ECCPageSize_512Bytes;
•
TCLRSetupTime = 0;
•
TARSetupTime = 0.
In all timing tables, the THCLK is the HCLK clock period.
Table 84. Switching characteristics for PC Card/CF read and write cycles
 in I/O space(1)(2)
Symbol
Parameter
Min
Max
Unit
tw(NIOWR)
FSMC_NIOWR low width
8THCLK –1
 -
ns
tv(NIOWR-D)
FSMC_NIOWR low to FSMC_D[15:0] valid
 -
5THCLK– 1
ns
th(NIOWR-D)
FSMC_NIOWR high to FSMC_D[15:0] invalid
8THCLK– 2
-
ns
td(NCE4_1-NIOWR) FSMC_NCE4_1 low to FSMC_NIOWR valid
 -
5THCLK+ 2.5
ns
th(NCEx-NIOWR)
FSMC_NCEx high to FSMC_NIOWR invalid
5THCLK–1.5
 -
ns
td(NIORD-NCEx)
FSMC_NCEx low to FSMC_NIORD valid
 -
5THCLK+ 2
ns
th(NCEx-NIORD)
FSMC_NCEx high to FSMC_NIORD) valid
5THCLK– 1.5
 -
ns
tw(NIORD)
FSMC_NIORD low width
8THCLK–0.5
 -
ns
tsu(D-NIORD)
FSMC_D[15:0] valid before FSMC_NIORD high
-
ns
td(NIORD-D)
FSMC_D[15:0] valid after FSMC_NIORD high
-
ns
1.
CL = 30 pF.
2.
Evaluated by characterization - not tested in production.

---

Electrical characteristics
Figure 68. NAND controller waveforms for read access
1.
y = 7 or 15 depending on the NAND flash memory interface.
Figure 69. NAND controller waveforms for write access
1.
y = 7 or 15 depending on the NAND flash memory interface.
Table 85. Switching characteristics for NAND Flash read cycles(1)
1.
CL = 30 pF.
Symbol
Parameter
Min
Max
Unit
tw(N0E)
FSMC_NOE low width
4THCLK–
0.5
4THCLK+ 3
ns
tsu(D-NOE)
FSMC_D[15-0] valid data before FSMC_NOE high
-
ns
th(NOE-D)
FSMC_D[15-0] valid data after FSMC_NOE high
-
ns
td(ALE-NOE)
FSMC_ALE valid before FSMC_NOE low
-
3THCLK
ns
th(NOE-ALE)
FSMC_NWE high to FSMC_ALE invalid
3THCLK– 2
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
6.3.27
Camera interface (DCMI) timing specifications
Unless otherwise specified, the parameters given in Table 87 for DCMI are derived from
tests performed under the ambient temperature, fHCLK frequency and VDD supply voltage
summarized in Table 13, with the following configuration:
•
PCK polarity: falling
•
VSYNC and HSYNC polarity: high
•
Data format: 14 bits
Figure 70. DCMI timing diagram
Table 86. Switching characteristics for NAND Flash write cycles(1)
1.
CL = 30 pF.
Symbol
Parameter
Min
Max
Unit
tw(NWE)
FSMC_NWE low width
4THCLK–1
4THCLK+ 3
ns
tv(NWE-D)
FSMC_NWE low to FSMC_D[15-0] valid
-
ns
th(NWE-D)
FSMC_NWE high to FSMC_D[15-0] invalid
3THCLK –2
-
ns
td(D-NWE)
FSMC_D[15-0] valid before FSMC_NWE high
5THCLK–3
-
ns
td(ALE-NWE)
FSMC_ALE valid before FSMC_NWE low
-
3THCLK
ns
th(NWE-ALE)
FSMC_NWE high to FSMC_ALE invalid
3THCLK–2
-
ns
Table 87. DCMI characteristics(1)
Symbol
Parameter
Min
Max
Unit
-
Frequency ratio DCMI_PIXCLK/fHCLK
-
0.4
-
DCMI_PIXCLK
Pixel clock input
-
MHz
Dpixel
Pixel clock input duty cycle
%
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
SD/SDIO MMC card host interface (SDIO) characteristics
Unless otherwise specified, the parameters given in Table 88 are derived from tests
performed under ambient temperature, fPCLKx frequency and VDD supply voltage conditions
summarized in Table 14 with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load C = 30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output
characteristics.
Figure 71. SDIO high-speed mode
tsu(DATA)
Data input setup time
2.5
-
ns
th(DATA)
Data hold time
-
tsu(HSYNC),
tsu(VSYNC)
HSYNC/VSYNC input setup time
-
th(HSYNC),
th(VSYNC)
HSYNC/VSYNC input hold time
0.5
-
1.
Evaluated by characterization - not tested in production.
Table 87. DCMI characteristics(1) (continued)
Symbol
Parameter
Min
Max
Unit

---

Electrical characteristics
Figure 72. SD default mode
6.3.29
RTC characteristics
ai14888
CK
D, CMD
(output)
tOVD
tOHD
Table 88. Dynamic characteristics: SD/MMC characteristics(1)
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
SDIO_CK/fPCLK2 frequency ratio
-
-
-
-
tW(CKL)
Clock low time
fPP = 48 MHz
8.5
-
ns
tW(CKH)
Clock high time
fPP = 48 MHz
8.3
-
CMD, D inputs (referenced to CK) in MMC and SD HS mode
tISU
Input setup time HS
fPP = 48 MHz
-
-
ns
tIH
Input hold time HS
fPP = 48 MHz
-
-
CMD, D outputs (referenced to CK) in MMC and SD HS mode
tOV
Output valid time HS
fPP = 48 MHz
-
4.5
ns
tOH
Output hold time HS
fPP = 48 MHz
-
-
CMD, D inputs (referenced to CK) in SD default mode
tISUD
Input setup time SD
fPP = 24 MHz
1.5
-
-
ns
tIHD
Input hold time SD
fPP = 24 MHz
0.5
-
-
CMD, D outputs (referenced to CK) in SD default mode
tOVD
Output valid default time SD
fPP = 24 MHz
-
4.5
ns
tOHD
Output hold default time SD
fPP = 24 MHz
0.5
-
-
1.
Evaluated by characterization - not tested in production.
Table 89. RTC characteristics
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
ECOPACK packages, depending on their level of environmental compliance. ECOPACK
specifications, grade definitions and product status are available at: www.st.com.
ECOPACK is an ST trademark.
7.1
Device marking
Refer to technical note “Reference device marking schematics for STM32 microcontrollers
and microprocessors” (TN1433), available on www.st.com, for the location of pin 1 / ball A1
as well as the location and orientation of the marking areas versus pin 1 / ball A1.
Parts marked as “ES”, “E”, or accompanied by an engineering sample notification letter, are
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
WLCSP90 package information
Figure 73. WLCSP90 - 4.223 x 3.969 mm, 0.400 mm pitch wafer level chip scale
package outline
1.
Drawing is not to scale.
A0JW_ME_V4
SIDE VIEW
Detail A
A1
Detail A
(Rotated 90°)
eee
D
Seating plane
A2
A
b
E
e
e1
e
G
e2
A3
A1 ball location
BOTTOM VIEW
BUMP SIDE
A2
A
A3
FRONT VIEW
TOP VIEW
WAFER BACK SIDE
 bbb Z
J
A
F
ccc
ddd
Z
Z X Y
Z
Bump
A1 orientation
reference
 aaa
 (4X)

---

Package information
Figure 74. WLCSP90 - 4.223 x 3.969 mm, 0.400 mm pitch wafer level chip scale
package recommended footprint
Table 90. WLCSP90 - 4.223 x 3.969 mm, 0.400 mm pitch wafer level chip scale
package mechanical data
 Symbol
millimeters
inches(1)
1.
Values in inches are converted from mm and rounded to 4 decimal digits.
Min
Typ
Max
Min
Typ
Max
A
0.540
0.570
0.600
0.0213
0.0224
0.0236
A1
-
0.190
-
-
0.0075
-
A2
-
0.380
-
-
0.0150
-
A3(2)
2.
Back side coating.
-
0.025
-
-
0.0010
-
b(3)
3.
Dimension is measured at the maximum bump diameter parallel to primary datum Z.
0.240
0.270
0.300
0.0094
0.0106
0.0118
D
4.188
4.223
4.258
0.1649
0.1663
0.1676
E
3.934
3.969
4.004
0.1549
0.1563
0.1576
e
-
0.400
-
-
0.0157
-
e1
-
3.600
-
-
0.1417
-
e2
-
3.200
-
-
0.1260
-
F
-
0.3115
-
-
0.0123
-
G
-
0.3845
-
-
0.0151
-
aaa
-
0.100
-
-
0.0039
-
bbb
-
0.100
-
-
0.0039
-
ccc
-
0.100
-
-
0.0039
-
ddd
-
0.050
-
-
0.0020
-
eee
-
0.050
-
-
0.0020
-
MS18965V2
Dsm
Dpad

---

Package information
Device marking for WLCSP90
The following figure gives an example of topside marking and ball A1 position identifier
location.
The printed markings may differ depending on the supply chain.
Other optional marking or inset/upset marks, which depend on supply chain operations, are
not indicated below.
Figure 75. WLCSP90 marking example (package top view)
1.
Parts marked as “ES”, “E” or accompanied by an Engineering Sample notification letter, are not yet
qualified and therefore not yet ready to be used in production and any consequences deriving from such
usage will not be at ST charge. In no event, ST will be liable for any customer usage of these engineering
samples in production. ST Quality has to be contacted prior to any decision to use these Engineering
Samples to run qualification activity.
Table 91. WLCSP90 recommended PCB design rules
 Dimension
Recommended values
Pitch
0.4 mm
Dpad
260 µm max. (circular)
220 µm recommended
Dsm
300 µm min. (for 260 µm diameter pad)
PCB pad design
Non-solder mask defined via underbump allowed
MSv36120V1
Ball A1
indentifer
Product identification(1)
F405OEB
R
Date code
Y
WW

---

Package information
7.3
LQFP64 package information (5W)
This LQFP is 64-pin, 10 x 10 mm low-profile quad flat package.
Figure 76. LQFP64 - Outline(15)
D 1/4
E 1/4
(6)
aaa C A-B D
4x N/4 TIPS
bbb H A-B D 4x
(13) (N – 4)x e
0.05
A
A2 A1 (12)
b
ddd
C A-B D
ccc
C
C
D
(5) (2)
(4)
D1
D
(3)
D 1/4
E 1/4
(6)
(3)
A
B
(3)
(5)
(2)
E1
E
A
A
(Section A-A)
(4)
5W_LQFP64_ME_V1
(10)
N
BOTTOM VIEW
TOP VIEW
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
WITH PLATING
BASE METAL
(L1)
(2)
0.25
(11)
(9)(11)
(11)
(11)
(11)
(1)

---

Package information
Table 92. LQFP64 - Mechanical data
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
12.00 BSC
0.4724 BSC
D1(2)(5)
10.00 BSC
0.3937 BSC
E(4)
12.00 BSC
0.4724 BSC
E1(2)(5)
10.00 BSC
0.3937 BSC
e
0.50 BSC
0.1970 BSC
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

---

Package information
7.4
LQFP100 package information (1L)
This LQFP is a 100-pin, 14 x 14 mm low-profile quad flat package.
Note:
See list of notes in the notes section.
Figure 77. LQFP100 - Outline(15)
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
Table 93. LQFP100 - Mechanical data
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
The top package body size may be smaller than the bottom package size by as much
as 0.15 mm.
3.
Datums A-B and D to be determined at datum plane H.
4.
To be determined at the seating datum plane C.
5.
Dimensions D1 and E1 do not include mold flash or protrusions. Allowable mold flash
or protrusions is 0.25 mm per side. D1 and E1 are maximum plastic body size
dimensions including mold mismatch.
6.
Details of pin 1 identifier are optional but must be located within the zone indicated.
7.
All dimensions are in millimeters.
8.
No intrusion is allowed inwards the leads.
9.
Dimension b does not include a dambar protrusion. Allowable dambar protrusion shall
not cause the lead width to exceed the maximum b dimension by more than 0.08 mm.
The dambar cannot be located on the lower radius or the foot. The minimum space
between the protrusion and an adjacent lead is 0.07 mm for 0.4 mm and 0.5 mm pitch
packages.
10. The exact shape of each corner is optional.
11. These dimensions apply to the flat section of the lead that is between 0.10 mm and
0.25 mm from the lead tip.
12. A1 is defined as the distance from the seating plane to the lowest point on the package
body.
13. N is the number of terminal positions for the specified body size.
14. Values in inches are converted from mm and rounded to four decimal digits.
15. Drawing is not to scale.
Figure 78. LQFP100 - Footprint example
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
7.5
LQFP144 package information (1A)
This LQFP is a 144-pin, 20 x 20 mm low-profile quad flat package.
Note:
See list of notes in the notes section.
Figure 79. LQFP144 - Outline(15)
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
Table 94. LQFP144 - Mechanical data
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
The top package body size may be smaller than the bottom package size by as much
as 0.15 mm.
3.
Datums A-B and D to be determined at datum plane H.
4.
To be determined at the seating datum plane C.
5.
Dimensions D1 and E1 do not include mold flash or protrusions. Allowable mold flash
or protrusions is 0.25 mm per side. D1 and E1 are maximum plastic body size
dimensions including mold mismatch.
6.
Details of pin 1 identifier are optional but must be located within the zone indicated.
7.
All dimensions are in millimeters.
8.
No intrusion is allowed inwards the leads.
9.
Dimension b does not include a dambar protrusion. Allowable dambar protrusion shall
not cause the lead width to exceed the maximum b dimension by more than 0.08 mm.
The dambar cannot be located on the lower radius or the foot. The minimum space
between the protrusion and an adjacent lead is 0.07 mm for 0.4 mm and 0.5 mm pitch
packages.
10. The exact shape of each corner is optional.
11. These dimensions apply to the flat section of the lead that is between 0.10 mm and
0.25 mm from the lead tip.
12. A1 is defined as the distance from the seating plane to the lowest point on the package
body.
13. N is the number of terminal positions for the specified body size.
14. Values in inches are converted from mm and rounded to four decimal digits.
15. Drawing is not to scale.

---

Package information
Figure 80. LQFP144 - Footprint example
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
7.6
UFBGA(176+25) package information (A0E7)
This UFBGA is a 176+25-ball, 10 x 10 mm, 0.65 mm pitch, ultra fine pitch ball grid array
package.
Figure 81. UFBGA(176+25) - Outline
1.
Drawing is not to scale.
Table 95. UFBGA(176+25) - Mechanical data
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
Figure 82. UFBGA(176+25) - Footprint example
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
Table 96. UFBGA(176+25) - Example of PCB design rules (0.65 mm pitch BGA)
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
Table 95. UFBGA(176+25) - Mechanical data (continued)
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
7.7
LQFP176 package information (1T)
This LQFP is a 176-pin, 24 x 24 mm, 0.5 mm pitch, low profile quad flat package.
Note:
See list of notes in the notes section.
Figure 83. LQFP176 - Outline(15)
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
Table 97. LQFP176 - Mechanical data
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
The top package body size may be smaller than the bottom package size by as much
as 0.15 mm.
3.
Datums A-B and D to be determined at datum plane H.
4.
To be determined at the seating datum plane C.
5.
Dimensions D1 and E1 do not include mold flash or protrusions. Allowable mold flash
or protrusions is 0.25 mm per side. D1 and E1 are maximum plastic body size
dimensions including mold mismatch.
6.
Details of pin 1 identifier are optional but must be located within the zone indicated.
7.
All dimensions are in millimeters.
8.
No intrusion is allowed inwards the leads.
9.
Dimension b does not include a dambar protrusion. Allowable dambar protrusion shall
not cause the lead width to exceed the maximum b dimension by more than 0.08 mm.
The dambar cannot be located on the lower radius or the foot. The minimum space
between the protrusion and an adjacent lead is 0.07 mm for 0.4 mm and 0.5 mm pitch
packages.
10. The exact shape of each corner is optional.
11. These dimensions apply to the flat section of the lead that is between 0.10 mm and
0.25 mm from the lead tip.
12. A1 is defined as the distance from the seating plane to the lowest point on the package
body.
13. N is the number of terminal positions for the specified body size.
14. Values in inches are converted from mm and rounded to four decimal digits.
15. Drawing is not to scale.

---

Package information
Figure 84. LQFP176 - Footprint example
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
7.8
Thermal characteristics
The maximum chip-junction temperature, TJ max, in degrees Celsius, may be calculated
using the following equation:
TJ max = TA max + (PD max x ΘJA)
Where:
•
TA max is the maximum ambient temperature in ° C,
•
ΘJA is the package junction-to-ambient thermal resistance, in ° C/W,
•
PD max is the sum of PINT max and PI/O max (PD max = PINT max + PI/Omax),
•
PINT max is the product of IDD and VDD, expressed in Watts. This is the maximum chip
internal power.
PI/O max represents the maximum power dissipation on output pins where:
PI/O max = Σ (VOL × IOL) + Σ((VDD – VOH) × IOH),
taking into account the actual VOL / IOL and VOH / IOH of the I/Os at low and high level in the
application.
Reference document
JESD51-2 Integrated Circuits Thermal Test Method Environment Conditions - Natural
Convection (Still Air). Available from www.jedec.org.
Table 98. Package thermal characteristics
Symbol
Parameter
Value
Unit
ΘJA
Thermal resistance junction-ambient
LQFP64 - 10 × 10 mm / 0.5 mm pitch
°C/W
Thermal resistance junction-ambient
LQFP100 - 14 × 14 mm / 0.5 mm pitch
Thermal resistance junction-ambient
LQFP144 - 20 × 20 mm / 0.5 mm pitch
Thermal resistance junction-ambient
LQFP176 - 24 × 24 mm / 0.5 mm pitch
Thermal resistance junction-ambient
UFBGA176 - 10× 10 mm / 0.65 mm pitch
Thermal resistance junction-ambient
WLCSP90 - 0.400 mm pitch
38.1

---

Ordering information
Ordering information
For a list of available options (speed, package, etc.) or for further information on any aspect
of this device, please contact your nearest ST sales office.
Example:
STM32
F
405 R
E
T
xxx
Device family
STM32 = Arm-based 32-bit microcontroller
Product type
F = general-purpose
Device subfamily
405 = STM32F40xxx, connectivity
407= STM32F40xxx, connectivity, camera interface, Ethernet
Pin count
R = 64 pins
O = 90 pins
V = 100 pins
Z = 144 pins
I = 176 pins
Flash memory size
E = 512 Kbytes of flash memory
G = 1024 Kbytes of flash memory
Package
T = LQFP
H = UFBGA
Y = WLCSP
Temperature range
6 = Industrial temperature range, –40 to 85 °C.
7 = Industrial temperature range, –40 to 105 °C.
Options
xxx = programmed parts
TR = tape and reel

---

Application block diagrams
Appendix A
Application block diagrams
A.1
USB OTG full speed (FS) interface solutions
Figure 85. USB controller configured as peripheral-only and used
in Full speed mode
1.
External voltage regulator only needed when building a VBUS powered device.
2.
The same application can be developed using the OTG HS in FS mode to achieve enhanced performance
thanks to the large Rx/Tx FIFO and to a dedicated DMA controller.
Figure 86. USB controller configured as host-only and used in full speed mode
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
Figure 87. USB controller configured in dual mode and used in full speed mode
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
A.2
USB OTG high speed (HS) interface solutions
Figure 88. USB controller configured as peripheral, host, or dual-mode
and used in high speed mode
1.
It is possible to use MCO1 or MCO2 to save a crystal. It is however not mandatory to clock the
STM32F40xxx with a 24 or 26 MHz crystal when using USB HS. The above figure only shows an example
of a possible connection.
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
A.3
Ethernet interface solutions
Figure 89. MII mode using a 25 MHz crystal
1.
fHCLK must be greater than 25 MHz.
2.
Pulse per second when using IEEE1588 PTP optional signal.
Figure 90. RMII with a 50 MHz oscillator
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
Figure 91. RMII with a 25 MHz crystal and PHY with PLL
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

Table 99. Document revision history
Date
Changes
15-Sep-2011
Initial release.
24-Jan-2012
Added WLCSP90 package on cover page.
Renamed USART4 and USART5 into UART4 and UART5,
respectively.
Updated number of USB OTG HS and FS in Table 3: STM32F405xx
and STM32F407xx: features and peripheral counts.
Updated Figure 3: Compatible board design between
STM32F10xx/STM32F2/STM32F40xxx for LQFP144 package and
Figure 4: Compatible board design between STM32F2 and
STM32F40xxx  for LQFP176 and BGA176 packages, and removed
note 1 and 2.
Updated Section 3.0.9: Flexible static memory controller (FSMC).
Modified I/Os used to reprogram the Flash memory for CAN2 and USB
OTG FS in Section 3.0.13: Boot modes.
Updated note in Section 3.0.14: Power supply schemes.
PDR_ON no more available on LQFP100 package. Updated Section
3.0.16: Voltage regulator. Updated condition to obtain a minimum
supply voltage of 1.7 V in the whole document.
Renamed USART4/5 to UART4/5 and added LIN and IrDA feature for
UART4 and UART5 in Table 6: USART feature comparison.
Removed support of I2C for OTG PHY in Section 3.0.30: Universal
serial bus on-the-go full-speed (OTG_FS).
Added Table 7: Legend/abbreviations used in the pinout table.
Table 8: STM32F40xxx pin and ball definitions: replaced VSS_3,
VSS_4, and VSS_8 by VSS; reformatted Table 8: STM32F40xxx pin
and ball definitions to better highlight I/O structure, and alternate
functions versus additional functions; signal corresponding to
LQFP100 pin 99 changed from PDR_ON to VSS; EVENTOUT added
in the list of alternate functions for all I/Os; ADC3_IN8 added as
alternate function for PF10; FSMC_CLE and FSMC_ALE added as
alternate functions for PD11 and PD12, respectively; PH10 alternate
function TIM15_CH1_ETR renamed TIM5_CH1; updated PA4 and PA5
I/O structure to TTa.
Removed OTG_HS_SCL, OTG_HS_SDA, OTG_FS_INTN in Table 8:
STM32F40xxx pin and ball definitions and Table 10: Alternate function
mapping.
Changed TCM data RAM to CCM data RAM in Figure 18:
STM32F40xxx memory map.
Added IVDD and IVSS maximum values in Table 14: Current
characteristics.
Added Note 1 related to fHCLK, updated Note 2 in Table 16: General
operating conditions, and added maximum power dissipation values.
Updated Table 17: Limitations depending on the operating power
supply range.

---

24-Jan-2012
(continued)
Added V12 in Table 21: Embedded reset and power control block
characteristics.
Updated Table 23: Typical and maximum current consumption in Run
mode, code with data processing   running from Flash memory (ART
accelerator disabled) and Table 22: Typical and maximum current
consumption in Run mode, code with data processing   running from
Flash memory (ART accelerator enabled) or RAM. Added Figure  ,
Figure 25, Figure 26, and Figure 27.
Updated Table 24: Typical and maximum current consumption in Sleep
mode and removed Note 1.
Updated Table 25: Typical and maximum current consumptions in Stop
mode and Table 26: Typical and maximum current consumptions in
Standby mode, Table 27: Typical and maximum current consumptions
in VBAT mode, and Table 29: Switching output I/O current
consumption.
Section : On-chip peripheral current consumption: modified conditions,
and updated Table 30: Peripheral current consumption and Note 2.
Changed fHSE_ext to 50 MHz and tr(HSE)/tf(HSE) maximum value in
Table 32: High-speed external user clock characteristics.
Added Cin(LSE) in Table 33: Low-speed external user clock
characteristics.
Updated maximum PLL input clock frequency, removed related note,
and deleted jitter for MCO for RMII Ethernet typical value in Table 38:
Main PLL characteristics. Updated maximum PLLI2S input clock
frequency and removed related note in Table 39: PLLI2S (audio PLL)
characteristics.
Updated Section : Flash memory to specify that the devices are
shipped to customers with the Flash memory erased. Updated Table
41: Flash memory characteristics, and added tME in Table 42: Flash
memory programming.
Updated Table 45: EMS characteristics, and Table 46: EMI
characteristics.
Updated Table 58: I2S dynamic characteristics
Updated Figure 45: ULPI timing diagram and Table 64: ULPI timing.
Added tCOUNTER and tMAX_COUNT in Table 54: Characteristics of
TIMx connected to the APB1 domain and Table 55: Characteristics of
TIMx connected to the APB2 domain. Updated Table 67: Dynamic
characteristics: Ethernet MAC signals for RMII.
Removed USB-IF certification in Section : USB OTG FS
characteristics.
Table 99. Document revision history (continued)
Date
Changes

---

24-Jan-2012
(continued)
Updated Table 63: USB HS clock timing parameters
Updated Table 69: ADC characteristics.
Updated Table 70: ADC accuracy at fADC = 30 MHz.
Updated Note 1 in Table 76: DAC characteristics.
Section 6.3.26: FSMC characteristics: updated Table 77 toTable 88,
changed CL value to 30 pF, and modified FSMC configuration for
asynchronous timings and waveforms. Updated Figure 59:
Synchronous multiplexed PSRAM write timings.
Updated Table 100: Package thermal characteristics.
Appendix A.1: USB OTG full speed (FS) interface solutions: modified
Figure 93: USB controller configured as peripheral-only and used in
Full speed mode added Note 2, updated Figure 94: USB controller
configured as host-only and used in full speed mode and added Note
2, changed Figure 95: USB controller configured in dual mode and
used in full speed mode and added Note 3.
Appendix A.2: USB OTG high speed (HS) interface solutions: removed
figures USB OTG HS device-only connection in FS mode and USB
OTG HS host-only connection in FS mode, and updated Figure 96:
USB controller configured as peripheral, host, or dual-mode and used
in high speed mode and added Note 2.
Added Appendix A.3: Ethernet interface solutions.
Table 99. Document revision history (continued)
Date
Changes

---

31-May-2012
Updated Figure 5: STM32F40xxx block diagram and Figure 7: Power
supply supervisor interconnection with internal reset OFF
Added SDIO, added notes related to FSMC and SPI/I2S in Table 3:
STM32F405xx and STM32F407xx: features and peripheral counts.
Starting from Silicon revision Z, USB OTG full-speed interface is now
available for all STM32F405xx devices.
Added full information on WLCSP90 package together with
corresponding part numbers.
Changed number of AHB buses to 3.
Modified available Flash memory sizes in Section 3.0.4: Embedded
Flash memory.
Modified number of maskable interrupt channels in Section 3.0.10:
Nested vectored interrupt controller (NVIC).
Updated case of Regulator ON/internal reset ON, Regulator
ON/internal reset OFF, and Regulator OFF/internal reset ON in Section
3.0.16: Voltage regulator.
Updated standby mode description in Section 3.0.19: Low-power
modes.
Added Note 1 below Figure 16: STM32F40xxx UFBGA176 ballout.
Added Note 1 below Figure 17: STM32F40xxx WLCSP90 ballout.
Updated Table 8: STM32F40xxx pin and ball definitions.
Added Table 9: FSMC pin definition.
Removed OTG_HS_INTN alternate function in Table 8: STM32F40xxx
pin and ball definitions and Table 10: Alternate function mapping.
Removed I2S2_WS on PB6/AF5 in Table 10: Alternate function
mapping.
Replaced JTRST by NJTRST, removed ETH_RMII _TX_CLK, and
modified I2S3ext_SD on PC11 in Table 10: Alternate function
mapping.
Added Table 12: register boundary addresses.
Updated Figure 18: STM32F40xxx memory map.
Updated VDDA and VREF+ decoupling capacitor in Figure 21: Power
supply scheme.
Added power dissipation maximum value for WLCSP90 in Table 16:
General operating conditions.
Updated VPOR/PDR in Table 21: Embedded reset and power control
block characteristics.
Updated notes in Table 23: Typical and maximum current consumption
in Run mode, code with data processing   running from Flash memory
(ART accelerator disabled), Table 22: Typical and maximum current
consumption in Run mode, code with data processing   running from
Flash memory (ART accelerator enabled) or RAM, and Table 24:
Typical and maximum current consumption in Sleep mode.
Updated maximum current consumption at TA = 25 °n Table 25:
Typical and maximum current consumptions in Stop mode.
Table 99. Document revision history (continued)
Date
Changes

---

31-May-2012
(continued)
Removed fHSE_ext typical value in Table 32: High-speed external user
clock characteristics. Updated Table 34: HSE 4-26 MHz oscillator
characteristics and Table 35: LSE oscillator characteristics (fLSE =
32.768 kHz).
Added fPLL48_OUT maximum value in Table 38: Main PLL
characteristics.
Modified equation 1 and 2 in Section 6.3.11: PLL spread spectrum
clock generation (SSCG) characteristics.
Updated Table 41: Flash memory characteristics, Table 42: Flash
memory programming, and Table 43: Flash memory programming with
VPP.
Updated Section : Output driving current.
Table 56: I2C characteristics: Note 4 updated and applied to th(SDA) in
Fast mode, and removed note 4 related to th(SDA) minimum value.
Updated Table 69: ADC characteristics. Updated note concerning ADC
accuracy vs. negative injection current below Table 70: ADC accuracy
at fADC = 30 MHz.
Added WLCSP90 thermal resistance in Table 100: Package thermal
characteristics.
Updated Table 92: WLCSP90 - 4.223 x 3.969 mm, 0.400 mm pitch
wafer level chip scale package mechanical data.
Updated Figure 87: UFBGA176+25 ball, 10 x 10 mm, 0.65 mm pitch,
ultra fine pitch ball grid array package outline and Table 97:
UFBGA176+25 ball, 10 × 10 × 0.65 mm pitch, ultra thin fine pitch ball
grid array mechanical data.
Added Figure 91: LQFP176 - 176-pin, 24 x 24 mm low profile quad flat
recommended footprint.
Removed 256 and 768 Kbyte Flash memory density from Table  : .
Table 99. Document revision history (continued)
Date
Changes

---

04-Jun-2013
Modified Note 1 below Table 3: STM32F405xx and STM32F407xx:
features and peripheral counts.
Updated Figure 4 title.
Updated Note 3 below Figure 21: Power supply scheme.
Changed simplex mode into half-duplex mode in Section 3.0.25: Inter-
integrated sound (I2S).
Replaced DAC1_OUT and DAC2_OUT by DAC_OUT1 and
DAC_OUT2, respectively.
Updated pin 36 signal in Figure 15: STM32F40xxx LQFP176 pinout.
Changed pin number from F8 to D4 for PA13 pin in Table 8:
STM32F40xxx pin and ball definitions.
Replaced TIM2_CH1/TIM2_ETR by TIM2_CH1_ETR for PA0 and PA5
pins in Table 10: Alternate function mapping.
Changed system memory into System memory + OTP in Figure 18:
STM32F40xxx memory map.
Added Note 1 below Table 18: VCAP_1/VCAP_2 operating conditions.
Updated IDDA description in Table 76: DAC characteristics.
Removed PA9/PB13 connection to VBUS in Figure 93: USB controller
configured as peripheral-only and used in Full speed mode and Figure
94: USB controller configured as host-only and used in full speed
mode.
Updated SPI throughput on front page and Section 3.0.24: Serial
peripheral interface (SPI)
Updated operating voltages in Table 3: STM32F405xx and
STM32F407xx: features and peripheral counts.
Updated note in Section 3.0.14: Power supply schemes
Updated Section 3.0.15: Power supply supervisor
Updated “Regulator ON” paragraph in Section 3.0.16: Voltage
regulator
Removed note in Section 3.0.19: Low-power modes
Corrected wrong reference manual in Section 3.0.28: Ethernet MAC
interface with dedicated DMA and IEEE 1588 support
Updated Table 17: Limitations depending on the operating power
supply range
Updated Table 26: Typical and maximum current consumptions in
Standby mode
Updated Table 27: Typical and maximum current consumptions in
VBAT mode
Updated Table 39: PLLI2S (audio PLL) characteristics
Updated Table 46: EMI characteristics
Updated Table 51: Output voltage characteristics
Updated Table 53: NRST pin characteristics
Updated Table 57: SPI dynamic characteristics
Updated Table 58: I2S dynamic characteristics
Deleted Table 59
Updated Table 64: ULPI timing
Updated Figure 46: Ethernet SMI timing diagram
Table 99. Document revision history (continued)
Date
Changes

---

04-Jun-2013
(continued)
Updated Figure 87: UFBGA176+25 ball, 10 x 10 mm, 0.65 mm pitch,
ultra fine pitch ball grid array package outline
Updated Table 97: UFBGA176+25 ball, 10 × 10 × 0.65 mm pitch, ultra
thin fine pitch ball grid array mechanical data
Updated Figure 5: STM32F40xxx block diagram
Updated Section 2: Description
Updated footnote (3) in Table 3: STM32F405xx and STM32F407xx:
features and peripheral counts.
Updated Figure 3: Compatible board design between
STM32F10xx/STM32F2/STM32F40xxx for LQFP144 package
Updated Figure 4: Compatible board design between STM32F2 and
STM32F40xxx  for LQFP176 and BGA176 packages
Updated Section 3.0.14: Power supply schemes
Updated Section 3.0.15: Power supply supervisor
Updated Section 3.0.16: Voltage regulator, including figures.
Updated Table 16: General operating conditions, including footnote (2).
Updated Table 17: Limitations depending on the operating power
supply range, including footnote (3).
Updated footnote (1) in Table 69: ADC characteristics.
Updated footnote (2) in Table 70: ADC accuracy at fADC = 30 MHz.
Updated footnote (1) in Table 76: DAC characteristics.
Updated Figure 9: Regulator OFF.
Updated Figure 7: Power supply supervisor interconnection with
internal reset OFF.
Added Section 3.0.17: Regulator ON/OFF and internal reset ON/OFF
availability.
Updated footnote (2) of Figure 21: Power supply scheme.
Replaced respectively “I2S3S_WS" by "I2S3_WS”, “I2S3S_CK” by
“I2S3_CK” and “FSMC_BLN1” by “FSMC_NBL1” in Table 10: Alternate
function mapping.
Added “EVENTOUT” as alternate function “AF15” for pin PC13, PC14,
PC15, PH0, PH1, PI8 in Table 10: Alternate function mapping
Replaced “DCMI_12” by “DCMI_D12” in Table 8: STM32F40xxx pin
and ball definitions.
Removed the following sentence from Section : I2C interface
characteristics: ”Unless otherwise specified, the parameters
given in Table 56 are derived from tests performed under the
ambient temperature, fPCLK1 frequency and VDD supply
voltage conditions summarized in Table 16.”.
In Table 8: STM32F40xxx pin and ball definitions on page 53:
– For pin PC13, replaced “RTC_AF1” by “RTC_OUT, RTC_TAMP1,
RTC_TS”
– for pin PI8, replaced “RTC_AF2” by “RTC_TAMP1, RTC_TAMP2,
RTC_TS”.
– for pin PB15, added RTC_REFIN in Alternate functions column.
In Table 10: Alternate function mapping on page 70, for port
PB15, replaced “RTC_50Hz” by “RTC_REFIN”.
Table 99. Document revision history (continued)
Date
Changes

---

04-Jun-2013
(continued)
Updated Figure 6: Multi-AHB matrix.
Updated Figure 7: Power supply supervisor interconnection with
internal reset OFF
Changed 1.2 V to V12 in Section : Regulator OFF
Updated LQFP176 pin 48.
Updated Section 1: Introduction.
Updated Section 2: Description.
Updated operating voltage in Table 3: STM32F405xx and
STM32F407xx: features and peripheral counts.
Updated Note 1.
Updated Section 3.0.15: Power supply supervisor.
Updated Section 3.0.16: Voltage regulator.
Updated Figure 9: Regulator OFF.
Updated Table 4: Regulator ON/OFF and internal reset ON/OFF
availability.
Updated Section 3.0.19: Low-power modes.
Updated Section 3.0.20: VBAT operation.
Updated Section 3.0.22: Inter-integrated circuit interface (I²C) .
Updated pin 48 in Figure 15: STM32F40xxx LQFP176 pinout.
Updated Table 7: Legend/abbreviations used in the pinout table.
Updated Table 8: STM32F40xxx pin and ball definitions.
Updated Table 16: General operating conditions.
Updated Table 17: Limitations depending on the operating power
supply range.
Updated Section 6.3.7: Wakeup time from low-power mode.
Updated Table 36: HSI oscillator characteristics.
Updated Section 6.3.15: I/O current injection characteristics.
Updated Table 50: I/O static characteristics.
Updated Table 53: NRST pin characteristics.
Updated Table 56: I2C characteristics.
Updated Figure 39: I2C bus AC waveforms and measurement circuit.
Updated Section 6.3.19: Communications interfaces.
Updated Table 69: ADC characteristics.
Added Table 72: Temperature sensor calibration values.
Added Table 75: Internal reference voltage calibration values.
Updated Section 6.3.26: FSMC characteristics.
Updated Section 6.3.28: SD/SDIO MMC card host interface (SDIO)
characteristics.
Updated Table 25: Typical and maximum current consumptions in Stop
mode.
Updated Section : SPI interface characteristics included Table 57.
Updated Section : I2S interface characteristics included Table 58.
Updated Table 66: Dynamic characteristics: Ethernet MAC signals for
SMI.
Updated Table 68: Dynamic characteristics: Ethernet MAC signals for
MII.
Table 99. Document revision history (continued)
Date
Changes

---

04-Jun-2013
(continued)
Updated Table 66: Dynamic characteristics: Ethernet MAC signals for
SMI.
Updated Table 68: Dynamic characteristics: Ethernet MAC signals for
MII.
Updated Table 81: Synchronous multiplexed NOR/PSRAM read
timings.
Updated Table 82: Synchronous multiplexed PSRAM write timings.
Updated Table 83: Synchronous non-multiplexed NOR/PSRAM read
timings.
Updated Table 84: Synchronous non-multiplexed PSRAM write
timings.
Updated Section 6.3.27: Camera interface (DCMI) timing specifications
including Table 89: DCMI characteristics and addition of Figure 72:
DCMI timing diagram.
Updated Section 6.3.28: SD/SDIO MMC card host interface (SDIO)
characteristics including Table 90.
Updated Chapter Figure 9.
Table 99. Document revision history (continued)
Date
Changes

---

06-Mar-2015
Replace Cortex-M4F by Cortex-M4 with FPU throughout the
document.
Updated Section : Regulator OFF and Table 4: Regulator ON/OFF and
internal reset ON/OFF availability for LQFP176.
Updated Figure 15: STM32F40xxx LQFP176 pinout and Table 8:
STM32F40xxx pin and ball definitions.
Updated Figure 6: Multi-AHB matrix.
Added note 1 below Figure 12: STM32F40xxx LQFP64 pinout, Figure
13: STM32F40xxx LQFP100 pinout, Figure 14: STM32F40xxx
LQFP144 pinout and Figure 15: STM32F40xxx LQFP176 pinout.
Updated IVDD and IVSS in Table 14: Current characteristics.
Updated PLS[2:0]=101 (falling edge) configuration in Table 21:
Embedded reset and power control block characteristics.
Added Section : Additional current consumption. Updated Section :
On-chip peripheral current consumption.
Updated Table 31: Low-power mode wakeup timings.
Updated Table 34: HSE 4-26 MHz oscillator characteristics and Table
35: LSE oscillator characteristics (fLSE = 32.768 kHz).
Changed condition related to VESD(CDM) in Table 47: ESD absolute
maximum ratings.
Updated Table 49: I/O current injection susceptibility, Table 50: I/O
static characteristics, Table 51: Output voltage characteristics
conditions, Table 52: I/O AC characteristics and Figure 37: I/O AC
characteristics definition.
Updated Section : I2C interface characteristics.
Remove note 3 in Table 71: Temperature sensor characteristics.
Updated Figure 72: DCMI timing diagram.
Modified Figure 75: WLCSP90 - 4.223 x 3.969 mm, 0.400 mm pitch
wafer level chip scale package outline and Table 92: WLCSP90 - 4.223
x 3.969 mm, 0.400 mm pitch wafer level chip scale package
mechanical data. Added Figure 76: WLCSP90 - 4.223 x 3.969 mm,
0.400 mm pitch wafer level chip scale recommended footprint and
Table 93: WLCSP90 recommended PCB design rules. /
Modified Figure 78: LQFP64 – 64-pin, 10 x 10 mm low-profile quad flat
package outline and Table 94: LQFP64 – 64-pin 10 x 10 mm low-profile
quad flat package  mechanical data.
Updated Figure 87: UFBGA176+25 ball, 10 x 10 mm, 0.65 mm pitch,
ultra fine pitch ball grid array package outline and Table 97:
UFBGA176+25 ball, 10 × 10 × 0.65 mm pitch, ultra thin fine pitch ball
grid array mechanical data. Added Figure 88: UFBGA176+25 - 201-
ball, 10 x 10 mm, 0.65 mm pitch, ultra fine pitch ball grid array
recommended footprint and Table 98: UFBGA176+25 recommended
PCB design rules (0.65 mm pitch BGA).
Updated Figure 90: LQFP176 - 176-pin, 24 x 24 mm low profile quad
flat package outline.
Added Section : Device marking for WLCSP90, Section : Device
marking for LQFP64, Section : Device marking for LFP100, Section :
Device marking for LQFP144, Section : Device marking for
UFBGA176+25 and Section : Device marking for LQFP176.
Table 99. Document revision history (continued)
Date
Changes

---

22-Oct-2015
In the whole document, updated notes related to values specified by
design or by characterization.
Updated Table 36: HSI oscillator characteristics.
Changed fVCO_OUT minimum value and VCO freq to 100 MHz in
Table 38: Main PLL characteristics and Table 39: PLLI2S (audio PLL)
characteristics.
Updated Figure 39: SPI timing diagram - slave mode and CPHA = 0.
Updated Figure 53: 12-bit buffered /non-buffered DAC.
Removed note 1 related to better performance using a restricted VDD
range in Table 70: ADC accuracy at fADC = 30 MHz.
Upated Figure 84: LQFP144 - 144-pin, 20 x 20 mm low-profile quad flat
package outline.
Updated Figure 87: UFBGA176+25 ball, 10 x 10 mm, 0.65 mm pitch,
ultra fine pitch ball grid array package outline and Table 97:
UFBGA176+25 ball, 10 × 10 × 0.65 mm pitch, ultra thin fine pitch ball
grid array mechanical data.
16-Mar-2016
Updated Figure 2: Compatible board design
STM32F10xx/STM32F2/STM32F40xxx for LQFP100 package.
Updated |VSSX- VSS| in Table 13: Voltage characteristics to add
VREF-.
Added VREF- in Table 69: ADC characteristics.
Updated Table 92: WLCSP90 - 4.223 x 3.969 mm, 0.400 mm pitch
wafer level chip scale package mechanical data.
09-Sep-2016
Removed note 1 below Figure 5: STM32F40xxx block diagram.
Updated definition of stresses above maximum ratings in Section 6.2:
Absolute maximum ratings.
Updated th(NSS) in Figure 39: SPI timing diagram - slave mode and
CPHA = 0 and Figure 40: SPI timing diagram - slave mode and CPHA
= 1.
Added note related to optional marking and inset/upset marks in all
package marking sections.
Updated Figure 87: UFBGA176+25 ball, 10 x 10 mm, 0.65 mm pitch,
ultra fine pitch ball grid array package outline and Table 97:
UFBGA176+25 ball, 10 × 10 × 0.65 mm pitch, ultra thin fine pitch ball
grid array mechanical data.
Table 99. Document revision history (continued)
Date
Changes

---

14-Aug-2020
Renamed Section 3 and Section 8 into Functional overview and
Ordering information, respectively.
Added Arm logo and legal notice in Section 1: Introduction and
updated Arm wordmark in the whole document. Removed USB
certified logos.
Updated camera interfaces for STM32F405OE in Table 3:
STM32F405xx and STM32F407xx: features and peripheral counts.
Added OTP memory in Features and Section 3.0.4: Embedded Flash
memory.
Changed random number generator to true random number generator
in the whole document and updated Section 3.0.34: True random
number generator (RNG).
Added Note 1 related to UFBGA176 in Table 8: STM32F40xxx pin and
ball definitions.
Updated Section 6.2: Absolute maximum ratings introduction.
Updated VPVD minimum value for PLS[2:0]=101 (falling edge) in Table
21: Embedded reset and power control block characteristics.
Updated Note 1 in Table 36: HSI oscillator characteristics.
Added reference to application note AN4899 in Section 6.3.16: I/O port
characteristics.
Replaced DCMI_PIXCK by DCMI_PIXCLK in Table 10: Alternate
function mapping.
Renamed Section 8 into Ordering information.
Updated D1 in Figure 87: UFBGA176+25 ball, 10 x 10 mm, 0.65 mm
pitch, ultra fine pitch ball grid array package outline.
Table 99. Document revision history (continued)
Date
Changes

---

08-Nov-2024
Updated:
– Cover page
– Section 1: Introduction
– Figure 5: STM32F40xxx block diagram
– Section 3.0.18: Real-time clock (RTC), backup SRAM and backup
registers
– Table 7: STM32F40xxx pin and ball definitions
– Table 9: Alternate function mapping
– I/O system current consumption
– Note 1 in Table 34: HSI oscillator characteristics.
– Table 33: LSE oscillator characteristics (fLSE = 32.768 kHz)
– Figure 37: I/O AC characteristics definition
– Figure 39: SPI timing diagram - slave mode and CPHA = 0,
Figure 40: SPI timing diagram - slave mode and CPHA = 1, and
Figure 41: SPI timing diagram - master mode
– Table 44: EMI characteristics for fHSE = 25 MH and fCPU =
168 MHz
– Figure 49: ADC accuracy characteristics and Figure 50: Typical
connection diagram when using the ADC with FT/TT pins featuring
analog switch function
– Figure 68: NAND controller waveforms for read access and
Figure 69: NAND controller waveforms for write access
– Table 96: UFBGA(176+25) - Example of PCB design rules (0.65 mm
pitch BGA) title
– Section 7: Package information
Added Section 9: Important security notice
04-Feb-2026
Updated Figure 7: Power supply supervisor interconnection with
internal reset OFF.
Updated Figure 8: PDR_ON and NRST control with internal reset OFF.
Updated Figure 9: Regulator OFF.
Updated Figure 18: STM32F40xxx memory map.
Updated Figure 20: Pin input voltage.
Updated Table 45: ESD absolute maximum ratings.
Updated Section 7.3: LQFP64 package information (5W).
Table 99. Document revision history (continued)
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
