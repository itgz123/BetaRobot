/**
 * @file drv_tx_slot.c
 * @brief 串口 DMA 发送槽池实现（状态机 + 在途槽唯一 + 完成回调补发）
 *
 * @note 全部逻辑就是一张 4 状态的私有转移函数 TxSlotTransition()：
 *       借出/提交/完成/中止/补发/回收都只是给它派一个事件。
 *       状态副作用只有一处 —— 进入 SEND 认领 in_flight、退出 SEND 归还 in_flight。
 * @note 并发模型：单写者（任务）+ 回调（ISR/任务）。任务侧"判在途 + 启动"必须在
 *       临界区内完成，否则完成中断会在两步之间插进来；回调侧本身已经在中断里，
 *       但同样走临界区，这样万一 bsp 从任务上下文回调（发送卡死自恢复）也安全。
 *       PRIMASK 保存/恢复支持嵌套：临界区内再触发一次错误回调不会提前开中断。
 * @note 本文件不含日志、不含时间源、不依赖 bsp/lib：协议与日志留给调用方，
 *       换传输（UART/CAN/虚拟口）只换 transport.start 的实现。
 */

#include "drv_tx_slot.h"

#include <stddef.h>

#ifdef DRV_TX_SLOT_STANDALONE
/* PC 单测：没有中断，临界区退化为空操作 */
static inline uint32_t TxSlotCriticalEnter(void)
{
    return 0u;
}
static inline void TxSlotCriticalExit(uint32_t primask)
{
    (void)primask;
}
#else
#include "main.h" // __get_PRIMASK / __disable_irq / __enable_irq
static inline uint32_t TxSlotCriticalEnter(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}
static inline void TxSlotCriticalExit(uint32_t primask)
{
    /* 只在原本开着中断时恢复：支持在 ISR / 外层临界区里嵌套调用 */
    if (primask == 0u)
    {
        __enable_irq();
    }
}
#endif

/*============================================
 *              私有事件与转移
 *============================================*/

typedef enum : uint8_t
{
    TX_SLOT_EV_CLAIM = 0,    // FREE → WRITE
    TX_SLOT_EV_RELEASE,      // WRITE → FREE（放弃写入）
    TX_SLOT_EV_SUBMIT_OK,    // WRITE → SEND（启动成功）
    TX_SLOT_EV_SUBMIT_BUSY,  // WRITE → WAIT_SEND（传输层忙）
    TX_SLOT_EV_SUBMIT_FAIL,  // WRITE → FREE（启动失败）
    TX_SLOT_EV_DONE,         // SEND → FREE（完成回调）
    TX_SLOT_EV_ABORT,        // SEND → FREE（错误回调确认已中止）
    TX_SLOT_EV_DEQUEUE_OK,   // WAIT_SEND → SEND（补发启动成功）
    TX_SLOT_EV_DEQUEUE_FAIL, // WAIT_SEND → FREE（补发启动失败/忙，丢弃）
    TX_SLOT_EV_RECLAIM,      // WAIT_SEND → FREE（孤儿回收）
} TxSlotEvent_e;

/**
 * @brief 在槽上派发一个事件，命中则转移并执行副作用
 * @return true=发生了转移 false=当前状态不处理该事件
 * @note 顺序与 UML 一致：退出旧状态（归还 in_flight）→ 切换 → 进入新状态（认领 in_flight）。
 *       状态转移表就是下面这个嵌套 switch；表很小，写成跳转表比查数组更直白。
 */
static bool TxSlotTransition(TxSlot_s *slot, TxSlotEvent_e event)
{
    TxSlotState_e from = slot->state;
    TxSlotState_e to;

    switch (from)
    {
    case TX_SLOT_FREE:
        if (event != TX_SLOT_EV_CLAIM)
        {
            return false;
        }
        to = TX_SLOT_WRITE;
        break;

    case TX_SLOT_WRITE:
        switch (event)
        {
        case TX_SLOT_EV_RELEASE:
        case TX_SLOT_EV_SUBMIT_FAIL:
            to = TX_SLOT_FREE;
            break;
        case TX_SLOT_EV_SUBMIT_OK:
            to = TX_SLOT_SEND;
            break;
        case TX_SLOT_EV_SUBMIT_BUSY:
            to = TX_SLOT_WAIT_SEND;
            break;
        default:
            return false;
        }
        break;

    case TX_SLOT_SEND:
        if (event != TX_SLOT_EV_DONE && event != TX_SLOT_EV_ABORT)
        {
            return false;
        }
        to = TX_SLOT_FREE;
        break;

    case TX_SLOT_WAIT_SEND:
        if (event == TX_SLOT_EV_DEQUEUE_OK)
        {
            to = TX_SLOT_SEND;
        }
        else if (event == TX_SLOT_EV_DEQUEUE_FAIL || event == TX_SLOT_EV_RECLAIM)
        {
            to = TX_SLOT_FREE;
        }
        else
        {
            return false;
        }
        break;

    default:
        return false;
    }

    /* 退出 SEND：归还 in_flight（只归还自己的，幂等） */
    if (from == TX_SLOT_SEND && slot->pool != NULL && slot->pool->in_flight == slot)
    {
        slot->pool->in_flight = NULL;
    }

    slot->state = to;

    /* 进入 SEND：认领 in_flight（此刻才交给传输层，见文件头"先启动、成功才认领"） */
    if (to == TX_SLOT_SEND && slot->pool != NULL)
    {
        slot->pool->in_flight = slot;
    }

    return true;
}

/**
 * @brief 按数组顺序补发第一个 WAIT_SEND 槽（调用方须已临界 / 处于 ISR）
 * @note 无论启动结果如何都只处理一个槽：成功则等下一次完成回调，失败则丢弃本帧。
 *       这里沿用原两模块的语义 —— 补发时"忙"也算失败（若真没有在途却仍忙，
 *       说明传输层状态不自洽，留着这个槽也不会有人再来补发，直接腾出来更安全）。
 */
static void TxSlotDequeueLocked(TxSlotPool_s *pool)
{
    if (pool->in_flight != NULL)
    {
        return; // 有在途：等它的完成回调再来补发
    }

    for (uint8_t i = 0; i < pool->slot_count; i++)
    {
        TxSlot_s *slot = &pool->slots[i];
        if (slot->state != TX_SLOT_WAIT_SEND)
        {
            continue;
        }

        TxSlotStartResult_e ret = pool->transport.start(pool->transport.transport, slot->buf, slot->len);
        if (ret == TX_SLOT_START_OK)
        {
            (void)TxSlotTransition(slot, TX_SLOT_EV_DEQUEUE_OK);
        }
        else
        {
            (void)TxSlotTransition(slot, TX_SLOT_EV_DEQUEUE_FAIL);
            pool->start_fail_cnt++;
            pool->dequeue_fail_cnt++;
        }
        return; // 一次只补发一个
    }
}

/*============================================
 *              外部接口
 *============================================*/

void TxSlotPoolInit(TxSlotPool_s *pool, const TxSlotTransport_s *transport, TxSlot_s *slots, uint8_t slot_count, uint8_t *buf_base, uint16_t slot_buf_size)
{
    if (pool == NULL)
    {
        return;
    }

    pool->transport.start = NULL;
    pool->transport.transport = NULL;
    pool->slots = NULL;
    pool->slot_count = 0;
    pool->in_flight = NULL;
    pool->reclaim_cnt = 0;
    pool->start_fail_cnt = 0;
    pool->dequeue_fail_cnt = 0;

    if (transport == NULL || transport->start == NULL || slots == NULL || slot_count == 0 || buf_base == NULL || slot_buf_size == 0)
    {
        return;
    }

    pool->transport = *transport;
    pool->slots = slots;
    pool->slot_count = slot_count;

    for (uint8_t i = 0; i < slot_count; i++)
    {
        slots[i].buf = &buf_base[(uint32_t)i * slot_buf_size];
        slots[i].cap = slot_buf_size;
        slots[i].len = 0;
        slots[i].pool = pool;
        slots[i].state = TX_SLOT_FREE;
    }
}

TxSlot_s *TxSlotClaim(TxSlotPool_s *pool)
{
    if (pool == NULL || pool->slots == NULL)
    {
        return NULL;
    }

    for (uint8_t i = 0; i < pool->slot_count; i++)
    {
        TxSlot_s *slot = &pool->slots[i];
        if (slot->state == TX_SLOT_FREE)
        {
            /* 单写者独占 FREE→WRITE；WRITE 槽对 ISR 不可见，故格式化不必进临界区 */
            slot->state = TX_SLOT_WRITE;
            slot->len = 0;
            return slot;
        }
    }

    return NULL;
}

TxSlotSubmitResult_e TxSlotSubmit(TxSlot_s *slot)
{
    if (slot == NULL || slot->pool == NULL || slot->pool->transport.start == NULL || slot->state != TX_SLOT_WRITE)
    {
        return TX_SLOT_SUBMIT_FAILED;
    }

    TxSlotPool_s *pool = slot->pool;
    TxSlotSubmitResult_e result;

    uint32_t primask = TxSlotCriticalEnter();

    if (pool->in_flight != NULL)
    {
        /* 已有在途：排队，等它的完成回调补发 */
        (void)TxSlotTransition(slot, TX_SLOT_EV_SUBMIT_BUSY);
        result = TX_SLOT_SUBMIT_QUEUED;
    }
    else
    {
        /* 先启动、成功才认领：USARTTransmit 内部可能触发 err_callback（发送卡死自恢复），
         * 此刻 in_flight 仍为空，那个回调碰不到本帧（详见文件头）。 */
        TxSlotStartResult_e ret = pool->transport.start(pool->transport.transport, slot->buf, slot->len);
        if (ret == TX_SLOT_START_OK)
        {
            (void)TxSlotTransition(slot, TX_SLOT_EV_SUBMIT_OK);
            result = TX_SLOT_SUBMIT_SENT;
        }
        else if (ret == TX_SLOT_START_BUSY)
        {
            (void)TxSlotTransition(slot, TX_SLOT_EV_SUBMIT_BUSY);
            result = TX_SLOT_SUBMIT_QUEUED;
        }
        else
        {
            (void)TxSlotTransition(slot, TX_SLOT_EV_SUBMIT_FAIL);
            pool->start_fail_cnt++;
            result = TX_SLOT_SUBMIT_FAILED;
        }
    }

    TxSlotCriticalExit(primask);
    return result;
}

void TxSlotRelease(TxSlot_s *slot)
{
    if (slot == NULL || slot->state != TX_SLOT_WRITE)
    {
        return;
    }
    (void)TxSlotTransition(slot, TX_SLOT_EV_RELEASE);
}

void TxSlotOnTxCplt(TxSlotPool_s *pool)
{
    if (pool == NULL || pool->slots == NULL)
    {
        return;
    }

    uint32_t primask = TxSlotCriticalEnter();

    if (pool->in_flight != NULL)
    {
        /* 已在临界区：这里的 volatile 限定只是给调试器看的，丢掉它没有副作用 */
        (void)TxSlotTransition((TxSlot_s *)pool->in_flight, TX_SLOT_EV_DONE);
    }
    /* 没有在途也照样扫：错误回调释放过在途槽后，真正的完成回调仍会到达，
     * 正好用它把这期间积压的排队帧带出去。 */
    TxSlotDequeueLocked(pool);

    TxSlotCriticalExit(primask);
}

void TxSlotOnError(TxSlotPool_s *pool, bool tx_aborted)
{
    if (pool == NULL || !tx_aborted || pool->in_flight == NULL)
    {
        return; // 幂等：没有在途槽（已释放过/本就空闲）时什么都不做
    }

    uint32_t primask = TxSlotCriticalEnter();

    if (pool->in_flight != NULL)
    {
        (void)TxSlotTransition((TxSlot_s *)pool->in_flight, TX_SLOT_EV_ABORT);
    }

    TxSlotCriticalExit(primask);
}

uint8_t TxSlotReclaimOrphans(TxSlotPool_s *pool)
{
    if (pool == NULL || pool->slots == NULL)
    {
        return 0;
    }

    uint8_t reclaimed = 0;

    uint32_t primask = TxSlotCriticalEnter();

    /* 有在途时一个都不动：排队的槽可能正排在补发的路上。
     * 临界区同时挡住"补发"与"回收"对同一个 WAIT_SEND 槽的竞争。 */
    if (pool->in_flight == NULL)
    {
        for (uint8_t i = 0; i < pool->slot_count; i++)
        {
            if (TxSlotTransition(&pool->slots[i], TX_SLOT_EV_RECLAIM))
            {
                reclaimed++;
            }
        }
    }

    TxSlotCriticalExit(primask);

    pool->reclaim_cnt += reclaimed;
    return reclaimed;
}

bool TxSlotHasInFlight(const TxSlotPool_s *pool)
{
    return (pool != NULL) && (pool->in_flight != NULL);
}

uint8_t TxSlotCount(const TxSlotPool_s *pool, TxSlotState_e state)
{
    if (pool == NULL || pool->slots == NULL)
    {
        return 0;
    }

    uint8_t count = 0;
    for (uint8_t i = 0; i < pool->slot_count; i++)
    {
        if (pool->slots[i].state == state)
        {
            count++;
        }
    }
    return count;
}
