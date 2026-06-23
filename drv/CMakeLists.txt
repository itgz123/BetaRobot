# drv 层源文件
set(MODULE_SOURCES
    drv_pid/drv_pid.c
    drv_chassis/drv_chassis.c
    drv_sbus/drv_sbus.c
    drv_bmi088/drv_bmi088.c
    drv_daemon/drv_daemon.c
    drv_vofa/drv_vofa.c
    drv_motor/drv_djimotor/drv_djimotor.c
    drv_axis_lite/drv_axis_mit_lite.c
    drv_mit/drv_mit.c
)

# drv 层头文件路径
set(MODULE_INCLUDE_DIRS
    drv_bmi088
    drv_chassis
    drv_pid
    drv_sbus
    drv_daemon
    drv_vofa
    drv_motor
    drv_motor/drv_djimotor
    drv_axis_lite
    drv_mit
)
