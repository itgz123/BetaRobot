# drvlib 层源文件（器件 + 算法联合：通信、标定、滤波一体）
set(MODULE_SOURCES
    drvlib_bmi088_kalman/drvlib_bmi088_kalman.c
    drvlib_motor/drvlib_motor.c
)

# drvlib 层头文件路径
#   drvlib_bmi088_calib / drvlib_ist8310_calib 只有头文件（共用的标定结构体与修正
#   函数），故只出现在这里
set(MODULE_INCLUDE_DIRS
    drvlib_bmi088_calib
    drvlib_bmi088_kalman
    drvlib_motor
)
