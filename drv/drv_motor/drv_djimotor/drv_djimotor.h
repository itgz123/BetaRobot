/**
 * @file drv_djimotor.h
 * @brief 大疆电机驱动封装 (M3508/M2006/GM6020)
 *
 * @note DRV 层职责：
 *       1. 封装大疆电机 CAN 通信
 *       2. 解析反馈数据（角度、速度、电流、温度）
 *       3. 支持电流/速度/位置闭环控制
 *       4. 不使用 FreeRTOS
 *
 * @note 使用示例：
 *       DJIMotorInstance motor_1 = {
 *           .base = { .type = MOTOR_TYPE_DJI },
 *           .can_inst = &can_1,
 *           .motor_id = 1,
 *           .type = DJI_MOTOR_M3508,
 *           .mode = DJI_MODE_CURRENT,
 *           .outer_loop = MOTOR_LOOP_CURRENT,
 *           .max_speed = 20.0f,
 *           .pid_speed = { .kp = 1.5f, .ki = 0.1f, .kd = 0.0f, .integral_limit = 10000.0f },
 *       };
 *       MotorInit(&motor_1.base);
 *       MotorSetSpeed(&motor_1.base, 10.0f);
 */

#ifndef __DRV_DJIMOTOR_H
#define __DRV_DJIMOTOR_H

#include "main.h"
#include "bsp_cfg.h"

#if defined(BSP_CAN_MODULE_ENABLED)

#include "bsp_can.h"
#include "drv_motor_base.h"
#include "drv_pid.h"
#include "bsp_dwt.h"
#include "bsp_math.h"
#include <stdint.h>

/*============================================
 *              宏定义
 *============================================*/

/* 电流/电压指令范围 */
#define DJI_M2006_CURRENT_MAX (10000)  /* M2006 最大电流指令 */
#define DJI_M3508_CURRENT_MAX (16384)  /* M3508 最大电流指令 */
#define DJI_GM6020_CURRENT_MAX (16384) /* GM6020 最大电流指令 */
#define DJI_GM6020_VOLTAGE_MAX (25000) /* GM6020 最大电压指令 */

/* 角度范围 */
#define DJI_ANGLE_MAX (8191)                                        /* 原始角度最大值 (0-8191) */
#define DJI_ANGLE_TO_RAD (2.0f * 3.14159265358979323846f / 8192.0f) /* 原始角度转弧度 */

/* 离线超时时间 (us) */
#define DJI_OFFLINE_TIMEOUT (500000) /* 500ms 无数据则离线 */

/*============================================
 *              类型定义
 *============================================*/

/**
 * @brief 大疆电机型号枚举
 */
typedef enum
{
    DJI_MOTOR_M3508 = 0, /* C620 电调 */
    DJI_MOTOR_M2006,     /* C610 电调 */
    DJI_MOTOR_GM6020,    /* 云台电机 */
} DJIMotorType_e;

/**
 * @brief 控制模式枚举（仅 GM6020 使用）
 */
typedef enum
{
    DJI_MODE_CURRENT = 0, /* 电流控制（默认） */
    DJI_MODE_VOLTAGE,     /* 电压控制（仅 GM6020） */
} DJIMotorMode_e;

/*============================================
 *              前向声明
 *============================================*/

typedef struct DJIMotorInstance DJIMotorInstance;
typedef struct DJIMotorGroup DJIMotorGroup;

/*============================================
 *              电机实例结构体
 *============================================*/

/**
 * @brief 大疆电机实例结构体
 * @note 继承 MotorInstance 基类，支持多态调用
 */
struct DJIMotorInstance
{
    /* ===== 基类（必须放在首位）===== */
    MotorInstance base; /* 电机基类，支持多态 */

    /* ===== 通信相关 ===== */
    CANInstance *can_inst; /* CAN 实例指针（分组时共享） */
    DJIMotorGroup *group;  /* 所属电机组（分组控制时使用） */
    uint8_t motor_id;      /* 电机 ID (1-8, GM6020 为 1-7) */
    uint8_t slot_index;    /* 在 CAN 帧中的槽位 (0-3) */

    /* ===== 配置 ===== */
    DJIMotorType_e type;        /* 电机型号 */
    DJIMotorMode_e mode;        /* 控制模式（仅 GM6020 使用） */
    MotorLoopType_e outer_loop; /* 外层闭环类型 */

    /* ===== 反馈数据（原始值）===== */
    uint16_t angle_raw;  /* 原始角度 (0-8191) */
    int16_t speed_rpm;   /* 转速 */
    int16_t current_raw; /* 原始电流/扭矩 */
    uint8_t temperature; /* 温度 (°C) */

    /* ===== 物理量转换 ===== */
    float angle_rad;   /* 角度 */
    float angle_total; /* 累计角度（多圈） */
    float speed_rad_s; /* 角速度 */
    float current_a;   /* 电流 (A) 或扭矩 */

    /* ===== 控制指令 ===== */
    int16_t cmd_value; /* 电流/电压指令（原始值） */
    int16_t cmd_max;   /* 指令最大值 */

    /* ===== PID 控制器 ===== */
    PIDInstance pid_speed; /* 速度环 PID */
    PIDInstance pid_angle; /* 位置环 PID */

    /* ===== 速度环参数 ===== */
    float max_speed; /* 最大速度，用于归一化 */

    /* ===== 时间管理 ===== */
    uint64_t last_time;    /* 上次控制时间 */
    uint64_t last_rx_time; /* 上次接收时间 */

    /* ===== 状态 ===== */
    uint8_t online;         /* 在线标志 */
    int16_t last_angle_raw; /* 上次角度（用于多圈计数） */
};

/*============================================
 *              电机组结构体（分组控制）
 *============================================*/

/**
 * @brief 电机组结构体
 * @note 多个电机共享一个 CAN 实例，统一发送控制帧
 */
struct DJIMotorGroup
{
    CANInstance can_inst;        /* CAN 实例 */
    DJIMotorInstance *motors[4]; /* 最多 4 个电机 */
    uint8_t motor_count;         /* 实际电机数量 */
    uint8_t tx_id_base;          /* 发送帧 ID 基地址 (0x200 或 0x1FF) */
};

/*============================================
 *              虚函数表声明
 *============================================*/

/**
 * @brief 大疆电机虚函数表
 * @note 用于多态调用，在 drv_djimotor.c 中定义
 */
extern const MotorInterface_s g_dji_motor_interface;

/*============================================
 *              外部接口声明
 *============================================*/

/*----------------- 电机组接口 -----------------*/

/**
 * @brief 初始化电机组（分组控制模式）
 * @param group       电机组实例指针
 * @param tx_id_base  发送帧 ID 基地址 (0x200 或 0x1FF)
 * @retval 0 成功
 * @retval -1 失败
 */
int8_t DJIMotorGroupInit(DJIMotorGroup *group, uint8_t tx_id_base);

/**
 * @brief 向电机组添加电机
 * @param group      电机组实例指针
 * @param motor      电机实例指针
 * @param slot_index 槽位索引 (0-3)
 * @retval 0 成功
 * @retval -1 失败（槽位已占用或组已满）
 */
int8_t DJIMotorGroupAddMotor(DJIMotorGroup *group, DJIMotorInstance *motor, uint8_t slot_index);

/**
 * @brief 刷新电机组发送（将所有电机指令打包发送）
 * @param group 电机组实例指针
 * @note 需要在控制循环中周期性调用
 */
void DJIMotorGroupRefresh(DJIMotorGroup *group);

/*----------------- 状态获取函数 -----------------*/

/**
 * @brief 获取电机角度
 * @param inst 电机实例指针
 * @return 角度
 */
float DJIMotorGetAngle(DJIMotorInstance *inst);

/**
 * @brief 获取电机角速度
 * @param inst 电机实例指针
 * @return 角速度
 */
float DJIMotorGetSpeed(DJIMotorInstance *inst);

/**
 * @brief 获取电机电流
 * @param inst 电机实例指针
 * @return 电流 (A) 或扭矩
 */
float DJIMotorGetCurrent(DJIMotorInstance *inst);

/**
 * @brief 获取电机温度
 * @param inst 电机实例指针
 * @return 温度 (°C)
 */
uint8_t DJIMotorGetTemperature(DJIMotorInstance *inst);

/**
 * @brief 检查电机是否在线
 * @param inst 电机实例指针
 * @return 1 在线, 0 离线
 */
uint8_t DJIMotorIsOnline(DJIMotorInstance *inst);

/*----------------- GM6020 专用函数 -----------------*/

/**
 * @brief 设置 GM6020 控制模式
 * @param inst 电机实例指针
 * @param mode 控制模式
 * @note 仅对 GM6020 有效
 */
void DJIMotorSetMode(DJIMotorInstance *inst, DJIMotorMode_e mode);

#endif /* BSP_CAN_MODULE_ENABLED */

#endif /* __DRV_DJIMOTOR_H */