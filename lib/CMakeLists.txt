# lib 层源文件（硬件无关的算法/协议模块）
# 卡尔曼族统一挂在 lib_kf/ 下：lib_kf_core.h 是三者共用的代数内核，
# lib_lkf（线性 KF）/ lib_ekf（非线性 EKF）/ lib_eskf（误差状态 ESKF）为同级子模块。
set(MODULE_SOURCES
    lib_crc/lib_crc.c
    lib_crc/lib_crc_tables.c
    lib_format/lib_format.c
    lib_hamming/lib_hamming.c
    lib_hamming/lib_hamming_ext.c
    lib_kf/lib_ekf/lib_ekf.c
    lib_kf/lib_eskf/lib_eskf.c
    lib_kf/lib_lkf/lib_lkf.c
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
    lib_hamming
    lib_kf
    lib_kf/lib_ekf
    lib_kf/lib_eskf
    lib_kf/lib_lkf
    lib_mahony
    lib_math
    lib_mit
    lib_pid
)
