/**
 * @file bsp_log.h
 * @brief 日志系统（日志层）：分级 + 颜色 + 过滤 + 模块实例限频 + 时间戳
 *
 * 分层设计：
 *   - 日志层（本文件 + bsp_log.c）：负责分级、过滤、限频、时间戳与格式串
 *     组装（"[分级][时间戳][模块名]:内容"），借用静态池缓冲格式化后直接
 *     交日志串口 DMA 发送。对外只提供 2 个接口：
 *       ① BSPLogInit()            初始化 bsp 相关外设（DWT 时间戳 + 日志串口）
 *       ② BSPLOG(inst, level, fmt, ...)  宏：发送一条日志
 *     日志实例经 LOG_INSTANCE_DEF 编译期声明（模块名 + 限频内联在宏里，无运行时配置）。
 *     各级别全局累计条数：extern uint64_t level_cnt[LOG_LEVEL_NUM] 按级别下标直接读取
 *   - 传输（bsp_log.c 内实现）：UART DMA 直接发送 + 缓冲池状态机调度。
 *     借用/提交/出队均内联在 bsp_log.c，池槽 data/state/len 集中在
 *     log_buf_s 结构体；DMA 完成回调 LogUartTxCplt 归还缓冲并调度下一个。
 *     公共归还接口：BSPLogBufRelease()（DMA 完成后调用，避免池耗尽）。
 *
 * BSPLOG 在 BSP_LOG_USED 与 LOG_UART 均定义时把参数透传给核心实现 BSPLogV()
 * （bsp_log.c，限频/组装/发送都在 .c 里做）；任一未定义时 BSPLOG 为空宏、
 * 初始化函数为空实现，日志关闭（与 bsp_log.c 的 #if 实现条件保持一致）。
 *
 * 用法：
 *     BSPLogInit();                            // 内含默认实例 g_log 的初始化
 *     BSPLOG(&g_log, LOG_LEVEL_INFO, "hello"); // 直接使用默认实例
 *     LOG_INSTANCE_DEF(g_motor_log, "motor", 20); // 或按模块自定义实例（模块名 + 限频，0 = 不限频）
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
 * @note 经 LOG_INSTANCE_DEF 编译期静态初始化；模块名与长度字段 const（编译期定值、运行期只读）
 */
typedef struct
{
    const char *module_name;           /* 模块名：编译期字符串常量（存 flash，'\0' 结尾，长度见 module_name_len） */
    const uint8_t module_name_len;     /* 模块名长度（不含 '\0'），编译期推导、只读 */
    uint8_t times_per_second;          /* 每秒最大条数（上限 255）：LOG_INSTANCE_DEF 传 0 按 255 档计 */
    uint8_t log_cnt;                   /* 计次：本 1 秒窗口内已发送条数 */
    uint64_t last_timestamp_us;        /* 时间戳：上一次记录窗口起点的 us 值 */
    uint64_t level_cnt[LOG_LEVEL_NUM]; /* 本实例各级别累计日志条数（借用成功即计入，含排队；限频/池满丢弃不计） */
} LOGInstance;

/**
 * @brief 定义日志实例（编译期声明，替代旧 BSPLogInitInstance 运行时配置）
 * @param name   变量名
 * @param module 模块名（须传字符串字面量，编译期存 flash，如 "motor"）
 * @param limit  每秒最大条数：0 表示 255（最高档，接近不限频）
 * @note 在 .c 文件顶层使用：LOG_INSTANCE_DEF(g_motor, "motor", 20);
 *       使用 BSPLOG 时传 &变量名。实例为全局符号（非 static），外部 TU 用
 *       extern LOGInstance g_motor; 即可共享（如 bsp_log.c 的 g_log、
 *       drv_bmi088.c 的 g_bmi088_log 供 drv_bmi088_heater.c extern 复用）。
 * @note 宏形参名不得与 LOGInstance 字段名（module_name/module_name_len/…）相同：
 *       指定初始化器 .module_name 词法上是 "."+"module_name" 两个 token，若形参也叫
 *       module_name，预处理器会把其中 module_name 当形参替换成实参字符串，产生语法错误。
 */
/* 日志实例定义：日志关闭（BSP_LOG_USED 或 LOG_UART 未定义）时为空宏，
 * 不分配 LOGInstance，零 RAM 占用 */
#if (defined(BSP_LOG_USED)) && (defined(LOG_UART))
#define LOG_INSTANCE_DEF(name, module, limit)                   \
    LOGInstance name = {                                        \
        .module_name = (module),                                \
        .module_name_len = sizeof(module) - 1, /* 字面量长度 */ \
        .times_per_second = ((limit) ? (limit) : 255)}
#else
#define LOG_INSTANCE_DEF(name, module, limit)
#endif

/*============ 外部接口（2 个） ============*/

#if (defined(BSP_LOG_USED)) && (defined(LOG_UART))
/*============ 日志宏 ============*/

/* 默认日志实例（bsp_log.c 定义，编译期初始化为模块名 "log"、255 档限频）：
 * 免 LOG_INSTANCE_DEF 样板，直接 BSPLOG(&g_log, ...) 使用；
 * 多实例按需仍可自行 LOG_INSTANCE_DEF + 编译期初始化。 */
extern LOGInstance g_log;
extern uint64_t level_cnt[LOG_LEVEL_NUM];

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

/**
 * @brief 初始化日志系统依赖的 bsp 相关外设
 * @note DWT 高精度时间戳由启动流程（DWT_Init）初始化，本接口预留；
 *       后续如需日志侧统一初始化 bsp 外设，可在此实现。
 * @note 日志实例经 LOG_INSTANCE_DEF 编译期声明，无需运行时配置函数。
 */
void BSPLogInit(void);

#else /* 日志关闭：BSP_LOG_USED 或 LOG_UART 未定义（与 bsp_log.c 的 #if 一致） */

/* 日志实例未分配（LOG_INSTANCE_DEF 为空宏）、BSPLogInit 无可初始化外设，
 * 均定义为空宏吃掉调用点；关闭态 2 个接口全是空宏，零代码零 RAM。 */
#define BSPLOG(inst, level, fmt, ...) ((void)0)
#define BSPLogInit() ((void)0)

#endif /* (defined(BSP_LOG_USED)) && (defined(LOG_UART)) */

#endif /* __BSP_LOG_H */
