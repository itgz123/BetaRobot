/**
 * @file test_fsm_state.c
 * @brief lib_fsm_state PC 端检验（派发结局 / 复位语义 / 外部自转移 / 状态内转移表 / 通配事件 /
 *        兜底与纠错 / 跟踪钩子 / 状态数据共享 / 重入），退出码 0 = PASS
 *
 * @note 用 tools/test_fsm_state.sh 编译运行（gcc，-LIB_FSM_STATE_STANDALONE 跳过 app_cfg.h）。
 *       脚本会用两套开关各跑一遍：默认一套，以及 PARANOID + UML_ORDER + TRACE 全开一套；
 *       轨迹断言里凡涉及**表动作**顺序的地方都用 ORD(...) 包起来，两套都成立。
 * @note 检验项：
 *       1. 初始化：落初始状态并执行其 entry；Init 会清零三个可选字段；ctx 原样保存；name 可读
 *       2. 重复 Init = 复位：先跑旧状态的 exit（旧 ctx），再落点跑新状态 entry；Init 不跑任何 entry 之外的转移
 *       3. initial 为 NULL：旧状态退出后停在"无状态"，Dispatch 一律 NOT_HANDLED、无副作用
 *       4. 手写 handle：判事件、过守卫、执行动作、return 目标状态
 *       5. 派发结局三分：SWITCHED / INTERNAL（返回自身，动作已执行）/ NOT_HANDLED（没人处理）
 *       6. 执行顺序：默认 动作 → 退出 → 进入；UML_ORDER 下（表驱动）退出 → 动作 → 进入
 *       7. 外部自转移（LibFsmStateSelfExternal）：退出再进入自身，from == to 但结局是 SWITCHED
 *       8. 守卫拦截：不切换、不退出、不进入
 *       9. 状态内转移表：首条命中即停、守卫拒绝落到下一行、不匹配的事件完全不响应、
 *          next 为 NULL 的错误行整行跳过（动作也不执行）
 *       10. 显式挂 LibFsmStateDefaultHandle 与自动回退等价
 *       11. 通配事件 LIB_FSM_STATE_EVENT_ANY：写在后面当兜底，具体事件仍优先
 *       12. default_state：没人处理时兜底；兜底就是自身时视为不切换
 *       13. error_state：handle 返回不像状态节点的指针时改落到它；没配就当作没处理
 *       14. 跟踪钩子：三种结局都回调，from/to/event/result 都对
 *       15. 空指针：fsm / current / handle / 未初始化状态机的组合都不崩
 *       16. event_data：NULL 与结构体指针都能穿透到 handle/guard/action
 *       17. 状态自己的数据：data 属于静态单例（多实例共享），ctx 属于各实例
 *       18. 重入：回调里再派发同一状态机被挡住，且不影响外层派发（PARANOID 下断言会拦住，改用编译期跳过）
 */

#include <stdio.h>
#include <string.h>

#include "lib_fsm_state.h"

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond, ...)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            g_fails++;                                                                                                 \
            printf("FAIL %d: ", __LINE__);                                                                             \
            printf(__VA_ARGS__);                                                                                       \
            printf("\n");                                                                                              \
        }                                                                                                              \
    } while (0)

/*============================ 轨迹串：记录回调被调用的顺序 ============================*/
/* entry/exit 用大小写字母，动作/守卫分支用数字与符号；表动作的顺序见 ORD(...) */

static char g_trace[128];
static int g_trace_len;

static void TraceReset(void)
{
    g_trace[0] = '\0';
    g_trace_len = 0;
}

static void TraceAdd(char c)
{
    if (g_trace_len < (int)sizeof(g_trace) - 1)
    {
        g_trace[g_trace_len++] = c;
        g_trace[g_trace_len] = '\0';
    }
}

#define CHECK_TRACE(expect) CHECK(strcmp(g_trace, (expect)) == 0, "轨迹串 = \"%s\"，期望 \"%s\"", g_trace, (expect))

/*
 * 表驱动的转移动作落在 exit 的前面还是后面，由编译开关决定：
 *   默认（动作写在 handle 里）     动作 → 退出 → 进入
 *   LIB_FSM_STATE_UML_ORDER        退出 → 动作 → 进入
 * 相邻的字符串字面量会自动拼接，所以 ORD("5", "w", "O") 在两种模式下分别是 "5wO" 与 "w5O"。
 */
#ifdef LIB_FSM_STATE_UML_ORDER
#define ORD(action, exit_letter, entry_letter) exit_letter action entry_letter
#else
#define ORD(action, exit_letter, entry_letter) action exit_letter entry_letter
#endif

/*============================ 被测状态机的业务定义 ============================*/

typedef enum : uint8_t
{
    E_TOGGLE = 0, /* 手写 handle 的状态用 */
    E_FAULT,
    E_RESET,
    E_TICK, /* 表驱动状态用 */
    E_RESTART,
    E_BAD,
    E_REENTER,
} test_event_e;

/** @brief 事件载荷 */
typedef struct
{
    int code;
} test_ev_s;

/** @brief 用户上下文：跨状态共享，属于"实例" */
typedef struct
{
    int limit;       // 守卫阈值
    int toggle_cnt;  // 手写 handle 的动作计数
    int tick_cnt;    // 表驱动状态的内部转移计数
    int last_code;   // 动作读到的载荷（event_data 为 NULL 记 -1）
    int guard_calls; // 表守卫调用次数
} test_ctx_s;

/** @brief 状态自己的数据：放在静态单例的 data 上，属于"状态"（多实例共享） */
typedef struct
{
    int enter_cnt;
    int exit_cnt;
    int tick_cnt;
} node_data_s;

static node_data_s s_off_data;
static node_data_s s_wait_data;
static node_data_s s_wait2_data;

/** @brief 重入用例：内层派发的返回值（-1 = 没调过） */
static int g_reenter_result = -1;

/*------------- 状态节点前置声明（handle 要返回节点地址，互相引用） -------------*/

static const LibFsmStateNode_s n_off;
static const LibFsmStateNode_s n_on;
static const LibFsmStateNode_s n_err;
static const LibFsmStateNode_s n_wait;
static const LibFsmStateNode_s n_wait2;
static const LibFsmStateNode_s n_any;
static const LibFsmStateNode_s n_silent;
static const LibFsmStateNode_s n_bogus; /* 故意"不像状态节点"：既没虚表也没转移表 */

/*------------- OFF：手写 handle（带守卫、外部自转移、非法目标） -------------*/

static void OffEntry(LibFsmStateMachine_s *fsm)
{
    ((node_data_s *)fsm->current->data)->enter_cnt++;
    TraceAdd('O');
}

static void OffExit(LibFsmStateMachine_s *fsm)
{
    ((node_data_s *)fsm->current->data)->exit_cnt++;
    TraceAdd('o');
}

static const LibFsmStateNode_s *OffHandle(LibFsmStateMachine_s *fsm, LibFsmStateEvent_t event, const void *event_data)
{
    test_ctx_s *ctx = fsm->ctx;
    const test_ev_s *ev = event_data;

    if (event == E_TOGGLE)
    {
        /* 守卫：切换次数到顶就拦下（返回 NULL = 不处理，不切换也不退出/进入） */
        if (ctx->toggle_cnt >= ctx->limit)
        {
            TraceAdd('x');
            return NULL;
        }

        ctx->toggle_cnt++;
        ctx->last_code = (ev != NULL) ? ev->code : -1;
        TraceAdd('1');
        return &n_on;
    }
    if (event == E_FAULT)
    {
        TraceAdd('f');
        return &n_err;
    }
    if (event == E_RESTART)
    {
        TraceAdd('r');
        return LibFsmStateSelfExternal(); /* 外部自转移：退出再进入 OFF 自己 */
    }
    if (event == E_BAD)
    {
        TraceAdd('b');
        return &n_bogus; /* 非法目标：框架会落到 error_state */
    }
    if (event == E_REENTER)
    {
        TraceAdd('R');
        g_reenter_result = (int)LibFsmStateDispatch(fsm, E_TOGGLE, NULL); /* 重入：必须被挡住 */
        TraceAdd('r');
        return NULL;
    }

    return NULL;
}

/*------------- ON：手写 handle -------------*/

static void OnEntry(LibFsmStateMachine_s *fsm)
{
    (void)fsm;
    TraceAdd('N');
}

static void OnExit(LibFsmStateMachine_s *fsm)
{
    (void)fsm;
    TraceAdd('n');
}

static const LibFsmStateNode_s *OnHandle(LibFsmStateMachine_s *fsm, LibFsmStateEvent_t event, const void *event_data)
{
    test_ctx_s *ctx = fsm->ctx;
    (void)event_data;

    if (event == E_TOGGLE)
    {
        ctx->toggle_cnt++;
        TraceAdd('2');
        return &n_off;
    }

    return NULL;
}

/*------------- ERR：手写 handle，含"返回自身"的内部转移 -------------*/

static void ErrEntry(LibFsmStateMachine_s *fsm)
{
    (void)fsm;
    TraceAdd('E');
}

static void ErrExit(LibFsmStateMachine_s *fsm)
{
    (void)fsm;
    TraceAdd('e');
}

static const LibFsmStateNode_s *ErrHandle(LibFsmStateMachine_s *fsm, LibFsmStateEvent_t event, const void *event_data)
{
    (void)fsm;
    (void)event_data;

    if (event == E_TICK)
    {
        TraceAdd('t');
        return &n_err; /* 返回自身：动作已做，但不切换（结局 INTERNAL） */
    }
    if (event == E_RESET)
    {
        TraceAdd('3');
        return &n_off;
    }
    if (event == E_TOGGLE)
    {
        TraceAdd('4');
        return &n_wait; /* 混合：切到表驱动状态 */
    }

    return NULL;
}

/*------------- 表驱动状态用的钩子与动作 -------------*/

static void WaitEntry(LibFsmStateMachine_s *fsm)
{
    (void)fsm;
    TraceAdd('W');
}

static void WaitExit(LibFsmStateMachine_s *fsm)
{
    (void)fsm;
    TraceAdd('w');
}

static void ActTableReset(LibFsmStateMachine_s *fsm, const void *event_data)
{
    test_ctx_s *ctx = fsm->ctx;
    const test_ev_s *ev = event_data;

    ctx->last_code = (ev != NULL) ? ev->code : -1;
    TraceAdd('5');
}

static void ActTableTick(LibFsmStateMachine_s *fsm, const void *event_data)
{
    test_ctx_s *ctx = fsm->ctx;
    node_data_s *data = fsm->current->data; /* 用 fsm->current 取"当前状态自己的数据" */

    (void)event_data;
    ctx->tick_cnt++;
    data->tick_cnt++;
    TraceAdd('6');
}

static void ActTableTickOverflow(LibFsmStateMachine_s *fsm, const void *event_data)
{
    (void)fsm;
    (void)event_data;
    TraceAdd('7');
}

static void ActTableFault(LibFsmStateMachine_s *fsm, const void *event_data)
{
    (void)fsm;
    (void)event_data;
    TraceAdd('8');
}

static void ActWildcard(LibFsmStateMachine_s *fsm, const void *event_data)
{
    (void)fsm;
    (void)event_data;
    TraceAdd('9');
}

/** @brief 表守卫：tick 未到限才允许"返回自身"那条 */
static bool GuardTickLimited(LibFsmStateMachine_s *fsm, const void *event_data)
{
    test_ctx_s *ctx = fsm->ctx;

    (void)event_data;
    ctx->guard_calls++;
    return ctx->tick_cnt < ctx->limit;
}

/*
 * n_wait 的表：表顺序即优先级
 *   第 0 行（仅非 PARANOID 构建）故意把 next 写成 NULL —— 配置错误行，必须整行跳过，
 *   ActShouldNotRun 一旦被调用就会往轨迹里塞 '!'，下面的轨迹断言立刻失败
 *   第 2/3 行事件相同，靠守卫分叉；第 2 行 next 写自身 = 内部转移
 */
#ifndef LIB_FSM_STATE_PARANOID
/* PARANOID 下那一行会被断言拦下，所以动作和表项一起不编译，免得留下未使用的函数 */
static void ActShouldNotRun(LibFsmStateMachine_s *fsm, const void *event_data)
{
    (void)fsm;
    (void)event_data;
    TraceAdd('!'); /* 出现了就说明错误行没被跳过 */
}
#endif

static const LibFsmStateTransition_s s_wait_table[] = {
#ifndef LIB_FSM_STATE_PARANOID
    {E_RESET, NULL, ActShouldNotRun, NULL}, /* 配置错误：next 为 NULL */
#endif
    {E_RESET, NULL, ActTableReset, &n_off},
    {E_TICK, GuardTickLimited, ActTableTick, &n_wait}, /* 返回自身 = 内部转移 */
    {E_TICK, NULL, ActTableTickOverflow, &n_err},      /* 守卫拒绝时落到这条 */
    {E_FAULT, NULL, ActTableFault, &n_err},
};

#define WAIT_TABLE_N ((uint32_t)(sizeof(s_wait_table) / sizeof(s_wait_table[0])))

LIB_FSM_STATE_CHECK_TABLE(s_wait_table);

/* 与 s_wait_table 同构，只是"返回自身"那行指向 n_wait2：
 * 表项的 next 是一个**具体节点**，所以两个节点共用一张表时，"自身"那行只对当初写表时那个节点成立 */
static const LibFsmStateTransition_s s_wait2_table[] = {
    {E_RESET, NULL, ActTableReset, &n_off},
    {E_TICK, GuardTickLimited, ActTableTick, &n_wait2},
    {E_TICK, NULL, ActTableTickOverflow, &n_err},
    {E_FAULT, NULL, ActTableFault, &n_err},
};

/* 通配事件：具体事件写在前面，通配行写在最后当兜底 */
static const LibFsmStateTransition_s s_any_table[] = {
    {E_RESET, NULL, ActTableReset, &n_off},
    {LIB_FSM_STATE_EVENT_ANY, NULL, ActWildcard, &n_err},
};

#define ANY_TABLE_N ((uint32_t)(sizeof(s_any_table) / sizeof(s_any_table[0])))

LIB_FSM_STATE_CHECK_TABLE(s_any_table);

/*------------- 前两个状态节点也要能用表（n_any） -------------*/

static void AnyEntry(LibFsmStateMachine_s *fsm)
{
    (void)fsm;
    TraceAdd('A');
}

static void AnyExit(LibFsmStateMachine_s *fsm)
{
    (void)fsm;
    TraceAdd('a');
}

/*------------- 虚表与节点定义 -------------*/

static const LibFsmStateOps_s s_off_ops = {.entry = OffEntry, .exit = OffExit, .handle = OffHandle};
static const LibFsmStateOps_s s_on_ops = {.entry = OnEntry, .exit = OnExit, .handle = OnHandle};
static const LibFsmStateOps_s s_err_ops = {.entry = ErrEntry, .exit = ErrExit, .handle = ErrHandle};
static const LibFsmStateOps_s s_wait_ops = {.entry = WaitEntry, .exit = WaitExit, .handle = NULL};
/* 显式挂默认 handle：与"handle 留空、靠自动回退"等价 */
static const LibFsmStateOps_s s_wait2_ops = {.entry = WaitEntry, .exit = WaitExit, .handle = LibFsmStateDefaultHandle};
static const LibFsmStateOps_s s_any_ops = {.entry = AnyEntry, .exit = AnyExit, .handle = NULL};
/* 什么都不做的状态：没有 handle、也没有转移表（作为"死状态"落点是合法的，因为 ops 非 NULL） */
static const LibFsmStateOps_s s_silent_ops = {.entry = NULL, .exit = NULL, .handle = NULL};

static const LibFsmStateNode_s n_off = {
    .name = "OFF", .ops = &s_off_ops, .transitions = NULL, .transition_count = 0u, .data = &s_off_data};
static const LibFsmStateNode_s n_on = {
    .name = "ON", .ops = &s_on_ops, .transitions = NULL, .transition_count = 0u, .data = NULL};
static const LibFsmStateNode_s n_err = {
    .name = "ERR", .ops = &s_err_ops, .transitions = NULL, .transition_count = 0u, .data = NULL};
/* 表驱动：handle 留空 → 框架自动回退 LibFsmStateDefaultHandle */
static const LibFsmStateNode_s n_wait = {.name = "WAIT",
                                         .ops = &s_wait_ops,
                                         .transitions = s_wait_table,
                                         .transition_count = WAIT_TABLE_N,
                                         .data = &s_wait_data};
/* 同构的表 + 显式 handle：表动作会访问 fsm->current->data，所以这个节点也要自带 data */
static const LibFsmStateNode_s n_wait2 = {.name = "WAIT2",
                                          .ops = &s_wait2_ops,
                                          .transitions = s_wait2_table,
                                          .transition_count = WAIT_TABLE_N,
                                          .data = &s_wait2_data};
static const LibFsmStateNode_s n_any = {
    .name = "ANY", .ops = &s_any_ops, .transitions = s_any_table, .transition_count = ANY_TABLE_N, .data = NULL};
static const LibFsmStateNode_s n_silent = {
    .name = "SILENT", .ops = &s_silent_ops, .transitions = NULL, .transition_count = 0u, .data = NULL};
/* 没有虚表也没有转移表：弱校验会判定它"不像状态节点" */
static const LibFsmStateNode_s n_bogus = {
    .name = "BOGUS", .ops = NULL, .transitions = NULL, .transition_count = 0u, .data = NULL};

/*============================ 跟踪钩子：记录每次派发的结局 ============================*/

typedef struct
{
    const LibFsmStateNode_s *from;
    const LibFsmStateNode_s *to;
    LibFsmStateEvent_t event;
    LibFsmStateResult_e result;
} trace_rec_s;

#define TRACE_MAX 16
static trace_rec_s g_recs[TRACE_MAX];
static int g_rec_n;

static void TraceHook(LibFsmStateMachine_s *fsm, const LibFsmStateNode_s *from, const LibFsmStateNode_s *to,
                      LibFsmStateEvent_t event, LibFsmStateResult_e result)
{
    (void)fsm;
    if (g_rec_n < TRACE_MAX)
    {
        g_recs[g_rec_n].from = from;
        g_recs[g_rec_n].to = to;
        g_recs[g_rec_n].event = event;
        g_recs[g_rec_n].result = result;
        g_rec_n++;
    }
}

/** @brief 落点 + 清轨迹（Init 会跑旧状态的 exit 与新状态的 entry，用例只关心这之后的轨迹） */
static void InitFresh(LibFsmStateMachine_s *fsm, const LibFsmStateNode_s *initial, void *ctx)
{
    LibFsmStateInit(fsm, initial, ctx);
    TraceReset();
}

/*============================ 1、2、3、14（Init 部分） ============================*/

static void TestInit(void)
{
    LibFsmStateMachine_s fsm = {0};
    test_ctx_s ctx = {0};

    s_off_data.enter_cnt = 0;
    s_off_data.exit_cnt = 0;

    TraceReset();
    LibFsmStateInit(&fsm, &n_off, &ctx);
    CHECK(LibFsmStateCurrent(&fsm) == &n_off, "初始状态应为 OFF");
    CHECK_TRACE("O"); /* 本方案的 Init 会跑初始状态的 entry（与 lib_fsm_table 不同） */
    CHECK(s_off_data.enter_cnt == 1, "OFF 的 enter 次数 = %d，期望 1", s_off_data.enter_cnt);
    CHECK(s_off_data.exit_cnt == 0, "Init 不是转移，不该调 exit，实际 %d 次", s_off_data.exit_cnt);
    CHECK(fsm.ctx == &ctx, "ctx 应原样存进状态机");
    CHECK(LibFsmStateIsIn(&fsm, &n_off), "IsIn 应认得出当前状态");
    CHECK(LibFsmStateCurrentName(&fsm) != NULL && strcmp(LibFsmStateCurrentName(&fsm), "OFF") == 0,
          "当前状态名 = %s，期望 OFF", LibFsmStateCurrentName(&fsm));
    CHECK(fsm.error_state == NULL && fsm.default_state == NULL && fsm.trace_fn == NULL, "Init 后可选字段应为空");

    /* 重复 Init = 复位：先退出旧状态（用旧 ctx），再落点跑新状态 entry */
    TraceReset();
    LibFsmStateInit(&fsm, &n_on, &ctx);
    CHECK(LibFsmStateCurrent(&fsm) == &n_on, "重新 Init 后应落到 ON");
    CHECK_TRACE("oN");
    CHECK(s_off_data.exit_cnt == 1, "重复 Init 应补一次旧状态的 exit（否则 entry 申请的资源会泄漏），实际 %d 次",
          s_off_data.exit_cnt);
    CHECK(s_off_data.enter_cnt == 1, "重复 Init 不该重跑旧状态的 entry，实际 %d 次", s_off_data.enter_cnt);

    /* 可选字段会被清零：配在 Init 之前是留不住的 */
    fsm.error_state = &n_err;
    fsm.default_state = &n_on;
    fsm.trace_fn = TraceHook;
    LibFsmStateInit(&fsm, &n_on, &ctx);
    CHECK(fsm.error_state == NULL && fsm.default_state == NULL && fsm.trace_fn == NULL,
          "Init 应把 error_state / default_state / trace_fn 清零");

    /* initial 为 NULL：退出旧状态 + 停在"无状态" */
    TraceReset();
    LibFsmStateInit(&fsm, NULL, &ctx);
    CHECK(LibFsmStateCurrent(&fsm) == NULL, "initial 为 NULL 时 current 应被写成 NULL");
    CHECK(LibFsmStateCurrentName(&fsm) == NULL, "无状态时状态名应为 NULL");
    CHECK(LibFsmStateIsIn(&fsm, NULL) == false, "IsIn(fsm, NULL) 应为 false");
    CHECK_TRACE("n"); /* 只退出旧状态 */
    CHECK(fsm.ctx == &ctx, "ctx 仍应被写入");
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_TOGGLE, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED, "无状态时一律 NOT_HANDLED");
    CHECK(g_trace_len == 0, "无状态时不得调用任何回调，轨迹 = \"%s\"", g_trace);

    /* 重新落回来，顺便验证 Init(NULL, ...) 之后还能正常用 */
    InitFresh(&fsm, &n_off, &ctx);
    CHECK(LibFsmStateIsIn(&fsm, &n_off), "重新 Init 应恢复");

    LibFsmStateInit(NULL, &n_on, &ctx); /* fsm 为 NULL：只要求不崩 */
}

/*============================ 4、5、6、8 ============================*/

static void TestManualDispatch(void)
{
    LibFsmStateMachine_s fsm = {0};
    test_ctx_s ctx = {0};
    test_ev_s ev = {.code = 42};

    ctx.limit = 3;
    InitFresh(&fsm, &n_off, &ctx);

    /* 动作写在 handle 里 → 顺序是 动作 → 退出旧状态 → 进入新状态（与 UML 开关无关） */
    CHECK(LibFsmStateDispatch(&fsm, E_TOGGLE, &ev) == LIB_FSM_STATE_RESULT_SWITCHED, "OFF 下 E_TOGGLE 应切到 ON");
    CHECK(LibFsmStateIsIn(&fsm, &n_on), "状态应为 ON");
    CHECK_TRACE("1oN");
    CHECK(ctx.last_code == 42, "handle 读到的载荷 = %d，期望 42", ctx.last_code);
    CHECK(ctx.toggle_cnt == 1, "toggle 计数 = %d，期望 1", ctx.toggle_cnt);

    /* 未知事件：handle 返回 NULL → NOT_HANDLED，没有任何副作用 */
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED, "ON 下 E_TICK 无人处理");
    CHECK(LibFsmStateIsIn(&fsm, &n_on), "未处理时状态不应改变");
    CHECK(g_trace_len == 0, "未处理时不得调用任何回调，轨迹 = \"%s\"", g_trace);

    /* 切回 OFF */
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_TOGGLE, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "ON 下 E_TOGGLE 应切回 OFF");
    CHECK_TRACE("2nO");
    CHECK(ctx.last_code == 42, "event_data 为 NULL 的这次不应改动 last_code");

    /* 守卫拦截：不切换、不退出、不进入（只有 handle 里那句 'x' 留痕） */
    ctx.toggle_cnt = ctx.limit;
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_TOGGLE, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED, "守卫拦截时不应切换");
    CHECK(LibFsmStateIsIn(&fsm, &n_off), "守卫拦截时状态不应改变");
    CHECK_TRACE("x");

    /* ERR 的"返回自身"：动作执行（'t'）、结局是 INTERNAL（不是 NOT_HANDLED），不退不进 */
    ctx.toggle_cnt = 0;
    InitFresh(&fsm, &n_err, &ctx);
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_INTERNAL, "返回自身是 INTERNAL");
    CHECK(LibFsmStateIsIn(&fsm, &n_err), "返回自身时状态不变");
    CHECK_TRACE("t"); /* 动作留下痕迹，但没有任何 entry/exit */

    /* ERR --E_RESET--> OFF（普通外部转移，动作 '3' 先于 exit 'e'） */
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_RESET, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "ERR 下 E_RESET 应切到 OFF");
    CHECK_TRACE("3eO");
}

/*============================ 7：外部自转移 ============================*/

static void TestSelfExternal(void)
{
    LibFsmStateMachine_s fsm = {0};
    test_ctx_s ctx = {0};

    ctx.limit = 3;
    InitFresh(&fsm, &n_off, &ctx);
    s_off_data.exit_cnt = 0;
    s_off_data.enter_cnt = 0;

    /* 标记本身不是状态节点，只是一个哨兵 */
    CHECK(LibFsmStateSelfExternal() != NULL, "自转移标记不应为 NULL");
    CHECK(LibFsmStateSelfExternal() != &n_off, "自转移标记不是某个具体状态节点");

    CHECK(LibFsmStateDispatch(&fsm, E_RESTART, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "外部自转移算切换");
    CHECK(LibFsmStateIsIn(&fsm, &n_off), "外部自转移后仍在 OFF");
    CHECK_TRACE("roO"); /* handle 里的动作 → 退出 → 再进入 */
    CHECK(s_off_data.exit_cnt == 1, "外部自转移应退一次，实际 %d 次", s_off_data.exit_cnt);
    CHECK(s_off_data.enter_cnt == 1, "外部自转移应进一次，实际 %d 次", s_off_data.enter_cnt);

    /* 与"返回自身"的区别：同样是 from == to，但结局是 SWITCHED，且 exit/entry 都跑了 */
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED, "OFF 不处理 E_TICK");
    CHECK(s_off_data.exit_cnt == 1, "不处理的事件不该触发 exit，实际 %d 次", s_off_data.exit_cnt);
}

/*============================ 9、10：状态内转移表 ============================*/

static void TestTableDriven(void)
{
    LibFsmStateMachine_s fsm = {0};
    test_ctx_s ctx = {0};
    test_ev_s ev = {.code = 7};

    ctx.limit = 2;
    s_wait_data.tick_cnt = 0;
    InitFresh(&fsm, &n_wait, &ctx);

    /* 表第 2 行：守卫通过 → 返回自身（内部转移），动作执行、不退出/进入 */
    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_INTERNAL, "表里写自身 = 内部转移");
    CHECK(LibFsmStateIsIn(&fsm, &n_wait), "状态应保持 WAIT");
    CHECK_TRACE("6");
    CHECK(s_wait_data.tick_cnt == 1, "状态自己的 tick 计数 = %d，期望 1", s_wait_data.tick_cnt);

    /* 第二次 tick 仍在限内 */
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_INTERNAL, "返回自身是 INTERNAL");
    CHECK_TRACE("6");
    CHECK(s_wait_data.tick_cnt == 2, "状态自己的 tick 计数 = %d，期望 2", s_wait_data.tick_cnt);

    /* 第三次：守卫拒绝第 2 行 → 落到第 3 行（切到 ERR） */
    ctx.guard_calls = 0;
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "守卫拒绝后应命中下一行并切换");
    CHECK(LibFsmStateIsIn(&fsm, &n_err), "状态应为 ERR");
    CHECK_TRACE(ORD("7", "w", "E"));
    CHECK(ctx.guard_calls == 1, "守卫调用次数 = %d，期望 1", ctx.guard_calls);

    /* 第 4 行：E_FAULT → ERR */
    InitFresh(&fsm, &n_wait, &ctx);
    CHECK(LibFsmStateDispatch(&fsm, E_FAULT, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "第 4 行应命中");
    CHECK(LibFsmStateIsIn(&fsm, &n_err), "状态应为 ERR");
    CHECK_TRACE(ORD("8", "w", "E"));

    /* 表里没有的事件：完全不响应 */
    InitFresh(&fsm, &n_wait, &ctx);
    CHECK(LibFsmStateDispatch(&fsm, E_TOGGLE, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED, "表里没有 E_TOGGLE");
    CHECK(g_trace_len == 0, "不匹配时不得调用任何回调，轨迹 = \"%s\"", g_trace);

    /* 第 1 行：切回 OFF，event_data 穿透到表动作；轨迹里没有 '!' 说明 next 为 NULL 的错误行被整行跳过 */
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_RESET, &ev) == LIB_FSM_STATE_RESULT_SWITCHED, "WAIT 下 E_RESET 应切到 OFF");
    CHECK(LibFsmStateIsIn(&fsm, &n_off), "状态应为 OFF");
    CHECK_TRACE(ORD("5", "w", "O"));
    CHECK(ctx.last_code == 7, "表动作读到的载荷 = %d，期望 7", ctx.last_code);

    /* 显式挂默认 handle 的节点：与自动回退等价 */
    ctx.tick_cnt = 0;
    ctx.limit = 1;
    s_wait2_data.tick_cnt = 0;
    InitFresh(&fsm, &n_wait2, &ctx);
    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_INTERNAL, "显式默认 handle：返回自身");
    CHECK(LibFsmStateIsIn(&fsm, &n_wait2), "状态应保持 WAIT2");
    CHECK_TRACE("6");
    CHECK(s_wait2_data.tick_cnt == 1, "WAIT2 自己的 tick 计数 = %d，期望 1", s_wait2_data.tick_cnt);
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_RESET, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "显式默认 handle：E_RESET 应切换");
    CHECK_TRACE(ORD("5", "w", "O"));
}

/*============================ 11：通配事件 ============================*/

static void TestAnyEvent(void)
{
    LibFsmStateMachine_s fsm = {0};
    test_ctx_s ctx = {0};

    /* 具体事件优先：E_RESET 命中第 1 行而不是通配行 */
    InitFresh(&fsm, &n_any, &ctx);
    CHECK(LibFsmStateDispatch(&fsm, E_RESET, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "具体事件应优先");
    CHECK(LibFsmStateIsIn(&fsm, &n_off), "状态应为 OFF");
    CHECK_TRACE(ORD("5", "a", "O"));

    /* 表里没有的事件落到通配行（否则这里会是 NOT_HANDLED） */
    InitFresh(&fsm, &n_any, &ctx);
    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "通配行应兜住 E_TICK");
    CHECK(LibFsmStateIsIn(&fsm, &n_err), "状态应为 ERR");
    CHECK_TRACE(ORD("9", "a", "E"));

    /* 手写 handle 的状态没有表，通配事件对它们没有意义（得自己在 handle 里判断） */
    InitFresh(&fsm, &n_silent, &ctx);
    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED, "SILENT 不处理任何事件");
}

/*============================ 12、13：default_state / error_state ============================*/

static void TestFallbackStates(void)
{
    LibFsmStateMachine_s fsm = {0};
    test_ctx_s ctx = {0};

    ctx.limit = 3;

    /* default_state：当前状态不处理该事件时切到兜底状态 */
    InitFresh(&fsm, &n_wait, &ctx);
    fsm.default_state = &n_err;
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_TOGGLE, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "没人处理应切到兜底状态");
    CHECK(LibFsmStateIsIn(&fsm, &n_err), "状态应为兜底状态 ERR");
    CHECK_TRACE("wE"); /* 兜底也是一次正常的外部转移，没有表动作 */
    CHECK(fsm.default_state == &n_err, "兜底状态字段不应被派发改动");

    /* 兜底状态就是当前状态：等于没兜底，结局仍是 NOT_HANDLED */
    InitFresh(&fsm, &n_wait, &ctx);
    fsm.default_state = &n_wait;
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_TOGGLE, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED, "兜底到自身视为不切换");
    CHECK(LibFsmStateIsIn(&fsm, &n_wait), "状态应保持 WAIT");
    CHECK(g_trace_len == 0, "兜底到自身时不得退/进，轨迹 = \"%s\"", g_trace);

    /* error_state：handle 返回不像状态节点的指针时改落到它 */
    InitFresh(&fsm, &n_off, &ctx);
    fsm.error_state = &n_err;
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_BAD, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "非法目标应改落到 error_state");
    CHECK(LibFsmStateIsIn(&fsm, &n_err), "状态应为 error_state");
    CHECK_TRACE("boE"); /* handle 里的动作 → 退出 OFF → 进入 ERR */

    /* 没配 error_state：当作没处理，不切换、不崩 */
    InitFresh(&fsm, &n_off, &ctx);
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_BAD, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED, "没配 error_state 就当没处理");
    CHECK(LibFsmStateIsIn(&fsm, &n_off), "非法目标不得把状态改坏");
    CHECK_TRACE("b"); /* 只有 handle 自己的动作留痕 */

    /* error_state 与 default_state 一起配：非法目标落 error_state，而不是 default_state */
    InitFresh(&fsm, &n_off, &ctx);
    fsm.error_state = &n_err;
    fsm.default_state = &n_wait;
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_BAD, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "两者都配时应落 error_state");
    CHECK(LibFsmStateIsIn(&fsm, &n_err), "状态应为 ERR");
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, LIB_FSM_STATE_EVENT_ANY, NULL) == LIB_FSM_STATE_RESULT_SWITCHED,
          "ERR 没人处理这个事件 → 落 default_state");
    CHECK(LibFsmStateIsIn(&fsm, &n_wait), "状态应为兜底状态 WAIT");
    CHECK_TRACE("eW"); /* 退出 ERR → 进入 WAIT */
}

/*============================ 混用：虚表状态 ↔ 表驱动状态 ============================*/

static void TestMixed(void)
{
    LibFsmStateMachine_s fsm = {0};
    test_ctx_s ctx = {0};

    ctx.limit = 2;
    s_wait_data.tick_cnt = 0;
    InitFresh(&fsm, &n_off, &ctx);

    CHECK(LibFsmStateDispatch(&fsm, E_FAULT, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "OFF --E_FAULT--> ERR");
    CHECK_TRACE("foE");

    CHECK(LibFsmStateDispatch(&fsm, E_TOGGLE, NULL) == LIB_FSM_STATE_RESULT_SWITCHED,
          "ERR --E_TOGGLE--> WAIT（切到表驱动状态）");
    CHECK(LibFsmStateIsIn(&fsm, &n_wait), "状态应为 WAIT");
    CHECK_TRACE("foE4eW");

    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_INTERNAL, "WAIT 表第 2 行：返回自身");
    CHECK(LibFsmStateIsIn(&fsm, &n_wait), "状态应保持 WAIT");

    CHECK(LibFsmStateDispatch(&fsm, E_RESET, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "WAIT 表第 1 行：切回 OFF");
    CHECK(LibFsmStateIsIn(&fsm, &n_off), "状态应为 OFF");
    CHECK_TRACE("foE4eW6" ORD("5", "w", "O"));
}

/*============================ 14：跟踪钩子 ============================*/

static void TestTraceHook(void)
{
    LibFsmStateMachine_s fsm = {0};
    test_ctx_s ctx = {0};

    ctx.limit = 2;

    /* 切换：from/to 与结局都对 */
    InitFresh(&fsm, &n_wait, &ctx);
    fsm.trace_fn = TraceHook;
    g_rec_n = 0;
    CHECK(LibFsmStateDispatch(&fsm, E_RESET, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "应切换");
    CHECK(g_rec_n == 1, "每次派发严格回调一次，实际 %d 次", g_rec_n);
    CHECK(g_recs[0].from == &n_wait && g_recs[0].to == &n_off, "跟踪的 from/to 不对");
    CHECK(g_recs[0].event == E_RESET, "跟踪的 event 不对：%d", (int)g_recs[0].event);
    CHECK(g_recs[0].result == LIB_FSM_STATE_RESULT_SWITCHED, "跟踪的结局应为 SWITCHED");

    /* 没人处理：from == to，结局 NOT_HANDLED（也照样回调） */
    g_rec_n = 0;
    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED, "OFF 不处理 E_TICK");
    CHECK(g_rec_n == 1, "没人处理也要回调一次，实际 %d 次", g_rec_n);
    CHECK(g_recs[0].from == &n_off && g_recs[0].to == &n_off, "没人处理时 from/to 都应是当前状态");
    CHECK(g_recs[0].result == LIB_FSM_STATE_RESULT_NOT_HANDLED, "跟踪的结局应为 NOT_HANDLED");

    /* 内部转移：from == to，结局 INTERNAL */
    InitFresh(&fsm, &n_err, &ctx);
    fsm.trace_fn = TraceHook;
    g_rec_n = 0;
    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_INTERNAL, "返回自身是 INTERNAL");
    CHECK(g_rec_n == 1, "内部转移也要回调一次，实际 %d 次", g_rec_n);
    CHECK(g_recs[0].from == &n_err && g_recs[0].to == &n_err, "内部转移 from/to 都是当前状态");
    CHECK(g_recs[0].result == LIB_FSM_STATE_RESULT_INTERNAL, "跟踪的结局应为 INTERNAL");

    /* 外部自转移：from == to，但结局是 SWITCHED —— 只有靠 result 才分得出来 */
    InitFresh(&fsm, &n_off, &ctx);
    fsm.trace_fn = TraceHook;
    g_rec_n = 0;
    CHECK(LibFsmStateDispatch(&fsm, E_RESTART, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "外部自转移算切换");
    CHECK(g_rec_n == 1, "外部自转移也要回调一次，实际 %d 次", g_rec_n);
    CHECK(g_recs[0].from == &n_off && g_recs[0].to == &n_off, "外部自转移 from/to 都是当前状态");
    CHECK(g_recs[0].result == LIB_FSM_STATE_RESULT_SWITCHED, "外部自转移的结局应为 SWITCHED");

    /* 跟踪钩子不会因为 Init 之外的操作消失 */
    CHECK(fsm.trace_fn == TraceHook, "trace_fn 应保持");
    fsm.trace_fn = NULL;
    g_rec_n = 0;
    CHECK(LibFsmStateDispatch(&fsm, E_TICK, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED, "摘掉钩子后仍应正常派发");
    CHECK(g_rec_n == 0, "钩子摘掉后不该再回调，实际 %d 次", g_rec_n);
}

/*============================ 15：空指针与空虚表 ============================*/

static void TestNullSafety(void)
{
    test_ctx_s ctx = {0};
    LibFsmStateMachine_s zeroed = {0}; /* 尚未 Init */
    LibFsmStateMachine_s fsm = {0};

    CHECK(LibFsmStateDispatch(NULL, E_TOGGLE, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED,
          "fsm 为 NULL 时应返回 NOT_HANDLED");
    CHECK(LibFsmStateCurrent(NULL) == NULL, "fsm 为 NULL 时当前状态应为 NULL");
    CHECK(LibFsmStateCurrentName(NULL) == NULL, "fsm 为 NULL 时状态名应为 NULL");
    CHECK(LibFsmStateIsIn(NULL, &n_off) == false, "fsm 为 NULL 时 IsIn 应为 false");
    CHECK(LibFsmStateDefaultHandle(NULL, E_TICK, NULL) == NULL, "fsm 为 NULL 时默认 handle 返回 NULL");

    /* 零初始化、未 Init 的状态机：current 为 NULL，一切照旧安全 */
    TraceReset();
    CHECK(LibFsmStateDispatch(&zeroed, E_TOGGLE, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED,
          "未初始化时应返回 NOT_HANDLED");
    CHECK(LibFsmStateCurrent(&zeroed) == NULL, "未初始化时 current 应为 NULL");
    CHECK(LibFsmStateIsIn(&zeroed, NULL) == false, "IsIn(未初始化, NULL) 应为 false");
    CHECK(g_trace_len == 0, "未初始化时不得调用任何回调，轨迹 = \"%s\"", g_trace);

    /* 既没有 handle 也没有转移表的状态：作为落点合法（ops 非 NULL），但不处理任何事件 */
    InitFresh(&fsm, &n_silent, &ctx);
    CHECK(LibFsmStateIsIn(&fsm, &n_silent), "状态应为 SILENT");
    CHECK(g_trace_len == 0, "entry 为 NULL 时不得留下轨迹");
    CHECK(LibFsmStateDispatch(&fsm, E_TOGGLE, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED, "SILENT 不处理任何事件");
    CHECK(LibFsmStateDefaultHandle(&fsm, E_TICK, NULL) == NULL, "无转移表时默认 handle 返回 NULL");
    CHECK(LibFsmStateDefaultHandle(NULL, E_TICK, NULL) == NULL, "NULL 状态机时也返回 NULL");

    /* 状态名照常可读 */
    CHECK(LibFsmStateCurrentName(&fsm) != NULL && strcmp(LibFsmStateCurrentName(&fsm), "SILENT") == 0,
          "当前状态名 = %s，期望 SILENT", LibFsmStateCurrentName(&fsm));
}

/*============================ 16、17：event_data / ctx / 状态数据共享 ============================*/

static void TestCtxAndNodeData(void)
{
    test_ctx_s ctx_a = {0};
    test_ctx_s ctx_b = {0};
    test_ev_s ev = {.code = 5};
    LibFsmStateMachine_s a = {0};
    LibFsmStateMachine_s b = {0};

    ctx_a.limit = 3;
    ctx_b.limit = 3;
    s_off_data.enter_cnt = 0;
    s_wait_data.tick_cnt = 0;

    LibFsmStateInit(&a, &n_off, &ctx_a);
    LibFsmStateInit(&b, &n_off, &ctx_b);

    /* 状态数据挂在静态单例上：两个实例各自 Init 都会累加同一份 s_off_data */
    CHECK(s_off_data.enter_cnt == 2, "OFF 的 enter 次数 = %d，期望 2（data 是全局的，两实例共享）",
          s_off_data.enter_cnt);
    CHECK(n_off.data == &s_off_data, "n_off.data 应指向 s_off_data");
    CHECK(n_wait.data == &s_wait_data, "n_wait.data 应指向 s_wait_data");
    CHECK(n_wait2.data == &s_wait2_data, "n_wait2.data 应指向 s_wait2_data");

    /* ctx 各归各的：动作只改本实例的 ctx */
    TraceReset();
    CHECK(LibFsmStateDispatch(&a, E_TOGGLE, &ev) == LIB_FSM_STATE_RESULT_SWITCHED, "实例 A 应切换");
    CHECK(ctx_a.toggle_cnt == 1, "实例 A 的 toggle 计数 = %d，期望 1", ctx_a.toggle_cnt);
    CHECK(ctx_b.toggle_cnt == 0, "实例 B 的 toggle 计数 = %d，期望 0", ctx_b.toggle_cnt);
    CHECK(ctx_a.last_code == 5, "实例 A 读到载荷 %d，期望 5", ctx_a.last_code);
    CHECK(ctx_b.last_code == 0, "实例 B 不应读到载荷，实际 %d", ctx_b.last_code);
    CHECK(LibFsmStateIsIn(&a, &n_on), "实例 A 应为 ON");
    CHECK(LibFsmStateIsIn(&b, &n_off), "实例 B 应仍为 OFF");

    /* 状态数据共享的反面：B 在 WAIT 下 tick，A 也"看得见"（因为数据挂在节点上） */
    ctx_a.limit = 2;
    ctx_b.limit = 2;
    LibFsmStateInit(&a, &n_wait, &ctx_a);
    LibFsmStateInit(&b, &n_wait, &ctx_b);
    CHECK(LibFsmStateDispatch(&a, E_TICK, NULL) == LIB_FSM_STATE_RESULT_INTERNAL, "A 在 WAIT 下 tick（返回自身）");
    CHECK(s_wait_data.tick_cnt == 1, "状态自己的 tick 计数 = %d，期望 1", s_wait_data.tick_cnt);
    CHECK(LibFsmStateDispatch(&b, E_TICK, NULL) == LIB_FSM_STATE_RESULT_INTERNAL, "B 也 tick 一次");
    CHECK(s_wait_data.tick_cnt == 2, "两个实例共用同一份状态数据，tick 计数 = %d，期望 2", s_wait_data.tick_cnt);
    CHECK(ctx_a.tick_cnt == 1 && ctx_b.tick_cnt == 1, "ctx 的 tick 计数各自为 1：A=%d B=%d", ctx_a.tick_cnt,
          ctx_b.tick_cnt);
    CHECK(g_trace_len > 0, "轨迹串非空（回调确实被调用过）");

    /* event_data 为 NULL 也能穿透（handle 里做 NULL 判断） */
    TraceReset();
    ctx_a.toggle_cnt = 0;
    LibFsmStateInit(&a, &n_off, &ctx_a);
    CHECK(LibFsmStateDispatch(&a, E_TOGGLE, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "event_data 为 NULL 时也应能切换");
    CHECK(ctx_a.last_code == -1, "载荷为 NULL 时 last_code = %d，期望 -1", ctx_a.last_code);
}

/*============================ 18：重入 ============================*/
/* PARANOID 下重入会断言 abort（断言没法在进程内检验），所以这一组只在非 PARANOID 构建里跑 */

#ifndef LIB_FSM_STATE_PARANOID
static void TestReentrancy(void)
{
    LibFsmStateMachine_s fsm = {0};
    test_ctx_s ctx = {0};

    ctx.limit = 3;
    InitFresh(&fsm, &n_off, &ctx);

    /* 回调里再派发同一状态机：内层被挡住（NOT_HANDLED），外层动作与状态不受影响 */
    g_reenter_result = -1;
    CHECK(LibFsmStateDispatch(&fsm, E_REENTER, NULL) == LIB_FSM_STATE_RESULT_NOT_HANDLED, "外层 handle 返回 NULL");
    CHECK(g_reenter_result == (int)LIB_FSM_STATE_RESULT_NOT_HANDLED, "内层重入应被挡成 NOT_HANDLED，实际 %d",
          g_reenter_result);
    CHECK(ctx.toggle_cnt == 0, "内层重入不得执行动作，toggle 计数 = %d", ctx.toggle_cnt);
    CHECK(LibFsmStateIsIn(&fsm, &n_off), "重入不得改变状态");
    CHECK_TRACE("Rr");

    /* 标志已放下：之后还能正常派发 */
    TraceReset();
    CHECK(LibFsmStateDispatch(&fsm, E_TOGGLE, NULL) == LIB_FSM_STATE_RESULT_SWITCHED, "重入之后应恢复正常");
    CHECK(LibFsmStateIsIn(&fsm, &n_on), "状态应为 ON");
    CHECK_TRACE("1oN");
}
#endif /* !LIB_FSM_STATE_PARANOID */

int main(void)
{
    printf("lib_fsm_state PC 端检验\n");
#ifdef LIB_FSM_STATE_PARANOID
    printf("(PARANOID 开：运行时自检生效)\n");
#endif
#ifdef LIB_FSM_STATE_UML_ORDER
    printf("(UML_ORDER 开：表驱动动作为 退出 → 动作 → 进入)\n");
#endif
#ifdef LIB_FSM_STATE_TRACE
    printf("(TRACE 开：每次派发打印一行，下面会有 [fsm] 输出)\n");
#endif

    TestInit();
    TestManualDispatch();
    TestSelfExternal();
    TestTableDriven();
    TestAnyEvent();
    TestFallbackStates();
    TestMixed();
    TestTraceHook();
    TestNullSafety();
    TestCtxAndNodeData();
#ifndef LIB_FSM_STATE_PARANOID
    TestReentrancy();
#endif

    printf("checks=%d fails=%d -> %s\n", g_checks, g_fails, g_fails ? "FAIL" : "PASS");
    return g_fails ? 1 : 0;
}
