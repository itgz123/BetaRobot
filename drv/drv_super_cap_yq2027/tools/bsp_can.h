/**
 * @file bsp_can.h
 * @brief 离线测试用的 bsp_can API 桩
 *
 * @note 只复制驱动用到的类型/函数；实现见 test_drv_super_cap_yq2027.c，
 *       负责记录调用参数供断言使用。
 */

#ifndef __TEST_BSP_CAN_H
#define __TEST_BSP_CAN_H

#include "app_cfg.h"
#include <stdint.h>

#define CAN_ID_UNUSED ((uint32_t)0xFFFFFFFF)

typedef enum
{
    CAN_1 = 0,
    CAN_2,
    CAN_NUM_MAX
} BoardCAN_e;

typedef enum : uint8_t
{
    CAN_STANDARD_DATA_FRAME = 0,
    CAN_EXTENDED_DATA_FRAME = 1,
    CAN_STANDARD_REMOTE_FRAME = 2,
    CAN_EXTENDED_REMOTE_FRAME = 3,
} CAN_Frame_Type_e;

typedef enum : uint8_t
{
    CAN_FILTER_MODE_MASK = 0,
    CAN_FILTER_MODE_LIST = 1,
    CAN_FILTER_MODE_RANGE = 2,
} CAN_Filter_Mode_e;

typedef enum : uint8_t
{
    CAN_FRAME_FORMAT_CLASSIC = 0,
    CAN_FRAME_FORMAT_FD = 1,
    CAN_FRAME_FORMAT_FD_BRS = 2,
} CAN_Mode_Type_e;

typedef enum : int8_t
{
    BSP_OK = 0,
    BSP_BUSY = -1,
    BSP_TIMEOUT = -2,
    BSP_PARAM_ERR = -3,
    BSP_HW_ERR = -4,
} BSP_Status_e;

typedef enum : uint8_t
{
    CAN_ERR_BUS_OFF = 0,
    CAN_ERR_ERROR_PASSIVE = 1,
    CAN_ERR_ERROR_WARNING = 2,
    CAN_ERR_PROTOCOL = 3,
    CAN_ERR_RX_OVERFLOW = 4,
    CAN_ERR_HW = 5,
} CAN_ErrReason_e;

typedef struct CANInstance CANInstance;

typedef struct
{
    uint32_t id;
    CAN_Frame_Type_e frame_type;
    uint8_t len;
    uint8_t data[64];
} CAN_Pack_s;

typedef struct CAN_Filter_s
{
    CAN_Filter_Mode_e mode;
    uint32_t id0;
    uint32_t id1;
    CAN_Frame_Type_e frame_type;
    void (*callback)(struct CANInstance *instance, const CAN_Pack_s *pack);
} CAN_Filter_s;

typedef struct
{
    BoardCAN_e can_e;
    CAN_Mode_Type_e mode;
    void *parent;
    CAN_Filter_s *filters;
    uint8_t filter_num;
    void (*tx_complete_callback)(struct CANInstance *instance, uint32_t tx_mailbox, BSP_Status_e result);
    void (*err_callback)(struct CANInstance *instance, CAN_ErrReason_e reason);
} CAN_Config_s;

struct CANInstance
{
    BoardCAN_e can_e;
    CAN_Mode_Type_e mode;
    void *parent;
    CAN_Filter_s *filters;
    uint8_t filter_num;
    void (*tx_complete_callback)(struct CANInstance *instance, uint32_t tx_mailbox, BSP_Status_e result);
    void (*err_callback)(struct CANInstance *instance, CAN_ErrReason_e reason);
};

#define CAN_INSTANCE_DEF(name) static CANInstance name

/* 测试记录 */
typedef struct
{
    int register_calls;
    int config_calls;
    int transmit_calls;
    BSP_Status_e transmit_result;
    CAN_Pack_s last_pack;
    CAN_Config_s last_config;
} TestCanState;

extern TestCanState g_can;

BSP_Status_e CANRegister(CANInstance *instance);
BSP_Status_e CANConfig(CANInstance *instance, const CAN_Config_s *config);
BSP_Status_e CANTransmit(CANInstance *instance, const CAN_Pack_s *pack, uint32_t timeout_ms,
                         uint32_t *tx_mailbox, uint8_t *tx_free_level);

#endif /* __TEST_BSP_CAN_H */
