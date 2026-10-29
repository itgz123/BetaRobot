/**
 * @file drv_buzzer_simple.h
 * @brief 蜂鸣器驱动（序列播放 / 优先级仲裁 / 倍速）
 *
 * @note 依赖：bsp_tim（PWM 占空比/频率）、bsp_dwt（节拍计时）、lib_music_simple（脉冲数组）。
 *       drv 层不依赖 FreeRTOS，也不创建任务：BuzzerTask 由 app 的周期任务调用（建议 1ms）。
 *
 * @note 报警和音乐本质都是「脉冲序列」=> 共用一个 BuzzerPlay，用优先级区分：
 *       - BUZZER_PRIO_HIGH（报警）：短音效，不被低优先级打断；同一序列正在播时重复调用会被忽略
 *         （幂等），于是调用方每拍重复调用自然变成"循环播放"。
 *       - BUZZER_PRIO_LOW（音乐）：可被高优先级抢占。抢占时保存播放游标与本拍剩余时长，
 *         报警播完从被打断处续播；高优先级在播期间 BuzzerPlay(LOW) 直接返回 0（不排队）。
 *
 * @note 打断时序：音乐被打断时先静音，隔一个脉冲间隔（BUZZER_PULSE_GAP_US）再起报警第一拍。
 * @note 每个脉冲之间都插入一段短静音间隔，用来把相邻的同音调分开（否则听起来是一个长音）。
 * @note 倍速：BuzzerPlay 的 speed 同时缩放脉冲时长与脉冲间隔；<= 0 按 1.0 处理。
 *
 * @note 实例只保存 2 个序列：正在播的 + 被打断的音乐（含续播游标）。
 *       高优先级报警短、不会被打断，因此不需要游标快照。
 *
 * @note 音量 0~1 线性映射到占空比 0~0.5：无源蜂鸣器/压电片在 50% 方波时 RMS 最大。
 *
 * @note 线程约束：本模块不加锁，所有 API 必须在同一个任务上下文调用
 *       （本工程 = app 的 daemon 任务，1ms）。跨任务调用由调用方自行加临界区。
 *
 * @note 门控：需在 app_cfg.h 定义 DRV_BUZZER_USED（同时需要 BSP_TIM_USED、LIB_MUSIC_USED）。
 */

#ifndef __DRV_BUZZER_SIMPLE_H
#define __DRV_BUZZER_SIMPLE_H

#include "bsp_tim.h"
#include "lib_music_simple.h"

/* 音源优先级：数值大者优先 */
typedef enum : uint8_t
{
    BUZZER_PRIO_LOW = 0,  // 音乐：可被打断，需要续播游标
    BUZZER_PRIO_HIGH = 1, // 报警：短音效，不被低优先级打断
} BuzzerPriority_e;

/*============================================
 *              配置结构体
 *============================================*/
typedef struct
{
    BoardTIM_e tim_e; // 蜂鸣器 PWM 的板载定时器枚举
    float volume;     // 默认音量（0~1），内部换算成占空比 volume*0.5
} Buzzer_Config_s;

/*============================================
 *              实例结构体
 *============================================*/
typedef struct BuzzerInstance
{
    PWMInstance pwm; // 底层 PWM（占空比 = 音量、频率 = 音调）
    float volume;    // 默认音量（0~1）

    /* —— 正在播放的序列（任一时刻最多 1 个）—— */
    const MusicPulse_t *seq; // 当前序列
    uint16_t seq_len;        // 序列长度
    uint16_t seq_idx;        // 正在发声 / 刚播完等待间隔的这一拍
    uint16_t pending_idx;    // 间隔结束后要起播的下标
    float speed;             // 当前序列倍速（已归一，恒 > 0）
    BuzzerPriority_e prio;   // 当前序列优先级
    uint8_t playing;         // 1 = 有序列在播；0 = 空闲（seq 同时为 NULL）
    uint8_t waiting_gap;     // 1 = 处于脉冲间静音间隔
    uint64_t next_us;        // waiting_gap ? 间隔结束时刻 : 本拍结束时刻（DWT 绝对时刻）

    /* —— 低优先级被打断时的续播快照（高优先级不存）—— */
    const MusicPulse_t *music_seq;
    uint16_t music_len;
    uint16_t music_idx;       // 被打断处；在间隔中被打断时记 pending_idx
    uint32_t music_remain_us; // 被打断那一拍的剩余时长；0 = 从整拍重放
    float music_speed;
    uint8_t music_suspended; // 1 = 有被挂起的音乐
} BuzzerInstance;

/* 实例定义宏（文件作用域，静态零初始化即可） */
#define BUZZER_INSTANCE_DEF(name) BuzzerInstance name = {0}

/*============================================
 *              生命周期
 *============================================*/

/**
 * @brief 注册蜂鸣器实例（仅调用一次）
 * @return 0 成功，-1 失败
 */
int8_t BuzzerRegister(BuzzerInstance *instance);

/**
 * @brief 配置蜂鸣器（可重复调用）
 * @param config 含 tim_e 与默认音量
 * @return 0 成功，-1 失败
 * @note 要求先 BuzzerRegister；内部完成 PWM 的 Config（启动输出、初始静音）。
 */
int8_t BuzzerConfig(BuzzerInstance *instance, const Buzzer_Config_s *config);

/*============================================
 *              音量 / 停止
 *============================================*/

/**
 * @brief 设置默认音量（0~1，超出钳位）
 * @note 立即作用于当前输出：序列中只有 volume_pct==0 的拍受影响；静音间隔中不写硬件。
 */
void BuzzerSetVolume(BuzzerInstance *instance, float volume);

/**
 * @brief 立即静音，并丢弃当前序列 / 被挂起的音乐
 */
void BuzzerStop(BuzzerInstance *instance);

/*============================================
 *              序列播放
 *============================================*/

/**
 * @brief 播放一个脉冲序列（非阻塞）
 * @param seq      脉冲数组
 * @param num      脉冲个数（用 MUSIC_PULSE_COUNT 取）
 * @param priority BUZZER_PRIO_LOW 音乐 / BUZZER_PRIO_HIGH 报警
 * @param speed    倍速，<=0 按 1.0；脉冲时长与脉冲间隔都按此缩放
 * @return 1 已开始播放（或同一高优先级序列正在播）；0 参数非法 / 音乐被报警挡住
 *
 * @note HIGH：打断低优先级音乐时先保存游标、静音，隔一个脉冲间隔后起报警第一拍。
 * @note HIGH 同序列幂等：同一报警正在播时直接返回 1，适合 daemon 每拍重复调用。
 * @note LOW：高优先级在播时返回 0（不排队）；否则替换当前音乐（被挂起的音乐一并丢弃）。
 */
uint8_t BuzzerPlay(BuzzerInstance *instance, const MusicPulse_t *seq, uint16_t num, BuzzerPriority_e priority, float speed);

/**
 * @brief 节拍推进（非阻塞）
 * @note 必须周期调用（建议 1ms，与 DaemonTask 同一个任务）；用 DWT 绝对时刻，
 *       任务偶尔落后不会累积漂移，也不会把落后期间的所有拍一次性补出来。
 */
void BuzzerTask(BuzzerInstance *instance);

#endif /* __DRV_BUZZER_SIMPLE_H */
