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
BOARD_VALUE := $(shell grep -E "^#define DEVELOPMENT_BOARD" app/inc/app_cfg.h | awk '{print $$3}')

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
bsp/src/bsp_cfg.c \
bsp/src/bsp_dwt.c \
bsp/src/bsp_gpio.c \
bsp/src/bsp_log.c \
app/src/app_robot.c \
app/src/app_cmd.c \
app/src/app_chassis.c \
app/src/app_motor.c \
app/src/app_error.c \
$(HAL_DIR)/Drivers/CMSIS/DSP/Source/CommonTables/arm_common_tables.c \
$(HAL_DIR)/Drivers/CMSIS/DSP/Source/CommonTables/arm_const_structs.c \
$(HAL_DIR)/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_sin_f32.c \
$(HAL_DIR)/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_cos_f32.c \
$(HAL_DIR)/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_atan2_f32.c \
bsp/src/bsp_usart.c \
bsp/src/bsp_tim.c \
drv/src/drv_sbus.c \

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

######################################
# 添加公共头文件路径
######################################
C_INCLUDES += -Iapp/inc \
-Ibsp/inc \
-Idrv/inc \
-IMiddlewares/Third_Party/SEGGER/RTT \
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

# ######################################
# # build 目录创建（覆盖子 Makefile）
# ######################################
# $(BUILD_DIR):
# 	powershell -Command "New-Item -ItemType Directory -Force -Path build | Out-Null"

# ######################################
# # 清理目标（覆盖子 Makefile）
# ######################################
# .PHONY: clean
# clean:
# 	powershell -Command "if (Test-Path build) { Remove-Item -Recurse -Force build/* }"

# *** EOF ***