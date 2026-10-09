/**
 * @file test_fsm_table.c
 * @brief lib_fsm_table PC 端检验（转移顺序 / 守卫分叉 / 通配符 / 内部转移 / 值域保护），退出码 0 = PASS
 *
 * @note 用 tools/test_fsm_table.sh 一键编译运行（gcc，-DLIB_FSM_TABLE_STANDALONE 跳过 app_cfg.h）。
 * @note 检验项：
 *       1. 初始化：current 落到初始状态，且不执行任何回调（含初始状态的进入动作）
 *       2. 启动：LibFsmTableStart 执行当前状态的进入动作；不防重入；NULL/越界静默跳过
 *       3. 基本转移：返回值、状态迁移、ctx 改动对调用方可见
 *       4. 执行顺序：退出旧状态 → 转移动作 → 切换状态 → 进入新状态（轨迹串逐字符比对）
 *       5. 首条命中：同 from/event 多条规则按表序取第一条守卫通过者，且只执行一条
 *       6. 守卫拒绝：返回 false、状态不变，且退出动作与转移动作都不执行
 *       7. 自转移 from==to：退出与进入各执行一次
 *       8. 内部转移：只执行转移动作，不退不进、状态不变，to 字段被忽略；守卫同样生效
 *       9. 通配符：ANY_STATE / ANY_EVENT 能兜底；通配符同样"第一条命中即停"（顺序即优先级）
 *       10. 无匹配：不适用事件 / 空表 / transitions 为 NULL / 条数为 0 → false 且状态不变
 *       11. event_data：NULL 与结构体指针都能穿透到守卫与转移动作
 *       12. 值域保护：状态值越界（含负值）与 state_count 为 0 时静默跳过进入退出动作
 *       13. state_actions 为 NULL：只做转移、不调用任何状态动作
 *       14. 多实例：共享同一张表，各自维护 current
 *       15. 顺序扫描：命中第 k 条时守卫恰被调用 k 次（不提前终止、不越过后继续）
 *       16. 哨兵：LibFsmTableCurrent(NULL) 返回 LIB_FSM_TABLE_INVALID_STATE，与合法状态 0 可区分
 *       17. 跟踪钩子：命中/未命中/内部转移各报一次，from/to/event/transitioned 正确、ctx 原样回传
 */

#include <stdio.h>
#include <string.h>

#include "lib_fsm_table.h"

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond, ...)                                                                                                                                       \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        g_checks++;                                                                                                                                            \
        if (!(cond))                                                                                                                                           \
        {                                                                                                                                                      \
            g_fails++;                                                                                                                                         \
            printf("FAIL %d: ", __LINE__);                                                                                                                     \
            printf(__VA_ARGS__);                                                                                                                               \
            printf("\n");                                                                                                                                      \
        }                                                                                                                                                      \
    } while (0)

/*============================ 轨迹串：记录回调被调用的顺序 ============================*/
/* 进入/退出动作用大小写字母（I/i=IDLE、R/r=RUN、E/e=ERR），转移动作用数字 */

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

/*============================ 被测状态机的业务定义 ============================*/

typedef enum : uint8_t
{
    S_IDLE = 0,
    S_RUN,
    S_ERR,
    S_COUNT, /* 状态个数：动作表长度 */
} test_state_e;

typedef enum : uint8_t
{
    E_START = 0,
    E_STOP,
    E_FAULT,
    E_TICK,  /* 内部转移用：RUN 下每次 tick 刷新 */
    E_RESET, /* 通配符兜底用：任意状态都回 IDLE */
} test_event_e;

/** @brief 事件载荷：守卫按阈值比对、转移动作把它记进 ctx */
typedef struct
{
    int code;
} test_ev_s;

/** @brief 用户上下文 */
typedef struct
{
    int limit;       // 守卫阈值：载荷 code <= limit 才放行
    int action_cnt;  // 转移动作累计调用次数
    int last_code;   // 转移动作看到的载荷（event_data 为 NULL 记 -1）
    int guard_calls; // 守卫调用次数（仅用于第 15 项，验证扫描顺序与提前终止）
    int trace_calls; // 跟踪钩子调用次数（验证 ctx 原样回传）
} test_ctx_s;

/*------------- 进入/退出动作 --------------*/

static void IdleEntry(void *ctx)
{
    (void)ctx;
    TraceAdd('I');
}

static void IdleExit(void *ctx)
{
    (void)ctx;
    TraceAdd('i');
}

static void RunEntry(void *ctx)
{
    (void)ctx;
    TraceAdd('R');
}

static void RunExit(void *ctx)
{
    (void)ctx;
    TraceAdd('r');
}

static void ErrEntry(void *ctx)
{
    (void)ctx;
    TraceAdd('E');
}

static void ErrExit(void *ctx)
{
    (void)ctx;
    TraceAdd('e');
}

static const LibFsmTableStateActions_s s_actions[S_COUNT] = {
    [S_IDLE] = {.entry = IdleEntry, .exit = IdleExit},
    [S_RUN] = {.entry = RunEntry, .exit = RunExit},
    [S_ERR] = {.entry = ErrEntry, .exit = ErrExit},
};

/*------------- 转移动作 --------------*/

static void ActStart(void *ctx, const void *event_data)
{
    test_ctx_s *c = ctx;
    const test_ev_s *ev = event_data;

    TraceAdd('1');
    c->action_cnt++;
    c->last_code = (ev != NULL) ? ev->code : -1;
}

static void ActStop(void *ctx, const void *event_data)
{
    (void)event_data;
    TraceAdd('2');
    ((test_ctx_s *)ctx)->action_cnt++;
}

static void ActFault(void *ctx, const void *event_data)
{
    (void)event_data;
    TraceAdd('3');
    ((test_ctx_s *)ctx)->action_cnt++;
}

static void ActTick(void *ctx, const void *event_data)
{
    (void)event_data;
    TraceAdd('4');
    ((test_ctx_s *)ctx)->action_cnt++;
}

static void ActReset(void *ctx, const void *event_data)
{
    (void)event_data;
    TraceAdd('5');
    ((test_ctx_s *)ctx)->action_cnt++;
}

static void ActPing(void *ctx, const void *event_data)
{
    (void)event_data;
    TraceAdd('6');
    ((test_ctx_s *)ctx)->action_cnt++;
}

/*------------- 守卫（第 15 项的两个守卫带计数副作用，仅测试用） --------------*/

static bool GuardAlways(void *ctx, const void *event_data)
{
    (void)ctx;
    (void)event_data;
    return true;
}

static bool GuardNever(void *ctx, const void *event_data)
{
    (void)ctx;
    (void)event_data;
    return false;
}

static bool GuardCodeLeLimit(void *ctx, const void *event_data)
{
    test_ctx_s *c = ctx;
    const test_ev_s *ev = event_data;

    c->guard_calls++;

    /* 载荷为 NULL 视为无约束放行：事件可以不携带参数 */
    return (ev == NULL) || (ev->code <= c->limit);
}

static bool GuardCountReject(void *ctx, const void *event_data)
{
    (void)event_data;
    ((test_ctx_s *)ctx)->guard_calls++;
    return false;
}

static bool GuardCountAccept(void *ctx, const void *event_data)
{
    (void)event_data;
    ((test_ctx_s *)ctx)->guard_calls++;
    return true;
}

/*------------- 跟踪钩子记录 --------------*/

#define TRACE_MAX 32

typedef struct
{
    int from;
    int event;
    int to;
    int transitioned;
} trace_rec_s;

static trace_rec_s g_tr[TRACE_MAX];
static int g_tr_n;

static void TraceLogReset(void)
{
    g_tr_n = 0;
}

static void TraceHook(void *ctx, LibFsmTableState_t from, LibFsmTableEvent_t event, LibFsmTableState_t to, bool transitioned)
{
    ((test_ctx_s *)ctx)->trace_calls++;

    if (g_tr_n < TRACE_MAX)
    {
        g_tr[g_tr_n].from = (int)from;
        g_tr[g_tr_n].event = (int)event;
        g_tr[g_tr_n].to = (int)to;
        g_tr[g_tr_n].transitioned = transitioned ? 1 : 0;
        g_tr_n++;
    }
}

/* 断言第 idx 条跟踪记录的内容 */
static int RecIs(int idx, int from, int event, int to, int transitioned)
{
    return (idx < g_tr_n) && (g_tr[idx].from == from) && (g_tr[idx].event == event) && (g_tr[idx].to == to) && (g_tr[idx].transitioned == transitioned);
}

/*------------- 各测试用的转移表 --------------*/

/* 主表：IDLE --E_START--> RUN，RUN --E_STOP--> IDLE，RUN --E_FAULT--> ERR */
static const LibFsmTableTransition_s s_main_table[] = {
    {S_IDLE, E_START, S_RUN, GuardAlways, ActStart, LIB_FSM_TABLE_TRANS_EXTERNAL},
    {S_RUN, E_STOP, S_IDLE, NULL, ActStop, LIB_FSM_TABLE_TRANS_EXTERNAL},
    {S_RUN, E_FAULT, S_ERR, NULL, ActFault, LIB_FSM_TABLE_TRANS_EXTERNAL},
};

#define MAIN_TABLE_N ((uint32_t)(sizeof(s_main_table) / sizeof(s_main_table[0])))

/* 编译期自检宏真的会展开（此处参数合法，编译通过即证明可用） */
LIB_FSM_TABLE_CHECK_TABLE(s_main_table);
LIB_FSM_TABLE_CHECK_ACTIONS(s_actions, S_COUNT);

/*------------- 装配辅助 --------------*/

static void InitFsm(LibFsmTableInstance_s *fsm, test_ctx_s *ctx, const LibFsmTableTransition_s *table, uint32_t transition_count,
                    const LibFsmTableStateActions_s *actions, uint32_t state_count, LibFsmTableState_t initial_state)
{
    LibFsmTableConfig_s cfg = {0};

    cfg.initial_state = initial_state;
    cfg.transitions = table;
    cfg.transition_count = transition_count;
    cfg.state_actions = actions;
    cfg.state_count = state_count;
    cfg.ctx = ctx;
    cfg.trace_fn = NULL; /* 需要跟踪的用例自己再赋值 */

    LibFsmTableInit(fsm, &cfg);
}

/*============================ 1、3、4、11：初始化 / 基本转移 / 执行顺序 / event_data ============================*/

static void TestBasic(void)
{
    test_ctx_s ctx = {0};
    test_ev_s ev = {.code = 1};
    LibFsmTableInstance_s fsm;

    InitFsm(&fsm, &ctx, s_main_table, MAIN_TABLE_N, s_actions, S_COUNT, S_IDLE);
    TraceReset();

    CHECK(LibFsmTableCurrent(&fsm) == S_IDLE, "初始状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_IDLE);
    CHECK(g_trace_len == 0, "Init 不应执行任何回调（含初始状态的进入动作），轨迹 = \"%s\"", g_trace);

    /* IDLE → RUN：退出 IDLE('i') → 转移动作('1') → 切换状态 → 进入 RUN('R') */
    CHECK(LibFsmTableDispatch(&fsm, E_START, &ev) == true, "IDLE 下 E_START 应发生转移");
    CHECK(LibFsmTableCurrent(&fsm) == S_RUN, "状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_RUN);
    CHECK_TRACE("i1R");
    CHECK(ctx.action_cnt == 1, "转移动作调用次数 = %d，期望 1", ctx.action_cnt);
    CHECK(ctx.last_code == 1, "转移动作读到的载荷 = %d，期望 1", ctx.last_code);

    /* RUN → IDLE（载荷为 NULL，转移动作用不到事件参数） */
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_STOP, NULL) == true, "RUN 下 E_STOP 应发生转移");
    CHECK_TRACE("r2I");

    /* 回到 RUN，再走故障分支：RUN → ERR */
    CHECK(LibFsmTableDispatch(&fsm, E_START, &ev) == true, "IDLE 下 E_START 应发生转移");
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_FAULT, NULL) == true, "RUN 下 E_FAULT 应发生转移");
    CHECK(LibFsmTableCurrent(&fsm) == S_ERR, "状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_ERR);
    CHECK_TRACE("r3E");

    /* ERR 下没有 E_START 规则：不转移、不调用任何回调 */
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, &ev) == false, "ERR 状态下 E_START 不应有转移");
    CHECK(LibFsmTableCurrent(&fsm) == S_ERR, "未转移时状态不应改变");
    CHECK(g_trace_len == 0, "未转移时不得调用任何回调，轨迹 = \"%s\"", g_trace);

    /* 枚举外的未知事件同样只是不匹配 */
    CHECK(LibFsmTableDispatch(&fsm, 99, NULL) == false, "未知事件不应有转移");
}

/*============================ 2：启动 ============================*/

static void TestStart(void)
{
    test_ctx_s ctx = {0};
    LibFsmTableInstance_s fsm;

    InitFsm(&fsm, &ctx, s_main_table, MAIN_TABLE_N, s_actions, S_COUNT, S_IDLE);
    TraceReset();

    LibFsmTableStart(&fsm);
    CHECK_TRACE("I"); /* 初始状态的进入动作 */

    /* 不防重入：重复调用就重复执行，是否允许由调用方把握 */
    LibFsmTableStart(&fsm);
    CHECK_TRACE("II");

    /* 当前状态越界：静默跳过 */
    InitFsm(&fsm, &ctx, s_main_table, MAIN_TABLE_N, s_actions, S_COUNT, 9);
    TraceReset();
    LibFsmTableStart(&fsm);
    CHECK(g_trace_len == 0, "越界当前状态不得调用任何状态动作，轨迹 = \"%s\"", g_trace);

    /* 动作表为 NULL：静默跳过 */
    InitFsm(&fsm, &ctx, s_main_table, MAIN_TABLE_N, NULL, 0u, S_IDLE);
    LibFsmTableStart(&fsm);
    CHECK(g_trace_len == 0, "动作表为 NULL 时不得调用任何状态动作，轨迹 = \"%s\"", g_trace);

    LibFsmTableStart(NULL); /* 只要求不崩 */

    /* 启动后再派发，状态机照常工作 */
    InitFsm(&fsm, &ctx, s_main_table, MAIN_TABLE_N, s_actions, S_COUNT, S_IDLE);
    TraceReset();
    LibFsmTableStart(&fsm);
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "启动后应能正常派发");
    CHECK_TRACE("Ii1R");
}

/*============================ 5：首条命中（同 from/event 多条规则） ============================*/

static void TestFirstMatch(void)
{
    /* 第一条守卫拒绝 → 落到第二条 */
    static const LibFsmTableTransition_s reject_first[] = {
        {S_IDLE, E_START, S_ERR, GuardNever, ActFault, LIB_FSM_TABLE_TRANS_EXTERNAL},
        {S_IDLE, E_START, S_RUN, GuardAlways, ActStart, LIB_FSM_TABLE_TRANS_EXTERNAL},
    };
    /* 第一条守卫通过 → 不再看第二条 */
    static const LibFsmTableTransition_s accept_first[] = {
        {S_IDLE, E_START, S_ERR, GuardAlways, ActFault, LIB_FSM_TABLE_TRANS_EXTERNAL},
        {S_IDLE, E_START, S_RUN, GuardAlways, ActStart, LIB_FSM_TABLE_TRANS_EXTERNAL},
    };
    test_ctx_s ctx = {0};
    LibFsmTableInstance_s fsm;

    InitFsm(&fsm, &ctx, reject_first, 2u, s_actions, S_COUNT, S_IDLE);
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "守卫拒绝首条时应落到第二条");
    CHECK(LibFsmTableCurrent(&fsm) == S_RUN, "状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_RUN);
    CHECK_TRACE("i1R");
    CHECK(ctx.action_cnt == 1, "只应执行一条规则的转移动作，实际 %d 次", ctx.action_cnt);

    ctx.action_cnt = 0;
    InitFsm(&fsm, &ctx, accept_first, 2u, s_actions, S_COUNT, S_IDLE);
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "首条守卫通过时应命中首条");
    CHECK(LibFsmTableCurrent(&fsm) == S_ERR, "状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_ERR);
    CHECK_TRACE("i3E");
    CHECK(ctx.action_cnt == 1, "只应执行一条规则的转移动作，实际 %d 次", ctx.action_cnt);
}

/*============================ 6、11：守卫拒绝 / event_data ============================*/

static void TestGuardReject(void)
{
    static const LibFsmTableTransition_s table[] = {
        {S_IDLE, E_START, S_RUN, GuardCodeLeLimit, ActStart, LIB_FSM_TABLE_TRANS_EXTERNAL},
        {S_RUN, E_START, S_IDLE, GuardCodeLeLimit, ActStop, LIB_FSM_TABLE_TRANS_EXTERNAL},
    };
    test_ctx_s ctx = {0};
    test_ev_s ev = {.code = 9};
    LibFsmTableInstance_s fsm;

    ctx.limit = 5;
    InitFsm(&fsm, &ctx, table, 2u, s_actions, S_COUNT, S_IDLE);

    /* 载荷 9 > 阈值 5：守卫拦下，退出动作与转移动作都不执行 */
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, &ev) == false, "守卫不通过时不应转移");
    CHECK(LibFsmTableCurrent(&fsm) == S_IDLE, "守卫不通过时状态不应改变");
    CHECK(g_trace_len == 0, "守卫不通过时退出动作也不得执行，轨迹 = \"%s\"", g_trace);
    CHECK(ctx.guard_calls == 1, "守卫调用次数 = %d，期望 1", ctx.guard_calls);

    /* 载荷 5 <= 阈值 5：放行 */
    ev.code = 5;
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, &ev) == true, "守卫通过时应转移");
    CHECK_TRACE("i1R");
    CHECK(ctx.last_code == 5, "转移动作读到的载荷 = %d，期望 5", ctx.last_code);

    /* 无载荷（NULL）：守卫自行决定放行 */
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "event_data 为 NULL 时守卫应被调用并可放行");
    CHECK_TRACE("r2I");

    ctx.guard_calls = 0;
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "再次派发应仍旧命中");
    CHECK(ctx.guard_calls == 1, "守卫调用次数 = %d，期望 1", ctx.guard_calls);
}

/*============================ 7：自转移 from == to ============================*/

static void TestSelfTransition(void)
{
    static const LibFsmTableTransition_s table[] = {
        {S_RUN, E_START, S_RUN, NULL, ActStart, LIB_FSM_TABLE_TRANS_EXTERNAL},
    };
    test_ctx_s ctx = {0};
    LibFsmTableInstance_s fsm;

    InitFsm(&fsm, &ctx, table, 1u, s_actions, S_COUNT, S_RUN);
    TraceReset();

    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "自转移也应报告发生了转移");
    CHECK(LibFsmTableCurrent(&fsm) == S_RUN, "自转移后状态不变");
    CHECK_TRACE("r1R"); /* 退出与进入各执行一次，不是"什么都不做" */
    CHECK(ctx.action_cnt == 1, "自转移的转移动作应执行一次，实际 %d 次", ctx.action_cnt);
}

/*============================ 8、17：内部转移 ============================*/

static void TestInternalTransition(void)
{
    static const LibFsmTableTransition_s table[] = {
        {S_RUN, E_TICK, S_RUN, NULL, ActTick, LIB_FSM_TABLE_TRANS_INTERNAL},
        {S_RUN, E_START, S_IDLE, NULL, ActStop, LIB_FSM_TABLE_TRANS_EXTERNAL},
    };
    /* 内部转移的 to 被忽略：这里故意写个越界值 */
    static const LibFsmTableTransition_s ignore_to[] = {
        {S_RUN, E_TICK, 99, NULL, ActTick, LIB_FSM_TABLE_TRANS_INTERNAL},
    };
    /* 内部转移同样要过守卫：守卫拒绝就落到下一条外部规则 */
    static const LibFsmTableTransition_s guarded[] = {
        {S_RUN, E_TICK, S_RUN, GuardNever, ActTick, LIB_FSM_TABLE_TRANS_INTERNAL},
        {S_RUN, E_TICK, S_ERR, NULL, ActFault, LIB_FSM_TABLE_TRANS_EXTERNAL},
    };
    test_ctx_s ctx = {0};
    LibFsmTableInstance_s fsm;

    InitFsm(&fsm, &ctx, table, 2u, s_actions, S_COUNT, S_RUN);

    /* 只执行转移动作：没有退出、没有进入、状态不变 */
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_TICK, NULL) == true, "内部转移应报告发生了转移");
    CHECK(LibFsmTableCurrent(&fsm) == S_RUN, "内部转移后状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_RUN);
    CHECK_TRACE("4");
    CHECK(ctx.action_cnt == 1, "内部转移的动作应执行一次，实际 %d 次", ctx.action_cnt);

    /* 同一张表里的外部转移不受影响 */
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "外部转移照常");
    CHECK_TRACE("r2I");

    /* to 字段被忽略：写 99 也不会把状态改成 99 */
    InitFsm(&fsm, &ctx, ignore_to, 1u, s_actions, S_COUNT, S_RUN);
    CHECK(LibFsmTableDispatch(&fsm, E_TICK, NULL) == true, "内部转移应报告发生了转移");
    CHECK(LibFsmTableCurrent(&fsm) == S_RUN, "内部转移忽略 to，状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_RUN);

    /* 守卫拒绝内部转移 → 落到下一条外部规则 */
    ctx.action_cnt = 0;
    InitFsm(&fsm, &ctx, guarded, 2u, s_actions, S_COUNT, S_RUN);
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_TICK, NULL) == true, "守卫拒绝内部转移后应命中下一条");
    CHECK(LibFsmTableCurrent(&fsm) == S_ERR, "状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_ERR);
    CHECK_TRACE("r3E");
    CHECK(ctx.action_cnt == 1, "只应执行一条规则的动作，实际 %d 次", ctx.action_cnt);
}

/*============================ 9：通配符 ============================*/

static void TestAnyWildcard(void)
{
    /* 通配规则放表尾兜底：ANY_STATE+E_RESET 在 3 号，S_ERR+ANY_EVENT 在 4 号 */
    static const LibFsmTableTransition_s table[] = {
        {S_IDLE, E_START, S_RUN, NULL, ActStart, LIB_FSM_TABLE_TRANS_EXTERNAL},
        {S_RUN, E_FAULT, S_ERR, NULL, ActFault, LIB_FSM_TABLE_TRANS_EXTERNAL},
        {LIB_FSM_TABLE_ANY_STATE, E_RESET, S_IDLE, NULL, ActReset, LIB_FSM_TABLE_TRANS_EXTERNAL},
        {S_ERR, LIB_FSM_TABLE_ANY_EVENT, S_IDLE, NULL, ActPing, LIB_FSM_TABLE_TRANS_EXTERNAL},
    };
    /* 通配规则放表头会吃掉后面的规则 */
    static const LibFsmTableTransition_s shadow[] = {
        {LIB_FSM_TABLE_ANY_STATE, LIB_FSM_TABLE_ANY_EVENT, S_ERR, NULL, ActFault, LIB_FSM_TABLE_TRANS_EXTERNAL},
        {S_IDLE, E_START, S_RUN, NULL, ActStart, LIB_FSM_TABLE_TRANS_EXTERNAL},
    };
    /* 通配规则也要过守卫 */
    static const LibFsmTableTransition_s guarded[] = {
        {LIB_FSM_TABLE_ANY_STATE, E_RESET, S_ERR, GuardNever, ActFault, LIB_FSM_TABLE_TRANS_EXTERNAL},
        {LIB_FSM_TABLE_ANY_STATE, E_RESET, S_IDLE, NULL, ActReset, LIB_FSM_TABLE_TRANS_EXTERNAL},
    };
    test_ctx_s ctx = {0};
    LibFsmTableInstance_s fsm;

    /* ANY_STATE：从"没有 E_RESET 显式规则"的状态也能兜到 */
    InitFsm(&fsm, &ctx, table, 4u, s_actions, S_COUNT, S_IDLE);
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "IDLE → RUN");
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_RESET, NULL) == true, "RUN 下 E_RESET 应由通配规则兜住");
    CHECK(LibFsmTableCurrent(&fsm) == S_IDLE, "状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_IDLE);
    CHECK_TRACE("r5I");

    /* ANY_EVENT：ERR 下任意事件都由 4 号（通配事件）兜住 */
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "IDLE → RUN");
    CHECK(LibFsmTableDispatch(&fsm, E_FAULT, NULL) == true, "RUN → ERR");
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_TICK, NULL) == true, "ERR 下 E_TICK 应由通配事件规则兜住");
    CHECK(LibFsmTableCurrent(&fsm) == S_IDLE, "状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_IDLE);
    CHECK_TRACE("e6I");

    /* 两条通配规则同时命中时按表序取前者：ERR + E_RESET → 小号(ANY_STATE)先赢 */
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "IDLE → RUN");
    CHECK(LibFsmTableDispatch(&fsm, E_FAULT, NULL) == true, "RUN → ERR");
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_RESET, NULL) == true, "ERR 下 E_RESET 应命中");
    CHECK_TRACE("e5I"); /* 3 号（ANY_STATE+E_RESET，动作 '5'）先于 4 号（动作 '6'） */

    /* 通配规则放表头：后面的规则永远轮不到 */
    InitFsm(&fsm, &ctx, shadow, 2u, s_actions, S_COUNT, S_IDLE);
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "表头通配规则应命中");
    CHECK(LibFsmTableCurrent(&fsm) == S_ERR, "状态 = %d，期望 %d（后面的规则被吃掉）", (int)LibFsmTableCurrent(&fsm), (int)S_ERR);
    CHECK_TRACE("i3E");

    /* 通配规则被守卫拒绝 → 落到下一条通配规则 */
    InitFsm(&fsm, &ctx, guarded, 2u, s_actions, S_COUNT, S_RUN);
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_RESET, NULL) == true, "守卫拒绝首条通配规则后应落到下一条");
    CHECK(LibFsmTableCurrent(&fsm) == S_IDLE, "状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_IDLE);
    CHECK_TRACE("r5I");
}

/*============================ 12、13：状态值域保护 / 动作表为 NULL ============================*/

static void TestStateRange(void)
{
    /* 状态值 9 / -2 都落在动作表 [0,3) 之外：转移照常发生，进入退出动作静默跳过
     * （-1 是框架的 LIB_FSM_TABLE_INVALID_STATE 哨兵，故这里用 -2 做负值样本） */
    static const LibFsmTableTransition_s table[] = {
        {S_IDLE, E_START, 9, NULL, ActStart, LIB_FSM_TABLE_TRANS_EXTERNAL},
        {9, E_STOP, -2, NULL, ActStop, LIB_FSM_TABLE_TRANS_EXTERNAL},
        {-2, E_FAULT, S_RUN, NULL, NULL, LIB_FSM_TABLE_TRANS_EXTERNAL},
    };
    /* 下面两项只看"状态动作有没有被调用"，转移动作取 NULL 以免污染轨迹 */
    static const LibFsmTableTransition_s silent_table[] = {
        {S_IDLE, E_START, S_RUN, NULL, NULL, LIB_FSM_TABLE_TRANS_EXTERNAL},
    };
    test_ctx_s ctx = {0};
    LibFsmTableInstance_s fsm;

    InitFsm(&fsm, &ctx, table, 3u, s_actions, S_COUNT, S_IDLE);

    /* 越界目标状态：退出 IDLE('i') 与转移动作('1') 照常，进入 9 无动作 */
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "目标状态越界时转移本身应发生");
    CHECK(LibFsmTableCurrent(&fsm) == 9, "状态 = %d，期望 9", (int)LibFsmTableCurrent(&fsm));
    CHECK_TRACE("i1");

    /* 越界源状态 + 负目标状态：退出/进入都跳过，只剩转移动作('2') */
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_STOP, NULL) == true, "源状态越界时转移仍应匹配");
    CHECK(LibFsmTableCurrent(&fsm) == -2, "状态 = %d，期望 -2", (int)LibFsmTableCurrent(&fsm));
    CHECK_TRACE("2");

    /* 负源状态：进入正常状态 RUN，进入动作照常执行 */
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_FAULT, NULL) == true, "负源状态也能匹配到规则");
    CHECK(LibFsmTableCurrent(&fsm) == S_RUN, "状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_RUN);
    CHECK_TRACE("R");

    /* state_count = 0：动作表按"没有动作"处理 */
    InitFsm(&fsm, &ctx, silent_table, 1u, s_actions, 0u, S_IDLE);
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "state_count 为 0 不影响转移");
    CHECK(LibFsmTableCurrent(&fsm) == S_RUN, "状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_RUN);
    CHECK(g_trace_len == 0, "state_count 为 0 时不得调用任何状态动作，轨迹 = \"%s\"", g_trace);

    /* state_actions 整体为 NULL：只做转移 */
    InitFsm(&fsm, &ctx, silent_table, 1u, NULL, S_COUNT, S_IDLE);
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "动作表为 NULL 不影响转移");
    CHECK(LibFsmTableCurrent(&fsm) == S_RUN, "状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_RUN);
    CHECK(g_trace_len == 0, "动作表为 NULL 时不得调用任何状态动作，轨迹 = \"%s\"", g_trace);
}

/*============================ 10、16：空表 / 空指针 / 哨兵 ============================*/

static void TestNoTable(void)
{
    test_ctx_s ctx = {0};
    LibFsmTableInstance_s fsm;

    /* transitions 为 NULL */
    InitFsm(&fsm, &ctx, NULL, 0u, s_actions, S_COUNT, S_IDLE);
    TraceReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == false, "转移表为 NULL 时应返回 false");
    CHECK(LibFsmTableCurrent(&fsm) == S_IDLE, "转移表为 NULL 时状态不应改变");

    /* 条数为 0（表指针非 NULL）：同样直接挡掉 */
    InitFsm(&fsm, &ctx, s_main_table, 0u, s_actions, S_COUNT, S_IDLE);
    CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == false, "条数为 0 时应返回 false");
    CHECK(g_trace_len == 0, "空表时不得调用任何回调，轨迹 = \"%s\"", g_trace);

    /* 实例指针为 NULL：不崩，返回 false / 哨兵 */
    CHECK(LibFsmTableDispatch(NULL, E_START, NULL) == false, "NULL 实例应返回 false");
    CHECK(LibFsmTableCurrent(NULL) == LIB_FSM_TABLE_INVALID_STATE, "NULL 实例应返回哨兵 %d，实际 %d", (int)LIB_FSM_TABLE_INVALID_STATE,
          (int)LibFsmTableCurrent(NULL));
    CHECK(LibFsmTableCurrent(NULL) != S_IDLE, "哨兵不得与合法状态 0 混淆");
    LibFsmTableInit(NULL, NULL); /* 只要求不崩 */

    /* 配置指针为 NULL：实例保持原样 */
    InitFsm(&fsm, &ctx, s_main_table, MAIN_TABLE_N, s_actions, S_COUNT, S_IDLE);
    LibFsmTableInit(&fsm, NULL);
    CHECK(LibFsmTableCurrent(&fsm) == S_IDLE, "配置为 NULL 时实例不应被改动");
    CHECK(fsm.transition_count == MAIN_TABLE_N, "配置为 NULL 时实例的表指针不应被清空");
}

/*============================ 14：多实例共享同一张表 ============================*/

static void TestMultiInstance(void)
{
    test_ctx_s ctx_a = {0};
    test_ctx_s ctx_b = {0};
    test_ev_s ev = {.code = 1};
    LibFsmTableInstance_s a;
    LibFsmTableInstance_s b;

    InitFsm(&a, &ctx_a, s_main_table, MAIN_TABLE_N, s_actions, S_COUNT, S_IDLE);
    InitFsm(&b, &ctx_b, s_main_table, MAIN_TABLE_N, s_actions, S_COUNT, S_IDLE);

    TraceReset();
    CHECK(LibFsmTableDispatch(&a, E_START, &ev) == true, "实例 A 应发生转移");
    CHECK(LibFsmTableCurrent(&a) == S_RUN, "实例 A 状态 = %d", (int)LibFsmTableCurrent(&a));
    CHECK(LibFsmTableCurrent(&b) == S_IDLE, "实例 B 不应受影响，状态 = %d", (int)LibFsmTableCurrent(&b));
    CHECK(ctx_a.action_cnt == 1, "实例 A 的动作次数 = %d，期望 1", ctx_a.action_cnt);
    CHECK(ctx_b.action_cnt == 0, "实例 B 的动作次数 = %d，期望 0", ctx_b.action_cnt);

    CHECK(LibFsmTableDispatch(&b, E_START, &ev) == true, "实例 B 应发生转移");
    CHECK(LibFsmTableCurrent(&b) == S_RUN, "实例 B 状态 = %d", (int)LibFsmTableCurrent(&b));
    CHECK(ctx_b.action_cnt == 1, "实例 B 的动作次数 = %d，期望 1", ctx_b.action_cnt);
}

/*============================ 15：顺序扫描 + 首条即停 ============================*/

static void TestScanOrder(void)
{
    LibFsmTableTransition_s table[20];
    test_ctx_s ctx = {0};
    LibFsmTableInstance_s fsm;
    const int accept_idx[3] = {0, 4, 19};
    uint32_t i;
    int k;

    for (k = 0; k < 3; k++)
    {
        for (i = 0u; i < 20u; i++)
        {
            table[i].from = S_IDLE;
            table[i].event = E_START;
            table[i].to = (LibFsmTableState_t)(i + 1);
            table[i].guard = GuardCountReject;
            table[i].action = NULL;
            table[i].kind = LIB_FSM_TABLE_TRANS_EXTERNAL;
        }
        table[accept_idx[k]].to = S_RUN;
        table[accept_idx[k]].guard = GuardCountAccept;
        table[accept_idx[k]].action = ActStart;

        ctx.guard_calls = 0;
        ctx.action_cnt = 0;
        InitFsm(&fsm, &ctx, table, 20u, NULL, 0u, S_IDLE);
        TraceReset();

        CHECK(LibFsmTableDispatch(&fsm, E_START, NULL) == true, "第 %d 条放行时应发生转移", accept_idx[k]);
        CHECK(ctx.guard_calls == accept_idx[k] + 1, "命中第 %d 条时守卫应调用 %d 次，实际 %d 次", accept_idx[k], accept_idx[k] + 1, ctx.guard_calls);
        CHECK(LibFsmTableCurrent(&fsm) == S_RUN, "状态 = %d，期望 %d", (int)LibFsmTableCurrent(&fsm), (int)S_RUN);
        CHECK(ctx.action_cnt == 1, "转移动作应只执行一次，实际 %d 次", ctx.action_cnt);
    }
}

/*============================ 17：跟踪钩子 ============================*/

static void TestTrace(void)
{
    static const LibFsmTableTransition_s internal_tbl[] = {
        {S_RUN, E_TICK, S_RUN, NULL, ActTick, LIB_FSM_TABLE_TRANS_INTERNAL},
    };
    test_ctx_s ctx = {0};
    test_ev_s ev = {.code = 1};
    LibFsmTableInstance_s fsm;

    InitFsm(&fsm, &ctx, s_main_table, MAIN_TABLE_N, s_actions, S_COUNT, S_IDLE);
    fsm.trace_fn = TraceHook;

    /* 命中：IDLE → RUN */
    TraceLogReset();
    CHECK(LibFsmTableDispatch(&fsm, E_START, &ev) == true, "应发生转移");
    CHECK(g_tr_n == 1, "跟踪记录数 = %d，期望 1", g_tr_n);
    CHECK(RecIs(0, S_IDLE, E_START, S_RUN, 1), "记录 0 = {from:%d ev:%d to:%d tr:%d}", g_tr[0].from, g_tr[0].event, g_tr[0].to, g_tr[0].transitioned);

    /* 命中：RUN → ERR */
    TraceLogReset();
    CHECK(LibFsmTableDispatch(&fsm, E_FAULT, NULL) == true, "RUN 下 E_FAULT 应转移到 ERR");
    CHECK(RecIs(0, S_RUN, E_FAULT, S_ERR, 1), "记录 0 = {from:%d ev:%d to:%d tr:%d}", g_tr[0].from, g_tr[0].event, g_tr[0].to, g_tr[0].transitioned);

    /* 未命中：也报一次，from == to == 当前状态、transitioned = false */
    TraceLogReset();
    CHECK(LibFsmTableDispatch(&fsm, E_STOP, NULL) == false, "ERR 下 E_STOP 无转移");
    CHECK(g_tr_n == 1, "未命中也要报一次，记录数 = %d，期望 1", g_tr_n);
    CHECK(RecIs(0, S_ERR, E_STOP, S_ERR, 0), "记录 0 = {from:%d ev:%d to:%d tr:%d}", g_tr[0].from, g_tr[0].event, g_tr[0].to, g_tr[0].transitioned);

    /* 内部转移：from == to == 当前状态 */
    InitFsm(&fsm, &ctx, internal_tbl, 1u, s_actions, S_COUNT, S_RUN);
    fsm.trace_fn = TraceHook;
    TraceLogReset();
    CHECK(LibFsmTableDispatch(&fsm, E_TICK, NULL) == true, "内部转移应报告发生了转移");
    CHECK(RecIs(0, S_RUN, E_TICK, S_RUN, 1), "记录 0 = {from:%d ev:%d to:%d tr:%d}", g_tr[0].from, g_tr[0].event, g_tr[0].to, g_tr[0].transitioned);

    /* ctx 原样回传：钩子每次都数到（上面共派发 4 次） */
    CHECK(ctx.trace_calls == 4, "钩子调用次数 = %d，期望 4", ctx.trace_calls);
}

int main(void)
{
    printf("lib_fsm_table PC 端检验\n");

    TestBasic();
    TestStart();
    TestFirstMatch();
    TestGuardReject();
    TestSelfTransition();
    TestInternalTransition();
    TestAnyWildcard();
    TestStateRange();
    TestNoTable();
    TestMultiInstance();
    TestScanOrder();
    TestTrace();

    printf("checks=%d fails=%d -> %s\n", g_checks, g_fails, g_fails ? "FAIL" : "PASS");
    return g_fails ? 1 : 0;
}
