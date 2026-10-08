/**
 * @file drvlib_motor.h
 * @brief 电机三级级联 PID 控制核：位置/速度/电流 + 反馈后处理（drvlib 层）
 *
 * ═══════════════════ 分层定位 ═══════════════════
 *   drvs_motor : 纯协议（字节 ↔ 物理量、收发），不做闭环/滤波/方向/多圈/偏置，
 *                无虚表、无使能状态，把"跨帧推断与策略"全部留给上层
 *   本模块     : drvlib 层 —— 把 lib_pid 的三个 PID 与"电机反馈后处理策略"组合
 *                起来，对上给 app 一个可直接驱动的三环控制对象
 *   app        : 胶水 —— 从 DrvsXxxGetData 取一帧反馈填进 DrvlibMotorFeedback_s，
 *                调 DrvlibMotorSetRef 拿到下发量交给 DrvsXxxSetRef / GroupSend
 *
 * ═══════════════════ 为什么本模块不依赖任何 drvs 头 ═══════════════════
 * 它只认 DrvlibMotorFeedback_s（位置/速度/电流/时间戳），对后端完全无关 ——
 * DM/RS/LK/DJI 广播乃至非 CAN 电机都能喂进来。
 *
 * ═══════════════════ 三级级联 ═══════════════════
 *   ref(位置 rad) ─PID[ANGLE]→ setpoint(速度 rad/s) ─PID[SPEED]→
 *     setpoint(电流 A) ─PID[CURRENT]→ 输出电流(A) → drvs
 * 由 loop_type 位掩码决定哪几环参与；未参与的环把输入原样透传给下一环。
 *   仅 SPEED  ：ref=速度 → PID → 电流    （旧 drv_motor 的常用通路）
 *   仅 ANGLE  ：ref=位置 → PID → 电流
 *   仅 CURRENT：ref=电流 → PID(电流反馈) → 电流
 *   位全为 0  ：开环，ref 直接作输出
 * 最内环语义是**电流环**（measure = 反馈电流 A）。要"力矩"语义时由 app 自己在
 * 传入 ref 前 ÷Kt、在拿到返回值后 ×Kt —— 本模块不掺和单位换算。
 * 方向同理：要取反就在 app 侧给 ref / 反馈量加符号，再给返回值加符号。
 *
 * ═══════════════════ 反馈后处理（drvs 明确上交的策略） ═══════════════════
 *   位置：多圈累加 → 偏置 → 归一化(WRAP)，顺序固定不可换
 *   速度：一阶低通；  电流：一阶低通（共用同一 RC）
 *   位置模式：LIMITED(限幅) / WRAP(归一化) / CONTINUOUS(不限幅)
 * 减速比不在这里（设计文档明确归 axis/chassis，电机侧是纯电机量）。
 *
 * ═══════════════════ dt ═══════════════════
 * 本模块不自带时间源：dt 由 app 每次 SetRef 传入，单位**秒**。
 * ⚠ APP_TASK_DEF 给 app 的 dt 是**毫秒**（bsp_app.h），须 ×0.001f 换算后传入。
 *
 * ═══════════════════ 使用顺序 ═══════════════════
 * @code
 *     DRVS_DJIMOTOR_BROADCAST_INSTANCE_DEF(m);
 *     DRVS_DJIMOTOR_BROADCAST_GROUP_DEF(m_group);
 *     DRVLIB_MOTOR_INSTANCE_DEF(ctrl);
 *
 *     DrvsDJIMotorBroadcastRegister(&m);
 *     DrvsDJIMotorBroadcastConfig(&m, &(DrvsDJIMotorBroadcastConfig_s){ ... });
 *     DrvlibMotorConfig(&ctrl, &(DrvlibMotor_Config_s){
 *         .loop_type = DRVLIB_MOTOR_LOOP_SPEED,
 *         .position_span = 6.2831853f,
 *         .pid[DRVLIB_MOTOR_STAGE_SPEED] = { .kp = 0.1f },
 *     });
 *     DrvlibMotorEnable(&ctrl);
 *
 *     // 周期任务（dt 为秒）：
 *     DrvsDJIMotorBroadcastData_s d = DrvsDJIMotorBroadcastGetData(&m);
 *     DrvlibMotorFeedback_s fb = { .position = d.position, .speed = d.speed,
 *                                  .current = d.current, .timestamp_us = d.timestamp_us };
 *     float out = DrvlibMotorSetRef(&ctrl, ref, &fb, dt_ms * 0.001f);
 *     DrvsDJIMotorBroadcastSetRef(&m, out);
 *     DrvsDJIMotorBroadcastGroupSend(&m_group);
 * @endcode
 */

#ifndef __DRVLIB_MOTOR_H
#define __DRVLIB_MOTOR_H

#include "app_cfg.h"

/* 依赖被组合的算法层：lib_pid（PID）+ lib_math（角度/限幅）（任一未开则整体不编译） */
#if defined(DRVLIB_MOTOR_USED) && defined(LIB_PID_USED) && defined(LIB_MATH_USED)

#include "lib_pid.h"
#include "lib_math.h"

/*============================ 枚举 ============================*/

/**
 * @brief PID 数组下标 = 级联环序（由外向内）；末项为环数，兼作 pid[] 维度
 */
typedef enum : uint8_t
{
    DRVLIB_MOTOR_STAGE_ANGLE = 0,   /* 最外环：位置 (rad) */
    DRVLIB_MOTOR_STAGE_SPEED = 1,   /* 中间环：速度 (rad/s) */
    DRVLIB_MOTOR_STAGE_CURRENT = 2, /* 最内环：电流 (A) */
    DRVLIB_MOTOR_STAGE_NUM = 3,     /* 环数 = pid[] 数组维度 */
} DrvlibMotorStage_e;

/**
 * @brief 环路选择位掩码：各环 = 1 << 对应 stage
 */
typedef enum : uint8_t
{
    DRVLIB_MOTOR_LOOP_OPEN = 0x00,                               /* 开环：ref 直接作输出 */
    DRVLIB_MOTOR_LOOP_ANGLE = 1 << DRVLIB_MOTOR_STAGE_ANGLE,     /* 最外环：measure = 位置 (rad) */
    DRVLIB_MOTOR_LOOP_SPEED = 1 << DRVLIB_MOTOR_STAGE_SPEED,     /* 中间环：measure = 速度 (rad/s) */
    DRVLIB_MOTOR_LOOP_CURRENT = 1 << DRVLIB_MOTOR_STAGE_CURRENT, /* 最内环：measure = 电流 (A) */
} DrvlibMotorLoop_e;

typedef enum : uint8_t
{
    DRVLIB_MOTOR_POS_LIMITED = 0,    /* 限幅：setpoint 限到 [min,max] */
    DRVLIB_MOTOR_POS_WRAP = 1,       /* 环绕：setpoint 归一化、误差归一化 */
    DRVLIB_MOTOR_POS_CONTINUOUS = 2, /* 连续：不限幅 */
} DrvlibMotorPositionMode_e;

typedef enum : uint8_t
{
    DRVLIB_MOTOR_FF_DISABLE = 0,  /* 无前馈 */
    DRVLIB_MOTOR_FF_EXTERNAL = 1, /* 用外部指针前馈 */
} DrvlibMotorFeedforwardSrc_e;

typedef enum : uint8_t
{
    DRVLIB_MOTOR_LPF_DISABLE = 0, /* 速度/电流不滤波（透传） */
    DRVLIB_MOTOR_LPF_ENABLE = 1,  /* 速度/电流一阶低通，RC 见 cfg.lpf_rc */
} DrvlibMotorLpf_e;

/*============================ 反馈输入 ============================*/

/**
 * @brief app 从 DrvsXxxGetData 拷进来的一帧协议原始量
 * @note 都是**未处理**的：未累加、未偏置、未滤波。跨帧推断与策略由本模块做。
 * @note position 的语义随后端：DJI/LK 单圈 [0,2π)；DM/RS [-p_max, p_max]（跨边界回绕），
 *       后者须把 cfg.position_span 设为 2*p_max 才能正确累加。
 * @note timestamp_us == 0 视为"尚无数据"，本帧不闭环（返回 0）。
 * @note 要取反方向就在**填这里之前**给相应字段加符号（本模块不做方向）。
 */
typedef struct
{
    float position;        /* rad */
    float speed;           /* rad/s */
    float current;         /* A */
    uint64_t timestamp_us; /* CAN 帧到达时间戳 (us) */
} DrvlibMotorFeedback_s;

/*============================ 配置结构体 ============================*/

/**
 * @brief drvlib_motor 配置结构体（Config 时整份拷贝进实例）
 * @note 外部前馈指针须由 app 保证长期有效（Config 只存指针）。
 */
typedef struct
{
    /* ---- 控制模式 ---- */
    DrvlibMotorLoop_e loop_type; /* 环路位掩码，见 DrvlibMotorLoop_e */

    /* ---- 位置模式 ---- */
    DrvlibMotorPositionMode_e position_mode;
    float angle_limit_min; /* LIMITED: 限幅下界 / WRAP: 归一化下界 */
    float angle_limit_max; /* LIMITED: 限幅上界 / WRAP: 归一化上界 */
    float position_span;   /* 单范围回绕跨度 (rad)，<=0 则不做多圈累加 */
    float position_offset; /* 位置偏置 (rad)：累加后叠加 */

    /* ---- 速度/电流低通 ---- */
    DrvlibMotorLpf_e lpf_enable;
    float lpf_rc; /* 时间常数 (s)，<=0 视为不用（透传） */

    /* ---- 前馈来源（三环各一，DISABLE 或指针为空则前馈 0） ---- */
    DrvlibMotorFeedforwardSrc_e angle_feedforward_src;
    DrvlibMotorFeedforwardSrc_e speed_feedforward_src;
    DrvlibMotorFeedforwardSrc_e current_feedforward_src;
    float *angle_feedforward_ptr;
    float *speed_feedforward_ptr;
    float *current_feedforward_ptr;

    /* ---- PID（按 stage 下标；只初始化参与环路的那些） ---- */
    PID_Init_Config_s pid[DRVLIB_MOTOR_STAGE_NUM];

    /* ---- 其他 ---- */
    float dt_max;        /* 单帧 dt 上限 (s)，<=0 不钳位 */
    uint8_t vofa_enable; /* 1 = 每帧写 VOFA CH1~CH3 并发送；默认 0 */
} DrvlibMotor_Config_s;

/*============================ 输出数据结构 ============================*/

/**
 * @brief 处理后的反馈量（由 app 直接读 inst->data）
 */
typedef struct
{
    double position;       /* 处理后多圈位置 (rad)：累加 + 偏置(+WRAP 归一化) */
    float position_single; /* 原始单范围位置 (rad) */
    int64_t position_cnt;  /* 回绕计数 */
    float speed;           /* 处理后速度 (rad/s)：低通 */
    float current;         /* 处理后电流 (A)：低通 */
    uint64_t timestamp_us;
} DrvlibMotor_Data_t;

/*============================ 实例结构体 ============================*/

typedef struct DrvlibMotorInstance
{
    PIDInstance pid[DRVLIB_MOTOR_STAGE_NUM]; /* 三个级联环 */
    DrvlibMotor_Config_s cfg;                /* 配置快照（含 app 传入的指针） */

    /* 反馈后处理状态 */
    float position_single_last; /* 上次原始位置，用于回绕检测 */
    int64_t position_cnt;       /* 回绕累加计数 */
    float speed_lpf_state;      /* 速度低通状态 */
    float current_lpf_state;    /* 电流低通状态 */
    uint8_t has_last;           /* 是否已收到过一帧 */

    uint8_t enable;
    uint8_t registered;

    DrvlibMotor_Data_t data; /* 最近一帧处理后反馈（app 直接读） */
} DrvlibMotorInstance;

/*============================ 实例定义宏 ============================*/

/**
 * @brief 定义控制实例（纯计算对象，无硬件句柄）
 * @example DRVLIB_MOTOR_INSTANCE_DEF(friction_motor_ctrl);
 */
#define DRVLIB_MOTOR_INSTANCE_DEF(name) static DrvlibMotorInstance name = {0}

/*============================ 公开接口 ============================*/

/**
 * @brief 注册（纯计算对象无硬件，仅清零并标记）
 * @param inst 实例指针
 * @return 0 成功，-1 失败
 * @note 调用一次即可，要求在 Config 之前
 */
int8_t DrvlibMotorRegister(DrvlibMotorInstance *inst);

/**
 * @brief 配置（可重入；会重置反馈累加与 PID 状态）
 * @param inst 实例指针
 * @param cfg  配置结构体指针
 * @return 0 成功，-1 失败（inst/cfg 为空）
 * @note 复刻旧 drv_motor：位置环/速度环的 PID 自动补 PID_ENABLE_TRAPEZOID_INTEGRAL；
 *       位置模式为 WRAP 时位置环自动启用误差归一化（范围 = max-min）。
 *       CURRENT 环不做这些默认（新环，保持 app 配置原样）。
 * @note 不改动既有使能态（使能须显式调 DrvlibMotorEnable）。
 */
int8_t DrvlibMotorConfig(DrvlibMotorInstance *inst, const DrvlibMotor_Config_s *cfg);

/**
 * @brief 传入一帧反馈并计算，返回下发量（由 app 周期任务调用）
 * @param inst 实例指针
 * @param ref  参考值（按 loop_type 解释：位置/速度/电流；开环时直接作输出）
 * @param fb   一帧反馈（协议原始量）；NULL 或 timestamp_us==0 视为无数据，返回 0
 * @param dt   本帧步长 (s)，由 app 传入（APP_TASK_DEF 的 dt 是 ms，须 ×0.001f）
 * @return 下发量（电流 A）；未使能 / 无数据时返回 0
 * @note 本函数同时承担「设参考值」与「算一帧」两职；处理方法后的反馈见 inst->data。
 */
float DrvlibMotorSetRef(DrvlibMotorInstance *inst, float ref, const DrvlibMotorFeedback_s *fb, float dt);

/**
 * @brief 使能（仅置软件标志；协议层使能由 app 调 DrvsXxxSendCmd）
 */
void DrvlibMotorEnable(DrvlibMotorInstance *inst);

/**
 * @brief 失能（清标志并复位 PID；停机还需 app 把返回值 0 下发）
 */
void DrvlibMotorDisable(DrvlibMotorInstance *inst);

/**
 * @brief 复位（清三个 PID 状态与低通状态，不清位置累加）
 */
void DrvlibMotorReset(DrvlibMotorInstance *inst);

#endif /* DRVLIB_MOTOR_USED && LIB_PID_USED && LIB_MATH_USED */

#endif /* __DRVLIB_MOTOR_H */
