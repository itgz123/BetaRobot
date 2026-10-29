/**
 * @file drv_buzzer_simple.c
 * @brief 蜂鸣器驱动实现（序列播放 / 优先级仲裁 / 倍速）
 */

#include "drv_buzzer_simple.h"

#include "app_cfg.h"

#ifdef DRV_BUZZER_USED

#include "bsp_dwt.h"

/* 无源蜂鸣器/压电片：音量 1.0 对应占空比 0.5（方波 RMS 最大） */
#define BUZZER_DUTY_PER_VOLUME 0.5f

/* 脉冲之间的静音间隔：隔开相邻同音调，否则听不出是两拍。
 * 用编译选项（-D）可覆盖；倍速播放时随 speed 一起缩放。 */
#ifndef BUZZER_PULSE_GAP_US
#define BUZZER_PULSE_GAP_US 5000u
#endif

#define BUZZER_MS_TO_US 1000u

/*------------- 私有辅助 --------------*/

static float Buzzer_ClampVolume(float volume)
{
    if (!(volume > 0.0f)) // 同时挡掉 NaN
        return 0.0f;
    if (volume > 1.0f)
        return 1.0f;
    return volume;
}

/**
 * @brief 按倍速缩放一段时长（us）
 * @note speed <= 0 按 1.0；结果至少 1us —— 节拍推进靠"到点了吗"判断，
 *       0 时长会让 next_us 停在原地，任务里会退化成死循环。
 */
static uint32_t Buzzer_ScaledUs(uint32_t base_us, float speed)
{
    uint32_t scaled;

    if (!(speed > 0.0f))
        speed = 1.0f;

    scaled = (uint32_t)((float)base_us / speed + 0.5f);
    return (scaled == 0u) ? 1u : scaled;
}

/**
 * @brief 这一拍的基准时长（us，未按倍速缩放）
 * @note duration_ms == 0 时按 1ms 处理（理由同 Buzzer_ScaledUs）
 */
static uint32_t Buzzer_PulseDurationUs(const MusicPulse_t *pulse)
{
    uint32_t duration_us = (uint32_t)pulse->duration_ms * BUZZER_MS_TO_US;
    return (duration_us == 0u) ? BUZZER_MS_TO_US : duration_us;
}

static uint8_t Buzzer_SequenceValid(const MusicPulse_t *seq, uint16_t num)
{
    return (seq != NULL && num > 0u) ? 1u : 0u;
}

/**
 * @brief 静音（未 Config 时不调用 PWM，避免每拍刷 bsp_tim 的告警日志）
 */
static void Buzzer_Silence(BuzzerInstance *instance)
{
    if (instance->pwm.map.htim != NULL)
        PWMSetDutyRatio(&instance->pwm, 0.0f);
}

/**
 * @brief 把 seq[seq_idx] 这一拍落到硬件，并算出它的结束时刻
 * @param duration_us 非 0 时覆盖这一拍的时长（续播被报警打断的拍时用剩余时长）
 */
static void Buzzer_ApplyPulse(BuzzerInstance *instance, uint64_t now, uint32_t duration_us)
{
    const MusicPulse_t *pulse = &instance->seq[instance->seq_idx];
    float volume = (pulse->volume_pct == 0u) ? instance->volume : ((float)pulse->volume_pct / 100.0f);

    if (pulse->freq_hz == 0u)
    {
        Buzzer_Silence(instance); // 休止符只占节拍，不出声
    }
    else
    {
        /* 频率带预装载：改 PSC/ARR 会同时影响该定时器的其它通道（本板只有蜂鸣器一路） */
        (void)PWMSetFrequency(&instance->pwm, (float)pulse->freq_hz);
        PWMSetDutyRatio(&instance->pwm, volume * BUZZER_DUTY_PER_VOLUME);
    }

    if (duration_us == 0u)
        duration_us = Buzzer_ScaledUs(Buzzer_PulseDurationUs(pulse), instance->speed);

    instance->next_us = now + (uint64_t)duration_us;
    instance->waiting_gap = 0u;
}

/**
 * @brief 进入脉冲间静音间隔；间隔结束后从 next_idx 起播
 */
static void Buzzer_BeginGap(BuzzerInstance *instance, uint64_t now, uint16_t next_idx)
{
    Buzzer_Silence(instance);
    instance->pending_idx = next_idx;
    instance->waiting_gap = 1u;
    instance->next_us = now + (uint64_t)Buzzer_ScaledUs(BUZZER_PULSE_GAP_US, instance->speed);
}

/**
 * @brief 从头开始播一个序列（先走一个脉冲间隔，保证切换时干净断开）
 */
static void Buzzer_StartSequence(BuzzerInstance *instance, const MusicPulse_t *seq, uint16_t num, BuzzerPriority_e prio, float speed, uint64_t now)
{
    instance->seq = seq;
    instance->seq_len = num;
    instance->seq_idx = 0u;
    instance->pending_idx = 0u;
    instance->speed = (speed > 0.0f) ? speed : 1.0f;
    instance->prio = prio;
    instance->playing = 1u;
    Buzzer_BeginGap(instance, now, 0u);
}

/**
 * @brief 序列播完：有被挂起的音乐就续播，否则回空闲并静音
 */
static void Buzzer_EndSequence(BuzzerInstance *instance, uint64_t now)
{
    if (instance->prio == BUZZER_PRIO_HIGH && instance->music_suspended && instance->music_seq != NULL && instance->music_len > 0u)
    {
        /* 音乐续播：直接接上被打断的那一拍（剩余时长 0 则整拍重放） */
        instance->seq = instance->music_seq;
        instance->seq_len = instance->music_len;
        instance->seq_idx = instance->music_idx;
        instance->speed = instance->music_speed;
        instance->prio = BUZZER_PRIO_LOW;
        instance->music_suspended = 0u;
        Buzzer_ApplyPulse(instance, now, instance->music_remain_us);
        return;
    }

    instance->playing = 0u;
    instance->waiting_gap = 0u;
    instance->seq = NULL;
    instance->seq_len = 0u;
    instance->seq_idx = 0u;
    instance->pending_idx = 0u;
    instance->music_suspended = 0u;
    Buzzer_Silence(instance);
}

/*------------- 生命周期 --------------*/

int8_t BuzzerRegister(BuzzerInstance *instance)
{
    if (instance == NULL)
        return -1;
    return PWMRegister(&instance->pwm);
}

int8_t BuzzerConfig(BuzzerInstance *instance, const Buzzer_Config_s *config)
{
    PWM_Config_s pwm_config;

    if (instance == NULL || config == NULL)
        return -1;

    /* 频率沿用 CubeMX 给蜂鸣器定时器配的谐振点，音调由每一拍现改 */
    pwm_config.tim_e = config->tim_e;
    pwm_config.frequency_hz = 0.0f;
    if (PWMConfig(&instance->pwm, &pwm_config) != 0)
        return -1;

    instance->volume = Buzzer_ClampVolume(config->volume);
    instance->seq = NULL;
    instance->seq_len = 0u;
    instance->seq_idx = 0u;
    instance->pending_idx = 0u;
    instance->speed = 1.0f;
    instance->prio = BUZZER_PRIO_LOW;
    instance->playing = 0u;
    instance->waiting_gap = 0u;
    instance->next_us = 0u;
    instance->music_seq = NULL;
    instance->music_len = 0u;
    instance->music_idx = 0u;
    instance->music_remain_us = 0u;
    instance->music_speed = 1.0f;
    instance->music_suspended = 0u;
    Buzzer_Silence(instance);

    return 0;
}

/*------------- 音量 / 停止 --------------*/

void BuzzerSetVolume(BuzzerInstance *instance, float volume)
{
    const MusicPulse_t *pulse;

    if (instance == NULL)
        return;

    instance->volume = Buzzer_ClampVolume(volume);

    /* 只有"正在发声且用实例默认音量"的拍需要立即重写占空比；
     * 间隔中已经静音，休止符也不发声 */
    if (!instance->playing || instance->waiting_gap || instance->seq == NULL || instance->seq_idx >= instance->seq_len)
        return;

    pulse = &instance->seq[instance->seq_idx];
    if (pulse->volume_pct == 0u && pulse->freq_hz != 0u)
        PWMSetDutyRatio(&instance->pwm, instance->volume * BUZZER_DUTY_PER_VOLUME);
}

void BuzzerStop(BuzzerInstance *instance)
{
    if (instance == NULL)
        return;

    instance->playing = 0u;
    instance->waiting_gap = 0u;
    instance->seq = NULL;
    instance->seq_len = 0u;
    instance->seq_idx = 0u;
    instance->pending_idx = 0u;
    instance->music_suspended = 0u;
    Buzzer_Silence(instance);
}

/*------------- 序列播放 --------------*/

uint8_t BuzzerPlay(BuzzerInstance *instance, const MusicPulse_t *seq, uint16_t num, BuzzerPriority_e priority, float speed)
{
    uint64_t now;

    if (instance == NULL || instance->pwm.map.htim == NULL || !Buzzer_SequenceValid(seq, num))
        return 0;

    if (!(speed > 0.0f))
        speed = 1.0f;

    if (priority == BUZZER_PRIO_HIGH)
    {
        /* 同一报警正在播：幂等返回。调用方每拍重复调用即变成"循环报警" */
        if (instance->playing && instance->prio == BUZZER_PRIO_HIGH && instance->seq == seq && instance->seq_len == num)
            return 1;

        now = DWT_GetTimeUs();

        if (instance->playing && instance->prio == BUZZER_PRIO_LOW && instance->seq != NULL)
        {
            /* 抢占音乐：先记游标（间隔中被打断就记将要起播的 pending_idx），
             * 再静音；Buzzer_StartSequence 自带的间隔满足"暂停音、隔一拍再报警" */
            instance->music_seq = instance->seq;
            instance->music_len = instance->seq_len;
            instance->music_idx = instance->waiting_gap ? instance->pending_idx : instance->seq_idx;
            instance->music_remain_us = (!instance->waiting_gap && instance->next_us > now) ? (uint32_t)(instance->next_us - now) : 0u;
            instance->music_speed = instance->speed;
            instance->music_suspended = 1u;
        }

        Buzzer_StartSequence(instance, seq, num, priority, speed, now);
        return 1;
    }

    /* 低优先级：报警在播时不排队，直接拒绝 */
    if (instance->playing && instance->prio == BUZZER_PRIO_HIGH)
        return 0;

    instance->music_suspended = 0u;
    Buzzer_StartSequence(instance, seq, num, BUZZER_PRIO_LOW, speed, DWT_GetTimeUs());
    return 1;
}

void BuzzerTask(BuzzerInstance *instance)
{
    if (instance == NULL || !instance->playing)
        return;

    for (;;)
    {
        uint64_t now = DWT_GetTimeUs();

        if (now < instance->next_us)
            break;

        if (instance->waiting_gap)
        {
            instance->seq_idx = instance->pending_idx;
            instance->waiting_gap = 0u;
            Buzzer_ApplyPulse(instance, now, 0u);
            continue;
        }

        /* 当前脉冲结束：还有下一拍就先进间隔，否则整段结束 */
        if ((uint32_t)instance->seq_idx + 1u >= (uint32_t)instance->seq_len)
        {
            Buzzer_EndSequence(instance, now);
            break;
        }

        Buzzer_BeginGap(instance, now, (uint16_t)((uint32_t)instance->seq_idx + 1u));
    }
}

#else /* !DRV_BUZZER_USED */

int8_t BuzzerRegister(BuzzerInstance *instance)
{
    (void)instance;
    return -1;
}

int8_t BuzzerConfig(BuzzerInstance *instance, const Buzzer_Config_s *config)
{
    (void)instance;
    (void)config;
    return -1;
}

void BuzzerSetVolume(BuzzerInstance *instance, float volume)
{
    (void)instance;
    (void)volume;
}

void BuzzerStop(BuzzerInstance *instance)
{
    (void)instance;
}

uint8_t BuzzerPlay(BuzzerInstance *instance, const MusicPulse_t *seq, uint16_t num, BuzzerPriority_e priority, float speed)
{
    (void)instance;
    (void)seq;
    (void)num;
    (void)priority;
    (void)speed;
    return 0;
}

void BuzzerTask(BuzzerInstance *instance)
{
    (void)instance;
}

#endif /* DRV_BUZZER_USED */
