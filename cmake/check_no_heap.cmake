##########################################################################################
# 链接后断言：固件不得引用堆分配（JSF AV Rule 206）
#
#   AV Rule 206: "Allocation/deallocation from/to the free store (heap) shall not occur
#                 after initialization."
#
# JSF 没有提供任何编译器开关能表达这条，所以在链接完成后用 nm 扫一遍固件：
# 只要出现 malloc / free / _sbrk 一类的符号就判失败。配合链接期 --gc-sections，
# 未被引用的分配函数会被回收，因此"能扫到"就等于"确实被引用了"。
#
# 比 -Wl,--wrap=malloc 好在哪：--wrap 只能拦**本工程自己**写的 malloc 调用，
# 拦不住 newlib 内部（printf → _malloc_r）——那条路才是嵌入式里最常见的泄漏堆的
# 来源。本脚本看的是最终产物，两边都覆盖。
#
# 用法（顶层 CMakeLists.txt 的 POST_BUILD 调用）：
#   cmake -DNM=<nm 路径> -DELF=<固件 elf> -P check_no_heap.cmake
##########################################################################################

if(NOT NM OR NOT ELF)
    message(FATAL_ERROR "check_no_heap.cmake 需要 -DNM=<nm> -DELF=<elf>")
endif()

if(NOT EXISTS "${ELF}")
    message(FATAL_ERROR "check_no_heap.cmake: 固件不存在: ${ELF}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only "${ELF}"
    OUTPUT_VARIABLE nm_output
    ERROR_VARIABLE nm_error
    RESULT_VARIABLE nm_result)

if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "check_no_heap.cmake: nm 执行失败: ${nm_error}")
endif()

# 堆分配相关符号（newlib 的 _*_r 变体与 _sbrk 也要拦：它们才是真正落地的那个）
set(HEAP_SYMBOLS
    malloc free calloc realloc
    _malloc_r _free_r _calloc_r _realloc_r
    _sbrk _sbrk_r
    __wrap_malloc __wrap_free)

set(violations "")
foreach(sym IN LISTS HEAP_SYMBOLS)
    if(nm_output MATCHES "(^|\n)[0-9a-fA-F]+[ \t]+[A-Za-z][ \t]+${sym}(\n|$)")
        list(APPEND violations "${sym}")
    endif()
endforeach()

if(violations)
    string(REPLACE ";" ", " violations_str "${violations}")
    message(FATAL_ERROR
        "JSF AV Rule 206 违规：固件引用了堆分配符号 [${violations_str}]\n"
        "  固件: ${ELF}\n"
        "  AV Rule 206 禁止初始化之后从堆分配/释放。请改成静态分配，"
        "或确认确实需要堆后在 CMakeLists.txt 里关掉 JSF_AV_NO_HEAP。")
endif()
