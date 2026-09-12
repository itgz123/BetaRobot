##########################################################################################################################
# BetaRobot 顶层 CMakeLists.txt
#
# 功能：自动发现 app/ 下的所有应用，每个应用一个独立 target（Ninja Multi-Config）
#       各 app 的独立产物天然隔离在 CMakeFiles/<app>.dir/<Debug|Release>/ 下
#       读取 app/<APP_NAME>/app_cfg.h 决定开发板，读取 CubeMX 生成的变量作为 HAL 配置
#
# 使用方法：
#   cmake --preset default                  # 配置（一次即可，app 列表自动发现）
#   cmake --build --preset Debug            # 编译当前 app 的 Debug
#   cmake --build --preset Release          # 编译当前 app 的 Release
#   cmake --build --preset Debug --target download_dap   # DAPlink 下载
#
# 切换应用：修改 user_cfg.h 中的 APP_NAME，下次构建自动 reconfigure
#           其他应用已编译的产物保留，切回时无需重编
# 切换开发板：修改 app/<APP_NAME>/app_cfg.h 中的 DEVELOPMENT_BOARD
# 切换 CubeMX 配置：修改 app/<APP_NAME>/app_cfg.h 中的 HAL_CONFIG_NAME
# 构建类型：由 build preset / --config 决定（Debug 或 Release），不再写在头文件里
#
# 调试入口：当前 app 的 Debug 产物会被复制到 build/active/Debug/，
#           launch.json 固定指向该目录（BetaRobot.elf / device.svd / openocd_*.cfg）
##########################################################################################################################

cmake_minimum_required(VERSION 3.23)

project(BetaRobot VERSION 1.0.0 LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

######################################
# 辅助函数
######################################
# 检测宏是否定义（开关型宏，不需要值）
function(check_define output file_name define_name)
    file(STRINGS "${file_name}" define_line REGEX "^[ \t]*#[ \t]*define[ \t]+${define_name}([ \t]|$)")
    if(define_line)
        set(${output} ON PARENT_SCOPE)
    else()
        set(${output} OFF PARENT_SCOPE)
    endif()
endfunction()

# 从头文件读取宏定义值（需要具体值的宏）
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

# 导入模块（include 子 CMakeLists.txt 并添加路径前缀）
# 说明：模块 CMakeLists.txt 里的 set(MODULE_SOURCES ...) 落在本函数作用域内，
#       函数返回即消失，因此可被 build_app() 对多个 app 反复调用而不串扰。
function(import_module module_dir result_sources result_includes)
    unset(MODULE_SOURCES)
    unset(MODULE_INCLUDE_DIRS)
    include("${module_dir}/CMakeLists.txt")
    prefix_paths(${result_sources} "${module_dir}" ${MODULE_SOURCES})
    prefix_paths(${result_includes} "${module_dir}" ${MODULE_INCLUDE_DIRS})
    set(${result_sources} ${${result_sources}} PARENT_SCOPE)
    set(${result_includes} ${${result_includes}} PARENT_SCOPE)
endfunction()

######################################
# 开发板配置表（数据驱动）
######################################
# 开发板配置表: CPU;FPU;链接脚本（使用 cubemx/board_config）
set(BOARD_CFG_DM_MC02      "cortex-m7;fpv5-d16;STM32H723XG_FLASH.ld")
set(BOARD_CFG_DJI_A        "cortex-m4;fpv4-sp-d16;STM32F427XX_FLASH.ld")
set(BOARD_CFG_DJI_C        "cortex-m4;fpv4-sp-d16;STM32F407XX_FLASH.ld")

include("${CMAKE_SOURCE_DIR}/cmake/extract-cubemx-vars.cmake")

######################################
# 单个 app 的构建定义
######################################
function(build_app app_name)
    set(app_dir "${CMAKE_SOURCE_DIR}/app/${app_name}")

    if(NOT EXISTS "${app_dir}/app_cfg.h")
        message(FATAL_ERROR "app/${app_name}/app_cfg.h not found")
    endif()

    # ---- 读取该 app 的开发板 / HAL 配置 ----
    read_define(app_hal   "${app_dir}/app_cfg.h" HAL_CONFIG_NAME)
    read_define(app_board "${app_dir}/app_cfg.h" DEVELOPMENT_BOARD)
    check_define(app_disas   "${app_dir}/app_cfg.h" GENERATE_DISASSEMBLY)
    check_define(app_readelf "${app_dir}/app_cfg.h" GENERATE_READELF)

    if(NOT DEFINED BOARD_CFG_${app_board})
        message(FATAL_ERROR "Unknown DEVELOPMENT_BOARD '${app_board}' in app/${app_name}/app_cfg.h")
    endif()
    list(GET BOARD_CFG_${app_board} 0 MCU_CPU)
    list(GET BOARD_CFG_${app_board} 1 MCU_FPU)
    list(GET BOARD_CFG_${app_board} 2 LD_SCRIPT)
    set(MCU_FLAGS -mcpu=${MCU_CPU} -mthumb -mfpu=${MCU_FPU} -mfloat-abi=hard)

    set(board_dir "${CMAKE_SOURCE_DIR}/cubemx/board_config/${app_board}")
    set(LINKER_SCRIPT "${board_dir}/${LD_SCRIPT}")
    if(NOT EXISTS "${LINKER_SCRIPT}")
        message(FATAL_ERROR "Linker script not found: ${LINKER_SCRIPT}")
    endif()

    set(hal_dir "${CMAKE_SOURCE_DIR}/cubemx/${app_hal}")
    if(NOT EXISTS "${hal_dir}/cmake/stm32cubemx/CMakeLists.txt")
        message(FATAL_ERROR
            "HAL_CONFIG_NAME '${app_hal}' → CMake not found:\n"
            "  ${hal_dir}/cmake/stm32cubemx/CMakeLists.txt\n"
            "  Possible reasons:\n"
            "  1. CubeMX config was not committed: need cubemx/${app_hal}/\n"
            "  2. Typo in app/${app_name}/app_cfg.h HAL_CONFIG_NAME")
    endif()

    # ---- 读取 CubeMX 生成的变量（每个 app 独立提取，互不覆盖）----
    set(gen_dir "${CMAKE_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${gen_dir}")
    load_cubemx_vars(
        "${hal_dir}/cmake/stm32cubemx/CMakeLists.txt"
        "${hal_dir}"
        "${gen_dir}/cubemx_vars_${app_hal}.cmake")

    normalize_paths(HAL_DRIVER_SOURCES ${STM32_Drivers_Src})
    normalize_paths(HAL_USB_LIBRARY_SOURCES ${USB_Device_Library_Src})
    normalize_paths(HAL_FREERTOS_SOURCES ${FreeRTOS_Src})
    normalize_paths(HAL_INCLUDE_PATHS ${MX_Include_Dirs})

    set(HAL_C_SOURCES
        ${HAL_DRIVER_SOURCES}
        ${HAL_USB_LIBRARY_SOURCES}
        ${HAL_FREERTOS_SOURCES}
    )

    # ---- 导入 app/hal/lib/bsp/drv 模块 ----
    # 注意：hal 模块变量名不能用 HAL_INCLUDE_PATHS / HAL_C_SOURCES（已被 CubeMX 变量占用）
    import_module("${app_dir}"               APP_SOURCE_FILES   APP_INCLUDE_PATHS)
    import_module("${CMAKE_SOURCE_DIR}/lib"  LIB_SOURCE_FILES   LIB_INCLUDE_PATHS)
    import_module("${CMAKE_SOURCE_DIR}/hal"  HAL_MODULE_SOURCES HAL_MODULE_INCLUDE_PATHS)
    import_module("${CMAKE_SOURCE_DIR}/bsp"  BSP_SOURCE_FILES   BSP_INCLUDE_PATHS)
    import_module("${CMAKE_SOURCE_DIR}/drv"  DRV_SOURCE_FILES   DRV_INCLUDE_PATHS)

    # ---- 启动文件使用 board_config 下的，而非 CubeMX 生成的 ----
    set(STARTUP_FILE)
    foreach(src IN LISTS MX_Application_Src)
        get_filename_component(abs_src "${src}" ABSOLUTE)
        if(abs_src MATCHES "[/\\\\]startup_([^/\\\\]+)\\.s$")
            set(STARTUP_FILE "${board_dir}/startup_${CMAKE_MATCH_1}.s")
        else()
            list(APPEND HAL_C_SOURCES "${abs_src}")
        endif()
    endforeach()
    if(NOT STARTUP_FILE OR NOT EXISTS "${STARTUP_FILE}")
        message(FATAL_ERROR "Startup file not found for board ${app_board}: ${STARTUP_FILE}")
    endif()

    # ---- HAL 宏定义 ----
    set(HAL_DEFINES)
    foreach(def IN LISTS MX_Defines_Syms)
        if(def)
            list(APPEND HAL_DEFINES "${def}")
        endif()
    endforeach()

    # ---- 可执行目标 ----
    # 全部 EXCLUDE_FROM_ALL，仅当前 APP_NAME 在下方被重新纳入 all
    add_executable(${app_name} EXCLUDE_FROM_ALL
        ${HAL_C_SOURCES}
        ${HAL_MODULE_SOURCES}
        ${APP_SOURCE_FILES}
        ${LIB_SOURCE_FILES}
        ${BSP_SOURCE_FILES}
        ${DRV_SOURCE_FILES}
        "${CMAKE_SOURCE_DIR}/bsp/bsp_map/${app_board}/bsp_map.c"
        ${STARTUP_FILE}
    )
    set_target_properties(${app_name} PROPERTIES SUFFIX ".elf")

    target_compile_definitions(${app_name} PRIVATE
        ${HAL_DEFINES}
        DEVELOPMENT_BOARD=${app_board}
    )

    target_include_directories(${app_name} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${APP_INCLUDE_PATHS}
        ${LIB_INCLUDE_PATHS}
        ${HAL_MODULE_INCLUDE_PATHS}
        ${BSP_INCLUDE_PATHS}
        ${DRV_INCLUDE_PATHS}
        ${HAL_INCLUDE_PATHS}
        # bsp_map 路径（各模块通过 #include "bsp_map.h" 引用）
        "${CMAKE_SOURCE_DIR}/bsp/bsp_map/${app_board}"
    )

    target_compile_options(${app_name} PRIVATE
        ${MCU_FLAGS}
        -Wall
        -fdata-sections
        -ffunction-sections
        # 源文件与字符串常量统一 UTF-8：支持中文日志（如 BSPLOG(...,"你好！")），
        # 编译出的字符串是 UTF-8 字节，串口按 UTF-8 接收/显示即可正确呈现
        $<$<COMPILE_LANGUAGE:C>:-finput-charset=UTF-8>
        $<$<COMPILE_LANGUAGE:C>:-fexec-charset=UTF-8>
        $<$<CONFIG:Debug>:-Og>
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-gdwarf-2>
        $<$<CONFIG:Release>:-Os>
        $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>
    )

    target_link_options(${app_name} PRIVATE
        ${MCU_FLAGS}
        -specs=nano.specs
        -T${LINKER_SCRIPT}
        "-Wl,-Map=${CMAKE_BINARY_DIR}/$<CONFIG>/${app_name}.map,--cref"
        -Wl,--gc-sections
        -Wl,--print-memory-usage
    )

    target_link_libraries(${app_name} PRIVATE c m nosys)

    # ---- 构建后生成 hex/bin ----
    add_custom_command(TARGET ${app_name} POST_BUILD
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${app_name}>
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${app_name}>
                "${CMAKE_BINARY_DIR}/$<CONFIG>/${app_name}.hex"
        COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:${app_name}>
                "${CMAKE_BINARY_DIR}/$<CONFIG>/${app_name}.bin"
        VERBATIM
    )

    # ---- 反汇编（GENERATE_DISASSEMBLY）----
    if(app_disas)
        add_custom_command(TARGET ${app_name} POST_BUILD
            COMMAND ${CMAKE_OBJDUMP} -d -S $<TARGET_FILE:${app_name}>
                    > "${CMAKE_BINARY_DIR}/$<CONFIG>/${app_name}.lst"
            COMMENT "Generating disassembly file..."
            VERBATIM
        )
    endif()

    # ---- readelf 输出（GENERATE_READELF）----
    if(app_readelf)
        set(READELF_OUTPUT_DIR "${CMAKE_BINARY_DIR}/readelf/${app_name}/$<CONFIG>")
        set(READELF_ERR "${CMAKE_BINARY_DIR}/$<CONFIG>/${app_name}_readelf_err.txt")

        set(READELF_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E make_directory "${READELF_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E echo "Generating readelf output files..."
            COMMAND ${CMAKE_READELF} -aW $<TARGET_FILE:${app_name}>
                    > "${READELF_OUTPUT_DIR}/${app_name}.elf_all.txt"
                    2> "${READELF_ERR}" || ${CMAKE_COMMAND} -E true
        )
        foreach(dump IN ITEMS
                abbrev addr aranges cu_index decodedline frames frames-interp
                gdb_index info loc macro pubnames pubtypes Ranges rawline
                str str-offsets trace_abbrev trace_aranges trace_info)
            list(APPEND READELF_COMMANDS
                COMMAND ${CMAKE_READELF} --debug-dump=${dump} $<TARGET_FILE:${app_name}>
                        > "${READELF_OUTPUT_DIR}/${app_name}.elf_${dump}.txt"
                        2> "${READELF_ERR}" || ${CMAKE_COMMAND} -E true
            )
        endforeach()
        add_custom_command(TARGET ${app_name} POST_BUILD ${READELF_COMMANDS}
            COMMENT "Generating readelf output files in build/readelf/${app_name}/..."
            VERBATIM
        )
    endif()

    # compile_commands.json 只导出当前 app，避免多个 board 的编译命令互相混淆
    if(app_name STREQUAL APP_NAME)
        set_target_properties(${app_name} PROPERTIES EXPORT_COMPILE_COMMANDS ON)
    else()
        set_target_properties(${app_name} PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
    endif()

    # ---- 配置期依赖：这些文件变化时自动 reconfigure ----
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${app_dir}/app_cfg.h"
        "${app_dir}/CMakeLists.txt"
        "${hal_dir}/cmake/stm32cubemx/CMakeLists.txt"
    )

    if(app_name STREQUAL APP_NAME)
        message(STATUS "  app '${app_name}': board=${app_board}, hal=${app_hal} [active]")
    else()
        message(STATUS "  app '${app_name}': board=${app_board}, hal=${app_hal}")
    endif()
endfunction()

######################################
# 读取当前 APP_NAME（user_cfg.h）
######################################
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/user_cfg.h")
    message(FATAL_ERROR
        "user_cfg.h not found.\n"
        "  Copy user_cfg.h.example to user_cfg.h and set APP_NAME.")
endif()

read_define(APP_NAME "${CMAKE_SOURCE_DIR}/user_cfg.h" APP_NAME)

# APP_NAME 变化时触发 reconfigure，从而翻转 all 的成员
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/user_cfg.h")

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/app/${APP_NAME}/CMakeLists.txt")
    message(FATAL_ERROR
        "App ${APP_NAME} is not initialized: ${CMAKE_SOURCE_DIR}/app/${APP_NAME}\n"
        "  app 是独立 git 仓库（主仓库不追踪）。请先拉取：\n"
        "    bash scripts/setup_apps.sh\n"
        "  或在 app/ 下单独 clone 该 app 仓库到 ${APP_NAME}")
endif()

######################################
# 自动发现 app/ 下的所有应用
######################################
file(GLOB app_dirs LIST_DIRECTORIES true CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/app/*")
set(APP_NAMES "")
foreach(d IN LISTS app_dirs)
    if(IS_DIRECTORY "${d}" AND EXISTS "${d}/CMakeLists.txt")
        get_filename_component(n "${d}" NAME)
        if(n MATCHES "^(all|clean|install|test|help|package)$")
            message(FATAL_ERROR "app 目录名 '${n}' 与 CMake 保留 target 名冲突，请重命名")
        endif()
        list(APPEND APP_NAMES "${n}")
    endif()
endforeach()

if(NOT APP_NAME IN_LIST APP_NAMES)
    message(FATAL_ERROR
        "APP_NAME '${APP_NAME}' is not among discovered apps: ${APP_NAMES}\n"
        "  请检查 user_cfg.h 中的 APP_NAME，或拉取对应 app 仓库。")
endif()

foreach(app IN LISTS APP_NAMES)
    build_app("${app}")
endforeach()

# 当前 app 重新纳入 all（其余保持 EXCLUDE_FROM_ALL，仍可按 target 名显式构建）
set_property(TARGET ${APP_NAME} PROPERTY EXCLUDE_FROM_ALL FALSE)

read_define(ACTIVE_BOARD "${CMAKE_SOURCE_DIR}/app/${APP_NAME}/app_cfg.h" DEVELOPMENT_BOARD)
set(ACTIVE_BOARD_DIR "${CMAKE_SOURCE_DIR}/cubemx/board_config/${ACTIVE_BOARD}")

######################################
# 当前 app 的产物复制到固定调试目录（launch.json 指向这里）
######################################
# 用独立 custom target 而非 POST_BUILD：POST_BUILD 只在 target 实际重建时执行，
# 切回已编译过的 app 时不会重建，active/ 会停留在上一个 app 的产物（F5 调试到错误的固件）。
# custom target 无输出文件，每次构建都会执行；copy_if_different 保证内容未变时不写入。
add_custom_target(active_artifacts ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/active/$<CONFIG>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${APP_NAME}>
            "${CMAKE_BINARY_DIR}/active/$<CONFIG>/BetaRobot.elf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ACTIVE_BOARD_DIR}/${ACTIVE_BOARD}.svd"
            "${CMAKE_BINARY_DIR}/active/$<CONFIG>/device.svd"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ACTIVE_BOARD_DIR}/openocd_daplink.cfg"
            "${CMAKE_BINARY_DIR}/active/$<CONFIG>/openocd_daplink.cfg"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ACTIVE_BOARD_DIR}/openocd_rtt.cfg"
            "${CMAKE_BINARY_DIR}/active/$<CONFIG>/openocd_rtt.cfg"
    DEPENDS ${APP_NAME}
    COMMENT "Copying active app '${APP_NAME}' artifacts to build/active/$<CONFIG>/"
    VERBATIM
)

######################################
# 平台检测和工具适配
######################################
find_program(OPENOCD_PROGRAM openocd)

######################################
# 下载目标（仅 DAPlink）
######################################
if(OPENOCD_PROGRAM)
    add_custom_target(download_dap
        COMMAND "${OPENOCD_PROGRAM}"
                -f "${CMAKE_SOURCE_DIR}/cubemx/board_config/${ACTIVE_BOARD}/openocd_daplink.cfg"
                -c "program $<TARGET_FILE:${APP_NAME}> verify reset exit"
        DEPENDS ${APP_NAME}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Downloading active app '${APP_NAME}' via DAP-Link..."
        VERBATIM
    )
else()
    add_custom_target(download_dap
        COMMAND ${CMAKE_COMMAND} -E echo "OpenOCD not found. Please install OpenOCD and add it to PATH."
        DEPENDS ${APP_NAME}
    )
endif()

######################################
# 打印配置信息
######################################
message(STATUS "APP_NAME (active): ${APP_NAME}")
message(STATUS "DEVELOPMENT_BOARD: ${ACTIVE_BOARD}")
message(STATUS "CONFIGURATION_TYPES: ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "GENERATOR: ${CMAKE_GENERATOR}")
