##########################################################################################################################
# BetaRobot 顶层 Makefile
# 
# 功能：根据 DEVELOPMENT_BOARD 宏选择对应开发板的 hal 目录，导入 CubeMX 生成的 Makefile
# 
# 使用方法：
#   make                  # 使用默认开发板编译
#   make BOARD=DM_MC02    # 指定开发板编译
#   make clean            # 清理构建
##########################################################################################################################

######################################
# 开发板选择
######################################
# 与 app/inc/app_cfg.h 中的定义保持一致
# 可通过命令行传入: make BOARD=DM_MC02
BOARD ?= TELESKY_VET6

# 根据开发板选择 hal 目录
ifeq ($(BOARD), TELESKY_VET6)
    HAL_DIR = hal/STM32F407VET6
else ifeq ($(BOARD), DM_MC02)
    HAL_DIR = hal/STM32H723VGT6
else ifeq ($(BOARD), DJI_A)
    HAL_DIR = hal/STM32F427IIH6
else ifeq ($(BOARD), DJI_C)
    HAL_DIR = hal/STM32F407IGH6
else
    $(error Unknown BOARD: $(BOARD). Valid options: TELESKY_VET6, DM_MC02, DJI_A, DJI_C)
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
C_SOURCES += main.c

######################################
# 修正 ASM_SOURCES
######################################
ASM_SOURCES := $(addprefix $(HAL_DIR)/,$(ASM_SOURCES))
ASMM_SOURCES := $(addprefix $(HAL_DIR)/,$(ASMM_SOURCES))

######################################
# 修正 C_INCLUDES（将 -Ixxx 替换为 -I$(HAL_DIR)/xxx）
######################################
C_INCLUDES := $(patsubst -I%,-I$(HAL_DIR)/%,$(C_INCLUDES))
AS_INCLUDES := $(patsubst -I%,-I$(HAL_DIR)/%,$(AS_INCLUDES))

######################################
# 添加公共头文件路径
######################################
C_INCLUDES += -Iapp/inc
C_INCLUDES += -Ibsp/inc
C_INCLUDES += -Idrv/inc

######################################
# 修正 LDSCRIPT
######################################
LDSCRIPT := $(HAL_DIR)/$(LDSCRIPT)

######################################
# 添加开发板选择宏
######################################
C_DEFS += -DDEVELOPMENT_BOARD=$(BOARD)

######################################
# 重新计算 vpath（CFLAGS 会自动使用更新后的变量）
######################################
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))
vpath %.S $(sort $(dir $(ASMM_SOURCES)))

# *** EOF ***