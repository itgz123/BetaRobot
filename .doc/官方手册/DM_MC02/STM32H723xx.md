# Arm® Cortex®-M7 32-bit 550 MHz MCU, up to 1 MB flash, 564 KB


---

May 2025
STM32H723VE STM32H723VG
STM32H723ZE STM32H723ZG
Arm® Cortex®-M7 32-bit 550 MHz MCU, up to 1 MB flash, 564 KB
RAM, Ethernet, USB, 3x FDCAN, Graphics, 2x 16-bit ADCs
Datasheet - production data
Features
Includes ST state-of-the-art patented
technology
Core
•
32-bit Arm® Cortex®-M7 CPU with DP-FPU, L1
cache: 32-Kbyte data cache and 32-Kbyte
instruction cache allowing 0-wait state
execution from embedded flash memory and
external memories, frequency up to 550 MHz,
MPU, 2778 CoreMark, and DSP instructions
Memories
•
Up to 1 Mbyte of embedded flash memory with
ECC
•
SRAM: total 564 Kbytes all with ECC, including
128 Kbytes of data TCM RAM for critical real-
time data + 432 Kbytes of system RAM (up to
256 Kbytes can remap on instruction TCM
RAM for critical real-time instructions) +
4 Kbytes of backup SRAM (available in the
lowest-power modes)
•
Flexible external memory controller with up to
16-bit data bus: SRAM, PSRAM,
SDRAM/LPSDR SDRAM, NOR/NAND
memories
•
2 x Octo-SPI interface with XiP
•
2 x SD/SDIO/MMC interface
•
Bootloader
Graphics
•
Chrom-ART Accelerator graphical hardware
accelerator enabling enhanced graphical user
interface to reduce CPU load
•
LCD-TFT controller supporting up to XGA
resolution
Clock, reset, and supply management
•
1.62 V to 3.6 V application supply and I/O
•
POR, PDR, PVD, and BOR
•
Dedicated USB power
•
Embedded LDO regulator
•
Internal oscillators: 64 MHz HSI, 48 MHz
HSI48, 4 MHz CSI, 32 kHz LSI
•
External oscillators: 4-50 MHz HSE,
32.768 kHz LSE
Low power
•
Sleep, Stop, and Standby modes
•
VBAT supply for RTC, 32×32-bit backup
registers
Analog
•
2×16-bit ADC, up to 3.6 MSPS in 16-bit: up to
18 channels and 7.2 MSPS in double-
interleaved mode
•
1 x 12-bit ADC, up to 5 MSPS in 12-bit, up to 12
channels
•
2 x comparators
•
2 x operational amplifier GBW = 8 MHz
•
2× 12-bit D/A converters
FBGA
FBGA
LQFP100
(14x14 mm)
LQFP144
(20x20 mm)
UFBGA144
(7x7 mm)
TFBGA100
(8x8 mm)
www.st.com

---

Digital filters for sigma delta modulator
(DFSDM)
•
8 channels/4 filters
4 DMA controllers to offload the CPU
•
1 × MDMA with linked list support
•
2 × dual-port DMAs with FIFO
•
1 × basic DMA with request router capabilities
24 timers
•
Seventeen 16-bit (including 5 x low power
16-bit timer available in stop mode) and four
32-bit timers, each with up to 4 IC/OC/PWM or
pulse counter and quadrature (incremental)
encoder input
•
2x watchdogs, 1x SysTick timer
Debug mode
•
SWD and JTAG interfaces
•
2-Kbyte embedded trace buffer
Up to 114 I/O ports with interrupt
capability
Up to 35 communication interfaces
•
Up to 5 × I2C Fm+ interfaces
(SMBus/PMBus™)
•
Up to 6 USART/UART/LPUART (SPI, ISO7816
interface, LIN, IrDA, modem)
•
Up to 6 SPIs (+ up to 5 with USART + 2 with
OCTOSPI), 4 with muxed duplex I2S for audio
class accuracy via internal audio PLL or
external clock and up to 5 x SPI (from 5 x
USART when configured in synchronous
mode)
•
2x SAI (serial audio interface)
•
1× FD/TTCAN and 2x FDCANs
•
8- to 14-bit camera interface
•
16-bit parallel slave synchronous interface
•
SPDIF-IN interface
•
HDMI-CEC
•
Ethernet MAC interface with DMA controller
•
USB 2.0 high-speed/full-speed
device/host/OTG controller with dedicated
DMA, on-chip FS PHY and ULPI for external
HS PHY
•
SWPMI single-wire protocol master I/F
•
MDIO slave interface
Mathematical acceleration
•
CORDIC for trigonometric functions
acceleration
•
FMAC: Filter mathematical accelerator
Digital temperature sensor
True random number generator
CRC calculation unit
RTC with subsecond accuracy and
hardware calendar
ROP, PC-ROP, tamper detection
96-bit unique ID
All packages are ECOPACK2 compliant

---

Contents
Contents
Introduction . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 13
Description . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 14
Functional overview  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 20
3.1
Arm® Cortex®-M7 with FPU . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 20
3.2
Memory protection unit (MPU) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 20
3.3
Memories  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 21
3.3.1
Embedded flash memory . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 21
3.3.2
Embedded SRAM  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 21
Error code correction (ECC) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .22
3.4
Boot modes  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 22
3.5
CORDIC coprocessor (CORDIC) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 22
CORDIC features . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .23
3.6
Filter mathematical accelerator (FMAC) . . . . . . . . . . . . . . . . . . . . . . . . . . 23
FMAC features . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .23
3.7
Power supply management  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 24
3.7.1
Power supply scheme  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 24
3.7.2
Power supply supervisor  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 24
3.7.3
Voltage regulator  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26
3.8
Low-power strategy  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26
3.9
Reset and clock controller (RCC) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 27
3.9.1
Clock management  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 27
3.9.2
System reset sources  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 28
3.10
General-purpose input/outputs (GPIOs) . . . . . . . . . . . . . . . . . . . . . . . . . . 28
3.11
Bus-interconnect matrix  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 28
3.12
DMA controllers  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 30
3.13
Chrom-ART Accelerator (DMA2D) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 30
3.14
Nested vectored interrupt controller (NVIC) . . . . . . . . . . . . . . . . . . . . . . . 31
3.15
Extended interrupt and event controller (EXTI)  . . . . . . . . . . . . . . . . . . . . 31
3.16
Cyclic redundancy check calculation unit (CRC)  . . . . . . . . . . . . . . . . . . . 31
3.17
Flexible memory controller (FMC)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 32
3.18
Octo-SPI memory interface (OCTOSPI)  . . . . . . . . . . . . . . . . . . . . . . . . . 32

---

Contents
3.19
Analog-to-digital converters (ADCs) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 33
3.20
Temperature sensor . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 33
3.21
Digital temperature sensor (DTS) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 33
3.22
VBAT operation  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 34
3.23
Digital-to-analog converters (DAC)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 34
3.24
Ultra-low-power comparators (COMP) . . . . . . . . . . . . . . . . . . . . . . . . . . . 35
3.25
Operational amplifiers (OPAMP)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 35
3.26
Digital filter for sigma-delta modulators (DFSDM)  . . . . . . . . . . . . . . . . . . 36
3.27
Digital camera interface (DCMI) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 38
3.28
PSSI . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 38
3.29
LCD-TFT controller . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 38
3.30
True random number generator (RNG)  . . . . . . . . . . . . . . . . . . . . . . . . . . 39
3.31
Timers and watchdogs . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 40
3.31.1
Advanced-control timers (TIM1, TIM8)  . . . . . . . . . . . . . . . . . . . . . . . . . 42
3.31.2
General-purpose timers (TIMx)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 42
3.31.3
Basic timers TIM6 and TIM7  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 43
3.31.4
Low-power timers (LPTIM1, LPTIM2, LPTIM3, LPTIM4, LPTIM5)  . . . . 43
3.31.5
Independent watchdog  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 43
3.31.6
Window watchdog  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 43
3.31.7
SysTick timer . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 43
3.32
Real-time clock (RTC), backup SRAM and backup registers . . . . . . . . . . 44
3.33
Inter-integrated circuit interface (I2C) . . . . . . . . . . . . . . . . . . . . . . . . . . . . 45
3.34
Universal synchronous/asynchronous receiver transmitter (USART)  . . . 45
3.35
Low-power universal asynchronous receiver transmitter (LPUART)  . . . . 46
3.36
Serial peripheral interface (SPI)/inter- integrated sound interfaces (I2S) . 47
3.37
Serial audio interfaces (SAI) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 47
3.38
SPDIFRX Receiver Interface (SPDIFRX) . . . . . . . . . . . . . . . . . . . . . . . . . 48
3.39
Single wire protocol master interface (SWPMI)  . . . . . . . . . . . . . . . . . . . . 48
3.40
Management data input/output (MDIO) slaves . . . . . . . . . . . . . . . . . . . . . 49
3.41
SD/SDIO/MMC card host interfaces (SDMMC)  . . . . . . . . . . . . . . . . . . . . 49
3.42
Controller area network (FDCAN1, FDCAN2, FDCAN3) . . . . . . . . . . . . . 49
3.43
Universal serial bus on-the-go high-speed (OTG_HS) . . . . . . . . . . . . . . . 50
3.44
Ethernet MAC interface with dedicated DMA controller (ETH) . . . . . . . . . 50

---

Contents
3.45
High-definition multimedia interface (HDMI)
- consumer electronics control (CEC)  . . . . . . . . . . . . . . . . . . . . . . . . . . . 51
3.46
Debug infrastructure . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 51
Memory mapping . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 52
Pinouts, pin descriptions and alternate functions . . . . . . . . . . . . . . . . 53
Electrical characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 86
6.1
Parameter conditions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 86
6.1.1
Minimum and maximum values . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 86
6.1.2
Typical values  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 86
6.1.3
Typical curves  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 86
6.1.4
Loading capacitor  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 86
6.1.5
Pin input voltage  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 86
6.1.6
Power supply scheme  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 87
6.1.7
Current consumption measurement  . . . . . . . . . . . . . . . . . . . . . . . . . . . 88
6.2
Absolute maximum ratings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 88
6.3
Operating conditions  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 90
6.3.1
General operating conditions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 90
6.3.2
VCAP external capacitor  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 92
6.3.3
Operating conditions at power-up / power-down . . . . . . . . . . . . . . . . . . 93
6.3.4
Embedded reset and power control block characteristics . . . . . . . . . . . 94
6.3.5
Embedded reference voltage characteristics . . . . . . . . . . . . . . . . . . . . . 95
6.3.6
Embedded USB regulator characteristics  . . . . . . . . . . . . . . . . . . . . . . . 96
6.3.7
Supply current characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 96
Typical and maximum current consumption . . . . . . . . . . . . . . . . . . . . . . . . . . . . .97
I/O system current consumption. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .102
On-chip peripheral current consumption  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .104
6.3.8
Wake-up time from low-power modes . . . . . . . . . . . . . . . . . . . . . . . . . 110
6.3.9
External clock source characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . 111
High-speed external user clock generated from an external source  . . . . . . . . .111
Low-speed external user clock generated from an external source . . . . . . . . . .112
High-speed external clock generated from a crystal/ceramic resonator. . . . . . .113
Low-speed external clock generated from a crystal/ceramic resonator . . . . . . .114
6.3.10
Internal clock source characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . 115
48 MHz high-speed internal RC oscillator (HSI48). . . . . . . . . . . . . . . . . . . . . . .115
64 MHz high-speed internal RC oscillator (HSI). . . . . . . . . . . . . . . . . . . . . . . . .116

---

Contents
4 MHz low-power internal RC oscillator (CSI)  . . . . . . . . . . . . . . . . . . . . . . . . . .117
Low-speed internal (LSI) RC oscillator . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .117
6.3.11
PLL characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 118
6.3.12
Memory characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 122
Flash memory. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .122
6.3.13
EMC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 123
Functional EMS (electromagnetic susceptibility)  . . . . . . . . . . . . . . . . . . . . . . . .123
Designing hardened software to avoid noise problems . . . . . . . . . . . . . . . . . . .123
Electromagnetic Interference (EMI) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .124
6.3.14
Absolute maximum ratings (electrical sensitivity)  . . . . . . . . . . . . . . . . 124
Electrostatic discharge (ESD). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .124
Static latchup  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .125
6.3.15
I/O current injection characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . 125
Functional susceptibility to I/O current injection . . . . . . . . . . . . . . . . . . . . . . . . .125
6.3.16
I/O port characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 126
General input/output characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .126
Output driving current . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .128
Output voltage levels  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .129
Output buffer timing characteristics (HSLV option disabled)  . . . . . . . . . . . . . . .131
Output buffer timing characteristics (HSLV option enabled). . . . . . . . . . . . . . . .133
Analog switch between ports Pxy_C and Pxy  . . . . . . . . . . . . . . . . . . . . . . . . . .134
6.3.17
NRST pin characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 134
6.3.18
FMC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 135
Asynchronous waveforms and timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .135
Synchronous waveforms and timings. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .143
NAND controller waveforms and timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .152
SDRAM waveforms and timings. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .154
6.3.19
Octo-SPI interface characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . 157
6.3.20
Delay block (DLYB) characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . 162
6.3.21
16-bit ADC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 162
General PCB design guidelines . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .170
6.3.22
12-bit ADC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 171
6.3.23
DAC characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 177
6.3.24
Voltage reference buffer characteristics  . . . . . . . . . . . . . . . . . . . . . . . 181
6.3.25
Analog temperature sensor characteristics . . . . . . . . . . . . . . . . . . . . . 182
6.3.26
Digital temperature sensor characteristics . . . . . . . . . . . . . . . . . . . . . . 183
6.3.27
Temperature and VBAT monitoring . . . . . . . . . . . . . . . . . . . . . . . . . . . . 183
6.3.28
Voltage booster for analog switch  . . . . . . . . . . . . . . . . . . . . . . . . . . . . 184
6.3.29
Comparator characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 184

---

Contents
6.3.30
Operational amplifier characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . 185
6.3.31
Digital filter for Sigma-Delta Modulators (DFSDM) characteristics  . . . 188
6.3.32
Camera interface (DCMI) timing specifications . . . . . . . . . . . . . . . . . . 190
6.3.33
Parallel synchronous slave interface (PSSI) characteristics  . . . . . . . . 191
6.3.34
LCD-TFT controller (LTDC) characteristics  . . . . . . . . . . . . . . . . . . . . . 192
6.3.35
Timer characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 194
6.3.36
Low-power timer characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 194
6.3.37
Communication interfaces . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 195
I2C interface characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .195
USART interface characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .196
SPI interface characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .198
I2S Interface characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .201
SAI characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .203
MDIO characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .205
SD/SDIO MMC card host interface (SDMMC) characteristics . . . . . . . . . . . . . .206
USB OTG_FS characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .208
USB OTG_HS characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .209
Ethernet interface characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .210
JTAG/SWD interface characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .212
Package information . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 215
7.1
Device marking . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 215
7.2
LQFP100 package information (1L) . . . . . . . . . . . . . . . . . . . . . . . . . . . . 216
Notes: . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .218
7.3
TFBGA100 package information (A08Q) . . . . . . . . . . . . . . . . . . . . . . . . 219
Notes: . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .220
7.4
LQFP144 package information (1A) . . . . . . . . . . . . . . . . . . . . . . . . . . . . 222
Notes: . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .224
7.5
UFBGA144 package information (A0AS) . . . . . . . . . . . . . . . . . . . . . . . . 226
7.6
Thermal characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 228
7.6.1
Reference documents  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 229
Ordering information  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 230
Important security notice . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 231

---

List of tables
List of tables
Table 1.
STM32H723xE/G features and peripheral counts  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 17
Table 2.
System versus domain low-power mode  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 27
Table 3.
DFSDM implementation . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 37
Table 4.
Timer feature comparison. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 40
Table 5.
USART features . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 46
Table 6.
Legend/abbreviations used in the pinout table . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 56
Table 7.
STM32H723 pin and ball descriptions  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 57
Table 8.
STM32H723 pin alternate functions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 72
Table 9.
Voltage characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 88
Table 10.
Current characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 89
Table 11.
Thermal characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 89
Table 12.
General operating conditions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 90
Table 13.
Supply voltage and maximum temperature configuration. . . . . . . . . . . . . . . . . . . . . . . . . . 92
Table 14.
VCAP operating conditions. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 92
Table 15.
Operating conditions at power-up/power-down  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 93
Table 16.
Reset and power control block characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 94
Table 17.
Embedded reference voltage . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 95
Table 18.
Internal reference voltage calibration values  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 96
Table 19.
USB regulator characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 96
Table 20.
Typical and maximum current consumption in Run mode,
 code with data processing running from ITCM  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 98
Table 21.
Typical and maximum current consumption in Run mode, code with data processing
 running from flash memory, cache ON  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 99
Table 22.
Typical and maximum current consumption in Run mode,
 code with data processing running from flash memory, cache OFF . . . . . . . . . . . . . . . . 100
Table 23.
Typical consumption in Run mode and corresponding performance
versus code position . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 101
Table 24.
Typical current consumption in Autonomous mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 101
Table 25.
Typical and maximum current consumption in Sleep mode . . . . . . . . . . . . . . . . . . . . . . . 101
Table 26.
Typical and maximum current consumption in Stop mode . . . . . . . . . . . . . . . . . . . . . . . . 102
Table 27.
Typical and maximum current consumption in Standby mode . . . . . . . . . . . . . . . . . . . . . 102
Table 28.
Typical and maximum current consumption in VBAT mode . . . . . . . . . . . . . . . . . . . . . . . 102
Table 29.
Peripheral current consumption in Run mode  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 104
Table 30.
Low-power mode wakeup timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 110
Table 31.
High-speed external user clock characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 111
Table 32.
Low-speed external user clock characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 112
Table 33.
4-50 MHz HSE oscillator characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 113
Table 34.
Low-speed external user clock characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 114
Table 35.
HSI48 oscillator characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 115
Table 36.
HSI oscillator characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 116
Table 37.
CSI oscillator characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 117
Table 38.
LSI oscillator characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 117
Table 39.
PLL1 characteristics (wide VCO frequency range). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 118
Table 40.
PLL1 characteristics (medium VCO frequency range) . . . . . . . . . . . . . . . . . . . . . . . . . . . 119
Table 41.
 PLL2 and PLL3 characteristics (wide VCO frequency range) . . . . . . . . . . . . . . . . . . . . . 120
Table 42.
PLL2 and PLL3 characteristics (medium VCO frequency range) . . . . . . . . . . . . . . . . . . . 121
Table 43.
Flash memory characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 122
Table 44.
Flash memory programming. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 122

---

List of tables
Table 45.
Flash memory endurance and data retention . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 122
Table 46.
EMS characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 123
Table 47.
EMI characteristics for fHSE =  8 MHz and fCPU = 550 MHz  . . . . . . . . . . . . . . . . . . . . . 124
Table 48.
ESD absolute maximum ratings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 124
Table 49.
Electrical sensitivities . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 125
Table 50.
I/O current injection susceptibility . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 125
Table 51.
I/O static characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 126
Table 52.
Output voltage characteristics for all I/Os except PC13, PC14 and PC15 . . . . . . . . . . . . 129
Table 53.
Output voltage characteristics for PC13, PC14 and PC15 . . . . . . . . . . . . . . . . . . . . . . . . 130
Table 54.
Output timing characteristics (HSLV OFF) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 131
Table 55.
Output timing characteristics (HSLV ON) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 133
Table 56.
Pxy_C and Pxy analog switch characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 134
Table 57.
NRST pin characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 134
Table 58.
Asynchronous non-multiplexed SRAM/PSRAM/NOR read timings . . . . . . . . . . . . . . . . . 136
Table 59.
Asynchronous non-multiplexed SRAM/PSRAM/NOR read-NWAIT timings . . . . . . . . . . . 136
Table 60.
Asynchronous non-multiplexed SRAM/PSRAM/NOR write timings . . . . . . . . . . . . . . . . . 138
Table 61.
Asynchronous non-multiplexed SRAM/PSRAM/NOR write-NWAIT timings. . . . . . . . . . . 138
Table 62.
Asynchronous multiplexed PSRAM/NOR read timings. . . . . . . . . . . . . . . . . . . . . . . . . . . 140
Table 63.
Asynchronous multiplexed PSRAM/NOR read-NWAIT timings . . . . . . . . . . . . . . . . . . . . 140
Table 64.
Asynchronous multiplexed PSRAM/NOR write timings  . . . . . . . . . . . . . . . . . . . . . . . . . . 142
Table 65.
Asynchronous multiplexed PSRAM/NOR write-NWAIT timings . . . . . . . . . . . . . . . . . . . . 142
Table 66.
Synchronous non-multiplexed NOR/PSRAM read timings . . . . . . . . . . . . . . . . . . . . . . . . 144
Table 67.
Synchronous non-multiplexed PSRAM write timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . 146
Table 68.
Synchronous multiplexed NOR/PSRAM read timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . 148
Table 69.
Synchronous multiplexed PSRAM write timings. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 150
Table 70.
Switching characteristics for NAND flash read cycles  . . . . . . . . . . . . . . . . . . . . . . . . . . . 152
Table 71.
Switching characteristics for NAND flash write cycles . . . . . . . . . . . . . . . . . . . . . . . . . . . 153
Table 72.
SDRAM read timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 154
Table 73.
LPSDR SDRAM read timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 155
Table 74.
SDRAM Write timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 156
Table 75.
LPSDR SDRAM Write timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 156
Table 76.
OCTOSPI characteristics in SDR mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 158
Table 77.
OCTOSPI characteristics in DTR mode (no DQS) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 159
Table 78.
OCTOSPI characteristics in DTR mode (with DQS)/Octal and Hyperbus  . . . . . . . . . . . . 160
Table 79.
Delay Block characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 162
Table 80.
16-bit ADC characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 162
Table 81.
Minimum sampling time vs RAIN (16-bit ADC). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 166
Table 82.
16-bit ADC accuracy. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 168
Table 83.
12-bit ADC characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 171
Table 84.
Minimum sampling time vs RAIN (12-bit ADC). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 174
Table 85.
12-bit ADC accuracy. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 176
Table 86.
DAC characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 177
Table 87.
DAC accuracy. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 179
Table 88.
VREFBUF characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 181
Table 89.
Temperature sensor characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 182
Table 90.
Temperature sensor calibration values. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 182
Table 91.
Digital temperature sensor characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 183
Table 92.
VBAT monitoring characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 183
Table 93.
VBAT charging characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 183
Table 94.
Temperature monitoring characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 184
Table 95.
Voltage booster for analog switch characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 184
Table 96.
COMP characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 184

---

List of tables
Table 97.
Operational amplifier characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 185
Table 98.
DFSDM measured timing . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 188
Table 99.
DCMI characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 190
Table 100.
PSSI transmit characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 191
Table 101.
PSSI receive characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 191
Table 102.
LTDC characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 192
Table 103.
TIMx characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 194
Table 104.
LPTIMx characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 194
Table 105.
Minimum i2c_ker_ck frequency in all I2C modes . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 195
Table 106.
I2C analog filter characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 195
Table 107.
USART (SPI mode) characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 196
Table 108.
SPI characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 198
Table 109.
I2S dynamic characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 201
Table 110.
SAI characteristics  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 203
Table 111.
MDIO slave timing parameters . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 205
Table 112.
Dynamics characteristics: SD / MMC characteristics, VDD = 2.7 to 3.6 V . . . . . . . . . . . . 206
Table 113.
Dynamics characteristics: eMMC characteristics VDD = 1.71V to 1.9V. . . . . . . . . . . . . . 207
Table 114.
USB OTG_FS electrical characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 209
Table 115.
Dynamics characteristics: USB ULPI . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 209
Table 116.
Dynamics characteristics: Ethernet MAC signals for SMI  . . . . . . . . . . . . . . . . . . . . . . . . 210
Table 117.
Dynamics characteristics: Ethernet MAC signals for RMII . . . . . . . . . . . . . . . . . . . . . . . . 211
Table 118.
Dynamics characteristics: Ethernet MAC signals for MII . . . . . . . . . . . . . . . . . . . . . . . . . 212
Table 119.
Dynamics JTAG characteristics . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 213
Table 120.
Dynamics SWD characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 213
Table 121.
LQFP100 - Mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 217
Table 122.
TFBGA100 - Mechanical data  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 220
Table 123.
TFBGA100 - Example of PCB design rules (0.8 mm pitch BGA) . . . . . . . . . . . . . . . . . . . 221
Table 124.
LQFP144 - Mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 223
Table 125.
UFBGA - 144 balls, 7 x 7 mm, 0.50 mm pitch, ultra fine pitch ball grid array
package mechanical data . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 226
Table 126.
UFBGA144 recommended PCB design rules (0.50 mm pitch BGA)  . . . . . . . . . . . . . . . . 227
Table 127.
Thermal characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 228
Table 128.

---

List of figures
List of figures
Figure 1.
STM32H723xE/G block diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 16
Figure 2.
Power-up/power-down sequence . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 25
Figure 3.
STM32H723xE/G bus matrix   . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 29
Figure 4.
TFBGA100 ballout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 53
Figure 5.
LQFP100 pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 54
Figure 6.
LQFP144 pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 55
Figure 7.
UFBGA144 ballout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 56
Figure 8.
Pin loading conditions. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 86
Figure 9.
Pin input voltage . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 86
Figure 10.
Power supply scheme  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 87
Figure 11.
Current consumption measurement scheme . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 88
Figure 12.
External capacitor CEXT . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 92
Figure 13.
High-speed external clock source AC timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . 111
Figure 14.
Low-speed external clock source AC timing diagram. . . . . . . . . . . . . . . . . . . . . . . . . . . . 112
Figure 15.
Typical application with an 8 MHz crystal . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 114
Figure 16.
Typical application with a 32.768 kHz crystal . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 115
Figure 17.
VIL/VIH for all I/Os except BOOT0 . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 127
Figure 18.
Recommended NRST pin protection . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 135
Figure 19.
Asynchronous non-multiplexed SRAM/PSRAM/NOR read waveforms . . . . . . . . . . . . . . 137
Figure 20.
Asynchronous non-multiplexed SRAM/PSRAM/NOR write waveforms . . . . . . . . . . . . . . 139
Figure 21.
Asynchronous multiplexed PSRAM/NOR read waveforms. . . . . . . . . . . . . . . . . . . . . . . . 141
Figure 22.
Asynchronous multiplexed PSRAM/NOR write waveforms  . . . . . . . . . . . . . . . . . . . . . . . 143
Figure 23.
Synchronous non-multiplexed NOR/PSRAM read timings . . . . . . . . . . . . . . . . . . . . . . . . 145
Figure 24.
Synchronous non-multiplexed PSRAM write timings . . . . . . . . . . . . . . . . . . . . . . . . . . . . 147
Figure 25.
Synchronous multiplexed NOR/PSRAM read timings  . . . . . . . . . . . . . . . . . . . . . . . . . . . 149
Figure 26.
Synchronous multiplexed PSRAM write timings. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 151
Figure 27.
NAND controller waveforms for read access . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 153
Figure 28.
NAND controller waveforms for write access . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 154
Figure 29.
SDRAM read access waveforms (CL = 1) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 155
Figure 30.
SDRAM write access waveforms . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 157
Figure 31.
OCTOSPI SDR read/write timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 158
Figure 32.
OCTOSPI DTR mode timing diagram. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 159
Figure 33.
OCTOSPI Hyperbus clock timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 161
Figure 34.
OCTOSPI Hyperbus read timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 161
Figure 35.
OCTOSPI Hyperbus write timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 162
Figure 36.
ADC accuracy characteristics. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 169
Figure 37.
Typical connection diagram when using the ADC with FT/TT pins
featuring analog switch function169
Figure 38.
Power supply and reference decoupling (VREF+ not connected to VDDA). . . . . . . . . . . . . 170
Figure 39.
Power supply and reference decoupling (VREF+ connected to VDDA). . . . . . . . . . . . . . . . 170
Figure 40.
12-bit buffered /non-buffered DAC . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 180
Figure 41.
Channel transceiver timing diagrams . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 189
Figure 42.
DCMI timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 190
Figure 43.
LCD-TFT horizontal timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 193
Figure 44.
LCD-TFT vertical timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 193
Figure 45.
USART timing diagram in SPI master mode. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 197
Figure 46.
USART timing diagram in SPI slave mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 197
Figure 47.
SPI timing diagram - slave mode and CPHA = 0 . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 199

---

List of figures
Figure 48.
SPI timing diagram - slave mode and CPHA = 1 . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 200
Figure 49.
SPI timing diagram - master mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 200
Figure 50.
I2S slave timing diagram (Philips protocol)(1) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 202
Figure 51.
I2S master timing diagram (Philips protocol)(1). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 202
Figure 52.
SAI master timing waveforms . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 204
Figure 53.
SAI slave timing waveforms . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 205
Figure 54.
MDIO slave timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 206
Figure 55.
SD high-speed mode  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 208
Figure 56.
SD default mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 208
Figure 57.
SDMMC DDR mode . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 208
Figure 58.
ULPI timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 210
Figure 59.
Ethernet SMI timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 211
Figure 60.
Ethernet RMII timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 211
Figure 61.
Ethernet MII timing diagram . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 212
Figure 62.
JTAG timing diagram  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 213
Figure 63.
SWD timing diagram. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 214
Figure 64.
LQFP100 - Outline(15). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 216
Figure 65.
LQFP100 - Footprint example  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 218
Figure 66.
TFBGA100 - Outline(13)  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 219
Figure 67.
TFBGA100 - Footprint example . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 221
Figure 68.
LQFP144 - Outline(15). . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 222
Figure 69.
LQFP144 - Footprint example  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 225
Figure 70.
UFBGA - 144 balls, 7 x 7 mm, 0.50 mm pitch, ultra fine pitch ball grid array
package outline. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 226
Figure 71.
UFBGA - 144 balls, 7 x 7 mm, 0.50 mm pitch, ultra fine pitch ball grid array
package recommended footprint  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 227

---

Introduction
Introduction
This document provides information on STM32H723xE/G microcontrollers, such as
description, functional overview, pin assignment and definition, packaging, and ordering
information.
This document should be read in conjunction with the STM32H723xE/G reference manual
(RM0468), available from the STMicroelectronics website www.st.com.
For information on the device errata with respect to the datasheet and reference manual,
refer to the STM32H723 errata sheet (ES0491) available on the STMicroelectronics website
www.st.com.
For information on the Arm®(a) Cortex®-M7 core, refer to the Cortex®-M7 Technical
Reference Manual, available from the http://www.arm.com website.
a.
Arm is a registered trademark of Arm Limited (or its subsidiaries) in the US and/or elsewhere.

---

Description
Description
STM32H723xE/G devices are based on the high-performance Arm® Cortex®-M7 32-bit
RISC core operating at up to 550 MHz. The Cortex® -M7 core features a floating-point unit
(FPU) which supports Arm® double-precision (IEEE 754 compliant) and single-precision
data-processing instructions and data types. The Cortex -M7 core includes 32 Kbytes of
instruction cache and 32 Kbytes of data cache. STM32H723xE/G devices support a full set
of DSP instructions and a memory protection unit (MPU) to enhance application security.
STM32H723xE/G devices incorporate high-speed embedded memories with up to 1 Mbyte
of flash memory, up to 564 Kbytes of RAM (including 192 Kbytes that can be shared
between ITCM and AXI, plus 64 Kbytes exclusively ITCM, plus 128 Kbytes exclusively AXI,
128 Kbyte DTCM, 48 Kbytes AHB and 4 Kbytes of backup RAM), as well as an extensive
range of enhanced I/Os and peripherals connected to APB buses, AHB buses, 2x32-bit
multi-AHB bus matrix and a multilayer AXI interconnect supporting internal and external
memory access. To improve application robustness, all memories feature error code
correction (one error correction, two error detections).
The devices embed peripherals allowing mathematical/arithmetic function acceleration
(CORDIC coprocessor for trigonometric functions and FMAC unit for filter functions). All the
devices offer three ADCs, two DACs, two operational amplifiers, two ultra-low-power
comparators, a low-power RTC, four general-purpose 32-bit timers, 12 general-purpose 16-
bit timers including two PWM timers for motor control, five low-power timers, a true random
number generator (RNG). The devices support four digital filters for external sigma-delta
modulators (DFSDM). They also feature standard and advanced communication interfaces.
•
Standard peripherals
–
Five I2Cs
–
Five USARTs, five UARTs, and one LPUART
–
Six SPIs, four I2Ss. To achieve audio class accuracy, the I2S peripherals can be
clocked by a dedicated internal audio PLL or by an external clock to allow
synchronization (note that the five USARTs also provide SPI slave capability).
–
Two SAI serial audio interfaces
–
One SPDIFRX interface with four inputs
–
One SWPMI (Single Wire Protocol Master Interface)
–
Management Data Input/Output (MDIO) slaves
–
Two SDMMC interfaces
–
A USB OTG high-speed interface with full-speed capability (with the ULPI)
–
Two FDCANs plus one TT-FDCAN interface
–
An Ethernet interface
–
Chrom-ART Accelerator
–
HDMI-CEC

---

Description
•
Advanced peripherals including
–
A flexible memory control (FMC) interface
–
Two Octo-SPI memory interfaces
–
A camera interface for CMOS sensors
–
An LCD-TFT display controller
Refer to Table 1: STM32H723xE/G features and peripheral counts for the list of peripherals
available on each part number.
STM32H723xE/G devices operate in the –40 to +85 °C ambient temperature range from a
1.62 to 3.6 V power supply. The supply voltage can drop down to 1.62 V by using an
external power supervisor (see Section 3.7.2: Power supply supervisor) and connecting the
PDR_ON pin to VSS. Otherwise, the supply voltage must stay above 1.71 V with the
embedded power voltage detector enabled.
Dedicated supply inputs for USB are available to allow a greater power supply choice.
A comprehensive set of power-saving modes allows the design of low-power applications.
STM32H723xE/G devices are offered in several packages ranging from 100 to 144
pins/balls. The set of included peripherals changes with the device chosen.
These features make STM32H723xE/G microcontrollers suitable for a wide range of
applications:
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
•
Mobile applications, Internet of Things
•
Wearable devices: smart watches.
Figure 1 shows the device block diagram.

---

Description
Figure 1. STM32H723xE/G block diagram
MSv52561V5
TT-FDCAN1
FDCAN2
I2C1/SMBUS
I2C2/SMBUS
I2C3/SMBUS
AXI/AHB12 (275MHz)
CH[1:4]N, CH[1:4], ETR, BKIN, BKIN2
as AF
A P B 1 3
0 MHz
SCL, SDA, SMBA as AF
APB1   138MHz (max)
MDMA
PJ,PK[11:0]
SCL, SDA, SMBA as AF
SCL, SDA, SMBA as AF
MOSI, MISO, SCK, NSS /
SDO, SDI, CK, WS, MCK, as AF
TX, RX, RXFD_MODE,
TXFD_MODE as AF
CH[4;1], ETR as AF
FIFO
LCD-TFT
FIFO
CHROM-ART
(DMA2D)
LCD_R[7:0], LCD_G[7:0],
LCD_B[7:0], LCD_HSYNC,
LCD_VSYNC, LCD_DE, LCD_CLK
64-bit AXI BUS-MATRIX
CEC as AF
IN[1:4] as AF
MDC, MDIO as AF
AXIM
AXIM
Arm  CPU
Cortex-M7
550 MHz
AHBP
AHBS
TRACECLK
TRACED[3:0]
NJTRST, JTDI,
JTCK/SWCLK
JTDO/SWDIO, JTDO
JTAG/SW
ETM
I-Cache
32KB
D-Cache
32KB
I-TCM 64KB
D-TCM
64KB
16 Streams
FIFO
SDMMC1
D[7:0], D123DIR, D0DIR,
CMD, CKas AF
FIFO
DMA1
FIFOs
8 Stream
DMA2
FIFOs
ETHER
MAC
FIFO
SDMMC2
FIFO
OTG_HS
FIFO
SRAM1
16 KB
8 Stream
FMC_signals
DMA/
DMA/
PHY
MII / RMII
MDIO
as AF
DP, DM, STP,
NXT,ULPI:CK
, D[7:0], DIR,
ID, VBUS
AHB1 (275
ADC1
OUT1, OUT2 as AF
16b
AXI/AHB34 (275MHz)
WWDG
AHB2 (275MHz)
AHB2 (275MHz)
PA..H[15:0]
HSYNC, VSYNC, PIXCLK, D[13:0]
PDCK, DE, RDY, D[15:0]
UART9
MOSI, MISO, SCK, NSS as AF
MOSI, MISO, SCK, NSS as AF
32-bit AHB BUS-MATRIX
BDMA
DMA
Mux2
Up to 20 analog inputs Most
are common to ADC1 & 2
HSEM
AHB4 (275MHz)
AHB4
AHB4_MEMD3 (275MHz)
AHB4
AHB4
AHB4
VDDA, VSSA
NRESET
WKUP[1;2;4;6]
@VDD
RCC
Reset &
control
OSC32_IN
OSC32_OUT
AWU
VCORE BBgen + POWER MNGT
LS
LS
OSC_IN
OSC_OUT
TS, TAMP1, TAMP3,
OUT, REFIN
VDD
VSS
VCAP, VDDLDO
@VDD
@VDD
@VSW
PWRCTRL
AHB4 (275MHz)
SUPPLY SUPERVISION
Int
POR
reset
@VDD
VINM, VINP, VOUT as AF
CKOUT, DATIN[7:0], CKIN[7:0]
2 compl. chan.(TIM15_CH1[1:2]N),
2 chan. (TIM_CH15[1:2], BKIN as AF
1 compl. chan.(TIM16_CH1N),
1 chan. (TIM16_CH1, BKIN as AF
1 compl. chan.(TIM17_CH1N),
1 chan. (TIM17_CH1, BKIN as AF
D[7:0],
D123DIR,
D0DIR,
CMD, CKas AF
Up to 17 analog inputs
Some common to ADC1 and 2
SD_[A;B], SCK_[A;B], FS_[A;B],
MCLK_[A;B],  D[3:1], CK[2:1] as AF
SCL, SDA, SMBA as AF
COMPx_INP, COMPx_INM,
COMPx_OUT as AF
OUT as AF
D-TCM
64KB
AHB/APB
OCTOSPI1
1 MB FLASH
128 KB AXI
SRAM
FMC
DFSDM
USART10
SD_[A;B], SCK_[A;B], FS_[A;B],
MCLK_[A;B],  D[3:1], CK[2:1] as AF
FIFO
SAI1
SPI5
TIM17
TIM16
TIM15
SPI4
MOSI, MISO, SCK, NSS /
SDO, SDI, CK, WS, MCK, as AF
SPI1/I2S1
USART6
USART1
TIM1/PWM
16b
TIM8/PWM
16b
APB2   138 MHz (max)
ADC3
GPIO PORTA.. H
GPIO PORTJ,K
SAI4
COMP1&2
LPTIM5
OUT as AF
LPTIM4
OUT as AF
LPTIM3
I2C4
MOSI, MISO, SCK, NSS /
SDO, SDI, CK, WS, MCK, as AF
SPI6/I2S6
RX, TX, CK, CTS, RTS as AF
LPUART1
LPTIM2
VREF
SYSCFG
EXTI WKUP
CRC
DAP
RNG
DMA
Mux1
To APB1-2
peripherals
SRAM2
16 KB
ADC2
AHB/APB
TIM6 16b
TIM7 16b
SWPMI
TIM2
32b
TIM3
16b
TIM4
16b
TIM5
32b
TIM12
16b
TIM13
16b
TIM14
16b
USART2
USART3
UART4
UART5
UART7
UART8
SPI2/I2S2
MOSI, MISO, SCK, NSS /
SDO, SDI, CK, WS, MCK, as AF
SPI3/I2S3
MDIOS
10 KB SRAM
RAM
I/F
USBCR
SPDIFRX1
HDMI-CEC
DAC
LPTIM1
OPAMP2
AHB/APB
XTAL 32 kHz
RTC
Backup registers
XTAL OSC
4- 48 MHz
CSI RC
LSI RC
PLL1+PLL2+PLL3
POR/PDR/BOR
PVD
Voltage
regulator
3.3 to 1.2V
LSI
HSI
CSI
HSI48
IN1, IN2, ETR, OUT as AF
AHB1       (275MHz)
16 KB SRAM
4 KB BKP
RAM
AHB4
32-bit AHB BUS-MATRIX
APB4   138MHz (max)
APB4   138 MHz (max)
APB4   138 MHz (max)
IWDG
Temperature
sensor
Shared AXI
I-TCM 192KB
OCTOSPI2
OCTOSPIM
AHB4
OCTOSPI2
signals
OCTOSPI1
signals
DLYBSD1
APB3 (138MHz)
DLYBOS1-2
AHB3
FDCAN3
FIFO
DCMI
PSSI
RX, TX, CK, CTS, RTS, DE as AF
RX, TX, CTS, RTS, DE as AF
CORDIC
FMAC
TIM23
TIM24
32b
32b
I2C5/SMBUS
SCL, SDA, SMBA as AF
Digital filter
RX, TX, CK, CTS, RTS, DE as AF
RX, TX, CK, CTS, RTS, DE as AF
RX, TX, CK, CTS, RTS, DE as AF
RX, TX, CK, CTS, RTS, DE as AF
RX, TX, CTS, RTS, DE as AF
RX, TX, CTS, RTS, DE as AF
RX, TX, CTS, RTS, DE as AF
RX, TX, CTS, RTS, DE as AF
CH[1:4]N, CH[1:4], ETR, BKIN, BKIN2
as AF
IN1, IN2, ETR, OUT as AF
16b
16b
16b
16b
CH[4;1], ETR as AF
CH[4;1], ETR as AF
CH[4;1], ETR as AF
CH[4;1], ETR as AF
CH[4;1], ETR as AF
CH[2;1] as AF
CH1 as AF
CH1 as AF
TX, RX, RXFD_MODE,
TXFD_MODE as AF
TX, RX, RXFD_MODE,
TXFD_MODE as AF
OPAMP1
VINM, VINP, VOUT as AF
HSI48 RC
HSI RC
VBAT
DLYBSD2
AHB/APB
FIFO

---

Description
Table 1. STM32H723xE/G features and peripheral counts
Peripherals
STM32H723
VGH/VEH
STM32H723
VGT/VET
STM32H723
ZGT/ZET
STM32H723
ZGI/ZEI
Flash memory (Kbytes)(1)
SRAM (Kbytes)
SRAM mapped onto AXI bus
SRAM1 (D2 domain)
SRAM2 (D2 domain)
SRAM4 (D3 domain)
RAM shared between ITCM and AXI (Kbytes)
TCM RAM (Kbytes)
ITCM RAM (instruction)
DTCM RAM (data)
Backup SRAM (Kbytes)
FMC
Interface
NOR flash
memory/RAM
controller
-
-
yes
yes
Multiplexed I/O
NOR flash
memory
yes
yes
yes
yes
16-bit NAND flash memory
yes
yes
yes
yes
16-bit SDRAM controller
-
-
yes
yes
GPIO
Octo-SPI interface
OTFDEC
no
CORDIC
yes
FMAC
yes
Timers
General purpose 32 bits
General purpose 16 bits
Advanced control
(PWM)
Basic
Low-power
RTC
Window watchdog /
independent watchdog
Wakeup pins
Tamper pins
Random number generator
yes

---

Description
Cryptographic accelerator
no
Communication
interfaces
SPI / I2S
I2C
USART/UART/
LPUART
5/5/1
5/5/1
5/5/1
5/5/1
SAI/PDM
2/2(2)
2/2(2)
SPDIFRX
HDMI-CEC
SWPMI
MDIO
/1
SDMMC
FDCAN/TT-FDCAN
USB [OTG_HS(ULPI)/FS(PHY)]
1 [1/1]
1 [1/1]
1 [1/1]
1 [1/1]
Ethernet [MII/RMII]
1 [0/1]
1 [0/1]
1 [0/1]
1 [1/1]
Camera interface/PSSI
yes
LCD-TFT
yes
yes
yes
yes
Chrom-ART Accelerator (DMA2D)
yes
16-bit ADCs
Number of ADCs
Number of direct
channelsADC1/ADC2
Number of fast channels
ADC1/ADC2
Number of slow channels
ADC1/ADC2
12-bit ADCs
Number of ADCs
Number of direct channels
Number of fast channels
Number of slow channels
12-bit DAC
Present in IC
yes
Number of channels
Comparators
Operational amplifiers
DFSDM
yes
Maximum CPU frequency
550 MHz
USB separate supply pad
yes
-
yes
yes
Table 1. STM32H723xE/G features and peripheral counts (continued)
Peripherals
STM32H723
VGH/VEH
STM32H723
VGT/VET
STM32H723
ZGT/ZET
STM32H723
ZGI/ZEI

---

Description
USB internal regulator
-
-
-
-
LDO
yes
SMPS step-down converter
-
-
-
-
Operating voltage
1.62 to
3.6 V
1.71 to
3.6 V
1.62 to 3.6 V
Operating
temperatures
Ambient temperature
-40°C to +85°C
Junction temperature
-40°C to +125°C
Package
TFBGA100
LQFP100
LQFP144
UFBGA144
1.
STM32H723xGy products have 1024 Kbytes of flash memory, whereas STM32H723xEy products have 512 Kbytes
2.
For limitations on peripheral features depending on packages, check the available pins/balls in Table 7: STM32H723 pin
and ball descriptions.
Table 1. STM32H723xE/G features and peripheral counts (continued)
Peripherals
STM32H723
VGH/VEH
STM32H723
VGT/VET
STM32H723
ZGT/ZET
STM32H723
ZGI/ZEI

---

Functional overview
Functional overview
3.1
Arm® Cortex®-M7 with FPU
The Arm® Cortex®-M7 with double-precision FPU processor is the latest generation of Arm
processors for embedded systems. It was developed to provide a low-cost platform that
meets the needs of MCU implementation, with a reduced pin count and optimized power
consumption, while delivering outstanding computational performance and low interrupt
latency.
The Cortex®-M7 processor is a highly efficient high-performance featuring:
•
Six-stage dual-issue pipeline
•
Dynamic branch prediction
•
Harvard architecture with L1 caches (32 Kbytes of I-cache and 32 Kbytes of D-cache)
•
64-bit AXI interface
•
64-bit ITCM interface
•
2x32-bit DTCM interfaces
The following memory interfaces are supported:
•
Separate Instruction and Data buses (Harvard Architecture) to optimize CPU latency
•
Tightly Coupled Memory (TCM) interface designed for fast and deterministic SRAM
accesses
•
AXI Bus interface to optimize Burst transfers
•
Dedicated low-latency AHB-Lite peripheral bus (AHBP) to connect to peripherals.
The processor supports a set of DSP instructions, which allow efficient signal processing
and complex algorithm execution.
It also supports single and double precision FPU (floating-point unit) speeds up software
development by using metalanguage development tools, while avoiding saturation.
Figure 1 shows the general block diagram of the STM32H723xE/G family.
3.2
Memory protection unit (MPU)
The memory protection unit (MPU) manages the CPU access rights and the attributes of the
system resources. It has to be programmed and enabled before use. Its main purposes are
to prevent an untrusted user program to accidentally corrupt data used by the OS and/or by
a privileged task, but also to protect data processes or read-protect memory regions.
The MPU defines access rules for privileged accesses and user program accesses. It
allows defining up to 16 protected regions that can in turn be divided into up to eight
independent subregions, where region address, size, and attributes can be configured. The
protection area ranges from 32 bytes to 4 Gbytes of addressable memory.
When an unauthorized access is performed, a memory management exception is
generated.

---

Functional overview
3.3
Memories
3.3.1
Embedded flash memory
The STM32H723xE/G devices embed up to 1 Mbyte of flash memory that can be used for
storing programs and data.
The flash memory is organized as 266-bit flash words memory that can be used for storing
both code and data constants. Each word consists of:
•
one flash word (eight words, 32 bytes, or 256 bits)
•
10 ECC bits (single-error correction and double-error detection).
The flash memory is organized as follows:
•
up to 1 Mbyte of user flash memory block containing eight user sectors of 128 Kbytes
(4 K flash memory words)
•
128 Kbytes of system flash memory from which the device can boot
•
2 Kbytes (64 flash words) of user option bytes for user configuration
3.3.2
Embedded SRAM
All devices feature:
•
from 128 to 320 Kbytes of AXI-SRAM mapped onto the AXI bus on D1 domain
•
SRAM1 mapped on D2 domain: 16 Kbytes
•
SRAM2 mapped on D2 domain: 16 Kbytes
•
SRAM4 mapped on D3 domain: 16 Kbytes
•
4 Kbytes of backup SRAM
The content of this area is protected against possible unwanted write accesses, and
can be retained in Standby or VBAT mode.
•
RAM mapped to TCM interface (ITCM and DTCM):
Both ITCM and DTCM RAMs are zero wait state memories. They can be accessed
either from the CPU or the MDMA (even in Sleep mode) through a specific AHB slave
of the Cortex®-M7CPU(AHBSAHBP):
–
64 to 256 Kbytes of ITCM-RAM (instruction RAM)
This RAM is connected to an ITCM 64-bit interface designed for execution of
critical real-time routines by the CPU.
–
128 Kbytes of DTCM-RAM (2x 64-Kbyte DTCM-RAMs on 2x32-bit DTCM ports)
The DTCM-RAM could be used for critical real-time data, such as interrupt service
routines or stack/heap memory. Both DTCM-RAMs can be used in parallel (for
load/store operations) thanks to the Cortex®-M7 dual issue capability.
The MDMA can be used to load code or data in ITCM or DTCM RAMs. As reflected
above, 192 Kbyte of RAM can be used either for AXI SRAM or ITCM, with a 64Kbyte
granularity.

---

Functional overview
Error code correction (ECC)
Over the product lifetime, and/or due to external events such as radiations, invalid bits in
memories may occur. They can be detected and corrected by ECC. This is an expected
behavior that has to be managed at final-application software level in order to ensure data
integrity through ECC algorithms implementation.
SRAM data are protected by ECC:
•
7 ECC bits are added per 32-bit word.
•
8 ECC bits are added per 64-bit word for AXI-SRAM and ITCM-RAM.
The ECC mechanism is based on the SECDED algorithm. It supports single-error correction
and double-error detection.
3.4
Boot modes
At startup, the boot memory space is selected by the BOOT pin and BOOT_ADDx option
bytes, allowing to program any boot memory address from 0x0000 0000 to 0x3FFF FFFF,
which includes:
•
All flash address space
•
All RAM address space: ITCM, DTCM RAMs and SRAMs
•
The system memory bootloader
The bootloader is located in nonuser system memory. It is used to reprogram the flash
memory through a serial interface (USART, I2C, SPI, FDCAN, USB-DFU). Refer to
application note AN2606 “STM32 microcontroller system memory Boot mode” for details.
3.5
CORDIC coprocessor (CORDIC)
The CORDIC coprocessor provides hardware acceleration of certain mathematical
functions, notably trigonometric, commonly used in motor control, metering, signal
processing and many other applications.
It speeds up the calculation of these functions compared to a software implementation,
allowing a lower operating frequency, or freeing up processor cycles in order to perform
other tasks.
The filter mathematical accelerator unit performs arithmetic operations on vectors. It
comprises a multiplier/accumulator (MAC) unit, together with address generation logic,
which allows it to index vector elements held in local memory.
The unit includes support for circular buffers on input and output, which allows digital filters
to be implemented. Both finite and infinite impulse response filters can be realized.
The unit allows frequent or lengthy filtering operations to be offloaded from the CPU, freeing
up the processor for other tasks. In many cases it can accelerate such calculations
compared to a software implementation, resulting in a speed-up of time critical tasks.

---

Functional overview
CORDIC features
•
24-bit CORDIC rotation engine
•
Circular and Hyperbolic modes
•
Rotation and Vectoring modes
•
Functions: Sine, Cosine, Sinh, Cosh, Atan, Atan2, Atanh, Modulus, Square root,
Natural logarithm
•
Programmable precision up to 20-bit
•
Fast convergence: 4 bits per clock cycle
•
Supports 16-bit and 32-bit fixed point input and output formats
•
Low latency AHB slave interface
•
Results can be read as soon as ready without polling or interrupt
•
DMA read and write channels
3.6
Filter mathematical accelerator (FMAC)
The filter mathematical accelerator unit performs arithmetic operations on vectors. It
comprises a multiplier/accumulator (MAC) unit, together with address generation logic,
which allows it to index vector elements held in local memory.
The unit includes support for circular buffers on input and output, which allows digital filters
to be implemented. Both finite and infinite impulse response filters can be realized.
The unit allows frequent or lengthy filtering operations to be offloaded from the CPU, freeing
up the processor for other tasks. In many cases it can accelerate such calculations
compared to a software implementation, resulting in a speed-up of time critical tasks.
FMAC features
•
16 x 16-bit multiplier
•
24+2-bit accumulator with addition and subtraction
•
16-bit input and output data
•
256 x 16-bit local memory
•
Up to three areas can be defined in memory for data buffers (two inputs, one output),
defined by programmable base address pointers and associated size registers
•
Input and output sample buffers can be circular
•
Buffer “watermark” feature reduces overhead in interrupt mode
•
Filter functions: FIR, IIR (direct form 1)
•
AHB slave interface
•
DMA read and write data channels

---

Functional overview
3.7
Power supply management
3.7.1
Power supply scheme
STM32H723xE/G power supply voltages are the following:
•
VDD = 1.62 to 3.6 V: external power supply for I/Os, provided externally through VDD
pins.
•
VDDLDO = 1.62 to 3.6 V: supply voltage for the internal regulator supplying VCORE
•
VDDA = 1.62 to 3.6 V: external analog power supplies for ADC, DAC, COMP and
OPAMP.
•
VDD33USB: allows the support of a VDD supply different from 3.3 V while powering the
USB transceiver with 3.3V on VDD33USB.
•
VBAT = 1.2 to 3.6 V: power supply for the VSW domain when VDD is not present.
•
VCAP: VCORE supply voltage, which values depend on voltage scaling (1.0 V, 1.1 V,
1.2 V or 1.35 V). They are configured through VOS bits in PWR_D3CR register. The
VCORE domain is split into the following power domains that can be independently
switch off.
–
D1 domain containing some peripherals and the Cortex®-M7 core
–
D2 domain containing a large part of the peripherals
–
D3 domain containing some peripherals and the system control
During power-up and power-down phases, the following power sequence requirements
must be respected (see Figure 2):
•
When VDD is below VDDmin, other power supplies (VDDA, VDD33USB) must remain
below VDD + 300 mV.
•
When VDD is above VDDmin, all power supplies are independent.
During the power-down phase, VDD can temporarily become lower than other supplies only
if the energy provided to the microcontroller remains below 1 mJ. This allows external
decoupling capacitors to be discharged with different time constants during the power-down
transient phase.
3.7.2
Power supply supervisor
The devices have an integrated power-on reset (POR)/ power-down reset (PDR) circuitry
coupled with a brownout reset (BOR) circuitry:
•
Power-on reset (POR)
The POR supervisor monitors VDD power supply and compares it to a fixed threshold.
The devices remain in reset mode when VDD is below this threshold,
•
Power-down reset (PDR)
The PDR supervisor monitors VDD power supply. A reset is generated when VDD drops
below a fixed threshold.
The PDR supervisor can be enabled/disabled through PDR_ON pin.
•
Brownout reset (BOR)
The BOR supervisor monitors VDD power supply. Three BOR thresholds (from 2.1 to
2.7 V) can be configured through option bytes. A reset is generated when VDD drops
below this threshold.

---

Functional overview
Figure 2. Power-up/power-down sequence
1.
VDDx refers to any power supply among VDDA, VDD33USB.
MSv47490V2
0.3
VPDR
3.6
Operating mode
Power-on
Power-down
time
V
VDDX
(1)
VDD
Invalid supply area
VDDX < VDD + 300 mV
VDDX independent from VDD
VPOR

---

Functional overview
3.7.3
Voltage regulator
The same voltage regulator supplies the three power domains (D1, D2, and D3). D1 and D2
can be independently switched off.
Voltage regulator output can be adjusted according to application needs through six power
supply levels:
•
Run mode (VOS0 to VOS3)
–
Scale 0: boosted performance
–
Scale 1: high performance
–
Scale 2: medium performance and consumption
–
Scale 3: optimized performance and low-power consumption
•
Stop mode (SVOS3 to SVOS5)
–
Scale 3: peripheral with wake-up from Stop mode capabilities (UART, SPI, I2C,
LPTIM) are operational
–
Scale 4 and 5 where the peripheral with wake-up from Stop mode is disabled. The
peripheral functionality is disabled but wake-up from Stop mode is possible
through GPIO or asynchronous interrupt.
3.8
Low-power strategy
There are several ways to reduce power consumption on STM32H723xE/G:
•
Decrease the dynamic power consumption by slowing down the system clocks even in
Run mode and by individually clock gating the peripherals that are not used.
•
Save power when the CPU is idle, by selecting among the available low-power modes
according to the user application needs. This allows the best compromise between
short startup time and low power consumption to be achieved, according to the
available wake-up sources.
The devices feature several low-power modes:
•
CSleep (CPU clock stopped)
•
CStop (CPU subsystem clock stopped)
•
DStop (Domain bus matrix clock stopped)
•
Stop (system clock stopped)
•
DStandby (Domain powered down)
•
Standby (system powered down)
CSleep and CStop low-power modes are entered by the MCU when executing the WFI
(Wait for Interrupt) or WFE (Wait for Event) instructions, or when the SLEEPONEXIT bit of
the Cortex®-Mx core is set after returning from an interrupt service routine.
A domain can enter low-power mode (DStop or DStandby) when the processor, its
subsystem, and the peripherals allocated in the domain enter low-power mode.
If part of the domain is not in low-power mode, the domain remains in the current mode.
Finally, the system can enter Stop or Standby when all EXTI wake-up sources are cleared
and the power domains are in DStop or DStandby mode.

---

Functional overview
3.9
Reset and clock controller (RCC)
The clock and reset controller is located in D3 domain. The RCC manages the generation of
all the clocks, as well as the clock gating and the control of the system and peripheral
resets. It provides a high flexibility in the choice of clock sources and allows clock ratios to
be applied to improve the power consumption. In addition, on some communication
peripherals that are capable to work with two different clock domains (either a bus interface
clock or a kernel peripheral clock), thus the system frequency can be changed without
modifying the baud rate.
3.9.1
Clock management
The devices embed four internal oscillators, two oscillators with external crystal or
resonator, two internal oscillators with fast startup time and three PLLs.
The RCC receives the following clock source inputs:
•
Internal oscillators:
–
64 MHz HSI clock
–
48 MHz RC oscillator
–
4 MHz CSI clock
–
32 kHz LSI clock
•
External oscillators:
–
HSE clock: 4-50 MHz (generated from an external source) or 4-48 MHz(generated
from a crystal/ceramic resonator)
–
LSE clock: 32.768 kHz
The RCC provides three PLLs: one for system clock, two for kernel clocks.
The system starts on the HSI clock. The user application can then select the clock
configuration.
Table 2. System versus domain low-power mode
System power mode
D1 domain power mode
D2 domain power mode
D3 domain power mode
Run
DRun/DStop/DStandby
DRun/DStop/DStandby
DRun
Stop
DStop/DStandby
DStop/DStandby
DStop
Standby
DStandby
DStandby
DStandby

---

Functional overview
3.9.2
System reset sources
Power-on reset initializes all registers while system reset reinitializes the system except for
the debug, part of the RCC and power controller status registers, as well as the backup
power domain.
A system reset is generated in the following cases:
•
Power-on reset (pwr_por_rst)
•
Brownout reset
•
Low level on NRST pin (external reset)
•
Window watchdog
•
Independent watchdog
•
Software reset
•
Low-power mode security reset
•
Exit from Standby
3.10
General-purpose input/outputs (GPIOs)
Each of the GPIO pins can be configured by software as output (push-pull or open-drain,
with or without pull-up or pull-down), as input (floating, with or without pull-up or pull-down)
or as peripheral alternate function. Most of the GPIO pins are shared with digital or analog
alternate functions. All GPIOs are high-current-capable and have speed selection to better
manage internal noise, power consumption and electromagnetic emission.
After reset, all GPIOs (except debug pins) are in Analog mode to reduce power
consumption (refer to GPIOs register reset values in the device reference manual).
The I/O configuration can be locked if needed by following a specific sequence in order to
avoid spurious writing to the I/Os registers.
3.11
Bus-interconnect matrix
The devices feature an AXI bus matrix, two AHB bus matrices and bus bridges that allow
the interconnection of bus masters with bus slaves (see Figure 3).

---

Functional overview
Figure 3. STM32H723xE/G bus matrix
MSv65313V1
AXIM
DMA2
Ethernet
MAC
SDMMC2
DMA1
USBHS1
APB1
SDMMC1
MDMA
DMA2D
LTDC
BDMA
APB4
Cortex-M7
I$
32KB
D$
32KB
AHBP
DMA1_MEM
DMA1_PERIPH
DMA2_MEM
DMA2_PERIPH
APB3
32-bit AHB bus matrix
D2 domain
64-bit AXI bus matrix
D1 domain
32-bit AHB bus matrix
D3 domain
DTCM
128 Kbyte
ITCM
64 Kbyte
Flash A
Up to 1 Mbyte
AXI SRAM
192K byte
AXI SRAM
128 Kbyte
FMC
SRAM1 16
Kbyte
SRAM2 16
Kbyte
AHB1
AHB2
AHB4
SRAM4
16 Kbyte
Backup
SRAM
4 Kbyte
AHBS
CPU
D2-to-D1 AHB
D2-to-D3 AHB
D1-to-D2 AHB
D1-to-D3 AHB
32-bit bus
64-bit bus
Bus multiplexer
Legend
Master interface
Slave interface
AHB3
AXI
AHB
APB
APB2
TCM
ITCM
192 Kbyte
OR
OCTOSPI2
OCTOSPI1

---

Functional overview
3.12
DMA controllers
The devices feature four DMA instances and a DMA request router to unload CPU activity:
•
A master direct memory access (MDMA)
The MDMA is a high-speed DMA controller, which is in charge of all types of memory
transfers (peripheral to memory, memory to memory, memory to peripheral), without
any CPU action. It features a master AXI interface and a dedicated AHB interface to
access Cortex®-M7 TCM memories.
The MDMA is located in D1 domain. It is able to interface with the other DMA
controllers located in D2 domain to extend the standard DMA capabilities, or can
manage peripheral DMA requests directly.
Each of the 16 channels can perform single block transfers, repeated block transfers
and linked list transfers.
•
Two dual-port DMAs (DMA1, DMA2) located in D2 domain, with FIFO and request
router capabilities.
•
One basic DMA (BDMA) located in D3 domain, with request router capabilities.
•
A DMA request multiplexer (DMAMUX)
The DMA request router could be considered as an extension of the DMA controller. It
routes the DMA peripheral requests to the DMA controller itself. This allowing
managing the DMA requests with a high flexibility, maximizing the number of DMA
requests that run concurrently, as well as generating DMA requests from peripheral
output trigger or DMA event.
3.13
Chrom-ART Accelerator (DMA2D)
The Chrom-ART Accelerator (DMA2D) is a specialized DMA dedicated to image
manipulation. It can perform the following operations:
•
Filling a part or the whole of a destination image with a specific color
•
Copying a part or the whole of a source image into a part or the whole of a destination
image
•
Copying a part or the whole of a source image into a part or the whole of a destination
image with a pixel format conversion
•
Blending a part and/or two complete source images with different pixel format and copy
the result into a part or the whole of a destination image with a different color format.
•
All the classical color coding schemes are supported from 4-bit up to 32-bit per pixel
with indexed or direct color mode, including block based YCbCr to handle JPEG
decoder output.
•
The DMA2D has its own dedicated memories for CLUTs (color look-up tables).
An interrupt can be generated when an operation is complete or at a programmed
watermark.
All the operations are fully automated and are running independently from the CPU or the
DMAs.

---

Functional overview
3.14
Nested vectored interrupt controller (NVIC)
The devices embed a nested vectored interrupt controller, which is able to manage 16
priority levels, and handle up to 140 maskable interrupt channels plus the 16 interrupt lines
of the Cortex®-M7 with FPU core.
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
Processor context automatically saved on interrupt entry, and restored on interrupt exit
with no instruction overhead
This hardware block provides flexible interrupt management features with minimum interrupt
latency.
3.15
Extended interrupt and event controller (EXTI)
The EXTI controller performs interrupt and event management. In addition, it can wake up
the processor, power domains and/or D3 domain from Stop mode.
The EXTI handles up to 80 independent event/interrupt lines split as 26 configurable events
and 54 direct events.
Configurable events have dedicated pending flags, active edge selection, and software
trigger capable.
Direct events provide interrupts or events from peripherals having a status flag.
3.16
Cyclic redundancy check calculation unit (CRC)
The CRC (cyclic redundancy check) calculation unit is used to get a CRC code using a
programmable polynomial.
Among other applications, CRC-based techniques are used to verify data transmission or
storage integrity. In the scope of the EN/IEC 60335-1 standard, they offer a means of
verifying the flash memory integrity. The CRC calculation unit helps compute a signature of
the software during runtime, to be compared with a reference signature generated at link-
time and stored at a given memory location.

---

Functional overview
3.17
Flexible memory controller (FMC)
The FMC controller main features are the following:
•
Interface with static-memory mapped devices including:
–
Static random access memory (SRAM)
–
NOR flash memory/OneNAND flash memory
–
PSRAM (four memory banks)
–
NAND flash memory with ECC hardware to check up to 8 Kbytes of data
•
Interface with synchronous DRAM (SDRAM/Mobile LPSDR SDRAM) memories
•
8-,16-bit data bus width
•
Independent Chip Select control for each memory bank
•
Independent configuration for each memory bank
•
Write FIFO
•
Read FIFO for SDRAM controller
•
The maximum FMC_CLK/FMC_SDCLK frequency for synchronous accesses is the
FMC kernel clock divided by 2.
3.18
Octo-SPI memory interface (OCTOSPI)
The OCTOSPI is a specialized communication interface targeting single, dual, quad, or octal
SPI memories. The STM32H723xE/G embeds two separate Octo-SPI interfaces.
Each OCTOSPI instance supports single/dual/quad/octal SPI formats. multiplexing of
single/dual/quad/octal SPI over the same bus can be achieved using the integrated Octo-
SPI I/O manager (OCTOSPIM).
The OCTOSPI can operate in any of the three following modes:
•
Indirect mode: all the operations are performed using the OCTOSPI registers
•
Status-polling mode: the external memory status register is periodically read and an
interrupt can be generated in case of flag setting
•
Memory-mapped mode: the external memory is memory mapped and it is seen by the
system as if it was an internal memory supporting both read and write operations.
The OCTOSPI supports two frame formats supported by most external serial memories
such as serial PSRAMs, serial NAND and serial NOR flash memories, Hyper RAMs and
Hyper flash memories.
Multichip package (MCP) combining any of the above mentioned memory types can also be
supported.
•
The classical frame format with the command, address, alternate byte, dummy cycles,
and data phase
•
The HyperBus™ frame format.

---

Functional overview
3.19
Analog-to-digital converters (ADCs)
STM32H723xE/G devices embed three analog-to-digital converters, two of 16-bit resolution,
and the third of 12-bit resolution. The 16-bit resolution ADCs can be configured as 16, 14,
12, 10 or 8 bits. The 12-bit resolution ADC can be configured to 12, 10 or 8 bits.
Each ADC shares up to 20 external channels, performing conversions in Single-shot or
Scan mode. In Scan mode, automatic conversion is performed on a selected group of
analog inputs.
Additional logic functions embedded in the ADC interface allow:
•
simultaneous sample and hold
•
Interleaved sample and hold
The ADC can be served by the DMA controller, thus allowing automatic transfer of ADC
converted values to a destination location without any software action.
In addition, an analog watchdog feature can accurately monitor the converted voltage of
one, some, or all selected channels. An interrupt is generated when the converted voltage is
outside the programmed thresholds.
To synchronize A/D conversion and timers, the ADCs can be triggered by any of the TIM1,
TIM2, TIM3, TIM4, TIM6, TIM8, TIM15, TIM23, TIM24, and LPTIM1 timers.
3.20
Temperature sensor
STM32H723xE/G devices embed a temperature sensor that generates a voltage (VTS) that
varies linearly with the temperature. This temperature sensor is internally connected to
ADC3_IN17. The conversion range is between 1.7 V and 3.6 V. It can measure the device
junction temperature ranging from −40 to +125°C.
The temperature sensor have a good linearity, but it has to be calibrated to obtain a good
overall accuracy of the temperature measurement. As the temperature sensor offset varies
from chip to chip due to process variation, the uncalibrated internal temperature sensor is
suitable for applications that detect temperature changes only. To improve the accuracy of
the temperature sensor measurement, each device is individually factory-calibrated by ST.
The temperature sensor factory calibration data are stored by ST in the system memory
area, which is accessible in read-only mode.
3.21
Digital temperature sensor (DTS)
STM32H723xE/G devices embed a sensor that converts the temperature into a square
wave the frequency of which is proportional to the temperature. The PCLK or the LSE clock
can be used as the reference clock for the measurements. A formula given in the product
reference manual allows calculation of the temperature according to the measured
frequency stored in the DTS_DR register.

---

Functional overview
3.22
VBAT operation
The VBAT power domain contains the RTC, the backup registers, and the backup SRAM.
To optimize battery duration, this power domain is supplied by VDD when available or by the
voltage applied on VBAT pin (when VDD supply is not present). VBAT power is switched
when the PDR detects that VDD dropped below the PDR level.
The voltage on the VBAT pin could be provided by an external battery, a supercapacitor or
directly by VDD, in which case, the VBAT mode is not functional.
VBAT operation is activated when VDD is not present.
The VBAT pin supplies the RTC, the backup registers, and the backup SRAM.
Note:
When the microcontroller is supplied from VBAT, external interrupts and RTC alarm/events
do not exit it from VBAT operation.
When PDR_ON pin is connected to VSS (Internal Reset OFF), the VBAT functionality is no
more available and VBAT pin should be connected to VDD.
3.23
Digital-to-analog converters (DAC)
The two 12-bit buffered DAC channels can be used to convert two digital signals into two
analog voltage signal outputs.
This dual digital interface supports the following features:
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
DMA capability for each channel including DMA underrun error detection
•
external triggers for conversion
•
input voltage reference VREF+ or internal VREFBUF reference.
The DAC channels are triggered through the timer update outputs that are also connected
to different DMA streams.

---

Functional overview
3.24
Ultra-low-power comparators (COMP)
STM32H723xE/G devices embed two rail-to-rail comparators (COMP1 and COMP2). They
feature programmable reference voltage (internal or external), hysteresis, and speed (low
speed for low-power) as well as selectable output polarity.
The reference voltage can be one of the following:
•
An external I/O
•
A DAC output channel
•
An internal reference voltage or submultiple (1/4, 1/2, 3/4).
All comparators can wake up from Stop mode, generate interrupts and breaks for the timers,
and be combined into a window comparator.
3.25
Operational amplifiers (OPAMP)
STM32H723xE/G devices embed two rail-to-rail operational amplifiers (OPAMP1 and
OPAMP2) with external or internal follower routing and PGA capability.
The operational amplifier main features are:
•
PGA with a noninverting gain ranging of 2, 4, 8 or 16 or inverting gain ranging of -1, -3,
-7 or -15
•
One positive input connected to DAC
•
Output connected to internal ADC
•
Low input bias current down to 1 nA
•
Low input offset voltage down to 1.5 mV
•
Gain bandwidth up to 7.3 MHz
The devices embed two operational amplifiers (OPAMP1 and OPAMP2) with two inputs and
one output each. These three I/Os can be connected to the external pins, thus enabling any
type of external interconnections. The operational amplifiers can be configured internally as
a follower, as an amplifier with a noninverting gain ranging from 2 to 16 or with inverting gain
ranging from -1 to -15.

---

Functional overview
3.26
Digital filter for sigma-delta modulators (DFSDM)
The devices embed one DFSDM with four digital filters modules and eight external input
serial channels (transceivers) or alternately eight internal parallel inputs support.
The DFSDM peripheral is dedicated to interface the external Σ∆ modulators to
microcontroller and then to perform digital filtering of the received data streams (which
represent analog value on Σ∆ modulators inputs). DFSDM can also interface PDM (Pulse
Density Modulation) microphones and perform PDM to PCM conversion and filtering in
hardware. DFSDM features optional parallel data stream inputs from internal ADC
peripherals or microcontroller memory (through DMA/CPU transfers into DFSDM).
DFSDM transceivers support several serial interface formats (to support various Σ∆
modulators). DFSDM digital filter modules perform digital processing according to user-
selected filter parameters with up to 24-bit final ADC resolution.
The DFSDM peripheral supports:
•
8 multiplexed input digital serial channels:
–
configurable SPI interface to connect various SD modulators
–
configurable Manchester coded 1 wire interface support
–
PDM (Pulse Density Modulation) microphone input support
–
maximum input clock frequency up to 20 MHz (10 MHz for Manchester coding)
–
clock output for SD modulators: 0..20 MHz
•
alternative inputs from eight internal digital parallel channels (up to 16-bit input
resolution):
–
internal sources: ADC data or memory data streams (DMA)
•
4 digital filter modules with adjustable digital signal processing:
–
Sincx filter: filter order/type (1..5), oversampling ratio (up to 1..1024)
–
integrator: oversampling ratio (1..256)
•
up to 24-bit output data resolution, signed output data format
•
automatic data offset correction (offset stored in register by user)
•
continuous or single conversion
•
start-of-conversion triggered by:
–
software trigger
–
internal timers
–
external events
–
start-of-conversion synchronously with first digital filter module (DFSDM0)
•
analog watchdog feature:
–
low value and high value data threshold registers
–
dedicated configurable Sincx digital filter (order = 1..3, oversampling ratio = 1..32)
–
input from final output data or from selected input digital serial channels
–
continuous monitoring independently from standard conversion
•
short circuit detector to detect saturated analog input values (bottom and top range):
–
up to 8-bit counter to detect 1..256 consecutive 0’s or 1’s on serial data stream
–
monitoring continuously each input serial channel
•
break signal generation on analog watchdog event or on short circuit detector event

---

Functional overview
•
extremes detector:
–
storage of minimum and maximum values of final conversion data
–
refreshed by software
•
DMA capability to read the final conversion data
•
interrupts: end of conversion, overrun, analog watchdog, short circuit, input serial
channel clock absence
•
“regular” or “injected” conversions:
–
“regular” conversions can be requested at any time or even in Continuous mode
without having any impact on the timing of “injected” conversions
–
“injected” conversions for precise timing and with high conversion priority
•
Pulse skipper feature to support beamforming applications (delay-line like behavior).
Table 3. DFSDM implementation
DFSDM features
DFSDM1
Number of filters
Number of input
transceivers/channels
Internal ADC parallel input
X
Number of external triggers
Regular channel information in
identification register
X

---

Functional overview
3.27
Digital camera interface (DCMI)
The devices embed a camera interface that can connect with camera modules and CMOS
sensors through an 8-bit to 14-bit parallel interface, to receive video data. The camera
interface can achieve a data transfer rate up to 140 Mbyte/s using an 80 MHz pixel clock. It
features:
•
Programmable polarity for the input pixel clock and synchronization signals
•
Parallel data communication can be 8-, 10-, 12-, or 14-bit
•
Supports 8-bit progressive video monochrome or raw bayer format, YCbCr 4:2:2
progressive video, RGB 565 progressive video or compressed data (like JPEG)
•
Supports Continuous mode or Snapshot (a single frame) mode
•
Capability to automatically crop the image
3.28
PSSI
The PSSI is a generic synchronous 8-/16-bit parallel data input/output slave interface. It
allows the transmitter to send a data valid signal to indicate when the data is valid, and the
receiver to output a flow control signal to indicate when it is ready to sample the data.
The main PSSI features are:
•
Slave mode operation
•
8- or 16-bit parallel data input or output
•
8-word (32-byte) FIFO
•
Data enable (DE) alternate function input and Ready (RDY) alternate function output.
When enabled, these signals can either allow the transmitter to indicate when the data is
valid or the receiver to indicate when it is ready to sample the data, or both.
The PSSI shares most of its circuitry with the digital camera interface (DCMI). It therefore
cannot be used simultaneously with the DCMI.
3.29
LCD-TFT controller
The LCD-TFT display controller provides a 24-bit parallel digital RGB (Red, Green, Blue)
and delivers all signals to interface directly to a broad range of LCD and TFT panels up to
XGA (1024 x 768) resolution with the following features:
•
2 display layers with dedicated FIFO (64x64-bit)
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
Up to four programmable interrupt events
•
AXI master interface with burst of 16 words

---

Functional overview
3.30
True random number generator (RNG)
The RNG is a true random number generator that provides full entropy outputs to the
application as 32-bit samples. It is composed of a live entropy source (analog) and an
internal conditioning component.
The RNG can be used to construct a nondeterministic random bit generator (NDRBG), as a
NIST SP 800-90B compliant entropy source.
The RNG true random number generator has been tested using German BSI statistical tests
of AIS-31 (T0 to T8), and NIST SP800-90B statistical test suite.

---

Functional overview
3.31
Timers and watchdogs
The devices include two advanced-control timers, twelve general-purpose timers, two basic
timers, five low-power timers, two watchdogs and a SysTick timer.
All timer counters can be frozen in Debug mode.
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
Comple-
mentary
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
137.5
General
purpose
TIM2,
TIM5,
TIM23,
TIM24
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
137.5
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
137.5
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
137.5
TIM13,
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
137.5
TIM15
16-bit
Up
Any
integer
between 1
and
65536
Yes
137.5
TIM16,
TIM17
16-bit
Up
Any
integer
between 1
and
65536
Yes
137.5

---

Functional overview
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
137.5
Low-
power
timer
LPTIM1,
LPTIM2,
LPTIM3,
LPTIM4,
LPTIM5
16-bit
Up
1, 2, 4, 8,
16, 32,
64, 128
No
No
137.5
1.
The maximum timer clock is up to 275 MHz depending on the TIMPRE bit in the RCC_CFGR register and D2PRE1/2 bits in
RCC_D2CFGR register.
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
Comple-
mentary
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

---

Functional overview
3.31.1
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
PWM generation (Edge- or center-aligned modes)
•
One-pulse mode output
If configured as standard 16-bit timers, they have the same features as the general-purpose
TIMx timers. If configured as 16-bit PWM generators, they have full modulation capability (0-
100%).
The advanced-control timer can work together with the TIMx timers via the Timer Link
feature for synchronization or event chaining.
TIM1 and TIM8 support independent DMA request generation.
3.31.2
General-purpose timers (TIMx)
There are 10 synchronizable general-purpose timers embedded in the STM32H723xE/G
devices (see Table 4: Timer feature comparison for differences).
•
TIM2, TIM3, TIM4, TIM5, TIM23, TIM24
The devices include four full-featured general-purpose timers: TIM2, TIM3, TIM4,
TIM5, TIM23 and TIM24. TIM2, TIM5, TIM23 and TIM24 are based on a 32-bit
autoreload up/downcounter and a 16-bit prescaler while TIM3 and TIM4 are based on a
16-bit autoreload up/downcounter and a 16-bit prescaler. All timers feature 4
independent channels for input capture/output compare, PWM or One-pulse mode
output. This gives up to 24 input capture/output compare/PWMs on the largest
packages.
TIM2, TIM3, TIM4, TIM5, TIM23 and TIM24 general-purpose timers can work together,
or with the other general-purpose timers and the advanced-control timers TIM1 and
TIM8 via the Timer Link feature for synchronization or event chaining.
Any of these general-purpose timers can be used to generate PWM outputs.
TIM2, TIM3, TIM4, TIM5, TIM23, and TIM24 all have independent DMA request
generation. They are capable of handling quadrature (incremental) encoder signals
and the digital outputs from one to four hall-effect sensors.
•
TIM12, TIM13, TIM14, TIM15, TIM16, TIM17
These timers are based on a 16-bit autoreload upcounter and a 16-bit prescaler.
TIM13, TIM14, TIM16 and TIM17 feature one independent channel, whereas TIM12
and TIM15 have two independent channels for input capture/output compare, PWM or
One-pulse mode output. They can be synchronized with the TIM2, TIM3, TIM4, TIM5,
TIM23, and TIM24 full-featured general-purpose timers or used as simple time bases.

---

Functional overview
3.31.3
Basic timers TIM6 and TIM7
These timers are mainly used for DAC trigger and waveform generation. They can also be
used as a generic 16-bit time base.
TIM6 and TIM7 support independent DMA request generation.
3.31.4
Low-power timers (LPTIM1, LPTIM2, LPTIM3, LPTIM4, LPTIM5)
The low-power timers have an independent clock and is running also in Stop mode if it is
clocked by LSE, LSI or an external clock. It is able to wake up the devices from Stop mode.
This low-power timer supports the following features:
•
16-bit up counter with 16-bit autoreload register
•
16-bit compare register
•
Configurable output: pulse, PWM
•
Continuous / One-shot mode
•
Selectable software / hardware input trigger
•
Selectable clock source:
•
Internal clock source: LSE, LSI, HSI or APB clock
•
External clock source over LPTIM input (working even with no internal clock source
running, used by the Pulse Counter Application)
•
Programmable digital glitch filter
•
Encoder mode
3.31.5
Independent watchdog
The independent watchdog is based on a 12-bit downcounter and 8-bit prescaler. It is
clocked from an independent 32 kHz internal RC and as it operates independently from the
main clock, it can operate in Stop and Standby modes. It can be used either as a watchdog
to reset the device when a problem occurs, or as a free-running timer for application timeout
management. It is hardware- or software-configurable through the option bytes.
A window option allows the device to be reset when a reload operation is made too early
after the previous reload.
3.31.6
Window watchdog
The window watchdog is based on a 7-bit downcounter that can be set as free-running. It
can be used as a watchdog to reset the device when a problem occurs. It is clocked from
the main clock. It has an early warning interrupt capability and the counter can be frozen in
Debug mode.
3.31.7
SysTick timer
This timer is dedicated to real-time operating systems, but could also be used as a standard
down counter. It features:
•
A 24-bit down counter
•
Autoreload capability
•
Maskable system interrupt generation when the counter reaches 0
•
Programmable clock source.

---

Functional overview
3.32
Real-time clock (RTC), backup SRAM and backup registers
The RTC is an independent BCD timer/counter. It supports the following features:
•
Calendar with subsecond, seconds, minutes, hours (12 or 24 format), week day, date,
month, year, in BCD (binary-coded decimal) format.
•
Automatic correction for 28, 29 (leap year), 30, and 31 days of the month.
•
Two programmable alarms.
•
On-the-fly correction from 1 to 32767 RTC clock pulses. This can be used to
synchronize it with a master clock.
•
Reference clock detection: a more precise second source clock (50 or 60 Hz) can be
used to enhance the calendar precision.
•
Digital calibration circuit with 0.95 ppm resolution, to compensate for quartz crystal
inaccuracy.
•
Three anti-tamper detection pins with programmable filter.
•
Timestamp feature which can be used to save the calendar content. This function can
be triggered by an event on the timestamp pin, or by a tamper event, or by a switch to
VBAT mode.
•
17-bit autoreload wake-up timer (WUT) for periodic events with programmable
resolution and period.
The RTC and the 32 backup registers are supplied through a switch that takes power either
from the VDD supply when present or from the VBAT pin.
The backup registers are 32-bit registers used to store 128 bytes of user application data
when VDD power is not present. They are not reset by a system or power reset, or when the
device wakes up from Standby mode.
The RTC clock sources can be:
•
A 32.768 kHz external crystal (LSE)
•
An external resonator or oscillator (LSE)
•
The internal low-power RC oscillator (LSI, with typical frequency of 32 kHz)
•
The high-speed external clock (HSE) divided by 32.
The RTC is functional in VBAT mode and in all low-power modes when it is clocked by the
LSE. When clocked by the LSI, the RTC is not functional in VBAT mode, but is functional in
all low-power modes.
All RTC events (Alarm, wake-up timer, timestamp or tamper) can generate an interrupt and
wake up the device from the low-power modes.

---

Functional overview
3.33
Inter-integrated circuit interface (I2C)
STM32H723xE/G devices embed five I2C interfaces.
The I2C bus interface handles communications between the microcontroller and the serial
I2C bus. It controls all I2C bus-specific sequencing, protocol, arbitration and timing.
The I2C peripheral supports:
•
I2C-bus specification and user manual rev. 5 compatibility:
–
Target and controller modes, multicontroller capability
–
Standard-mode (Sm), with a bitrate up to 100 kbit/s
–
Fast-mode (Fm), with a bitrate up to 400 kbit/s
–
Fast-mode Plus (Fm+), with a bitrate up to 1 Mbit/s and 20 mA output drive I/Os
–
7-bit and 10-bit addressing mode, multiple 7-bit target addresses
–
Programmable setup and hold times
–
Optional clock stretching
•
System Management Bus (SMBus) specification rev 2.0 compatibility:
–
Hardware PEC (Packet Error Checking) generation and verification with ACK
control
–
Address resolution protocol (ARP) support
–
SMBus alert
•
Power system management protocol (PMBusTM) specification rev 1.1 compatibility
•
Independent clock: a choice of independent clock sources allowing the I2C
communication speed to be independent from the PCLK reprogramming.
•
Wake up from Stop mode on address match
•
Programmable analog and digital noise filters
•
1-byte buffer with DMA capability
3.34
Universal synchronous/asynchronous receiver transmitter
(USART)
STM32H723xE/G devices have five embedded universal synchronous receiver transmitters
(USART1, USART2, USART3, USART6, and USART10) and five universal asynchronous
receiver transmitters (UART4, UART5, UART7, UART8, and UART9). Refer to Table 5:
USART features for a summary of USARTx and UARTx features.
These interfaces provide asynchronous communication, IrDA SIR ENDEC support,
multiprocessor communication mode, single-wire half-duplex communication mode and
have LIN master/slave capability. They provide hardware management of the CTS and RTS
signals, and RS485 Driver Enable. They are able to communicate at speeds of up to
17 Mbit/s.
USART1, USART2, USART3, USART6, and USART10 also provide Smartcard mode (ISO
7816 compliant) and SPI-like communication capability.
The USARTs embed a Transmit FIFO (TXFIFO) and a Receive FIFO (RXFIFO). FIFO mode
is enabled by software and is disabled by default.

---

Functional overview
All USART have a clock domain independent from the CPU clock, allowing the USARTx to
wake up the MCU from Stop mode. The wake-up from Stop mode is programmable and can
be done on:
•
Start bit detection
•
Any received data frame
•
A specific programmed data frame
•
Specific TXFIFO/RXFIFO status when FIFO mode is enabled.
All USART interfaces can be served by the DMA controller.
3.35
Low-power universal asynchronous receiver transmitter
(LPUART)
The device embeds one Low-Power UART (LPUART1). The LPUART supports
asynchronous serial communication with minimum power consumption. It supports half
duplex single wire communication and modem operations (CTS/RTS). It allows
multiprocessor communication.
The LPUARTs embed a Transmit FIFO (TXFIFO) and a Receive FIFO (RXFIFO). FIFO
mode is enabled by software and is disabled by default.
Table 5. USART features
USART modes/features(1)
1. X = supported.
USART1/2/3/6/10
UART4/5/7/8/9
Hardware flow control for modem
X
X
Continuous communication using DMA
X
X
Multiprocessor communication
X
X
Synchronous SPI mode (master/slave)
X
-
Smartcard mode
X
-
Single-wire half-duplex communication
X
X
IrDA SIR ENDEC block
X
X
LIN mode
X
X
Dual clock domain and wake-up from low power mode
X
X
Receiver timeout interrupt
X
X
Modbus communication
X
X
Auto baud rate detection
X
X
Driver Enable
X
X
USART data length
7, 8 and 9 bits
Tx/Rx FIFO
X
X
Tx/Rx FIFO size

---

Functional overview
The LPUART has a clock domain independent from the CPU clock, and can wake up the
system from Stop mode. The wake-up from Stop mode are programmable and can be done
on:
•
Start bit detection
•
Any received data frame
•
A specific programmed data frame
•
Specific TXFIFO/RXFIFO status when FIFO mode is enabled.
Only a 32.768 kHz clock (LSE) is needed to allow LPUART communication up to
9600 baud. Therefore, even in Stop mode, the LPUART can wait for an incoming frame
while having an extremely low energy consumption. Higher speed clock can be used to
reach higher baud rates.
LPUART interface can be served by the DMA controller.
3.36
Serial peripheral interface (SPI)/inter- integrated sound
interfaces (I2S)
The devices feature up to six SPIs (SPI2S1, SPI2S2, SPI2S3, SPI4, SPI5 and SPI2S6) that
allow communicating up to 150 Mbits/s in master and slave modes, in half-duplex, full-
duplex and simplex modes. The 3-bit prescaler gives eight master mode frequencies and
the frame is configurable from 4 to 32 bits for SPI1/I2S1, SPI2/I2S2, SPI3/I2S3, and from 4
to 16 bits for the other peripherals.
All SPI interfaces support NSS pulse mode, TI mode, Hardware CRC calculation, and 16x
8-bit embedded Rx and Tx FIFOs (SPI1/I2S1, SPI2/I2S2, SPI3/I2S3), and 8x 8-bit
embedded Rx and Tx FIFOs (SPI4, SPI5, SPI6/I2S6), all with DMA capability.
Four standard I2S interfaces (multiplexed with SPI1, SPI2, SPI3 and SPI6) are available.
They can be operated in master or slave mode, in half-, full-duplex or simplex
communication mode, and can be configured to operate as a 16-/32-bit resolution input or
output channel (except SPI2S6 which is limited to 16 bits). Audio sampling frequencies from
8 kHz up to 192 kHz are supported. When either or both of the I2S interfaces is/are
configured in master mode, the master clock can be output to the external DAC/CODEC at
256 times the sampling frequency. All I2S interfaces support 16x 8-bit embedded Rx and Tx
FIFOs with DMA capability.
3.37
Serial audio interfaces (SAI)
The devices embed two SAIs (SAI1, and SAI4) that allow designing many stereo or mono
audio protocols such as I2S, LSB or MSB-justified, PCM/DSP, TDM or AC’97. An SPDIF
output is available when the audio block is configured as a transmitter. To bring this level of
flexibility and reconfigurability, the SAI contains two independent audio subblocks. Each
block has it own clock generator and I/O line controller.
Audio sampling frequencies up to 192 kHz are supported.
In addition, up to six microphones per SAI instance can be supported thanks to an
embedded PDM interface, with a maximum of 10 microphones due to pinout constraints.
The SAI can work in master or slave configuration. The audio subblocks can be either
receiver or transmitter and can work synchronously or asynchronously (with respect to the
other one). The SAI can be connected with other SAIs to work synchronously.

---

Functional overview
3.38
SPDIFRX Receiver Interface (SPDIFRX)
The SPDIFRX peripheral is designed to receive an S/PDIF flow compliant with IEC-60958
and IEC-61937. These standards support simple stereo streams up to high sample rate,
and compressed multichannel surround sound, such as those defined by Dolby or DTS (up
to 5.1).
The main SPDIFRX features are the following:
•
Up to four inputs available
•
Automatic symbol rate detection
•
Maximum symbol rate: 12.288 MHz
•
Stereo stream from 32 to 192 kHz supported
•
Supports Audio IEC-60958 and IEC-61937, consumer applications
•
Parity bit management
•
Communication using DMA for audio samples
•
Communication using DMA for control and user channel information
•
Interrupt capabilities
The SPDIFRX receiver provides all the necessary features to detect the symbol rate, and
decode the incoming data stream. The user can select the wanted SPDIF input, and when a
valid signal is available, the SPDIFRX resamples the incoming signal, decode the
Manchester stream, recognize frames, subframes and blocks elements. It delivers to the
CPU decoded data, and associated status flags.
The SPDIFRX also offers a signal named spdif_frame_sync, which toggles at the S/PDIF
subframe rate that is used to compute the exact sample rate for clock drift algorithms.
3.39
Single wire protocol master interface (SWPMI)
The Single wire protocol master interface (SWPMI) is the master interface corresponding to
the Contactless Frontend (CLF) defined in the ETSI TS 102 613 technical specification. The
main features are:
•
full-duplex communication mode
•
automatic SWP bus state management (active, suspend, resume)
•
configurable bitrate up to 2 Mbit/s
•
automatic SOF, EOF and CRC handling
SWPMI can be served by the DMA controller.

---

Functional overview
3.40
Management data input/output (MDIO) slaves
The devices embed an MDIO slave interface it includes the following features:
•
32 MDIO Registers addresses, each of which is managed using separate input and
output data registers:
–
32 x 16-bit firmware read/write, MDIO read-only output data registers
–
32 x 16-bit firmware read-only, MDIO write-only input data registers
•
Configurable slave (port) address
•
Independently maskable interrupts/events:
–
MDIO Register write
–
MDIO Register read
–
MDIO protocol error
•
Able to operate in and wake up from Stop mode
3.41
SD/SDIO/MMC card host interfaces (SDMMC)
Two SDMMC host interfaces are available. They support MultiMediaCard System
Specification Version 4.51 in three different databus modes: 1 bit (default), 4 bits and 8 bits.
Both interfaces support the SD memory card specifications version 4.1. and the SDIO card
specification version 4.0. in two different databus modes: 1 bit (default) and 4 bits.
Each SDMMC host interface supports only one SD/SDIO/MMC card at any one time and a
stack of MMC Version 4.51 or previous.
The SDMMC host interface embeds a dedicated DMA controller allowing high-speed
transfers between the interface and the SRAM.
3.42
Controller area network (FDCAN1, FDCAN2, FDCAN3)
The controller area network (CAN) subsystem consists of two CAN modules, a shared
message RAM memory and a clock calibration unit.
All CAN modules (FDCAN1, FDCAN2, and FDCAN3) are compliant with ISO 11898-1 (CAN
protocol specification version 2.0 part A, B) and CAN FD protocol specification version 1.0.
FDCAN1 supports time triggered CAN (TT-FDCAN) specified in ISO 11898-4, including
event synchronized time-triggered communication, global system time, and clock drift
compensation. The FDCAN1 contains additional registers, specific to the time triggered
feature. The CAN FD option can be used together with event-triggered and time-triggered
CAN communication.
A 10-Kbyte message RAM memory implements filters, receive FIFOs, receive buffers,
transmit event FIFOs, transmit buffers (and triggers for TT-FDCAN). This message RAM is
shared between the three modules - FDCAN1 FDCAN2 and FDCAN3.
The common clock calibration unit is optional. It can be used to generate a calibrated clock
for FDCAN1, FDCAN2 and FDCAN3 from the HSI internal RC oscillator and the PLL, by
evaluating CAN messages received by the FDCAN1.

---

Functional overview
3.43
Universal serial bus on-the-go high-speed (OTG_HS)
The devices embed a USB OTG high-speed (up to 480 Mbit/s) device/host/OTG peripheral
that supports both full-speed and high-speed operations. It integrates the transceivers for
full-speed operation (12 Mbit/s) and a UTMI low-pin interface (ULPI) for high-speed
operation (480 Mbit/s). When using the USB OTG_HS interface in HS mode, an external
PHY device connected to the ULPI is required.
The USB OTG_HS peripheral is compliant with the USB 2.0 specification and with the OTG
2.0 specification. It features software-configurable endpoint setting and supports
suspend/resume. The USB OTG_HS controller requires a dedicated 48 MHz clock that is
generated by a PLL connected to the HSE oscillator.
The main features are:
•
Combined Rx and Tx FIFO size of 4 Kbytes with dynamic FIFO sizing
•
Supports the session request protocol (SRP) and host negotiation protocol (HNP)
•
8 bidirectional endpoints
•
16 host channels with periodic OUT support
•
Software configurable to OTG1.3 and OTG2.0 modes of operation
•
USB 2.0 LPM (Link Power Management) support
•
Battery Charging Specification Revision 1.2 support
•
Internal FS OTG PHY support
•
External HS or HS OTG operation supporting ULPI in SDR mode The OTG PHY is
connected to the microcontroller ULPI port through 12 signals. It can be clocked using
the 60 MHz output.
•
Internal USB DMA
•
HNP/SNP/IP inside (no need for any external resistor)
•
For OTG/Host modes, a power switch is needed in case bus-powered devices are
connected
3.44
Ethernet MAC interface with dedicated DMA controller (ETH)
The devices provide an IEEE-802.3-2002-compliant media access controller (MAC) for
ethernet LAN communications through an industry-standard medium-independent interface
(MII) or a reduced medium-independent interface (RMII). The microcontroller requires an
external physical interface device (PHY) to connect to the physical LAN bus (twisted-pair,
fiber, etc.). The PHY is connected to the device MII port using 17 signals for MII or 9 signals
for RMII, and can be clocked using the 25 MHz (MII) from the microcontroller.

---

Functional overview
The devices include the following features:
•
Supports 10 and 100 Mbit/s rates
•
Dedicated DMA controller allowing high-speed transfers between the dedicated SRAM
and the descriptors
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
3.45
High-definition multimedia interface (HDMI)
- consumer electronics control (CEC)
The devices embed a HDMI-CEC controller that provides hardware support for the
Consumer Electronics Control (CEC) protocol (Supplement 1 to the HDMI standard).
This protocol provides high-level control functions between all audiovisual products in an
environment. It is specified to operate at low speeds with minimum processing and memory
overhead. It has a clock domain independent from the CPU clock, allowing the HDMI-CEC
controller to wake up the MCU from Stop mode on data reception.
3.46
Debug infrastructure
The devices offer a comprehensive set of debug and trace features to support software
development and system integration.
•
Breakpoint debugging
•
Code execution tracing
•
Software instrumentation
•
JTAG debug port
•
Serial-wire debug port
•
Trigger input and output
•
Serial-wire trace port
•
Trace port
•
Arm® CoreSight™ debug and trace components
The debug can be controlled via a JTAG/Serial-wire debug access port, using industry-
standard debugging tools. The trace port performs data capture for logging and analysis.

---

Memory mapping
Memory mapping
Refer to the product line reference manual for details on the memory mapping as well as the
boundary addresses for all peripherals.

---

Pinouts, pin descriptions and alternate functions
Pinouts, pin descriptions and alternate functions
Figure 4. TFBGA100 ballout
1.
The above figure shows the package top view.
MSv52520V1.
PC14-
OSC32_IN
PC13
PE2
PB9
PB7
PB4
PB3
PA15
PA14
PA13
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
PC15-
OSC32_OUT
VBAT
PE3
PB8
PB6
PD5
PD2
PC11
PC10
PA12
PH0-OSC_IN
VSS
PE4
PE1
PB5
PC12
PA9
PA11
PH1-
OSC_OUT
VDD
PE5
PA10
NRST
PC2_C
PE6
PC7
PC0
PC1
VSSA
PA0
VDDA
PA1
PA5
PB14
VSS
PA2
PA6
PD13
VDD
PA3
PA7
PB1
PE9
PB12
PD8
PD12
PE0
BOOT0
PD7
PD4
PD6
PD3
PD0
PA8
VSS
VSS
VSS
VCAP
PD1
PC9
PC3_C
VDD
VDD
VDD33USB
PDR_ON
VCAP
PC8
PA4
PC4
PB2
PE10
PE14
PD15
PD11
PC6
PB15
PC5
PE7
PE11
PE15
PD14
PD10
PB0
PE8
PE12
PB10
PB13
PD9
PE13
PB11

---

Pinouts, pin descriptions and alternate functions
Figure 5. LQFP100 pinout
1.
The above figure shows the package top view.
MSv52521V1.
LQFP100
PE2
PE3
PE4
PE6
PC13
PC15-OSC32_OUT
VDD
NRST
PC2_C
PE5
VBAT
PC14-OSC32_IN
VSS
PH1-OSC_OUT
PC1
VSSA
PA0
PA2
PH0-OSC_IN
PC0
PC3_C
VREF+
PA1
PA3
VDDA
VDD
VSS
VCAP
PA12
PA10
PA8
PC8
PD15
PD12
PA13
PA11
PA9
PC9
PC6
PD13
PD10
PB15
PB13
PC7
PD14
PD11
PD9
PB14
PB12
PD8
VSS
VDD
PA4
PA6
PC4
PB0
PB2
PE9
PE12
PA5
PA7
PC5
PB1
PE8
PE11
PE14
PB11
VSS
PE7
PE10
PE13
PE15
VCAP
VDD
PB10
VDD
VSS
PE1
PB9
BOOT0
PB6
PB4
PD6
PD3
PE0
PB8
PB7
PB5
PD7
PD4
PD1
PC11
PA15
PB3
PD5
PD2
PD0
PC10
PA14
PC12

---

Pinouts, pin descriptions and alternate functions
Figure 6. LQFP144 pinout
1.
The above figure shows the package top view.
MSv52522V1.
LQFP144
VDD
PE2
PC13
PC15-OSC32_OUT
PF1
PF3
PF5
VDD
PF7
PF9
PH0-OSC_IN
NRST
PC1
PC3_C
VSSA
VDDA
PA1
PE3
PE5
PC14-OSC32_IN
PF0
PF2
PF4
VSS
PF6
PF8
PF10
PH1-OSC_OUT
PC0
PC2_C
VDD
VREF+
PA0
PA2
PE4
VBAT
PE6
PA10
PA8
PC8
PC6
VSS
PG7
PG5
PG3
PD15
VDD
PD13
PD11
PD9
PB15
PB13
VSS
PA13
PA9
PC9
PC7
VDD33USB
PG8
PG6
PG4
PG2
PD14
VSS
PD12
PD10
PD8
PB14
PB12
VCAP
PA11
PA12
VDD
BOOT0
PB6
PB4
PG15
VSS
PG13
PG11
PG9
PD6
VSS
PD4
PD2
PD0
PC11
PA15
PDR_ON
PE0
PB7
PB5
PB3
VDD
PG14
PG12
PG10
PD7
VDD
PD5
PD3
PD1
PC12
PC10
PA14
PE1
PB8
PB9
PA3
PA7
PC5
PB1
PF11
VSS
PF13
PF15
PG1
PE8
VSS
PE10
PE12
PE14
PB10
VCAP
VSS
PA4
PC4
PB0
PB2
PF12
VDD
PF14
PG0
PE7
PE9
VDD
PE11
PE13
PE15
PB11
VDD
VDD
PA6
PA5

---

Pinouts, pin descriptions and alternate functions
Figure 7. UFBGA144 ballout
1.
The above figure shows the package top view.
MSv52523V1.
PC13
PE3
PE2
PE1
PE0
PB4
PB3
PD6
PD7
PA15
PA14
PA13
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
PC14-
OSC32_IN
PE4
PE5
PE6
PB9
PB5
PG15
PG12
PD5
PC11
PC10
PA12
PC15-
OSC32_OUT
VBAT
PF0
PF1
PB8
PG11
PD4
PC12
VDD33USB
PA11
PH0-OSC_IN
VSS
VDD
PD1
PA10
PA9
PH1-
OSC_OUT
PF3
PF4
PD0
PC9
PA8
NRST
PF7
PC8
PC7
PF10
PF9
PG8
PC6
PC0
PC1
PC2
PG7
PG6
PG5
VSSA
PA0
PA4
PG4
PG3
PG2
VREF-
PA1
PC5
PF13
PE13
PD9
PD13
PD14
PD15
VREF+
PA2
PA6
PB0
PF12
PF15
PE8
PE14
PD8
PD12
PB14
PB15
VDDA
PA3
PA7
PB1
PF11
PF14
PE7
PE15
PB10
PB11
PB12
PB13
PF2
BOOT0
PB7
PG13
PB6
PG14
PG10
PD3
PG9
PD2
VSS
VSS
PF5
PDR_ON
PF6
VDD
VDD
VDD
VDD
VDD
VDD
VDD
PF8
VSS
VSS
VCAP
VDD
VDD
VSS
VDD
PE11
PD11
VSS
VCAP
PC3
PE12
PD10
PG1
PE10
PC4
PB2
PG0
PE9
VSS
PA5
Table 6. Legend/abbreviations used in the pinout table
Name
Abbreviation
Definition
Pin name
Unless otherwise specified in brackets below the pin name, the pin function during
and after reset is the same as the actual pin name
Pin type
S
Supply pin
I
Input only pin
I/O
Input / output pin
ANA
Analog-only Input
I/O structure
FT
5 V tolerant I/O
TT
3.3 V tolerant I/O
B
Dedicated BOOT0 pin
RST
Bidirectional reset pin with embedded weak pull-up resistor
Option for TT and FT I/Os
_f
I2C FM+ option
_a
analog option (supplied by VDDA)
_u
USB option (supplied by VDD33USB)
_h
High-speed low-voltage I/O
Notes
Unless otherwise specified by a note, all I/Os are set as floating inputs during and
after reset.

---

Pinouts, pin descriptions and alternate functions
Pin functions
Alternate
functions
Functions selected through GPIOx_AFR registers
Additional
functions
Functions directly selected/enabled through peripheral registers
Table 6. Legend/abbreviations used in the pinout table (continued)
Name
Abbreviation
Definition
Table 7. STM32H723 pin and ball descriptions
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144
A3
A3
PE2
I/O
FT_h
-
TRACECLK, SAI1_CK1,
USART10_RX, SPI4_SCK,
SAI1_MCLK_A, SAI4_MCLK_A,
OCTOSPIM_P1_IO2, SAI4_CK1,
ETH_MII_TXD3, FMC_A23,
EVENTOUT
-
B3
A2
PE3
I/O
FT_h
-
TRACED0, TIM15_BKIN, SAI1_SD_B,
SAI4_SD_B, USART10_TX, FMC_A19,
EVENTOUT
-
C3
B2
PE4
I/O
FT_h
-
TRACED1, SAI1_D2,
DFSDM1_DATIN3, TIM15_CH1N,
SPI4_NSS, SAI1_FS_A, SAI4_FS_A,
SAI4_D2, FMC_A20,
DCMI_D4/PSSI_D4, LCD_B0,
EVENTOUT
-
D3
B3
PE5
I/O
FT_h
-
TRACED2, SAI1_CK2,
DFSDM1_CKIN3, TIM15_CH1,
SPI4_MISO, SAI1_SCK_A,
SAI4_SCK_A, SAI4_CK2, FMC_A21,
DCMI_D6/PSSI_D6, LCD_G0,
EVENTOUT
-
E3
B4
PE6
I/O
FT_h
-
TRACED3, TIM1_BKIN2, SAI1_D1,
TIM15_CH2, SPI4_MOSI, SAI1_SD_A,
SAI4_SD_A, SAI4_D1, SAI4_MCLK_B,
TIM1_BKIN2_COMP12, FMC_A22,
DCMI_D7/PSSI_D7, LCD_G1,
EVENTOUT
-
B2
C2
VBAT
S
-
-
-
-
A2
A1
PC13
I/O
FT
-
EVENTOUT
RTC_TAMP1/
RTC_TS,
WKUP4
A1
B1
PC14-OSC32_IN
I/O
FT
-
EVENTOUT
OSC32_IN
B1
C1
PC15-OSC32_OUT
I/O
FT
-
EVENTOUT
OSC32_OUT

---

Pinouts, pin descriptions and alternate functions
-
-
C3
PF0
I/O
FT_fh
-
I2C2_SDA(boot), I2C5_SDA,
OCTOSPIM_P2_IO0, FMC_A0,
TIM23_CH1, EVENTOUT
-
-
-
C4
PF1
I/O
FT_fh
-
I2C2_SCL(boot), I2C5_SCL,
OCTOSPIM_P2_IO1, FMC_A1,
TIM23_CH2, EVENTOUT
-
-
-
D4
PF2
I/O
FT_h
-
I2C2_SMBA, I2C5_SMBA,
OCTOSPIM_P2_IO2, FMC_A2,
TIM23_CH3, EVENTOUT
-
-
-
E2
PF3
I/O
FT_ha
-
OCTOSPIM_P2_IO3, FMC_A3,
TIM23_CH4, EVENTOUT
ADC3_INP5
-
-
E3
PF4
I/O
FT_ha
-
OCTOSPIM_P2_CLK, FMC_A4,
EVENTOUT
ADC3_INN5,
ADC3_INP9
-
-
E4
PF5
I/O
FT_ha
-
OCTOSPIM_P2_NCLK, FMC_A5,
EVENTOUT
ADC3_INP4
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
VDD
S
-
-
-
-
-
-
F3
PF6
I/O
FT_ha
-
TIM16_CH1, FDCAN3_RX, SPI5_NSS,
SAI1_SD_B, UART7_RX, SAI4_SD_B,
OCTOSPIM_P1_IO3, TIM23_CH1,
EVENTOUT
ADC3_INN4,
ADC3_INP8
-
-
F2
PF7
I/O
FT_ha
-
TIM17_CH1, FDCAN3_TX, SPI5_SCK,
SAI1_MCLK_B, UART7_TX,
SAI4_MCLK_B, OCTOSPIM_P1_IO2,
TIM23_CH2, EVENTOUT
ADC3_INP3
-
-
G3
PF8
I/O
FT_ha
-
TIM16_CH1N, SPI5_MISO,
SAI1_SCK_B,
UART7_RTS/UART7_DE,
SAI4_SCK_B, TIM13_CH1,
OCTOSPIM_P1_IO0, TIM23_CH3,
EVENTOUT
ADC3_INN3,
ADC3_INP7
-
-
G2
PF9
I/O
FT_ha
-
TIM17_CH1N, SPI5_MOSI,
SAI1_FS_B, UART7_CTS, SAI4_FS_B,
TIM14_CH1, OCTOSPIM_P1_IO1,
TIM23_CH4, EVENTOUT
ADC3_INP2
-
-
G1
PF10
I/O
FT_ha
-
TIM16_BKIN, SAI1_D3, PSSI_D15,
OCTOSPIM_P1_CLK, SAI4_D3,
DCMI_D11/PSSI_D11, LCD_DE,
EVENTOUT
ADC3_INN2,
ADC3_INP6
C1
D1
PH0-OSC_IN
I/O
FT
-
EVENTOUT
OSC_IN
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
D1
E1
PH1-OSC_OUT
I/O
FT
-
EVENTOUT
OSC_OUT
E1
F1
NRST
I/O
RST
-
-
-
F1
H1
PC0
I/O
FT_ha
-
FMC_D12/FMC_AD12,
DFSDM1_CKIN0, DFSDM1_DATIN4,
SAI4_FS_B, FMC_A25,
OTG_HS_ULPI_STP, LCD_G2,
FMC_SDNWE, LCD_R5, EVENTOUT
ADC123_INP10
F2
H2
PC1
I/O
FT_ha
-
TRACED0, SAI4_D1, SAI1_D1,
DFSDM1_DATIN0, DFSDM1_CKIN4,
SPI2_MOSI/I2S2_SDO, SAI1_SD_A,
SAI4_SD_A, SDMMC2_CK,
OCTOSPIM_P1_IO4, ETH_MDC,
MDIOS_MDC, LCD_G5, EVENTOUT
ADC123_INN10,
ADC123_INP11,
RTC_TAMP3,
WKUP6
-
-
-
H3
PC2
I/O
FT_a
-
PWR_DEEPSLEEP, DFSDM1_CKIN1,
OCTOSPIM_P1_IO5,
SPI2_MISO/I2S2_SDI,
DFSDM1_CKOUT,
OCTOSPIM_P1_IO2,
OTG_HS_ULPI_DIR, ETH_MII_TXD2,
FMC_SDNE0, EVENTOUT
ADC123_INN11,
ADC123_INP12
E2
(1)
(1)
(1)
-
PC2_C(2)
AN
A
TT_a
-
-
ADC3_INN1,
ADC3_INP0
-
-
-
H4
PC3
I/O
FT_a
-
PWR_SLEEP, DFSDM1_DATIN1,
OCTOSPIM_P1_IO6,
SPI2_MOSI/I2S2_SDO,
OCTOSPIM_P1_IO0,
OTG_HS_ULPI_NXT,
ETH_MII_TX_CLK, FMC_SDCKE0,
EVENTOUT
ADC12_INN12,
ADC12_INP13
F3
(1)
(1)
(1)
-
PC3_C(2)
AN
A
TT_a
-
-
ADC3_INP1
-
-
-
VDD
S
-
-
-
-
G1
J1
VSSA
S
-
-
-
-
-
-
-
K1
VREF-
S
-
-
-
-
-
L1
VREF+
S
-
-
-
-
H1
M1
VDDA
S
-
-
-
-
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
G2
J2
PA0
I/O
FT_ha
-
TIM2_CH1/TIM2_ETR, TIM5_CH1,
TIM8_ETR, TIM15_BKIN,
SPI6_NSS/I2S6_WS,
USART2_CTS/USART2_NSS,
UART4_TX, SDMMC2_CMD,
SAI4_SD_B, ETH_MII_CRS,
FMC_A19, EVENTOUT
ADC1_INP16,
WKUP1
H2
K2
PA1
I/O
FT_ha
-
TIM2_CH2, TIM5_CH2, LPTIM3_OUT,
TIM15_CH1N,
USART2_RTS/USART2_DE,
UART4_RX, OCTOSPIM_P1_IO3,
SAI4_MCLK_B,
ETH_MII_RX_CLK/ETH_RMII_REF_C
LK, OCTOSPIM_P1_DQS, LCD_R2,
EVENTOUT
ADC1_INN16,
ADC1_INP17
J2
L2
PA2
I/O
FT_ha
-
TIM2_CH3, TIM5_CH3, LPTIM4_OUT,
TIM15_CH1, OCTOSPIM_P1_IO0,
USART2_TX(boot), SAI4_SCK_B,
ETH_MDIO, MDIOS_MDIO, LCD_R1,
EVENTOUT
ADC12_INP14,
WKUP2
K2
M2
PA3
I/O
FT_ha
-
TIM2_CH4, TIM5_CH4, LPTIM5_OUT,
TIM15_CH2, I2S6_MCK,
OCTOSPIM_P1_IO2,
USART2_RX(boot), LCD_B2,
OTG_HS_ULPI_D0, ETH_MII_COL,
OCTOSPIM_P1_CLK, LCD_B5,
EVENTOUT
ADC12_INP15
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
VDD
S
-
-
-
-
G3
J3
PA4
I/O
TT_ha
-
D1PWREN, TIM5_ETR,
SPI1_NSS(boot)/I2S1_WS,
SPI3_NSS/I2S3_WS, USART2_CK,
SPI6_NSS/I2S6_WS,
FMC_D8/FMC_AD8,
DCMI_HSYNC/PSSI_DE,
LCD_VSYNC, EVENTOUT
ADC12_INP18,
DAC1_OUT1
H3
K3
PA5
I/O
TT_ha
-
D2PWREN, TIM2_CH1/TIM2_ETR,
TIM8_CH1N,
SPI1_SCK(boot)/I2S1_CK,
SPI6_SCK/I2S6_CK,
OTG_HS_ULPI_CK,
FMC_D9/FMC_AD9, PSSI_D14,
LCD_R4, EVENTOUT
ADC12_INN18,
ADC12_INP19,
DAC1_OUT2
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
J3
L3
PA6
I/O
FT_ha
-
TIM1_BKIN, TIM3_CH1, TIM8_BKIN,
SPI1_MISO(boot)/I2S1_SDI,
OCTOSPIM_P1_IO3,
SPI6_MISO/I2S6_SDI, TIM13_CH1,
TIM8_BKIN_COMP12, MDIOS_MDC,
TIM1_BKIN_COMP12,
DCMI_PIXCLK/PSSI_PDCK, LCD_G2,
EVENTOUT
ADC12_INP3
K3
M3
PA7
I/O
TT_ha
-
TIM1_CH1N, TIM3_CH2, TIM8_CH1N,
SPI1_MOSI(boot)/I2S1_SDO,
SPI6_MOSI/I2S6_SDO, TIM14_CH1,
OCTOSPIM_P1_IO2,
ETH_MII_RX_DV/ETH_RMII_CRS_DV,
FMC_SDNWE, LCD_VSYNC,
EVENTOUT
ADC12_INN3,
ADC12_INP7,
OPAMP1_VINM
G4
J4
PC4
I/O
TT_ha
-
PWR_DEEPSLEEP, FMC_A22,
DFSDM1_CKIN2, I2S1_MCK,
SPDIFRX1_IN3, SDMMC2_CKIN,
ETH_MII_RXD0/ETH_RMII_RXD0,
FMC_SDNE0, LCD_R7, EVENTOUT
ADC12_INP4,
OPAMP1_VOUT,
COMP1_INM
H4
K4
PC5
I/O
TT_ha
-
PWR_SLEEP, SAI4_D3, SAI1_D3,
DFSDM1_DATIN2, PSSI_D15,
SPDIFRX1_IN4, OCTOSPIM_P1_DQS,
ETH_MII_RXD1/ETH_RMII_RXD1,
FMC_SDCKE0, COMP1_OUT,
LCD_DE, EVENTOUT
ADC12_INN4,
ADC12_INP8,
OPAMP1_VINM
J4
L4
PB0
I/O
TT_ha
-
TIM1_CH2N, TIM3_CH3, TIM8_CH2N,
OCTOSPIM_P1_IO1,
DFSDM1_CKOUT, UART4_CTS,
LCD_R3, OTG_HS_ULPI_D1,
ETH_MII_RXD2, LCD_G1, EVENTOUT
ADC12_INN5,
ADC12_INP9,
OPAMP1_VINP,
COMP1_INP
K4
M4
PB1
I/O
FT_ha
-
TIM1_CH3N, TIM3_CH4, TIM8_CH3N,
OCTOSPIM_P1_IO0,
DFSDM1_DATIN1, LCD_R6,
OTG_HS_ULPI_D2, ETH_MII_RXD3,
LCD_G0, EVENTOUT
ADC12_INP5,
COMP1_INM
G5
J5
PB2
I/O
FT_ha
-
RTC_OUT, SAI4_D1, SAI1_D1,
DFSDM1_CKIN1, SAI1_SD_A,
SPI3_MOSI/I2S3_SDO, SAI4_SD_A,
OCTOSPIM_P1_CLK,
OCTOSPIM_P1_DQS, ETH_TX_ER,
TIM23_ETR, EVENTOUT
COMP1_INP
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
-
-
M5
PF11
I/O
FT_ha
-
SPI5_MOSI, OCTOSPIM_P1_NCLK,
SAI4_SD_B, FMC_NRAS,
DCMI_D12/PSSI_D12, TIM24_CH1,
EVENTOUT
ADC1_INP2
-
-
L5
PF12
I/O
FT_ha
-
OCTOSPIM_P2_DQS, FMC_A6,
TIM24_CH2, EVENTOUT
ADC1_INN2,
ADC1_INP6
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
VDD
S
-
-
-
-
-
-
K5
PF13
I/O
FT_ha
-
DFSDM1_DATIN6, I2C4_SMBA,
FMC_A7, TIM24_CH3, EVENTOUT
ADC2_INP2
-
-
M6
PF14
I/O
FT_fha
-
DFSDM1_CKIN6, I2C4_SCL, FMC_A8,
TIM24_CH4, EVENTOUT
ADC2_INN2,
ADC2_INP6
-
-
L6
PF15
I/O
FT_fh
-
I2C4_SDA, FMC_A9, EVENTOUT
-
-
-
K6
PG0
I/O
FT_h
-
OCTOSPIM_P2_IO4, UART9_RX,
FMC_A10, EVENTOUT
-
-
-
J6
PG1
I/O
TT_h
-
OCTOSPIM_P2_IO5, UART9_TX,
FMC_A11, EVENTOUT
OPAMP2_VINM
H5
M7
PE7
I/O
TT_ha
-
TIM1_ETR, DFSDM1_DATIN2,
UART7_RX, OCTOSPIM_P1_IO4,
FMC_D4/FMC_AD4, EVENTOUT
OPAMP2_VOUT,
COMP2_INM
J5
L7
PE8
I/O
TT_ha
-
TIM1_CH1N, DFSDM1_CKIN2,
UART7_TX, OCTOSPIM_P1_IO5,
FMC_D5/FMC_AD5, COMP2_OUT,
EVENTOUT
OPAMP2_VINM
K5
K7
PE9
I/O
TT_ha
-
TIM1_CH1, DFSDM1_CKOUT,
UART7_RTS/UART7_DE,
OCTOSPIM_P1_IO6,
FMC_D6/FMC_AD6, EVENTOUT
OPAMP2_VINP,
COMP2_INP
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
VDD
S
-
-
-
-
G6
J7
PE10
I/O
FT_ha
-
TIM1_CH2N, DFSDM1_DATIN4,
UART7_CTS, OCTOSPIM_P1_IO7,
FMC_D7/FMC_AD7, EVENTOUT
COMP2_INM
H6
H8
PE11
I/O
FT_ha
-
TIM1_CH2, DFSDM1_CKIN4,
SPI4_NSS(boot), SAI4_SD_B,
OCTOSPIM_P1_NCS,
FMC_D8/FMC_AD8, LCD_G3,
EVENTOUT
COMP2_INP
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
J6
J8
PE12
I/O
FT_h
-
TIM1_CH3N, DFSDM1_DATIN5,
SPI4_SCK(boot), SAI4_SCK_B,
FMC_D9/FMC_AD9, COMP1_OUT,
LCD_B4, EVENTOUT
-
K6
K8
PE13
I/O
FT_h
-
TIM1_CH3, DFSDM1_CKIN5,
SPI4_MISO(boot), SAI4_FS_B,
FMC_D10/FMC_AD10, COMP2_OUT,
LCD_DE, EVENTOUT
-
G7
L8
PE14
I/O
FT_h
-
TIM1_CH4, SPI4_MOSI(boot),
SAI4_MCLK_B, FMC_D11/FMC_AD11,
LCD_CLK, EVENTOUT
-
H7
M8
PE15
I/O
FT_h
-
TIM1_BKIN, USART10_CK,
FMC_D12/FMC_AD12,
TIM1_BKIN_COMP12, LCD_R7,
EVENTOUT
-
J7
M9
PB10
I/O
FT_fh
-
TIM2_CH3, LPTIM2_IN1, I2C2_SCL,
SPI2_SCK/I2S2_CK,
DFSDM1_DATIN7, USART3_TX(boot),
OCTOSPIM_P1_NCS,
OTG_HS_ULPI_D3, ETH_MII_RX_ER,
LCD_G4, EVENTOUT
-
K7
M10
PB11
I/O
FT_f
-
TIM2_CH4, LPTIM2_ETR, I2C2_SDA,
DFSDM1_CKIN7, USART3_RX(boot),
OTG_HS_ULPI_D4,
ETH_MII_TX_EN/ETH_RMII_TX_EN,
LCD_G5, EVENTOUT
-
F8
H7
VCAP
S
-
-
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
VDD
S
-
-
-
-
K8
M11
PB12
I/O
FT_h
-
TIM1_BKIN, OCTOSPIM_P1_NCLK,
I2C2_SMBA, SPI2_NSS/I2S2_WS,
DFSDM1_DATIN1, USART3_CK,
FDCAN2_RX, OTG_HS_ULPI_D5,
ETH_MII_TXD0/ETH_RMII_TXD0,
OCTOSPIM_P1_IO0,
TIM1_BKIN_COMP12, UART5_RX,
EVENTOUT
-
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
J8
M12
PB13
I/O
FT_h
-
TIM1_CH1N, LPTIM2_OUT,
OCTOSPIM_P1_IO2,
SPI2_SCK/I2S2_CK, DFSDM1_CKIN1,
USART3_CTS/USART3_NSS,
FDCAN2_TX, OTG_HS_ULPI_D6,
ETH_MII_TXD1/ETH_RMII_TXD1,
SDMMC1_D0, DCMI_D2/PSSI_D2,
UART5_TX, EVENTOUT
-
H10
L11
PB14
I/O
FT_h
-
TIM1_CH2N, TIM12_CH1,
TIM8_CH2N, USART1_TX,
SPI2_MISO/I2S2_SDI,
DFSDM1_DATIN2,
USART3_RTS/USART3_DE,
UART4_RTS/UART4_DE,
SDMMC2_D0, FMC_D10/FMC_AD10,
LCD_CLK, EVENTOUT
-
G10
L12
PB15
I/O
FT_h
-
RTC_REFIN, TIM1_CH3N,
TIM12_CH2, TIM8_CH3N,
USART1_RX, SPI2_MOSI/I2S2_SDO,
DFSDM1_CKIN2, UART4_CTS,
SDMMC2_D1, FMC_D11/FMC_AD11,
LCD_G7, EVENTOUT
-
K9
L9
PD8
I/O
FT_h
-
DFSDM1_CKIN3, USART3_TX(boot),
SPDIFRX1_IN2,
FMC_D13/FMC_AD13, EVENTOUT
-
J9
K9
PD9
I/O
FT_h
-
DFSDM1_DATIN3, USART3_RX(boot),
FMC_D14/FMC_AD14, EVENTOUT
-
H9
J9
PD10
I/O
FT_h
-
DFSDM1_CKOUT, USART3_CK,
FMC_D15/FMC_AD15, LCD_B3,
EVENTOUT
-
G9
H9
PD11
I/O
FT_h
-
LPTIM2_IN2, I2C4_SMBA,
USART3_CTS/USART3_NSS,
OCTOSPIM_P1_IO0, SAI4_SD_A,
FMC_A16/FMC_CLE, EVENTOUT
-
K10
L10
PD12
I/O
FT_fh
-
LPTIM1_IN1, TIM4_CH1, LPTIM2_IN1,
I2C4_SCL, FDCAN3_RX,
USART3_RTS/USART3_DE,
OCTOSPIM_P1_IO1, SAI4_FS_A,
FMC_A17/FMC_ALE,
DCMI_D12/PSSI_D12, EVENTOUT
-
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
J10
K10
PD13
I/O
FT_fh
-
LPTIM1_OUT, TIM4_CH2, I2C4_SDA,
FDCAN3_TX, OCTOSPIM_P1_IO3,
SAI4_SCK_A,
UART9_RTS/UART9_DE, FMC_A18,
DCMI_D13/PSSI_D13, EVENTOUT
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
VDD
S
-
-
-
-
H8
K11
PD14
I/O
FT_h
-
TIM4_CH3, UART8_CTS, UART9_RX,
FMC_D0/FMC_AD0, EVENTOUT
-
G8
K12
PD15
I/O
FT_h
-
TIM4_CH4, UART8_RTS/UART8_DE,
UART9_TX, FMC_D1/FMC_AD1,
EVENTOUT
-
-
-
J12
PG2
I/O
FT_h
-
TIM8_BKIN, TIM8_BKIN_COMP12,
FMC_A12, TIM24_ETR, EVENTOUT
-
-
-
J11
PG3
I/O
FT_h
-
TIM8_BKIN2, TIM8_BKIN2_COMP12,
FMC_A13, TIM23_ETR, EVENTOUT
-
-
-
J10
PG4
I/O
FT_h
-
TIM1_BKIN2, TIM1_BKIN2_COMP12,
FMC_A14/FMC_BA0, EVENTOUT
-
-
-
H12
PG5
I/O
FT_h
-
TIM1_ETR, FMC_A15/FMC_BA1,
EVENTOUT
-
-
-
H11
PG6
I/O
FT_h
-
TIM17_BKIN, OCTOSPIM_P1_NCS,
FMC_NE3, DCMI_D12/PSSI_D12,
LCD_R7, EVENTOUT
-
-
-
H10
PG7
I/O
FT_h
-
SAI1_MCLK_A, USART6_CK,
OCTOSPIM_P2_DQS, FMC_INT,
DCMI_D13/PSSI_D13, LCD_CLK,
EVENTOUT
-
-
-
G11
PG8
I/O
FT_h
-
TIM8_ETR, SPI6_NSS/I2S6_WS,
USART6_RTS/USART6_DE,
SPDIFRX1_IN3, ETH_PPS_OUT,
FMC_SDCLK, LCD_G7, EVENTOUT
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
F6
-
C11
VDD33USB
S
-
-
-
-
F10
G12
PC6
I/O
FT_h
-
TIM3_CH1, TIM8_CH1,
DFSDM1_CKIN3, I2S2_MCK,
USART6_TX, SDMMC1_D0DIR,
FMC_NWAIT, SDMMC2_D6,
SDMMC1_D6, DCMI_D0/PSSI_D0,
LCD_HSYNC, EVENTOUT
SWPMI_IO
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
E10
F12
PC7
I/O
FT_h
-
DBTRGIO, TIM3_CH2, TIM8_CH2,
DFSDM1_DATIN3, I2S3_MCK,
USART6_RX, SDMMC1_D123DIR,
FMC_NE1, SDMMC2_D7, SWPMI_TX,
SDMMC1_D7, DCMI_D1/PSSI_D1,
LCD_G6, EVENTOUT
-
F9
F11
PC8
I/O
FT_h
-
TRACED1, TIM3_CH3, TIM8_CH3,
USART6_CK,
UART5_RTS/UART5_DE,
FMC_NE2/FMC_NCE, FMC_INT,
SWPMI_RX, SDMMC1_D0,
DCMI_D2/PSSI_D2, EVENTOUT
-
E9
E11
PC9
I/O
FT_fh
-
MCO2, TIM3_CH4, TIM8_CH4,
I2C3_SDA(boot), I2S_CKIN,
I2C5_SDA, UART5_CTS,
OCTOSPIM_P1_IO0, LCD_G3,
SWPMI_SUSPEND, SDMMC1_D1,
DCMI_D3/PSSI_D3, LCD_B2,
EVENTOUT
-
D9
E12
PA8
I/O
FT_fh
-
MCO1, TIM1_CH1, TIM8_BKIN2,
I2C3_SCL(boot), I2C5_SCL,
USART1_CK, OTG_HS_SOF,
UART7_RX, TIM8_BKIN2_COMP12,
LCD_B3, LCD_R6, EVENTOUT
-
C9
D12
PA9
I/O
FT_u
-
TIM1_CH2, LPUART1_TX,
I2C3_SMBA, SPI2_SCK/I2S2_CK,
I2C5_SMBA, USART1_TX(boot),
ETH_TX_ER, DCMI_D0/PSSI_D0,
LCD_R5, EVENTOUT
OTG_HS_VBUS
D10
D11
PA10
I/O
FT_u
-
TIM1_CH3, LPUART1_RX,
USART1_RX(boot), OTG_HS_ID,
MDIOS_MDIO, LCD_B4,
DCMI_D1/PSSI_D1, LCD_B1,
EVENTOUT
-
C10
C12
PA11
I/O
FT_u
-
TIM1_CH4, LPUART1_CTS,
SPI2_NSS/I2S2_WS, UART4_RX,
USART1_CTS/USART1_NSS,
FDCAN1_RX, LCD_R4, EVENTOUT
OTG_HS_DM
(boot)
B10
B12
PA12
I/O
FT_u
-
TIM1_ETR,
LPUART1_RTS/LPUART1_DE,
SPI2_SCK/I2S2_CK, UART4_TX,
USART1_RTS/USART1_DE,
SAI4_FS_B, FDCAN1_TX,
TIM1_BKIN2, LCD_R5, EVENTOUT
OTG_HS_DP
(boot)
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
A10
A12
PA13(JTMS/SWDIO)
I/O
FT
-
JTMS/SWDIO, EVENTOUT
-
E7
G9
VCAP
S
-
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
VDD
S
-
-
-
-
A9
A11
PA14(JTCK/SWCLK)
I/O
FT
-
JTCK/SWCLK, EVENTOUT
-
A8
A10
PA15(JTDI)
I/O
FT
-
JTDI, TIM2_CH1/TIM2_ETR, CEC,
SPI1_NSS/I2S1_WS,
SPI3_NSS(boot)/I2S3_WS,
SPI6_NSS/I2S6_WS,
UART4_RTS/UART4_DE, LCD_R3,
UART7_TX, LCD_B6, EVENTOUT
-
B9
B11
PC10
I/O
FT_fh
-
DFSDM1_CKIN5, I2C5_SDA,
SPI3_SCK(boot)/I2S3_CK,
USART3_TX, UART4_TX,
OCTOSPIM_P1_IO1, LCD_B1,
SWPMI_RX, SDMMC1_D2,
DCMI_D8/PSSI_D8, LCD_R2,
EVENTOUT
-
B8
B10
PC11
I/O
FT_fh
-
DFSDM1_DATIN5, I2C5_SCL,
SPI3_MISO(boot)/I2S3_SDI,
USART3_RX, UART4_RX,
OCTOSPIM_P1_NCS, SDMMC1_D3,
DCMI_D4/PSSI_D4, LCD_B4,
EVENTOUT
-
C8
C10
PC12
I/O
FT_h
-
TRACED3, FMC_D6/FMC_AD6,
TIM15_CH1, I2C5_SMBA,
SPI6_SCK/I2S6_CK,
SPI3_MOSI(boot)/I2S3_SDO,
USART3_CK, UART5_TX,
SDMMC1_CK, DCMI_D9/PSSI_D9,
LCD_R6, EVENTOUT
-
D8
E10
PD0
I/O
FT_h
-
DFSDM1_CKIN6, UART4_RX,
FDCAN1_RX(boot), UART9_CTS,
FMC_D2/FMC_AD2, LCD_B1,
EVENTOUT
-
E8
D10
PD1
I/O
FT_h
-
DFSDM1_DATIN6, UART4_TX,
FDCAN1_TX(boot),
FMC_D3/FMC_AD3, EVENTOUT
-
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
B7
E9
PD2
I/O
FT_h
-
TRACED2, FMC_D7/FMC_AD7,
TIM3_ETR, TIM15_BKIN, UART5_RX,
LCD_B7, SDMMC1_CMD,
DCMI_D11/PSSI_D11, LCD_B2,
EVENTOUT
-
C7
D9
PD3
I/O
FT_h
-
DFSDM1_CKOUT,
SPI2_SCK/I2S2_CK,
USART2_CTS/USART2_NSS,
FMC_CLK, DCMI_D5/PSSI_D5,
LCD_G7, EVENTOUT
-
D7
C9
PD4
I/O
FT_h
-
USART2_RTS/USART2_DE,
OCTOSPIM_P1_IO4, FMC_NOE,
EVENTOUT
-
B6
B9
PD5
I/O
FT_h
-
USART2_TX, OCTOSPIM_P1_IO5,
FMC_NWE, EVENTOUT
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
VDD
S
-
-
-
-
C6
A8
PD6
I/O
FT_h
-
SAI4_D1, SAI1_D1, DFSDM1_CKIN4,
DFSDM1_DATIN1,
SPI3_MOSI/I2S3_SDO, SAI1_SD_A,
USART2_RX, SAI4_SD_A,
OCTOSPIM_P1_IO6, SDMMC2_CK,
FMC_NWAIT, DCMI_D10/PSSI_D10,
LCD_B2, EVENTOUT
-
D6
A9
PD7
I/O
FT_h
-
DFSDM1_DATIN4,
SPI1_MOSI/I2S1_SDO,
DFSDM1_CKIN1, USART2_CK,
SPDIFRX1_IN1, OCTOSPIM_P1_IO7,
SDMMC2_CMD, FMC_NE1,
EVENTOUT
-
-
-
E8
PG9
I/O
FT_h
-
FDCAN3_TX, SPI1_MISO/I2S1_SDI,
USART6_RX, SPDIFRX1_IN4,
OCTOSPIM_P1_IO6, SAI4_FS_B,
SDMMC2_D0, FMC_NE2/FMC_NCE,
DCMI_VSYNC/PSSI_RDY, EVENTOUT
-
-
-
D8
PG10
I/O
FT_h
-
FDCAN3_RX, OCTOSPIM_P2_IO6,
SPI1_NSS/I2S1_WS, LCD_G3,
SAI4_SD_B, SDMMC2_D1, FMC_NE3,
DCMI_D2/PSSI_D2, LCD_B2,
EVENTOUT
-
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
-
-
C8
PG11
I/O
FT_h
-
LPTIM1_IN2, USART10_RX,
SPI1_SCK/I2S1_CK, SPDIFRX1_IN1,
OCTOSPIM_P2_IO7, SDMMC2_D2,
ETH_MII_TX_EN/ETH_RMII_TX_EN,
DCMI_D3/PSSI_D3, LCD_B3,
EVENTOUT
-
-
-
B8
PG12
I/O
FT_h
-
LPTIM1_IN1, OCTOSPIM_P2_NCS,
USART10_TX, SPI6_MISO/I2S6_SDI,
USART6_RTS/USART6_DE,
SPDIFRX1_IN2, LCD_B4,
SDMMC2_D3,
ETH_MII_TXD1/ETH_RMII_TXD1,
FMC_NE4, TIM23_CH1, LCD_B1,
EVENTOUT
-
-
-
D7
PG13
I/O
FT_h
-
TRACED0, LPTIM1_OUT,
USART10_CTS/USART10_NSS,
SPI6_SCK/I2S6_CK,
USART6_CTS/USART6_NSS,
SDMMC2_D6,
ETH_MII_TXD0/ETH_RMII_TXD0,
FMC_A24, TIM23_CH2, LCD_R0,
EVENTOUT
-
-
-
C7
PG14
I/O
FT_h
-
TRACED1, LPTIM1_ETR,
USART10_RTS/USART10_DE,
SPI6_MOSI/I2S6_SDO, USART6_TX,
OCTOSPIM_P1_IO7, SDMMC2_D7,
ETH_MII_TXD1/ETH_RMII_TXD1,
FMC_A25, TIM23_CH3, LCD_B0,
EVENTOUT
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
VDD
S
-
-
-
-
-
-
B7
PG15
I/O
FT_h
-
USART6_CTS/USART6_NSS,
OCTOSPIM_P2_DQS, USART10_CK,
FMC_NCAS, DCMI_D13/PSSI_D13,
EVENTOUT
-
A7
A7
PB3
(JTDO/TRACESWO)
I/O
FT_h
-
JTDO/TRACESWO, TIM2_CH2,
SPI1_SCK/I2S1_CK,
SPI3_SCK/I2S3_CK,
SPI6_SCK/I2S6_CK, SDMMC2_D2,
CRS_SYNC, UART7_RX, TIM24_ETR,
EVENTOUT
-
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
A6
A6
PB4(NJTRST)
I/O
FT_h
-
NJTRST, TIM16_BKIN, TIM3_CH1,
SPI1_MISO/I2S1_SDI,
SPI3_MISO/I2S3_SDI,
SPI2_NSS/I2S2_WS,
SPI6_MISO/I2S6_SDI, SDMMC2_D3,
UART7_TX, EVENTOUT
-
C5
B6
PB5
I/O
FT_h
-
TIM17_BKIN, TIM3_CH2, LCD_B5,
I2C1_SMBA, SPI1_MOSI/I2S1_SDO,
I2C4_SMBA, SPI3_MOSI/I2S3_SDO,
SPI6_MOSI/I2S6_SDO, FDCAN2_RX,
OTG_HS_ULPI_D7, ETH_PPS_OUT,
FMC_SDCKE1, DCMI_D10/PSSI_D10,
UART5_RX, EVENTOUT
-
B5
C6
PB6
I/O
FT_fh
-
TIM16_CH1N, TIM4_CH1,
I2C1_SCL(boot), CEC, I2C4_SCL,
USART1_TX, LPUART1_TX,
FDCAN2_TX, OCTOSPIM_P1_NCS,
DFSDM1_DATIN5, FMC_SDNE1,
DCMI_D5/PSSI_D5, UART5_TX,
EVENTOUT
-
A5
D6
PB7
I/O
FT_fa
-
TIM17_CH1N, TIM4_CH2, I2C1_SDA,
I2C4_SDA, USART1_RX,
LPUART1_RX, DFSDM1_CKIN5,
FMC_NL, DCMI_VSYNC/PSSI_RDY,
EVENTOUT
PVD_IN
D5
D5
BOOT0
I
B
-
-
VPP
B4
C5
PB8
I/O
FT_fh
-
TIM16_CH1, TIM4_CH3,
DFSDM1_CKIN7, I2C1_SCL,
I2C4_SCL, SDMMC1_CKIN,
UART4_RX, FDCAN1_RX,
SDMMC2_D4, ETH_MII_TXD3,
SDMMC1_D4, DCMI_D6/PSSI_D6,
LCD_B6, EVENTOUT
-
A4
B5
PB9
I/O
FT_fh
-
TIM17_CH1, TIM4_CH4,
DFSDM1_DATIN7, I2C1_SDA(boot),
SPI2_NSS/I2S2_WS, I2C4_SDA,
SDMMC1_CDIR, UART4_TX,
FDCAN1_TX, SDMMC2_D5,
I2C4_SMBA, SDMMC1_D5,
DCMI_D7/PSSI_D7, LCD_B7,
EVENTOUT
-
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
D4
A5
PE0
I/O
FT_h
-
LPTIM1_ETR, TIM4_ETR,
LPTIM2_ETR, UART8_RX,
SAI4_MCLK_A, FMC_NBL0,
DCMI_D2/PSSI_D2, LCD_R0,
EVENTOUT
-
C4
A4
PE1
I/O
FT_h
-
LPTIM1_IN2, UART8_TX, FMC_NBL1,
DCMI_D3/PSSI_D3, LCD_R6,
EVENTOUT
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
F7
-
E5
PDR_ON
S
-
-
-
-
-
-
VDD
S
-
-
-
-
C2
-
-
D2
VSS
S
-
-
-
-
E6
-
-
E6
VSS
S
-
-
-
-
J1
-
-
E7
VSS
S
-
-
-
-
E4
-
-
G4
VSS
S
-
-
-
-
E5
-
-
G8
VSS
S
-
-
-
-
-
-
-
G10
VSS
S
-
-
-
-
-
-
-
H5
VSS
S
-
-
-
-
-
-
-
H6
VSS
S
-
-
-
-
D2
-
-
D3
VDD
S
-
-
-
-
F5
-
-
F4
VDD
S
-
-
-
-
K1
-
-
F5
VDD
S
-
-
-
-
F4
-
-
F6
VDD
S
-
-
-
-
-
-
-
F7
VDD
S
-
-
-
-
-
-
-
F8
VDD
S
-
-
-
-
-
-
-
F9
VDD
S
-
-
-
-
-
-
-
F10
VDD
S
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
-
G6
VDD
S
-
-
-
-
-
-
-
G7
VDD
S
-
-
-
-
1.
There is a direct path between Pxy_C and Pxy pins/balls, through an analog switch. Pxy alternate functions are available
on Pxy_C when the analog switch is closed. The analog switch is configured through a SYSCFG register. Refer to the
product reference manual for a detailed description of the switch configuration bits
2.
Pxy_C pins have specific electrical limitations described in Section 6: Electrical characteristics.
Table 7. STM32H723 pin and ball descriptions (continued)
Pin number
Pin name (function
after reset)
Pin type
I/O structure
Notes
Alternate functions
Additional
functions
TFBGA100
LQFP100
LQFP144
UFBGA144

---

Pinouts, pin descriptions and alternate functions
Table 8. STM32H723 pin alternate functions
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS
Port A
PA0
-
TIM2_CH
1/TIM2_
ETR
TIM5_CH1
TIM8_
ETR
TIM15_
BKIN
SPI6_
NSS/I2S
6_WS
-
USART2
_CTS/
USART2
_NSS
UART4_
TX
SDMMC2_
CMD
SAI4_SD_
B
ETH_MII_
CRS
FMC_A19
-
-
EVENT
OUT
PA1
-
TIM2_CH
TIM5_CH2
LPTIM3_
OUT
TIM15_
CH1N
-
-
USART2
_RTS/
USART2
_DE
UART4_
RX
OCTOSPI
M_P1_IO3
SAI4_
MCLK_B
ETH_MII_
RX_CLK/
ETH_RMII_
REF_CLK
OCTOSPI
M_P1_
DQS
-
LCD_
R2
EVENT
OUT
PA2
-
TIM2_CH
TIM5_CH3
LPTIM4_
OUT
TIM15_
CH1
-
OCTOSPI
M_P1_IO0
USART2
_TX
SAI4_SCK
_B
-
-
ETH_MDIO
MDIOS_
MDIO
-
LCD_R
EVENT
OUT
PA3
-
TIM2_CH
TIM5_CH4
LPTIM5_
OUT
TIM15_
CH2
I2S6_
MCK
OCTOSPI
M_P1_IO2
USART2
_RX
-
LCD_B2
OTG_HS_
ULPI_D0
ETH_MII_
COL
OCTOSPI
M_P1_
CLK
-
LCD_B
EVENT
OUT
PA4
D1PWR
EN
-
TIM5_
ETR
-
-
SPI1_
NSS/
I2S1_WS
SPI3_NSS
/I2S3_WS
USART2
_CK
SPI6_NSS
/I2S6_WS
-
-
-
FMC_D8/
FMC_AD8
DCMI_
HSYNC/
PSSI_DE
LCD_
VSYNC
EVENT
OUT
PA5
D2PWR
EN
TIM2_CH
1/TIM2_
ETR
-
TIM8_CH
1N
-
SPI1_
SCK/
I2S1_CK
-
-
SPI6_SCK
/I2S6_CK
-
OTG_HS_
ULPI_CK
-
FMC_D9/
FMC_AD9
PSSI_D1
LCD_R
EVENT
OUT
PA6
-
TIM1_
BKIN
TIM3_CH1
TIM8_
BKIN
-
SPI1_
MISO/
I2S1_SDI
OCTOSPI
M_P1_IO3
-
SPI6_
MISO/I2S6
_SDI
TIM13_CH
TIM8_
BKIN_
COMP12
MDIOS_
MDC
TIM1_
BKIN_
COMP12
DCMI_
PIXCLK/
PSSI_
PDCK
LCD_G
EVENT
OUT
PA7
-
TIM1_CH
1N
TIM3_CH2
TIM8_CH
1N
-
SPI1_
MOSI/I2S
1_SDO
-
-
SPI6_
MOSI/I2S6
_SDO
TIM14_CH
OCTOSPI
M_P1_IO2
ETH_MII_
RX_DV/
ETH_RMII_
CRS_DV
FMC_SDN
WE
-
LCD_
VSYNC
EVENT
OUT

---

Pinouts, pin descriptions and alternate functions
Port A
PA8
MCO1
TIM1_CH
-
TIM8_
BKIN2
I2C3_
SCL
-
I2C5_SCL
USART1
_CK
-
-
OTG_HS_
SOF
UART7_RX
TIM8_
BKIN2_
COMP12
LCD_B3
LCD_R
EVENT
OUT
PA9
-
TIM1_CH
-
LPUART
1_TX
I2C3_
SMBA
SPI2_
SCK/
I2S2_CK
I2C5_
SMBA
USART1
_TX
-
-
-
ETH_TX_
ER
-
DCMI_
D0/PSSI
_D0
LCD_R
EVENT
OUT
PA10
-
TIM1_CH
-
LPUART
1_RX
-
-
-
USART1
_RX
-
-
OTG_HS_
ID
MDIOS_
MDIO
LCD_B4
DCMI_
D1/PSSI
_D1
LCD_B
EVENT
OUT
PA11
-
TIM1_CH
-
LPUART
1_CTS
-
SPI2_
NSS/
I2S2_WS
UART4_
RX
USART1
_CTS/
USART1
_NSS
-
FDCAN1_
RX
-
-
-
-
LCD_R
EVENT
OUT
PA12
-
TIM1_
ETR
-
LPUART
1_RTS/
LPUART
1_DE
-
SPI2_
SCK/
I2S2_CK
UART4_
TX
USART1
_RTS/
USART1
_DE
SAI4_FS_
B
FDCAN1_
TX
-
-
TIM1_
BKIN2
-
LCD_R
EVENT
OUT
PA13
JTMS/
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
EVENT
OUT
PA14
JTCK/
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
EVENT
OUT
PA15
JTDI
TIM2_
CH1/TIM2
_ETR
-
-
CEC
SPI1_
NSS/
I2S1_WS
SPI3_NSS
/I2S3_WS
SPI6_
NSS/
I2S6_WS
UART4_
RTS/
UART4_
DE
LCD_R3
-
UART7_TX
-
-
LCD_B
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

Pinouts, pin descriptions and alternate functions
Port B
PB0
-
TIM1_CH
2N
TIM3_CH3
TIM8_CH
2N
OCTO
SPIM_P1
_IO1
-
DFSDM1_
CKOUT
-
UART4_
CTS
LCD_R3
OTG_HS_
ULPI_D1
ETH_MII_
RXD2
-
-
LCD_G
EVENT
OUT
PB1
-
TIM1_CH
3N
TIM3_CH4
TIM8_CH
3N
OCTO
SPIM_P1
_IO0
-
DFSDM1_
DATIN1
-
-
LCD_R6
OTG_HS_
ULPI_D2
ETH_MII_
RXD3
-
-
LCD_G
EVENT
OUT
PB2
RTC_
OUT
SAI4_D1
SAI1_D1
-
DFSDM1
_CKIN1
-
SAI1_SD_
A
SPI3_
MOSI/I2S
3_SDO
SAI4_SD_
A
OCTO
SPIM_P1_
CLK
OCTO
SPIM_P1_
DQS
ETH_TX_
ER
-
TIM23_
ETR
-
EVENT
OUT
PB3
JTDO/
TRACE
SWO
TIM2_CH
-
-
-
SPI1_
SCK/
I2S1_CK
SPI3_SCK
/I2S3_CK
-
SPI6_SCK
/I2S6_CK
SDMMC2_
D2
CRS_
SYNC
UART7_RX
-
-
TIM24_
ETR
EVENT
OUT
PB4
NJT
RST
TIM16_
BKIN
TIM3_CH1
-
-
SPI1_
MISO/
I2S1_SDI
SPI3_
MISO/
I2S3_SDI
SPI2_
NSS/
I2S2_WS
SPI6_
MISO/
I2S6_SDI
SDMMC2_
D3
-
UART7_TX
-
-
-
EVENT
OUT
PB5
-
TIM17_
BKIN
TIM3_CH2
LCD_B5
I2C1_
SMBA
SPI1_
MOSI/I2S
1_SDO
I2C4_
SMBA
SPI3_
MOSI/I2S
3_SDO
SPI6_
MOSI/I2S6
_SDO
FDCAN2_
RX
OTG_HS_
ULPI_D7
ETH_PPS_
OUT
FMC_SDC
KE1
DCMI_
D10/PSS
I_D10
UART5
_RX
EVENT
OUT
PB6
-
TIM16_
CH1N
TIM4_CH1
-
I2C1_
SCL
CEC
I2C4_SCL
USART1
_TX
LPUART1
_TX
FDCAN2_
TX
OCTO
SPIM_P1_
NCS
DFSDM1_
DATIN5
FMC_SDN
E1
DCMI_
D5/PSSI
_D5
UART5
_TX
EVENT
OUT
PB7
-
TIM17_
CH1N
TIM4_CH2
-
I2C1_
SDA
-
I2C4_SDA
USART1
_RX
LPUART1
_RX
-
-
DFSDM1_
CKIN5
FMC_NL
DCMI_
VSYNC/
PSSI_
RDY
-
EVENT
OUT
PB8
-
TIM16_C
H1
TIM4_CH3
DFSDM1
_CKIN7
I2C1_
SCL
-
I2C4_SCL
SDMMC1
_CKIN
UART4_
RX
FDCAN1_
RX
SDMMC2_
D4
ETH_MII_
TXD3
SDMMC1_
D4
DCMI_
D6/PSSI
_D6
LCD_B
EVENT
OUT
PB9
-
TIM17_
CH1
TIM4_CH4
DFSDM1
_DATIN7
I2C1_
SDA
SPI2_
NSS/I2S
2_WS
I2C4_SDA
SDMMC1
_CDIR
UART4_
TX
FDCAN1_
TX
SDMMC2_
D5
I2C4_
SMBA
SDMMC1_
D5
DCMI_
D7/PSSI
_D7
LCD_B
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

Pinouts, pin descriptions and alternate functions
Port B
PB10
-
TIM2_CH
-
LPTIM2_
IN1
I2C2_
SCL
SPI2_
SCK/
I2S2_CK
DFSDM1_
DATIN7
USART3
_TX
-
OCTO
SPIM_P1_
NCS
OTG_HS_
ULPI_D3
ETH_MII_
RX_ER
-
-
LCD_G
EVENT
OUT
PB11
-
TIM2_CH
-
LPTIM2_
ETR
I2C2_
SDA
-
DFSDM1_
CKIN7
USART3
_RX
-
-
OTG_HS_
ULPI_D4
ETH_MII_
TX_EN/
ETH_RMII_
TX_EN
-
-
LCD_G
EVENT
OUT
PB12
-
TIM1_BKI
N
-
OCTO
SPIM_P1
_NCLK
I2C2_SM
BA
SPI2_
NSS/
I2S2_WS
DFSDM1_
DATIN1
USART3
_CK
-
FDCAN2_
RX
OTG_HS_
ULPI_D5
ETH_MII_
TXD0/
ETH_RMII_
TXD0
OCTOSPI
M_P1_IO0
TIM1_
BKIN_
COMP12
UART5
_RX
EVENT
OUT
PB13
-
TIM1_CH
1N
-
LPTIM2_
OUT
OCTO
SPIM_P1
_IO2
SPI2_
SCK/
I2S2_CK
DFSDM1_
CKIN1
USART3
_CTS/
USART3
_NSS
-
FDCAN2_
TX
OTG_HS_
ULPI_D6
ETH_MII_
TXD1/
ETH_RMII_
TXD1
SDMMC1_
D0
DCMI_
D2/PSSI
_D2
UART5
_TX
EVENT
OUT
PB14
-
TIM1_CH
2N
TIM12_CH
TIM8_CH
2N
USART1
_TX
SPI2_
MISO/
I2S2_SDI
DFSDM1_
DATIN2
USART3
_RTS/
USART3
_DE
UART4_
RTS/UAR
T4_DE
SDMMC2_
D0
-
-
FMC_D10/
FMC_
AD10
-
LCD_C
LK
EVENT
OUT
PB15
RTC_
REFIN
TIM1_CH
3N
TIM12_CH
TIM8_CH
3N
USART1
_RX
SPI2_
MOSI/I2S
2_SDO
DFSDM1_
CKIN2
-
UART4_
CTS
SDMMC2_
D1
-
-
FMC_D11/
FMC_AD1
-
LCD_G
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

Pinouts, pin descriptions and alternate functions
Port C
PC0
-
FMC_D12
/FMC_AD
-
DFSDM1
_CKIN0
-
-
DFSDM1_
DATIN4
-
SAI4_FS_
B
FMC_A25
OTG_HS_
ULPI_STP
LCD_G2
FMC_SDN
WE
-
LCD_R
EVENT
OUT
PC1
TRACE
D0
SAI4_D1
SAI1_D1
DFSDM1
_DATIN0
DFSDM1
_CKIN4
SPI2_
MOSI/I2S
2_SDO
SAI1_SD_
A
-
SAI4_SD_
A
SDMMC2_
CK
OCTO
SPIM_P1_
IO4
ETH_MDC
MDIOS_
MDC
-
LCD_G
EVENT
OUT
PC2
PWR_
DEEP
SLEEP
-
-
DFSDM1
_CKIN1
OCTO
SPIM_P1
_IO5
SPI2_
MISO/I2S
2_SDI
DFSDM1_
CKOUT
-
-
OCTOSPI
M_P1_IO2
OTG_HS_
ULPI_DIR
ETH_MII_
TXD2
FMC_SDN
E0
-
-
EVENT
OUT
PC3
PWR_
SLEEP
-
-
DFSDM1
_DATIN1
OCTO
SPIM_P1
_IO6
SPI2_
MOSI/I2S
2_SDO
-
-
-
OCTOSPI
M_P1_IO0
OTG_HS_
ULPI_NXT
ETH_MII_
TX_CLK
FMC_SDC
KE0
-
-
EVENT
OUT
PC4
PWR_
DEEP
SLEEP
FMC_A22
-
DFSDM1
_CKIN2
-
I2S1_
MCK
-
-
-
SPDIFRX1
_IN3
SDMMC2_
CKIN
ETH_MII_
RXD0/ETH
_RMII_RXD
FMC_SDN
E0
-
LCD_R
EVENT
OUT
PC5
PWR_
SLEEP
SAI4_D3
SAI1_D3
DFSDM1
_DATIN2
PSSI_D1
-
-
-
-
SPDIFRX1
_IN4
OCTOSPI
M_P1_DQ
S
ETH_MII_R
XD1/ETH_
RMII_RXD1
FMC_SDC
KE0
COMP1_
OUT
LCD_D
E
EVENT
OUT
PC6
-
-
TIM3_CH1
TIM8_CH
DFSDM1
_CKIN3
I2S2_
MCK
-
USART6
_TX
SDMMC1_
D0DIR
FMC_
NWAIT
SDMMC2_
D6
-
SDMMC1_
D6
DCMI_
D0/PSSI
_D0
LCD_H
SYNC
EVENT
OUT
PC7
DB
TRGIO
-
TIM3_CH2
TIM8_CH
DFSDM1
_DATIN3
-
I2S3_
MCK
USART6
_RX
SDMMC1_
D123DIR
FMC_NE1
SDMMC2_
D7
SWPMI_TX
SDMMC1_
D7
DCMI_
D1/PSSI
_D1
LCD_G
EVENT
OUT
PC8
TRACE
D1
-
TIM3_CH3
TIM8_CH
-
-
-
USART6
_CK
UART5_
RTS/
UART5_
DE
FMC_NE2
/FMC_
NCE
FMC_INT
SWPMI_RX
SDMMC1_
D0
DCMI_
D2/PSSI
_D2
-
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

Pinouts, pin descriptions and alternate functions
Port C
PC9
MCO2
-
TIM3_CH4
TIM8_CH
I2C3_
SDA
I2S_
CKIN
I2C5_SDA
-
UART5_C
TS
OCTO
SPIM_P1_
IO0
LCD_G3
SWPMI_
SUSPEND
SDMMC1_
D1
DCMI_D
3/PSSI_
D3
LCD_B
EVENT
OUT
PC10
-
-
-
DFSDM1
_CKIN5
I2C5_
SDA
-
SPI3_SCK
/I2S3_CK
USART3
_TX
UART4_
TX
OCTO
SPIM_P1_
IO1
LCD_B1
SWPMI_RX
SDMMC1_
D2
DCMI_D
8/PSSI_
D8
LCD_R
EVENT
OUT
PC11
-
-
-
DFSDM1
_DATIN5
I2C5_
SCL
-
SPI3_
MISO/
I2S3_SDI
USART3
_RX
UART4_
RX
OCTO
SPIM_P1_
NCS
-
-
SDMMC1_
D3
DCMI_
D4/PSSI
_D4
LCD_B
EVENT
OUT
PC12
TRACE
D3
FMC_D6/
FMC_AD6
TIM15_CH
-
I2C5_
SMBA
SPI6_
SCK/
I2S6_CK
SPI3_
MOSI/
I2S3_SDO
USART3
_CK
UART5_
TX
-
-
-
SDMMC1_
CK
DCMI_
D9/PSSI
_D9
LCD_R
EVENT
OUT
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
EVENT
OUT
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
EVENT
OUT
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
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

Pinouts, pin descriptions and alternate functions
Port D
PD0
-
-
-
DFSDM1
_CKIN6
-
-
-
-
UART4_
RX
FDCAN1_
RX
-
UART9_
CTS
FMC_D2/
FMC_AD2
-
LCD_B
EVENT
OUT
PD1
-
-
-
DFSDM1
_DATIN6
-
-
-
-
UART4_
TX
FDCAN1_
TX
-
-
FMC_D3/
FMC_AD3
-
-
EVENT
OUT
PD2
TRACE
D2
FMC_D7/
FMC_AD7
TIM3_
ETR
-
TIM15_
BKIN
-
-
-
UART5_
RX
LCD_B7
-
-
SDMMC1_
CMD
DCMI_
D11/PSSI
_D11
LCD_B
EVENT
OUT
PD3
-
-
-
DFSDM1
_CKOUT
-
SPI2_
SCK/
I2S2_CK
-
USART2
_CTS/
USART2
_NSS
-
-
-
-
FMC_CLK
DCMI_
D5/PSSI
_D5
LCD_G
EVENT
OUT
PD4
-
-
-
-
-
-
-
USART2
_RTS/
USART2
_DE
-
-
OCTOSPI
M_P1_IO4
-
FMC_NOE
-
-
EVENT
OUT
PD5
-
-
-
-
-
-
-
USART2
_TX
-
-
OCTOSPI
M_P1_IO5
-
FMC_NWE
-
-
EVENT
OUT
PD6
-
SAI4_D1
SAI1_D1
DFSDM1
_CKIN4
DFSDM1
_DATIN1
SPI3_
MOSI/I2S
3_SDO
SAI1_SD_
A
USART2
_RX
SAI4_SD_
A
-
OCTO
SPIM_P1_
IO6
SDMMC2_
CK
FMC_
NWAIT
DCMI_D
10/PSSI_
D10
LCD_B
EVENT
OUT
PD7
-
-
-
DFSDM1
_DATIN4
-
SPI1_
MOSI/I2S
1_SDO
DFSDM1_
CKIN1
USART2
_CK
-
SPDIFRX1
_IN1
OCTO
SPIM_P1_
IO7
SDMMC2_
CMD
FMC_NE1
-
-
EVENT
OUT
PD8
-
-
-
DFSDM1
_CKIN3
-
-
-
USART3
_TX
-
SPDIFRX1
_IN2
-
-
FMC_D13/
FMC_
AD13
-
-
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

Pinouts, pin descriptions and alternate functions
Port D
PD9
-
-
-
DFSDM1
_DATIN3
-
-
-
USART3
_RX
-
-
-
-
FMC_D14/
FMC_AD1
-
-
EVENT
OUT
PD10
-
-
-
DFSDM1
_CKOUT
-
-
-
USART3
_CK
-
-
-
-
FMC_D15/
FMC_AD1
-
LCD_B
EVENT
OUT
PD11
-
-
-
LPTIM2_I
N2
I2C4_SM
BA
-
-
USART3
_CTS/
USART3
_NSS
-
OCTOSPI
M_P1_IO0
SAI4_SD_
A
-
FMC_A16/
FMC_CLE
-
-
EVENT
OUT
PD12
-
LPTIM1_
IN1
TIM4_CH1
LPTIM2_
IN1
I2C4_
SCL
FDCAN3
_RX
-
USART3
_RTS/
USART3
_DE
-
OCTO
SPIM_P1_
IO1
SAI4_FS_
A
-
FMC_A17/
FMC_ALE
DCMI_
D12/PSS
I_D12
-
EVENT
OUT
PD13
-
LPTIM1_
OUT
TIM4_CH2
-
I2C4_
SDA
FDCAN3
_TX
-
-
-
OCTO
SPIM_P1_
IO3
SAI4_
SCK_A
UART9_
RTS/
UART9_DE
FMC_A18
DCMI_
D13/
PSSI_
D13
-
EVENT
OUT
PD14
-
-
TIM4_CH3
-
-
-
-
-
UART8_
CTS
-
-
UART9_RX
FMC_D0/
FMC_AD0
-
-
EVENT
OUT
PD15
-
-
TIM4_CH4
-
-
-
-
-
UART8_
RTS/
UART8_
DE
-
-
UART9_TX
FMC_D1/
FMC_AD1
-
-
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

Pinouts, pin descriptions and alternate functions
Port E
PE0
-
LPTIM1_
ETR
TIM4_
ETR
-
LPTIM2_
ETR
-
-
-
UART8_
RX
-
SAI4_
MCLK_A
-
FMC_NBL
DCMI_
D2/PSSI
_D2
LCD_R
EVENT
OUT
PE1
-
LPTIM1_
IN2
-
-
-
-
-
-
UART8_
TX
-
-
-
FMC_NBL
DCMI_
D3/
PSSI_D3
LCD_R
EVENT
OUT
PE2
TRACE
CLK
-
SAI1_
CK1
-
USART1
0_RX
SPI4_
SCK
SAI1_
MCLK_A
-
SAI4_
MCLK_A
OCTOSPI
M_P1_IO2
SAI4_CK1
ETH_MII_
TXD3
FMC_A23
-
-
EVENT
OUT
PE3
TRACE
D0
-
-
-
TIM15_
BKIN
-
SAI1_SD_
B
-
SAI4_SD_
B
-
-
USART10_
TX
FMC_A19
-
-
EVENT
OUT
PE4
TRACE
D1
-
SAI1_D2
DFSDM1
_DATIN3
TIM15_
CH1N
SPI4_NS
S
SAI1_FS_
A
-
SAI4_FS_
A
-
SAI4_D2
-
FMC_A20
DCMI_
D4/PSSI
_D4
LCD_B
EVENT
OUT
PE5
TRACE
D2
-
SAI1_CK2
DFSDM1
_CKIN3
TIM15_
CH1
SPI4_
MISO
SAI1_SCK
_A
-
SAI4_SCK
_A
-
SAI4_CK2
-
FMC_A21
DCMI_
D6/PSSI
_D6
LCD_G
EVENT
OUT
PE6
TRACE
D3
TIM1_
BKIN2
SAI1_D1
-
TIM15_
CH2
SPI4_
MOSI
SAI1_SD_
A
-
SAI4_SD_
A
SAI4_D1
SAI4_
MCLK_B
TIM1_BKIN
2_COMP12
FMC_A22
DCMI_
D7/PSSI
_D7
LCD_G
EVENT
OUT
PE7
-
TIM1_ET
R
-
DFSDM1
_DATIN2
-
-
-
UART7_
RX
-
-
OCTO
SPIM_P1_
IO4
-
FMC_D4/
FMC_AD4
-
-
EVENT
OUT
PE8
-
TIM1_CH
1N
-
DFSDM1
_CKIN2
-
-
-
UART7_
TX
-
-
OCTO
SPIM_P1_
IO5
-
FMC_D5/
FMC_AD5
COMP2_
OUT
-
EVENT
OUT
PE9
-
TIM1_CH
-
DFSDM1
_CKOUT
-
-
-
UART7_
RTS/
UART7_
DE
-
-
OCTO
SPIM_P1_
IO6
-
FMC_D6/
FMC_AD6
-
-
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

Pinouts, pin descriptions and alternate functions
Port E
PE10
-
TIM1_CH
2N
-
DFSDM1
_DATIN4
-
-
-
UART7_
CTS
-
-
OCTO
SPIM_P1_
IO7
-
FMC_D7/
FMC_AD7
-
-
EVENT
OUT
PE11
-
TIM1_CH
-
DFSDM1
_CKIN4
-
SPI4_
NSS
-
-
-
-
SAI4_SD_
B
OCTO
SPIM_P1_
NCS
FMC_D8/
FMC_AD8
-
LCD_G
EVENT
OUT
PE12
-
TIM1_CH
3N
-
DFSDM1
_DATIN5
-
SPI4_
SCK
-
-
-
-
SAI4_SCK
_B
-
FMC_D9/
FMC_AD9
COMP1_
OUT
LCD_B
EVENT
OUT
PE13
-
TIM1_CH
-
DFSDM1
_CKIN5
-
SPI4_
MISO
-
-
-
-
SAI4_FS_
B
-
FMC_D10/
FMC_
AD10
COMP2_
OUT
LCD_
DE
EVENT
OUT
PE14
-
TIM1_CH
-
-
-
SPI4_
MOSI
-
-
-
-
SAI4_
MCLK_B
-
FMC_D11/
FMC_
AD11
-
LCD_
CLK
EVENT
OUT
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
USART10_
CK
FMC_D12/
FMC_
AD12
TIM1_
BKIN_
COMP12
LCD_
R7
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

Pinouts, pin descriptions and alternate functions
Port F
PF0
-
-
-
-
I2C2_
SDA
-
I2C5_SDA
-
-
OCTO
SPIM_P2_
IO0
-
-
FMC_A0
TIM23_
CH1
-
EVENT
OUT
PF1
-
-
-
-
I2C2_
SCL
-
I2C5_SCL
-
-
OCTO
SPIM_P2_
IO1
-
-
FMC_A1
TIM23_
CH2
-
EVENT
OUT
PF2
-
-
-
-
I2C2_
SMBA
-
I2C5_
SMBA
-
-
OCTO
SPIM_P2_
IO2
-
-
FMC_A2
TIM23_
CH3
-
EVENT
OUT
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
OCTO
SPIM_P2_
IO3
-
-
FMC_A3
TIM23_
CH4
-
EVENT
OUT
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
OCTO
SPIM_P2_
CLK
-
-
FMC_A4
-
-
EVENT
OUT
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
OCTO
SPIM_P2_
NCLK
-
-
FMC_A5
-
-
EVENT
OUT
PF6
-
TIM16_
CH1
FDCAN3_
RX
-
-
SPI5_
NSS
SAI1_SD_
B
UART7_
RX
SAI4_SD_
B
-
OCTO
SPIM_P1_
IO3
-
-
TIM23_
CH1
-
EVENT
OUT
PF7
-
TIM17_
CH1
FDCAN3_
TX
-
-
SPI5_
SCK
SAI1_
MCLK_B
UART7_
TX
SAI4_
MCLK_B
-
OCTO
SPIM_P1_
IO2
-
-
TIM23_
CH2
-
EVENT
OUT
PF8
-
TIM16_
CH1N
-
-
-
SPI5_
MISO
SAI1_SCK
_B
UART7_
RTS/
UART7_
DE
SAI4_SCK
_B
TIM13_CH
OCTO
SPIM_P1_
IO0
-
-
TIM23_
CH3
-
EVENT
OUT
PF9
-
TIM17_
CH1N
-
-
-
SPI5_
MOSI
SAI1_FS_
B
UART7_
CTS
SAI4_FS_
B
TIM14_CH
OCTO
SPIM_P1_
IO1
-
-
TIM23_
CH4
-
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

Pinouts, pin descriptions and alternate functions
Port F
PF10
-
TIM16_BK
IN
SAI1_D3
-
PSSI_
D15
-
-
-
-
OCTO
SPIM_P1_
CLK
SAI4_D3
-
-
DCMI_
D11/PSSI
_D11
LCD_D
E
EVENT
OUT
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
OCTO
SPIM_P1_
NCLK
SAI4_SD_
B
-
FMC_
NRAS
DCMI_
D12/PSS
I_D12
TIM24_
CH1
EVENT
OUT
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
OCTO
SPIM_P2_
DQS
-
-
FMC_A6
-
TIM24_
CH2
EVENT
OUT
PF13
-
-
-
DFSDM1
_DATIN6
I2C4_
SMBA
-
-
-
-
-
-
-
FMC_A7
-
TIM24_
CH3
EVENT
OUT
PF14
-
-
-
DFSDM1
_CKIN6
I2C4_
SCL
-
-
-
-
-
-
-
FMC_A8
-
TIM24_
CH4
EVENT
OUT
PF15
-
-
-
-
I2C4_
SDA
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
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

Pinouts, pin descriptions and alternate functions
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
OCTO
SPIM_P2_
IO4
-
UART9_RX
FMC_A10
-
-
EVENT
OUT
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
OCTO
SPIM_P2_
IO5
-
UART9_TX
FMC_A11
-
-
EVENT
OUT
PG2
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
TIM8_BKIN
_COMP12
FMC_A12
-
TIM24_
ETR
EVENT
OUT
PG3
-
-
-
TIM8_
BKIN2
-
-
-
-
-
-
-
TIM8_
BKIN2_
COMP12
FMC_A13
TIM23_
ETR
-
EVENT
OUT
PG4
-
TIM1_BKI
N2
-
-
-
-
-
-
-
-
-
TIM1_
BKIN2_
COMP12
FMC_A14/
FMC_BA0
-
-
EVENT
OUT
PG5
-
TIM1_
ETR
-
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
EVENT
OUT
PG6
-
TIM17_
BKIN
-
-
-
-
-
-
-
-
OCTO
SPIM_P1_
NCS
-
FMC_NE3
DCMI_D
12/PSSI_
D12
LCD_R
EVENT
OUT
PG7
-
-
-
-
-
-
SAI1_
MCLK_A
USART6
_CK
-
OCTO
SPIM_P2_
DQS
-
-
FMC_INT
DCMI_D
13/PSSI_
D13
LCD_
CLK
EVENT
OUT
PG8
-
-
-
TIM8_
ETR
-
SPI6_
NSS/I2S
6_WS
-
USART6
_RTS/
USART6
_DE
SPDIFRX1
_IN3
-
-
ETH_PPS_
OUT
FMC_
SDCLK
-
LCD_G
EVENT
OUT
PG9
-
-
FDCAN3_
TX
-
-
SPI1_
MISO/I2S
1_SDI
-
USART6
_RX
SPDIFRX1
_IN4
OCTO
SPIM_P1_
IO6
SAI4_FS_
B
SDMMC2_
D0
FMC_NE2/
FMC_NCE
DCMI_
VSYNC/
PSSI_
RDY
-
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

Pinouts, pin descriptions and alternate functions
Port G
PG10
-
-
FDCAN3_
RX
OCTO
SPIM_P2
_IO6
-
SPI1_
NSS/I2S
1_WS
-
-
-
LCD_G3
SAI4_SD_
B
SDMMC2_
D1
FMC_NE3
DCMI_
D2/PSSI
_D2
LCD_B
EVENT
OUT
PG11
-
LPTIM1_
IN2
-
-
USART1
0_RX
SPI1_
SCK/I2S
1_CK
-
-
SPDIFRX1
_IN1
OCTO
SPIM_P2_
IO7
SDMMC2_
D2
ETH_MII_
TX_EN/
ETH_RMII_
TX_EN
-
DCMI_
D3/PSSI
_D3
LCD_B
EVENT
OUT
PG12
-
LPTIM1_
IN1
-
OCTO
SPIM_P2
_NCS
USART1
0_TX
SPI6_
MISO/I2S
6_SDI
-
USART6
_RTS/
USART6
_DE
SPDIFRX1
_IN2
LCD_B4
SDMMC2_
D3
ETH_MII_
TXD1/ETH
_RMII_TXD
FMC_NE4
TIM23_
CH1
LCD_B
EVENT
OUT
PG13
TRACE
D0
LPTIM1_
OUT
-
-
USART1
0_CTS/
USART1
0_NSS
SPI6_
SCK/I2S
6_CK
-
USART6
_CTS/
USART6
_NSS
-
-
SDMMC2_
D6
ETH_MII_
TXD0/ETH
_RMII_TXD
FMC_A24
TIM23_
CH2
LCD_R
EVENT
OUT
PG14
TRACE
D1
LPTIM1_
ETR
-
-
USART1
0_RTS/
USART1
0_DE
SPI6_
MOSI/I2S
6_SDO
-
USART6
_TX
-
OCTO
SPIM_P1_
IO7
SDMMC2_
D7
ETH_MII_
TXD1/ETH
_RMII_TXD
FMC_A25
TIM23_
CH3
LCD_B
EVENT
OUT
PG15
-
-
-
-
-
-
-
USART6
_CTS/
USART6
_NSS
-
OCTO
SPIM_P2_
DQS
-
USART10_
CK
FMC_NCA
S
DCMI_D
13/PSSI_
D13
-
EVENT
OUT
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
EVENT
OUT
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
EVENT
OUT
Table 8. STM32H723 pin alternate functions (continued)
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
FMC/
LPTIM1/
SAI4/TIM1
6/17/TIM1
x/TIM2x
FDCAN3/
PDM_
SAI1/
TIM3/4/5/1
DFSDM1
/LCD/
LPTIM2/
3/4/5/
LPUART
1/OCTO
SPIM_P1
/2/TIM8
CEC/
DCMI/
PSSI/
DFSDM1
/I2C1/2/3/
4/5/
LPTIM2/
OCTO
SPIM_P1
/TIM15/
USART1/
CEC/
FDCAN3/
SPI1/I2S
1/SPI2/
I2S2/SPI
3/I2S3/
SPI4/5/6
DFSDM1/I
2C4/5/
OCTO
SPIM_P1/
SAI1/
SPI3/
I2S3/
UART4
SDMMC1
/SPI2/I2S
2/SPI3/
I2S3/
SPI6/
UART7/
USART1/
2/3/6
LPUART1/
SAI4/
SDMMC1/
SPDIFRX1
/SPI6/
UART4/5/
FDCAN1/2
/FMC/
LCD/
OCTO
SPIM_P1/
2/SAI4/
SDMMC2/
SPDIFRX1
/TIM13/14
CRS/
FMC/
LCD/
OCTO
SPIM_P1/
OTG1_FS/
OTG1_HS/
SAI4/
SDMMC2/
TIM8
DFSDM1/
ETH/I2C4/
LCD/MDIO
S/OCTOSP
IM_P1/
SDMMC2/
SWPMI1/
TIM1x/TIM
8/UART7/9/
USART10
FMC/LCD/
MDIOS/
OCTOSPI
M_P1/
SDMMC1/
TIM1x/
TIM8
COMP/
DCMI/
PSSI/
LCD/
TIM1x/
TIM23
LCD/
TIM24/
UART5
SYS

---

6.1
Parameter conditions
Unless otherwise specified, all voltages are referenced to VSS.
6.1.1
Minimum and maximum values
Unless otherwise specified the minimum and maximum values are guaranteed in the worst
conditions of junction temperature, supply voltage and frequencies by tests in production on
100% of the devices with a junction temperature at TJ = 25 °C and TJ = TJmax (given by the
selected temperature range).
Data based on characterization results, design simulation and/or technology characteristics
are indicated in the table footnotes. Based on characterization, the minimum and maximum
values refer to sample tests and represent the mean value plus or minus three times the
standard deviation (mean±3σ).
6.1.2
Typical values
Unless otherwise specified, typical data are based on TJ = 25 °C, VDD = 3.3 V (for the
1.7 V ≤ VDD ≤ 3.6 V voltage range). They are given only as design guidelines and are not
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
The loading conditions used for pin parameter measurement are shown in Figure 8.
6.1.5
Pin input voltage
The input voltage measurement on a pin of the device is described in Figure 9.
Figure 8. Pin loading conditions
Figure 9. Pin input voltage
MS19011V2
C = 50 pF
MCU pin
MS19010V2
MCU pin
VIN

---

6.1.6
Power supply scheme
Figure 10. Power supply scheme
1.
Refer to application note AN5419 “Getting started with STM32H723/733, STM32H725/735 and
STM32H730 Value Line hardware development“ for the possible power scheme and connected capacitors.
MSv65399V2
BKUP
IOs
VDD domain
Analog domain
Core domain (VCORE)
Backup domain
D3 domain
(System
logic,
EXTI,
Peripherals,
RAM)
D1 domain
(CPU, peripherals,
RAM)
Level shifter
OPAMP,
Comparator
ADC, DAC
Flash
D2 domain
(peripherals,
RAM)
Power
switch
Power switch
VCAP
VSS
VDDLDO
VBAT
VDDA
VREF+
VREF-
VSSA
Backup
regulator
VDD
Backup
RAM
Power
switch
LSI, HSI,
CSI, HSI48,
HSE, PLLs
IOs
Power
switch
VSS
VSS
REF_BUF
VSS
IO
logic
VREF+
VSW
LSE, RTC,
Wakeup logic,
backup
registers, Reset
IO
logic
VBKP
VBAT
charging
VREF-
VDD33USB
USB
FS IOs
LDO
voltage
regulator

---

6.1.7
Current consumption measurement
Figure 11. Current consumption measurement scheme
6.2
Absolute maximum ratings
Stresses above the absolute maximum ratings listed in Table 9: Voltage characteristics,
Table 10: Current characteristics, and Table 11: Thermal characteristics may cause
permanent damage to the device. These are stress ratings only and the functional operation
of the device at these conditions is not implied. Exposure to maximum rating conditions for
extended periods may affect device reliability. Device mission profile (application conditions)
is compliant with JEDEC JESD47 Qualification Standard, extended mission profiles are
available on demand.
Note:
For information on product lifetime estimation, refer to application note AN5337: Guidelines
for estimating STM32H7 MCUs lifetime, available from the STMicroelectronics website
www.st.com.
IDD_VBAT
LDO ON
VBAT
IDD
VDD
VDDA
VDDLDO
Table 9. Voltage characteristics
Symbols
Ratings
Min
Max
Unit
VDDX - VSS
(1)
External main supply voltage (including VDD,
VDDLDO, VDDA, VDD33USB, VBAT)
−0.3
4.0
V
VIN
(2)
Input voltage on FT_xxx pins
VSS−0.3
Min(Min(VDD,
VDDA, VDD33USB,
VBAT) + 4.0 , 6 V)
(3)(4)(5)
V
Input voltage on TT_xx pins
VSS−0.3
4.0
V
Input voltage on BOOT0 pin
VSS
9.0
V
Input voltage on any other pins
VSS-0.3
4.0
V
|∆VDDX|
Variations between different VDDX power
pins of the same domain
-
mV
|VSSx-VSS|
Variations between all the different ground
pins
-
mV

---

1.
All main power (VDD, VDDA, VDD33USB, VBAT) and ground (VSS, VSSA) pins must always be connected to
the external power supply, in the permitted range.
2.
VIN maximum must always be respected. Refer to Table 50: I/O current injection susceptibility for the
maximum allowed injected current values.
3.
This formula has to be applied on power supplies related to the IO structure described by the pin definition
table.
4.
To sustain a voltage higher than 4 V the internal pull-up/pull-down resistors must be disabled.
5.
When an FT_a pin is used by an analog peripheral such as ADC, the maximum VIN is 4 V.
Table 10. Current characteristics
Symbols
Ratings
Max
Unit
ΣIVDD
Total current into sum of all VDD power lines (source)(1)
1.
All main power (VDD, VDDA, VDD33USB) and ground (VSS, VSSA) pins must always be connected to the
external power supplies, in the permitted range.
mA
ΣIVSS
Total current out of sum of all VSS ground lines (sink)(1)
IVDD
Maximum current into each VDD power pin (source)(1)
IVSS
Maximum current out of each VSS ground pin (sink)(1)
IIO
Output current sunk or sourced by any I/O and control pin, except
Pxy_C
Output current sunk or sourced by Pxy_C pins
ΣI(PIN)
Total output current sunk by sum of all I/Os and control pins(2)
2.
This current consumption must be correctly distributed over all I/Os and control pins. The total output
current must not be sunk/sourced between two consecutive power supply pins referring to high pin count
QFP packages.
Total output current sourced by sum of all I/Os and control pins(2)
IINJ(PIN)
(3)(4)
3.
Positive injection is not possible on these I/Os and does not occur for input voltages lower than the
specified maximum value.
4.
A positive injection is induced by VIN>VDD while a negative injection is induced by VIN<VSS. IINJ(PIN) must
never be exceeded. Refer also to Table 9: Voltage characteristics for the maximum allowed input voltage
values.
Injected current on FT_xxx, TT_xx, RST and B pins except PA4,
PA5
−5/+0
Injected current on PA4, PA5
−0/0
ΣIINJ(PIN)
Total injected current (sum of all I/Os and control pins)(5)
5.
When several inputs are submitted to a current injection, the maximum ∑IINJ(PIN) is the absolute sum of the
positive and negative injected currents (instantaneous values).
±25
Table 11. Thermal characteristics
Symbol
Ratings
 Value
Unit
TSTG
Storage temperature range
−65 to +150
°C
TJ
Maximum junction
temperature
Industrial temperature range 6

---

6.3
Operating conditions
6.3.1
General operating conditions
Table 12. General operating conditions
Symbol
Parameter
Operating
conditions
Min
Typ
Max
Unit
VDD
Standard operating voltage
-
1.62(1)
-
3.6
V
VDDLDO
Supply voltage for the internal
regulator
VDDLDO ≤ VDD
1.62(1)
-
3.6
VDD33USB
Standard operating voltage,
USB domain
USB used
3.0
-
3.6
USB not used
-
3.6
VDDA
Analog operating voltage
ADC or COMP used
1.62
-
3.6
DAC used
1.8
-
OPAMP used
2.0
-
VREFBUF used
1.8
-
ADC, DAC, OPAMP,
COMP, VREFBUF not
used
-
VBAT
Supply voltage for Backup
domain
-
1.2(2)
-
3.6
VIN
I/O Input voltage
TT_xx I/O except
Pxy_C
−0.3
-
VDD+0.3
Pxy_C I/O
−0.3
-
Min(VDDA,
VDD) + 0.3
BOOT0
-
All I/Os except
BOOT0, TT_xx and
Pxy_C
−0.3
-
Min(VDD, VDDA,
VDD33USB) + 3.
6 < 5.5(3)
VCORE
Internal regulator ON (LDO)(4)
VOS3
 0.95
1.0
 1.05
V
VOS2
 1.05
 1.10
 1.15
VOS1
 1.15
 1.21
 1.26
VOS0
 1.30
 1.36
 1.40
Regulator OFF: external VCORE
voltage must be supplied from
external regulator on VCAP pins
VOS3
 0.98
 1.03
 1.08
VOS2
 1.08
 1.13
 1.18
VOS1
 1.18
 1.23
 1.28
VOS0
 1.33
 1.38
 1.40

---

fCPU
Arm® Cortex®-M7 clock
frequency
VOS3
-
-
MHz
VOS2
-
-
VOS1
-
-
VOS0
-
-
VOS0 and
CPU_FREQ_BOOST
-
-
fACLK
AXI clock frequency
VOS3
-
-
VOS2
-
-
VOS1
-
-
VOS0
-
-
fHCLK
AHB clock frequency
VOS3
-
-
VOS2
-
-
VOS1
-
-
VOS0
-
-
fPCLK
APB clock frequency
VOS3
-
-
42.5(5)
VOS2
-
-
VOS1
-
-
VOS0
-
-
137.5
TA
(6)
Ambient temperature for
temperature range 3
Maximum power
dissipation
−40
°C
Ambient temperature for
temperature range 6
Maximum power
dissipation
−40
Low-power
dissipation(7)
−40
1.
When RESET is released, the functionality is guaranteed down to VPDRmax or down to the specified VDDmin when the PDR is
OFF. The PDR can only be switched OFF though the PDR_ON pin that not available in all packages.
2.
VBAT minimum value can be reduced to 0 V if VDD is present.
3.
This formula has to be applied on power supplies related to the I/O structure described by the pin definition table.
4.
At startup, the external VCORE voltage must remain higher or equal to 1.10 V before disabling the internal regulator (LDO).
5.
This value corresponds to the maximum APB clock frequency when at least one peripheral is enabled.
6.
The device junction temperature must be kept below maximum TJ indicated in Table 13: Supply voltage and maximum
temperature configuration and the maximum temperature.
7.
In low-power dissipation state, TA can be extended to this range as long as TJ does not exceed TJmax (see Section 7.6:
Thermal characteristics).
Table 12. General operating conditions (continued)
Symbol
Parameter
Operating
conditions
Min
Typ
Max
Unit

---

6.3.2
VCAP external capacitor
Stabilization for the main regulator is achieved by connecting an external capacitor CEXT to
the VCAP pin. CEXT is specified in Table 14. Two external capacitors can be connected to
VCAP pins.
Figure 12. External capacitor CEXT
1.
Legend: ESR is the equivalent series resistance.
Table 13. Supply voltage and maximum temperature configuration
Power scale
VCORE source
Max. TJ (°C)
Min. VDD(V)
Min. VDDLDO (V)
VOS0
LDO
1.7
1.7
External (Bypass)
1.62
-
VOS1
LDO
1.62
1.62
External (Bypass)
-
-
VOS2 or VOS3
LDO
1.62
1.62
External (Bypass)
-
-
SVOS4/SVOS5
LDO
1.62
1.62
External (Bypass)
1.62
-
Table 14. VCAP operating conditions(1)
1.
When bypassing the voltage regulator, the two 2.2 µF VCAP capacitors are not required and should be
replaced by two 100 nF decoupling capacitors.
Symbol
Parameter
Conditions
CEXT
Capacitance of external capacitor
2.2 µF(2)(3)
2.
This value corresponds to CEXT typical value. A variation of +/-20% is tolerated.
3.
If a third VCAP pin is available on the package, it must be connected to the other VCAP pins but no
additional capacitor is required.
ESR
ESR of external capacitor
< 100 mΩ
MS19044V2
ESR
R Leak
C

---

6.3.3
Operating conditions at power-up / power-down
Subject to general operating conditions for TA.
Table 15. Operating conditions at power-up/power-down
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
tVDDA
VDDA rise time rate
∞
VDDA fall time rate
∞
tVDDUSB
VDDUSB rise time rate
∞
VDDUSB fall time rate
∞
tVCORE
(1)
1.
tVCORE should be achieved when VCORE is provided by an external supply voltage (bypass with
VDDLDO = VCORE).
VCORE rise time rate(2)
2.
VCORE rising slope must respect the above constraints. There are no constraints on the delay between VDD
rising and VCORE rising.
VCORE fall time rate
∞

---

6.3.4
Embedded reset and power control block characteristics
The parameters given in Table 16 are derived from tests performed under ambient
temperature and VDD supply voltage conditions summarized in Table 12: General operating
conditions.
Table 16. Reset and power control block characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
tRSTTEMPO
(1)
Reset temporization
after BOR0 released
-
-
µs
VBOR0/POR/PDR
Power-on/power-down reset
threshold
Rising edge(1)
1.62
1.67
1.71
V
   Falling edge
1.58
1.62
1.68
VBOR1
Brown-out reset threshold 1
Rising edge
2.04
2.10
2.15
   Falling edge
1.95
2.00
2.06
VBOR2
Brown-out reset threshold 2
Rising edge
2.34
2.41
2.47
   Falling edge
2.25
2.31
2.37
VBOR3
Brown-out reset threshold 3
Rising edge
2.63
2.70
2.78
   Falling edge
2.54
2.61
2.68
VPVD0
Programmable Voltage
Detector threshold 0
Rising edge
1.90
1.96
2.01
   Falling edge
1.81
1.86
1.91
VPVD1
Programmable Voltage
Detector threshold 1
Rising edge
2.05
2.10
2.16
   Falling edge
1.96
2.01
2.06
VPVD2
Programmable Voltage
Detector threshold 2
Rising edge
2.19
2.26
2.32
   Falling edge
2.10
2.15
2.21
VPVD3
Programmable Voltage
Detector threshold 3
Rising edge
2.35
2.41
2.47
   Falling edge
2.25
2.31
2.37
VPVD4
Programmable Voltage
Detector threshold 4
Rising edge
2.49
2.56
2.62
   Falling edge
2.39
2.45
2.51
VPVD5
Programmable Voltage
Detector threshold 5
Rising edge
2.64
2.71
2.78
   Falling edge
2.55
2.61
2.68
VPVD6
Programmable Voltage
Detector threshold 6
Rising edge
2.78
2.86
2.94
   Falling edge in Run mode
2.69
2.76
2.83
Vhyst_POR_PDR
Hysteresis voltage for
Power-on/power-down reset
(including BOR0)
Hysteresis in Run mode
-
43.00
-
mV
Vhyst_BOR_PVD
Hysteresis voltage for BOR
(except BOR0)
Hysteresis in Run mode
-
-
IDD_BOR_PVD
(1)
BOR and PVD consumption
from VDD
-
-
-
0.630
µA
IDD_POR_PVD
POR and PVD consumption
from VDD
-
0.8
-
1.200

---

6.3.5
Embedded reference voltage characteristics
The parameters given in Table 17 are derived from tests performed under ambient
temperature and VDD supply voltage conditions summarized in Table 12: General operating
conditions.
VAVM_0
Analog voltage detector for
VDDA threshold 0
Rising edge
1.66
1.71
1.76
V
   Falling edge
1.56
1.61
1.66
VAVM_1
Analog voltage detector for
VDDA threshold 1
Rising edge
2.06
2.12
2.19
   Falling edge
1.96
2.02
2.08
VAVM_2
Analog voltage detector for
VDDA threshold 2
Rising edge
2.42
2.50
2.58
   Falling edge
2.35
2.42
2.49
VAVM_3
Analog voltage detector for
VDDA threshold 3
Rising edge
2.74
2.83
2.91
   Falling edge
2.64
2.72
2.80
Vhyst_VDDA
Hysteresis of VDDA voltage
detector
-
-
-
mV
IDD_PVM
PVM consumption from
VDD(1)
-
-
-
0.25
µA
IDD_VDDA
Voltage detector
consumption on VDDA
(1)
   Resistor bridge
-
-
2.5
µA
1.
Guaranteed by design.
Table 16. Reset and power control block characteristics (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Table 17. Embedded reference voltage
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
VREFINT
Internal reference voltages
-40°C < TJ < TJmax
1.180
1.216
1.255
V
tS_vrefint
(1)(2)
(3)
ADC sampling time when
reading the internal reference
voltage
-
4.3
-
-
µs
tS_vbat
(2)
VBAT sampling time when
reading the internal VBAT
reference voltage
-
-
-
tstart_vrefint
(2)
Start time of reference voltage
buffer when ADC is enable
-
-
-
4.4
Irefbuf
(2)
Reference Buffer
consumption for ADC
VDD = 3.3 V
13.5
µA
ΔVREFINT
(2)
Internal reference voltage
spread over the temperature
range
-40°C < TJ < TJmax
-
mV
Tcoeff
(2)
Average temperature
coefficient
Average temperature
coefficient
-
ppm/°C
VDDcoeff
(2)
Average Voltage coefficient
3.0 V < VDD < 3.6 V
-
ppm/V

---

6.3.6
Embedded USB regulator characteristics
The parameters given in Table 19 are derived from tests performed under ambient
temperature and VDD supply voltage conditions summarized in Table 12: General operating
conditions.
6.3.7
Supply current characteristics
The current consumption is a function of several parameters and factors such as the
operating voltage, ambient temperature, I/O pin loading, device software configuration,
operating frequencies, I/O pin switching rate, program location in memory and executed
binary code.
The current consumption is measured as described in Figure 11: Current consumption
measurement scheme.
All the Run-mode current consumption measurements given in this section are performed
with a CoreMark code.
VREFINT_DIV1
1/4 reference voltage
-
-
-
%
VREFINT
VREFINT_DIV2
1/2 reference voltage
-
-
-
VREFINT_DIV3
3/4 reference voltage
-
-
-
1.
 The shortest sampling time for the application can be determined by multiple iterations.
2.
Guaranteed by design.
3.
Guaranteed by design. and tested in production at 3.3 V.
Table 17. Embedded reference voltage (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Table 18. Internal reference voltage calibration values
Symbol
Parameter
Memory address
VREFIN_CAL
Raw data acquired at temperature of 30 °C, VDDA = 3.3 V
1FF1 E860 - 1FF1 E861
Table 19. USB regulator characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
VREGOUTV33V
Regulated output voltage
-
-
3.6
V
IOUT
Output current load sinked by
USB block
-
-
-
mA
TWKUP
Wakeup time
-
-
us

---

Typical and maximum current consumption
The MCU is placed under the following conditions:
•
All I/O pins are in analog input mode.
•
All peripherals are disabled except when explicitly mentioned.
•
The flash memory access time is adjusted with the minimum wait states number,
depending on the fACLK frequency (refer to the table “Number of wait states according to
CPU clock (frcc_c_ck) frequency and VCORE range” available in the reference manual).
•
When the peripherals are enabled, the AHB clock frequency is the CPU frequency
divided by 2 and the APB clock frequency is AHB clock frequency divided by 2.
•
For typical values, the power supply is 3 V unless otherwise specified.
The parameters given in the below tables are derived from tests performed at supply
voltage conditions summarized in Table 12: General operating conditions, and at ambient
temperature unless otherwise specified.

---

Table 20. Typical and maximum current consumption in Run mode,
 code with data processing running from ITCM(1)
Symbol
Parameter
Conditions
frcc_c_ck
(MHz)
Typ
Max(2)
Unit
TJ =
25 °C
TJ =
85 °C
TJ =
105 °C
TJ =
125 °C
IDD
Supply
current in
Run mode
All
peripherals
disabled
VOS0(3)
-
mA
-
VOS0
-
-
-
-
VOS1
90.5
69.5
VOS2
45.5
VOS3
32.5
13.5
6.9
All
peripherals
enabled
VOS0
(3)
-
-
VOS0
-
-
VOS1
VOS2
VOS3
1.
Data are in DTCM for best computation performance, the cache has no influence on consumption in this case.
2.
Guaranteed by characterization results, unless otherwise specified.
3.
CPU_FREQ_BOOST is enabled.

---

Table 21. Typical and maximum current consumption in Run mode, code with data processing
 running from flash memory, cache ON(1)
Symbol
Parameter
Conditions
frcc_c_ck
(MHz)
Typ
Max(2)
Unit
TJ =
25 °C
TJ =
85 °C
TJ =
105 °C
TJ =
125 °C
IDD
Supply
current in Run
mode
All
peripherals
disabled
VOS0(3)
-
mA
-
VOS0
-
-
VOS1
VOS2
46.5
-
-
-
-
42.5
VOS3
33.5
-
-
-
-
-
-
-
-
-
-
-
-
6.85
-
-
-
-
All
peripherals
enabled
VOS0
(3)
-
-
VOS0
-
-
VOS1
VOS2
VOS3
1.
Data are in DTCM for best computation performance, the cache has no influence on consumption in this case.
2.
Guaranteed by characterization results, unless otherwise specified.
3.
CPU_FREQ_BOOST is enabled.

---

Table 22. Typical and maximum current consumption in Run mode,
 code with data processing running from flash memory, cache OFF(1)
1.
Data are in DTCM for best computation performance, the cache has no influence on consumption in this
case.
Symbol
Parameter
Conditions
frcc_c_ck
(MHz)
Typ
Unit
IDD
Supply current
in Run mode
All peripherals
disabled
VOS0(2)
2.
CPU_FREQ_BOOST is enabled.
mA
VOS0
76.5
VOS1
66.5
51.5
VOS2
47.5
43.5
VOS3
24.5
All peripherals
enabled
VOS0(2)
VOS0
VOS1
VOS2
73.5
VOS3

---

Table 23. Typical consumption in Run mode and corresponding performance
versus code position
Symbol
Parameter
Conditions
frcc_c_c k
(MHz)
Coremark
Typ
Unit
IDD/
Coremark
Unit
Peripheral
Code
IDD
Supply
current in
Run mode
All
peripherals
disabled,
cache ON
ITCM
mA
52.2
µA/
Core-
mark
FLASH
52.2
AXI
SRAM
52.2
SRAM 1
54.0
SRAM 4
52.2
All
peripherals
disabled
cache OFF
FLASH
107.3
AXI
SRAM
82.6
SRAM 1
96.5
122.2
SRAM 4
89.5
123.8
Table 24. Typical current consumption in Autonomous mode
Symbol
Parameter
Conditions
frcc_c_c k
(MHz)
Typ
Unit
IDD
Supply current in
Autonous mode
Run, D1Stop,
D2Stop
VOS3
3.6
mA
Run,
D1Standby,
D2Standby
VOS3
2.6
Table 25. Typical and maximum current consumption in Sleep mode
Symbol
Parameter
Conditions
frcc_c_ck
(MHz)
Typ
Max(1)
1.
Guaranteed by characterization results.
Unit
TJ =
25 °C
TJ =
85 °C
TJ =
105 °C
TJ =
125 °C
IDD(Sleep)
Supply
current in
Sleep mode
All
peripherals
disabled
VOS0
(2)
2.
CPU_FREQ_BOOST is enabled.
-
-
-
-
mA
33.5
-
VOS0
33.5
-
-
VOS1
22.5
18.5
VOS2
16.5
9.7
VOS3
8.5

---

I/O system current consumption
The current consumption of the I/O system has two components: static and dynamic.
Table 26. Typical and maximum current consumption in Stop mode
Symbol
Parameter
Conditions
Typ
Max(1)
1.
Guaranteed by characterization results.
Unit
TJ =
25 °C
TJ =
85 °C
TJ =
105 °C
TJ =
125 °C
IDD(Stop)
Supply
current in
Stop and
DStop
modes
Flash memory in low-
power mode
SVOS5
0.52
3.7
mA
SVOS4
0.81
6.1
SVOS3
1.15
8.6
Flash memory in
normal mode
SVOS5
0.535
3.7
SVOS4
0.96
6.2
SVOS3
1.45
8.8
Table 27. Typical and maximum current consumption in Standby mode
Symbol
Parameter
Conditions
Typ(1)
1.
These values are given for PDR OFF. When the PDR is ON, the typical current consumption is increased
(refer to Table 16: Reset and power control block characteristics.
Max at 3.6 V(2)
2.
Guaranteed by characterization results.
Unit
Backup
SRAM
RTC
and
LSE(3)
3.
The LSE is in Low-drive mode.
1.65
V
2.4 V
3 V
3.3 V
TJ =
25 °
C
TJ =
85 °
C
TJ =
105 °
C
TJ =
125 °
C
IDD
(Standby)
Supply
current in
Standby
mode,
IWDG OFF
OFF
OFF
2.2
2.35
2.5
2.8
-
-
-
-
µA
ON
OFF
3.5
3.7
4.3
-
-
-
-
OFF
ON
2.2
2.4
2.85
3.25
4.5
ON
ON
3.5
3.8
4.35
4.75
8.3
Table 28. Typical and maximum current consumption in VBAT mode
Sym-
bol
Para-
meter
Conditions
Typ
Max at 3.6 V(1)(2)
1.
Guaranteed by characterization results.
2.
The LDO regulator is used before switching to VBAT mode.
Unit
Back-up
SRAM
RTC
and
LSE(3)
3.
The LSE is in Low-drive mode.
1.2 V
2 V
3 V
3.3 V
TJ =
25 °C
TJ =
85 °C
TJ =
105 °
C
TJ =
125 °
C
IDD
(VBAT)
Supply
current in
VBAT
mode
OFF
OFF
0.008
0.01
0.025
0.05
0.3
3.1
7.4
µA
ON
OFF
1.5
1.7
1.9
1.9
OFF
ON
0.4
0.5
0.75
0.8
-
-
-
-
ON
ON
1.8
2.1
2.8
3.2
-
-
-
-

---

I/O static current consumption
All the I/Os used as input with pull-up or pull-down generate a current consumption when
the pin is externally held to the opposite level.
The value of this current consumption can be simply computed by using the pull-up/pull-
down resistors values given in Table 51: I/O static characteristics.
For the output pins, any internal or external pull-up or pull-down and external load must also
be considered to estimate the current consumption.
An additional I/O current consumption is due to I/Os configured as inputs if an intermediate
voltage level is externally applied. This current consumption is caused by the input Schmitt
trigger circuits used to discriminate the input value. Unless this specific configuration is
required by the application, this supply current consumption can be avoided by configuring
these I/Os in analog mode. This is notably the case of ADC input pins which should be
configured as analog inputs.
Caution:
Any floating input pin can also settle to an intermediate voltage level or switch inadvertently,
as a result of external electromagnetic noise. To avoid a current consumption related to
floating pins, they must either be configured in analog mode, or forced internally to a definite
digital value. This can be done either by using pull-up/down resistors or by configuring the
pins in output mode.
I/O dynamic current consumption
In addition to the internal peripheral current consumption (see Table 29: Peripheral current
consumption in Run mode), the I/Os used by an application also contribute to the current
consumption. When an I/O pin switches, it uses the current from the MCU supply voltage to
supply the I/O pin circuitry and to charge/discharge the capacitive load (internal and
external) connected to the pin:
where
ISW is the current sunk by a switching I/O to charge/discharge the capacitive load
VDDx is the MCU supply voltage
fSW is the I/O switching frequency
CL is the total capacitance seen by the I/O pin: C = CINT+ CEXT
The test pin is configured in push-pull output mode and is toggled by software at a fixed
frequency.
ISW
VDDx
fSW
CL
×
×
=

---

On-chip peripheral current consumption
The MCU is placed under the following conditions:
•
At startup, all I/O pins are in analog input configuration.
•
All peripherals are disabled unless otherwise mentioned.
•
The I/O compensation cell is enabled.
•
frcc_c_ck is the CPU clock. fPCLK = frcc_c_ck/4, and fHCLK = frcc_c_ck/2.
The given value is calculated by measuring the difference of current consumption
–
with all peripherals clocked off
–
with only one peripheral clocked on
–
frcc_c_ck = 550 MHz (Scale 0), frcc_c_ck = 400 MHz (Scale 1), frcc_c_ck = 300 MHz
(Scale 2), frcc_c_ck = 170 MHz (Scale 3)
•
The ambient operating temperature is 25 °C and VDD=3.3 V
•
The LDO regulator supplies VCORE.
Table 29. Peripheral current consumption in Run mode
Peripheral
IDD(Typ)
Unit
VOS0
VOS1
VOS2
VOS3
AHB3
MDMA
3.70
3.10
2.90
2.60
µA/MHz
DMA2D
2.70
2.30
2.10
1.90
Flash memory
15.20
14.00
12.00
10.90
FMC registers
0.90
0.90
0.80
0.70
FMC kernel
7.00
6.10
5.60
5.40
OCTOSPI1 registers
1.40
1.30
0.50
0.40
OCTOSPI1 kernel
3.10
1.20
0.50
0.20
SDMMC1 registers
8.70
7.60
6.90
6.10
SDMMC1 kernel
2.10
1.80
1.40
1.20
OCTOSPI2 registers
1.40
1.30
0.90
0.60
OCTOSPI2 kernel
2.50
1.50
1.40
0.50
AXI SRAM
8.50
7.50
6.90
6.00
AHB1
DMA1
0.70
0.60
0.50
0.40
µA/MHz
DMA2
1.00
0.80
0.70
0.70
DMAMUX1
0.10
0.10
0.10
0.10
ADC1/2 registers
4.50
4.00
3.60
2.30
ADC1/2 kernel
0.90
0.80
0.60
0.40
USB1 registers
20.80
17.50
16.50
14.80
USB1 kernel
1.20
0.90
0.90
0.90
USB1 ULPI kernel
31.00
30.00
29.50
27.00
Ethernet
17.30
14.40
13.70
12.30

---

AHB2
DCMI
4.80
4.00
3.80
3.40
µA/MHz
HSEM
0.60
0.60
0.10
0.10
RNG1 registers
1.20
1.00
0.90
0.70
RNG1 kernel
15.00
13.60
10.00
9.00
SDMMC2 registers
15.00
12.20
11.70
10.40
SDMMC2 kernel
2.10
1.80
1.40
1.20
BDMA
6.50
5.90
4.80
4.30
SRAM1
2.40
2.00
1.80
1.60
SRAM2
2.70
2.30
2.00
1.80
CORDIC
0.80
0.60
0.50
0.50
FMAC
2.40
2.10
1.90
1.60
AHB4
GPIOA
0.10
0.10
0.10
0.10
µA/MHz
GPIOB
0.90
0.80
0.10
0.10
GPIOC
0.50
0.10
0.10
0.10
GPIOD
0.90
0.80
0.10
0.10
GPIOE
0.90
0.80
0.10
0.10
GPIOF
0.30
0.10
0.10
0.10
GPIOG
0.90
0.80
0.30
0.20
GPIOH
0.10
0.10
0.10
0.10
GPIOJ
0.90
0.80
0.30
0.20
GPIOK
0.80
0.80
0.10
0.10
HSEM
0.60
0.60
0.10
0.10
BDMA
6.50
5.90
4.80
4.30
CRC
0.90
0.30
0.30
0.30
ADC3 registers
2.10
1.40
1.30
1.20
ADC3 kernel
0.40
0.30
0.30
0.20
Backup SRAM
1.80
1.00
1.00
0.80
APB3
LTDC
9.00
7.90
7.70
6.40
WWDG1
0.60
0.50
0.50
0.50
Table 29. Peripheral current consumption in Run mode (continued)
Peripheral
IDD(Typ)
Unit
VOS0
VOS1
VOS2
VOS3

---

APB1
TIM2
4.50
4.40
3.30
3.00
µA/MHz
TIM3
3.80
3.20
2.90
2.70
TIM4
3.60
3.10
2.60
2.50
TIM5
4.10
3.40
3.10
2.90
TIM6
1.50
1.10
1.00
1.00
TIM7
1.40
1.10
0.90
0.90
TIM12
2.30
1.80
1.60
1.60
TIM13
1.90
1.40
1.30
1.20
TIM14
1.60
1.20
1.10
1.10
TIM23
4.60
3.90
3.60
3.40
TIM24
4.40
3.80
3.50
3.30
LPTIM1 registers
3.50
2.90
2.70
2.60
LPTIM1 kernel
2.60
2.30
2.00
1.80
SPI2 registers
2.10
1.60
0.90
0.80
SPI2 kernel
1.50
1.20
1.10
1.00
SPI3 registers
2.40
2.00
1.90
1.80
SPDIFRX registers
0.60
0.50
0.50
0.50
SPDIFRX kernel
3.50
2.80
2.40
2.20
USART2 registers
6.60
5.70
5.20
4.90
USART2 kernel
4.80
4.80
4.60
3.80
USART3 registers
5.90
5.40
4.60
4.30
USART3 kernel
4.00
3.40
3.00
2.90
UART4 registers
5.60
4.80
3.50
3.10
UART4 kernel
3.80
3.20
3.00
2.40
UART5 registers
5.60
4.60
4.40
4.00
UART5 kernel
3.90
3.40
3.30
3.20
UART7 registers
5.40
4.60
4.20
3.90
UART7 kernel
3.80
3.30
3.00
3.00
UART8 registers
5.60
4.10
3.50
3.40
UART8 kernel
3.60
3.20
3.20
3.10
I2C1 registers
0.90
0.60
0.60
0.50
I2C1 kernel
2.30
2.00
1.80
1.60
I2C2 registers
1.00
0.70
0.60
0.60
Table 29. Peripheral current consumption in Run mode (continued)
Peripheral
IDD(Typ)
Unit
VOS0
VOS1
VOS2
VOS3

---

APB1
I2C2 kernel
2.30
1.90
1.70
1.20
µA/MHz
I2C3 registers
0.90
0.60
0.50
0.50
I2C3 kernel
2.30
2.00
1.00
1.00
I2C5 registers
0.90
0.60
0.50
0.50
I2C5 kernel
2.20
2.10
1.90
1.80
CEC registers
0.60
0.30
0.20
0.20
CEC kernel
0.10
0.10
0.10
0.10
DAC1
1.60
1.30
1.10
1.10
FDCAN1/2/3 registers
24.10
20.90
18.20
17.40
FDCAN1/2/3 kernel
9.90
9.90
9.00
8.00
CRS
4.90
3.90
3.50
3.20
SWPMI registers
1.10
0.80
0.80
0.80
SWPMI kernel
1.50
1.10
1.00
1.00
OPAMP
0.50
0.40
0.30
0.20
Table 29. Peripheral current consumption in Run mode (continued)
Peripheral
IDD(Typ)
Unit
VOS0
VOS1
VOS2
VOS3

---

APB2
TIM1
5.30
4.40
4.20
3.80
µA/MHz
TIM8
5.60
5.40
5.20
3.90
USART1 registers
1.80
1.60
1.40
1.10
USART1 kernel
3.00
2.90
2.80
2.70
USART6 registers
1.90
1.70
1.50
1.20
USART6 kernel
4.50
4.00
3.60
3.10
UART9 registers
1.70
1.70
1.60
1.10
UART9 kernel
3.80
3.30
2.90
2.90
USART10 registers
1.80
1.70
1.40
1.10
USART10 kernel
3.80
3.30
2.90
2.90
SPI1 registers
1.90
1.80
1.40
1.20
SPI1 kernel
1.50
1.20
1.10
1.00
SPI4 registers
1.80
1.60
1.40
1.10
SPI4 kernel
1.50
1.20
1.10
1.00
SPI5 registers
1.60
1.60
1.40
1.10
SPI5 kernel
1.50
1.20
1.10
1.00
TIM15
2.80
2.50
2.30
1.90
TIM16
2.00
1.90
1.60
1.30
TIM17
2.10
2.00
1.70
1.40
SAI1 registers
1.40
1.40
1.20
0.90
SAI1 kernel
0.80
0.70
0.70
0.70
DFSDM1 registers
5.60
5.40
5.30
4.00
DFSDM1 kernel
0.30
0.20
0.20
0.10
SYSCFG
1.20
1.10
1.10
1.10
Table 29. Peripheral current consumption in Run mode (continued)
Peripheral
IDD(Typ)
Unit
VOS0
VOS1
VOS2
VOS3

---

APB4
LPUART1 registers
1.80
0.90
0.80
0.60
µA/MHz
LPUART1 kernel
2.40
2.30
2.00
1.90
SPI6 registers
2.60
2.30
2.10
1.80
SPI6 kernel
1.20
1.10
1.00
0.90
I2C4 registers
0.70
0.70
0.60
0.40
I2C4 kernel
2.00
1.70
1.70
1.40
LPTIM2 registers
1.50
0.70
0.50
0.30
LPTIM2 kernel
2.50
2.10
2.00
1.90
LPTIM3 registers
2.90
2.60
2.30
1.90
LPTIM3 kernel
2.40
2.00
1.90
1.70
LPTIM4 registers
2.60
2.30
2.10
1.80
LPTIM4 kernel
2.10
1.80
1.70
1.60
LPTIM5 registers
2.60
2.30
2.00
1.70
LPTIM5 kernel
2.10
1.80
1.60
1.50
COMP1/2
0.70
0.30
0.20
0.10
VREF
0.10
0.10
0.10
0.10
RTC
0.10
0.10
0.10
0.10
WWDG1
0.60
0.50
0.50
0.50
SAI4 registers
2.40
2.20
2.10
1.70
SAI4 kernel
0.90
0.90
0.90
0.70
DTS
2.90
2.60
2.30
2.00
Table 29. Peripheral current consumption in Run mode (continued)
Peripheral
IDD(Typ)
Unit
VOS0
VOS1
VOS2
VOS3

---

6.3.8
Wake-up time from low-power modes
The wake-up times given in Table 30 are measured starting from the wake-up event trigger
up to the first instruction executed by the CPU:
•
For Stop or Sleep modes: the wake-up event is WFE.
•
WKUP (PC1) pin is used to wake-up from Standby, Stop and Sleep modes.
All timings are derived from tests performed under ambient temperature and VDD=3.3 V.
Table 30. Low-power mode wakeup timings
Symbol
Parameter
Conditions
Typ(1)
Max(1)
(2)
Unit
tWUSLEEP
(3)
Wakeup from Sleep
-
14.00
15.00
CPU
clock
cycles
tWUSTOP
(3)
Wakeup from Stop
mode
SVOS3, HSI, flash memory in Normal mode
4.6
6.2
µs
SVOS3, HSI, flash memory in low-power mode
12.4
17.4
SVOS4, HSI, flash memory in Normal mode
15.5
21.1
SVOS4, HSI, flash memory in low-power mode
23.3
31.8
SVOS5, HSI, flash memory in Normal mode
39.1
52.6
SVOS5, HSI, flash memory in low-power mode
39.1
52.7
SVOS3, CSI, flash memory in Normal mode
30.0
41.6
SVOS3, CSI, flash memory in low-power mode
40.6
55.0
SVOS4, CSI, flash memory in Normal mode
41.0
55.4
SVOS4, CSI, flash memory in low-power mode
51.5
68.8
SVOS5, CSI, flash memory in Normal mode
67.3
89.5
SVOS5, CSI, flash memory in low-power mode
67.2
89.5
tWUSTDBY
(3)
Wakeup from
Standby mode
-
400.0
504.3
1.
Guaranteed by characterization results.
2.
The maximum values have been measured at -40 °C, in worst conditions.
3.
The wake-up times are measured from the wake-up event to the point in which the application code reads the first

---

6.3.9
External clock source characteristics
High-speed external user clock generated from an external source
In bypass mode the HSE oscillator is switched off and the input pin is a standard I/O.
The external clock signal has to respect the Table 51: I/O static characteristics. However,
the recommended clock input waveform is shown in Figure 13.
Figure 13. High-speed external clock source AC timing diagram
Table 31. High-speed external user clock characteristics(1)
1.
Guaranteed by design.
Symbol
Parameter
Min
Typ
Max
Unit
fHSE_ext
User external clock source frequency
MHz
VHSEH
Digital OSC_IN input high-level
voltage
0.7 VDD
-
VDD
V
VHSEL
Digital OSC_IN input low-level voltage
VSS
-
0.3 VDD
tW(HSE)
OSC_IN high or low time
-
-
ns
ai17528b
OSC_IN
External
STM32
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

Low-speed external user clock generated from an external source
In bypass mode the LSE oscillator is switched off and the input pin is a standard I/O. The
external clock signal has to respect the Table 51: I/O static characteristics. However, the
recommended clock input waveform is shown in Figure 14.
Figure 14. Low-speed external clock source AC timing diagram
Table 32. Low-speed external user clock characteristics(1)
1.
Guaranteed by design.
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fLSE_ext
User external clock source
frequency
-
-
32.768
kHz
VLSEH
OSC32_IN input pin high-level
voltage
-
0.7 VDD
-
VDD
V
VLSEL
OSC32_IN input pin low-level
voltage
-
VSS
-
0.3 VDD
tw(LSEH)
tw(LSEL)
OSC32_IN high or low time
-
-
-
ns
ai17529b
OSC32_IN
External
STM32
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

High-speed external clock generated from a crystal/ceramic resonator
The high-speed external (HSE) clock can be supplied with a 4 to 50 MHz crystal/ceramic
resonator oscillator. All the information given in this paragraph are based on
characterization results obtained with typical external components specified in Table 33. In
the application, the resonator and the load capacitors have to be placed as close as
possible to the oscillator pins in order to minimize output distortion and startup stabilization
time. Refer to the crystal resonator manufacturer for more details on the resonator
characteristics (frequency, package, accuracy).
Note:
For information on selecting the crystal, refer to application note AN2867 “Oscillator design
guide for STM8AF/AL/S, STM32 MCUs and MPUs” available from the ST website
www.st.com.
Table 33. 4-50 MHz HSE oscillator characteristics(1)
1.
Guaranteed by design.
Symbol
Parameter
Operating
conditions(2)
2.
Resonator characteristics given by the crystal/ceramic resonator manufacturer.
Min
Typ
Max
Unit
F
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
IDD(HSE)
HSE current
consumption
During startup(3)
3.
This consumption level occurs during the first 2/3 of the tSU(HSE) startup time.
-
-
mA
VDD=3 V, Rm=30 Ω
CL=10 pF at 4 MHz
-
0.35
-
VDD=3 V, Rm=30 Ω
CL=10 pF at 8 MHz
-
0.40
-
VDD=3 V, Rm=30 Ω
CL=10 pF at 16 MHz
-
0.45
-
VDD=3 V, Rm=30 Ω
CL=10 pF at 32 MHz
-
0.65
-
VDD=3 V, Rm=30 Ω
CL=10 pF at 48 MHz
-
0.95
-
Gmcritmax
Maximum critical crystal
gm
Startup
-
-
1.5
mA/V
tSU
(4)
4.
tSU(HSE) is the startup time measured from the moment it is enabled (by software) to a stabilized 8 MHz
oscillation is reached. This value is measured for a standard crystal resonator and it can vary significantly
with the crystal manufacturer.
Start-up time
VDD is stabilized
-
-
ms

---

Figure 15. Typical application with an 8 MHz crystal
1.
REXT value depends on the crystal characteristics.
Low-speed external clock generated from a crystal/ceramic resonator
The low-speed external (LSE) clock can be supplied with a 32.768 kHz crystal/ceramic
resonator oscillator. All the information given in this paragraph are based on
characterization results obtained with typical external components specified in Table 34. In
the application, the resonator and the load capacitors have to be placed as close as
possible to the oscillator pins in order to minimize output distortion and startup stabilization
time. Refer to the crystal resonator manufacturer for more details on the resonator
characteristics (frequency, package, accuracy).
ai17530b
OSC_OUT
OSC_IN
fHSE
CL1
RF
STM32
8 MHz
resonator
Resonator with
integrated capacitors
Bias
controlled
gain
REXT(1)
CL2
Table 34. Low-speed external user clock characteristics(1)
Symbol
Parameter
Operating conditions(2)
Min
Typ
Max
Unit
F
Oscillator frequency
-
-
32.768
-
kHz
IDD
LSE current
consumption
LSEDRV[1:0] = 00,
Low drive capability
-
-
nA
LSEDRV[1:0] = 01,
Medium Low drive capability
-
-
LSEDRV[1:0] = 10,
Medium high drive capability
-
-
LSEDRV[1:0] = 11,
High drive capability
-
-
Gmcritmax
Maximum critical crystal
gm
LSEDRV[1:0] = 00,
Low drive capability
-
-
0.5
µA/V
LSEDRV[1:0] = 01,
Medium Low drive capability
-
-
0.75
LSEDRV[1:0] = 10,
Medium high drive capability
-
-
1.7
LSEDRV[1:0] = 11,
High drive capability
-
-
2.7
tSU
(3)
Startup time
VDD is stabilized
-
-
s
1.
Guaranteed by design.
2.
Refer to the note and caution paragraphs below the table, and to the application note AN2867 “Oscillator design guide for
STM8AF/AL/S, STM32 MCUs and MPUs”.
3.
tSU is the startup time measured from the moment it is enabled (by software) to a stabilized 32.768k Hz oscillation is
reached. This value is measured for a standard crystal resonator and it can vary significantly with the crystal manufacturer.

---

Note:
For information on selecting the crystal, refer to the application note AN2867 “Oscillator
design guide for STM8AF/AL/S, STM32 MCUs and MPUs” available from the ST website
www.st.com.
Figure 16. Typical application with a 32.768 kHz crystal
1.
An external resistor is not required between OSC32_IN and OSC32_OUT and it is forbidden to add one.
6.3.10
Internal clock source characteristics
The parameters given in Table 35 to Table 37 are derived from tests performed under
ambient temperature and VDD supply voltage conditions summarized in Table 12: General
operating conditions.
48 MHz high-speed internal RC oscillator (HSI48)
ai17531d
STM32
OSC32_OUT
fLSE
CL1
RF
32.768 kHz
resonator
Bias
controlled
gain
OSC32_IN
CL2
Resonator with
integrated
capacitors
Table 35. HSI48 oscillator characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fHSI48
HSI48 frequency
VDD=3.3 V,
TJ=30 °C
47.5(1)
48.5(1)
MHz
TRIM(2)
USER trimming step
-
-
0.175
0.250
%
USER TRIM
COVERAGE(3)
USER TRIMMING coverage
± 32 steps
±4.70
±5.6
-
%
DuCy(HSI48)(2)
Duty Cycle
-
-
%
ACCHSI48_REL(3)(4) Accuracy of the HSI48 oscillator over
temperature (factory calibrated)
TJ=-40 to 125 °C
–4.5
-
3.5
%
∆VDD(HSI48)(2)(5)
HSI48 oscillator frequency drift with
VDD
(6) (the reference is 3.3 V)
VDD=3 to 3.6 V
-
0.025
0.05
%
VDD=1.62 V to 3.6 V
-
0.05
0.1
tsu(HSI48)
(2)
HSI48 oscillator start-up time
-
-
2.1
4.0
µs
IDD(HSI48)
(2)
HSI48 oscillator power consumption
-
-
µA
NT jitter(2)
Next transition jitter
Accumulated jitter on 28 cycles(7)
-
-
± 0.15
-
ns
PT jitter(2)
Paired transition jitter
Accumulated jitter on 56 cycles(7)
-
-
± 0.25
-
ns
1.
Guaranteed by test in production.
2.
Guaranteed by design.
3.
Guaranteed by characterization results.
4.
 ∆fHSI = ACCHSI48_REL + ∆VDD.

---

64 MHz high-speed internal RC oscillator (HSI)
5.
 ∆fHSI = ACCHSI48_REL + ∆VDD.
6.
These values are obtained by using the formula: (Freq(3.6 V) - Freq(3.0 V)) / Freq(3.0 V) or (Freq(3.6 V) - Freq(1.62 V)) /
Freq(1.62 V).
7.
Jitter measurements are performed without clock source activated in parallel.
Table 36. HSI oscillator characteristics(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fHSI
HSI frequency
VDD=3.3 V, TJ=30 °C
63.7(2)
64.3(2)
MHz
TRIM
HSI user trimming step
Trimming is not a multiple
of 32
-
0.24
0.32
%
Trimming is 128, 256 and
−5.2
−1.8
-
Trimming is 64, 192, 320
and 448
−1.4
−0.8
-
Other trimming are a
multiple of 32 (not
including multiple of 64
and 128)
−0.6
−0.25
-
DuCy(HSI)
Duty cycle
-
-
%
ΔVDD (HSI)
HSI oscillator frequency drift over
VDD (the reference is 3.3 V)
VDD=1.62 to 3.6 V
−0.12
-
0.03
%
ΔTEMP(HSI)
HSI oscillator frequency drift over
temperature (the reference is
64 MHz)
TJ=-20 to 105 °C
−1(3)
-
1(3)
%
TJ=−40 to TJmax °C
−2(3)
-
1(3)
tsu(HSI)
HSI oscillator start-up time
-
-
1.4
µs
tstab(HSI)
HSI oscillator stabilization time
at 1% of target frequency
-
at 5% of target frequency
-
-
IDD(HSI)
HSI oscillator power consumption
-
-
µA
1.
Guaranteed by design unless otherwise specified.
2.
Guaranteed by test in production.
3.
Guaranteed by characterization results.

---

4 MHz low-power internal RC oscillator (CSI)
Low-speed internal (LSI) RC oscillator
Table 37. CSI oscillator characteristics(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fCSI
CSI frequency
VDD=3.3 V, TJ=30 °C
3.96(2)
4.04(2)
MHz
TRIM
CSI trimming step
Trimming is not a
multiple of 16
-
0.40
0.75
%
Trimming is a multiple
of 32
−4.75
−2.75
0.75
Other trimming values
not multiple of 16
(excluding multiple of
32)
−0.43
0.00
0.75
DuCy(CSI)
Duty cycle
-
-
%
∆TEMP (CSI)
CSI oscillator frequency drift over
temperature
TJ = 0 to 85 °C
−3.7(3)
-
4.5(3)
%
TJ = −40 to 125 °C
−11(3)
-
7.5(3)
∆VDD (CSI)
CSI oscillator frequency drift over
VDD
VDD = 1.62 to 3.6 V
−0.06
-
0.06
%
tsu(CSI)
CSI oscillator startup time
-
-
µs
tstab(CSI)
CSI oscillator stabilization time
(to reach ± 3% of fCSI)
-
-
-
cycle
IDD(CSI)
CSI oscillator power consumption
-
-
µA
1.
Guaranteed by design, unless otherwise specified.
2.
Guaranteed by test in production.
3.
Guaranteed by characterization results.
Table 38. LSI oscillator characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fLSI
LSI frequency
VDD = 3.3 V, TJ = 25 °C
31.4(1)
32.6(1)
kHz
TJ = –40 to 110 °C,
VDD = 1.62 to 3.6 V
29.76(2)
-
33.6(2)
TJ = –40 to 125 °C,
VDD = 1.62 to 3.6 V
29.4(2)
-
33.6(2)
tsu(LSI)
(3)
LSI oscillator startup time
-
-
µs
tstab(LSI)
(3)
LSI oscillator stabilization time
(5% of final value)
-
-
IDD(LSI)
(3)
LSI oscillator power consumption
-
-
nA
1.
Guaranteed by test in production.
2.
Guaranteed by characterization results.
3.
Guaranteed by design.

---

6.3.11
PLL characteristics
The parameters given in Table 39, Table 42 are derived from tests performed under
temperature and VDD supply voltage conditions summarized in Table 12: General operating
conditions.
Table 39. PLL1 characteristics (wide VCO frequency range)(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fPLL_IN
PLL input clock
-
-
MHz
PLL input clock duty cycle
-
-
%
fPLL_P_OUT
PLL multiplier output clock P
VOS0
1.5
-
550(2)
MHz
VOS1
1.5
-
400(2)
VOS2
1.5
-
300(2)
VOS3
1.5
-
170(2)
fVCO_OUT
PLL VCO output
-
-
836(3)
tLOCK
PLL lock time
Normal mode
150(3)
µs
Sigma-delta mode (CKIN ≥
8 MHz)
Jitter
Cycle-to-cycle jitter(4)
fPLL_OUT =
fVCO_OUT/100
fVCO_OUT
= 192 MHz
-
-
ps
fVCO_OUT
= 400 MHz
-
-
fVCO_OUT
= 560 MHz
-
-
fVCO_OUT
= 800 MHz
-
-
Period jitter
fVCO_OUT
= 192 MHz
-
-
fVCO_OUT
= 560 MHz
-
-
fVCO_OUT
= 800 MHz
-
-
Long term jitter
Normal mode
(CKIN = 2 MHz)
fVCO_OUT
= 192 MHz
-
0.15
-
%
fVCO_OUT
= 400 MHz
-
0.14
-
fVCO_OUT
= 832 MHz
-
0.16
-
Sigma-delta
mode (CKIN =
16 MHz)
fVCO_OUT
= 192 MHz
-
0.17
-
fVCO_OUT
= 500 MHz
-
0.08
-
fVCO_OUT
= 836 MHz
-
0.06
-

---

IDD(PLL)
PLL power consumption
fVCO_OUT =
560 MHz
VDDA
µA
VCORE
fVCO_OUT =
192 MHz
VDDA
VCORE
1.
Guaranteed by design unless otherwise specified.
2.
This value must be limited to the maximum frequency due to the product limitation.
3.
Guaranteed by characterization results.
4.
Integer mode only.
Table 39. PLL1 characteristics (wide VCO frequency range)(1) (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Table 40. PLL1 characteristics (medium VCO frequency range)(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fPLL_IN
PLL input clock
-
-
MHz
PLL input clock duty cycle
-
-
%
fPLL_OUT
PLL multiplier output clock P, Q, R
VOS0
1.17
-
MHz
VOS1
1.17
-
VOS2
1.17
-
VOS3
1.17
-
fVCO_OUT
PLL VCO output
-
-
tLOCK
PLL lock time
Normal mode
-
60(2)
100(2)
µs
Sigma-delta mode
forbidden
Jitter
Cycle-to-cycle jitter(3)
-
fVCO_OUT =
150 MHz
-
-
±ps
fVCO_OUT =
300 MHz
-
-
fVCO_OUT =
400 MHz
-
-
fVCO_OUT =
420 MHz
-
-
Period jitter
fPLL_OUT =
50 MHz
fVCO_OUT =
150 MHz
-
-
±-ps
fVCO_OUT =
400 MHz
-
-
Long term jitter
Normal mode
fVCO_OUT =
400 MHz
-
±0.3
-
%
I(PLL)
PLL power consumption on VDD
fVCO_OUT =
420 MHz
VDD
-
µA
VCORE
-
-
fVCO_OUT =
150 MHz
VDD
-
VCORE
-
-

---

1.
Guaranteed by design unless otherwise specified.
2.
Guaranteed by characterization results.
3.
Integer mode only.
Table 41.  PLL2 and PLL3 characteristics (wide VCO frequency range)(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fPLL_IN
PLL input clock
-
-
MHz
PLL input clock duty cycle
-
-
%
fPLL_OUT
PLL multiplier output clock P,
Q, R
VOS0
1.5
-
550(2)
MHz
VOS1
1.5
-
400(2)
VOS2
1.5
-
300(2)
VOS3
1.5
-
170(2)
fVCO_OUT
PLL VCO output
-
-
960(3)
tLOCK
PLL lock time
Normal mode
-
150(3)
µs
Sigma-delta mode (fPLL_IN
≥ 8 MHz)
-
166(3)
Jitter
Cycle-to-cycle jitter(4)
fVCO_OUT = 192 MHz
-
-
±ps
fVCO_OUT = 200 MHz
-
-
fVCO_OUT = 400 MHz
-
-
fVCO_OUT = 800 MHz
-
-
Long term jitter
Normal
mode
(fPLL_IN =
2 MHz)
fVCO_OUT =
560 MHz
-
±0.2
-
%
Normal
mode
(fPLL_IN =
16 MHz)
fVCO_OUT =
560 MHz
-
±0.8
-
Sigma-delta
mode
(fPLL_IN =
2 MHz)
fVCO_OUT =
560 MHz
-
±0.2
-
Sigma-delta
mode
(fPLL_IN =
16 MHz)
fVCO_OUT =
560 MHz
-
±0.8
-
IDD(PLL)
(3) PLL power consumption
fVCO_OUT =
836 MHz
VDD
-
µA
VCORE
-
-
fVCO_OUT =
192 MHz
VDD
-
VCORE
-
-
1.
Guaranteed by design unless otherwise specified.
2.
This value must be limited to the maximum frequency due to the product limitation.

---

3.
Guaranteed by characterization results.
4.
Integer mode only.
Table 42. PLL2 and PLL3 characteristics (medium VCO frequency range)(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fPLL_IN
PLL input clock
-
-
MHz
PLL input clock duty cycle
-
-
%
fPLL_OUT
PLL multiplier output clock
P, Q, R
VOS0
1.17
-
MHz
VOS1
1.17
-
-
VOS2
1.17
-
-
VOS3
1.17
-
-
fVCO_OUT
PLL VCO output
-
-
-
tLOCK
PLL lock time
Normal mode
-
100(2)
µs
Sigma-delta mode
forbidden
Jitter
Cycle-to-cycle jitter(3)
fVCO_OUT = 150 MHz
-
-
±ps
fVCO_OUT = 200 MHz
-
-
fVCO_OUT = 400 MHz
-
-
fVCO_OUT = 420 MHz
-
-
Period jitter
fPLL_OUT =
50 MHz
fVCO_OUT =
150 MHz
-
-
±ps
fVCO_OUT = 400 MHz
-
-
Long term jitter
Normal mode
fVCO_OUT =
400 MHz
-
±0.3
-
%
IDD(PLL)
PLL power consumption on
VDD
fVCO_OUT =
420 MHz
VDD
-
µA
VCORE
-
-
fVCO_OUT =
150 MHz
VDD
-
VCORE
-
-
1.
Guaranteed by design unless otherwise specified.
2.
Guaranteed by characterization results.
3.
Integer mode only.

---

6.3.12
Memory characteristics
Flash memory
The characteristics are given at TJ = –40 to 125 °C unless otherwise specified.
The devices are shipped to customers with the flash memory erased.
Table 43. Flash memory characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
IDD
Supply current
Write / Erase 8-bit mode
-
6.5
-
mA
Write / Erase 16-bit mode
-
11.5
-
Write / Erase 32-bit mode
-
-
Write / Erase 64-bit mode
-
-
Table 44. Flash memory programming
Symbol
Parameter
 Conditions
Min(1)
Typ
Max(1)
Unit
tprog
Word (266 bits) programming
time
Program/erase parallelism x 8
-
580(2)
µs
Program/erase parallelism x 16
-
Program/erase parallelism x 32
-
Program/erase parallelism x 64
-
tERASE
Sector (128 Kbytes) erase time
Program/erase parallelism x 8
-
s
Program/erase parallelism x 16
-
1.8
3.6
Program/erase parallelism x 32
-
1.1
2.2
tME
Mass erase time (1 Mbyte)
Program/erase parallelism x 8
-
Program/erase parallelism x 16
-
Program/erase parallelism x 32
-
Program/erase parallelism x 64
-
Vprog
Programming voltage
Program parallelism x 8
1.62
-
3.6
V
Program parallelism x 16
Program parallelism x 32
Program parallelism x 64
1.8
-
3.6
1.
Guaranteed by characterization results.
2.
The maximum programming time is measured after 10K erase operations.
Table 45. Flash memory endurance and data retention
Symbol
Parameter
 Conditions
Min(1)
Unit
NEND
Endurance
TJ = –40 to +125 °C
kcycles
tRET
Data retention
1 kcycle at TA = 85 °C
Years
10 kcycles at TA = 55 °C

---

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
A device reset allows normal operations to be resumed.
The test results are given in Table 46. They are based on the EMS levels and classes
defined in application note AN1709 “EMC design guide for STM8, STM32 and Legacy
MCUs ”.
As a consequence, it is recommended to add a serial resistor (1 kΏ) located as close as
possible to the MCU to the pins exposed to noise (connected to tracks longer than 50 mm
on PCB).
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
Critical Data corruption (control registers...)
1.
Guaranteed by characterization results.
Table 46. EMS characteristics
Symbol
Parameter
Conditions
Level/
Class
VFESD
Voltage limits to be applied on any I/O pin to induce
a functional disturbance
VDD = 3.3 V, TA = 25 °C,
LQFP176, conforming to
IEC 61000-4-2
3B
VFTB
Fast transient voltage burst limits to be applied
through 100 pF on VDD and VSS pins to induce a
functional disturbance
VDD = 3.3 V, TA = 25 °C,
LQFP176, conforming to
IEC 61000-4-4
5A

---

Prequalification trials
Most of the common failures (unexpected reset and program counter corruption) can be
reproduced by manually forcing a low state on the NRST pin or the Oscillator pins for 1
second.
To complete these trials, ESD stress can be applied directly on the device, over the range of
specification values. When unexpected behavior is detected, the software can be hardened
to prevent unrecoverable errors occurring (see application note AN1015 “Software
techniques for improving microcontrollers EMC performance”).
Electromagnetic Interference (EMI)
The electromagnetic field emitted by the device are monitored while a simple application,
executing EEMBC code, is running. This emission test is compliant with SAE IEC61967-2
standard, which specifies the test board and the pin loading.
6.3.14
Absolute maximum ratings (electrical sensitivity)
Based on three different tests (ESD, LU) using specific measurement methods, the device is
stressed in order to determine its performance in terms of electrical sensitivity.
Electrostatic discharge (ESD)
Electrostatic discharges (a positive then a negative pulse) are applied to the pins of each
sample according to each pin combination. This test conforms to the ANSI/ESDA/JEDEC
JS-001 and  ANSI/ESDA/JEDEC JS-002  standards.
Table 47. EMI characteristics for fHSE =  8 MHz and fCPU = 550 MHz
Symbol
Parameter
Conditions
Monitored
frequency band
Max
Unit
SEMI
Peak
level(1)
VDD = 3.6 V, TA = 25 °C, LQFP176 package,
compliant with IEC61967-2
0.1 to 30 MHz
dBµV
30 to 130 MHz
130 MHz to 1 GHz
1 GHz to 2 GHz
Level(2)
0.1 MHz to 2 GHz
2.5
-
1.
Refer to AN1709 “EMI radiated test” chapter.
2.
Refer to AN1709 “EMI level classification” chapter.
Table 48. ESD absolute maximum ratings
Symbol
Ratings
Conditions
Packages
Class Maximum
value(1)
Unit
VESD(HBM)
Electrostatic discharge
voltage (human body model)
TA = 25 °C conforming to
ANSI/ESDA/JEDEC JS-001
All packages
V
VESD(CDM)
Electrostatic discharge
voltage (charge device
model)
TA = +25 °C conforming to
ANSI/ESDA/JEDEC JS-002
All LQFP
packages
C1
All BGA and
WLCSP packages
C2a
1.
Guaranteed by characterization results.

---

Static latchup
Two complementary static tests are required on six parts to assess the latchup
performance:
•
A supply overvoltage is applied to each power supply pin
•
A current injection is applied to each input, output and configurable I/O pin
These tests are compliant with JESD78 IC latchup standard.
6.3.15
I/O current injection characteristics
As a general rule, a current injection to the I/O pins, due to external voltage below VSS or
above VDD (for standard, 3.3 V-capable I/O pins) should be avoided during the normal
product operation. However, in order to give an indication of the robustness of the
microcontroller in cases when an abnormal injection accidentally happens, susceptibility
tests are performed on a sample basis during the device characterization.
Functional susceptibility to I/O current injection
While a simple application is executed on the device, the device is stressed by injecting
current into the I/O pins programmed in floating input mode. While current is injected into
the I/O pin, one at a time, the device is checked for functional failures.
The failure is indicated by an out of range parameter: ADC error above a certain limit (higher
than 5 LSB TUE), out of conventional limits of induced leakage current on adjacent pins (out
of –5 µA/+0 µA range), or other functional failure (for example reset, oscillator frequency
deviation).
The following tables are the compilation of the SIC1/SIC2 and functional ESD results.
Negative induced A negative induced leakage current is caused by negative injection and
positive induced leakage current by positive injection.
Table 49. Electrical sensitivities
Symbol
Parameter
Conditions
Class
LU
Static latchup class
Conforming to JESD78,
TJ = TJMax
II level A
Table 50. I/O current injection susceptibility(1)
Symbol
Description
Functional susceptibility
Unit
Negative
injection
Positive
injection
IINJ
PA12, PE8
mA
PC4, PE12, PF15, PH0
NA
PA0, PA0_C, PA1, PA1_C, PC2, PC2_C, PC3, PC3_C, PA4,
PA5, PE7, PG1, PH4, PH5, BOOT0
All other I/Os
NA
1.
Guaranteed by characterization results.

---

6.3.16
I/O port characteristics
General input/output characteristics
Unless otherwise specified, the parameters given in Table 51: I/O static characteristics are
derived from tests performed under the conditions summarized in Table 12: General
operating conditions. All I/Os are CMOS and TTL compliant (except for BOOT0).
Note:
For information on GPIO configuration, refer to application note AN4899 “STM32 GPIO
configuration for hardware settings and low-power consumption”, available from the ST
website www.st.com.
Table 51. I/O static characteristics
Symbol
Parameter
Condition
Min
Typ
Max
Unit
VIL
I/O input low level voltage
except BOOT0
1.62 V<VDD<3.6 V
-
-
0.3VDD
(1)
V
I/O input low level voltage
except BOOT0
-
-
0.4VDD−0.1
(2)
BOOT0 I/O input low level
voltage
-
-
0.19VDD+0.1
(2)
VIH
I/O input high level voltage
except BOOT0 and Pxy_C I/Os
1.62 V<VDD<3.6 V
0.7VDD
(1)
-
-
V
Pxy_C pin input high level
voltage
0.7VDD
(3)
I/O input high level voltage
except BOOT0
0.47VDD+
0.25(2)
-
-
BOOT0 I/O input high level
voltage
0.17VDD+
0.6(2)
-
-
VHYS
(2)
TT_xx, FT_xxx and NRST I/O
input hysteresis
1.62 V< VDD <3.6 V
-
-
mV
BOOT0 I/O input hysteresis
-
-
Ilkg
(4)
FT_xx Input leakage current(2)
0< VIN ≤ Max(VDDXXX)(9)
-
-
+/-250
nA
Max(VDDXXX) < VIN  ≤ 5.5 V
(5)(6)(9)
-
-
 FT_u IO
0< VIN ≤ Max(VDDXXX)(9)
-
-
+/- 350
Max(VDDXXX) < VIN  ≤ 5.5 V
(5)(6)(9)
-
-
5000(7)
TT_xx Input leakage current
0< VIN ≤ Max(VDDXXX) (9)
-
-
+/-250
VPP (BOOT0 alternate
function)
0< VIN ≤ VDD
-
-
VDD < VIN  ≤ 9 V
RPU
Weak pull-up equivalent
resistor(8)
VIN=VSS
kΩ
RPD
Weak pull-down equivalent
resistor(8)
VIN=VDD
(9)
CIO
I/O pin capacitance
-
-
-
pF

---

All I/Os are CMOS and TTL compliant (no software configuration required). Their
characteristics cover more than the strict CMOS-technology or TTL parameters. The
coverage of these requirements for FT I/Os is shown in Figure 17.
Figure 17. VIL/VIH for all I/Os except BOOT0
1.
Compliant with CMOS requirements.
2.
Guaranteed by design.
3.
To use these I/Os in digital input mode, VDD must respect the following condition: 0.7 VDD < VDDA + 0.3 V.
4.
This parameter represents the pad leakage of the I/O itself. The total product pad leakage is provided by the following
formula: ITotal_Ieak_max = 10 μA + [number of I/Os where VIN is applied on the pad] ₓ Ilkg(Max).
5.
All FT_xx IO except FT_lu, FT_u and PC3.
6.
VIN must be less than Max(VDDXXX) + 3.6 V.
7.
To sustain a voltage higher than MIN(VDD, VDDA, VDD33USB) +0.3 V, the internal pull-up and pull-down resistors must be
disabled.
8.
The pull-up and pull-down resistors are designed with a true resistance in series with a switchable PMOS/NMOS. This
PMOS/NMOS contribution to the series resistance is minimal (~10% order).
9.
Max(VDDXXX) is the maximum value of all the I/O supplies.
MSv46121V3
0.5
1.5
2.5
1.6
1.8
2.2
2.4
2.6
2.8
3.2
3.4
3.6
Voltage
TLL requirement: VIHmin = 2 V
TLL requirement: VILmin = 0.8 V
CMOS requirement: VIHmin=0.7VDD
CMOS requirement: VILmax=0.3VDD
Based on simulation VIHmin=0.47VDD+0.25
Based on simulation VILmax=0.4VDD-0.1

---

Output driving current
The GPIOs (general purpose input/outputs) can sink or source up to ±8 mA, and sink or
source up to ±20 mA (with a relaxed VOL/VOH).
In the user application, the number of I/O pins which can drive current must be limited to
respect the absolute maximum rating specified in Section 6.2. In particular:
•
The sum of the currents sourced by all the I/Os on VDD, plus the maximum Run
consumption of the MCU sourced on VDD, cannot exceed the absolute maximum rating
ΣIVDD (see Table 10).
•
The sum of the currents sunk by all the I/Os on VSS plus the maximum Run
consumption of the MCU sunk on VSS cannot exceed the absolute maximum rating
ΣIVSS (see Table 10).

---

Output voltage levels
Unless otherwise specified, the parameters given in Table 52: Output voltage characteristics
for all I/Os except PC13, PC14 and PC15 and Table 53: Output voltage characteristics for
PC13, PC14 and PC15 are derived from tests performed under ambient temperature and
VDD supply voltage conditions summarized in Table 12: General operating conditions. All
I/Os are CMOS and TTL compliant.
Table 52. Output voltage characteristics for all I/Os except PC13, PC14 and PC15(1)
Symbol
Parameter
Conditions(3)
Min
Max
Unit
VOL
Output low level voltage
CMOS port(2)
IIO = 8 mA
2.7 V ≤ VDD ≤ 3.6 V
-
0.4
V
VOH
Output high level voltage
CMOS port(2)
IIO = −8 mA
2.7 V ≤ VDD ≤ 3.6 V
VDD − 0.4
-
VOL
(3)
Output low level voltage
TTL port(2)
IIO = 8 mA
2.7 V ≤ VDD ≤ 3.6 V
-
0.4
VOH
(3)
Output high level voltage
TTL port(2)
IIO = −8 mA
2.7 V ≤ VDD ≤ 3.6 V
2.4
-
VOL
(3)
Output low level voltage
IIO = 20 mA
2.7 V ≤ VDD ≤ 3.6 V
-
1.3
VOH
(3)
Output high level voltage
IIO = −20 mA
2.7 V ≤ VDD ≤ 3.6 V
VDD − 1.3
-
VOL
(3)
Output low level voltage
IIO = 4 mA
1.62 V ≤ VDD ≤ 3.6 V
-
0.4
VOH (3)
Output high level voltage
IIO = −4 mA
1.62 V ≤VDD < 3.6 V
VDD − 0.4
-
VOL
(3)
Output low level voltage for Pxy_C
pins
IIO = 1 mA
1.62 V ≤ VDD < 3.6 V
-
0.4
VOH
(3)
Output high level voltage for Pxy_C
pins(4)
IIO = 1 mA
1.62 V ≤ VDD < 3.6 V
Min(VDD − 0.4,
VDDA + 0.3)
-
VOLFM+
(3)
Output low level voltage for an FTf
I/O pin in FM+ mode
IIO = 20 mA
2.3 V ≤ VDD ≤ 3.6 V
-
0.4
IIO = 10 mA
1.62 V ≤ VDD ≤ 3.6 V
-
0.4
1.
The IIO current sourced or sunk by the device must always respect the absolute maximum rating specified in Table 9:
Voltage characteristics, and the sum of the currents sourced or sunk by all the I/Os (I/O ports and control pins) must always
respect the absolute maximum ratings ΣIIO.
2.
TTL and CMOS outputs are compatible with JEDEC standards JESD36 and JESD52.
3.
Guaranteed by design.
4.
If VDDA + 0.3V < VDD - 0.4 V, an injection current from VDD to VDDA can be observed that can perturb the analog
peripherals.

---

          Table 53. Output voltage characteristics for PC13, PC14 and PC15(1)
Symbol
Parameter
Conditions(3)
Min
Max
Unit
VOL
Output low level voltage
CMOS port(2)
IIO = 3 mA
2.7 V≤ VDD ≤3.6 V
-
0.4
V
VOH
Output high level voltage
CMOS port(2)
IIO = −3 mA
2.7 V≤ VDD ≤3.6 V
VDD−0.4
-
VOL
(3)
Output low level voltage
TTL port(2)
IIO = 3 mA
2.7 V≤ VDD ≤3.6 V
-
0.4
VOH
(2)
Output high level voltage
TTL port(2)
IIO = −3 mA
2.7 V≤ VDD ≤3.6 V
2.4
-
VOL
(2)
Output low level voltage
IIO = 1.5 mA
1.62 V≤ VDD ≤3.6 V
-
0.4
VOH
(2)
Output high level voltage
IIO = −1.5 mA
1.62 V≤ VDD ≤3.6 V
VDD−0.4
-
1.
The IIO current sourced or sunk by the device must always respect the absolute maximum rating specified in Table 9:
Voltage characteristics, and the sum of the currents sourced or sunk by all the I/Os (I/O ports and control pins) must always
respect the absolute maximum ratings ΣIIO.
2.
TTL and CMOS outputs are compatible with JEDEC standards JESD36 and JESD52.
3.
Guaranteed by design.

---

Output buffer timing characteristics (HSLV option disabled)
The HSLV bit of SYSCFG_CCCSR register can be used to optimize the I/O speed when the
product voltage is below 2.7 V.
Table 54. Output timing characteristics (HSLV OFF)(1)
Speed
Symbol
Parameter
conditions
Min
Max
Unit
Fmax
(2)
Maximum frequency
C=50 pF, 2.7 V≤ VDD≤3.6 V
-
MHz
C=50 pF, 1.62 V≤VDD≤2.7 V
-
C=30 pF, 2.7 V≤VDD≤3.6 V
-
C=30 pF, 1.62 V≤VDD≤2.7 V
-
C=10 pF, 2.7 V≤VDD≤3.6 V
-
C=10 pF, 1.62 V≤VDD≤2.7 V
-
tr/tf
(3)
Output high to low level
fall time and output low
to high level rise time
C=50 pF, 2.7 V≤ VDD≤3.6 V
-
16.6
ns
C=50 pF, 1.62 V≤VDD≤2.7 V
-
33.3
C=30 pF, 2.7 V≤VDD≤3.6 V
-
13.3
C=30 pF, 1.62 V≤VDD≤2.7 V
-
C=10 pF, 2.7 V≤VDD≤3.6 V
-
C=10 pF, 1.62 V≤VDD≤2.7 V
-
Fmax
(2)
Maximum frequency
C=50 pF, 2.7 V≤ VDD≤3.6 V
-
MHz
C=50 pF, 1.62 V≤VDD≤2.7 V
-
C=30 pF, 2.7 V≤VDD≤3.6 V
-
C=30 pF, 1.62 V≤VDD≤2.7 V
-
C=10 pF, 2.7 V≤VDD≤3.6 V
-
C=10 pF, 1.62 V≤VDD≤2.7 V
-
tr/tf
(3)
Output high to low level
fall time and output low
to high level rise time
C=50 pF, 2.7 V≤ VDD≤3.6 V
-
5.2
ns
C=50 pF, 1.62 V≤VDD≤2.7 V
-
C=30 pF, 2.7 V≤VDD≤3.6 V
-
4.2
C=30 pF, 1.62 V≤VDD≤2.7 V
-
7.5
C=10 pF, 2.7 V≤VDD≤3.6 V
-
2.8
C=10 pF, 1.62 V≤VDD≤2.7 V
-
5.2

---

Fmax
(2)
Maximum frequency
C=50 pF, 2.7 V≤VDD≤3.6 V(4)
-
MHz
C=50 pF, 1.62 V≤VDD≤2.7 V(4)
-
C=30 pF, 2.7 V≤VDD≤3.6 V(4)
-
C=30 pF, 1.62 V≤VDD≤2.7 V(4)
-
C=10 pF, 2.7 V≤VDD≤3.6 V(4)
-
C=10 pF, 1.62 V≤VDD≤2.7 V(4)
-
tr/tf
(3)
Output high to low level
fall time and output low
to high level rise time
C=50 pF, 2.7 V≤VDD≤3.6 V(4)
-
3.8
ns
C=50 pF, 1.62 V≤VDD≤2.7 V(4)
-
6.9
C=30 pF, 2.7 V≤VDD≤3.6 V(4)
-
2.8
C=30 pF, 1.62 V≤VDD≤2.7 V(4)
-
5.2
C=10 pF, 2.7 V≤VDD≤3.6 V(4)
-
1.8
C=10 pF, 1.62 V≤VDD≤2.7 Vv
-
3.3
Fmax
(2)
Maximum frequency
C=50 pF, 2.7 V≤VDD≤3.6 Vv
-
MHz
C=50 pF, 1.62 V≤VDD≤2.7 V(4)
-
C=30 pF, 2.7 V≤VDD≤3.6 Vv
-
C=30 pF, 1.62 V≤VDD≤2.7 V(4)
-
C=10 pF, 2.7 V≤VDD≤3.6 V(4)
-
C=10 pF, 1.62 V≤VDD≤2.7 V(4)
-
tr/tf
(3)
Output high to low level
fall time and output low
to high level rise time
C=50 pF, 2.7 V≤VDD≤3.6 V(4)
-
3.3
ns
C=50 pF, 1.62 V≤VDD≤2.7 V(4)
-
6.6
C=30 pF, 2.7 V≤VDD≤3.6 V(4)
-
2.4
C=30 pF, 1.62 V≤VDD≤2.7 V(4)
-
4.5
C=10 pF, 2.7 V≤VDD≤3.6 V(4)
-
1.5
C=10 pF, 1.62 V≤VDD≤2.7 V(4)
-
2.7
1.
Guaranteed by design.
2.
The maximum frequency is achieved with a duty cycle of 45 to 55 %, when loaded by the specified capacitance.
3.
The fall and rise times are defined between 90% and 10% and between 10% and 90% of the output waveform, respectively.
4.
Compensation system enabled.
Table 54. Output timing characteristics (HSLV OFF)(1) (continued)
Speed
Symbol
Parameter
conditions
Min
Max
Unit

---

Output buffer timing characteristics (HSLV option enabled)
Table 55. Output timing characteristics (HSLV ON)(1)
Speed
Symbol
Parameter
conditions
Min
Max
Unit
Fmax
(2)
Maximum frequency
C=50 pF, 1.62 V≤VDD≤2.7 V
-
MHz
C=30 pF, 1.62 V≤VDD≤2.7 V
-
C=10 pF, 1.62 V≤VDD≤2.7 V
-
tr/tf
(3)
Output high to low level
fall time and output low
to high level rise time
C=50 pF, 1.62 V≤VDD≤2.7 V
-
ns
C=30 pF, 1.62 V≤VDD≤2.7 V
-
C=10 pF, 1.62 V≤VDD≤2.7 V
-
6.6
Fmax
(2)
Maximum frequency
C=50 pF, 1.62 V≤VDD≤2.7 V
-
MHz
C=30 pF, 1.62 V≤VDD≤2.7 V
-
C=10 pF, 1.62 V≤VDD≤2.7 V
-
tr/tf
(3)
Output high to low level
fall time and output low
to high level rise time
C=50 pF, 1.62 V≤VDD≤2.7 V
-
6.6
ns
C=30 pF, 1.62 V≤VDD≤2.7 V
-
4.8
C=10 pF, 1.62 V≤VDD≤2.7 V
-
Fmax
(2)
Maximum frequency
C=50 pF, 1.62 V≤VDD≤2.7 V(4)
-
MHz
C=30 pF, 1.62 V≤VDD≤2.7 V(4)
-
C=10 pF, 1.62 V≤VDD≤2.7 V(4)
-
tr/tf
(3)
Output high to low level
fall time and output low
to high level rise time
C=50 pF, 1.62 V≤VDD≤2.7 V(4)
-
5.8
ns
C=30 pF, 1.62 V≤VDD≤2.7 V(4)
-
C=10 pF, 1.62 V≤VDD≤2.7 V(4)
-
2.4
Fmax
(2)
Maximum frequency
C=50 pF, 1.62 V≤VDD≤2.7 V(4)
-
MHz
C=30 pF, 1.62 V≤VDD≤2.7 V(4)
-
C=10 pF, 1.62 V≤VDD≤2.7 V(4)
-
tr/tf
(3)
Output high to low level
fall time and output low
to high level rise time
C=50 pF, 1.62 V≤VDD≤2.7 V(4)
-
5.3
ns
C=30 pF, 1.62 V≤VDD≤2.7 V(4)
-
3.6
C=10 pF, 1.62 V≤VDD≤2.7 V(4)
-
1.9
1.
Guaranteed by design.
2.
The maximum frequency is achieved with a duty cycle of 45 to 55 %, when loaded by the specified capacitance.
3.
The fall and rise times are defined between 90% and 10% and between 10% and 90% of the output waveform, respectively.
4.
Compensation system enabled.

---

Analog switch between ports Pxy_C and Pxy
PA0_C, PA1_C, PC2_C and PC3_C can be connected internally to PA0, PA1, PC2 and
PC3, respectively (refer to SYSCFG_PMCR register in RM0468 reference manual). The
switch is controlled by VDDSWITCH voltage level. It is defined through BOOSTVDDSEL bit of
SYSCFG_PMCR. If the switch is closed the switch characteristics are given in the table
below.
6.3.17
NRST pin characteristics
The NRST pin input driver uses CMOS technology. It is connected to a permanent pull-up
resistor, RPU (see Table 51: I/O static characteristics).
Unless otherwise specified, the parameters given in Table 57 are derived from tests
performed under the ambient temperature and VDD supply voltage conditions summarized
in Table 12: General operating conditions.
Table 56. Pxy_C and Pxy analog switch characteristics
Parameter
Conditions
Min
Typ
Max
Unit
Switch
impedance
Switch control boosted
-
-
Ω
Switch control
not boosted
VDDSWITCH > 2.7 V
-
-
VDDSWITCH > 2.4 V
-
-
VDDSWITCH > 2.0 V
-
-
VDDSWITCH > 1.8 V
-
-
VDDSWITCH > 1.62 V
-
-
Table 57. NRST pin characteristics
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
RPU
(2)
Weak pull-up equivalent
resistor(1)
1.
The pull-up is designed with a true resistance in series with a switchable PMOS. This PMOS contribution
to the series resistance must be minimum (~10% order).
VIN = VSS
㏀
VF(NRST)
(2)
2.
Guaranteed by design.
NRST Input filtered pulse
1.71 V < VDD < 3.6 V
-
-
ns
VNF(NRST)
(2) NRST Input not filtered pulse
1.71 V < VDD < 3.6 V
-
-
1.62 V < VDD < 3.6 V
-
-

---

Figure 18. Recommended NRST pin protection
1.
The reset network protects the device against parasitic resets.
2.
The user must ensure that the level on the NRST pin can go below the VIL(NRST) max level specified in
Table 51. Otherwise the reset is not taken into account by the device.
6.3.18
FMC characteristics
Unless otherwise specified, the parameters given in Table 58 to Table 71 for the FMC
interface are derived from tests performed under the ambient temperature, fHCLK frequency
and VDD supply voltage conditions summarized in Table 12: General operating conditions,
with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 11
•
Measurement points are done at CMOS levels: 0.5VDD
•
IO Compensation cell activated.
•
HSLV activated when VDD ≤ 2.7 V
•
VOS level set to VOS0.
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output alternate
function characteristics.
Asynchronous waveforms and timings
Figure 19 through Figure 21 represent asynchronous waveforms and Table 58 through
Table 65 provide the corresponding timings. The results shown in these tables are obtained
with the following FMC configuration:
•
AddressSetupTime = 0x1
•
AddressHoldTime = 0x1
•
DataSetupTime = 0x1 (except for asynchronous NWAIT mode , DataSetupTime = 0x5)
•
BusTurnAroundDuration = 0x0
•
Capacitive load CL = 30 pF
In all timing tables, the TKERCK is the fmc_ker_ck clock period.
ai14132d
STM32
RPU
NRST(2)
VDD
Filter
Internal Reset
0.1 μF
External
reset circuit(1)

---

Table 58. Asynchronous non-multiplexed SRAM/PSRAM/NOR read timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
3Tfmc_ker_ck–1
         3Tfmc_ker_ck+1
ns
tv(NOE_NE)
FMC_NEx low to FMC_NOE low
0.5
tw(NOE)
FMC_NOE low time
2Tfmc_ker_ck –1
2Tfmc_ker_ck+1
th(NE_NOE)
FMC_NOE high to FMC_NE high
hold time
 Tfmc_ker_ck
-
tv(A_NE)
FMC_NEx low to FMC_A valid
-
0.5
th(A_NOE)
Address hold time after
FMC_NOE high
2Tfmc_ker_ck
-
tsu(Data_NE)
Data to FMC_NEx high setup
time
Tfmc_ker_ck+14
-
tsu(Data_NOE)
Data to FMC_NOEx high setup
time
-
th(Data_NOE)
Data hold time after FMC_NOE
high
-
th(Data_NE)
Data hold time after FMC_NEx
high
-
tv(NADV_NE)
FMC_NEx low to FMC_NADV low
-
tw(NADV)
FMC_NADV low time
-
Tfmc_ker_ck+1
Table 59. Asynchronous non-multiplexed SRAM/PSRAM/NOR read-NWAIT
timings(1)(2)
1.
Guaranteed by characterization results.
2.
NWAIT pulse width is equal to 1 fmc_ker_ck cycle.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
7Tfmc_ker_ck–1
7Tfmc_ker_ck+1
ns
tw(NOE)
FMC_NOE low time
5Tfmc_ker_ck–1
5Tfmc_ker_ck +1
tw(NWAIT)
FMC_NWAIT low time
Tfmc_ker_ck– 0.5
         -
tsu(NWAIT_NE)
FMC_NWAIT valid before FMC_NEx
high
4Tfmc_ker_ck +9
-
th(NE_NWAIT)
FMC_NEx hold time after
FMC_NWAIT invalid
3Tfmc_ker_ck+12
-

---

Figure 19. Asynchronous non-multiplexed SRAM/PSRAM/NOR read waveforms
1.
Mode 2/B, C and D only. In Mode 1, FMC_NADV is not used.
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

Table 60. Asynchronous non-multiplexed SRAM/PSRAM/NOR write timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
3Tfmc_ker_ck –1
3Tfmc_ker_ck + 1
ns
tv(NWE_NE)
FMC_NEx low to FMC_NWE low
 Tfmc_ker_ck–1
Tfmc_ker_ck
tw(NWE)
FMC_NWE low time
 Tfmc_ker_ck –0.5
Tfmc_ker_ck+0.5
th(NE_NWE)
FMC_NWE high to FMC_NE high
hold time
 Tfmc_ker_ck
 -
tv(A_NE)
FMC_NEx low to FMC_A valid
 -
th(A_NWE)
Address hold time after FMC_NWE
high
Tfmc_ker_ck –0.5
 -
tv(BL_NE)
FMC_NEx low to FMC_BL valid
 -
0.5
th(BL_NWE)
FMC_BL hold time after FMC_NWE
high
Tfmc_ker_ck –0.5
 -
tv(Data_NE)
Data to FMC_NEx low to Data valid
 -
Tfmc_ker_ck+ 2
th(Data_NWE)
Data hold time after FMC_NWE high
Tfmc_ker_ck
 -
tv(NADV_NE)
FMC_NEx low to FMC_NADV low
 -
tw(NADV)
FMC_NADV low time
 -
Tfmc_ker_ck+ 1
Table 61. Asynchronous non-multiplexed SRAM/PSRAM/NOR write-NWAIT
timings(1)(2)
1.
Guaranteed by characterization results.
2.
NWAIT pulse width is equal to 1 fmc_ker_ck cycle.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
8Tfmc_ker_ck –1
8Tfmc_ker_ck+1
ns
tw(NWE)
FMC_NWE low time
6Tfmc_ker_ck –1
6Tfmc_ker_ck+1
tsu(NWAIT_NE)
FMC_NWAIT valid before FMC_NEx
high
5Tfmc_ker_ck+13
-
th(NE_NWAIT)
FMC_NEx hold time after
FMC_NWAIT invalid
4Tfmc_ker_ck+12
-

---

Figure 20. Asynchronous non-multiplexed SRAM/PSRAM/NOR write waveforms
1.
Mode 2/B, C and D only. In Mode 1, FMC_NADV is not used.
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

Table 62. Asynchronous multiplexed PSRAM/NOR read timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
4Tfmc_ker_ck –1
4Tfmc_ker_ck +1
ns
tv(NOE_NE)
FMC_NEx low to FMC_NOE low
2Tfmc_ker_ck
2Tfmc_ker_ck
+0.5
ttw(NOE)
FMC_NOE low time
Tfmc_ker_ck –1
Tfmc_ker_ck+1
th(NE_NOE)
FMC_NOE high to FMC_NE high hold
time
 Tfmc_ker_ck
 -
tv(A_NE)
FMC_NEx low to FMC_A valid
 -
0.5
tv(NADV_NE)
FMC_NEx low to FMC_NADV low
4.0
tw(NADV)
FMC_NADV low time
Tfmc_ker_ck –0.5
Tfmc_ker_ck +1
th(AD_NADV)
FMC_AD(address) valid hold time
after FMC_NADV high)
Tfmc_ker_ckk –4
 -
th(A_NOE)
Address hold time after FMC_NOE
high
Tfmc_ker_ck –0.5
 -
tsu(Data_NE)
Data to FMC_NEx high setup time
Tfmc_ker_ck +14
 -
tsu(Data_NOE)
Data to FMC_NOE high setup time
 -
th(Data_NE)
Data hold time after FMC_NEx high
-
th(Data_NOE)
Data hold time after FMC_NOE high
-
Table 63. Asynchronous multiplexed PSRAM/NOR read-NWAIT timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
8Tfmc_ker_ck –1
8Tfmc_ker_ck +1
ns
tw(NOE)
FMC_NWE low time
5Tfmc_ker_ck –1
5Tfmc_ker_ck +1
tsu(NWAIT_NE)
FMC_NWAIT valid before
FMC_NEx high
4Tfmc_ker_ck +9
-
th(NE_NWAIT)
FMC_NEx hold time after
FMC_NWAIT invalid
3Tfmc_ker_ck +12
-

---

Figure 21. Asynchronous multiplexed PSRAM/NOR read waveforms
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

Table 64. Asynchronous multiplexed PSRAM/NOR write timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
4Tfmc_ker_ck –1
4Tfmc_ker_ck
ns
tv(NWE_NE)
FMC_NEx low to FMC_NWE low
Tfmc_ker_ck –1
Tfmc_ker_ck +0.5
tw(NWE)
FMC_NWE low time
2Tfmc_ker_ck –0.5
2Tfmc_ker_ck +0.5
th(NE_NWE)
FMC_NWE high to FMC_NE high hold
time
Tfmc_ker_ck –0.5
-
tv(A_NE)
FMC_NEx low to FMC_A valid
-
tv(NADV_NE)
FMC_NEx low to FMC_NADV low
5.0
tw(NADV)
FMC_NADV low time
Tfmc_ker_ck –0.5
Tfmc_ker_ck + 1
th(AD_NADV)
FMC_AD(adress) valid hold time after
FMC_NADV high)
Tfmc_ker_ck –4.5
-
th(A_NWE)
Address hold time after FMC_NWE
high
Tfmc_ker_ck – 0.5
-
th(BL_NWE)
FMC_BL hold time after FMC_NWE
high
Tfmc_ker_ck – 0.5
-
tv(BL_NE)
FMC_NEx low to FMC_BL valid
-
0.5
tv(Data_NADV)
FMC_NADV high to Data valid
-
Tfmc_ker_ck +2
th(Data_NWE)
Data hold time after FMC_NWE high
Tfmc_ker_ck
-
Table 65. Asynchronous multiplexed PSRAM/NOR write-NWAIT timings(1)(2)
1.
Guaranteed by characterization results.
2.
NWAIT pulse width is equal to 1 fmc_ker_ck cycle.
Symbol
Parameter
Min
Max
Unit
tw(NE)
FMC_NE low time
9Tfmc_ker_ck –1
9Tfmc_ker_ck
ns
tw(NWE)
FMC_NWE low time
7Tfmc_ker_ck –0.5
7Tfmc_ker_ck +0.5
tsu(NWAIT_NE)
FMC_NWAIT valid before FMC_NEx
high
5Tfmc_ker_ck +9
-
th(NE_NWAIT)
FMC_NEx hold time after
FMC_NWAIT invalid
4Tfmc_ker_ck +12
-

---

Figure 22. Asynchronous multiplexed PSRAM/NOR write waveforms
Synchronous waveforms and timings
Figure 25 through Figure 24 represent synchronous waveforms and Table 68 through
Table 67 provide the corresponding timings. The results shown in these tables are obtained
with the following FMC configuration:
•
BurstAccessMode = FMC_BurstAccessMode_Enable
•
MemoryType = FMC_MemoryType_CRAM
•
WriteBurst = FMC_WriteBurst_Enable
•
CLKDivision = 1
•
DataLatency = 1 for NOR flash, DataLatency = 0 for PSRAM, CL = 30 pF
In all the timing tables, the Tfmc_ker_ck is the fmc_ker_ck clock period, with the following
FMC_CLK maximum values:
•
For 2.7 V<VDD<3.6 V: maximum FMC_CLK = 137 MHz at CL = 20 pF
•
For 1.8 V<VDD<1.9 V: maximum FMC_CLK = 100 MHz at CL = 20 pF
•
For 1.62 V<VDD<1.8 V: maximumFMC_CLK = 88 MHz at CL = 15 pF
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

---

Table 66. Synchronous non-multiplexed NOR/PSRAM read timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(CLK)
FMC_CLK period
2Tfmc_ker_ck –0.5
 -
ns
t(CLKL-NExL)
FMC_CLK low to FMC_NEx low (x=0..2)
 -
td(CLKH-NExH)
FMC_CLK high to FMC_NEx high (x= 0…2)
Tfmc_ker_ck+1.5
 -
td(CLKL-NADVL)
FMC_CLK low to
FMC_NADV low
1.62 V <VDD < 3.6 V
 -
5.5
2.7 V <VDD < 3.6 V
2.0
td(CLKL-NADVH)
FMC_CLK low to
FMC_NADV high
1.62 V <VDD < 3.6 V
 -
2.7 V <VDD < 3.6 V
-
td(CLKL-AV)
FMC_CLK low to FMC_Ax valid (x=16…25)
 -
td(CLKH-AIV)
FMC_CLK high to FMC_Ax invalid (x=16…25)
Tfmc_ker_ck
 -
td(CLKL-NOEL)
FMC_CLK ow to FMC_NOE low
 -
2.5
td(CLKH-NOEH)
FMC_CLK high to FMC_NOE high
Tfmc_ker_ck+1
 -
tsu(DV-CLKH)
FMC_D[15:0] valid data before FMC_CLK high
 -
th(CLKH-DV)
FMC_D[15:0] valid data after FMC_CLK high
 -
t(NWAIT-CLKH)
FMC_NWAIT valid before FMC_CLK high
-
th(CLKH-NWAIT)
FMC_NWAIT valid after FMC_CLK high
2.5
-

---

Figure 23. Synchronous non-multiplexed NOR/PSRAM read timings
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

Table 67. Synchronous non-multiplexed PSRAM write timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
t(CLK)
FMC_CLK period
2Tfmc_ker_ck –0.5
 -
ns
td(CLKL-NExL)
FMC_CLK low to FMC_NEx low (x=0..2)
 -
t(CLKH-NExH)
FMC_CLK high to FMC_NEx high (x= 0…2)
 Tfmc_ker_ck+1.5
 -
td(CLKL-NADVL)
FMC_CLK low to
FMC_NADV low
1.62 V <VDD < 3.6 V
 -
5.5
2.7 V <VDD < 3.6 V
td(CLKL-NADVH)
FMC_CLK low to
FMC_NADV high
1.62 V <VDD < 3.6 V
 -
2.7 V <VDD < 3.6 V
-
td(CLKL-AV)
FMC_CLK low to FMC_Ax valid (x=16…25)
 -
td(CLKH-AIV)
FMC_CLK high to FMC_Ax invalid (x=16…25)
Tfmc_ker_ck
 -
td(CLKL-NWEL)
FMC_CLK low to FMC_NWE low
 -
2.5
td(CLKH-NWEH)
FMC_CLK high to FMC_NWE high
Tfmc_ker_ck+1
 -
td(CLKL-Data)
FMC_D[15:0] valid data after FMC_CLK low
 -
3.5
td(CLKL-NBLL)
FMC_CLK low to FMC_NBL low
-
td(CLKH-NBLH)
FMC_CLK high to FMC_NBL high
Tfmc_ker_ck+0.5
 -
tsu(NWAIT-
CLKH)
FMC_NWAIT valid before FMC_CLK high
-
th(CLKH-NWAIT)
FMC_NWAIT valid after FMC_CLK high
2.5
-

---

Figure 24. Synchronous non-multiplexed PSRAM write timings
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

Table 68. Synchronous multiplexed NOR/PSRAM read timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(CLK)
FMC_CLK period
2Tfmc_ker_ck –0.5
 -
ns
td(CLKL-NExL)
FMC_CLK low to FMC_NEx low (x=0..2)
 -
td(CLKH_NExH)
FMC_CLK high to FMC_NEx high (x= 0…2)
Tfmc_ker_ck+1.5
 -
td(CLKL-NADVL)
FMC_CLK low to
FMC_NADV low
1.62 V <VDD < 3.6 V
 -
5.5
2.7 V <VDD < 3.6 V
td(CLKL-NADVH)
FMC_CLK low to
FMC_NADV high
1.62 V <VDD < 3.6 V
 -
2.7 V <VDD < 3.6 V
-
td(CLKL-AV)
FMC_CLK low to FMC_Ax valid (x=16…25)
 -
td(CLKH-AIV)
FMC_CLK high to FMC_Ax invalid (x=16…25)
Tfmc_ker_ck
 -
td(CLKL-NOEL)
FMC_CLK low to FMC_NOE low
 -
2.5
td(CLKH-NOEH)
FMC_CLK high to FMC_NOE high
Tfmc_ker_ck +1
 -
td(CLKL-ADV)
FMC_CLK low to FMC_AD[15:0] valid
 -
td(CLKL-ADIV)
FMC_CLK low to FMC_AD[15:0] invalid
 -
tsu(ADV-CLKH)
FMC_A/D[15:0] valid data before FMC_CLK high
 -
th(CLKH-ADV)
FMC_A/D[15:0] valid data after FMC_CLK high
 -
tsu(NWAIT-
CLKH)
FMC_NWAIT valid before FMC_CLK high
-
th(CLKH-NWAIT)
FMC_NWAIT valid after FMC_CLK high
2.5
-

---

Figure 25. Synchronous multiplexed NOR/PSRAM read timings
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

Table 69. Synchronous multiplexed PSRAM write timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(CLK)
FMC_CLK period, VDD = 2.7 to 3.6 V
2Tfmc_ker_ck –0.5
 -
ns
td(CLKL-NExL)
FMC_CLK low to FMC_NEx low (x =0..2)
 -
td(CLKH-NExH)
FMC_CLK high to FMC_NEx high
(x = 0…2)
Tfmc_ker_ck +1.5
 -
td(CLKL-NADVL)
FMC_CLK low to
FMC_NADV low
1.62 V <VDD < 3.6 V
 -
5.5
2.7 V <VDD < 3.6 V
2.0
td(CLKL-NADVH)
FMC_CLK low to
FMC_NADV high
1.62 V <VDD < 3.6 V
 -
2.7 V <VDD < 3.6 V
-
td(CLKL-AV)
FMC_CLK low to FMC_Ax valid (x =16…25)
 -
td(CLKH-AIV)
FMC_CLK high to FMC_Ax invalid (x =16…25)
Tfmc_ker_ck
 -
td(CLKL-NWEL)
FMC_CLK low to FMC_NWE low
 -
2.5
t(CLKH-NWEH)
FMC_CLK high to FMC_NWE high
Tfmc_ker_ck +1
 -
td(CLKL-ADV)
FMC_CLK low to to FMC_AD[15:0] valid
-
2.5
td(CLKL-ADIV)
FMC_CLK low to FMC_AD[15:0] invalid
 -
td(CLKL-DATA)
FMC_A/D[15:0] valid data after FMC_CLK low
 -
3.5
td(CLKL-NBLL)
FMC_CLK low to FMC_NBL low
-
td(CLKH-NBLH)
FMC_CLK high to FMC_NBL high
Tfmc_ker_ck +0.5
-
tsu(NWAIT-CLKH)
FMC_NWAIT valid before FMC_CLK high
-
th(CLKH-NWAIT)
FMC_NWAIT valid after FMC_CLK high
2.5
-

---

Figure 26. Synchronous multiplexed PSRAM write timings
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

NAND controller waveforms and timings
Figure 27 through Figure 30 represent synchronous waveforms, and Table 70 and Table 71
provide the corresponding timings. The results shown in this table are obtained with the
following FMC configuration and a capacitive load (CL) of 30 pF:
•
COM.FMC_SetupTime = 0x01
•
COM.FMC_WaitSetupTime = 0x03
•
COM.FMC_HoldSetupTime = 0x02
•
COM.FMC_HiZSetupTime = 0x01
•
ATT.FMC_SetupTime = 0x01
•
ATT.FMC_WaitSetupTime = 0x03
•
ATT.FMC_HoldSetupTime = 0x02
•
ATT.FMC_HiZSetupTime = 0x01
•
Bank = FMC_Bank_NAND
•
MemoryDataWidth = FMC_MemoryDataWidth_16b
•
ECC = FMC_ECC_Enable
•
ECCPageSize = FMC_ECCPageSize_512Bytes
•
TCLRSetupTime = 0
•
TARSetupTime = 0
In all timing tables, the Tfmc_ker_ck is the fmc_ker_ck clock period.
Table 70. Switching characteristics for NAND flash read cycles(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(N0E)
FMC_NOE low width
4Tfmc_ker_ck – 0.5
4Tfmc_ker_ck+0.5
ns
tsu(D-NOE)
FMC_D[15-0] valid data before
FMC_NOE high
-
th(NOE-D)
FMC_D[15-0] valid data after
FMC_NOE high
-
td(ALE-NOE)
FMC_ALE valid before FMC_NOE low
-
3Tfmc_ker_ck +0.5
th(NOE-ALE)
FMC_NWE high to FMC_ALE invalid
4Tfmc_ker_ck –1
-

---

Figure 27. NAND controller waveforms for read access
1.
y = 7 or 15 depending on the NAND flash memory interface.
Table 71. Switching characteristics for NAND flash write cycles(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(NWE)
FMC_NWE low width
4Tfmc_ker_ck – 0.5
4Tfmc_ker_ck +0.5
ns
tv(NWE-D)
FMC_NWE low to FMC_D[15-0]
valid
-
th(NWE-D)
FMC_NWE high to FMC_D[15-0]
invalid
2Tfmc_ker_ck +1.5
-
td(D-NWE)
FMC_D[15-0] valid before
FMC_NWE high
5Tfmc_ker_ck – 5
-
td(ALE-NWE)
FMC_ALE valid before FMC_NWE
low
-
3Tfmc_ker_ck +0.5
th(NWE-ALE)
FMC_NWE high to FMC_ALE invalid
2Tfmc_ker_ck – 0.5
-
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

---

Figure 28. NAND controller waveforms for write access
1.
y = 7 or 15 depending on the NAND flash memory interface.
SDRAM waveforms and timings
In all timing tables, the TKERCK is the fmc_ker_ck clock period, with the following
FMC_SDCLK maximum values:
•
For 2.7 V<VDD<3.6 V: maximum FMC_CLK = 95 MHz at 20 pF
•
For 1.8 V<VDD<1.9 V: maximum FMC_CLK = 90 MHz at 20 pF
•
For 1.62 V<DD<1.8 V: maximum FMC_CLK = 85 MHz at 15 pF
Table 72. SDRAM read timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(SDCLK)
FMC_SDCLK period
2Tfmc_ker_ck –
0.5
2Tfmc_ker_ck
+0.5
ns
tsu(SDCLKH _Data)
Data input setup time
-
th(SDCLKH_Data)
Data input hold time
1.5
-
td(SDCLKL_Add)
Address valid time
-
2.0
td(SDCLKL- SDNE)
Chip select valid time
-
1.5(2)
2.
Using PC2_C I/O adds 4.5 ns to this timing.
th(SDCLKL_SDNE)
Chip select hold time
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
2.0
th(SDCLKL_SDNCAS)
SDNCAS hold time
0.5
-
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

Figure 29. SDRAM read access waveforms (CL = 1)
Table 73. LPSDR SDRAM read timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tW(SDCLK)
FMC_SDCLK period
2Tfmc_ker_ck –
0.5
2Tfmc_ker_ck+0.5
ns
tsu(SDCLKH_Data)
Data input setup time
-
th(SDCLKH_Data)
Data input hold time
2.5
-
td(SDCLKL_Add)
Address valid time
-
td(SDCLKL_SDNE)
Chip select valid time
-
1.5(2)(3)
2.
Using PC2 I/O adds 4 ns to this timing.
3.
Using PC2_C I/O adds 16.5 ns to this timing.
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
0.5
-
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

Table 74. SDRAM Write timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(SDCLK)
FMC_SDCLK period
2Tfmc_ker_ck – 0.5
2Tfmc_ker_ck+0.5
ns
td(SDCLKL _Data)
Data output valid time
-
th(SDCLKL _Data)
Data output hold time
0.5
-
td(SDCLKL_Add)
Address valid time
-
td(SDCLKL_SDNWE)
SDNWE valid time
-
th(SDCLKL_SDNWE)
SDNWE hold time
-
td(SDCLKL_ SDNE)
Chip  select valid time
-
1.5(2)
2.
Using PC2_C I/O adds 4.5 ns to this timing.
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
td(SDCLKL_SDNCAS)
SDNCAS hold time
0.5
-
Table 75. LPSDR SDRAM Write timings(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tw(SDCLK)
FMC_SDCLK period
2Tfmc_ker_ck – 0.5 2Tfmc_ker_ck+0.5
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
2.5
td(SDCLKL-SDNWE)
SDNWE valid time
-
th(SDCLKL-SDNWE)
SDNWE hold time
-
td(SDCLKL- SDNE)
Chip  select valid time
-
1.5(2)(3)
2.
Using PC2 I/O adds 4 ns to this timing.
3.
Using PC2_C I/O adds 16.5 ns to this timing.
th(SDCLKL- SDNE)
Chip  select hold time
-
td(SDCLKL-SDNRAS)
SDNRAS valid time
-
th(SDCLKL-SDNRAS)
SDNRAS hold time
-
td(SDCLKL-SDNCAS)
SDNCAS valid time
-
td(SDCLKL-SDNCAS)
SDNCAS hold time
0.5
-

---

Figure 30. SDRAM write access waveforms
6.3.19
Octo-SPI interface characteristics
Unless otherwise specified, the parameters given in Table 76 and Table 78 for OCTOSPI
are derived from tests performed under the ambient temperature, fHCLK frequency and VDD
supply voltage conditions summarized in Table 12: General operating conditions, with the
following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 11
•
Measurement points are done at CMOS levels: 0.5VDD
•
IO Compensation cell activated.
•
HSLV activated when VDD ≤ 2.5 V
•
VOS level set to VOS0
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output alternate
function characteristics.
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

Figure 31. OCTOSPI SDR read/write timing diagram
Table 76. OCTOSPI characteristics in SDR mode(1)(2)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
F(CLK)
OCTOSPI clock frequency
1.71 V < VDD < 3.6 V,
VOS0,
CLOAD = 15 pF
-
-
MHz
1.71 V < VDD < 3.6 V,
VOS0, CLOAD =20 pF
-
-
2.7 V < VDD < 3.6 V,
VOS0,
CLOAD = 20 pF
-
-
tw(CKH)
OCTOSPI clock high and low
time, even division
PRESCALER[7:0] = n
 = 0,1,3,5
t(CK)/2
-
t(CK)/2+1
ns
tw(CKL)
t(CK)/2–1
-
t(CK)/2
tw(CKH)
OCTOSPI clock high and low
time, odd division
PRESCALER[7:0] = n
 = 2,4,6,8
(n/2)*t(CK)/
(n+1)
-
(n/2)*t(CK)/
(n+1)+1
tw(CKL)
(n/2+1)*t(CK)/
(n+1)–1
-
(n/2+1)*t(CK)
/(n+1)
ts(IN)
(3)
Data input setup time
-
3.0
-
-
th(IN)
(3)
Data input hold time
-
1.5
-
-
tv(OUT)
Data output valid time
-
-
0.5
1(4)
th(OUT)
Data output hold time
-
-
-
1.
All values apply to Octal and Quad-SPI mode.
2.
Guaranteed by characterization results.
3.
Delay block bypassed.
4.
Using PC2 or PC3 I/O in the data bus adds 4 ns to this timing value.
MSv36878V3
Data output
D0
D1
D2
Clock
Data input
D0
D1
D2
t(CLK)
tw(CLKH)
tw(CLKL)
tr(CLK)
tf(CLK)
ts(IN)
th(IN)
tv(OUT)
th(OUT)

---

Figure 32. OCTOSPI DTR mode timing diagram
Table 77. OCTOSPI characteristics in DTR mode (no DQS)(1)(2)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
FCK
(3)
OCTOSPI clock frequency
1.71 V < VDD < 3.6 V,
VOS0, CLOAD = 15 pF
-
-
90(4)
MHz
1.71 V < VDD < 3.6 V,
VOS0, CLOAD = 20 pF
-
-
87(4)
2.7 V < VDD < 3.6 V,
VOS0, CLOAD = 20 pF
-
-
tw(CKH)
OCTOSPI clock high and
low time, even division
PRESCALER[7:0] = n
= 0,1,3,5
t(CK)/2
-
t(CK)/2+1
ns
tw(CKL)
t(CK)/2–1
-
t(CK)/2
tw(CKH)
OCTOSPI clock high and
low time, odd division
PRESCALER[7:0] = n
= 2,4,6,8
(n/2)*t(CK)/
(n+1)
-
(n/2)*t(CK)/
(n+1)+1
tw(CKL)
(n/2+1)*t(CK)/(
n+1) – 1
-
(n/2+1)*
t(CK)/(n+1)
tsr(IN)
tsf(IN)
(5)
Data input setup time
-
3.0
-
-
thr(IN)
thf(IN)
(5)
Data input hold time
-
1.50
-
-
tvr(OUT)
tvf(OUT)
Data output valid time
DHQC = 0
-
7(6)
DHQC = 1,
Prescaler = 1,2 ...
-
tpclk/4+
tpclk/4+1.25
(6)
thr(OUT)
thf(OUT)
Data output hold time
DHQC = 0
4.5
-
-
DHQC = 1,
Prescaler = 1,2 ...
tpclk/4
-
-
1.
All values apply to Octal and Quad-SPI mode.
2.
Guaranteed by characterization results.
3.
DHQC must be set to reach the mentioned frequency.
4.
Using PC2 or PC3 I/O in the data bus decreases the frequency to 47 MHz.
5.
Delay block bypassed.
6.
Using PC2 or PC3 I/O in the data bus adds 4 ns to this timing value.
MSv36879V4
Data output
D0
D2
D4
Clock
Data input
D0
D2
D4
t(CLK)
tw(CLKH)
tw(CLKL)
tr(CLK)
tf(CLK)
tsf(IN)thf(IN)
tvf(OUT)
thr(OUT)
D1
D3
D5
D1
D3
D5
tvr(OUT)
thf(OUT)
tsr(IN)thr(IN)

---

Table 78. OCTOSPI characteristics in DTR mode (with DQS)/Octal and Hyperbus(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
FCK
(2)(3)
OCTOSPI clock frequency
2,7 V < VDD < 3.6 V,
VOS0, CLOAD = 20 pF
-
-
MHz
1.71 V < VDD < 3.6 V,
VOS0, CLOAD = 20 pF
-
-
100(4)
tw(CKH)
OCTOSPI clock high and
low time, even division
PRESCALER[7:0] = n =
0,1,3,5
t(CK)/2
-
t(CK)/2+1
ns
tw(CKL)
t(CK)/2–1
-
t(CK)/2
tw(CKH)
OCTOSPI clock high and
low time, odd division
PRESCALER[7:0] = n =
2,4,6,8
(n/2)*t(CK)/
(n+1)
-
(n/2)*t(CK)/
(n+1)+1
ns
tw(CKL)
(n/2+1)*t(CK)/(
n+1)–1
-
(n/2+1)*t(CK)/
(n+1)
tv(CK)
Clock valid time
-
-
-
t(CK)+1
th(CK)
Clock hold time
-
t(CK)/2
-
-
VODr(CK)
CK,CK crossing level on CK
rising edge
VDD = 1.8 V
-
mV
VODf(CK)
CK,CK crossing level on CK
falling edge
VDD = 1.8 V
-
tw(CS)
Chip select high time
-
3*t(CK)
-
-
ns
tv(DQ)
Data input vallid time
-
-
-
tv(DS)
Data strobe input valid time
-
-
-
th(DS)
Data strobe input hold time
-
-
-
tv(RWDS)
Data strobe output valid
time
-
-
-
3 x t(CK)
tsr(DQ)
Data input setup time
Rising edge
-
-
tsf(DQ)
Falling edge
-
-
thr(DQ)
Data input hold time
Rising edge
-
-
thf(DQ)
Falling edge
-
-
tvr(OUT)
Data output valid time rising
edge
DHQC = 0
-
7(5)
DHQC = 1,
Prescaler = 1,2...
-
tpclk/4+
tpclk/4+1.25
(5)
tvf(OUT)
Data output valid time
falling edge
DHQC = 0
-
5.5
6(5)
DHQC = 1,
Prescaler = 1,2...
-
tpclk/4+
0.5
tpclk/4+0.75
(5)
thr(OUT)
Data output hold time rising
edge
DHQC = 0
4.5
-
-
DHQC = 1,
Prescaler = 1,2...
tpclk/4
-
-
thf(OUT)
Data output hold time falling
edge
DHQC = 0
4.5
-
-
DHQC = 1,
Prescaler = 1,2...
tpclk/4
-
-
1.
Guaranteed by characterization results.

---

Figure 33. OCTOSPI Hyperbus clock timing diagram
Figure 34. OCTOSPI Hyperbus read timing diagram
2.
Maximum frequency values are given for a RWDS to DQ skew of maximum +/-1.0 ns.
3.
Activating DHQC is mandatory to reach this frequency
4.
Using PC2 or PC3 I/O on data bus decreases the frequency to 47 MHz.
5.
Using PC2 or PC3 I/O on the data bus adds 4 ns to this timing value.
MSv47732V3
CLK
t(NCLK)
tw(NCLKL)
tw(NCLKH)
tf(NCLK)
tr(NCLK)
tr(CLK)
tw(CLKH)
tw(CLKL)
t(CLK)
tf(CLK)
NCLK
VOD(CLK)
MSv47733V3
NCS
t  ACC  = Initial access
Latency count
Command address
47:40 39:32
31:24
23:16
15:8
7:0
Dn
A
Dn
B
Dn+1
A
Dn+1
B
Host drives DQ[7:0] and the memory drives RWDS.
CLK, NCLK
RWDS
DQ[7:0]
Memory drives DQ[7:0] and RWDS.
tw(CS)
tv(RWDS)
tv(CLK)
tv(DS)
tv(DQ)
th(CLK)
th(DS)
tv(OUT)
th(OUT)
th(DQ)
ts(DQ)

---

Figure 35. OCTOSPI Hyperbus write timing diagram
6.3.20
Delay block (DLYB) characteristics
Unless otherwise specified, the parameters given in Table 79 for Delay Block are derived
from tests performed under the ambient temperature, frcc_c_ck frequency and VDD supply
voltage summarized in Table 12: General operating conditions, with the following
configuration:
6.3.21
16-bit ADC characteristics
Unless otherwise specified, the parameters given in Table 80, Table 81 and Table 82 are
derived from tests performed under the ambient temperature, fHCLK frequency and VDDA
supply voltage conditions summarized in Table 12: General operating conditions.
MSv47734V3
NCS
Access latency
Latency count
Command address
47:40
39:32
31:24
23:16
15:8
7:0
Dn
A
Dn
B
Dn+1
A
Dn+1
B
Host drives DQ[7:0] and the memory drives RWDS.
Host drives DQ[7:0] and RWDS.
CLK, NCLK
RWDS
DQ[7:0]
tw(CS)
tv(RWDS)
tv(CLK)
th(CLK)
High = 2x latency count
Low = 1x latency count
Read write recovery
th(OUT)
tv(OUT)
th(OUT)
tv(OUT)
th(OUT)
tv(OUT)
Table 79. Delay Block characteristics
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit
tinit
Initial delay
-
ps
t∆
Unit Delay
-
-
Table 80. 16-bit ADC characteristics(1)(2)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
VDDA
Analog supply
voltage for ADC
ON
-
1.62
-
3.6
V
VREF+
Positive
reference voltage
VDDA ≥ 2 V
1.62
-
VDDA
VDDA < 2 V
VDDA
VREF-
Negative
reference voltage
-
VSSA

---

fADC
ADC clock
frequency
1.62 V ≤ VDDA ≤ 3.6 V
BOOST = 11
0.12
-
MHz
BOOST = 10
0.12
-
BOOST = 01
0.12
-
12.5
BOOST = 00
-
-
6.25
fs
(3)
Sampling rate for
Direct channels
Resolution = 16 bits,
VDDA >2.5 V
TJ = 90 °C
fADC = 36 MHz
SMP = 1.5
-
-
3.60
MSps
Resolution = 16 bits
fADC = 37 MHz
SMP = 2.5
-
-
3.35
Resolution = 14 bits
TJ = 125 °C
fADC = 50 MHz
SMP = 2.5
-
-
5.00
Resolution = 12 bits
fADC = 50 MHz
SMP = 2.5
-
-
5.50
Resolution = 10 bits
fADC = 50 MHz
SMP = 1.5
-
-
7.10
Resolution = 8 bits
fADC = 50 MHz
SMP = 1.5
-
-
8.30
Resolution = 14 bits
TJ = 140 °C
fADC = 49 MHz
SMP = 1.5
-
-
4.90
Resolution = 12 bits
fADC = 50 MHz
SMP = 1.5
-
-
5.50
Resolution = 10 bits
fADC = 50 MHz
SMP = 1.5
-
-
6.70
Resolution = 8 bits
fADC = 50 MHz
SMP = 1.5
-
-
8.30
Sampling rate for
Fast channels
Resolution = 16 bits,
VDDA >2.5 V
TJ = 90 °C
fADC = 32 MHz
SMP = 2.5
-
-
2.90
Resolution = 16 bits
fADC = 31 MHz
SMP = 2.5
-
-
2.80
Resolution = 14 bits
TJ = 125 °C
fADC = 33 MHz
SMP = 2.5
-
-
3.30
Resolution = 12 bits
fADC = 39 MHz
SMP = 2.5
-
-
4.30
Resolution = 10 bits
fADC = 48 MHz
SMP = 2.5
-
-
6.00
Resolution = 8 bits
fADC = 50 MHz
SMP = 2.5
-
-
7.10
Resolution = 12 bits
TJ = 140 °C
fADC = 37 MHz
SMP = 2.5
-
-
4.10
Resolution = 10 bits
fADC = 46 MHz
SMP = 2.5
-
-
5.70
Resolution = 8 bits
fADC = 50 MHz
SMP = 2.5
-
-
7.10
Sampling rate for
Slow channels(4)
Resolution = 16 bits
TJ = 90 °C
-
-
1.00
resolution = 14 bits
TJ = 125 °C
-
-
resolution = 12 bits
-
-
resolution = 10 bits
-
-
resolution = 8 bits
-
-
resolution = 12 bits
TJ = 140 °C
-
-
resolution = 10 bits
-
-
resolution = 8 bits
-
-
VAIN
(5)
Conversion
voltage range
-
-
VREF+
V
VCMIV
Common mode
input voltage
-
VREF/2
− 10%
VREF/
VREF/2
+ 10%
V
Table 80. 16-bit ADC characteristics(1)(2) (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit

---

RAIN
(6)
External input
impedance
Resolution = 16 bits, TJ = 125 °C
-
-
-
-
Ω
Resolution = 14 bits, TJ = 125 °C
-
-
-
-
Resolution = 12 bits, TJ =125 °C
-
-
-
-
1,150
Resolution = 10 bits, TJ = 125 °C
-
-
-
-
5,650
Resolution = 8 bits, TJ = 125 °C
-
-
-
-
26,500
CADC
Internal sample
and hold
capacitor
-
-
-
pF
tADCVREG
_STUP
ADC LDO startup
time
-
-
us
tSTAB
ADC Power-up
time
LDO already started
-
-
conver
sion
cycle
tCAL
Offset and
linearity
calibration time
-
16,5010
1/fADC
tOFF_
CAL
Offset calibration
time
-
1,280
1/fADC
tLATR
Trigger
conversion
latency regular
and injected
channels without
conversion abort
CKMODE = 00
1.5
2.5
1/fADC
CKMODE = 01
-
-
2.5
CKMODE = 10
-
-
2.5
CKMODE = 11
-
-
2.25
tLATRINJ
Trigger
conversion
latency regular
injected channels
aborting a regular
conversion
CKMODE = 00
2.5
3.5
1/fADC
CKMODE = 01
-
-
3.5
CKMODE = 10
-
-
3.5
CKMODE = 11
-
-
3.25
tS
Sampling time
-
1.5
-
810.5
1/fADC
tCONV
Total conversion
time (including
sampling time)
Resolution = N bits
ts + 0.5
+ N/2
-
-
1/fADC
tTRIG
External trigger
period
-
tCONV
-
-
1/fADC
Table 80. 16-bit ADC characteristics(1)(2) (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit

---

IDDA_D
(ADC)
ADC
consumption on
VDDA,
BOOST=11,
Differential mode
Resolution = 16 bits, fADC = 25 MHz
-
-
-
1,440
-
µA
Resolution = 14 bits, fADC = 30 MHz
-
-
-
1,350
-
Resolution = 12 bits, fADC = 40 MHz
-
-
-
-
ADC
consumption on
VDDA,
BOOST=10,
Differential mode,
fADC = 25 MHz
Resolution = 16 bits
-
-
-
1,080
-
Resolution = 14 bits
-
-
-
-
Resolution = 12 bits
-
-
-
-
ADC
consumption on
VDDA,
BOOST=01,
Differential mode,
fADC = 12.5 MHz
Resolution = 16 bits
-
-
-
-
Resolution = 14 bits
-
-
-
-
Resolution = 12 bits
-
-
-
-
ADC
consumption on
VDDA,
BOOST=00,
Differential mode,
fADC = 6.25 MHz
Resolution = 16 bits
-
-
-
-
Resolution = 14 bits
-
-
-
-
Resolution = 12 bits
-
-
-
-
IDDA_SE
(ADC)
ADC
consumption on
VDDA,
BOOST=11,
Single-ended
mode
Resolution = 16 bits, fADC=25 MHz
-
-
-
-
Resolution = 14 bits, fADC=30 MHz
-
-
-
-
Resolution = 12 bits, fADC=40 MHz
-
-
-
-
ADC
consumption on
VDDA,
BOOST=10,
Singl-ended
mode,
fADC = 25 MHz
Resolution = 16 bits
-
-
-
-
Resolution = 14 bits
-
-
-
-
Resolution = 12 bits
-
-
-
292.5
-
ADC
consumption on
VDDA,
BOOST=01,
Single-ended
mode,
fADC = 12.5 MHz
Resolution = 16 bits
-
-
-
-
Resolution = 14 bits
-
-
-
-
Resolution = 12 bits
-
-
-
157.5
-
ADC
consumption on
VDDA
BOOST=00,
Single-ended
mode
fADC=6.25 MHz
Resolution = 16 bits
-
-
-
-
Resolution = 14 bits
-
-
-
-
Resolution = 12 bits
-
-
-
112.5
-
IDD
(ADC)
ADC
consumption on
VDD
 fADC=50 MHz
-
-
-
-
fADC=25 MHz
-
-
-
-
fADC=12.5 MHz
-
-
-
-
fADC=6.25 MHz
-
-
-
-
fADC=3.125 MHz
-
-
-
-
1.
Guaranteed by design.
2.
The voltage booster on ADC switches must be used for VDDA < 2.4 V (embedded I/O switches).
3.
These values are valid for TFBGA100, UFBGA169 and UFBGA176+25 packages and one ADC. The values for other packages and multiple
ADCs may be different.
4.
For slow channels, the performance should be limited to 1 Msps what ever the value of fADC.
Table 80. 16-bit ADC characteristics(1)(2) (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit

---

5.
Depending on the package, VREF+ can be internally connected to VDDA and VREF- to VSSA.
6.
The tolerance is 10 LSBs for 16-bit resolution, 4 LSBs for 14-bit resolution, and 2 LSBs for 12-bit, 10-bit and 8-bit resolutions.
Table 81. Minimum sampling time vs RAIN (16-bit ADC)(1)(2)
Resolution
RAIN (Ω)
Minimum sampling time (s)
Direct
channels(3)
Fast channels(4)
Slow channels(5)
16 bits
7.37E-08
1.14E-07
1.72E-07
14 bits
6.29E-08
9.74E-08
1.55E-07
6.84E-08
1.02E-07
1.58E-07
7.80E-08
1.12E-07
1.62E-07
9.86E-08
1.32E-07
1.80E-07
1.32E-07
1.61E-07
2.01E-07
12 bits
5.32E-08
8.00E-08
1.29E-07
5.74E-08
8.50E-08
1.32E-07
6.58E-08
9.31E-08
1.40E-07
8.37E-08
1.10E-07
1.51E-07
1.11E-07
1.34E-07
1.73E-07
1.56E-07
1.78E-07
2.14E-07
2.16E-07
2.39E-07
2.68E-07
3.01E-07
3.29E-07
3.54E-07
10 bits
4.34E-08
6.51E-08
1.08E-07
4.68E-08
6.89E-08
1.11E-07
5.35E-08
7.55E-08
1.16E-07
6.68E-08
8.77E-08
1.26E-07
8.80E-08
1.08E-07
1.40E-07
1.24E-07
1.43E-07
1.71E-07
1.69E-07
1.89E-07
2.13E-07
2.38E-07
2.60E-07
2.80E-07
3.45E-07
3.66E-07
3.84E-07
5.15E-07
5.35E-07
5.48E-07
7.42E-07
7.75E-07
7.78E-07
1.10E-06
1.14E-06
1.14E-06

---

8 bits
3.32E-08
5.10E-08
8.61E-08
3.59E-08
5.35E-08
8.83E-08
4.10E-08
5.83E-08
9.22E-08
5.06E-08
6.76E-08
9.95E-08
6.61E-08
8.22E-08
1.11E-07
9.17E-08
1.08E-07
1.32E-07
1.24E-07
1.40E-07
1.63E-07
1.74E-07
1.91E-07
2.12E-07
2.53E-07
2.70E-07
2.85E-07
3.73E-07
3.93E-07
4.05E-07
5.39E-07
5.67E-07
5.75E-07
8.02E-07
8.36E-07
8.38E-07
1.13E-06
1.18E-06
1.18E-06
1.62E-06
1.69E-06
1.68E-06
10000
2.36E-06
2.47E-06
2.45E-06
15000
3.50E-06
3.69E-06
3.65E-06
1.
Guaranteed by design.
2.
Data valid at up to 130 °C, with a 47 pF PCB capacitor, and VDDA=1.6 V.
3.
Direct channels are connected to analog I/Os (PA0_C, PA1_C, PC2_C and PC3_C) to optimize ADC performance.
4.
Fast channels correspond to PA6, PB1, PC4, PF11, PF13 for ADCx_INPx, and to PA7, PB0, PC5, PF12, PF14 for
ADCx_INNx.
5.
Slow channels correspond to all ADC inputs except for the Direct and Fast channels.
Table 81. Minimum sampling time vs RAIN (16-bit ADC)(1)(2) (continued)
Resolution
RAIN (Ω)
Minimum sampling time (s)
Direct
channels(3)
Fast channels(4)
Slow channels(5)

---

Note:
ADC accuracy vs. negative injection current: injecting a negative current on any analog
input pins should be avoided as this significantly reduces the accuracy of the conversion
being performed on another analog input. It is recommended to add a Schottky diode (pin to
ground) to analog pins which may potentially inject negative currents.
Any positive injection current within the limits specified for IINJ(PIN) and ΣIINJ(PIN) does not
affect the ADC accuracy.
Table 82. 16-bit ADC accuracy(1)(2)
Symbol
Parameter
Conditions(3)
Min
Typ
Max
Unit
ET
Total undadjusted error
Direct
channel
Single ended
-
+10/–20
-
LSB
Differential
-
±15
-
Fast channel
Single ended
-
+10/–20
-
Differential
-
±15
-
Slow
channel
Single ended
-
±10
-
Differential
±10
-
EO
Offset error
-
-
±10
-
EG
Gain error
-
-
±15
-
ED
Differential linearity error
Single ended
-
+3/–1
-
Differential
-
+4.5/–1
-
EL
Integral linearity error
Direct
channel
Single ended
-
±11
-
Differential
-
±7
-
Fast channel
Single ended
-
±13
-
Differential
-
±7
-
Slow
channel
Single ended
-
±10
-
Differential
-
±6
-
ENOB
Effective number of bits
Single ended
-
12.2
-
Bits
Differential
-
13.2
-
SINAD
Signal-to-noise and
distortion ratio
Single ended
-
75.2
-
dB
Differential
-
81.2
-
SNR
Signal-to-noise ratio
Single ended
-
77.0
-
Differential
-
81.0
-
THD
Total harmonic distortion
Single ended
-
-
Differential
-
-
1.
Guaranteed by characterization results for BGA packages. The values for LQFP packages might differ.
2.
ADC DC accuracy values are measured after internal calibration.
3.
ADC clock frequency = 25 MHz, ADC resolution = 16 bits, VDDA=VREF+=3.3 V, BOOST=11 and 16-bit mode.

---

Figure 36. ADC accuracy characteristics
1.
Example of an actual transfer curve.
2.
Ideal transfer curve.
3.
End point correlation line.
4.
ET = Total Unadjusted Error: maximum deviation between the actual and the ideal transfer curves.
EO = Offset Error: deviation between the first actual transition and the first ideal one.
EG = Gain Error: deviation between the last ideal transition and the last actual one.
ED = Differential Linearity Error: maximum deviation between actual steps and the ideal one.
EL = Integral Linearity Error: maximum deviation between any actual transition and the end point
correlation line.
Figure 37. Typical connection diagram when using the ADC with FT/TT pins
featuring analog switch function
1.
Refer to Table 80: 16-bit ADC characteristics for the values of RAIN and CADC.
2.
Cparasitic represents the capacitance of the PCB (dependent on soldering and PCB layout quality) plus the
pad capacitance (refer to Table 51: I/O static characteristics). A high Cparasitic value downgrades
conversion accuracy. To remedy this, fADC should be reduced.
3.
Refer to Table 51: I/O static characteristics for the value of Ilkg.
4.
Refer to Figure 10: Power supply scheme.
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

General PCB design guidelines
Power supply decoupling should be performed as shown in Figure 38 or Figure 39,
depending on whether VREF+ is connected to VDDA or not. The 100 nF capacitors should be
ceramic (good quality). They should be placed them as close as possible to the chip.
Figure 38. Power supply and reference decoupling (VREF+ not connected to VDDA)
1.
When VREF+ and VREF- inputs are not available, they are internally connected to VDDA and VSSA,
respectively.
Figure 39. Power supply and reference decoupling (VREF+ connected to VDDA)
1.
When VREF+ and VREF- inputs are not available, they are internally connected to VDDA and VSSA,
respectively.
MSv50648V2
1 μF // 100 nF
1 μF // 100 nF
STM32
VREF+
(1)
VSSA/VREF-
(1)
VDDA
MSv50649V1
1 μF // 100 nF
STM32
VREF+/VDDA
(1)
VREF-/VSSA
(1)

---

6.3.22
12-bit ADC characteristics
Unless otherwise specified, the parameters given in Table 83, Table 84 and Table 85 are
derived from tests performed under the ambient temperature and VDDA supply voltage
conditions summarized in Table 12: General operating conditions. In Table 83, Table 84 and
Table 85, fADC refers to fadc_ker_ck.
Table 83. 12-bit ADC characteristics(1)(2)
Sym-
bol
Parameter
Conditions
Min
Typ
Max
Unit
VDDA
Analog
power
supply for
ADC ON
-
1.62
-
3.6
V
VREF+
(3)
Positive
reference
voltage
VDDA  ≥ VREF+
1.62
-
VDDA
VREF-
Negative
reference
voltage
-
VSSA
-
-
fADC
ADC clock
frequency
1,62 V ≤ VDDA ≤ 3.6 V
1.5
-
MHz
fS
(4)
Sampling
rate for
Direct
channels
Resolution
 = 12 bits
Continuous
and
Discontinuous
mode(5)
2.4 V ≤ VDDA ≤ 3.6 V
–40 °C ≤ TJ ≤ 130 °C
fADC = 75
MHz
SMP
= 2.5
-
-
MSPS
1.6V ≤ VDDA≤ 3.6 V
fADC = 60
MHz
-
-
Single mode
2.4 V ≤ VDDA ≤ 3.6 V
fADC = 50
MHz(6)
-
-
3.33
1.6 V ≤ VDDA ≤ 3.6 V
fADC = 38
 MHz(6)
-
-
2.53
Resolution
= 10 bits
Continuous
and
Discontinuous
mode(5)
1.6V ≤ VDDA ≤ 3.6V
–40 °C ≤ TJ ≤ 130 °C
fADC = 75
MHz
SMP
= 2.5
-
-
5.77
Single mode
2.4 V ≤ VDDA ≤ 3.6 V
fADC = 58
MHz(6)
-
-
4.46
1.6V ≤ VDDA ≤ 3.6V
fADC = 42
MHz(6)
-
-
3.23
Resolution
= 8 bits
Continuous
and
Discontinuous
mode(5)
1.6V ≤ VDDA ≤ 3.6V
–40 °C ≤ TJ ≤ 130 °C
fADC = 75
MHz
SMP
= 2.5
-
-
6.82
Single mode
2.4 V ≤ VDDA ≤ 3.6 V
fADC = 67
MHz(6)
-
-
6.09
1.6V ≤ VDDA ≤ 3.6V
fADC = 48
MHz(6)
-
-
4.36
Resolution
= 6 bits
Continuous
and
Discontinuous
mode(5)
1.6V ≤ VDDA ≤ 3.6V
–40 °C ≤ TJ ≤ 130 °C
fADC = 75
MHz
SMP
= 2.5
-
-
8.33
Single mode
2.4 V ≤ VDDA ≤ 3.6 V
fADC = 75
MHz(6)
-
-
8.33
1.6V ≤ VDDA ≤ 3.6V
fADC = 55
MHz(6)
-
-
6.11

---

fS
(4)
(conti-
nued)
Sampling
rate for fast
channels
(VIN[0:5])
Resolution
= 12 bits
Continuous
and
Discontinuous
mode(5)
2.4 V ≤ VDDA ≤ 3.6 V
–40 °C ≤ TJ ≤ 130 °C
fADC = 65
MHz
SMP
= 2.5
-
-
4.33
MSPS
1.6V ≤ VDDA ≤ 3.6V
fADC = 58
MHz
-
-
3.87
Single mode
2.4 V ≤ VDDA ≤ 3.6 V
fADC = 32
MHz(6)
-
-
2.13
1.6V ≤ VDDA ≤ 3.6V
fADC =
26 MHz(6)
-
-
1.73
Resolution
= 10 bits
Continuous
and
Discontinuous
mode(5)
1.6V ≤ VDDA ≤ 3.6V
–40 °C ≤ TJ ≤ 130 °C
fADC = 75
MHz
SMP
= 2.5
-
-
5.77
Single mode
2.4 V ≤ VDDA ≤ 3.6 V
fADC = 36
MHz(6)
-
-
2.77
1.6V ≤ VDDA ≤ 3.6V
fADC = 30
MHz(6)
-
-
2.31
Resolution
= 8 bits
Continuous
and
Discontinuous
mode(5)
1.6V ≤ VDDA ≤ 3.6V
–40 °C ≤ TJ ≤ 130 °C
fADC = 75
MHz
SMP
= 2.5
-
-
6.82
Single mode
2.4 V ≤ VDDA ≤ 3.6 V
fADC =44
MHz(6)
-
-
4.00
1.6V ≤ VDDA ≤ 3.6V
fADC = 35
MHz(6)
-
-
3.18
Resolution
= 6 bits
Continuous
and
Discontinuous
mode(5)
1.6V ≤ VDDA ≤ 3.6V
–40 °C ≤ TJ ≤ 130 °C
fADC = 75
 MHz
SMP
= 2.5
-
-
8.33
Single mode
2.4 V ≤ VDDA ≤ 3.6 V
fADC = 56
MHz(6)
-
-
6.22
1.6V ≤ VDDA ≤ 3.6V
fADC = 42
MHz(6)
-
-
4.66
Sampling
rate for slow
channels
Resolution
= 12 bits
-
-
–40 °C ≤ TJ ≤ 130 °C
fADC = 15
MHz(6)
SMP
= 2.5
-
-
1.00
Resolution
= 10 bits
-
-
1.28
Resolution
= 8 bits
-
-
1.63
Resolution
= 6 bits
-
-
2.08
tTRIG
External
trigger
period
Resolution = 12 bits
-
-
1/fADC
VAIN
Conversion
voltage
range
-
-
VREF+
V
VCMIV
Common
mode input
voltage
-
VREF
/2−
10%
VREF
/2
VREF/2
+ 10%
RAIN
(7)
External
input
impedance
Resolution = 12 bits, TJ = 125 °C
-
-
Ω
Resolution = 10 bits, TJ = 125 °C
-
-
Resolution = 8 bits, TJ = 125 °C
-
-
12000
Resolution = 6 bits, TJ = 125 °C
-
-
80000
Table 83. 12-bit ADC characteristics(1)(2) (continued)
Sym-
bol
Parameter
Conditions
Min
Typ
Max
Unit

---

CADC
Internal
sample and
hold
capacitor
-
-
-
pF
tADCV
REG_
STUP
ADC LDO
startup time
-
-
µs
tSTAB
ADC power-
up time
LDO already started
-
-
con-
version
cycle
tOFF_
CAL
Offset
calibration
time
-
-
-
1/fADC
tLATR
Trigger
conversion
latency for
regular and
injected
channels
without
aborting the
conversion
CKMODE = 00
1.5
2.5
CKMODE = 01
-
-
2.5
CKMODE = 10
-
-
2.5
CKMODE = 11
-
-
2.25
tLATR
INJ
Trigger
conversion
latency for
regular and
injected
channels
when a
regular
conversion
is aborted
CKMODE = 00
2.5
3.5
CKMODE = 01
-
-
3.5
CKMODE = 10
-
-
3.5
CKMODE = 11
-
-
3.25
tS
Sampling
time
-
2.5
-
640.5
tCONV
Total
conversion
time
(including
sampling
time)
N-bits resolution
tS +
0.5 +
N
-
-
IDDA_
D(ADC)
ADC
consumption
on VDDA and
VREF,
Differential
mode
fS= 5 MSPS
-
-
µA
fS = 1 MSPS
-
-
fS =  0.1 MSPS
-
-
IDDA_
SE
(ADC)
ADC
consumption
on VDDA and
VREF,
Single-
ended mode
fS= 5 MSPS
-
-
fS = 1 MSPS
-
-
fS =  0.1 MSPS
-
-
IDD
(ADC)
ADC
consumption
on VDD per
fADC
-
-
2.4
-
µA/
MHz
1.
Guaranteed by design.
2.
The voltage booster on ADC switches must be used for VDDA < 2.4 V (embedded I/O switches).
3.
Depending on the package, VREF+ can be internally connected to VDDA and VREF- to VSSA.
4.
Guaranteed by characterization for BGA and CSP packages. The values for LQFP packages may be different.
5.
The conversion of the first element in the group is excluded.
Table 83. 12-bit ADC characteristics(1)(2) (continued)
Sym-
bol
Parameter
Conditions
Min
Typ
Max
Unit

---

6.
fADC value corresponds to the maximum frequency that can be reached considering a 2.5 sampling period. For other SMPy
sampling periods, the maximum frequency is fADC value * SMPy / 2.5 with a limitation to 75 MHz.
7.
The tolerance is 2 LSBs for 12-bit, 10-bit and 8-bit resolutions. It is otherwise specified.
Table 84. Minimum sampling time vs RAIN (12-bit ADC)(1)(2)
Resolution
RAIN (Ω)
Minimum sampling time (s)
Direct channels(3)
Fast channels(4)
Slow channels(5)
12 bits
5.55E-08
7.04E-08
1.03E-07
5.76E-08
7.22E-08
1.05E-07
6.17E-08
7.65E-08
1.07E-07
7.02E-08
8.45E-08
1.13E-07
8.59E-08
1.00E-07
1.22E-07
1.11E-07
1.26E-07
1.41E-07
1.46E-07
1.61E-07
1.69E-07
1.98E-07
2.17E-07
2.25E-07
10 bits
4.90E-08
6.06E-08
8.77E-08
5.07E-08
6.27E-08
8.95E-08
5.41E-08
6.67E-08
9.22E-08
6.18E-08
7.50E-08
9.59E-08
7.51E-08
8.70E-08
1.04E-07
9.46E-08
1.07E-07
1.17E-07
1.22E-07
1.34E-07
1.42E-07
1.63E-07
1.77E-07
1.86E-07
2.27E-07
2.42E-07
2.43E-07
3.27E-07
3.40E-07
3.35E-07
4.53E-07
4.86E-07
4.73E-07
6.56E-07
6.93E-07
6.72E-07

---

8 bits
4.35E-08
5.31E-08
7.36E-08
4.47E-08
5.48E-08
7.47E-08
4.72E-08
5.79E-08
7.63E-08
5.33E-08
6.35E-08
7.88E-08
6.26E-08
7.26E-08
8.47E-08
7.84E-08
8.80E-08
9.48E-08
9.80E-08
1.07E-07
1.14E-07
1.28E-07
1.39E-07
1.43E-07
1.76E-07
1.88E-07
1.90E-07
2.49E-07
2.66E-07
2.64E-07
3.50E-07
3.63E-07
3.63E-07
5.09E-07
5.27E-07
5.24E-07
7.00E-07
7.28E-07
7.09E-07
9.84E-07
1.03E-06
1.00E-06
10000
1.43E-06
1.48E-06
1.44E-06
15000
2.10E-06
2.18E-06
2.11E-06
6 bits
3.79E-08
4.58E-08
5.74E-08
3.88E-08
4.69E-08
5.81E-08
4.09E-08
4.89E-08
5.93E-08
4.48E-08
5.25E-08
6.14E-08
5.07E-08
5.81E-08
6.58E-08
6.04E-08
6.79E-08
7.46E-08
7.37E-08
8.10E-08
8.60E-08
9.31E-08
1.01E-07
1.04E-07
1.23E-07
1.32E-07
1.34E-07
1.71E-07
1.82E-07
1.82E-07
2.39E-07
2.50E-07
2.49E-07
3.43E-07
3.57E-07
3.49E-07
4.72E-07
4.92E-07
4.81E-07
6.65E-07
6.89E-07
6.68E-07
10000
9.54E-07
9.88E-07
9.54E-07
15000
1.40E-06
1.45E-06
1.39E-06
1.
Guaranteed by design.
2.
Data valid up to 130 °C, with a 22 pF PCB capacitor and VDDA = 1.62 V.
Table 84. Minimum sampling time vs RAIN (12-bit ADC)(1)(2) (continued)
Resolution
RAIN (Ω)
Minimum sampling time (s)
Direct channels(3)
Fast channels(4)
Slow channels(5)

---

3.
Direct channels are connected to analog I/Os (PA0_C, PA1_C, PC2_C and PC3_C) to optimize ADC performance.
4.
Fast channels correspond to ADCx_INx[0:5].
5.
Slow channels correspond to all ADC inputs except for the Direct and Fast channels.
Table 85. 12-bit ADC accuracy(1)(2)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
ET
Total
unadjusted
error
Direct channel
Single
ended
-
3.5
±LSB
Differential
-
2.5
Fast channel
Single
ended
-
3.5
Differential
-
2.5
Slow channel
Single
ended
-
3.5
Differential
-
2.5
EO
Offset error
-
-
+/-2
+/-5
EG
Gain error
-
-
+/-2
+/-5
ED
Differential
linearity
error
Single ended
-
+/-
0.75
+1.5/-
Differential
-
+/-0.5 +1.25
/-1
EL
Integral
linearity
error
Direct channel
Single
ended
-
+/-1
+/-2.5
Differential
-
+/-1
+/-2
Fast channel
Single
ended
-
+/-1
+/-2.5
Differential
-
+/-1
+/-2
Slow channel
Single
ended
-
+/-1
+/-2.5
Differential
-
+/-1
+/-2
ENOB
Effective
number of
bits
Single ended
-
11.2
-
bits
Differential
-
11.5
-
SINAD
Signal-to-
noise and
distortion
ratio
Single ended
-
68.9
-
dB
Differential
-
71.1
-
SNR
Signal-to-
noise ratio
Single ended
-
69.1
-
Differential
-
71.4
-
THD
Total
harmonic
distortion
Single ended
-
-79.6
-
Differential
-
-81.8
-

---

6.3.23
DAC characteristics
1.
Guaranteed by characterization for BGA packages. The maximum values are preliminary data. The values for LQFP
packages may be different.
2.
ADC DC accuracy values are measured after internal calibration in Continuous and Discontinuous mode.
Table 86. DAC characteristics(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
VDDA
Analog supply voltage
-
1.8
3.3
3.6
V
VREF+
Positive reference voltage
-
1.80
-
VDDA
VREF-
Negative reference
voltage
-
-
VSSA
-
RL
Resistive Load
DAC output buffer
ON
connected
to VSSA
-
-
kΩ
connected
to VDDA
-
-
RO
Output Impedance
DAC output buffer OFF
10.3
RBON
Output impedance
sample and hold mode,
output buffer ON
DAC output buffer
ON
VDD = 2.7 V
-
-
1.6
kΩ
VDD = 2.0 V
-
-
2.6
RBOFF
Output impedance
sample and hold mode,
output buffer OFF
DAC output buffer
OFF
VDD = 2.7 V
-
-
17.8
kΩ
VDD = 2.0 V
-
-
18.7
CL
Capacitive Load
DAC output buffer OFF
-
-
pF
CSH
Sample and Hold mode
-
0.1
µF
VDAC_OUT
Voltage on DAC_OUT
output
DAC output buffer ON
0.2
-
VREF+
− 0.2
V
DAC output buffer OFF
-
VREF+
tSETTLING
Settling time (full scale:
for a 12-bit code transition
between the lowest and
the highest input codes
when DAC_OUT reaches
the final value of ±0.5LSB,
±1LSB, ±2LSB, ±4LSB,
±8LSB)
Normal mode, DAC
output buffer ON,
CL ≤ 50 pF,
RL ≥ 5 ㏀
±0.5 LSB
-
2.05
µs
±1 LSB
-
1.97
2.87
±2 LSB
-
1.67
2.84
±4 LSB
-
1.66
2.78
±8 LSB
-
1.65
2.7
Normal mode, DAC output buffer
OFF, ±1LSB CL=10 pF
-
1.7
tWAKEUP
(2)
Wakeup time from off
state (setting the ENx bit
in the DAC Control
register) until the final
value of ±1LSB is reached
Normal mode, DAC output buffer
ON, CL ≤ 50 pF, RL = 5 ㏀
-
7.5
µs
Normal mode, DAC output buffer
OFF, CL ≤ 10 pF
PSRR
DC VDDA supply rejection
ratio
Normal mode, DAC output buffer
ON, CL ≤ 50 pF, RL = 5 ㏀
-
−80
−28
dB

---

tSAMP
Sampling time in Sample
and Hold mode
CL=100 nF
(code transition between
the lowest input code and
the highest input code
when DAC_OUT reaches
the ±1LSB final value)
MODE<2:0>_V12=100/101
(BUFFER ON)
-
0.7
2.6
ms
MODE<2:0>_V12=110
(BUFFER OFF)
-
11.5
18.7
MODE<2:0>_V12=111
(INTERNAL BUFFER OFF)
-
0.3
0.6
µs
Ileak
Output leakage current
-
(3)
nA
CIint
Internal sample and hold
capacitor
-
1.8
2.2
2.6
pF
tTRIM
Middle code offset trim
time
Minimum time to verify the each
code
-
-
µs
Voffset
Middle code offset for 1
trim code step
VREF+ = 3.6 V
-
-
µV
VREF+ = 1.8 V
-
-
IDDA(DAC)
DAC quiescent
consumption from VDDA
DAC output buffer
ON
No load,
middle code
(0x800)
-
-
µA
No load,
worst code
(0xF1C)
-
-
DAC output buffer
OFF
No load,
middle/
worst code
(0x800)
-
-
Sample and Hold mode,
CSH=100 nF
-
360*TON/
(TON+TOFF)
(4)
-
IDDV(DAC)
DAC consumption from
VREF+
DAC output buffer
ON
No load,
middle code
(0x800)
-
-
No load,
worst code
(0xF1C)
-
-
DAC output buffer
OFF
No load,
middle/
worst code
(0x800)
-
-
Sample and Hold mode, Buffer
ON, CSH=100 nF (worst code)
-
170*TON/
(TON+TOFF)
(4)
-
Sample and Hold mode, Buffer
OFF, CSH=100 nF (worst code)
-
160*TON/
(TON+TOFF)
(4)
-
1.
Guaranteed by design unless otherwise specified.
Table 86. DAC characteristics(1) (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit

---

2.
In buffered mode, the output can overshoot above the final value for low input code (starting from the minimum value).
3.
Refer to Table 51: I/O static characteristics.
4.
TON is the refresh phase duration, while TOFF is the hold phase duration. Refer to the product reference manual for more
details.
Table 87. DAC accuracy(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
DNL
Differential non
linearity(2)
DAC output buffer ON
−2
-
LSB
DAC output buffer OFF
−2
-
-
Monotonicity
10 bits
-
-
-
-
INL
Integral non linearity(3)
DAC output buffer ON, CL ≤50 pF,
RL ≥5 ㏀
−4
-
LSB
DAC output buffer OFF,
CL ≤ 50 pF, no RL
−4
-
Offset
Offset error at code
0x800 (3)
DAC output
buffer ON,
CL ≤50 pF,
RL ≥5 ㏀
VREF+ = 3.6 V
-
-
±12
LSB
VREF+ = 1.8 V
-
-
±25
DAC output buffer OFF,
CL ≤ 50 pF, no RL
-
-
±8
Offset1
Offset error at code
0x001(4)
DAC output buffer OFF,
CL ≤ 50 pF, no RL
-
-
±5
LSB
OffsetCal
Offset error at code
0x800 after factory
calibration
DAC output
buffer ON,
CL ≤50 pF,
RL ≥5 ㏀
VREF+ = 3.6 V
-
-
±5
LSB
VREF+ = 1.8 V
-
-
±7
Gain
Gain error(5)
 DAC output buffer ON,CL ≤50 pF,
RL ≥5 ㏀
-
-
±1
%
DAC output buffer OFF,
CL ≤ 50 pF, no RL
-
-
±1
TUE
Total unadjusted error
DAC output buffer ON, CL ≤50 pF,
RL ≥5 ㏀
-
-
±30
LSB
DAC output buffer OFF, CL ≤
50 pF, no RL
±12
TUECal
Total unadjusted error
after calibration
DAC output buffer ON, CL ≤50 pF,
RL ≥5 ㏀
-
-
±23
SNR
Signal-to-noise ratio(6)
DAC output buffer ON,CL ≤50 pF,
RL ≥5 ㏀, 1 kHz, BW = 500 KHz
-
67.8
-
dB
DAC output buffer OFF,
CL ≤ 50 pF, no RL,1 kHz, BW  =
500 KHz
-
67.8
-

---

Figure 40. 12-bit buffered /non-buffered DAC
1.
The DAC integrates an output buffer that can be used to reduce the output impedance and to drive external loads directly
without the use of an external operational amplifier. The buffer can be bypassed by configuring the BOFFx bit in the
DAC_CR register.
THD
Total harmonic
distortion(6)
DAC output buffer ON, CL ≤50 pF,
RL ≥5 ㏀, 1 kHz
-
−78.6
-
dB
DAC output buffer OFF,
CL ≤ 50 pF, no RL, 1 kHz
-
−78.6
-
SINAD
Signal-to-noise and
distortion ratio(6)
DAC output buffer ON, CL ≤50 pF,
RL ≥5 ㏀, 1 kHz
-
67.5
-
dB
DAC output buffer OFF,
CL ≤ 50 pF, no RL, 1 kHz
-
67.5
-
ENOB
Effective number of
bits
DAC output buffer ON,
CL ≤50 pF, RL ≥5 ㏀, 1 kHz
-
10.9
-
bits
DAC output buffer OFF,
CL ≤ 50 pF, no RL, 1 kHz
-
10.9
-
1.
Guaranteed by characterization results.
2.
Difference between two consecutive codes minus 1 LSB.
3.
Difference between the value measured at Code i and the value measured at Code i on a line drawn between Code 0 and
last Code 4095.
4.
Difference between the value measured at Code (0x001) and the ideal value.
5.
Difference between the ideal slope of the transfer function and the measured slope computed from code 0x000 and 0xFFF
when the buffer is OFF, and from code giving 0.2 V and (VREF+ - 0.2 V) when the buffer is ON.
6.
Signal is −0.5dBFS with Fsampling=1 MHz.
Table 87. DAC accuracy(1) (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
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

6.3.24
Voltage reference buffer characteristics
Table 88. VREFBUF characteristics(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
VDDA
Analog supply voltage
Normal mode,
VDDA = 3.3 V
VSCALE = 000
2.8
3.3
3.6
V
VSCALE = 001
2.4
-
3.6
VSCALE = 010
2.1
-
3.6
VSCALE = 011
1.8
-
3.6
Degraded mode(2)
VSCALE = 000
1.62
-
2.80
VSCALE = 001
1.62
-
2.40
VSCALE = 010
1.62
-
2.10
VSCALE = 011
1.62
-
1.80
VREFBUF
_OUT
Voltage Reference
Buffer Output, at 30 °C,
Iload= 100 µA
Normal mode at 30 °C,
Iload = 100 µA
VSCALE = 000
2.4980
2.5000
2.5035
VSCALE = 001
2.0460
2.0490
2.0520
VSCALE = 010
1.8010
1.8040
1.8060
VSCALE = 011
1.4995
1.5015
1.5040
Degraded mode(2)
VSCALE = 000
VDDA−
150 mV
-
VDDA
VSCALE = 001
VDDA−
150 mV
-
VDDA
VSCALE = 010
VDDA−
150 mV
-
VDDA
VSCALE = 011
VDDA−
150 mV
-
VDDA
TRIM
Trim step resolution
-
-
-
±0.05
±0.1
%
CL
Load capacitor
-
-
0.5
1.50
µF
esr
Equivalent Serial
Resistor of CL
-
-
-
-
Ω
ILOAD
Static load current
-
-
-
-
mA
Iline_reg
Line regulation
2.8 V ≤ VDDA ≤ 3.6 V
Iload = 500 µA
-
-
ppm/V
Iload = 4 mA
-
-
Iload_reg
Load regulation
500 µA ≤ ILOAD ≤ 4 mA
Normal mode
-
-
ppm/
mA
Tcoeff
Temperature coefficient
−40 °C < TJ < +130 °C
-
-
Tcoeff
VREFINT
+ 100
ppm/
°C
PSRR
Power supply rejection
DC
-
-
-
dB
100KHz
-
-
-

---

6.3.25
Analog temperature sensor characteristics
tSTART
Start-up time
CL=0.5 µF
-
-
-
µs
CL=1 µF
-
-
-
CL=1.5 µF
-
-
-
IINRUSH
Control of maximum
DC current drive on
VREFBUF_OUT during
startup phase(3)
-
-
-
mA
IDDA
(VREFBUF)
VREFBUF
consumption from
VDDA
ILOAD = 0 µA
-
-
µA
ILOAD = 500 µA
-
-
ILOAD = 4 mA
-
-
1.
Guaranteed by design, unless otherwise specified.
2.
 In degraded mode, the voltage reference buffer cannot accurately maintain the output voltage (VDDA−drop voltage).
3.
To properly control VREFBUF IINRUSH current during the startup phase and the change of scaling, VDDA voltage should be in
the range of 1.8 V-3.6 V, 2.1 V-3.6 V, 2.4 V-3.6 V and 2.8 V-3.6 V for VSCALE = 011, 010, 001 and 000, respectively.
Table 88. VREFBUF characteristics(1) (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Table 89. Temperature sensor characteristics
Symbol
Parameter
Min
Typ
Max
Unit
TL
(1)
1.
Guaranteed by design.
VSENSE linearity with temperature
-
-
±3
°C
Avg_Slope(2)
2.
Guaranteed by characterization results.
Average slope
-
-
mV/°C
V30
(3)
3. Measured at VDDA = 3.3 V ± 10 mV. The V30 ADC conversion result is stored in the TS_CAL1
byte.
Voltage at 30°C ± 5 °C
-
0.62
-
V
tstart_run
Startup time in Run mode (buffer startup)
-
-
25.2
µs
tS_temp
(1)
ADC sampling time when reading the temperature
-
-
Isens
(1)
Sensor consumption
-
0.18
0.31
µA
Isensbuf
(1)
Sensor buffer consumption
-
3.8
6.5
Table 90. Temperature sensor calibration values
Symbol
Parameter
Memory address
TS_CAL1
Temperature sensor raw data acquired value at
30 °C, VDDA=3.3 V
0x1FF1 E820 -0x1FF1 E821
TS_CAL2
Temperature sensor raw data acquired value at
130 °C, VDDA=3.3 V
0x1FF1 E840 - 0x1FF1 E841

---

6.3.26
Digital temperature sensor characteristics
6.3.27
Temperature and VBAT monitoring
Table 91. Digital temperature sensor characteristics(1)
1.
Guaranteed by design, unless otherwise specified.
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fDTS
(2)
2.
Guaranteed by characterization results.
Output Clock frequency
-
kHz
TLC
(2)
Temperature linearity coefficient
VOS2
Hz/°
C
TTOTAL_ERROR
(2)
Temperature offset
measurement, all VOS
TJ = −40°C to
30°C
−13
-
°C
TJ = 30°C to
Tjmax
−7
-
TVDD_CORE
Additional error due to supply
variation
VOS2
-
°C
VOS0, VOS1,
VOS3
−1
-
tTRIM
Calibration time
-
-
-
ms
tWAKE_UP
Wake-up time from off state until
DTS ready bit is set
-
-
116.00
μs
IDDCORE_DTS
DTS consumption on
VDD_CORE
-
8.5
70.0
μA
Table 92. VBAT monitoring characteristics
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
1.
Guaranteed by design.
Error on Q
–10
-
+10
%
tS_vbat
(1)
ADC sampling time when reading VBAT input
-
-
µs
VBAThigh
High supply monitoring
-
3.55
-
V
VBATlow
Low supply monitoring
-
1.36
-
Table 93. VBAT charging characteristics
Symbol
Parameter
Condition
Min
Typ
Max
Unit
RBC
Battery charging resistor
VBRS in PWR_CR3= 0
-
-
KΩ
VBRS in PWR_CR3= 1
1.5
-

---

6.3.28
Voltage booster for analog switch
6.3.29
Comparator characteristics
Table 94. Temperature monitoring characteristics
Symbol
Parameter
Min
Typ
Max
Unit
TEMPhigh
High temperature monitoring
-
-
°C
TEMPlow
Low temperature monitoring
-
–25
-
Table 95. Voltage booster for analog switch characteristics(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Condition
Min
Typ
Max
Unit
VDD
Supply voltage
-
1.62
2.6
3.6
V
tSU(BOOST) Booster startup time
-
-
-
µs
IDD(BOOST) Booster consumption
 1.62 V ≤ VDD ≤ 2.7 V
-
-
µA
2.7 V < VDD < 3.6 V
-
-
Table 96. COMP characteristics(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
VDDA
Analog supply voltage
-
1.62
3.3
3.6
V
VIN
Comparator input voltage
range
-
-
VDDA
VBG
Scaler input voltage
-
(2)
VSC
Scaler offset voltage
-
-
±5
±10
mV
IDDA(SCALER)
Scaler static consumption
from VDDA
BRG_EN=0 (bridge disable)
-
0.2
0.3
µA
BRG_EN=1 (bridge enable)
-
0.8
tSTART_SCALER
Scaler startup time
-
-
µs
tSTART
Comparator startup time to
reach propagation delay
specification
High-speed mode
-
µs
Medium mode
-
Ultra-low-power mode
-
tD
(3)
Propagation delay for
200 mV step with 100 mV
overdrive
High-speed mode
-
ns
Medium mode
-
0.5
0.9
µs
Ultra-low-power mode
-
2.5
Propagation delay for step
> 200 mV with 100 mV
overdrive only on positive
inputs
High-speed mode
-
ns
Medium mode
-
0.5
1.2
µs
Ultra-low-power mode
-
2.5
Voffset
Comparator offset error
Full common mode range
-
±5
±20
mV

---

6.3.30
Operational amplifier characteristics
Vhys
Comparator hysteresis
No hysteresis
-
-
mV
Low hysteresis
Medium hysteresis
High hysteresis
IDDA(COMP)
Comparator consumption
from VDDA
Ultra-low-
power mode
Static
-
nA
With 50 kHz
±100 mV overdrive
square signal
-
-
Medium mode
Static
-
µA
With 50 kHz
±100 mV overdrive
square signal
-
-
High-speed
mode
Static
-
With 50 kHz
±100 mV overdrive
square signal
-
-
1.
Guaranteed by design, unless otherwise specified.
2.
Refer to Table 17: Embedded reference voltage.
3.
Guaranteed by characterization results.
Table 96. COMP characteristics(1) (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
Table 97. Operational amplifier characteristics(1)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
VDDA
Analog supply voltage
Range
-
3.3
3.6
V
CMIR
Common Mode Input
Range
-
-
VDDA
VIOFFSET
Input offset voltage
25°C, no load on output
-
-
±1.5
mV
All voltages and
temperature, no load
-
-
±2.5
ΔVIOFFSET
Input offset voltage drift
-
-
±3.0
-
μV/°C
TRIMOFFSETP
TRIMLPOFFSETP
Offset trim step at low
common input voltage
(0.1*VDDA)
-
-
1.1
1.5
mV
TRIMOFFSETN
TRIMLPOFFSETN
Offset trim step at high
common input voltage
(0.9*VDDA)
-
-
1.1
1.5
ILOAD
Drive current
-
-
-
μA
ILOAD_PGA
Drive current in PGA mode
-
-
-

---

CLOAD
Capacitive load
-
-
-
pF
CMRR
Common mode rejection
ratio
-
-
-
dB
PSRR
Power supply rejection
ratio
CLOAD ≤ 50pf /
RLOAD ≥ 4 kΩ(2) at 1 kHz,
Vcom=VDDA/2
-
dB
GBW
Gain bandwidth for high
supply range
200 mV ≤ Output dynamic
range ≤ VDDA - 200 mV
7.3
12.3
MHz
SR
Slew rate (from 10% and
90% of output voltage)
Normal mode
-
-
V/µs
High-speed mode
-
-
AO
Open loop gain
200 mV ≤ Output dynamic
range ≤ VDDA - 200 mV
dB
φm
Phase margin
-
-
-
°
GM
Gain margin
-
-
-
dB
VOHSAT
High saturation voltage
Iload=max or RLOAD=min,
Input at VDDA
VDDA
−100 mV
-
-
mV
VOLSAT
Low saturation voltage
Iload=max or RLOAD=min,
Input at 0 V
-
-
tWAKEUP
Wake up time from OFF
state
Normal
mode
CLOAD ≤ 50pf,
RLOAD ≥ 4 kΩ,
follower
configuration
-
0.8
3.2
µs
High
speed
mode
CLOAD ≤ 50pf,
RLOAD ≥ 4 kΩ,
follower
configuration
-
0.9
2.8
PGA gain
Non inverting gain error
value
PGA gain = 2
−1
-
%
PGA gain = 4
−2
-
PGA gain = 8
−2.5
-
2.5
PGA gain = 16
−3
-
Inverting gain error value
PGA gain = 2
−1
-
PGA gain = 4
−1
-
PGA gain = 8
−2
-
PGA gain = 16
−3
-
External non-inverting gain
error value
PGA gain = 2
−1
-
PGA gain = 4
−3
-
PGA gain = 8
−3.5
-
3.5
PGA gain = 16
−4
-
Table 97. Operational amplifier characteristics(1) (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit

---

Rnetwork
R2/R1 internal resistance
values in non-inverting
PGA mode(3)
PGA Gain=2
-
-
kΩ/
kΩ
PGA Gain=4
-
-
PGA Gain=8
-
-
PGA Gain=16
-
-
R2/R1 internal resistance
values in inverting PGA
mode(3)
PGA Gain = -1
-
-
PGA Gain = -3
-
-
PGA Gain = -7
-
-
PGA Gain = -15
-
-
Delta R
Resistance variation (R1
or R2)
-
−15
-
%
PGA BW
PGA bandwidth for
different non inverting gain
Gain=2
-
GBW/2
-
MHz
Gain=4
-
GBW/4
-
Gain=8
-
GBW/8
-
Gain=16
-
GBW/16
-
PGA bandwidth for
different inverting gain
Gain = -1
-
5.00
-
MHz
Gain = -3
-
3.00
-
Gain = -7
-
1.50
-
Gain = -15
-
0.80
-
en
Voltage noise density
at
1 KHz
output loaded
with 4 kΩ
-
-
nV/√
Hz
at
10 KHz
-
-
IDDA(OPAMP)
OPAMP consumption from
VDDA
Normal
mode
no Load,
quiescent mode,
follower
-
µA
High-
speed
mode
-
1.
Guaranteed by design, unless otherwise specified.
2.
RLOAD is the resistive load connected to VSSA or to VDDA.
3.
R2 is the internal resistance between the OPAMP output and th OPAMP inverting input. R1 is the internal resistance
between the OPAMP inverting input and ground. PGA gain = 1 + R2/R1.
Table 97. Operational amplifier characteristics(1) (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit

---

6.3.31
Digital filter for Sigma-Delta Modulators (DFSDM) characteristics
Unless otherwise specified, the parameters given in Table 98 for DFSDM are derived from
tests performed under the ambient temperature, fPCLKx frequency and supply voltage
conditions summarized in Table 12: General operating conditions.
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load CL = 30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
•
VOS level set to VOS0
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output alternate
function characteristics (DìFSDM_CKINx, DFSDM_DATINx, DFSDM_CKOUT for DFSDM).
Table 98. DFSDM measured timing
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fDFSDMCLK
DFSDM
clock
1.62 < VDD < 3.6 V
-
-
(1)
MHz
fCKIN
(1/TCKIN)
Input clock
frequency
SPI mode
(SITP[1:0] = 0,1),
External clock mode
(SPICKSEL[1:0] = 0)
-
-
SPI mode
(SITP[1:0] = 0,1),
Internal clock mode
(SPICKSEL[1:0] # 0)
-
-
fCKOUT
Output clock
frequency
1.62 < VDD < 3.6 V
-
-
DuCyCKOUT
Output clock
frequency
duty cycle
1.62 < VDD
< 3.6 V
Even
division,
CKOUTDIV
= n, 1, 3, 5..
%
Odd
division,
CKOUTDIV
= n, 2, 4, 6..
(((n/2+1)/(n+1))
*100)−5
(((n/2+1)/(n+1))
*100)
(((n/2+1)/(n+1))
*100)+5

---

twh(CKIN)
twl(CKIN)
Input clock
high and low
time
SPI mode
(SITP[1:0] = 0,1),
External clock mode
(SPICKSEL[1:0] = 0)
TCKIN/2−0.5
TCKIN/2
-
ns
tsu
Data input
setup time
SPI mode
(SITP[1:0] = 0,1),
External clock mode
(SPICKSEL[1:0] = 0)
-
-
th
Data input
hold time
SPI mode
(SITP[1:0] = 0,1),
External clock mode
(SPICKSEL[1:0] = 0)
-
-
TManchester
Manchester
data period
(recovered
clock period)
Manchester mode
(SITP[1:0] = 2,3),
Internal clock mode
(SPICKSEL[1:0] # 0)
(CKOUTDIV+1)
* TDFSDMCLK
-
(2*CKOUTDIV)
* TDFSDMCLK
1.
The maximum DFSDM kernel clock frequency is specified in the RCC chapter of the reference manual (RM0468).
Table 98. DFSDM measured timing (continued)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit

---

Figure 41. Channel transceiver timing diagrams
MS30766V2
SITP = 0
DFSDM_CKOUT
DFSDM_DATINy
SITP = 1
tsu
th
tsu
th
tf
tr
twl
twh
SPI timing : SPICKSEL = 1, 2, 3
recovered clock
SITP = 2
DFSDM_DATINy
SITP = 3
Manchester timing
recovered data
SITP = 00
DFSDM_CKINy
DFSDM_DATINy
SITP = 01
tsu
th
tsu
th
tf
tr
twl
twh
SPI timing : SPICKSEL = 0
SPICKSEL=2
SPICKSEL=1
(SPICKSEL=0)
SPICKSEL=3

---

6.3.32
Camera interface (DCMI) timing specifications
Unless otherwise specified, the parameters given in Table 99 for DCMI are derived from
tests performed under the ambient temperature, fHCLK frequency and VDD supply voltage
summarized in Table 12: General operating conditions, with the following configuration:
•
DCMI_PIXCLK polarity: falling
•
DCMI_VSYNC and DCMI_HSYNC polarity: high
•
Data formats: 14 bits
•
Capacitive load CL=30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
•
VOS level set to VOS0
Figure 42. DCMI timing diagram
Table 99. DCMI characteristics(1)
1.
Guaranteed by characterization results.
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
Pixel Clock input
-
MHz
Dpixel
Pixel Clock input duty cycle
%
tsu(DATA)
Data input setup time
-
ns
th(DATA)
Data hold time
-
 tsu(HSYNC),
tsu(VSYNC)
DCMI_HSYNC/ DCMI_VSYNC input setup time
-
th(HSYNC),
th(VSYNC)
DCMI_HSYNC/ DCMI_VSYNC input hold time
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

6.3.33
Parallel synchronous slave interface (PSSI) characteristics
Unless otherwise specified, the parameters given in Table 100 and Table 101 for PSSI are
derived from tests performed under the ambient temperature, fHCLK frequency and VDD
supply voltage summarized in Table 12: General operating conditions.
Table 100. PSSI transmit characteristics(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
-
Frequency ratio
PSSI_PDCK/fHCLK
-
0.4
-
PSSI_PDCK
PSSI Clock input
-
MHz
-
35(2)
2.
This value is obtained by using PA9, PA10 or PH4 I/O.
Dpixel
PSSI Clock input duty cycle
%
tov(DATA)
Data output valid time
-
ns
-
-
-
14(2)
toh(DATA)
Data output hold time
4.5
-
tov((DE)
DE output valid time
-
toh(DE)
DE output hold time
-
tsu(RDY)
RDY input setup time
-
th(RDY)
RDY input hold time
-
Table 101. PSSI receive characteristics(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
-
Frequency ratio
PSSI_PDCK/fHCLK
-
0.4
-
PSSI_PDCK
PSSI Clock input
-
MHz
Dpixel
PSSI Clock input duty cycle
%
tsu(DATA)
Data input setup time
1.5
-
ns
th(DATA)
Data input hold time
0.5
-
tsu((DE)
DE input setup time
-
th(DE)
DE input hold time
-
tov(RDY)
RDY output valid time
-
toh(RDY)
RDY output hold time
5.5
-

---

6.3.34
LCD-TFT controller (LTDC) characteristics
Unless otherwise specified, the parameters given in Table 102 for LCD-TFT are derived
from tests performed under the ambient temperature, fHCLK frequency and VDD supply
voltage summarized in Table 12: General operating conditions, with the following
configuration:
•
LCD_CLK polarity: high
•
LCD_DE polarity: low
•
LCD_VSYNC and LCD_HSYNC polarity: high
•
Pixel formats: 24 bits
•
Output speed is set to OSPEEDRy[1:0] = 11
•
Capacitive load CL=30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
•
IO Compensation cell activated.
•
HSLV activated when VDD ≤ 2.7 V
•
VOS level set to VOS0
Table 102. LTDC characteristics(1)
Symbol
Parameter
Min
Max
Unit
fCLK
LTDC clock
output
frequency
2.7<VDD<3.6 V, 20 pF
-
MHz
2.7<VDD<3.6 V
1.62<VDD<3.6 V
90/76.5(2)
DCLK
LTDC clock output duty cycle
%
tw(CLKH),
tw(CLKL)
Clock High time, low time
tw(CLK)//2−0.5
tw(CLK)/2+0.5
ns
tv(DATA)
Data output valid time
2.7<VDD<3.6 V
-
2.0
1.62<VDD<3.6 V
2.5/6.5(2)
th(DATA)
Data output hold time
-
tv(HSYNC),
tv(VSYNC),
tv(DE)
HSYNC/VSYNC/DE output
valid time
2.7<VDD<3.6 V
-
1.5
1.62<VDD<3.6 V
-
2.0
th(HSYNC),
th(VSYNC),
th(DE)
HSYNC/VSYNC/DE output hold time
-
1.
Guaranteed by characterization results.
2.
This value is valid when PA[9], PA[10], PA[11], PA[12], PA[15], PB[11], PH[4], PJ[8], PJ[9], PJ[10], PJ[11], PK[0], PK[1] or
PK[2] is used.

---

Figure 43. LCD-TFT horizontal timing diagram
Figure 44. LCD-TFT vertical timing diagram
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

6.3.35
Timer characteristics
The parameters given in Table 103 are guaranteed by design.
Refer to Section 6.3.16: I/O port characteristics for details on the input/output alternate
function characteristics (output compare, input capture, external clock, PWM output).
6.3.36
Low-power timer characteristics
The parameters given in Table 104 are guaranteed by design.
Refer to Section 6.3.16: I/O port characteristics for details on the input/output alternate
function characteristics (output compare, input capture, external clock, PWM output).
Table 103. TIMx characteristics(1)(2)
1.
TIMx is used as a general term to refer to the TIM1 to TIM17 timers.
2.
Guaranteed by design.
Symbol
Parameter
Conditions(3)
3.
The maximum timer frequency on APB1 or APB2 is up to 275 MHz, by setting the TIMPRE bit in the
RCC_CFGR register, if APBx prescaler is 1 or 2 or 4, then TIMxCLK = rcc_hclk1, otherwise TIMxCLK = 4x
Frcc_pclkx1 or TIMxCLK = 4x Frcc_pclkx2.
Min
Max
Unit
tres(TIM)
Timer resolution time
AHB/APBx prescaler=1
or 2 or 4, fTIMxCLK =
275 MHz
-
tTIMxCLK
AHB/APBx
prescaler>4, fTIMxCLK =
137.5 MHz
-
tTIMxCLK
fEXT
Timer external clock
frequency on CH1 to CH4  fTIMxCLK = 240 MHz
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
Table 104. LPTIMx characteristics(1)(2)
1.
LPTIMx is used as a general term to refer to the LPTIM1 to LPTIM5 timers.
2.
Guaranteed by design.
Symbol
Parameter
Min
Max
Unit
tres(TIM)
Timer resolution time
-
tTIMxCLK
fLPTIMxCLK
Timer kernel clock
137.5
MHz
fEXT
Timer external clock frequency on Input1 and
Input2
fLPTIMxCLK/2
ResTIM
Timer resolution
-
bit
tMAX_COUNT
Maximum possible count
-
65536
tTIMxCLK

---

6.3.37
Communication interfaces
I2C interface characteristics
The I2C interface meets the timings requirements of the I2C-bus specification and user
manual revision 03 for:
•
 Standard-mode (Sm): with a bit rate up to 100 kbit/s
•
 Fast-mode (Fm): with a bit rate up to 400 kbit/s
•
 Fast-mode Plus (Fm+): with a bit rate up to 1 Mbit/s.
The I2C timings requirements are guaranteed by design when the I2C peripheral is properly
configured (refer to RM0399 reference manual) and when the i2c_ker_ck frequency is
greater than the minimum shown in the table below:
The SDA and SCL I/O requirements are met with the following restrictions:
•
The SDA and SCL I/O pins are not “true” open-drain. When configured as open-drain,
the PMOS connected between the I/O pin and VDD is disabled, but still present.
•
The 20 mA output drive requirement in Fast-mode Plus is not supported. This limits the
maximum load CLoad supported in Fm+, which is given by these formulas:
tr(SDA/SCL)=0.8473xRP * CLoad
RP(min)= (VDD-VOL(max))/IOL(max)
Where RP is the I2C lines pull-up. Refer to Section 6.3.16: I/O port characteristics for
the I2C I/Os characteristics.
All I2C SDA and SCL I/Os embed an analog filter. Refer to the table below for the analog fil-
ter characteristics:
Table 105. Minimum i2c_ker_ck frequency in all I2C modes
Symbol
Parameter
Condition
Min
Unit
f(I2CCLK)
I2CCLK
frequency
Standard-mode
-
MHz
Fast-mode
Analog Filtre ON
DNF=0
Analog Filtre OFF
DNF=1
Fast-mode Plus
Analog Filtre ON
DNF=0
Analog Filtre OFF
DNF=1
-
Table 106. I2C analog filter characteristics(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Max
Unit
tAF
Maximum pulse width of spikes that
are suppressed by analog filter
50(2)
2.
Spikes with widths below tAF(min) are filtered.
80(3)
3.
Spikes with widths above tAF(max) are not filtered.
ns

---

USART interface characteristics
Unless otherwise specified, the parameters given in Table 107 for USART are derived from
tests performed under the ambient temperature, fPCLKx frequency and VDD supply voltage
conditions summarized in Table 12: General operating conditions, with the following
configuration:
•
Output speed is set to OSPEEDRy[1:0] = 11
•
Capacitive load CL = 30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
•
IO Compensation cell activated.
•
VOS level set to VOS0
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output alternate
function characteristics (NSS, CK, TX, RX for USART).
Table 107. USART (SPI mode) characteristics(1)
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit
fCK
USART clock frequency
SPI master mode,
1.62 V < VDD < 3.6 V
-
-
17.0
MHz
SPI slave receiver
mode,
1.62 V < VDD < 3.6 V
45.0
SPI slave transmitter
mode,
1.62 V < VDD < 3.6 V
-
-
27.0
SPI slave transmitter
mode,
2.5 V < VDD < 3.6 V
37.0
tsu(NSS)
NSS setup time
SPI slave mode
tker+1
-
-
ns
th(NSS)
NSS hold time
SPI slave mode
-
-
tw(CKH),
tw(CKL)
CK high and low time
SPI master mode
1/fCK/2-2
1/fCK/2
1/fCK/2+2
tsu(RX)
Data input setup time
SPI master mode
-
-
SPI slave mode
1.0
-
-
th(RX)
Data input hold time
SPI master mode
-
-
SPI slave mode
2.0
-
-
tv(TX)
Data output valid time
SPI slave mode, ,
1.62 V < VDD < 3.6 V
-
12.0
SPI slave mode,
2.5 V < VDD < 3.6 V
-
12.0
13.5
SPI master mode
-
0.5
th(TX)
Data output hold time
SPI slave mode
-
-
SPI master mode
-
-
1.
Guaranteed by characterization results.

---

Figure 45. USART timing diagram in SPI master mode
1.
Measurement points are done at 0.5VDD and with external CL = 30 pF.
Figure 46. USART timing diagram in SPI slave mode
MSv65386V6
1/fCK
tw(CKH)
tw(CKL)
CPHA=0
CPOL=0
CPHA=0
CPOL=1
TX output
RX input
CK output
MSB OUT
LSB OUT
BIT1 OUT
MSB IN
LSB IN
BIT6 IN
CPHA=1
CPOL=0
CPHA=1
CPOL=1
CK output
tsu(RX)
tv(TX)
th(TX)
th(RX)
MSv65387V6
NSS input
CPHA=0
CPOL=0
CK input
CPHA=0
CPOL=1
TX output
RX input
th(RX)
tw(CKL)
tw(CKH)
1/fCK
th(NSS)
tsu(NSS)
tv(TX)
Next bits IN
Last bit OUT
First bit IN
First bit OUT
Next bits OUT
th(TX)
Last bit IN
tsu(RX)

---

SPI interface characteristics
Unless otherwise specified, the parameters given in Table 108 for SPI are derived from tests
performed under the ambient temperature, fPCLKx frequency and VDD supply voltage
conditions summarized in Table 12: General operating conditions, with the following
configuration:
•
Output speed is set to OSPEEDRy[1:0] = 11
•
Capacitive load CL = 30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
•
IO Compensation cell activated.
•
HSLV activated when VDD ≤ 2.7 V
•
VOS level set to VOS0
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output alternate
function characteristics (SS, SCK, MOSI, MISO for SPI).
Table 108. SPI characteristics(1)(2)
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit
fSCK
SPI clock frequency
Master mode,
2.7 V < VDD < 3.6 V, SPI1, 2, 3
-
-
MHz
Master mode,
1.62 V < VDD < 3.6 V, SPI1, 2,
80/66(3)
Master mode,
1.62 V < VDD < 3.6 V, SPI4, 5,
68.5
Slave receiver mode,
1.62 V < VDD < 3.6 V, SPI1, 2,
Slave receiver mode,
1.62 V < VDD < 3.6 V, SPI4, 5,
68.5
Slave mode transmitter/full
duplex, 2.7 V < VDD < 3.6 V
Slave mode transmitter/full
duplex, 1.62 V < VDD < 3.6 V
42.5/31(4)
tsu(NSS)
NSS setup time
Slave mode
-
-
-
th(NSS)
NSS hold time
Slave mode
-
-
tw(SCKH),
tw(SCKL)
SCK high and low time
Master mode
tSCK/2-1(5) tSCK/2(5)
tSCK/2+1(5)

---

Figure 47. SPI timing diagram - slave mode and CPHA = 0
tsu(MI)
Data input setup time
Master mode
2.5
-
-
ns
tsu(SI)
Slave mode
-
-
th(MI)
Data input hold time
Master mode
-
-
th(SI)
Slave mode
1.5
-
-
ta(SO)
Data output access time
Slave mode
tdis(SO)
Data output disable time
Slave mode
tv(SO)
Data output valid time
Slave mode,
2.7 V < VDD < 3.6 V
-
7.5
Slave mode,
1.62 V < VDD < 3.6 V
-
7.5
12/16(4)
tv(MO)
Master mode,
1.62 V < VDD < 3.6 V
-
1.5/5.5(6)
th(SO)
Data output hold time
Slave mode
-
-
th(MO)
Master mode
0.5
-
-
1.
Guaranteed by characterization results.
2.
The values given in the above table might be degraded when PC3_C/PC2_C I/Os are used (not available on all packages).
3.
This value is obtained by using PA9 or PA12 I/O.
4.
This value is obtained by using PC2 or PJ11 I/O.
5. tSCK = tker_ck * baud rate prescaler.
6.
This value is obtained by using PC3 or PJ10 I/O.
Table 108. SPI characteristics(1)(2) (continued)
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit
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

---

Figure 48. SPI timing diagram - slave mode and CPHA = 1
Figure 49. SPI timing diagram - master mode
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

I2S Interface characteristics
Unless otherwise specified, the parameters given in Table 109 for I2S  are derived from tests
performed under the ambient temperature, fPCLKx frequency and VDD supply voltage
conditions summarized in Table 12: General operating conditions, with the following
configuration:
•
Output speed is set to OSPEEDRy[1:0] = 11
•
Capacitive load CL = 30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
•
IO Compensation cell activated.
•
HSLV activated when VDD ≤ 2.7 V
•
VOS level set to VOS0
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output alternate
function characteristics (CK,SD,WS).
Table 109. I2S dynamic characteristics(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Conditions
Min
Max
Unit
fMCK
I2S main clock output
  -
-
MHz
Master transmitter
-
50/40(2)
2.
This value is obtained when PA9 or PA12 are used.
Master receiver
-
50/40(2)
Slave transmitter
-
41.5/31(3)
3.
This value is obtained when PC2 is used.
Slave receiver
-
tv(WS)
WS valid time
Master mode
-
2/6(4)
4.
This value is obtained when PA11 or PA15 are used.
ns
th(WS)
WS hold time
-
tsu(WS)
WS setup time
Slave mode
-
th(WS)
WS hold time
-
tsu(SD_MR)
Data input setup time
Master receiver
2.5
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
1.5
-
tv(SD_ST)
Data output valid time
Slave transmitter (after enable
edge)
-
12/16(3)
tv(SD_MT)
Master transmitter (after
enable edge)
-
2/6(5)
th(SD_ST)
Data output hold time
Slave transmitter (after enable
edge)
6.5
-
th(SD_MT)
Master transmitter (after
enable edge)
0.5
-

---

Figure 50. I2S slave timing diagram (Philips protocol)(1)
1.
LSB transmit/receive of the previously transmitted byte. No LSB transmit/receive is sent before the first
byte.
Figure 51. I2S master timing diagram (Philips protocol)(1)
1.
LSB transmit/receive of the previously transmitted byte. No LSB transmit/receive is sent before the first
byte.
5.
This value is obtained when PC3 is used.

---

SAI characteristics
Unless otherwise specified, the parameters given in Table 110 for SAI are derived from tests
performed under the ambient temperature, fPCLKx frequency and VDD supply voltage
conditions summarized in Table 12: General operating conditions, with the following
configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load  CL = 30 pF
•
IO Compensation cell activated.
•
Measurement points are done at CMOS levels: 0.5VDD
•
VOS level set to VOS0
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output
alternate function characteristics (SCK,SD,WS).
Table 110. SAI characteristics(1)
Symbol
Parameter
Conditions
Min
Max
Unit
fMCK
SAI Main clock output
  -
-
MHz
fCK
SAI clock frequency(2)
Master transmitter, 2.7 V ≤ VDD ≤ 3.6 V
-
Master transmitter, 1.62 V ≤ VDD ≤ 3.6 V
-
Master receiver, 1.62 V ≤ VDD ≤ 3.6 V
-
Slave transmitter, 2.7 V ≤ VDD ≤ 3.6 V
-
47.5
Slave transmitter, 1.62 V ≤ VDD ≤ 3.6 V
-
41.5
Slave receiver, 1.62 V ≤ VDD ≤ 3.6 V
-

---

Figure 52. SAI master timing waveforms
tv(FS)
FS valid time
Master mode, 2.7 V ≤ VDD ≤ 3.6 V
-
ns
Master mode, 1.62 V ≤ VDD ≤ 3.6 V
-
15.5
tsu(FS)
FS setup time
Slave mode
2.5
-
th(FS)
FS hold time
Master mode
-
Slave mode
0.5
-
tsu(SD_A_MR)
Data input setup time
Master receiver
-
tsu(SD_B_SR)
Slave receiver
3.5
-
th(SD_A_MR)
Data input hold time
Master receiver
3.5
-
th(SD_B_SR)
Slave receiver
-
tv(SD_B_ST)
Data output valid time
Slave transmitter (after enable edge),
2.7 V ≤ VDD ≤ 3.6 V
-
10.5
Slave transmitter (after enable edge),
1.62 V ≤ VDD ≤ 3.6 V
-
th(SD_B_ST)
Data output hold time
Slave transmitter (after enable edge)
6.5
-
tv(SD_A_MT)
Data output valid time
Master transmitter (after enable edge),
2.7 V ≤ VDD ≤ 3.6 V
-
10.5
Master transmitter (after enable edge),
1.62 V ≤ VDD ≤ 3.6 V
-
14.5
th(SD_A_MT)
Data output hold time
Master transmitter (after enable edge)
-
1.
Guaranteed by characterization results.
2.
APB clock frequency must be at least twice SAI clock frequency.
Table 110. SAI characteristics(1) (continued)
Symbol
Parameter
Conditions
Min
Max
Unit
MS32771V2
SAI_SCK_X
(CKSTR = 1)
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
Slot n
tsu(SD_MR)
th(SD_MR)
SAI_SCK_X
(CKSTR = 0)
th(SD_MT)

---

Figure 53. SAI slave timing waveforms
MDIO characteristics
Unless otherwise specified, the parameters given in Table 111 for the MDIO are derived
from tests performed under the ambient temperature, fPCLKx frequency and VDD supply
voltage conditions summarized in Table 12: General operating conditions, with the following
configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
I/O Compensation cell activated.
•
Measurement points are done at CMOS levels: 0.5VDD
•
HSLV activated when VDD ≤ 2.7 V
•
VOS level set to VOS0
MS32772V2
SAI_SCK_X
(CKSTR = 1)
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
SAI_SCK_X
(CKSTR = 0)
Table 111. MDIO slave timing parameters
Symbol
Parameter
Min
Typ
Max
Unit
FMDC
Management Data Clock
-
-
MHz
td(MDIO)
Management Data Iput/output output valid time
ns
tsu(MDIO)
Management Data Iput/output setup time
-
-
th(MDIO)
Management Data Iput/output hold time
-
-

---

Figure 54. MDIO slave timing diagram
SD/SDIO MMC card host interface (SDMMC) characteristics
Unless otherwise specified, the parameters given in Table 112 and Table 113 for SDIO are
derived from tests performed under the ambient temperature, fPCLKx frequency and VDD
supply voltage summarized in Table 12: General operating conditions, with the following
configuration:
•
Output speed is set to OSPEEDRy[1:0] = 11
•
Capacitive load CL=30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
•
IO Compensation cell activated.
•
HSLV activated when VDD ≤ 2.7 V
•
VOS level set to VOS0
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output
characteristics.
          Table 112. Dynamics characteristics: SD / MMC characteristics, VDD = 2.7 to 3.6 V(1)(2)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fPP
Clock frequency in data transfer
mode
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
fPP =52MHz
8.5
9.5
-
ns
tW(CKH)
Clock high time
8.5
9.5
-
CMD, D inputs (referenced to CK) in eMMC legacy/SDR/DDR and SD HS/SDR/DDR mode
tISU
Input setup time HS
-
2.5
-
-
ns
tIH
Input hold time HS
-
0.5
-
-
tIDW
(3)
Input valid window (variable window)
-
1.5
-
-
CMD, D outputs (referenced to CK) in eMMC legacy/SDR/DDR and SD HS/SDR/DDR mode
tOV
Output valid time HS
-
-
5.5
ns
tOH
Output hold time HS
-
4.5
-
-
MSv40460V1
tsu(MDIO)
tMDC)
th(MDIO)
td(MDIO)

---

CMD, D inputs (referenced to CK) in SD default mode
tISUD
Input setup time SD
-
1.5
-
ns
tIHD
Input hold time SD
-
0.5
-
CMD, D outputs (referenced to CK) in SD default mode
tOVD
Output valid default time SD
-
-
ns
tOHD
Output hold default time SD
-
-
-
1.
Guaranteed by characterization results.
2.
Above 100 MHz, CL = 20 pF.
3.
The minimum window of time where the data needs to be stable for proper sampling in tuning mode.
Table 113. Dynamics characteristics: eMMC characteristics VDD = 1.71V to 1.9V(1)(2)
1.
Guaranteed by characterization results.
2.
CL = 20 pF.
Symbol
Parameter
Conditions
Min
Typ
Max
Unit
fPP
Clock frequency in data transfer
mode
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
fPP =52 MHz
8.5
9.5
-
ns
tW(CKH)
Clock high time
8.5
9.5
-
CMD, D inputs (referenced to CK) in eMMC mode
tISU
Input setup time HS
-
1.5
-
-
ns
tIH
Input hold time HS
-
1.5
-
-
tIDW
(3)
3.
The minimum window of time where the data needs to be stable for proper sampling in tuning mode.
Input valid window (variable
window)
-
3.5
-
-
CMD, D outputs (referenced to CK) in eMMC mode
tOVD
Output valid time HS
-
-
6.5
ns
tOHD
Output hold time HS
-
5.5
-
-
Table 112. Dynamics characteristics: SD / MMC characteristics, VDD = 2.7 to 3.6 V(1)(2)
Symbol
Parameter
Conditions
Min
Typ
Max
Unit

---

Figure 55. SD high-speed mode
Figure 56. SD default mode
Figure 57. SDMMC DDR mode
USB OTG_FS characteristics
Unless otherwise specified, the parameters given in Table 115 for ULPI are derived from
tests performed under the ambient temperature, fPCLKx frequency and VDD supply voltage
summarized in Table 12: General operating conditions, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 11
•
Capacitive load CL=20 pF
•
Measurement points are done at CMOS levels: 0.5VDD
•
IO Compensation cell activated.
•
VOS level set to VOS0
MSv69709V1
CK
D, CMD output
D, CMD input
tOV
tOH
tISU
tIH
tC(CK)
tW(CKH)
tW(CKL)
MSv69710V1
CK
D, CMD output
tOV
tOH
MSv69158V1
CK
D output
tOV
tOH
tISU
tIH
D input
tOV
tOH
Valid data
tISU
tIH
Valid data
Valid data
Valid data
tW(CKH)
tW(CKL)

---

Refer to Section 6.3.16: I/O port characteristics for more details on the input/output
characteristics.
USB OTG_HS characteristics
Unless otherwise specified, the parameters given in Table 115 for ULPI are derived from
tests performed under the ambient temperature, fPCLKx frequency and VDD supply voltage
summarized in Table 12: General operating conditions, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 11
•
Capacitive load CL=20 pF
•
Measurement points are done at CMOS levels: 0.5VDD
•
IO Compensation cell activated.
•
VOS level set to VOS0
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output
characteristics.
Table 114. USB OTG_FS electrical characteristics
Symbol
Parameter
Condition
Min
Typ
Max
Unit
VDD33US
B
USB transceiver operating voltage
-
3.0(1)
1.
The USB functionality is ensured down to 2.7 V. However, not all USB electrical characteristics are
degraded in the 2.7 to 3.0 V voltage range.
-
3.6
V
RPUI
Embedded USB_DP pull-up value
during idle
-
Ω
RPUR
Embedded USB_DP pull-up value
during reception
-
ZDRV
Output driver impedance(2)
2.
No external termination series resistors are required on USB_DP (D+) and USB_DM (D-); the matching
impedance is already included in the embedded driver.
Driver high
and low
Table 115. Dynamics characteristics: USB ULPI(1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Condition
Min
Typ
Max
Unit
tSC
Control in (ULPI_DIR , ULPI_NXT)
setup time
-
5.5
-
-
ns
tHC
Control in (ULPI_DIR, ULPI_NXT) hold
time
-
-
-
tSD
Data in setup time
-
2.5
-
-
tHD
Data in hold time
-
-
-
tDC/tDD
Control/Datal output delay
2.7 V < VDD < 3.6 V,
CL = 20 pF
-
6.0
8.0
1.71 V < VDD < 3.6 V
, CL = 15 pF
-
6.0

---

Figure 58. ULPI timing diagram
Ethernet interface characteristics
Unless otherwise specified, the parameters given in Table 116, Table 117 and Table 118 for
SMI, RMII and MII are derived from tests performed under the ambient temperature,
frcc_c_ck frequency and VDD supply voltage conditions summarized in Table 12: General
operating conditions, with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 10
•
Capacitive load CL=20 pF
•
Measurement points are done at CMOS levels: 0.5VDD
•
IO Compensation cell activated.
•
HSLV activated when VDD ≤ 2.7 V
•
VOS level set to VOS1
Due to timing constraint Pxy_C I/Os cannot be used as ethernet signals.
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output
characteristics:
Table 116. Dynamics characteristics: Ethernet MAC signals for SMI (1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Typ
Max
Unit
tMDC
MDC cycle time( 2.5 MHz)
ns
Td(MDIO)
Write data valid time
0.5
1.5
tsu(MDIO)
Read data setup time
12.5
-
-
th(MDIO)
Read data hold time
-
-
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

Figure 59. Ethernet SMI timing diagram
Figure 60. Ethernet RMII timing diagram
Table 117. Dynamics characteristics: Ethernet MAC signals for RMII (1)
1.
Guaranteed by characterization results.
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
tsu(CRS)
Carrier sense setup time
1.5
 -
-
tih(CRS)
Carrier sense hold time
1.5
 -
-
td(TXEN)
Transmit enable valid delay time
10.5
td(TXD)
Transmit data valid delay time
9.5
MS31384V1
ETH_MDC
ETH_MDIO(O)
ETH_MDIO(I)
tMDC
td(MDIO)
tsu(MDIO)
th(MDIO)
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

---

Figure 61. Ethernet MII timing diagram
JTAG/SWD interface characteristics
Unless otherwise specified, the parameters given in Table 119 and Table 120 for
JTAG/SWD are derived from tests performed under the ambient temperature, frcc_c_ck
frequency and VDD supply voltage summarized in Table 12: General operating conditions,
with the following configuration:
•
Output speed is set to OSPEEDRy[1:0] = 11
•
Capacitive load CL=30 pF
•
Measurement points are done at CMOS levels: 0.5VDD
•
VOS level set to VOS0
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output
characteristics:
Table 118. Dynamics characteristics: Ethernet MAC signals for MII (1)
1.
Guaranteed by characterization results.
Symbol
Parameter
Min
Typ
Max
Unit
tsu(RXD)
Receive data setup time
2.0
-
-
ns
tih(RXD)
Receive data hold time
2.0
-
-
tsu(DV)
Data valid setup time
1.5
-
-
tih(DV)
Data valid hold time
1.5
-
-
tsu(ER)
Error setup time
1.5
-
-
tih(ER)
Error hold time
0.5
-
-
td(TXEN)
Transmit enable valid delay time
9.0
td(TXD)
Transmit data valid delay time
8.5
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

---

Figure 62. JTAG timing diagram
Table 119. Dynamics JTAG characteristics
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit
Fpp
TCK clock frequency
2.7V <VDD< 3.6 V
-
-
MHz
1/tc(TCK)
1.62 <VDD< 3.6 V
-
-
27.5
tisu(TMS)
TMS input setup time
-
2.5
-
-
tih(TMS)
TMS input hold time
-
-
-
tisu(TDI)
TDI input setup time
-
1.5
-
-
-
tih(TDI)
TDI input hold time
-
-
-
-
tov(TDO)
TDO output valid time
2.7V <VDD< 3.6 V
-
13.5
-
1.62 <VDD< 3.6 V
-
-
toh(TDO)
TDO output hold time
-
-
-
-
Table 120. Dynamics SWD characteristics
Symbol
Parameter
 Conditions
Min
Typ
Max
Unit
Fpp
SWCLK clock frequency
2.7V <VDD< 3.6 V
-
-
MHz
1/tc(SWCLK)
1.62 <VDD< 3.6 V
-
-
52.5
tisu(SWDIO)
SWDIO input setup time
-
2.5
-
-
-
tih(SWDIO)
SWDIO input hold time
-
-
-
-
tov(SWDIO)
SWDIO output valid time
2.7V <VDD< 3.6 V
-
8.5
-
1.62 <VDD< 3.6 V
-
8.5
-
toh(SWDIO)
SWDIO output hold time
-
-
-
-
MSv40458V1
TDI/TMS
TCK
TDO
tc(TCK)
tw(TCKL) tw(TCKH)
th(TMS/TDI)
tsu(TMS/TDI)
tov(TDO)
toh(TDO)

---

Figure 63. SWD timing diagram
MSv40459V1
SWDIO
SWCLK
SWDIO
tc(SWCLK)
twSWCLKL) tw(SWCLKH)
th(SWDIO)
tsu(SWDIO)
tov(SWDIO)
toh(SWDIO)
(receive)
(transmit)

---

Package information
Package information
In order to meet environmental requirements, ST offers these devices in different grades of
ECOPACK packages, depending on their level of environmental compliance. ECOPACK
specifications, grade definitions and product status are available at www.st.com. ECOPACK
is an ST trademark.
7.1
Device marking
Refer to technical note “Reference device marking schematics for STM32 microcontrollers
and microprocessors” (TN1433) available on www.st.com, for the location of pin 1 / ball A1
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
Figure 64. LQFP100 - Outline(15)
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
Table 121. LQFP100 - Mechanical data
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
Figure 65. LQFP100 - Footprint example
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
TFBGA100 package information (A08Q)
This TFBGA is 100 - ball, 8X8 mm, 0.8 mm pitch fine pitch ball grid array package.
Note:
See list of notes in the notes section.
Figure 66. TFBGA100 - Outline(13)
A08Q_UFBGA100_ME_V2
e
D
A
Øb (N balls)
E
SIDE VIEW
BOTTOM VIEW
A
A
B
ddd
C
D1
E1
eee
C A B
fff
Ø
Ø
M
M C
A1 ball pad
corner
A1 ball pad
corner
SD
e
B
C
D
E
G
H
J
A2
C
(4x)
Seating
plane
A1
aaa C
ccc
C
(DATUM A)
(DATUM B)
F
SE
K
TOP VIEW

---

Package information
Notes:
1.
Dimensioning and tolerancing schemes conform to ASME Y14.5M-2018.
2.
TFBGA stands for thin profile fine pitch ball grid array: 1.00 mm < A ≤ 1.20 mm / fine
pitch e < 1.00 mm.
3.
The profile height, A, is the distance from the seating plane to the highest point on the
package. It is measured perpendicular to the seating plane.
4.
A1 is defined as the distance from the seating plane to the lowest point on the package
body.
5.
Dimension b is measured at the maximum diameter of the terminal (ball) in a plane
parallel to primary datum C.
6.
BSC stands for BASIC dimensions. It corresponds to the nominal value and has no
tolerance. For tolerances refer to form and position table. On the drawing these
dimensions are framed.
7.
Primary datum C is defined by the plane established by the contact points of three or
more solder balls that support the device when it is placed on top of a planar surface.
8.
The terminal (ball) A1 corner must be identified on the top surface of the package by
using a corner chamfer, ink or metalized markings, or other feature of package body or
Table 122. TFBGA100 - Mechanical data
Symbol
millimeters(1)
inches(12)
Min
Typ
Max
Min
Typ
Max
A(2)(3)
-
-
1.20
-
-
0.0472
A1(4)
0.15
-
-
0.0059
-
-
A2
-
0.74
-
-
0.0291
-
b(5)
0.35
0.40
0.45
0.0138
0.0157
0.0177
D
8.00 BSC(6)
0.3150 BSC
D1
7.20 BSC
0.2835 BSC
E
8.00 BSC
0.3150 BSC
E1
7.20 BSC
0.2835 BSC
e(9)
0.80 BSC
0.0315 BSC
N(11)
SD(12)
0.40 BSC
0.0157 BSC
SE(12)
0.40 BSC
0.0157 BSC
aaa
0.15
0.0059
ccc
0.20
0.0079
ddd
0.10
0.0039
eee
0.15
0.0059
fff
0.08
0.0031

---

Package information
integral heat slug. A distinguish feature is allowable on the bottom surface of the
package to identify the terminal A1 corner. Exact shape of each corner is optional.
9.
e represents the solder ball grid pitch.
10. N represents the total number of balls on the BGA.
11. Basic dimensions SD and SE are defined with respect to datums A and B. It defines the
position of the centre ball(s) in the outer row or column of a fully populated matrix.
12. Values in inches are converted from mm and rounded to 4 decimal digits.
13. Drawing is not to scale.
Figure 67. TFBGA100 - Footprint example
Table 123. TFBGA100 - Example of PCB design rules (0.8 mm pitch BGA)
Dimension
Values
Pitch
0.8
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
7.4
LQFP144 package information (1A)
This LQFP is a 144-pin, 20 x 20 mm low-profile quad flat package.
Note:
See list of notes in the notes section.
Figure 68. LQFP144 - Outline(15)
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
Table 124. LQFP144 - Mechanical data
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
Figure 69. LQFP144 - Footprint example
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
UFBGA144 package information (A0AS)
Figure 70. UFBGA - 144 balls, 7 x 7 mm, 0.50 mm pitch, ultra fine pitch ball grid array
package outline
1.
Drawing is not to scale.
          Table 125. UFBGA - 144 balls, 7 x 7 mm, 0.50 mm pitch, ultra fine pitch ball grid array
package mechanical data
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
0.460
0.530
0.600
0.0181
0.0209
0.0236
A1
0.050
0.080
0.110
0.0020
0.0031
0.0043
A2
0.400
0.450
0.500
0.0157
0.0177
0.0197
A3
-
0.130
-
-
0.0051
-
A4
0.270
0.320
0.370
0.0106
0.0126
0.0146
b
0.230
0.280
0.320
0.0091
0.0110
0.0126
D
6.950
7.000
7.050
0.2736
0.2756
0.2776
D1
5.450
5.500
5.550
0.2146
0.2165
0.2185
E
6.950
7.000
7.050
0.2736
0.2756
0.2776
E1
5.450
5.500
5.550
0.2146
0.2165
0.2185
e
 -
0.500
-
-
0.0197
-
F
0.700
0.750
0.800
0.0276
0.0295
0.0315
A0AS_ME_V2
Seating plane
A1
e
F
F
D
M
Øb (144 balls)
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
A3
A4
A1 ball
identifier
A1 ball
index area

---

Package information
Figure 71. UFBGA - 144 balls, 7 x 7 mm, 0.50 mm pitch, ultra fine pitch ball grid array
package recommended footprint
ddd
-
-
 0.100
-
-
 0.0039
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
Table 126. UFBGA144 recommended PCB design rules (0.50 mm pitch BGA)
Dimension
Recommended values
Pitch
0.50 mm
Dpad
0.280 mm
Dsm
0.370 mm typ. (depends on the soldermask
registration tolerance)
Stencil opening
0.280 mm
Stencil thickness
Between 0.100 mm and 0.125 mm
Pad trace width
0.120 mm
Table 125. UFBGA - 144 balls, 7 x 7 mm, 0.50 mm pitch, ultra fine pitch ball grid array
package mechanical data (continued)
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
7.6
Thermal characteristics
The maximum chip-junction temperature, TJmax, in degrees Celsius, may be calculated
using the following equation:
TJmax = TA max + (PD max × ΘJA)
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
Table 127. Thermal characteristics
Symbol
Definition
Parameter
Value
Unit
ΘJA
Thermal resistance
junction-ambient
Thermal resistance junction-ambient
LQFP100 - 14 x 14 mm /0.5 mm pitch
43.8
°C/W
Thermal resistance junction-ambient
TFBGA100 - 8 x 8 mm /0.8 mm pitch
43.2
Thermal resistance junction-ambient
LQFP144 - 20 x 20 mm /0.5 mm pitch
44.8
Thermal resistance junction-ambient
UFBGA144 - 7 x 7 mm /0.5 mm pitch
ΘJB
Thermal resistance
junction-board
Thermal resistance junction-ambient
LQFP100 - 14 x 14 mm /0.5 mm pitch
19.8
°C/W
Thermal resistance junction-ambient
TFBGA100 - 8 x 8 mm /0.8 mm pitch
24.8
Thermal resistance junction-ambient
LQFP144 - 20 x 20 mm /0.5 mm pitch
24.4
Thermal resistance junction-ambient
UFBGA144 - 7 x 7 mm /0.5 mm pitch
21.1
ΘJC
Thermal resistance
junction-case
Thermal resistance junction-ambient
LQFP100 - 14 x 14 mm /0.5 mm pitch
7.3
°C/W
Thermal resistance junction-ambient
TFBGA100 - 8 x 8 mm /0.8 mm pitch
13.2
Thermal resistance junction-ambient
LQFP144 - 20 x 20 mm /0.5 mm pitch
7.4
Thermal resistance junction-ambient
UFBGA144 - 7 x 7 mm /0.5 mm pitch
8.7

---

Package information
7.6.1
Reference documents
•
JESD51-2 Integrated Circuits Thermal Test Method Environment Conditions - Natural
Convection (Still Air). Available from www.jedec.org.
•
For information on thermal management, refer to application note “Guidelines for
thermal management on STM32 applications” (AN5036) available from www.st.com.

---

Ordering information
Ordering information
For a list of available options (speed, package, etc.) or for further information on any aspect
of this device, please contact your nearest ST sales office.
Example:
STM32  H
V
G
T
 TR
Device family
STM32 = Arm-based 32-bit microcontroller
Product type
H = High performance
Device subfamily
723 = STM32H723
Pin count
V = 100 pins
Z = 144 pins
Flash memory size
E = 512 Kbytes
G = 1024 Kbytes
Package
T = LQFP ECOPACK2
I = UFBGA pitch 0.5 mm ECOPACK2
H = TFBGA ECOPACK2
Temperature range
6 = Industrial temperature range –40 to 85 °C
Packing
TR = tape and reel
No character = tray or tube

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

Table 128. Document revision history
Date
Changes
10-Jul-2020
Initial release.
03-Sep-2020
Renamed Section 3.30 into True random number generator (RNG).
Replaced VDDIOx by VDD in Section 6: Electrical characteristics.
Updated IIO in Table 10: Current characteristics.
Updated Table 24: Typical current consumption in Autonomous mode,
Table 27: Typical and maximum current consumption in Standby mode and
Table 28: Typical and maximum current consumption in VBAT mode.
Added Section 6.3.15: I/O current injection characteristics.
Removed reference to PI8 in Table 52: Output voltage characteristics for all
I/Os except PC13, PC14 and PC15 and Table 53: Output voltage
characteristics for PC13, PC14 and PC15.
Added Section 6.3.15: I/O current injection characteristics.
Added Section : Analog switch between ports Pxy_C and Pxy.
07-Dec-2021
Added indication that patents apply to the devices in Section : Features.
Added reference to errata sheet in Section 1: Introduction.
Table 1: STM32H723xE/G features and peripheral counts:
– Changed number of general-purpose 32-bit timers to 4.
– For LQFP100 and TFBGA100 packages, replaced 2 Octo-SPI/Quad-SPI
interfaces by 1 and remove note.
In Section 3.7.1: Power supply scheme, changed VDD power supply
requirements.
Section 3.34: Universal synchronous/asynchronous receiver transmitter
(USART): changed USART communication speed to 17 Mbit/s
Table 7: STM32H723 pin and ball descriptions:
– Added Note 1.to the package pin/balls corresponding to Pxy_C.
– For PA15(JTDI), replaced SPI3_NSS/I2S3_WS alternate function by
SPI3_NSS(boot)/I2S3_WS.
Moved LSI clock from backup domain to VDD domain in Figure 10: Power
supply scheme.
Added VBAT in Table 12: General operating conditions.
Updated Table 15: Operating conditions at power-up/power-down title and
added tVCORE.Added tVCORE in Table 15: Operating conditions at power-
up/power-down.
Updated measurement conditions for Typical and maximum current
consumption.
Section : On-chip peripheral current consumption: updated measurement
conditions and Table 29: Peripheral current consumption in Run mode.
Updated Table 31: High-speed external user clock characteristics.
Changed unit for PLL long-term jitter in Table 39: PLL1 characteristics (wide
VCO frequency range).
Renamed ILEAK into Ilkg in Table 51: I/O static characteristics.

---

07-Dec-2021
3 (continued)
Table 55: Output timing characteristics (HSLV ON): updated load capacitance
condition for tr/tf and speed = 10.
Updated Figure 31: OCTOSPI SDR read/write timing diagram, Figure 32:
OCTOSPI DTR mode timing diagram, Figure 33: OCTOSPI Hyperbus clock
timing diagram, Figure 34: OCTOSPI Hyperbus read timing diagram and
 Figure 35: OCTOSPI Hyperbus write timing diagram.
Updated sampling rate for slow channels in Table 80: 16-bit ADC
characteristics.
Updated Figure 36: ADC accuracy characteristics and Figure 37: Typical
connection diagram when using the ADC with FT/TT pins featuring analog
switch function as well as notes below figure.
Updated TL max value in Table 89: Temperature sensor characteristics.
Changed temperature condition to 130 °C for TS_CAL2 in Table 90:
Temperature sensor calibration values.
Updated Figure 38: Power supply and reference decoupling (VREF+ not
connected to VDDA).
Updated Figure 45: USART timing diagram in SPI master mode and Figure 46:
USART timing diagram in SPI slave mode.
Updated Figure 55: SD high-speed mode, Figure 56: SD default mode and
Figure 57: SDMMC DDR mode.
Updated Figure 61: Ethernet MII timing diagram.
Updated Figure 64: LQFP100 - Outline(15), Table 136: LQFP176 - Mechanical
data, and Figure 67: TFBGA100 - Footprint example.
16-Nov-2023
Updated Figure 1: STM32H723xE/G block diagram.
In Table 1: STM32H723xE/G features and peripheral counts: changed the
number of available Ethernet MII and SAI PDM interfaces.
Updated I2S communication modes in Section 3.36: Serial peripheral interface
(SPI)/inter- integrated sound interfaces (I2S).
Updated Section 3.36: Serial peripheral interface (SPI)/inter- integrated sound
interfaces (I2S).
Updated Section 3.37: Serial audio interfaces (SAI).
Added note to Chapter 6.2.
Updated IIO definition in Table 10: Current characteristics.
Updated VIN in Table 12: General operating conditions to cover the case of
Pxy_C.
In Table 16: Reset and power control block characteristics:
– renamed power-on/power-down reset threshold VPOR/PDR into
VBOR0/POR/PDR.
– updated description of Vhyst_POR_PDR.
– renamed Hysteresis voltage for Power-on/power-down reset (including
BOR0) into Vhyst_POR_PDR.
Updated measurement conditions for Typical and maximum current
consumption parameters.
Updated Section : High-speed external clock generated from a crystal/ceramic
resonator.
Table 128. Document revision history
Date
Changes

---

16-Nov-2023
4 (continued)
Updated Table 47: EMI characteristics for fHSE =  8 MHz and
fCPU = 550 MHz.
Updated Section : I/O static current consumption and Section : I/O dynamic
current consumption.
Updated VIH and VOH in Table 51: I/O static characteristics and Table 52:
Output voltage characteristics for all I/Os except PC13, PC14 and PC15,
respectively, to cover the case of Pxy_C I/Os.
Updated note 2 in Table 54: Output timing characteristics (HSLV OFF) and
Table 55: Output timing characteristics (HSLV ON).
Reorganized Section 6.3.18: FMC characteristics and updated Figure 27:
NAND controller waveforms for read access and Figure 28: NAND controller
waveforms for write access.
Updated tTRIG in Table 82: 16-bit ADC accuracy.
Changed VDAC_OUT maximum value (buffer ON) in Table 86: DAC
characteristics.
Updated fDFSDMCLK maximum value in Table 98: DFSDM measured timing.
In Table 107: USART (SPI mode) characteristics, changed tw(SCKH) and
tw(SCKL) into tw(CKH) and tw(CKL), respectively.
Updated Figure 47: SPI timing diagram - slave mode and CPHA = 0,
Figure 48: SPI timing diagram - slave mode and CPHA = 1 and Figure 49: SPI
timing diagram - master mode.
Updated Figure 52: SAI master timing waveforms and Figure 53: SAI slave
timing waveforms.
Section : Ethernet interface characteristics:
– added constraints on Pxy_C I/Os.
– updated typical td(TXEN) value in Table 118: Dynamics characteristics:
Ethernet MAC signals for MII.
Added Section 7.1: Device marking, and removed device marking sections for
all packages.
Updated Table 127: Thermal characteristics with ΘJA, ΘJB, and ΘJC values for
the UFBGA144 package.
Changed SPIx_SS to SPIx_NSS in:
– Figure 1: STM32H723xE/G block diagram.
– Section 3.36: Serial peripheral interface (SPI)/inter- integrated sound
interfaces (I2S).
– Table 7: STM32H723 pin and ball descriptions.
– Table 8: STM32H723 pin alternate functions.
– Section : SPI interface characteristics.
Table 128. Document revision history
Date
Changes

---

22-May-2025
In the context of I2C, replaced master and slave by controller and target,
respectively.
Features
– Replaced Dhrystone/DMIPS by Coremark score on cover page.
– Updated USART/UART/LPUART feature, modified the number of SPIs
Updated Figure 2: Power-up/power-down sequence and moved figure to
Section 3.7.2: Power supply supervisor.
Updated note 1 in Table 4: Timer feature comparison.
Updated Table 1: STM32H723xE/G features and peripheral counts.
Section 6: Electrical characteristics:
– Table 9: Voltage characteristics: updated maximum input voltage on FT_xxx
pins, and added note 5.
– Changed CoreMark values to 2778 in Table 23: Typical consumption in Run
mode and corresponding performance  versus code position.
– Updated Figure 16: Typical application with a 32.768 kHz crystal.
– Replaced fPCLK2 by fHCLK in Section 6.3.21: 16-bit ADC characteristics.
– Updated gain error (EG) in Table 85: 12-bit ADC accuracy.
– Updated Table 107: USART (SPI mode) characteristics conditions, as well
as Figure 46: USART timing diagram in SPI slave mode and Figure 45:
USART timing diagram in SPI master mode.
– Updated Figure 47: SPI timing diagram - slave mode and CPHA = 0 and
Figure 48: SPI timing diagram - slave mode and CPHA = 1.
Added Figure 22: Asynchronous multiplexed PSRAM/NOR write waveforms.
Table 128. Document revision history
Date
Changes

---

IMPORTANT NOTICE – PLEASE READ CAREFULLY
STMicroelectronics NV and its subsidiaries (“ST”) reserve the right to make changes, corrections, enhancements, modifications, and
improvements to ST products and/or to this document at any time without notice. Purchasers should obtain the latest relevant information on
ST products before placing orders. ST products are sold pursuant to ST’s terms and conditions of sale in place at the time of order
acknowledgement.
Purchasers are solely responsible for the choice, selection, and use of ST products and ST assumes no liability for application assistance or
the design of Purchasers’ products.
No license, express or implied, to any intellectual property right is granted by ST herein.
Resale of ST products with provisions different from the information set forth herein shall void any warranty granted by ST for such product.
ST and the ST logo are trademarks of ST. For additional information about ST trademarks, please refer to www.st.com/trademarks. All other
product or service names are the property of their respective owners.
Information in this document supersedes and replaces information previously supplied in any prior versions of this document.
© 2025 STMicroelectronics – All rights reserved
