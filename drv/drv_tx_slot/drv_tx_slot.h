/**
 * @file drv_tx_slot.h
 * @brief 串口 DMA 发送槽池（drv_vofa / drv_terminal_lite 共用的发送缓冲状态机）
 *
 * @note 解决的问题（两个模块原本各抄了一份，逐字同构）：
 *       串口发送是异步的（DMA + 完成回调），上层却每周期都想发一帧。需要一个池子：
 *       借一块缓冲写入 → 提交 → 传输层忙就排队 → 完成回调归还并补发下一块。
 *       本模块只做这件事，协议（JustFloat 帧 / 文本行）和缓冲大小仍归调用方。
 *
 * @note 每槽状态机（UML 顺序：退出旧状态 → 转移动作 → 切换状态 → 进入新状态）：
 *
 *           ┌────────────── 借出(CLAIM) ──────────────┐
 *           │                                         v
 *        FREE <── 归还(RELEASE/失败) ── WRITE ── 提交成功 ──> SEND ── 完成/中止 ──> FREE
 *           ^                              │                  |
 *           │                              └─ 传输层忙 ──> WAIT_SEND
 *           └────────── 补发成功(DEQUEUE) ────────────────────┘  （WAIT_SEND → SEND）
 *           └── 孤儿回收(RECLAIM，仅当池中无在途) / 补发启动失败 ── FREE
 *
 *       - **在途槽唯一**：同一时刻至多一个槽处于 SEND，池用 in_flight 记住它。
 *         进入 SEND 认领、退出 SEND 归还，这两处各写一次，别处不碰 in_flight。
 *       - **先启动、成功才认领**：提交/补发时先调 transport.start()，只有返回 OK 才
 *         把槽切到 SEND。反过来（先认领）会给"早先挂起、此刻才被响应的错误中断"
 *         一个窗口：它看到本槽在途，会把尚未交给 DMA 的它当成被中止的那次释放掉，
 *         而外层照旧启动了 DMA —— 槽随即可能被别的发送借走、边发边被改写。
 *       - 错误回调**不补发** WAIT_SEND：它可能在 transport.start() 调用栈内被触发
 *         （bsp 的发送卡死自恢复），补发等于嵌套调用 start，外层回来还会覆盖槽状态。
 *         积压的排队帧由下一次完成回调带出。
 *
 * @note 上下文约束（重要）：
 *       - TxSlotClaim / TxSlotSubmit / TxSlotRelease / TxSlotReclaimOrphans 由**同一个
 *         任务**调用（单写者）；本模块不建任务、不加锁队列。
 *       - TxSlotOnTxCplt / TxSlotOnError 由串口回调（ISR 或任务上下文）调用；
 *         与单写者的并发由模块内部的临界区（PRIMASK）挡住，调用方无需再包临界区。
 *       - WRITE 槽不会被 ISR 看到，故"借出 → 写入 → 提交"期间的格式化可在临界区外做。
 *
 * @note 依赖与开关：本模块只依赖 <stdint.h>/<stdbool.h>，不依赖 bsp/lib，**不设 USE 开关**
 *       （与 drv_planner 一致，始终参与编译；没人引用时由 --gc-sections 丢弃）。
 *       写固件时从 main.h 取 PRIMASK 原语；PC 单测定义 DRV_TX_SLOT_STANDALONE 改为空实现。
 *
 * @note 为什么 bsp_log 不用这里：bsp 是 drv 的下层，让 bsp 反向包含 drv 会把依赖方向翻过来；
 *       日志链路又对递归极度敏感，故 bsp_log 保留自己的那份槽状态机，两者各自独立。
 */

#ifndef __DRV_TX_SLOT_H
#define __DRV_TX_SLOT_H

#include <stdbool.h>
#include <stdint.h>

/*============================================
 *              类型定义
 *============================================*/

/** @brief 槽状态（取值连续，可从调试器直接读） */
typedef enum : uint8_t
{
    TX_SLOT_FREE = 0,  // 空闲：可借出，也是"没有在途"的那一类槽
    TX_SLOT_WRITE,     // 已借出：调用方正往里写，尚未交给传输层（ISR 看不到它）
    TX_SLOT_SEND,      // 在途：已交给传输层，等待完成/中止回调（池中至多一个）
    TX_SLOT_WAIT_SEND, // 排队：提交时传输层忙，等完成回调补发
} TxSlotState_e;

/** @brief 传输层启动一次发送的结果（由调用方的 transport.start 实现给出） */
typedef enum : uint8_t
{
    TX_SLOT_START_OK = 0, // 已启动：会有一个完成回调
    TX_SLOT_START_BUSY,   // 传输层忙：上层排队重试
    TX_SLOT_START_FAIL,   // 启动失败：本次丢弃
} TxSlotStartResult_e;

/** @brief 提交结果（调用方据此打日志/计数） */
typedef enum : uint8_t
{
    TX_SLOT_SUBMIT_SENT = 0, // 已直接启动，等待完成回调
    TX_SLOT_SUBMIT_QUEUED,   // 传输层忙，已排进 WAIT_SEND
    TX_SLOT_SUBMIT_FAILED,   // 启动失败，槽已归还 FREE，本帧丢弃
} TxSlotSubmitResult_e;

/**
 * @brief 传输层适配（把"启动 DMA"抽出来，模块因此不依赖 bsp_usart）
 * @note start 必须是非阻塞的：启动 DMA 后立刻返回，完成由调用方转给
 *       TxSlotOnTxCplt/TxSlotOnError。它在模块的临界区内被调用，实现里不要长耗时。
 * @note start 可能在 ISR 上下文（补发）或任务上下文（提交）被调用。
 */
typedef struct
{
    TxSlotStartResult_e (*start)(void *transport, const uint8_t *buf, uint16_t len);
    void *transport; // 原样回传给 start（通常是 USARTInstance*）
} TxSlotTransport_s;

typedef struct TxSlotPool_s TxSlotPool_s;

/**
 * @brief 一个发送槽
 * @note buf/cap/len 是公开的：调用方直接往 buf 里格式化，然后写 len 再提交。
 *       buf 必须是传输层可直接读的缓冲（DMA 时需在 DMA_RAM）。
 */
typedef struct
{
    uint8_t *buf;                 // 发送缓冲（DMA 直接读取，生命周期与池同长）
    uint16_t cap;                 // 缓冲容量
    uint16_t len;                 // 本次有效长度，提交时由调用方填好
    TxSlotPool_s *pool;           // 所属池
    volatile TxSlotState_e state; // 槽状态
} TxSlot_s;

/** @brief 槽池：槽数组与传输层适配都由调用方提供 */
struct TxSlotPool_s
{
    TxSlotTransport_s transport;        // 传输层适配
    TxSlot_s *slots;                    // 槽数组（调用方持有）
    uint8_t slot_count;                 // 槽数
    volatile TxSlot_s *in_flight;       // 当前在途槽，NULL=无；由进入/退出 SEND 维护
    volatile uint32_t reclaim_cnt;      // 孤儿回收计数（自愈凭据：恒 0 说明从没出现过）
    volatile uint32_t start_fail_cnt;   // 启动失败次数（提交+补发）
    volatile uint32_t dequeue_fail_cnt; // 补发启动失败次数（单独的计数器）
};

/*============================================
 *              外部接口
 *============================================*/

/**
 * @brief 初始化槽池并把每个槽切成 FREE
 * @param pool        池对象（调用方静态分配）
 * @param transport   传输层适配（start 不可为 NULL）
 * @param slots       槽数组（调用方静态分配，slot_count 个）
 * @param slot_count  槽数（>=1）
 * @param buf_base    连续缓冲区首地址；第 i 个槽用 buf_base + i*slot_buf_size
 * @param slot_buf_size 每槽缓冲字节数
 * @note 必须在串口回调注册之前调用（池先就绪，回调才可能进来）。
 *       任一指针为 NULL、slot_count 为 0 或 slot_buf_size 为 0 时直接返回，池保持不可用。
 */
void TxSlotPoolInit(TxSlotPool_s *pool, const TxSlotTransport_s *transport, TxSlot_s *slots, uint8_t slot_count, uint8_t *buf_base, uint16_t slot_buf_size);

/**
 * @brief 借一个空闲槽用于写入（FREE → WRITE）
 * @return 槽指针；池已满返回 NULL
 * @note 单写者调用。借出的槽对 ISR 不可见，可以慢慢写。
 */
TxSlot_s *TxSlotClaim(TxSlotPool_s *pool);

/**
 * @brief 提交一个已写好的槽（WRITE → SEND / WAIT_SEND / FREE）
 * @param slot 由 TxSlotClaim 借出的槽（len 已填好）
 * @return SENT=已启动；QUEUED=传输层忙已排队；FAILED=启动失败，槽已归还
 * @note 临界区内判"有没有在途"并启动，与完成回调互斥；调用方不必再包临界区。
 * @note 在 ISR 里调用也安全：临界区只恢复到调用前的 PRIMASK，不会在中断里提前开中断。
 */
TxSlotSubmitResult_e TxSlotSubmit(TxSlot_s *slot);

/**
 * @brief 放弃一个已借出但未提交的槽（WRITE → FREE）
 * @note 格式化失败（无内容/超长清零）时归还；非 WRITE 状态的槽调用无效。
 */
void TxSlotRelease(TxSlot_s *slot);

/**
 * @brief DMA 发送完成回调的落点
 * @param pool 槽池
 * @note 归还 in_flight（SEND → FREE），然后按数组顺序补发第一个 WAIT_SEND。
 *       没有在途时也照样扫一遍：错误回调释放过在途槽之后，真正的完成回调
 *       仍会到达，正好用它把积压的排队帧带出去。
 * @note 补发启动失败/传输层忙的排队槽直接归还（本帧丢弃），并计入 dequeue_fail_cnt，
 *       与原两模块"补发失败即丢弃"的语义一致。
 */
void TxSlotOnTxCplt(TxSlotPool_s *pool);

/**
 * @brief 串口错误回调的落点
 * @param pool       槽池
 * @param tx_aborted true=本次发送已被中止（HAL 已把状态置回 READY），不会再有完成回调，
 *                   必须释放在途槽，否则它永久停在 SEND、反复出错会耗光槽池；
 *                   false=发送仍在途（错在接收侧），此刻不能释放 DMA 还在读的缓冲。
 * @note 幂等：重复调用无副作用（同一次错误可能既走硬件错误又走卡死恢复）。
 * @note 刻意不补发 WAIT_SEND（理由见文件头）。调用方负责把 bsp 的错误原因
 *       （USART_ERR_TX_ABORT / HAL READY）翻译成 tx_aborted，RX 类错误本模块不关心。
 */
void TxSlotOnError(TxSlotPool_s *pool, bool tx_aborted);

/**
 * @brief 回收"排队却无在途"的孤儿槽（WAIT_SEND → FREE）
 * @param pool 槽池
 * @return 本次回收的槽数
 * @note 判据是池里没有在途：没有在途就不会再有完成回调去补发那些槽，它们已经发不出去，
 *       占着槽只会让后面借不到。有在途时一个都不动（可能正排在补发的路上）。
 * @note 这是可选策略：vofa 在借不到槽时调用它；terminal_lite 不调用（它的排队帧
 *       会被下一次成功提交的完成回调带出去，早回收反而丢的是还能发的帧）。
 */
uint8_t TxSlotReclaimOrphans(TxSlotPool_s *pool);

/**
 * @brief 池中是否有在途槽
 * @note 只读，无副作用；可在任意上下文调用。
 */
bool TxSlotHasInFlight(const TxSlotPool_s *pool);

/**
 * @brief 统计处于某状态的槽数
 * @note 只读，主要用于调试/日志/PC 测试断言。
 */
uint8_t TxSlotCount(const TxSlotPool_s *pool, TxSlotState_e state);

#endif // __DRV_TX_SLOT_H
