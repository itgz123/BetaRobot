/**
 * @file lib_fsm_state.h
 * @brief 状态结构体 + 虚表通用状态机（每个状态是一个对象，行为/数据内聚在状态里）
 * @author TRW
 * @date 2026-10-08
 *
 * @note 与 lib_fsm_table 的关系：两者是同一问题的两条路线，按场景选用，不互相调用
 *       - lib_fsm_table：转移规则集中成一张表，状态只是整数，适合"状态少、规则多"
 *       - 本模块：每个状态是一个静态单例对象（自带 entry/exit/handle，可带自己的数据），
 *         适合"状态多、行为差异大、每个状态有自己的数据"
 *       - 本模块另带混合方案：状态内挂一张转移表（transitions），
 *         不写 handle 的简单状态自动走通用默认 handle，于是"虚表 + 表驱动"可混用
 *
 * @note 概念 → C 的对应：
 *       - 状态 State      LibFsmStateNode_s 静态单例（name / ops / transitions / data）
 *       - 虚表 Ops        LibFsmStateOps_s（entry / exit / handle 三个函数指针）
 *       - 事件 Event      LibFsmStateEvent_t，作为 handle 的参数
 *       - 转移 Transition 由 handle 的返回值表达（return 下一个状态节点）
 *       - 动作 Action     handle 内部直接调用（或表项里的 action）
 *       - 守卫 Guard      handle 内部的 if 判断（或表项里的 guard）
 *       - 进入/退出动作   ops->entry / ops->exit，由框架统一调用
 *       - 用户上下文      LibFsmStateMachine_s.ctx（void *，跨状态共享）
 *       - 状态自己的数据  LibFsmStateNode_s.data（属于状态、不属于状态机，见下方作用域说明）
 *
 * @note 执行顺序（默认模式）
 *       1. 调当前状态的 handle（或默认 handle）拿到 next
 *       2. next 为 NULL = 没人处理；next == 当前状态 = 换了但状态不变（内部转移）
 *       3. 否则：退出旧状态 → 切换 current → 进入新状态
 *       动作写在 handle 里，所以**副作用天然发生在 exit 之前**，真实顺序是
 *           动作 → 退出旧状态 → 进入新状态
 *       （lib_fsm_table 是 UML 的 退出 → 动作 → 进入。）
 *       定义 LIB_FSM_STATE_UML_ORDER 可换成 UML 序：此时默认 handle 不再就地执行表动作，
 *       而是挂起交给 Dispatch 在 exit 之后执行（退出 → 动作 → 进入）。
 *       注意该开关**只作用于表驱动路径**：自己写在 handle 里的动作顺序不变
 *       （框架没法重排你函数体里的语句），外部自转移（LibFsmStateSelfExternal）也遵循同一顺序。
 *
 * @note 派发结局见 LibFsmStateResult_e：SWITCHED / INTERNAL / NOT_HANDLED。
 *       返回值只描述**框架**做了什么（有没有退/进），不表示"动作有没有执行"——
 *       比如表里命中一行并执行了动作但 next 指向自身时，结局是 INTERNAL。
 *
 * @note from == to 一律视为"不切换"：handle 返回当前状态节点时，exit/entry 都不执行
 *       （与 lib_fsm_table 的外部自转移不同）。想要"退一次再进一次"的自转移，
 *       用 LibFsmStateSelfExternal() 当返回值即可。
 *       "状态不变但要响应事件"用"返回自身"即可：动作已执行、exit/entry 不执行（即内部转移）。
 *
 * @note 状态自己的数据 data 的作用域（务必分清）：
 *       - 状态节点是**静态单例**，data 指向的对象是**全局状态**，被所有用同一节点的状态机实例共享
 *       - 每个实例各自的数据请放 machine.ctx；data 只放"该状态独有的常量/工作区"
 *         （例如某个状态自己的计数器、缓冲），且调用方要自己接受"多实例共享"这件事
 *
 * @note 回调契约：
 *       - handle 只做决策：判事件、过守卫、执行动作，最后 return 目标状态（NULL = 不处理）
 *       - exit / entry 由框架调用，handle 里不要自己调（否则顺序容易乱），要自转移请用
 *         LibFsmStateSelfExternal
 *       - handle / entry / exit 内不得再对同一状态机调用 LibFsmStateDispatch：重入一律被挡住
 *         （内层直接返回 NOT_HANDLED，外层派发不受影响；PARANOID 下还会断言报错）
 *
 * @note 本模块不加锁；不含时间源、不含事件队列（与 lib_fsm_table 定位一致：
 *       超时请在外层到点派发 E_TIMEOUT，事件缓冲请自行搭环形队列）。
 *
 * @note 可选字段（error_state / default_state / trace_fn）**由 Init 清零**：
 *       想用它们请在 LibFsmStateInit 之后再赋值，别指望在 Init 之前配就能留下。
 *       - error_state：handle 返回的指针"看起来不是状态节点"时的落点（弱校验，见 .c）
 *       - default_state：整机兜底——当前状态不处理该事件时切到这里
 *       - trace_fn：每次派发结束回调一次（三种结局都会回调），用于日志/可视化/功耗统计
 *
 * @note 编译开关（固件里在 app_cfg.h 定义，PC 检验用命令行 -D；同一工程内必须一致）：
 *       - LIB_FSM_STATE_USED：打开模块实现（不开则 .c 是空翻译单元，RAM/FLASH 零占用）
 *       - LIB_FSM_STATE_STANDALONE：PC 端跳过 app_cfg.h
 *       - LIB_FSM_STATE_PARANOID：打开运行时自检断言（状态节点/转移表一致性、重入、非法表行），
 *         需要 <assert.h>，只在调试期开（断言失败会 abort，所以别在实机长期开着）
 *       - LIB_FSM_STATE_TRACE：打开内置调试打印（printf 每次派发一行），仅调试用
 *       - LIB_FSM_STATE_UML_ORDER：把表驱动的转移动作改到 exit 之后执行（UML 序）
 *       本文件仅依赖 <stdbool.h>/<stddef.h>/<stdint.h>。
 */

#ifndef __LIB_FSM_STATE_H
#define __LIB_FSM_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*============================================
 *              类型定义
 *============================================*/

/* 互相引用（handle 返回状态节点、回调收状态机指针），先做前置声明 */
typedef struct LibFsmStateNode_s LibFsmStateNode_s;
typedef struct LibFsmStateMachine_s LibFsmStateMachine_s;

/** @brief 事件值：直接用用户 enum，框架按 int32_t 存取 */
typedef int32_t LibFsmStateEvent_t;

/** @brief 通配事件：任何事件都命中；INT32_MIN 被框架保留，业务事件别取这个值 */
#define LIB_FSM_STATE_EVENT_ANY ((LibFsmStateEvent_t)INT32_MIN)

_Static_assert(sizeof(LibFsmStateEvent_t) == sizeof(int32_t), "LibFsmStateEvent_t 必须是 int32_t");

/**
 * @brief 一次派发的结局（为什么不是 bool：见文件头"派发结局"）
 * @note "返回自身"与"没人处理"都不是切换，但前者表示事件被处理了，只有枚举能区分
 */
typedef enum : uint8_t
{
    LIB_FSM_STATE_RESULT_NOT_HANDLED = 0, // 没人处理：handle 返回 NULL，且没配 default_state
    LIB_FSM_STATE_RESULT_INTERNAL = 1,    // 处理了、但没切换（返回自身；动作已执行）
    LIB_FSM_STATE_RESULT_SWITCHED = 2,    // 发生了退出/进入（含外部自转移）
} LibFsmStateResult_e;

/**
 * @brief 进入/退出动作：绑定在状态上，由框架调用
 * @param fsm 状态机指针（用 fsm->ctx 取用户上下文）
 */
typedef void (*LibFsmStateHook_fn)(LibFsmStateMachine_s *fsm);

/**
 * @brief 守卫：返回 true 才允许这条转移（表项里用）
 * @param fsm        状态机指针
 * @param event_data 本次事件参数，可能为 NULL
 * @note 无副作用、幂等：一次派发里可能被多条候选表项依次调用
 */
typedef bool (*LibFsmStateGuard_fn)(LibFsmStateMachine_s *fsm, const void *event_data);

/**
 * @brief 转移动作：发生转移时执行（表项里用）
 * @param fsm        状态机指针
 * @param event_data 本次事件参数，可能为 NULL
 */
typedef void (*LibFsmStateAction_fn)(LibFsmStateMachine_s *fsm, const void *event_data);

/**
 * @brief 状态处理函数：决定这次事件要不要切换、切到哪
 * @param fsm        状态机指针
 * @param event      本次事件
 * @param event_data 本次事件参数，可能为 NULL
 * @return 目标状态节点；返回 NULL 表示"不处理"、返回当前状态本身表示"处理了但不切换"
 *         （要外部自转移就返回 LibFsmStateSelfExternal()）
 * @note 判事件、过守卫、执行动作都在这里面；框架只负责按顺序调 exit / entry
 */
typedef const LibFsmStateNode_s *(*LibFsmStateHandle_fn)(LibFsmStateMachine_s *fsm, LibFsmStateEvent_t event, const void *event_data);

/**
 * @brief 跟踪钩子：每次派发结束调用一次（三种结局都调）
 * @param fsm    状态机指针
 * @param from   派发前的状态节点
 * @param to     派发后所在的状态节点（没切换、没人处理时与 from 相同；外部自转移也相同）
 * @param event  本次事件
 * @param result 本次结局
 * @note 跑在派发末尾、dispatching 标志已放下之后；里面不要再去派发同一状态机
 */
typedef void (*LibFsmStateTrace_fn)(LibFsmStateMachine_s *fsm, const LibFsmStateNode_s *from, const LibFsmStateNode_s *to, LibFsmStateEvent_t event,
                                    LibFsmStateResult_e result);

/** @brief 状态的虚表：进入 / 退出 / 处理事件 */
typedef struct
{
    LibFsmStateHook_fn entry;    // 进入该状态时调用，可为 NULL
    LibFsmStateHook_fn exit;     // 退出该状态时调用，可为 NULL
    LibFsmStateHandle_fn handle; // 处理事件，可为 NULL（此时若有转移表则自动用默认 handle）
} LibFsmStateOps_s;

/**
 * @brief 状态内转移表的一行（只在"用默认 handle"的状态里需要写）
 * @note next 必须是有效节点：**不切换请写该状态自身**（= 内部转移）；
 *       写成 NULL 视为配置错误，这一行会被跳过（PARANOID 下断言报错）
 */
typedef struct
{
    LibFsmStateEvent_t event;      // 触发事件；LIB_FSM_STATE_EVENT_ANY = 任何事件
    LibFsmStateGuard_fn guard;     // 守卫，NULL = 恒真
    LibFsmStateAction_fn action;   // 转移动作，NULL = 无动作
    const LibFsmStateNode_s *next; // 目标状态，不可为 NULL（写自身即内部转移）
} LibFsmStateTransition_s;

/**
 * @brief 状态对象：一般定义为 static const 单例，全局只有一份
 * @note ops 与 transitions 至少得有一个（否则这个状态既不处理事件也不是有效目标）
 */
struct LibFsmStateNode_s
{
    const char *name;                           // 状态名，调试/日志用，可为 NULL
    const LibFsmStateOps_s *ops;                // 虚表，可为 NULL（此时必须靠 transitions）
    const LibFsmStateTransition_s *transitions; // 状态内转移表，可为 NULL（自己写 handle 时不需要）
    uint32_t transition_count;                  // 转移表条数
    void *data;                                 // 该状态自己的数据（全局状态，多实例共享，见文件头）
};

/** @brief 状态机实例：一个实例一份 current + ctx；状态节点可被多个实例共享 */
struct LibFsmStateMachine_s
{
    const LibFsmStateNode_s *current; // 当前状态节点
    void *ctx;                        // 用户上下文，回调时通过 fsm->ctx 取

    /* 以下三个可选字段由 LibFsmStateInit 清零，请在 Init 之后再配 */
    const LibFsmStateNode_s *error_state;   // handle 返回非法目标时的落点，NULL = 不纠错
    const LibFsmStateNode_s *default_state; // 当前状态不处理事件时的兜底，NULL = 不兜底
    LibFsmStateTrace_fn trace_fn;           // 每次派发结束回调一次，NULL = 不跟踪

    uint8_t dispatching; // 正在派发中：用来挡住回调里的重入派发（PARANOID 下还会断言），
                         // UML 模式也用它判断表动作该不该挂起

#ifdef LIB_FSM_STATE_UML_ORDER
    LibFsmStateAction_fn pending_action; // UML 模式：默认 handle 挂起、交给 Dispatch 执行的动作
#endif
};

/** @brief 转移表静态断言：非空 + 元素类型正确（写在表定义之后，编译期拦住抄错类型/空表） */
#define LIB_FSM_STATE_CHECK_TABLE(tbl)                                                                                                                         \
    _Static_assert(sizeof(tbl) / sizeof((tbl)[0]) > 0u, "转移表不能为空");                                                                                     \
    _Static_assert(sizeof((tbl)[0]) == sizeof(LibFsmStateTransition_s), "转移表元素类型必须是 "                                                                \
                                                                        "LibFsmStateTransition_s")

/*============================================
 *              外部接口声明
 *============================================*/

/**
 * @brief 初始化状态机：落到初始状态并执行其 entry
 * @param fsm     状态机实例指针（**必须**是清零过的对象或已 Init 过的：本函数会读 fsm->current）
 * @param initial 初始状态节点指针，可为 NULL（表示"停在无状态"，此时只清状态机不跑任何钩子）
 * @param ctx     用户上下文，可为 NULL
 *
 * @note 会调用初始状态的 entry（与 lib_fsm_table 不同，本方案的既有语义）。
 * @note **对已初始化的状态机再次调用 = 复位**：先让旧状态退出（用旧 ctx），再重新落点，
 *       避免旧 entry 申请的资源泄漏。初始化本身不是一次转移，故只补 exit、不补 entry。
 * @note error_state / default_state / trace_fn 三个可选字段会被清零，请在 Init 之后配置。
 * @note initial 为 NULL 时 fsm->current 被写成 NULL（不会留下不可判定的旧值），
 *       之后 Dispatch 一律返回 NOT_HANDLED。
 * @note fsm 为 NULL 时直接返回。
 */
void LibFsmStateInit(LibFsmStateMachine_s *fsm, const LibFsmStateNode_s *initial, void *ctx);

/**
 * @brief 派发一个事件：当前状态的 handle 决定切不切、切到哪
 * @param fsm        状态机实例指针
 * @param event      事件值
 * @param event_data 事件参数，可为 NULL
 * @return 本次结局（SWITCHED / INTERNAL / NOT_HANDLED）
 *
 * @note 顺序与"返回自身不算切换"的语义见文件头；返回 INTERNAL / NOT_HANDLED 时
 *       **不保证没有副作用**——动作是否执行由你的 handle（或命中的表项）决定。
 * @note fsm 为 NULL、current 为 NULL（尚未初始化）时返回 NOT_HANDLED；
 *       当前状态既没有 handle 也没有转移表时按"没人处理"走（配了 default_state 就切过去）。
 * @note 配了 default_state 时，handle 返回 NULL（没人处理）会切到兜底状态，
 *       结局是 SWITCHED；"想吞掉事件别乱跑"请让 handle 返回自身而不是 NULL。
 * @note 配了 error_state 时，handle 返回的指针若"不像状态节点"（既没有虚表也没有非空转移表，
 *       见 .c 里的弱校验）会改落到它，避免野指针直接把 entry/exit 打崩。
 * @note 重入（回调里再派发同一状态机）一律被挡住：内层直接返回 NOT_HANDLED，不破坏外层派发；
 *       PARANOID 下还会断言报错（断言会 abort，所以只当调试用）。
 */
LibFsmStateResult_e LibFsmStateDispatch(LibFsmStateMachine_s *fsm, LibFsmStateEvent_t event, const void *event_data);

/**
 * @brief 读取当前状态节点
 * @param fsm 状态机实例指针
 * @return 当前状态节点指针；fsm 为 NULL 或尚未初始化时返回 NULL
 */
const LibFsmStateNode_s *LibFsmStateCurrent(const LibFsmStateMachine_s *fsm);

/**
 * @brief 当前状态是否就是 node（调用方最常见的判断，省得写两行 NULL 检查）
 * @param fsm  状态机实例指针
 * @param node 要比对的状态节点
 * @return true 表示 fsm 已初始化且当前状态正是 node
 */
bool LibFsmStateIsIn(const LibFsmStateMachine_s *fsm, const LibFsmStateNode_s *node);

/**
 * @brief 取当前状态名
 * @param fsm 状态机实例指针
 * @return 当前状态的 name；fsm 为 NULL、未初始化或该状态没写 name 时返回 NULL
 */
const char *LibFsmStateCurrentName(const LibFsmStateMachine_s *fsm);

/**
 * @brief 外部自转移标记：退出当前状态再重新进入它（与"返回自身 = 不退不进"区分开）
 * @return 只能当**返回值**用：`return LibFsmStateSelfExternal();`
 * @note 它不是状态节点，拿到手别解引用、别存起来、别跟真实节点比大小；
 *       框架识别到这个标记后按"退出旧状态 →（表动作）→ 再进入同一个状态"处理，结局是 SWITCHED。
 */
const LibFsmStateNode_s *LibFsmStateSelfExternal(void);

/**
 * @brief 通用默认 handle：在"当前状态"的转移表里查第一条命中的行
 * @param fsm        状态机实例指针
 * @param event      事件值
 * @param event_data 事件参数，可为 NULL
 * @return 该行的 next；没有命中的行返回 NULL
 *
 * @note 查表取"当前状态"（fsm->current）自己的 transitions，按数组顺序求值：
 *       事件对上（含 LIB_FSM_STATE_EVENT_ANY）且守卫通过的第一条生效，后面的不再看
 *       （表顺序即优先级，把通配行写在后面即可当兜底）。
 * @note next 为 NULL 的行视为配置错误，直接跳过（不执行它的 action）。
 * @note 命中时会**就地执行该行的 action**，所以它既是决策也是执行；
 *       开了 LIB_FSM_STATE_UML_ORDER 且当前正在 Dispatch 里时改为挂起、由 Dispatch 在 exit 后执行；
 *       想把动作留给别处，请自己写 handle。
 * @note 用法有两种（效果一致）：
 *       1. 状态的 ops->handle 留空、transitions 非空 → 框架自动回退到本函数
 *       2. 状态的 ops->handle 显式写成 LibFsmStateDefaultHandle
 */
const LibFsmStateNode_s *LibFsmStateDefaultHandle(LibFsmStateMachine_s *fsm, LibFsmStateEvent_t event, const void *event_data);

#endif /* __LIB_FSM_STATE_H */
