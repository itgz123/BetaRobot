# lib 层源文件（硬件无关的算法/协议模块）
set(MODULE_SOURCES
    lib_crc/lib_crc.c
    lib_crc/lib_crc_tables.c
    lib_format/lib_format.c
    lib_mahony/lib_mahony.c
    lib_math/lib_math.c
    lib_math/lib_math_trig_lut.c
    lib_mit/lib_mit.c
    lib_pid/lib_pid.c
)

# lib 层头文件路径
set(MODULE_INCLUDE_DIRS
    .
    lib_crc
    lib_dh_matrix
    lib_format
    lib_mahony
    lib_math
    lib_mit
    lib_pid
)
