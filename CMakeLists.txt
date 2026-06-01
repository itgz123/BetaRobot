##########################################################################################################################
# BetaRobot 顶层 CMakeLists.txt
#
# 功能：根据 user_cfg.h 中的 APP_NAME 选择应用，根据 app_cfg.h 中的 DEVELOPMENT_BOARD 选择开发板
#       读取 CubeMX 生成的 cmake/stm32cubemx/CMakeLists.txt 中的变量作为 HAL 源文件和头文件配置
#
# 使用方法：
#   cmake --preset Debug      # 配置 Debug 构建
#   cmake --preset Release    # 配置 Release 构建
#   cmake --build build -j24  # 编译
#   cmake --build build --target clean  # 清理
#
# 切换应用：修改 user_cfg.h 中的 APP_NAME
# 切换开发板：修改 app/<APP_NAME>/app_cfg.h 中的 DEVELOPMENT_BOARD
# 切换 CubeMX 配置：修改 app/<APP_NAME>/app_cfg.h 中的 HAL_CONFIG_NAME
# 切换构建类型：修改 user_cfg.h 中的 BUILD_TYPE (Debug/Release)
##########################################################################################################################

cmake_minimum_required(VERSION 3.20)

project(BetaRobot VERSION 1.0.0 LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(TARGET_NAME BetaRobot)

######################################
# 辅助函数
######################################
# 从头文件读取宏定义值
function(read_define output file_name define_name)
    file(STRINGS "${file_name}" define_line REGEX "^[ \t]*#[ \t]*define[ \t]+${define_name}[ \t]+[A-Za-z0-9_]+")
    if(NOT define_line)
        message(FATAL_ERROR "Cannot find ${define_name} in ${file_name}")
    endif()
    list(GET define_line 0 define_line)
    string(REGEX REPLACE "^[ \t]*#[ \t]*define[ \t]+${define_name}[ \t]+([A-Za-z0-9_]+).*$" "\\1" define_value "${define_line}")
    set(${output} "${define_value}" PARENT_SCOPE)
endfunction()

# 为路径列表添加前缀
function(prefix_paths output base)
    set(result)
    foreach(item IN LISTS ARGN)
        list(APPEND result "${base}/${item}")
    endforeach()
    set(${output} "${result}" PARENT_SCOPE)
endfunction()

# 规范化路径列表
function(normalize_paths output)
    set(result)
    foreach(item IN LISTS ARGN)
        get_filename_component(abs_path "${item}" ABSOLUTE)
        list(APPEND result "${abs_path}")
    endforeach()
    set(${output} "${result}" PARENT_SCOPE)
endfunction()

######################################
# 从 user_cfg.h 读取 APP_NAME
######################################
read_define(APP_NAME "${CMAKE_SOURCE_DIR}/user_cfg.h" APP_NAME)
set(APP_DIR "${CMAKE_SOURCE_DIR}/app/${APP_NAME}")

if(NOT EXISTS "${APP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "App ${APP_NAME} is not initialized or has no CMakeLists.txt: ${APP_DIR}")
endif()

######################################
# 从 app_cfg.h 读取 HAL_CONFIG_NAME 和 DEVELOPMENT_BOARD
######################################
read_define(HAL_CONFIG_NAME "${APP_DIR}/app_cfg.h" HAL_CONFIG_NAME)
set(HAL_DIR "${CMAKE_SOURCE_DIR}/cubemx/${HAL_CONFIG_NAME}")

read_define(DEVELOPMENT_BOARD "${APP_DIR}/app_cfg.h" DEVELOPMENT_BOARD)
set(BOARD "${DEVELOPMENT_BOARD}")

# 从 bsp_map.h 读取 CPU 内核类型，用于 ARM_MATH_CM4/CM7 定义
read_define(CPU_CORE "${CMAKE_SOURCE_DIR}/cubemx/board_config/${BOARD}/bsp_map.h" CPU_CORE)
# CPU_CORE 定义为 CORTEX_M4 或 CORTEX_M7，CMSIS-DSP 需要 ARM_MATH_CM4 或 ARM_MATH_CM7
if(CPU_CORE STREQUAL "CORTEX_M7")
    set(ARM_MATH_DEFINE "ARM_MATH_CM7")
else()
    set(ARM_MATH_DEFINE "ARM_MATH_CM4")
endif()

# 读取是否生成反汇编文件（GENERATE_DISASSEMBLY，0=否，1=是）
read_define(GENERATE_DISASSEMBLY "${APP_DIR}/app_cfg.h" GENERATE_DISASSEMBLY)

if(NOT EXISTS "${HAL_DIR}")
    message(FATAL_ERROR
        "HAL_CONFIG_NAME '${HAL_CONFIG_NAME}' → directory not found:\n"
        "  ${HAL_DIR}\n"
        "  Possible reasons:\n"
        "  1. Submodule not initialized: run 'git submodule update --init'\n"
        "  2. Typo in app/${APP_NAME}/app_cfg.h HAL_CONFIG_NAME\n"
        "  3. Missing CubeMX config: need to create cubemx/${HAL_CONFIG_NAME}/")
endif()

# 导入模块（include 子 CMakeLists.txt 并添加路径前缀）
function(import_module module_dir result_sources result_includes)
    include("${module_dir}/CMakeLists.txt")
    prefix_paths(${result_sources} "${module_dir}" ${MODULE_SOURCES})
    prefix_paths(${result_includes} "${module_dir}" ${MODULE_INCLUDE_DIRS})
    set(${result_sources} ${${result_sources}} PARENT_SCOPE)
    set(${result_includes} ${${result_includes}} PARENT_SCOPE)
endfunction()

# 导入 app/bsp/drv 模块
import_module("${APP_DIR}" APP_SOURCE_FILES APP_INCLUDE_PATHS)
import_module("${CMAKE_SOURCE_DIR}/bsp" BSP_SOURCE_FILES BSP_INCLUDE_PATHS)
import_module("${CMAKE_SOURCE_DIR}/drv" DRV_SOURCE_FILES DRV_INCLUDE_PATHS)

######################################
# 从 user_cfg.h 读取 BUILD_TYPE（可选，未定义默认 Debug）
######################################
file(STRINGS "${CMAKE_SOURCE_DIR}/user_cfg.h" build_line REGEX "^[ \t]*#[ \t]*define[ \t]+BUILD_TYPE[ \t]+[A-Za-z0-9_]+")
if(build_line)
    string(REGEX REPLACE ".*BUILD_TYPE[ \t]+([A-Za-z0-9_]+).*" "\\1" BUILD_TYPE_NAME "${build_line}")
else()
    set(BUILD_TYPE_NAME "Debug")
endif()

set(ALLOWED_BUILD_TYPES Debug Release RelWithDebInfo MinSizeRel)
if(NOT BUILD_TYPE_NAME IN_LIST ALLOWED_BUILD_TYPES)
    message(WARNING "Invalid BUILD_TYPE '${BUILD_TYPE_NAME}', defaulting to Debug. Allowed: ${ALLOWED_BUILD_TYPES}")
    set(BUILD_TYPE_NAME "Debug")
endif()

set(CMAKE_BUILD_TYPE "${BUILD_TYPE_NAME}" CACHE STRING "" FORCE)

######################################
# 配置变更检测：APP/BOARD/BUILD_TYPE/CubeMX 变化时自动清理
######################################
# 计算 CubeMX CMakeLists.txt 的修改时间作为 fingerprint 的一部分
if(EXISTS "${HAL_DIR}/cmake/stm32cubemx/CMakeLists.txt")
    file(TIMESTAMP "${HAL_DIR}/cmake/stm32cubemx/CMakeLists.txt" CUBEMX_TIMESTAMP UTC)
else()
    set(CUBEMX_TIMESTAMP "0")
endif()

set(CFG_FINGERPRINT "${APP_NAME}.${DEVELOPMENT_BOARD}.${HAL_CONFIG_NAME}.${CMAKE_BUILD_TYPE}.${CUBEMX_TIMESTAMP}")
set(CFG_STAMP "${CMAKE_BINARY_DIR}/.cfg_stamp")
set(CFG_STAMP_DIR "${CMAKE_BINARY_DIR}")

# 在配置阶段检测变更（build 目录可能不存在）
if(EXISTS "${CFG_STAMP}")
    file(READ "${CFG_STAMP}" OLD_FINGERPRINT LIMIT 256)
    string(STRIP "${OLD_FINGERPRINT}" OLD_FINGERPRINT)
    if(NOT CFG_FINGERPRINT STREQUAL OLD_FINGERPRINT)
        message(STATUS "")
        message(STATUS "========================================")
        message(STATUS "Configuration changed: ${OLD_FINGERPRINT} -> ${CFG_FINGERPRINT}")
        message(STATUS "Cleaning build directory...")
        message(STATUS "========================================")
        message(STATUS "")
        # 清理整个 build 目录
        file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}")
    endif()
endif()

# 确保 build 目录存在并写入新的 fingerprint
if(NOT EXISTS "${CFG_STAMP_DIR}")
    file(MAKE_DIRECTORY "${CFG_STAMP_DIR}")
endif()
file(WRITE "${CFG_STAMP}" "${CFG_FINGERPRINT}")

######################################
# 读取 CubeMX 生成的变量
######################################
include("${CMAKE_SOURCE_DIR}/cmake/extract-cubemx-vars.cmake")
set(CUBEMX_CMAKE "${HAL_DIR}/cmake/stm32cubemx/CMakeLists.txt")

if(NOT EXISTS "${CUBEMX_CMAKE}")
    message(FATAL_ERROR "CubeMX CMake not found: ${CUBEMX_CMAKE}")
endif()

load_cubemx_vars("${CUBEMX_CMAKE}" "${HAL_DIR}")

######################################
# 整理 HAL 源文件、头文件和宏定义
######################################
normalize_paths(HAL_DRIVER_SOURCES ${STM32_Drivers_Src})
normalize_paths(HAL_FREERTOS_SOURCES ${FreeRTOS_Src})
normalize_paths(HAL_INCLUDE_PATHS ${MX_Include_Dirs})

set(HAL_C_SOURCES
    ${HAL_DRIVER_SOURCES}
    ${HAL_FREERTOS_SOURCES}
)

set(STARTUP_FILE)
foreach(src IN LISTS MX_Application_Src)
    get_filename_component(abs_src "${src}" ABSOLUTE)
    if(abs_src MATCHES "[/\\\\]startup_([^/\\\\]+)\\.s$")
        # 使用 board_config 下的启动文件，而不是 CubeMX 生成的
        set(STARTUP_FILE "${CMAKE_SOURCE_DIR}/cubemx/board_config/${BOARD}/startup_${CMAKE_MATCH_1}.s")
    else()
        list(APPEND HAL_C_SOURCES "${abs_src}")
    endif()
endforeach()

if(NOT STARTUP_FILE OR NOT EXISTS "${STARTUP_FILE}")
    message(FATAL_ERROR "Startup file not found for board ${BOARD}")
endif()

set(HAL_DEFINES)
foreach(def IN LISTS MX_Defines_Syms)
    if(def)
        list(APPEND HAL_DEFINES "${def}")
    endif()
endforeach()

######################################
# MCU 配置（数据驱动）
######################################
# 开发板配置表: CPU;FPU;链接脚本
set(BOARD_CFG_STM32F407VET6 "cortex-m4;fpv4-sp-d16;STM32F407XX_FLASH.ld")
set(BOARD_CFG_DM_MC02      "cortex-m7;fpv5-d16;STM32H723XG_FLASH.ld")
set(BOARD_CFG_DJI_A        "cortex-m4;fpv4-sp-d16;STM32F427XX_FLASH.ld")
set(BOARD_CFG_DJI_C        "cortex-m4;fpv4-sp-d16;STM32F407XX_FLASH.ld")

if(NOT DEFINED BOARD_CFG_${BOARD})
    message(FATAL_ERROR "Unknown DEVELOPMENT_BOARD: ${BOARD}")
endif()

list(GET BOARD_CFG_${BOARD} 0 MCU_CPU)
list(GET BOARD_CFG_${BOARD} 1 MCU_FPU)
list(GET BOARD_CFG_${BOARD} 2 LD_SCRIPT)
set(MCU_FLAGS -mcpu=${MCU_CPU} -mthumb -mfpu=${MCU_FPU} -mfloat-abi=hard)
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/cubemx/board_config/${BOARD}/${LD_SCRIPT}")

######################################
# 项目源文件列表
######################################
set(PROJECT_SOURCES
    ${HAL_C_SOURCES}
    ${APP_SOURCE_FILES}
    ${BSP_SOURCE_FILES}
    ${DRV_SOURCE_FILES}
    ${CMAKE_SOURCE_DIR}/cubemx/board_config/${BOARD}/bsp_map.c
    # CMSIS-DSP 源文件
    ${HAL_DIR}/Drivers/CMSIS/DSP/Source/CommonTables/arm_common_tables.c
    ${HAL_DIR}/Drivers/CMSIS/DSP/Source/CommonTables/arm_const_structs.c
    ${HAL_DIR}/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_sin_f32.c
    ${HAL_DIR}/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_cos_f32.c
)

# V1.10.0 开发板额外加入 arm_atan2_f32.c（DM_MC02 使用 V1.6.0 无此文件）
if(NOT BOARD STREQUAL "DM_MC02")
    list(APPEND PROJECT_SOURCES ${HAL_DIR}/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_atan2_f32.c)
endif()

# 启动文件和 RTT 汇编文件
list(APPEND PROJECT_SOURCES
    ${STARTUP_FILE}
)

######################################
# 可执行目标
######################################
add_executable(${TARGET_NAME} ${PROJECT_SOURCES})
set_target_properties(${TARGET_NAME} PROPERTIES SUFFIX ".elf")

######################################
# 编译定义
######################################
target_compile_definitions(${TARGET_NAME} PRIVATE
    ${HAL_DEFINES}
    DEVELOPMENT_BOARD=${DEVELOPMENT_BOARD}
    ${ARM_MATH_DEFINE}
)

######################################
# 头文件路径
######################################
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${APP_INCLUDE_PATHS}
    ${BSP_INCLUDE_PATHS}
    ${DRV_INCLUDE_PATHS}
    ${HAL_INCLUDE_PATHS}
    # CMSIS-DSP 头文件
    ${HAL_DIR}/Drivers/CMSIS/DSP/Include
    ${HAL_DIR}/Drivers/CMSIS/DSP/PrivateInclude
)

######################################
# 编译选项
######################################
target_compile_options(${TARGET_NAME} PRIVATE
    ${MCU_FLAGS}
    -Wall
    -fdata-sections
    -ffunction-sections
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Debug>:-gdwarf-2>
    $<$<CONFIG:Release>:-Os>
    $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>
)

######################################
# 链接选项
######################################
target_link_options(${TARGET_NAME} PRIVATE
    ${MCU_FLAGS}
    -specs=nano.specs
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_BINARY_DIR}/${TARGET_NAME}.map,--cref
    -Wl,--gc-sections
    -Wl,--print-memory-usage
)

target_link_libraries(${TARGET_NAME} PRIVATE c m nosys)

######################################
# 构建后生成 hex/bin
######################################
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_NAME}>
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET_NAME}> ${CMAKE_BINARY_DIR}/${TARGET_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:${TARGET_NAME}> ${CMAKE_BINARY_DIR}/${TARGET_NAME}.bin
)

# 根据 GENERATE_DISASSEMBLY 决定是否生成反汇编文件
if(GENERATE_DISASSEMBLY)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJDUMP} -d -S $<TARGET_FILE:${TARGET_NAME}> > ${CMAKE_BINARY_DIR}/${TARGET_NAME}.lst
        COMMENT "Generating disassembly file..."
    )
endif()

######################################
# 平台检测和工具适配
######################################
# 查找工具可用性（find_program 会自动处理 .exe 后缀）
find_program(JLINK_PROGRAM JLink)
find_program(OPENOCD_PROGRAM openocd)

######################################
# 下载目标
######################################
if(OPENOCD_PROGRAM)
    add_custom_target(download_dap
        COMMAND ${OPENOCD_PROGRAM} -f ${CMAKE_SOURCE_DIR}/cubemx/board_config/${BOARD}/openocd_daplink.cfg -c "program $<TARGET_FILE:${TARGET_NAME}> verify reset exit"
        DEPENDS ${TARGET_NAME}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Downloading via DAP-Link..."
    )
else()
    add_custom_target(download_dap
        COMMAND ${CMAKE_COMMAND} -E echo "OpenOCD not found. Please install OpenOCD and add it to PATH."
        DEPENDS ${TARGET_NAME}
    )
endif()

if(JLINK_PROGRAM)
    add_custom_target(download_jlink
        COMMAND ${JLINK_PROGRAM} -CommanderScript ${CMAKE_SOURCE_DIR}/cubemx/board_config/${BOARD}/jlink_download.jlink
        DEPENDS ${TARGET_NAME}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Downloading via J-Link..."
    )
else()
    add_custom_target(download_jlink
        COMMAND ${CMAKE_COMMAND} -E echo "JLink not found. Please install J-Link and add it to PATH."
        DEPENDS ${TARGET_NAME}
    )
endif()

######################################
# RTT 调试连接目标
######################################
if(OPENOCD_PROGRAM)
    if(WIN32)
        # Windows: 使用 PowerShell 后台启动 openocd
        add_custom_target(rtt_connect
            COMMAND ${CMAKE_COMMAND} -E echo "Starting OpenOCD RTT server..."
            COMMAND powershell -Command "Start-Process openocd -ArgumentList '-f','${CMAKE_SOURCE_DIR}/cubemx/board_config/${BOARD}/openocd_rtt.cfg'"
            COMMAND ${CMAKE_COMMAND} -E sleep 2
            COMMAND telnet 127.0.0.1 8888
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Connecting to RTT..."
        )
    else()
        # Linux/macOS: 后台启动 openocd
        add_custom_target(rtt_connect
            COMMAND ${CMAKE_COMMAND} -E echo "Starting OpenOCD RTT server..."
            COMMAND openocd -f ${CMAKE_SOURCE_DIR}/cubemx/board_config/${BOARD}/openocd_rtt.cfg &
            COMMAND ${CMAKE_COMMAND} -E sleep 2
            COMMAND telnet 127.0.0.1 8888
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Connecting to RTT..."
        )
    endif()
else()
    add_custom_target(rtt_connect
        COMMAND ${CMAKE_COMMAND} -E echo "OpenOCD not found. Please install OpenOCD and add it to PATH."
    )
endif()

######################################
# 打印配置信息
######################################
message(STATUS "APP_NAME: ${APP_NAME}")
message(STATUS "DEVELOPMENT_BOARD: ${DEVELOPMENT_BOARD}")
message(STATUS "HAL_CONFIG_NAME: ${HAL_CONFIG_NAME}")
message(STATUS "BUILD_TYPE: ${BUILD_TYPE_NAME}")
message(STATUS "GENERATE_DISASSEMBLY: ${GENERATE_DISASSEMBLY}")
message(STATUS "HAL_DIR: ${HAL_DIR}")
