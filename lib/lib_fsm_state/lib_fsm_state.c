/**
 * @file lib_fsm_state.c
 * @brief 状态结构体 + 虚表通用状态机的实现
 * @author TRW
 * @date 2026-10-08
 *
 * @note 本文件只有三件事：
 *       1. Dispatch：调当前状态的 handle 拿 next，再按 next 的取值决定 不退不进 / 内部转移 /
 *          外部转移（退出 → 切换 → 进入）/ 外部自转移（退出 → 再进入自身）
 *       2. DefaultHandle：把"状态内转移表"翻译成同一个 handle 协议（顺序扫描、首条命中即停）
 *       3. Init/Current/IsIn/CurrentName/SelfExternal：落初始状态、读当前状态、自转移标记
 *       动作写在 handle 里，所以默认模式下副作用先于 exit 发生；
 *       开 LIB_FSM_STATE_UML_ORDER 后，表动作由 Dispatch 在 exit 之后执行（详见头文件）。
 * @note 返回值是 LibFsmStateResult_e：区分"没人处理"和"处理了但没切换"，
 *       调用方不会再被"false 但其实动作已经跑了"误导。
 */

#include <stdbool.h>
#include <stddef.h>

#include "lib_fsm_state.h"

#ifndef LIB_FSM_STATE_STANDALONE
#include "app_cfg.h"
#endif

#ifdef LIB_FSM_STATE_USED

#ifdef LIB_FSM_STATE_PARANOID
#include <assert.h>
#endif
#ifdef LIB_FSM_STATE_TRACE
#include <stdio.h>
#endif

/*============================ 私有定义 ============================*/

/**
 * @brief 外部自转移标记
 * @note 用一个不可能成为真实节点地址的值当哨兵；LibFsmStateSelfExternal 把它交给用户，
 *       Dispatch 认到它就做"退出 → 再进入同一个状态"。任何路径都不会解引用它。
 */
#define FSM_STATE_SELF_MARK ((const LibFsmStateNode_s *)(uintptr_t)1)

/*============================ 私有函数 ============================*/

/**
 * @brief 取状态名（调试打印用，没写名字就报 "?"）
 */
#ifdef LIB_FSM_STATE_TRACE
static const char *FsmStateNodeName(const LibFsmStateNode_s *node)
{
    return (node != NULL && node->name != NULL) ? node->name : "?";
}

/**
 * @brief 取结局名（调试打印用）
 */
static const char *FsmStateResultName(LibFsmStateResult_e result)
{
    switch (result)
    {
    case LIB_FSM_STATE_RESULT_SWITCHED:
        return "SWITCHED";
    case LIB_FSM_STATE_RESULT_INTERNAL:
        return "INTERNAL";
    default:
        return "NOT_HANDLED";
    }
}
#endif /* LIB_FSM_STATE_TRACE */

/**
 * @brief 取某状态的 entry/exit 钩子并执行
 * @param fsm   状态机（原样传给钩子，钩子用 fsm->ctx 取上下文）
 * @param node  状态节点（可为 NULL）
 * @param entry true=entry false=exit
 * @note 节点为 NULL、ops 为 NULL、对应钩子为 NULL 时静默跳过
 */
static void FsmStateRunHook(LibFsmStateMachine_s *fsm, const LibFsmStateNode_s *node, bool entry)
{
    LibFsmStateHook_fn hook;

    if (node == NULL || node->ops == NULL)
    {
        return;
    }

    hook = entry ? node->ops->entry : node->ops->exit;
    if (hook != NULL)
    {
        hook(fsm);
    }
}

/**
 * @brief 取某状态应该用的 handle
 * @param node 状态节点（可为 NULL）
 * @return 节点自带的 handle；没写 handle 但有转移表时回退到 LibFsmStateDefaultHandle；都不行返回 NULL
 */
static LibFsmStateHandle_fn FsmStateNodeHandle(const LibFsmStateNode_s *node)
{
    if (node == NULL)
    {
        return NULL;
    }
    if (node->ops != NULL && node->ops->handle != NULL)
    {
        return node->ops->handle;
    }
    if (node->transitions != NULL && node->transition_count > 0u)
    {
        return LibFsmStateDefaultHandle;
    }

    return NULL;
}

/**
 * @brief 弱校验：这个指针看起来像不像一个状态节点
 * @param node 待校验的指针（非 NULL）
 * @return 有虚表或有非空转移表 = true
 * @note 只读 node->ops / node->transitions 两个字段，不追指针不比对内容 —— 野指针仍可能崩，
 *       这是**启发式**判断（用来给 error_state 找落点），不是严格校验。
 */
static bool FsmStateLooksLikeNode(const LibFsmStateNode_s *node)
{
    return (node->ops != NULL) || (node->transitions != NULL && node->transition_count > 0u);
}

/**
 * @brief 校验一个状态节点自洽（只在 PARANOID 下编译）
 * @note 拦的是"写了表但没给条数/给了条数却没表""既没有虚表也没有表"这类抄错
 */
#ifdef LIB_FSM_STATE_PARANOID
static void FsmStateCheckNode(const LibFsmStateNode_s *node)
{
    if (node == NULL)
    {
        assert(!"LibFsmStateInit：状态节点不能为 NULL");
        return;
    }

    assert(node->ops != NULL || (node->transitions != NULL && node->transition_count > 0u));

    if (node->transitions == NULL)
    {
        assert(node->transition_count == 0u);
    }
    else
    {
        assert(node->transition_count > 0u);
    }
}
#endif /* LIB_FSM_STATE_PARANOID */

/**
 * @brief 派发收尾：放下重入标志、回调跟踪钩子、（TRACE 开关下）打印一行
 * @return 原样返回 result，方便调用处写成 return FsmStateFinish(...)
 */
static LibFsmStateResult_e FsmStateFinish(LibFsmStateMachine_s *fsm, const LibFsmStateNode_s *from, const LibFsmStateNode_s *to, LibFsmStateEvent_t event,
                                          LibFsmStateResult_e result)
{
    fsm->dispatching = 0u;

    if (fsm->trace_fn != NULL)
    {
        fsm->trace_fn(fsm, from, to, event, result);
    }

#ifdef LIB_FSM_STATE_TRACE
    printf("[fsm] %s --(%d)--> %s : %s\n", FsmStateNodeName(from), (int)event, FsmStateNodeName(to), FsmStateResultName(result));
#endif

    return result;
}

/*============================ 公开接口实现 ============================*/

void LibFsmStateInit(LibFsmStateMachine_s *fsm, const LibFsmStateNode_s *initial, void *ctx)
{
    if (fsm == NULL)
    {
        return;
    }

    /* 重复 Init = 复位：先让旧状态退出（此刻 ctx 还是旧的，资源归旧 ctx 管） */
    if (fsm->current != NULL)
    {
        FsmStateRunHook(fsm, fsm->current, false);
    }

    fsm->current = NULL;
    fsm->ctx = ctx;

    /* 可选字段一律清零：避免"没配过"和"配的是脏值"分不出来 */
    fsm->error_state = NULL;
    fsm->default_state = NULL;
    fsm->trace_fn = NULL;
    fsm->dispatching = 0u;
#ifdef LIB_FSM_STATE_UML_ORDER
    fsm->pending_action = NULL;
#endif

#ifdef LIB_FSM_STATE_PARANOID
    if (initial != NULL)
    {
        FsmStateCheckNode(initial);
    }
#endif

    fsm->current = initial;
    if (initial == NULL)
    {
        return; /* 停在"无状态"：不跑任何钩子，之后 Dispatch 一律 NOT_HANDLED */
    }

    /* 本方案的既有语义：Init 就把初始状态的 entry 跑掉（不是一次转移，故不调 exit） */
    FsmStateRunHook(fsm, initial, true);
}

LibFsmStateResult_e LibFsmStateDispatch(LibFsmStateMachine_s *fsm, LibFsmStateEvent_t event, const void *event_data)
{
    const LibFsmStateNode_s *cur;
    const LibFsmStateNode_s *next;
    LibFsmStateHandle_fn handle;
    LibFsmStateAction_fn pending = NULL;

    if (fsm == NULL || fsm->current == NULL)
    {
        return LIB_FSM_STATE_RESULT_NOT_HANDLED;
    }

    cur = fsm->current;

    if (fsm->dispatching != 0u)
    {
#ifdef LIB_FSM_STATE_PARANOID
        assert(!"LibFsmStateDispatch 重入：回调里不得再派发同一个状态机");
#endif
        return LIB_FSM_STATE_RESULT_NOT_HANDLED; /* 不动外层派发的状态 */
    }
    fsm->dispatching = 1u;

#ifdef LIB_FSM_STATE_UML_ORDER
    fsm->pending_action = NULL;
#endif

    /* 1. 当前状态决策（默认模式下动作写在这里面，所以它先于 exit 执行）；
     *    状态既没有 handle 也没有转移表时按"没人处理"走，好让 default_state 兜住 */
    handle = FsmStateNodeHandle(cur);
    next = (handle != NULL) ? handle(fsm, event, event_data) : NULL;

#ifdef LIB_FSM_STATE_UML_ORDER
    /* UML 模式：默认 handle 把动作挂起，这里取出来，等 exit 之后再执行 */
    pending = fsm->pending_action;
    fsm->pending_action = NULL;
#endif

    /* 2. 目标不像状态节点（写错/指针被踩）：不崩，能配 error_state 就切过去 */
    if (next != NULL && next != FSM_STATE_SELF_MARK && !FsmStateLooksLikeNode(next))
    {
        next = fsm->error_state;
    }

    /* 3. 没人处理：有兜底状态就切过去（兜底就是自身则视为不切换） */
    if (next == NULL)
    {
        next = fsm->default_state;
        if (next == cur)
        {
            next = NULL;
        }
    }

    /* 4. 外部自转移：退出 →（动作）→ 再进入同一个状态 */
    if (next == FSM_STATE_SELF_MARK)
    {
        FsmStateRunHook(fsm, cur, false);
        if (pending != NULL)
        {
            pending(fsm, event_data);
        }
        FsmStateRunHook(fsm, cur, true);
        return FsmStateFinish(fsm, cur, cur, event, LIB_FSM_STATE_RESULT_SWITCHED);
    }

    /* 5. 返回自身 = 不切换（内部转移）：挂起的动作照跑，不退不进 */
    if (next == cur)
    {
        if (pending != NULL)
        {
            pending(fsm, event_data);
        }
        return FsmStateFinish(fsm, cur, cur, event, LIB_FSM_STATE_RESULT_INTERNAL);
    }

    /* 6. 还是没人处理：不切换，但挂起的动作（表里命中过）该跑还是要跑 */
    if (next == NULL)
    {
        if (pending != NULL)
        {
            pending(fsm, event_data);
        }
        return FsmStateFinish(fsm, cur, cur, event, LIB_FSM_STATE_RESULT_NOT_HANDLED);
    }

    /* 7. 外部转移：退出旧状态 →（动作）→ 切换 → 进入新状态 */
    FsmStateRunHook(fsm, cur, false);
    if (pending != NULL)
    {
        pending(fsm, event_data);
    }
    fsm->current = next;
    FsmStateRunHook(fsm, next, true);

    return FsmStateFinish(fsm, cur, next, event, LIB_FSM_STATE_RESULT_SWITCHED);
}

const LibFsmStateNode_s *LibFsmStateCurrent(const LibFsmStateMachine_s *fsm)
{
    return (fsm != NULL) ? fsm->current : NULL;
}

bool LibFsmStateIsIn(const LibFsmStateMachine_s *fsm, const LibFsmStateNode_s *node)
{
    return (fsm != NULL) && (node != NULL) && (fsm->current == node);
}

const char *LibFsmStateCurrentName(const LibFsmStateMachine_s *fsm)
{
    const LibFsmStateNode_s *cur = LibFsmStateCurrent(fsm);

    return (cur != NULL) ? cur->name : NULL; /* 状态没写 name 时这里也是 NULL */
}

const LibFsmStateNode_s *LibFsmStateSelfExternal(void)
{
    return FSM_STATE_SELF_MARK;
}

const LibFsmStateNode_s *LibFsmStateDefaultHandle(LibFsmStateMachine_s *fsm, LibFsmStateEvent_t event, const void *event_data)
{
    const LibFsmStateNode_s *node;
    const LibFsmStateTransition_s *transition;
    uint32_t i;

    if (fsm == NULL || fsm->current == NULL || fsm->current->transitions == NULL)
    {
        return NULL;
    }

    node = fsm->current;

    for (i = 0u; i < node->transition_count; i++)
    {
        transition = &node->transitions[i];

        /* 事件要对上（LIB_FSM_STATE_EVENT_ANY 等价于"恒对上"） */
        if (transition->event != event && transition->event != LIB_FSM_STATE_EVENT_ANY)
        {
            continue;
        }

        /* next 为 NULL 的行是配置错误：跳过它，别执行它的动作（不切换请写状态自身） */
        if (transition->next == NULL)
        {
#ifdef LIB_FSM_STATE_PARANOID
            assert(!"转移表里有 next 为 NULL 的行：不切换请写该状态自身");
#endif
            continue;
        }

        /* 守卫（可省）：不通过就换下一条候选表项继续试 */
        if (transition->guard != NULL && !transition->guard(fsm, event_data))
        {
            continue;
        }

        /* 命中即执行该行的动作；UML 模式下改为挂起，由 Dispatch 在 exit 之后执行 */
        if (transition->action != NULL)
        {
#ifdef LIB_FSM_STATE_UML_ORDER
            if (fsm->dispatching != 0u)
            {
                fsm->pending_action = transition->action;
            }
            else
            {
                transition->action(fsm, event_data); /* 用户直接调用本函数（不在 Dispatch 里）：就地执行 */
            }
#else
            transition->action(fsm, event_data);
#endif
        }

        return transition->next; /* 表顺序即优先级：第一条命中的生效 */
    }

    return NULL; /* 该状态不处理这个事件 */
}

#endif /* LIB_FSM_STATE_USED */
