##########################################################################################################################
# BetaRobot 顶层 Makefile
# 
# 功能：根据 app_cfg.h 中的 DEVELOPMENT_BOARD 宏选择对应开发板的 hal 目录
# 
# 使用方法：
#   make        # 编译（根据 app_cfg.h 中的 DEVELOPMENT_BOARD 宏）
#   make clean  # 清理构建
# 
# 切换开发板：修改 app/inc/app_cfg.h 中的 DEVELOPMENT_BOARD 宏
##########################################################################################################################

######################################
# 从 app_cfg.h 读取 DEVELOPMENT_BOARD 值
######################################
# DEVELOPMENT_BOARD 定义格式: #define DEVELOPMENT_BOARD STM32F407VET6 (或 0/1/2/3)
# 这个必须要有git或者msys，因为使用了grep和swk
BOARD_VALUE := $(shell grep -E "^#define DEVELOPMENT_BOARD" app/app_cfg.h | awk '{print $$3}')

# 根据值选择开发板名称和 hal 目录
# 值为 0/STM32F407VET6 -> hal/STM32F407VET6
# 值为 1/DM_MC02      -> hal/DM_MC02
# 值为 2/DJI_A        -> hal/DJI_A
# 值为 3/DJI_C        -> hal/DJI_C
ifeq ($(BOARD_VALUE), 0)
    BOARD_NAME = STM32F407VET6
    HAL_DIR = hal/STM32F407VET6
else ifeq ($(BOARD_VALUE), 1)
    BOARD_NAME = DM_MC02
    HAL_DIR = hal/DM_MC02
else ifeq ($(BOARD_VALUE), 2)
    BOARD_NAME = DJI_A
    HAL_DIR = hal/DJI_A
else ifeq ($(BOARD_VALUE), 3)
    BOARD_NAME = DJI_C
    HAL_DIR = hal/DJI_C
else ifeq ($(BOARD_VALUE), STM32F407VET6)
    BOARD_NAME = STM32F407VET6
    HAL_DIR = hal/STM32F407VET6
else ifeq ($(BOARD_VALUE), DM_MC02)
    BOARD_NAME = DM_MC02
    HAL_DIR = hal/DM_MC02
else ifeq ($(BOARD_VALUE), DJI_A)
    BOARD_NAME = DJI_A
    HAL_DIR = hal/DJI_A
else ifeq ($(BOARD_VALUE), DJI_C)
    BOARD_NAME = DJI_C
    HAL_DIR = hal/DJI_C
else
    $(error Unknown DEVELOPMENT_BOARD: $(BOARD_VALUE). Valid options: 0/STM32F407VET6, 1/DM_MC02, 2/DJI_A, 3/DJI_C)
endif

######################################
# 构建变量
######################################
# 调试构建 (1=开启, 0=关闭)
DEBUG ?= 1

# 优化等级（在 include 之前 override，覆盖子 Makefile 的定义）
# -O0: 无优化，最易调试
# -O1: 基本优化
# -O2: 推荐的发布优化
# -O3: 激进优化
# -Og: 调试友好优化（默认）
# -Os: 代码大小优化
OPT ?= -Og
override OPT := $(OPT)

######################################
# 平台检测
######################################
# 检测操作系统（Git Bash/MinGW 下 uname -s 返回 MINGW64_NT-x.x 或 MSYS_NT-x.x）
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
# 判断是否为 Windows（包含 NT 或 MINGW 或 MSYS 或 Windows）
ifneq ($(findstring NT,$(UNAME_S)),)
    IS_WINDOWS := 1
else ifneq ($(findstring MINGW,$(UNAME_S)),)
    IS_WINDOWS := 1
else ifneq ($(findstring MSYS,$(UNAME_S)),)
    IS_WINDOWS := 1
else ifneq ($(findstring Windows,$(UNAME_S)),)
    IS_WINDOWS := 1
else
    IS_WINDOWS := 0
endif

######################################
# 在 include 之前覆盖 TARGET
# 这样子 Makefile 解析规则时会使用我们的 TARGET
######################################
override TARGET = BetaRobot

######################################
# 导入 CubeMX 生成的 Makefile
######################################
include $(HAL_DIR)/Makefile

######################################
# 修正 C_SOURCES
######################################
# 过滤 CubeMX 的 main.c，添加 HAL_DIR 前缀，添加根目录 main.c
C_SOURCES := $(addprefix $(HAL_DIR)/,$(filter-out Core/Src/main.c,$(C_SOURCES)))
C_SOURCES += main.c \
Middlewares/Third_Party/SEGGER/RTT/SEGGER_RTT_printf.c \
Middlewares/Third_Party/SEGGER/RTT/SEGGER_RTT.c \
app/app_robot/app_robot.c \
app/app_cmd/app_cmd.c \
app/app_chassis/app_chassis.c \
app/app_motor/app_motor.c \
app/app_error/app_error.c \
bsp/bsp_cfg/bsp_cfg.c \
bsp/bsp_dwt/bsp_dwt.c \
bsp/bsp_gpio/bsp_gpio.c \
bsp/bsp_log/bsp_log.c \
bsp/bsp_math/bsp_math.c \
bsp/bsp_spi/bsp_spi.c \
bsp/bsp_tim/bsp_tim.c \
bsp/bsp_usart/bsp_usart.c \
bsp/bsp_adc/bsp_adc.c \
drv/drv_motor/drv_dcmotor/drv_dcmotor.c \
drv/drv_pid/drv_pid.c \
drv/drv_chassis/drv_chassis.c \
drv/drv_sbus/drv_sbus.c \
$(HAL_DIR)/Drivers/CMSIS/DSP/Source/CommonTables/arm_common_tables.c \
$(HAL_DIR)/Drivers/CMSIS/DSP/Source/CommonTables/arm_const_structs.c \
$(HAL_DIR)/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_sin_f32.c \
$(HAL_DIR)/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_cos_f32.c \
drv/drv_bmi088/drv_bmi088.c \
app/app_sensor/app_sensor.c \
bsp/bsp_adc/bsp_can.c \

# arm_atan2_f32.c 仅在 CMSIS-DSP V1.9.0+ 中存在
# DJI_A 使用 V1.10.0，支持此文件
ifneq ($(filter $(BOARD_VALUE),DJI_A 2),)
C_SOURCES += $(HAL_DIR)/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_atan2_f32.c
endif

######################################
# 修正 ASM_SOURCES
######################################
ASM_SOURCES := $(addprefix $(HAL_DIR)/,$(ASM_SOURCES))
ASMM_SOURCES := $(addprefix $(HAL_DIR)/,$(ASMM_SOURCES))
ASM_SOURCES += Middlewares/Third_Party/SEGGER/RTT/SEGGER_RTT_ASM_ARMv7M.s \

######################################
# 修正 C_INCLUDES（将 -Ixxx 替换为 -I$(HAL_DIR)/xxx）
######################################
C_INCLUDES := $(patsubst -I%,-I$(HAL_DIR)/%,$(C_INCLUDES))
AS_INCLUDES := $(patsubst -I%,-I$(HAL_DIR)/%,$(AS_INCLUDES))

# CubeMX 模板中的 .s/.S 规则使用 $(CFLAGS)，
# 为保证仅修改根 Makefile 时 AS_INCLUDES 仍能生效，将其并入 CFLAGS。
override CFLAGS += $(AS_DEFS) $(AS_INCLUDES)

######################################
# 手动维护的头文件目录
######################################
# app 目录及其子目录
ALL_DIRS = app \
app/app_chassis \
app/app_cmd \
app/app_error \
app/app_motor \
app/app_robot \
bsp \
bsp/bsp_adc \
bsp/bsp_cfg \
bsp/bsp_dwt \
bsp/bsp_gpio \
bsp/bsp_log \
bsp/bsp_math \
bsp/bsp_module \
bsp/bsp_spi \
bsp/bsp_tim \
bsp/bsp_usart \
drv \
drv/drv_bmi088 \
drv/drv_chassis \
drv/drv_motor \
drv/drv_motor/drv_dcmotor \
drv/drv_pid \
drv/drv_sbus \
app/app_sensor
bsp/bsp_can \

# 添加 -I 前缀
C_INCLUDES += $(addprefix -I,$(ALL_DIRS))

# 添加其他公共头文件路径
C_INCLUDES += -IMiddlewares/Third_Party/SEGGER/RTT \
-IMiddlewares/Third_Party/SEGGER/Config \
-I$(HAL_DIR)/Drivers/CMSIS/DSP/Include \
-I$(HAL_DIR)/Drivers/CMSIS/DSP/PrivateInclude \

######################################
# 修正 LDSCRIPT
######################################
LDSCRIPT := $(HAL_DIR)/$(LDSCRIPT)

######################################
# 编译标志扩展
######################################
# 额外警告选项（子 Makefile 已有 -Wall）
# override CFLAGS += -Wextra -Wshadow

# 链接时优化（LTO），可减小代码体积（可选）
# override CFLAGS += -flto
# override LDFLAGS += -flto

######################################
# 添加开发板选择宏
######################################
C_DEFS += -DDEVELOPMENT_BOARD=$(BOARD_NAME)

######################################
# CMSIS-DSP 支持（Cortex-M4F）
######################################
C_DEFS += -DARM_MATH_CM4

######################################
# 重新计算 vpath（CFLAGS 会自动使用更新后的变量）
######################################
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))
vpath %.S $(sort $(dir $(ASMM_SOURCES)))

######################################
# 重新计算 OBJECTS（因为 C_SOURCES 和 ASM_SOURCES 已修改）
######################################
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASMM_SOURCES:.S=.o)))

######################################
# 重新定义 elf 目标的依赖（因为 OBJECTS 已修改）
######################################
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)

######################################
# 下载目标
######################################
.PHONY: download_dap download_jlink

# DAPlink 下载
download_dap: all
	openocd -f hal/$(BOARD_NAME)/openocd_daplink.cfg -c "program build/$(TARGET).elf verify reset exit"

# Jlink 下载
download_jlink: all
	JLink.exe -CommanderScript hal/$(BOARD_NAME)/jlink_download.jlink

######################################
# build 目录创建（自动检测平台）
######################################
$(BUILD_DIR):
ifeq ($(IS_WINDOWS), 1)
	powershell -Command "New-Item -ItemType Directory -Force -Path build | Out-Null"
else
	mkdir -p $@
endif

######################################
# 清理目标（自动检测平台）
######################################
.PHONY: clean
clean:
ifeq ($(IS_WINDOWS), 1)
	powershell -Command "if (Test-Path build) { Remove-Item -Recurse -Force build/* }"
else
	rm -rf $(BUILD_DIR)
endif

# *** EOF ***