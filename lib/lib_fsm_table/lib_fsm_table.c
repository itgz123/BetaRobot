/**
 * @file lib_fsm_table.c
 * @brief 表驱动通用状态机实现
 * @author TRW
 * @date 2026-10-08
 *
 * @note 全部逻辑就在 LibFsmTableDispatch 一处：线性扫描转移表，取第一条 from/event 命中
 *       且守卫通过的规则，按 UML 顺序执行 退出旧状态 → 转移动作 → 切换状态 → 进入新状态。
 *       表一般只有几条到几十条，线性扫描比建索引表更简单、不占 RAM，这是本模块的取舍。
 * @note 进入/退出动作表按下标直接索引，故查表前必须先做状态值域判断（见 FsmTableRunStateAction），
 *       越界状态静默跳过而不是读脏内存。
 * @note 不含时间源、不含队列：超时与事件缓冲都留给调用方，本文件是纯逻辑、可 PC 单测。
 */

#include <stdbool.h>
#include <stddef.h>

#include "lib_fsm_table.h"

#ifndef LIB_FSM_TABLE_STANDALONE
#include "app_cfg.h"
#endif

#ifdef LIB_FSM_TABLE_USED

/*============================ 私有函数 ============================*/

/**
 * @brief 执行某状态的进入或退出动作
 * @param inst  状态机实例
 * @param state 状态值
 * @param entry true=进入动作 false=退出动作
 * @note 动作表整体为 NULL、状态值越界（含负值）、该项动作为 NULL 时静默跳过；
 *       值域判断放在数组下标之前，负值在这里就被拦下，不会出现负下标。
 */
static void FsmTableRunStateAction(const LibFsmTableInstance_s *inst, LibFsmTableState_t state, bool entry)
{
    LibFsmTableStateAction_fn action;

    if (inst->state_actions == NULL)
    {
        return;
    }
    if (state < 0 || (uint32_t)state >= inst->state_count)
    {
        return;
    }

    action = entry ? inst->state_actions[state].entry : inst->state_actions[state].exit;
    if (action != NULL)
    {
        action(inst->ctx);
    }
}

/**
 * @brief 调一次跟踪钩子（没配钩子就什么都不做）
 * @param inst         状态机实例
 * @param from         转移前状态
 * @param event        本次事件
 * @param to           转移后状态
 * @param transitioned 是否发生了转移
 */
static void FsmTableRunTrace(const LibFsmTableInstance_s *inst, LibFsmTableState_t from, LibFsmTableEvent_t event, LibFsmTableState_t to, bool transitioned)
{
    if (inst->trace_fn != NULL)
    {
        inst->trace_fn(inst->ctx, from, event, to, transitioned);
    }
}

/**
 * @brief 一条规则是否匹配当前状态与本次事件（含通配符）
 * @note 通配符只放宽"相等"，不影响守卫与优先级：通配符规则同样是"第一条命中即停"
 */
static bool FsmTableMatch(const LibFsmTableInstance_s *inst, const LibFsmTableTransition_s *transition, LibFsmTableEvent_t event)
{
    bool state_match = (transition->from == LIB_FSM_TABLE_ANY_STATE) || (transition->from == inst->current);
    bool event_match = (transition->event == LIB_FSM_TABLE_ANY_EVENT) || (transition->event == event);

    return state_match && event_match;
}

/*============================ 公开接口实现 ============================*/

void LibFsmTableInit(LibFsmTableInstance_s *inst, const LibFsmTableConfig_s *cfg)
{
    if (inst == NULL || cfg == NULL)
    {
        return;
    }

    inst->current = cfg->initial_state;
    inst->transitions = cfg->transitions;
    inst->transition_count = cfg->transition_count;
    inst->state_actions = cfg->state_actions;
    inst->state_count = cfg->state_count;
    inst->ctx = cfg->ctx;
    inst->trace_fn = cfg->trace_fn;
}

void LibFsmTableStart(LibFsmTableInstance_s *inst)
{
    if (inst == NULL)
    {
        return;
    }

    /* 不记"已启动"标志：重复调用就会重复执行进入动作，是否允许由调用方把握 */
    FsmTableRunStateAction(inst, inst->current, true);
}

bool LibFsmTableDispatch(LibFsmTableInstance_s *inst, LibFsmTableEvent_t event, const void *event_data)
{
    const LibFsmTableTransition_s *transition;
    LibFsmTableState_t old_state;
    uint32_t i;

    if (inst == NULL || inst->transitions == NULL || inst->transition_count == 0u)
    {
        return false;
    }

    for (i = 0u; i < inst->transition_count; i++)
    {
        transition = &inst->transitions[i];

        /* 1. 源状态与事件都要对上（通配符等价于"恒相等"） */
        if (!FsmTableMatch(inst, transition, event))
        {
            continue;
        }

        /* 2. 守卫（可省）：不通过就换下一条候选规则继续试 */
        if (transition->guard != NULL && !transition->guard(inst->ctx, event_data))
        {
            continue;
        }

        old_state = inst->current;

        /* 3. 内部转移：状态不变，只执行转移动作（不退出、不进入） */
        if (transition->kind == LIB_FSM_TABLE_TRANS_INTERNAL)
        {
            if (transition->action != NULL)
            {
                transition->action(inst->ctx, event_data);
            }
            FsmTableRunTrace(inst, old_state, event, old_state, true);
            return true;
        }

        /* 4. 外部转移：退出旧状态 → 转移动作 → 切换状态 → 进入新状态 */
        FsmTableRunStateAction(inst, old_state, false);

        if (transition->action != NULL)
        {
            transition->action(inst->ctx, event_data);
        }

        inst->current = transition->to;

        FsmTableRunStateAction(inst, inst->current, true);

        FsmTableRunTrace(inst, old_state, event, inst->current, true);

        return true; /* 一次事件只处理第一条命中的转移 */
    }

    /* 没有命中的转移：也报一次，方便定位"为什么没转移" */
    FsmTableRunTrace(inst, inst->current, event, inst->current, false);

    return false;
}

LibFsmTableState_t LibFsmTableCurrent(const LibFsmTableInstance_s *inst)
{
    return (inst != NULL) ? inst->current : LIB_FSM_TABLE_INVALID_STATE;
}

#endif /* LIB_FSM_TABLE_USED */
