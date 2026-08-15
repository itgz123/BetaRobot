/**
 * @file bsp_usb_types.h
 * @brief USB 设备协议栈基础类型定义（纯 C，参考 XRUSB / LibXR 的 core/dev_core）
 *
 * 职责：集中定义 USB 协议层常量、枚举与 packed 结构体。
 *   - 整数类型统一用 stdint 的 uint8_t/u16/u32（不引入自定义整数别名）
 *   - 固定底层类型枚举（typedef enum : uint8_t，成员显式赋值）
 *   - packed 结构：SETUP 包、CDC 线路编码、串行状态通知、各描述符
 * 不包含任何 HAL 依赖，三块板通用。
 */

#ifndef __BSP_USB_TYPES_H
#define __BSP_USB_TYPES_H

#include "stdint.h"

/*------------- 协议栈配置（默认值，可在 app_cfg.h 或编译选项覆盖） --------------*/
#ifndef USB_INSTANCE_NUM
#define USB_INSTANCE_NUM 1 /* 实例注册表容量 */
#endif

#ifndef USB_EP_MAX
#define USB_EP_MAX 6 /* 数据端点号上界（0..USB_EP_MAX-1；多类组合需更多端点） */
#endif

#ifndef USB_CLASS_MAX
#define USB_CLASS_MAX 2 /* 类槽容量（每配置：默认 CDC + 预留 1 个扩展类） */
#endif

#ifndef USB_CONFIG_MAX
#define USB_CONFIG_MAX 2 /* 配置数量（SET_CONFIGURATION 可选 1..USB_CONFIG_MAX；对照 XRUSB configs 列表） */
#endif

#ifndef USB_DESC_CFG_BUFF_SIZE
#define USB_DESC_CFG_BUFF_SIZE 128 /* 配置描述符拼接缓冲字节数 */
#endif

/* FIFO 配额（单位 32 位字），Reset 回调据此重配 */
#ifndef USB_RX_FIFO_SIZE
#define USB_RX_FIFO_SIZE 80
#endif
#ifndef USB_TX0_FIFO_SIZE
#define USB_TX0_FIFO_SIZE 16
#endif
#ifndef USB_TX1_FIFO_SIZE
#define USB_TX1_FIFO_SIZE 32
#endif
#ifndef USB_TX2_FIFO_SIZE
#define USB_TX2_FIFO_SIZE 8
#endif
#ifndef USB_TX3_FIFO_SIZE
#define USB_TX3_FIFO_SIZE 16 /* 多类组合备用（HID 等第二类可能落到 EP3） */
#endif

/*------------- bmRequestType 掩码 --------------*/
typedef enum : uint8_t
{
    USB_REQ_DIR_MASK = 0x80,       /* 位7：方向 */
    USB_REQ_TYPE_MASK = 0x60,      /* 位6-5：请求类型 */
    USB_REQ_RECIPIENT_MASK = 0x1F, /* 位4-0：接收者 */
} USBReqMask_e;

/* 请求方向（位7 取值） */
typedef enum : uint8_t
{
    USB_DIR_OUT = 0x00,
    USB_DIR_IN = 0x80,
} USBReqDir_e;

/* 请求类型（位6-5 取值） */
typedef enum : uint8_t
{
    USB_TYPE_STANDARD = 0x00,
    USB_TYPE_CLASS = 0x20,
    USB_TYPE_VENDOR = 0x40,
} USBReqType_e;

/* 接收者（位4-0 取值） */
typedef enum : uint8_t
{
    USB_RECIPIENT_DEVICE = 0x00,
    USB_RECIPIENT_INTERFACE = 0x01,
    USB_RECIPIENT_ENDPOINT = 0x02,
    USB_RECIPIENT_OTHER = 0x03,
} USBRecipient_e;

/*------------- 标准请求码 --------------*/
typedef enum : uint8_t
{
    USB_STD_GET_STATUS = 0,
    USB_STD_CLEAR_FEATURE = 1,
    /* 2 = 保留 */
    USB_STD_SET_FEATURE = 3,
    /* 4 = 保留 */
    USB_STD_SET_ADDRESS = 5,
    USB_STD_GET_DESCRIPTOR = 6,
    USB_STD_SET_DESCRIPTOR = 7,
    USB_STD_GET_CONFIGURATION = 8,
    USB_STD_SET_CONFIGURATION = 9,
    USB_STD_GET_INTERFACE = 10,
    USB_STD_SET_INTERFACE = 11,
    USB_STD_SYNCH_FRAME = 12,
} USBStdRequest_e;

/* 特性选择子 */
typedef enum : uint8_t
{
    USB_FEATURE_ENDPOINT_HALT = 0,
    USB_FEATURE_DEVICE_REMOTE_WAKEUP = 1,
} USBFeature_e;

/*------------- 描述符类型 --------------*/
typedef enum : uint8_t
{
    USB_DESC_DEVICE = 0x01,
    USB_DESC_CONFIGURATION = 0x02,
    USB_DESC_STRING = 0x03,
    USB_DESC_INTERFACE = 0x04,
    USB_DESC_ENDPOINT = 0x05,
    USB_DESC_IAD = 0x0B,
    USB_DESC_CS_INTERFACE = 0x24,
} USBDescType_e;

/*------------- 设备类代码（设备描述符 bDeviceClass） --------------*/
typedef enum : uint8_t
{
    USB_CLASS_CDC_COMM = 0x02,
    USB_CLASS_CDC_DATA = 0x0A,
    USB_CLASS_MISC = 0xEF,
} USBClass_e;

/*------------- CDC 类特定请求 --------------*/
typedef enum : uint8_t
{
    USB_CDC_SET_LINE_CODING = 0x20,
    USB_CDC_GET_LINE_CODING = 0x21,
    USB_CDC_SET_CONTROL_LINE_STATE = 0x22,
    USB_CDC_SEND_BREAK = 0x23,
} USBCDCRequest_e;

/* CDC 控制线路状态位 */
typedef enum : uint8_t
{
    USB_CDC_DTR = 0x01,
    USB_CDC_RTS = 0x02,
} USBCDCControlBit_e;

/* CDC 功能描述符子类型 */
typedef enum : uint8_t
{
    USB_CDC_SUBTYPE_HEADER = 0x00,
    USB_CDC_SUBTYPE_CALL_MGMT = 0x01,
    USB_CDC_SUBTYPE_ACM = 0x02,
    /* 0x03-0x05 = 保留 */
    USB_CDC_SUBTYPE_UNION = 0x06,
} USBCDCSubtype_e;

/*------------- 端点方向 / 类型 / 状态 --------------*/
typedef enum : uint8_t
{
    USB_EP_DIR_OUT = 0,
    USB_EP_DIR_IN = 1,
} USBEPDir_e;

/* 端点号自动分配（USB_EPPoolGet 用） */
#define USB_EP_NUM_AUTO 0xFE

/* 端点缓冲块：单端点单方向两块缓冲（Active/Pending 各一块） */
#define USB_EP_BUFF_SIZE 64
typedef struct
{
    uint8_t blocks[2][USB_EP_BUFF_SIZE]; /* [0]=起始块 [1]=备用块（双缓冲第二半） */
} USB_EPBuffer_t;

typedef enum : uint8_t
{
    USB_EP_TYPE_CONTROL = 0,
    USB_EP_TYPE_ISO = 1,
    USB_EP_TYPE_BULK = 2,
    USB_EP_TYPE_INTERRUPT = 3,
} USBEPType_e;

typedef enum : uint8_t
{
    USB_EP_STATE_DISABLED = 0,
    USB_EP_STATE_IDLE = 1,
    USB_EP_STATE_BUSY = 2,
    USB_EP_STATE_STALLED = 3,
    USB_EP_STATE_ERROR = 4,
} USBEPState_e;

/* 端点地址换算（函数式宏保留：num/dir 可为变量） */
#define USB_EP_ADDR(num, dir) ((uint8_t)((num) | ((dir) == USB_EP_DIR_IN ? 0x80 : 0x00)))

/*------------- packed 协议结构 --------------*/
#pragma pack(push, 1)

/* USB SETUP 包（固定 8 字节） */
typedef struct
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USB_SetupPacket_t;

/* CDC 线路编码（固定 7 字节） */
typedef struct
{
    uint32_t dwDTERate;  /* 波特率（小端） */
    uint8_t bCharFormat; /* 停止位：0=1,1=1.5,2=2 */
    uint8_t bParityType; /* 校验：0=None,1=Odd,2=Even,3=Mark,4=Space */
    uint8_t bDataBits;   /* 数据位：5/6/7/8/16 */
} USB_LineCoding_t;

/* CDC 串行状态通知（固定 10 字节，经 EP2 IN 中断端点发送） */
typedef struct
{
    uint8_t bmRequestType; /* 固定 0xA1 */
    uint8_t bNotification; /* 固定 0x20 = SERIAL_STATE */
    uint16_t wValue;       /* 固定 0 */
    uint16_t wIndex;       /* 通信接口号 */
    uint16_t wLength;      /* 固定 2 */
    uint16_t serialState;  /* 串行状态位图 */
} USB_SerialStateNotif_t;

/* 设备描述符（18 字节） */
typedef struct
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} USB_DescDevice_t;

/* 配置描述符头（9 字节） */
typedef struct
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} USB_DescConfigHeader_t;

/* 接口关联描述符 IAD（8 字节） */
typedef struct
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bFirstInterface;
    uint8_t bInterfaceCount;
    uint8_t bFunctionClass;
    uint8_t bFunctionSubClass;
    uint8_t bFunctionProtocol;
    uint8_t iFunction;
} USB_DescIAD_t;

/* 接口描述符（9 字节） */
typedef struct
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} USB_DescInterface_t;

/* 端点描述符（7 字节） */
typedef struct
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} USB_DescEndpoint_t;

/* CDC 类特定功能描述符（共 19 字节，4 个功能块） */
typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint16_t bcdCDC; /* Header 专用：CDC 版本 0x0110 */
} USB_CDC_FuncHeader_t;

typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bmCapabilities;
    uint8_t bDataInterface;
} USB_CDC_FuncCallMgmt_t;

typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bmCapabilities;
} USB_CDC_FuncACM_t;

typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bMasterInterface;
    uint8_t bSlaveInterface0;
} USB_CDC_FuncUnion_t;

/* CDC 整体描述符块（66 字节）：
 *   IAD 8 + 通信接口 9 + Header 5 + CallMgmt 5 + ACM 4 + Union 5
 *   + 通信 INTERRUPT IN 端点 7 + 数据接口 9 + BULK OUT 7 + BULK IN 7 = 66
 */
typedef struct
{
    USB_DescIAD_t iad;
    USB_DescInterface_t comm_intf;
    USB_CDC_FuncHeader_t cdc_header;
    USB_CDC_FuncCallMgmt_t cdc_callmgmt;
    USB_CDC_FuncACM_t cdc_acm;
    USB_CDC_FuncUnion_t cdc_union;
    USB_DescEndpoint_t comm_ep;
    USB_DescInterface_t data_intf;
    USB_DescEndpoint_t data_ep_out;
    USB_DescEndpoint_t data_ep_in;
} USB_CDCDescBlock_t;

#pragma pack(pop)

/*------------- 前向声明 --------------*/
typedef struct USBEndpoint USBEndpoint;
typedef struct USBInstance USBInstance;

/*------------- 类请求结果（CDC 返回给 core 的数据阶段描述） --------------*/
typedef struct
{
    uint8_t *read_data;        /* Host->Device：数据阶段接收目标（长度 >= wLength） */
    uint16_t read_len;         /* read_data 容量 */
    const uint8_t *write_data; /* Device->Host：数据阶段发送源 */
    uint16_t write_len;        /* write_data 长度 */
    uint8_t read_zlp;          /* 无数据阶段，接收 ZLP */
    uint8_t write_zlp;         /* 无数据阶段，发送 ZLP */
} USB_ClassReqResult_t;

/* 静态断言：packed 结构大小正确 */
_Static_assert(sizeof(USB_SetupPacket_t) == 8, "USB_SetupPacket_t must be 8B");
_Static_assert(sizeof(USB_LineCoding_t) == 7, "USB_LineCoding_t must be 7B");
_Static_assert(sizeof(USB_SerialStateNotif_t) == 10, "USB_SerialStateNotif_t must be 10B");
_Static_assert(sizeof(USB_DescDevice_t) == 18, "USB_DescDevice_t must be 18B");
_Static_assert(sizeof(USB_DescConfigHeader_t) == 9, "USB_DescConfigHeader_t must be 9B");
_Static_assert(sizeof(USB_DescIAD_t) == 8, "USB_DescIAD_t must be 8B");
_Static_assert(sizeof(USB_DescInterface_t) == 9, "USB_DescInterface_t must be 9B");
_Static_assert(sizeof(USB_DescEndpoint_t) == 7, "USB_DescEndpoint_t must be 7B");
_Static_assert(sizeof(USB_CDCDescBlock_t) == 66, "USB_CDCDescBlock_t must be 66B");

#endif /* __BSP_USB_TYPES_H */
