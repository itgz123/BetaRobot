/**
 * @file bsp_log.h
 * @brief 日志系统（日志层）：分级 + 颜色 + 过滤 + 模块实例限频 + 时间戳
 *
 * 分层设计：
 *   - 日志层（本文件 + bsp_log.c）：负责分级、过滤、限频、时间戳与格式串
 *     组装（"[分级][时间戳][模块名]:内容"），借用静态池缓冲格式化后直接
 *     交日志串口 DMA 发送。对外只提供 4 个接口：
 *       ① BSPLogInit()            初始化 bsp 相关外设（DWT 时间戳 + 日志串口）
 *       ② BSPLogInitInstance()    配置一个日志实例（模块名 + 限频）
 *       ③ BSPLOG(inst, level, fmt, ...)  宏：发送一条日志
 *       ④ BSPLogGetLevelCount()   读取某级别累计日志条数（全局统计）
 *   - 传输（bsp_log.c 内实现）：UART DMA 直接发送 + 缓冲池状态机调度。
 *     借用/提交/出队均内联在 bsp_log.c，池槽 data/state/len 集中在
 *     log_buf_s 结构体；DMA 完成回调 LogUartTxCplt 归还缓冲并调度下一个。
 *     公共归还接口：BSPLogBufRelease()（DMA 完成后调用，避免池耗尽）。
 *
 * BSPLOG 在 BSP_LOG_USED 定义时把参数透传给核心实现 BSPLogV()（bsp_log.c，
 * 限频/组装/发送都在 .c 里做）；BSP_LOG_USED 未定义时 BSPLOG 为空宏，日志关闭。
 *
 * 用法：
 *     BSPLogInit();                            // 内含默认实例 g_log 的初始化
 *     BSPLOG(&g_log, LOG_LEVEL_INFO, "hello"); // 直接使用默认实例
 *     LOG_INSTANCE_DEF(g_motor_log);           // 或按模块自定义实例
 *     BSPLogInitInstance(&g_motor_log,
 *                        &(LOG_Config_s){.module_name = "motor", .times_per_second = 20});
 *     BSPLOG(&g_motor_log, LOG_LEVEL_INFO, "enc=%d speed=%d", enc, speed);
 *
 * 兼容旧宏（LOGDEBUG/LOGINFO/...）：暂以空宏占位，既有调用点不输出日志；
 * 调用点后续统一迁移到 BSPLOG 后删除。
 */

#ifndef __BSP_LOG_H
#define __BSP_LOG_H

#include <stddef.h>
#include <stdint.h>
#include "bsp_format.h"
#include "app_cfg.h"

/*============ 配置（可在 app_cfg.h 中预定义覆盖） ============*/
/* 模块名长度上限（含 '\0'，实际最多 MODULE_NAME_LEN_MAX-1 个字符） */
#ifndef MODULE_NAME_LEN_MAX
#define MODULE_NAME_LEN_MAX 16
#endif

/* 单条日志最大长度（静态池每缓冲大小） */
#ifndef LOG_LEN_MAX
#define LOG_LEN_MAX 256
#endif

/* 日志缓冲区数量 */
#ifndef LOG_BUF_NUM
#define LOG_BUF_NUM 5
#endif

/* 时间戳形式：0=不发送时间戳，1=us 级，2=ms 级（均为整数） */
#ifndef TIME_STAMP_STYLE
#define TIME_STAMP_STYLE 1
#endif

/* 是否输出 ANSI 颜色（0 则颜色宏为空串） */
#ifndef BSP_LOG_ENABLE_COLOR
#define BSP_LOG_ENABLE_COLOR 1
#endif

/*============ 日志级别 ============*/

typedef enum
{
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_NUM, /* 级别个数：供 level_cnt 计数数组定长，勿作实际级别使用 */
} LOG_LEVEL;

/* 全局过滤级别：级别 < 此值的日志不发送（编译期常量，-O 下整条剔除） */
#ifndef LOG_FILTER_LEVEL
#define LOG_FILTER_LEVEL LOG_LEVEL_DEBUG
#endif

/*============ ANSI 颜色 ============*/

#if BSP_LOG_ENABLE_COLOR
/* @note ANSI 色码固定，但显示效果由终端主题（背景色）决定：
 *       深色背景下标准色清晰，浅色背景下暗色（蓝/青）可读性差。
 *       DEBUG 用蓝；若深色背景下偏暗，可换亮蓝 "\033[94m"。 */
#define BSP_LOG_COLOR_RESET "\033[0m"
#define BSP_LOG_COLOR_BLUE "\033[34m"   /* BLUE 蓝 */
#define BSP_LOG_COLOR_GREEN "\033[32m"  /* INFO 绿 */
#define BSP_LOG_COLOR_YELLOW "\033[33m" /* WARNING 黄 */
#define BSP_LOG_COLOR_RED "\033[31m"    /* ERROR 红 */
#else
#define BSP_LOG_COLOR_RESET ""
#define BSP_LOG_COLOR_BLUE ""
#define BSP_LOG_COLOR_GREEN ""
#define BSP_LOG_COLOR_YELLOW ""
#define BSP_LOG_COLOR_RED ""
#endif

/*============ 日志实例 ============*/

/**
 * @brief 日志实例：每模块一个，自带模块名与独立发送限频
 * @note 通过 LOG_INSTANCE_DEF 静态分配 + BSPLogInitInstance 配置
 */
typedef struct
{
    char module_name[MODULE_NAME_LEN_MAX]; /* 模块名（'\0' 结尾，实际长度记录于 module_name_len） */
    uint8_t module_name_len;               /* 模块名实际长度（不含 '\0'） */
    uint8_t times_per_second;              /* 每秒最大条数，0 = 不限频（上限 255） */
    uint8_t log_cnt;                       /* 计次：本 1 秒窗口内已发送条数 */
    uint64_t last_timestamp_us;            /* 时间戳：上一次记录窗口起点的 us 值 */
    uint64_t level_cnt[LOG_LEVEL_NUM];     /* 本实例各级别累计日志条数（借用成功即计入，含排队；限频/池满丢弃不计） */
} LOGInstance;

typedef struct
{
    char module_name[MODULE_NAME_LEN_MAX]; /* 模块名（'\0' 结尾，实际长度记录于 module_name_len） */
    uint8_t times_per_second;
} LOG_Config_s;

/**
 * @brief 静态分配日志实例
 * @param name 变量名
 * @note 须在 .c 文件作用域或函数内使用：LOG_INSTANCE_DEF(g_motor);
 *       使用 BSPLOG 时传 &变量名。
 */
#define LOG_INSTANCE_DEF(name) static LOGInstance name = {0}

/*============ 外部接口（3 个） ============*/

/* 默认日志实例（bsp_log.c 定义，BSPLogInit 初始化为模块名 "log"、不限频）：
 * 免 LOG_INSTANCE_DEF/BSPLogInitInstance 样板，直接 BSPLOG(&g_log, ...) 使用；
 * 多实例按需仍可自行 LOG_INSTANCE_DEF + BSPLogInitInstance。 */
extern LOGInstance g_log;
extern uint64_t level_cnt[LOG_LEVEL_NUM];

/**
 * @brief 初始化日志系统依赖的 bsp 相关外设
 * @note DWT 高精度时间戳由启动流程（DWT_Init）初始化，本接口预留；
 *       后续如需日志侧统一初始化 bsp 外设，可在此实现。
 */
void BSPLogInit(void);

/**
 * @brief 配置日志实例
 * @param inst 实例
 * @param cfg  配置结构体：module_name（模块名，拷贝进定长数组，超长截断）、
 *             times_per_second（每秒最大条数，0 = 不限，上限 255）；可为 NULL
 * @note 限频为 1 秒窗口：窗口内计数达到 times_per_second 后，后续日志被丢弃，
 *       直到距 last_timestamp_us 满 1 秒重置。须在 DWT 初始化（BSPLogInit）后调用。
 */
void BSPLogInitInstance(LOGInstance *inst, LOG_Config_s *cfg);

#if defined(BSP_LOG_USED)
/*============ 日志宏 ============*/

/* 级别过滤判断宏：level 低于 LOG_FILTER_LEVEL 返回真（本条剔除）。
 * LOG_FILTER_LEVEL 为编译期常量、level 为调用点字面量枚举值，
 * -O 下判断折叠，过滤分支连 BSPLogV 调用一起消失。 */
#define BSPLOG_FILTER(level) ((level) < LOG_FILTER_LEVEL)

/* 核心实现（bsp_log.c）：限频检查 + 组装 "[颜色][分级][时间戳][模块名]:内容[重置]\r\n"
 * 并经日志串口 DMA 发送。BSPLOG 只做编译期过滤后把参数透传给本函数。 */
void BSPLogV(LOGInstance *inst, LOG_LEVEL level, const char *fmt, ...);

/* 发送日志：过滤（level 低于 LOG_FILTER_LEVEL 整条剔除）+ 透传参数给 BSPLogV */
#define BSPLOG(inst, level, fmt, ...)                   \
    do                                                  \
    {                                                   \
        if (BSPLOG_FILTER(level))                       \
        {                                               \
            break; /* 级别低于过滤值：整条剔除 */       \
        }                                               \
        BSPLogV((inst), (level), (fmt), ##__VA_ARGS__); \
    } while (0)

#else /* !defined(BSP_LOG_USED) */

#define BSPLOG(inst, level, fmt, ...) ((void)0)

#endif /* BSP_LOG_USED */

/* 以下之后删除（兼容旧宏：无实例参数。当前为空宏占位，不输出日志；
 * 调用点后续统一迁移到 BSPLOG(inst, level, ...) 后删除） */
#define LOGDEBUG(format, ...) ((void)0)
#define LOGINFO(format, ...) ((void)0)
#define LOGWARNING(format, ...) ((void)0)
#define LOGERROR(format, ...) ((void)0)
/* 以上之后删除 */

#endif /* __BSP_LOG_H */
