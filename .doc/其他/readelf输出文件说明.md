# readelf 工具输出文件说明

开启 `GENERATE_READELF` 宏后，编译会在 `build/elf/` 目录下生成以下文件。

## 文件列表

| readelf 选项                 | 文件名                            | 说明                                                                                                                  |
| ---------------------------- | --------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| `-aW`                        | `BetaRobot.elf_all.txt`           | 完整的 ELF 文件信息，包含 ELF 头、程序头、节头、符号表、重定位表等所有非调试信息                                      |
| `--debug-dump=abbrev`        | `BetaRobot.elf_abbrev.txt`        | DWARF 缩写表，定义了调试信息条目（DIE）的缩写形式，用于压缩调试信息                                                   |
| `--debug-dump=addr`          | `BetaRobot.elf_addr.txt`          | DWARF 地址表，存储地址到符号的映射信息，用于加速调试器查找                                                            |
| `--debug-dump=aranges`       | `BetaRobot.elf_aranges.txt`       | DWARF 地址范围表，记录每个编译单元的代码地址范围，用于快速定位函数所属的编译单元                                      |
| `--debug-dump=cu_index`      | `BetaRobot.elf_cu_index.txt`      | 编译单元索引，用于加速在大型程序的 DWARF 信息中查找特定编译单元                                                       |
| `--debug-dump=decodedline`   | `BetaRobot.elf_decodedline.txt`   | 解码后的行号信息，包含源文件名、行号与机器地址的完整映射，用于源码级调试                                              |
| `--debug-dump=frames`        | `BetaRobot.elf_frames.txt`        | 调用帧信息（CFI），记录函数栈帧布局、寄存器保存位置，用于回溯调用栈和异常处理                                         |
| `--debug-dump=frames-interp` | `BetaRobot.elf_frames-interp.txt` | 解释后的帧信息，将原始 CFI 数据转换为更易读的格式，显示每个指令位置的栈帧状态                                         |
| `--debug-dump=gdb_index`     | `BetaRobot.elf_gdb_index.txt`     | GDB 索引，预计算的调试索引，加速 GDB 启动时的符号加载                                                                 |
| `--debug-dump=info`          | `BetaRobot.elf_info.txt`          | **核心 DWARF 调试信息**，包含：变量和函数的定义与类型、结构体/联合体/枚举的布局与字段偏移、作用域信息、源文件位置映射 |
| `--debug-dump=loc`           | `BetaRobot.elf_loc.txt`           | 位置列表，描述变量在不同代码位置的存储位置（寄存器或栈槽），因为变量可能在不同位置存活                                |
| `--debug-dump=macro`         | `BetaRobot.elf_macro.txt`         | 宏定义信息，记录源码中 `#define` 定义的宏，允许调试器展开宏                                                           |
| `--debug-dump=pubnames`      | `BetaRobot.elf_pubnames.txt`      | 公开名称表，列出所有全局可见的函数和变量名称，用于快速符号查找                                                        |
| `--debug-dump=pubtypes`      | `BetaRobot.elf_pubtypes.txt`      | 公开类型表，列出所有全局可见的类型定义（结构体、枚举等），用于快速类型查找                                            |
| `--debug-dump=Ranges`        | `BetaRobot.elf_Ranges.txt`        | 地址范围列表，记录函数、变量等程序实体占用的内存地址范围，支持非连续地址范围                                          |
| `--debug-dump=rawline`       | `BetaRobot.elf_rawline.txt`       | 原始行号信息，直接从 `.debug_line` 节解码的数据，包含行号程序的原始状态机指令                                         |
| `--debug-dump=str`           | `BetaRobot.elf_str.txt`           | DWARF 字符串表，存储调试信息中使用的所有字符串（文件名、变量名、类型名等）                                            |
| `--debug-dump=str-offsets`   | `BetaRobot.elf_str-offsets.txt`   | 字符串偏移表，记录字符串在字符串表中的偏移量，用于 DWARF 5 格式的字符串引用                                           |
| `--debug-dump=trace_abbrev`  | `BetaRobot.elf_trace_abbrev.txt`  | 追踪缩写表，用于 CTF（Compile Time Format）追踪功能的缩写定义                                                         |
| `--debug-dump=trace_aranges` | `BetaRobot.elf_trace_aranges.txt` | 追踪地址范围，用于追踪功能的地址范围映射                                                                              |
| `--debug-dump=trace_info`    | `BetaRobot.elf_trace_info.txt`    | 追踪信息，记录程序的追踪点信息，用于性能分析和代码覆盖率                                                              |

## 常用文件说明

### `BetaRobot.elf_info.txt` - 结构体布局查看

这是最常用的文件，包含完整的 DWARF 调试信息。查看结构体字段偏移的方法：

1. 搜索 `DW_TAG_structure_type` 找到结构体定义
2. 每个 `DW_TAG_member` 表示一个字段
3. `DW_AT_data_member_location` 表示字段偏移量

示例：
```
 <1><0x1234>: Abbrev Number: 5 (DW_TAG_structure_type)
    <0x1235>   DW_AT_name        : my_struct
    ...
 <2><0x1250>: Abbrev Number: 7 (DW_TAG_member)
    <0x1251>   DW_AT_name        : field_a
    <0x1255>   DW_AT_type        : <0x2000>
    <0x1259>   DW_AT_data_member_location: 0   // 偏移 0 字节
 <2><0x1260>: Abbrev Number: 7 (DW_TAG_member)
    <0x1261>   DW_AT_name        : field_b
    <0x1265>   DW_AT_type        : <0x2010>
    <0x1269>   DW_AT_data_member_location: 4   // 偏移 4 字节
```

### `BetaRobot.elf_frames.txt` - 栈帧分析

用于分析函数调用栈，包含：
- 函数入口的栈指针调整
- 被调用者保存寄存器的存储位置
- 返回地址的保存位置

### `BetaRobot.elf_decodedline.txt` - 地址映射

用于将机器地址映射回源码位置：
```
File name                            Line number    Starting address
src/main.c                                   10              0x8001234
src/main.c                                   11              0x8001238
```

## 使用配置

在 `app/<APP_NAME>/app_cfg.h` 中开启：

```c
// 编译后是否生成readelf输出文件
// 注释此行则不生成
#define GENERATE_READELF
```
