# drv 层源文件
set(MODULE_SOURCES
    drv_sbus/drv_sbus.c
    drv_dbus/drv_dbus.c
    drv_bmi088/drv_bmi088.c
    drv_ist8310/drv_ist8310.c
    drv_daemon/drv_daemon.c
    drv_vofa/drv_vofa.c
    drv_motor/drv_djimotor/drv_djimotor_broadcast.c
    drv_motor/drv_dmmotor/drv_dmmotor.c
    drv_motor/drv_rsmotor/drv_rsmotor.c
    drv_motor/drv_lkmotor/drv_lkmotor.c
    drv_motor/drv_lkmotor/drv_lkmotor_broadcast.c
    drvs_motor/drvs_dmmotor/drvs_dmmotor.c
    drvs_motor/drvs_rsmotor/drvs_rsmotor.c
    drvs_motor/drvs_lkmotor/drvs_lkmotor.c
    drvs_motor/drvs_djimotor_broadcast/drvs_djimotor_broadcast.c
    drvs_motor/drvs_lkmotor_broadcast/drvs_lkmotor_broadcast.c
    drv_axis_lite/drv_axis_mit_lite.c
    drv_referee2026/public/referee2026_proto.c
    # referee2026：模块根放暴露接口（四条链路共用的 Register/Config/Send + 收发内核），
    # public 放协议层公共件，link_* 各放本链路的元信息表（只有类型与表，无函数）
    drv_referee2026/drv_referee2026.c
    drv_referee2026/link_common/referee2026_common.c
    drv_referee2026/link_video/referee2026_video.c
    drv_referee2026/link_none/referee2026_none.c
    drv_referee2026/link_radar/referee2026_radar.c
    drv_planner/drv_planner.c
    drv_pid/drv_pid.c
    drv_terminal_lite/drv_terminal_lite.c
    # comm开始
    drv_comm/drv_comm.c
    drv_comm/media/comm_media.c
    drv_comm/media/comm_media_usart.c
    drv_comm/media/comm_media_usb.c
    drv_comm/media/comm_media_usb_simple.c
    drv_comm/media/comm_media_can_pkt0.c
    drv_comm/media/comm_media_can_idseq.c
    drv_comm/proto/comm_proto.c
    drv_comm/proto/comm_proto_raw.c
    drv_comm/proto/comm_proto_custom.c
    drv_comm/proto/comm_proto_ext.c
    # comm结束
)

# drv 层头文件路径
set(MODULE_INCLUDE_DIRS
    drv_bmi088
    drv_ist8310
    drv_sbus
    drv_dbus
    drv_daemon
    drv_vofa
    drv_motor
    drv_motor/drv_djimotor
    drv_motor/drv_dmmotor
    drv_motor/drv_rsmotor
    drv_motor/drv_lkmotor
    drvs_motor/drvs_dmmotor
    drvs_motor/drvs_rsmotor
    drvs_motor/drvs_lkmotor
    drvs_motor/drvs_djimotor_broadcast
    drvs_motor/drvs_lkmotor_broadcast
    drv_axis_lite
    # referee2026：模块根放公开接口头 + public 放协议层公共件，link_* 各自放本链路的公开头（用哪条 include 哪个）
    drv_referee2026
    drv_referee2026/public
    drv_referee2026/link_common
    drv_referee2026/link_video
    drv_referee2026/link_none
    drv_referee2026/link_radar
    drv_planner
    drv_pid
    drv_terminal_lite
    # comm开始
    drv_comm/
    drv_comm/media
    drv_comm/proto
    # comm结束
)
