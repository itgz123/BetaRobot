/**
 * @file lib_fsm_table.h
 * @brief 表驱动通用状态机（状态/事件为整数、转移查表、动作用函数指针）
 * @author TRW
 * @date 2026-10-08
 *
 * @note 概念 → C 的对应：
 *       - 状态 State      用户 enum，框架内部按 int32_t 存取（枚举值即状态值）
 *       - 事件 Event      用户 enum，框架内部按 int32_t 存取
 *       - 转移 Transition LibFsmTableTransition_s 数组（from/event/to/guard/action/kind），表驱动
 *       - 守卫 Guard      bool (*)(void *ctx, const void *event_data)，返回 true 才允许转移
 *       - 转移动作 Action void (*)(void *ctx, const void *event_data)，转移时执行，可有副作用
 *       - 进入/退出动作   按状态值索引的函数指针表 LibFsmTableStateActions_s
 *       - 上下文 Context  void *ctx，框架不碰业务数据
 *       - 事件参数        const void *event_data，需要时传业务结构体指针
 *       - 跟踪钩子 Trace  可选回调，每次派发都报告"发生了什么/为什么没转移"
 *
 * @note 匹配与优先级（表顺序即优先级，这是本模块的隐式契约，这里显式写死）：
 *       1. 从表头开始逐条求值，**第一条命中的生效，后面的不再尝试**
 *       2. 命中判据 = （源状态匹配）且（事件匹配）且（守卫为 NULL 或守卫返回 true）
 *       3. 源状态匹配 = from == 当前状态，或 from == LIB_FSM_TABLE_ANY_STATE
 *          事件匹配 = event == 本次事件，或 event == LIB_FSM_TABLE_ANY_EVENT
 *       4. 所以同 from/event 的多条规则按数组顺序退化为"优先级从高到低"，
 *          守卫不通过的就跳过、继续试下一条；通配符规则一般放表尾兜底（放前面会吃掉后面的规则）
 *
 * @note 一次事件只处理一条转移；一条都不命中时 LibFsmTableDispatch 返回 false，
 *       由调用方决定忽略/记日志/默认处理（也可以挂 trace 钩子看到"为什么没转移"）。
 *
 * @note 转移的执行顺序（UML 惯例）：
 *           退出旧状态 → 转移动作 → 切换状态 → 进入新状态
 *       - 守卫在任何动作之前求值；守卫不通过时整条转移都不执行（退出动作也不会执行）
 *       - 外部自转移（from == to 且 kind == EXTERNAL）走完这四步，退出与进入各执行一次
 *       - 内部转移（kind == INTERNAL）不退出、不进入、不改 current，只执行转移动作：
 *         "状态不变但要响应事件"（如 RUN 下每次 tick 刷新计时器）就用它，
 *         此时 to 字段被忽略（照写当前状态便于阅读即可），trace 报告的 from/to 都是当前状态
 *
 * @note 状态值域约定（进入/退出动作表按状态值直接做数组下标）：
 *       - 状态枚举建议从 0 起连续，且 state_count 至少覆盖到"最大状态值 + 1"
 *       - 枚举有空洞也可以，只是空洞处的动作表项永远不会被调用
 *       - 越界或负的状态值不会造成越界访问：本模块静默跳过其进入/退出动作
 *         （状态值本身照常写入 current，是否合法由调用方保证）
 *       - state_actions 整体传 NULL（不要进入/退出动作）时 state_count 无意义
 *       - LIB_FSM_TABLE_INVALID_STATE(-1) 与两个通配符值保留给框架，业务状态不要取这几个值
 *
 * @note 回调契约：
 *       - 守卫只做判断、不得有副作用，并且**必须幂等**：同一次派发里它可能被调用多次
 *         （同 from/event 的多条规则依次求值），且会先于任何动作执行
 *       - 动作/进入/退出/trace 回调内不得再对本实例调用 LibFsmTableDispatch
 *
 * @note 本模块不加锁、不防重入：多任务/中断同时派发同一个实例需调用方自持互斥。
 *
 * @note 本模块不含时间源、不含事件队列：超时请由调用方在外层"到点派发 E_TIMEOUT"，
 *       事件来源（中断/网络/UI）需要缓冲时由调用方搭环形队列、主循环逐个取出再派发。
 *       本模块只回答"收到一个事件后怎么转移"，保持可测试的纯逻辑定位。
 *
 * @note 配置入口（与 lib_kf / lib_hamming 同惯例）：
 *       1) 固件：app_cfg.h 定义 LIB_FSM_TABLE_USED；
 *       2) PC 检验（tools/test_fsm_table.sh）：定义 LIB_FSM_TABLE_STANDALONE 跳过 app_cfg.h。
 *       本文件仅依赖 <stdbool.h>/<stddef.h>/<stdint.h>。
 */

#ifndef __LIB_FSM_TABLE_H
#define __LIB_FSM_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*============================================
 *              类型定义
 *============================================*/

/** @brief 状态值：直接用用户 enum（枚举值即状态值），框架按 int32_t 存取 */
typedef int32_t LibFsmTableState_t;

/** @brief 事件值：直接用用户 enum，框架按 int32_t 存取 */
typedef int32_t LibFsmTableEvent_t;

/**
 * @brief 守卫：返回 true 才允许这条转移
 * @param ctx        用户上下文（LibFsmTableConfig_s.ctx）
 * @param event_data 本次事件参数，可能为 NULL
 * @return true=放行 false=拦下（换下一条同 from/event 的规则继续试）
 * @note 守卫必须无副作用且幂等：它会先于退出/转移/进入动作执行，
 *       且同一次派发里可能被反复调用（多条候选规则依次求值）
 */
typedef bool (*LibFsmTableGuard_fn)(void *ctx, const void *event_data);

/**
 * @brief 转移动作：转移真正发生时执行
 * @param ctx        用户上下文
 * @param event_data 本次事件参数，可能为 NULL
 * @note 外部转移里它在"退出旧状态之后、切换状态之前"执行；内部转移里它是唯一被执行的用户回调
 */
typedef void (*LibFsmTableAction_fn)(void *ctx, const void *event_data);

/**
 * @brief 进入/退出动作：绑定在状态上，不需要事件参数
 * @param ctx 用户上下文
 */
typedef void (*LibFsmTableStateAction_fn)(void *ctx);

/**
 * @brief 跟踪钩子：每次派发结束都被调用一次，无论有没有发生转移
 * @param ctx          用户上下文
 * @param from         转移前的状态（未转移时 == 当前状态）
 * @param event        本次事件（含未匹配的）
 * @param to           转移后的状态（未转移或内部转移时 == 当前状态）
 * @param transitioned true=发生了转移 false=没有匹配
 * @note 用于定位"为什么没转移"：PC 上直接打日志，固件上可以塞环形缓冲或计数器；
 *       传 NULL 表示不跟踪（默认）。
 */
typedef void (*LibFsmTableTrace_fn)(void *ctx, LibFsmTableState_t from, LibFsmTableEvent_t event, LibFsmTableState_t to, bool transitioned);

/**
 * @brief 转移类型（与 UML 的"外部转移/内部转移"同义）
 */
typedef enum : uint8_t
{
    LIB_FSM_TABLE_TRANS_EXTERNAL = 0, // 外部转移（默认）：退出旧状态 → 转移动作 → 切换状态 → 进入新状态
    LIB_FSM_TABLE_TRANS_INTERNAL,     // 内部转移：状态不变，只执行转移动作（不退出、不进入）
} LibFsmTableTransKind_e;

/**
 * @brief 一条转移规则（通常是 static const 数组，可被多个实例共享）
 * @note 字段顺序即初始化顺序；kind 在末尾且 0 = 外部转移，故只写前 5 个字段的写法也成立
 *       （-Wextra 会报 missing-field-initializers，建议显式写出 kind 或用指派初始化）
 */
typedef struct
{
    LibFsmTableState_t from;     // 源状态，可为 LIB_FSM_TABLE_ANY_STATE
    LibFsmTableEvent_t event;    // 触发事件，可为 LIB_FSM_TABLE_ANY_EVENT
    LibFsmTableState_t to;       // 目标状态（内部转移时被忽略）
    LibFsmTableGuard_fn guard;   // 守卫：为 NULL 表示恒真
    LibFsmTableAction_fn action; // 转移动作：为 NULL 表示无动作
    LibFsmTableTransKind_e kind; // 转移类型：0=外部 1=内部
} LibFsmTableTransition_s;

/** @brief 某个状态的进入/退出动作（按状态值做数组下标） */
typedef struct
{
    LibFsmTableStateAction_fn entry; // 进入该状态时调用，为 NULL 表示无
    LibFsmTableStateAction_fn exit;  // 退出该状态时调用，为 NULL 表示无
} LibFsmTableStateActions_s;

/** @brief 初始化配置：表与上下文都由调用方持有，Init 只把这些指针/数值记进实例 */
typedef struct
{
    LibFsmTableState_t initial_state;               // 初始状态
    const LibFsmTableTransition_s *transitions;     // 转移表
    uint32_t transition_count;                      // 转移表条数
    const LibFsmTableStateActions_s *state_actions; // 进入/退出动作表，可为 NULL
    uint32_t state_count;                           // 动作表项数（状态值域上界）
    void *ctx;                                      // 用户上下文，回调时原样回传
    LibFsmTableTrace_fn trace_fn;                   // 跟踪钩子，可为 NULL
} LibFsmTableConfig_s;

/** @brief 状态机实例：一个实例一份 current，表与动作表可多实例共享 */
typedef struct
{
    LibFsmTableState_t current;                     // 当前状态
    const LibFsmTableTransition_s *transitions;     // 转移表（来自配置）
    uint32_t transition_count;                      // 转移表条数（来自配置）
    const LibFsmTableStateActions_s *state_actions; // 进入/退出动作表（来自配置，可为 NULL）
    uint32_t state_count;                           // 动作表项数（来自配置）
    void *ctx;                                      // 用户上下文（来自配置）
    LibFsmTableTrace_fn trace_fn;                   // 跟踪钩子（来自配置，可为 NULL）
} LibFsmTableInstance_s;

/*============================================
 *            保留值与通配符
 *============================================*/

/** @brief 无效状态：LibFsmTableCurrent 收到 NULL 实例时返回它（业务状态不要取 -1） */
#define LIB_FSM_TABLE_INVALID_STATE ((LibFsmTableState_t)(-1))

/** @brief 通配源状态：写进 from 表示"任意状态都能匹配"（一般放表尾兜底） */
#define LIB_FSM_TABLE_ANY_STATE ((LibFsmTableState_t)INT32_MIN)

/** @brief 通配事件：写进 event 表示"任意事件都能匹配"（一般放表尾兜底） */
#define LIB_FSM_TABLE_ANY_EVENT ((LibFsmTableEvent_t)INT32_MIN)

/*============================================
 *            编译期检查（可选）
 *============================================*/

/**
 * @brief 转移表自检：非空 + 元素类型正确
 * @param tbl 转移表数组本体（不是指针）
 * @note 用在文件作用域或函数内均可；C11 的 _Static_assert，失败即编译报错
 */
#define LIB_FSM_TABLE_CHECK_TABLE(tbl)                                                                                                                         \
    _Static_assert(sizeof(tbl) / sizeof((tbl)[0]) > 0u, "转移表不能为空");                                                                                     \
    _Static_assert(sizeof((tbl)[0]) == sizeof(LibFsmTableTransition_s), "转移表元素类型必须是 "                                                                \
                                                                        "LibFsmTableTransition_s")

/**
 * @brief 动作表自检：项数必须等于传给 Init 的 state_count
 * @param tbl         进入/退出动作表数组本体（不是指针）
 * @param state_count 与 LibFsmTableConfig_s.state_count 同值
 * @note 专门拦"枚举加了状态、动作表忘了加一项"这类错位：长度对不上直接编译报错
 */
#define LIB_FSM_TABLE_CHECK_ACTIONS(tbl, state_count)                                                                                                          \
    _Static_assert(sizeof(tbl) / sizeof((tbl)[0]) == (state_count), "进入/退出动作表长度与 state_count 不一致")

/*============================================
 *              外部接口声明
 *============================================*/

/**
 * @brief 初始化状态机实例
 * @param inst 状态机实例指针
 * @param cfg  初始化配置指针
 *
 * @note 只记录表指针、初始状态与跟踪钩子，不调用任何回调（含初始状态的进入动作）；
 *       要执行初始状态的进入动作，随后调一次 LibFsmTableStart。
 * @note 表内容应与实例同寿命（本模块不拷贝表）。
 * @note inst 或 cfg 为 NULL 时直接返回（实例内部值保持原样）。
 */
void LibFsmTableInit(LibFsmTableInstance_s *inst, const LibFsmTableConfig_s *cfg);

/**
 * @brief 启动状态机：执行当前状态（通常是初始状态）的进入动作
 * @param inst 状态机实例指针
 *
 * @note 在 LibFsmTableInit 之后调用一次；Init 本身不执行进入动作，两者分开是
 *       为了让"初始化"与"对外生效"的时机由调用方掌握（例如先把硬件准备好再启动）。
 * @note 本函数不记录"已启动"标志：重复调用会把当前状态的进入动作再执行一遍，
 *       需要防重复就由调用方自己保证只调一次。
 * @note inst 为 NULL 或当前状态越界时静默跳过。
 */
void LibFsmTableStart(LibFsmTableInstance_s *inst);

/**
 * @brief 派发一个事件：查表找第一条命中的转移并执行
 * @param inst       状态机实例指针
 * @param event      事件值
 * @param event_data 事件参数，可为 NULL，守卫与转移动作都能读到
 * @return true=发生了转移 false=当前状态下没有处理该事件的转移
 *
 * @note 命中判据与优先级见文件头（表顺序即优先级，第一条命中即停）；
 *       外部转移执行"退出旧状态 → 转移动作 → 切换状态 → 进入新状态"，
 *       内部转移只执行转移动作。
 * @note 无论是否转移，结束时都会调一次 trace_fn（若配置了）。
 * @note inst 为 NULL、转移表为 NULL 或条数为 0 时返回 false。
 */
bool LibFsmTableDispatch(LibFsmTableInstance_s *inst, LibFsmTableEvent_t event, const void *event_data);

/**
 * @brief 读取当前状态
 * @param inst 状态机实例指针
 * @return 当前状态值；inst 为 NULL 时返回 LIB_FSM_TABLE_INVALID_STATE(-1)
 *         （不再返回 0 —— 0 往往是合法状态，如 STATE_IDLE）
 */
LibFsmTableState_t LibFsmTableCurrent(const LibFsmTableInstance_s *inst);

#endif /* __LIB_FSM_TABLE_H */
