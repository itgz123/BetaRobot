# drv 层源文件
set(DRV_SOURCES
    drv_acmotor/drv_djimotor/drv_djimotor.c
    # drv_acmotor/drv_dmmotor/drv_dmmotor.c
    drv_pid/drv_pid.c
    drv_chassis/drv_chassis.c
    drv_sbus/drv_sbus.c
    drv_bmi088/drv_bmi088.c
    drv_acmotor/drv_acmotor_base.c
    drv_daemon/drv_daemon.c
    drv_vofa/drv_vofa.c
    drv_vofa/drv_vofa_lite.c
)

# drv 层头文件路径
set(DRV_INCLUDE_DIRS
    .
    drv_bmi088
    drv_chassis
    drv_acmotor
    drv_acmotor/drv_djimotor
    # drv_acmotor/drv_dmmotor
    drv_pid
    drv_sbus
    drv_daemon
    drv_vofa
    drv_dcmotor
)
