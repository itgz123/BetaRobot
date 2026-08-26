# drv 层源文件
set(MODULE_SOURCES
    drv_pid/drv_pid.c
    drv_chassis/drv_chassis_position_lite.c
    drv_sbus/drv_sbus.c
    drv_dbus/drv_dbus.c
    drv_bmi088/drv_bmi088.c
    drv_bmi088/drv_bmi088_heater.c
    drv_daemon/drv_daemon.c
    drv_mahony/drv_mahony.c
    drv_vofa/drv_vofa.c
    drv_motor/drv_djimotor/drv_djimotor.c
    drv_motor/drv_dmmotor/drv_dmmotor.c
    drv_motor/drv_rsmotor/drv_rsmotor.c
    drv_axis_lite/drv_axis_mit_lite.c
    drv_planner/drv_planner.c
    drv_mit/drv_mit.c
    # comm开始
    drv_comm/drv_comm.c
    drv_comm/media/comm_media.c
    drv_comm/media/comm_media_usart.c
    drv_comm/media/comm_media_usb.c
    drv_comm/media/comm_media_usb_simple.c
    # FIXME(bsp_can 枚举合并重构期间临时注释，恢复后需改 tx_id_type/rx_id_type → tx_frame_type/rx_frame_type)
    # drv_comm/media/comm_media_can_pkt0.c
    # drv_comm/media/comm_media_can_idseq.c
    drv_comm/proto/comm_proto.c
    drv_comm/proto/comm_proto_raw.c
    drv_comm/proto/comm_proto_custom.c
    # comm结束
)

# drv 层头文件路径
set(MODULE_INCLUDE_DIRS
    drv_bmi088
    drv_chassis
    drv_pid
    drv_sbus
    drv_dbus
    drv_daemon
    drv_vofa
    drv_motor
    drv_motor/drv_djimotor
    drv_motor/drv_dmmotor
    drv_motor/drv_rsmotor
    drv_axis_lite
    drv_planner
    drv_mahony
    drv_mit
    # comm开始
    drv_comm/
    drv_comm/media
    drv_comm/proto
    # comm结束
)
