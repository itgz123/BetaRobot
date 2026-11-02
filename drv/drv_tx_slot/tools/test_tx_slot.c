/**
 * @file test_tx_slot.c
 * @brief PC 端 drv_tx_slot 检验（gcc 编译，Git Bash 下可运行）
 *
 * @note 编译真实实现，用一个假的 transport.start 控制每次"启动 DMA"的结果，
 *       再用 TxSlotOnTxCplt / TxSlotOnError 模拟回调，覆盖：
 *       直发 / 排队 / 补发 / 补发失败 / 填满 / 归还 / 中止 / 幂等 / 孤儿回收 / 顺序。
 * @note 本文件不引用任何 bsp/motor 头，-DDRV_TX_SLOT_STANDALONE 让临界区退化为空。
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "drv_tx_slot.h"

/*============================================
 *              断言与假传输层
 *============================================*/

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                                                                                                            \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        g_checks++;                                                                                                                                            \
        if (!(cond))                                                                                                                                           \
        {                                                                                                                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                                                             \
            g_failures++;                                                                                                                                      \
        }                                                                                                                                                      \
    } while (0)

/* 下一次 start 的返回结果；调用后不回退，测试自己设置 */
static TxSlotStartResult_e g_next_start = TX_SLOT_START_OK;
static int g_start_calls = 0;
static const uint8_t *g_last_start_buf = NULL;
static uint16_t g_last_start_len = 0;

static TxSlotStartResult_e FakeStart(void *transport, const uint8_t *buf, uint16_t len)
{
    (void)transport;
    g_start_calls++;
    g_last_start_buf = buf;
    g_last_start_len = len;
    return g_next_start;
}

/*============================================
 *              公共夹具
 *============================================*/

#define SLOT_COUNT 3
#define SLOT_SIZE 16

static TxSlotPool_s s_pool;
static TxSlot_s s_slots[SLOT_COUNT];
static uint8_t s_bufs[SLOT_COUNT][SLOT_SIZE];

static void TestReset(void)
{
    static const TxSlotTransport_s transport = {
        .start = FakeStart,
        .transport = NULL,
    };
    memset(s_bufs, 0, sizeof(s_bufs));
    TxSlotPoolInit(&s_pool, &transport, s_slots, SLOT_COUNT, &s_bufs[0][0], SLOT_SIZE);
    g_next_start = TX_SLOT_START_OK;
    g_start_calls = 0;
    g_last_start_buf = NULL;
    g_last_start_len = 0;
}

/** 借一个槽、写 1 字节、提交；返回提交结果 */
static TxSlotSubmitResult_e ClaimAndSubmit(char c)
{
    TxSlot_s *slot = TxSlotClaim(&s_pool);
    if (slot == NULL)
    {
        return TX_SLOT_SUBMIT_FAILED; /* 用不到：测试里池永远有空位 */
    }
    slot->buf[0] = (uint8_t)c;
    slot->len = 1;
    return TxSlotSubmit(slot);
}

/*============================================
 *              用例
 *============================================*/

static void TestInitAndClaim(void)
{
    TestReset();

    CHECK(TxSlotCount(&s_pool, TX_SLOT_FREE) == SLOT_COUNT);
    CHECK(!TxSlotHasInFlight(&s_pool));

    /* 各槽缓冲按 base + i*size 切开，互不重叠 */
    CHECK(s_slots[0].buf == &s_bufs[0][0]);
    CHECK(s_slots[1].buf == &s_bufs[1][0]);
    CHECK(s_slots[2].buf == &s_bufs[2][0]);
    CHECK(s_slots[0].cap == SLOT_SIZE);

    TxSlot_s *a = TxSlotClaim(&s_pool);
    CHECK(a == &s_slots[0]);
    CHECK(a->state == TX_SLOT_WRITE);
    CHECK(!TxSlotHasInFlight(&s_pool)); /* 借出不算在途 */

    TxSlotRelease(a);
    CHECK(a->state == TX_SLOT_FREE);
    TxSlotRelease(a); /* 幂等：非 WRITE 再归还无副作用 */
    CHECK(a->state == TX_SLOT_FREE);
    CHECK(TxSlotCount(&s_pool, TX_SLOT_FREE) == SLOT_COUNT);
}

static void TestDirectSendAndCompletion(void)
{
    TestReset();

    TxSlot_s *a = TxSlotClaim(&s_pool);
    CHECK(a != NULL);
    a->buf[0] = 0xAA;
    a->len = 1;
    CHECK(TxSlotSubmit(a) == TX_SLOT_SUBMIT_SENT);
    CHECK(a->state == TX_SLOT_SEND);
    CHECK(TxSlotHasInFlight(&s_pool));
    CHECK(s_pool.in_flight == a);
    CHECK(g_start_calls == 1);
    CHECK(g_last_start_buf == a->buf && g_last_start_len == 1);

    TxSlotOnTxCplt(&s_pool);
    CHECK(a->state == TX_SLOT_FREE);
    CHECK(!TxSlotHasInFlight(&s_pool));
    CHECK(TxSlotCount(&s_pool, TX_SLOT_FREE) == SLOT_COUNT);
}

static void TestQueueAndDrainInOrder(void)
{
    TestReset();

    CHECK(ClaimAndSubmit('A') == TX_SLOT_SUBMIT_SENT);
    CHECK(ClaimAndSubmit('B') == TX_SLOT_SUBMIT_QUEUED);
    CHECK(ClaimAndSubmit('C') == TX_SLOT_SUBMIT_QUEUED);
    CHECK(TxSlotCount(&s_pool, TX_SLOT_WAIT_SEND) == 2);
    CHECK(s_pool.in_flight == &s_slots[0]);

    /* 池满：借不到 */
    CHECK(TxSlotClaim(&s_pool) == NULL);
    CHECK(TxSlotCount(&s_pool, TX_SLOT_FREE) == 0);

    /* 第一次完成：0 归还，补发 1（按数组顺序，先 B） */
    TxSlotOnTxCplt(&s_pool);
    CHECK(s_slots[0].state == TX_SLOT_FREE);
    CHECK(s_slots[1].state == TX_SLOT_SEND);
    CHECK(s_slots[2].state == TX_SLOT_WAIT_SEND);
    CHECK(s_pool.in_flight == &s_slots[1]);
    CHECK(g_last_start_buf == s_slots[1].buf);
    CHECK(s_slots[1].buf[0] == 'B');

    /* 第二次完成：1 归还，补发 2 */
    TxSlotOnTxCplt(&s_pool);
    CHECK(s_slots[1].state == TX_SLOT_FREE);
    CHECK(s_slots[2].state == TX_SLOT_SEND);
    CHECK(s_slots[2].buf[0] == 'C');

    /* 第三次完成：全部归还 */
    TxSlotOnTxCplt(&s_pool);
    CHECK(s_slots[2].state == TX_SLOT_FREE);
    CHECK(!TxSlotHasInFlight(&s_pool));
    CHECK(TxSlotCount(&s_pool, TX_SLOT_FREE) == SLOT_COUNT);
}

static void TestSubmitBusyMakesOrphanThenReclaim(void)
{
    TestReset();

    /* 没有在途，但传输层仍报忙（bsp 的正常背压路径）：进 WAIT_SEND 排队 */
    g_next_start = TX_SLOT_START_BUSY;
    CHECK(ClaimAndSubmit('X') == TX_SLOT_SUBMIT_QUEUED);
    CHECK(s_slots[0].state == TX_SLOT_WAIT_SEND);
    CHECK(!TxSlotHasInFlight(&s_pool));

    /* 排队却无在途 = 孤儿：回收成 FREE，并计数 */
    CHECK(s_pool.reclaim_cnt == 0);
    CHECK(TxSlotReclaimOrphans(&s_pool) == 1);
    CHECK(s_slots[0].state == TX_SLOT_FREE);
    CHECK(s_pool.reclaim_cnt == 1);
    CHECK(TxSlotReclaimOrphans(&s_pool) == 0); /* 没有孤儿了 */
    CHECK(s_pool.reclaim_cnt == 1);
}

static void TestReclaimKeepsQueuedWhenInFlight(void)
{
    TestReset();

    CHECK(ClaimAndSubmit('A') == TX_SLOT_SUBMIT_SENT);
    CHECK(ClaimAndSubmit('B') == TX_SLOT_SUBMIT_QUEUED);

    /* 有在途：排队槽可能正排在补发路上，一个都不能回收 */
    CHECK(TxSlotReclaimOrphans(&s_pool) == 0);
    CHECK(s_slots[1].state == TX_SLOT_WAIT_SEND);
    CHECK(s_pool.reclaim_cnt == 0);
}

static void TestSubmitFailReturnsSlot(void)
{
    TestReset();

    g_next_start = TX_SLOT_START_FAIL;
    CHECK(ClaimAndSubmit('X') == TX_SLOT_SUBMIT_FAILED);
    CHECK(s_slots[0].state == TX_SLOT_FREE);
    CHECK(!TxSlotHasInFlight(&s_pool));
    CHECK(s_pool.start_fail_cnt == 1);

    /* 提交失败后槽能立刻再借出来 */
    CHECK(TxSlotClaim(&s_pool) == &s_slots[0]);
}

static void TestDequeueFailDropsSlot(void)
{
    TestReset();

    CHECK(ClaimAndSubmit('A') == TX_SLOT_SUBMIT_SENT);
    CHECK(ClaimAndSubmit('B') == TX_SLOT_SUBMIT_QUEUED);

    /* 补发时启动失败：排队槽直接丢弃（沿用原两模块语义），不留在 WAIT_SEND */
    g_next_start = TX_SLOT_START_FAIL;
    TxSlotOnTxCplt(&s_pool);
    CHECK(s_slots[0].state == TX_SLOT_FREE);
    CHECK(s_slots[1].state == TX_SLOT_FREE);
    CHECK(!TxSlotHasInFlight(&s_pool));
    CHECK(s_pool.dequeue_fail_cnt == 1);
    CHECK(s_pool.start_fail_cnt == 1);
}

static void TestTxAbortReleasesAndIsIdempotent(void)
{
    TestReset();

    CHECK(ClaimAndSubmit('A') == TX_SLOT_SUBMIT_SENT);
    CHECK(TxSlotHasInFlight(&s_pool));

    /* 错误仍在接收侧：不得释放在途槽 */
    TxSlotOnError(&s_pool, false);
    CHECK(s_slots[0].state == TX_SLOT_SEND);
    CHECK(TxSlotHasInFlight(&s_pool));

    /* 确认已中止：释放 */
    TxSlotOnError(&s_pool, true);
    CHECK(s_slots[0].state == TX_SLOT_FREE);
    CHECK(!TxSlotHasInFlight(&s_pool));

    /* 同一次错误重复送达（硬件错误 + 卡死恢复）：无副作用 */
    TxSlotOnError(&s_pool, true);
    CHECK(s_slots[0].state == TX_SLOT_FREE);
    CHECK(TxSlotCount(&s_pool, TX_SLOT_FREE) == SLOT_COUNT);
}

static void TestCompletionAfterAbortDrainsQueue(void)
{
    TestReset();

    CHECK(ClaimAndSubmit('A') == TX_SLOT_SUBMIT_SENT);
    CHECK(ClaimAndSubmit('B') == TX_SLOT_SUBMIT_QUEUED);

    /* 在途被中止：没有完成回调可等了，排队帧暂时挂住 */
    TxSlotOnError(&s_pool, true);
    CHECK(!TxSlotHasInFlight(&s_pool));
    CHECK(s_slots[1].state == TX_SLOT_WAIT_SEND);

    /* 之后真正的完成回调到达（原地发出过的那一帧），正好把排队帧带出去 */
    TxSlotOnTxCplt(&s_pool);
    CHECK(s_slots[1].state == TX_SLOT_SEND);
    CHECK(s_pool.in_flight == &s_slots[1]);
    CHECK(g_last_start_buf == s_slots[1].buf);
}

static void TestInvalidInputs(void)
{
    TestReset();

    /* 非 WRITE 状态的槽提交/归还都无效，且不改状态 */
    TxSlot_s *a = TxSlotClaim(&s_pool);
    CHECK(a != NULL);
    a->len = 1;
    CHECK(TxSlotSubmit(a) == TX_SLOT_SUBMIT_SENT);
    CHECK(TxSlotSubmit(a) == TX_SLOT_SUBMIT_FAILED); /* 已 SEND，不能重复提交 */
    CHECK(a->state == TX_SLOT_SEND);
    TxSlotRelease(a); /* SEND 上调 Release 无效 */
    CHECK(a->state == TX_SLOT_SEND);
    TxSlotOnTxCplt(&s_pool);
    CHECK(a->state == TX_SLOT_FREE);

    /* NULL 参数不崩 */
    CHECK(TxSlotClaim(NULL) == NULL);
    CHECK(TxSlotSubmit(NULL) == TX_SLOT_SUBMIT_FAILED);
    CHECK(!TxSlotHasInFlight(NULL));
    CHECK(TxSlotCount(NULL, TX_SLOT_FREE) == 0);
    CHECK(TxSlotReclaimOrphans(NULL) == 0);
    TxSlotOnTxCplt(NULL);
    TxSlotOnError(NULL, true);
    TxSlotRelease(NULL);
}

static void TestInvalidInitIsInert(void)
{
    /* 非法配置：池保持不可用，所有接口安全空转 */
    TxSlotPool_s pool;
    static const TxSlotTransport_s transport = {.start = FakeStart, .transport = NULL};
    static TxSlot_s slots[2];

    TxSlotPoolInit(&pool, &transport, slots, 0, &s_bufs[0][0], SLOT_SIZE);
    CHECK(TxSlotClaim(&pool) == NULL);
    CHECK(!TxSlotHasInFlight(&pool));

    TxSlotPoolInit(&pool, NULL, slots, 2, &s_bufs[0][0], SLOT_SIZE);
    CHECK(TxSlotClaim(&pool) == NULL);

    TxSlotPoolInit(&pool, &transport, slots, 2, NULL, SLOT_SIZE);
    CHECK(TxSlotClaim(&pool) == NULL);
}

int main(void)
{
    TestInitAndClaim();
    TestDirectSendAndCompletion();
    TestQueueAndDrainInOrder();
    TestSubmitBusyMakesOrphanThenReclaim();
    TestReclaimKeepsQueuedWhenInFlight();
    TestSubmitFailReturnsSlot();
    TestDequeueFailDropsSlot();
    TestTxAbortReleasesAndIsIdempotent();
    TestCompletionAfterAbortDrainsQueue();
    TestInvalidInputs();
    TestInvalidInitIsInert();

    if (g_failures == 0)
    {
        printf("PASS: %d checks\n", g_checks);
        return 0;
    }
    printf("FAIL: %d/%d checks failed\n", g_failures, g_checks);
    return 1;
}
