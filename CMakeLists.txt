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

# CXX：目前只有 lib_tflm（TFLM 是 C++ 库）用得到。启用它不影响其它 app ——
# 没有 .cc 源的目标链接语言仍是 C，编译选项也按语言分别加（见下方 CXX 段落）。
project(BetaRobot VERSION 1.0.0 LANGUAGES C CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# C：C23。JSF AV Rule 8 要求"符合 ISO 标准"，所以不要 GNU 扩展。选 C23 而不是
#    C11 的原因：项目既定的 `typedef enum : uintN_t` 写法与 0b 二进制字面量在 C23
#    都是标准的一部分，于是 -Wpedantic 下不再有 1000+ 条
#    "ISO C does not support specifying ... before C23" 的告警（实测 1058 → 6）。
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# C++：TFLM 要求 C++17。JSF 草案里写的 -std=c++14 在这里用不了 ——
#      lib/lib_tflm/lib_tflm_port.cc 会 include TFLM 的 C++17 头。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
# JSF AV C++ 严格编译配置
######################################
# 依据：Joint Strike Fighter Air Vehicle C++ Coding Standards
#       (2RDU00001 Rev C, 2005)。与编译器开关相关的条款及对应关系：
#
#   AV Rule 208  "C++ exceptions shall not be used."   → -fno-exceptions（见 target_compile_options）
#   AV Rule 178  限制向下转型                            → -fno-rtti
#   AV Rule 218  "Compiler warning levels will be set in
#                 compliance with project policies."     → 等级由本项目定，见 JSF_AV_STRICT_WARNINGS
#   AV Rule 8    "All code shall conform to ISO/IEC 14882:2002(E)"
#                → 只要求"符合 ISO 标准"、不指定具体版本；本项目 C 用 C23、C++ 用 C++17
#   AV Rule 119  "Functions shall not call themselves (直接或间接递归)"
#                → 无对应编译器开关；用 -fstack-usage 的 .su + 调用图脚本判环，
#                  见 scripts/jsf-av/check_recursion.py（经函数指针的间接调用是盲区）
#   AV Rule 206  "Allocation/deallocation from/to the free store shall not occur
#                 after initialization."
#                → 无对应开关，用链接后 nm 断言，见 cmake/check_no_heap.cmake
#   AV Rule 15   运行时防御性检查 → -fstack-protector-strong（见下）
#                （注：-fsanitize=undefined 在本工具链不可用，原因见 MyCMakeLists.md）
#
# **与标准的偏离逐条记在根目录 MyCMakeLists.md**，改这里时请同步那份文档。
######################################

# 一方代码（app / lib / bsp / drv / drvlib + hal 模块）的严格警告集。
# 作用范围说明：cubemx/ 下的 HAL / CMSIS / FreeRTOS / USB 与 TFLM 是第三方。
# JSF 的前提之一就是不改第三方代码，且实测这些文件会产生 86 条无法就地修的告警
# （-Wconversion / -Wunused-parameter 为主），加 -Werror 会被它们直接挡死构建。
set(JSF_AV_STRICT_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
)

# 把一方代码的告警当错误（AV Rule 218 的"由项目策略定"落成最严一档）。
# 关掉它（-DJSF_AV_WERROR=OFF）只是降级为"照常告警"，其余严格度不受影响。
option(JSF_AV_WERROR          "一方代码的 JSF 告警当错误（-Werror）"          ON)
# GCC 栈保护。工具链没有 libssp，运行期两个符号由 bsp/bsp_stack_guard/ 自备，
# 链接期用 cmake/no-ssp-libs.specs 去掉 -lssp。
option(JSF_AV_STACK_PROTECTOR "开启 -fstack-protector-strong（含自备运行期）" ON)
# 每个编译单元生成 .su 栈用量文件，供 AV Rule 119（禁递归）/ 任务栈尺寸核对使用。
option(JSF_AV_STACK_USAGE     "开启 -fstack-usage 生成 .su 栈用量"            ON)
# 链接后断言固件不引用堆（AV Rule 206）。
option(JSF_AV_NO_HEAP         "链接后断言固件不引用 malloc/free/_sbrk"        ON)

if(JSF_AV_WERROR)
    list(APPEND JSF_AV_STRICT_WARNINGS -Werror)
endif()

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

    # ---- 导入 app/hal/lib/bsp/drv/drvlib 模块 ----
    # 注意：hal 模块变量名不能用 HAL_INCLUDE_PATHS / HAL_C_SOURCES（已被 CubeMX 变量占用）
    # MODULE_COMPILE_DEFINITIONS：模块自报的编译宏（如 lib_tflm 需要 -DCMSIS_NN）。
    # 模块用 set(... PARENT_SCOPE) 写进来 —— 它被 import_module() include，父作用域
    # 正是本函数，于是不必动 import_module() 的签名。
    set(MODULE_COMPILE_DEFINITIONS)
    import_module("${app_dir}"               APP_SOURCE_FILES   APP_INCLUDE_PATHS)
    import_module("${CMAKE_SOURCE_DIR}/lib"  LIB_SOURCE_FILES   LIB_INCLUDE_PATHS)
    import_module("${CMAKE_SOURCE_DIR}/hal"  HAL_MODULE_SOURCES HAL_MODULE_INCLUDE_PATHS)
    import_module("${CMAKE_SOURCE_DIR}/bsp"  BSP_SOURCE_FILES   BSP_INCLUDE_PATHS)
    import_module("${CMAKE_SOURCE_DIR}/drv"  DRV_SOURCE_FILES   DRV_INCLUDE_PATHS)
    import_module("${CMAKE_SOURCE_DIR}/drvlib" DRVLIB_SOURCE_FILES DRVLIB_INCLUDE_PATHS)

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
        ${DRVLIB_SOURCE_FILES}
        "${CMAKE_SOURCE_DIR}/bsp/bsp_map/${app_board}/bsp_map.c"
        ${STARTUP_FILE}
    )
    set_target_properties(${app_name} PROPERTIES SUFFIX ".elf")

    target_compile_definitions(${app_name} PRIVATE
        ${HAL_DEFINES}
        DEVELOPMENT_BOARD=${app_board}
        ${MODULE_COMPILE_DEFINITIONS}
        # 让 bsp/bsp_stack_guard/bsp_stack_guard.c 与 -fstack-protector-strong
        # 同生共死：开了保护才有运行期符号，关了就不占那 4 字节 .data。
        $<$<BOOL:${JSF_AV_STACK_PROTECTOR}>:JSF_AV_STACK_PROTECTOR>
    )

    target_include_directories(${app_name} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${APP_INCLUDE_PATHS}
        ${LIB_INCLUDE_PATHS}
        ${HAL_MODULE_INCLUDE_PATHS}
        ${BSP_INCLUDE_PATHS}
        ${DRV_INCLUDE_PATHS}
        ${DRVLIB_INCLUDE_PATHS}
        # bsp_map 路径（各模块通过 #include "bsp_map.h" 引用）
        "${CMAKE_SOURCE_DIR}/bsp/bsp_map/${app_board}"
    )

    # 第三方头文件（CubeMX 的 HAL / CMSIS / FreeRTOS / USB）走 -isystem。
    # 这是 D2「第三方豁免」的实现补丁：只豁免第三方的**编译单元**是不够的——
    # 第三方的**头文件**会被一方 .c 拉进来，照样吃 -Wconversion / -Wsign-conversion。
    # 实测 DJI_C(F4) 的 core_cm4.h 干净所以没暴露，DM_MC02(H7) 的 core_cm7.h
    # 在严格开关下产出 1600+ 条，且我们改不了（CubeMX 重新生成会覆盖）。
    # -isystem 让 GCC 不从这里报诊断（等价于把第三方头当系统头）。
    target_include_directories(${app_name} SYSTEM PRIVATE
        ${HAL_INCLUDE_PATHS}
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
        # C++ 源（lib_tflm 的 TFLM）：嵌入式下关掉 RTTI / 异常 / 线程安全静态量，
        # 与上游 tools/make/Makefile 的 CXXFLAGS 一致 —— 不关会引入 libstdc++ 的
        # __cxa_* 与 typeinfo，白白撑大固件
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-unwind-tables>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-asynchronous-unwind-tables>
        $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
        # ---- JSF AV ----
        # 栈保护对全 target 生效（含 cubemx/ 的 HAL：USB 那堆栈缓冲同样值得护）。
        # 它只增加检查代码、不引入编译期严格性问题，所以不受"只加一方代码"的限制。
        # 运行期符号见 bsp/bsp_stack_guard/，链接期见 cmake/no-ssp-libs.specs。
        $<$<BOOL:${JSF_AV_STACK_PROTECTOR}>:-fstack-protector-strong>
        # 栈用量是"测量"不是"规则"，同样全 target 生成（HAL / FreeRTOS 的栈用量
        # 对定任务栈尺寸、审 AV Rule 119 的深调用链都直接有用）。
        $<$<BOOL:${JSF_AV_STACK_USAGE}>:-fstack-usage>
        $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>
    )

    # ---- JSF AV：严格警告只加在**一方源文件**上 ----
    # 用源文件级 COMPILE_OPTIONS 而不是 target_compile_options，是因为本工程的
    # 第三方（cubemx/ 的 HAL/CMSIS/FreeRTOS/USB、TFLM）与一方代码在同一个 target 里，
    # 分不开。源文件级属性会追加在 target 级选项之后，只影响列出来的这些文件。
    #
    # 这些开关与语言无关（C / C++ 都适用），所以不需要 genex。
    set(jsf_av_first_party_sources
        ${APP_SOURCE_FILES}
        ${LIB_SOURCE_FILES}
        ${BSP_SOURCE_FILES}
        ${DRV_SOURCE_FILES}
        ${DRVLIB_SOURCE_FILES}
        ${HAL_MODULE_SOURCES}
        # bsp_map.c 是直接以绝对路径加进来的（见上方 add_executable），单独列出
        "${CMAKE_SOURCE_DIR}/bsp/bsp_map/${app_board}/bsp_map.c"
    )
    set_source_files_properties(${jsf_av_first_party_sources}
        PROPERTIES COMPILE_OPTIONS "${JSF_AV_STRICT_WARNINGS}")

    target_link_options(${app_name} PRIVATE
        ${MCU_FLAGS}
        -specs=nano.specs
        # 去掉 -fstack-protector-* 自动追加的 -lssp / -lssp_nonshared（工具链没有
        # libssp），运行期两个符号由 bsp/bsp_stack_guard/ 提供。文件内有详细说明。
        $<$<BOOL:${JSF_AV_STACK_PROTECTOR}>:-specs=${CMAKE_SOURCE_DIR}/cmake/no-ssp-libs.specs>
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

    # ---- JSF AV Rule 206：链接后断言固件不引用堆 ----
    # 放在产物生成之后：这时 --gc-sections 已经跑完，nm 扫到的就是真实引用，
    # 而不是"源码里出现过"。脚本本身的说明见 cmake/check_no_heap.cmake。
    if(JSF_AV_NO_HEAP)
        add_custom_command(TARGET ${app_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                    "-DNM=${CMAKE_NM}"
                    "-DELF=$<TARGET_FILE:${app_name}>"
                    -P "${CMAKE_SOURCE_DIR}/cmake/check_no_heap.cmake"
            COMMENT "Checking JSF AV Rule 206 (no heap allocation)..."
            VERBATIM
        )
    endif()

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

        # ---- 强制每次构建都重新链接 ----
        # -Wl,--print-memory-usage 的内存占用表只在链接步骤执行时打印，而工具链没有别的命令
        # 能给出 region 百分比（size/objdump 只有段的地址和大小）。增量构建（无源文件改动、
        # 或只改 app_cfg.h 触发 reconfigure）不重链就看不到它。
        # 挂一个每次都会重建的空文件到 LINK_DEPENDS，强制重链：实测重链 + 反汇编 +
        # 21 个 readelf dump 一起重跑约 0.4s，换取内存占用每次构建可见。
        set(relink_stamp "${CMAKE_BINARY_DIR}/$<CONFIG>/${app_name}_relink.stamp")
        add_custom_command(
            OUTPUT "${relink_stamp}"
            COMMAND ${CMAKE_COMMAND} -E touch "${relink_stamp}"
            VERBATIM
        )
        # 上面这条 custom_command 只负责让 ninja 认得 stamp 这个文件：没有 target 依赖这个
        # OUTPUT 时 CMake 不把它写进构建图，ninja 会报 "missing and no known rule to make it"。
        add_custom_target(${app_name}_relink_stamp DEPENDS "${relink_stamp}")
        set_target_properties(${app_name} PROPERTIES LINK_DEPENDS "${relink_stamp}")

        # 链接完成后把 stamp 顶新，下次构建必然重链。
        # 注意不能放在链接之前 touch：那样链接产出的 elf 比 stamp 新，重链会变成隔一次才发生一次。
        add_custom_command(TARGET ${app_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E touch "${relink_stamp}"
            VERBATIM
        )
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
        "    bash scripts/setup_project/setup_project.sh\n"
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
# 打印配置信息（active app）
######################################
# 重新读取 active app 的开关：build_app() 里的同名变量是函数作用域，这里取不到
read_define(ACTIVE_HAL      "${CMAKE_SOURCE_DIR}/app/${APP_NAME}/app_cfg.h" HAL_CONFIG_NAME)
check_define(ACTIVE_DISAS   "${CMAKE_SOURCE_DIR}/app/${APP_NAME}/app_cfg.h" GENERATE_DISASSEMBLY)
check_define(ACTIVE_READELF "${CMAKE_SOURCE_DIR}/app/${APP_NAME}/app_cfg.h" GENERATE_READELF)

message(STATUS "APP_NAME (active): ${APP_NAME}")
message(STATUS "DEVELOPMENT_BOARD: ${ACTIVE_BOARD}")
message(STATUS "HAL_CONFIG_NAME: ${ACTIVE_HAL}")
message(STATUS "HAL_DIR: ${CMAKE_SOURCE_DIR}/cubemx/${ACTIVE_HAL}")
message(STATUS "BUILD_TYPE: ${CMAKE_DEFAULT_BUILD_TYPE} (默认配置，可由 --config 覆盖)")
message(STATUS "GENERATE_DISASSEMBLY: ${ACTIVE_DISAS}")
message(STATUS "GENERATE_READELF: ${ACTIVE_READELF}")
message(STATUS "CONFIGURATION_TYPES: ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "GENERATOR: ${CMAKE_GENERATOR}")
