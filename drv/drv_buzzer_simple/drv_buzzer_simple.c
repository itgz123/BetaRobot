/**
 * @file drv_buzzer_simple.c
 * @brief 蜂鸣器驱动实现（序列播放 / 优先级仲裁 / 倍速）
 */

#include "drv_buzzer_simple.h"

#include "app_cfg.h"

#ifdef DRV_BUZZER_USED

#include "bsp_dwt.h"

#include <math.h>

/* 无源蜂鸣器/压电片：音量 1.0 对应占空比 0.5（方波 RMS 最大） */
#define BUZZER_DUTY_PER_VOLUME 0.5f

#define BUZZER_MS_TO_US 1000u

/* 占空比变化小于这个值就不写寄存器：1ms 一拍时衰减曲线足够平，避免无谓的 CCR 写 */
#define BUZZER_DUTY_EPSILON 1.0e-6f

/* BuzzerTask 单次最多推进多少拍：正常情况下最多 1 拍，这里是防御异常时长导致的死循环 */
#define BUZZER_TASK_MAX_STEPS 8u

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
    float scaled_f;

    if (!(speed > 0.0f))
        speed = 1.0f;

    scaled_f = (float)base_us / speed + 0.5f;
    if (!(scaled_f < 4294967296.0f)) // 极小 speed 下 float->uint32 是未定义行为
        return 0xFFFFFFFFu;

    return (scaled_f < 1.0f) ? 1u : (uint32_t)scaled_f;
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

static uint8_t Buzzer_SequenceValid(const MusicSequence_t *music)
{
    return (music != NULL && music->pulses != NULL && music->count > 0u) ? 1u : 0u;
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
 * @brief 这一拍的有效音量：脉冲自带 volume_pct 优先，否则用实例默认音量
 */
static float Buzzer_PulseVolume(const BuzzerInstance *instance, const MusicPulse_t *pulse)
{
    if (pulse->volume_pct == 0u)
        return instance->volume;
    return Buzzer_ClampVolume((float)pulse->volume_pct / 100.0f);
}

/**
 * @brief 把 seq->pulses[seq_idx] 这一拍落到硬件（满音量起音），并算出它的结束时刻
 * @param duration_us 非 0 时覆盖这一拍的时长（续播被报警打断的拍时用剩余时长，已是 us、不再缩放）
 *
 * @note 衰减时长 = min(decay_ms, 本拍时长)：音短就在音尾截断衰减直接换下一拍，
 *       音长则衰减到 0 后余下时间静音。换频/静音后一律写满音量，保证包络每次都从音头开始。
 */
static void Buzzer_ApplyPulse(BuzzerInstance *instance, uint64_t now, uint32_t duration_us)
{
    const MusicPulse_t *pulse = &instance->seq->pulses[instance->seq_idx];
    uint32_t note_us;
    uint32_t decay_us;

    note_us = (duration_us != 0u) ? duration_us : Buzzer_ScaledUs(Buzzer_PulseDurationUs(pulse), instance->speed);
    if (note_us == 0u)
        note_us = 1u; // 节拍推进靠"到点了吗"判断，0 时长会让 next_us 停在原地

    if (instance->decay_ms == 0u)
    {
        instance->env_decay_us = 0u; // 不衰减：整拍恒定音量
    }
    else
    {
        /* 与缩放后的音长取 min，否则倍速下衰减可能比音还长 */
        decay_us = Buzzer_ScaledUs((uint32_t)instance->decay_ms * BUZZER_MS_TO_US, instance->speed);
        instance->env_decay_us = (decay_us < note_us) ? decay_us : note_us;
    }
    instance->env_start_us = now;

    if (pulse->freq_hz == 0u)
    {
        Buzzer_Silence(instance); // 休止符只占节拍，不出声
    }
    else
    {
        /* 频率带预装载：改 PSC/ARR 会同时影响该定时器的其它通道（本板只有蜂鸣器一路） */
        (void)PWMSetFrequency(&instance->pwm, (float)pulse->freq_hz);
        /* 无条件写满音量：Buzzer_Silence 可能刚写过 0，按"和缓存相同就跳过"会漏掉这次起音 */
        PWMSetDutyRatio(&instance->pwm, Buzzer_PulseVolume(instance, pulse) * BUZZER_DUTY_PER_VOLUME);
    }

    instance->next_us = now + (uint64_t)note_us;
}

/**
 * @brief 按当前拍的衰减进度重写占空比（每 1ms 调一次即形成线性下坡）
 * @note env_decay_us == 0 表示"不衰减"，仍要按满音量写：
 *       decay_ms = 0 的配置下 BuzzerSetVolume 靠这条路径立即生效。
 */
static void Buzzer_UpdateEnvelope(BuzzerInstance *instance, uint64_t now)
{
    const MusicPulse_t *pulse;
    uint64_t elapsed;
    float ratio;
    float duty;

    if (instance->pwm.map.htim == NULL || !instance->playing || instance->seq == NULL || instance->seq_idx >= instance->seq->count)
        return;

    pulse = &instance->seq->pulses[instance->seq_idx];
    if (pulse->freq_hz == 0u)
        return;

    if (instance->env_decay_us == 0u)
    {
        ratio = 0.0f;
    }
    else
    {
        elapsed = (now > instance->env_start_us) ? (now - instance->env_start_us) : 0u;
        ratio = (elapsed >= (uint64_t)instance->env_decay_us) ? 1.0f : (float)elapsed / (float)instance->env_decay_us;
    }

    duty = Buzzer_PulseVolume(instance, pulse) * BUZZER_DUTY_PER_VOLUME * (1.0f - ratio);

    if (fabsf(duty - instance->pwm.dutyratio) > BUZZER_DUTY_EPSILON)
        PWMSetDutyRatio(&instance->pwm, duty);
}

/**
 * @brief 从头开始播一个序列（立即起第一拍）
 */
static void Buzzer_StartSequence(BuzzerInstance *instance, const MusicSequence_t *music, BuzzerPriority_e prio, float speed, uint64_t now)
{
    instance->seq = music;
    instance->seq_idx = 0u;
    instance->speed = (speed > 0.0f) ? speed : 1.0f;
    instance->prio = prio;
    instance->playing = 1u;
    Buzzer_ApplyPulse(instance, now, 0u);
}

/**
 * @brief 序列播完：有被挂起的音乐就续播，否则回空闲并静音
 */
static void Buzzer_EndSequence(BuzzerInstance *instance, uint64_t now)
{
    uint32_t remain;

    if (instance->prio == BUZZER_PRIO_HIGH && instance->music_suspended && instance->music_seq != NULL && instance->music_idx < instance->music_seq->count)
    {
        /* 音乐续播：从被打断的位置重新起音（剩余时长 0 则整拍重放） */
        remain = instance->music_remain_us;
        instance->music_remain_us = 0u;
        instance->seq = instance->music_seq;
        instance->seq_idx = instance->music_idx;
        instance->speed = instance->music_speed;
        instance->prio = BUZZER_PRIO_LOW;
        instance->music_suspended = 0u;
        Buzzer_ApplyPulse(instance, now, remain);
        return;
    }

    instance->playing = 0u;
    instance->seq = NULL;
    instance->seq_idx = 0u;
    instance->music_suspended = 0u;
    instance->env_start_us = 0u;
    instance->env_decay_us = 0u;
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
    instance->decay_ms = config->decay_ms;
    instance->seq = NULL;
    instance->seq_idx = 0u;
    instance->speed = 1.0f;
    instance->prio = BUZZER_PRIO_LOW;
    instance->playing = 0u;
    instance->next_us = 0u;
    instance->env_start_us = 0u;
    instance->env_decay_us = 0u;
    instance->music_seq = NULL;
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
    if (instance == NULL)
        return;

    instance->volume = Buzzer_ClampVolume(volume);

    /* 只有"正在发声且用实例默认音量"的拍需要立即重写占空比；
     * 休止符由 UpdateEnvelope 自己跳过。已衰减到 0 的静音尾部不会因调大音量重新起音。 */
    if (!instance->playing || instance->seq == NULL || instance->seq_idx >= instance->seq->count)
        return;

    if (instance->seq->pulses[instance->seq_idx].volume_pct == 0u)
        Buzzer_UpdateEnvelope(instance, DWT_GetTimeUs());
}

void BuzzerStop(BuzzerInstance *instance)
{
    if (instance == NULL)
        return;

    instance->playing = 0u;
    instance->seq = NULL;
    instance->seq_idx = 0u;
    instance->music_suspended = 0u;
    instance->env_start_us = 0u;
    instance->env_decay_us = 0u;
    Buzzer_Silence(instance);
}

void BuzzerStopMusic(BuzzerInstance *instance)
{
    if (instance == NULL)
        return;

    /* 被打断的音乐一律作废：即便此刻报警在播，也不能在报警结束后"接着放" */
    instance->music_seq = NULL;
    instance->music_idx = 0u;
    instance->music_remain_us = 0u;
    instance->music_speed = 1.0f;
    instance->music_suspended = 0u;

    /* 当前播的是音乐才停；报警（HIGH）继续按自己的节拍走 */
    if (instance->playing && instance->prio == BUZZER_PRIO_LOW)
    {
        instance->playing = 0u;
        instance->seq = NULL;
        instance->seq_idx = 0u;
        instance->env_start_us = 0u;
        instance->env_decay_us = 0u;
        Buzzer_Silence(instance);
    }
}

/*------------- 序列播放 --------------*/

uint8_t BuzzerPlay(BuzzerInstance *instance, const MusicSequence_t *music, BuzzerPriority_e priority, float speed)
{
    uint64_t now;

    if (instance == NULL || instance->pwm.map.htim == NULL || !Buzzer_SequenceValid(music))
        return 0;

    if (!(speed > 0.0f))
        speed = 1.0f;

    if (priority == BUZZER_PRIO_HIGH)
    {
        /* 同一报警正在播：幂等返回。调用方每拍重复调用即变成"循环报警" */
        if (instance->playing && instance->prio == BUZZER_PRIO_HIGH && instance->seq == music)
            return 1;

        now = DWT_GetTimeUs();

        if (instance->playing && instance->prio == BUZZER_PRIO_LOW && instance->seq != NULL)
        {
            /* 抢占音乐：记下续播游标。当前拍还在响 → 记本拍 + 剩余时长；
             * 已经到点但 BuzzerTask 还没推进 → 记下一拍（已响完的拍不能重放）。
             * 下一拍越界说明音乐其实已播完，不再续播（否则会对 pulses[count] 越界读）。 */
            uint16_t idx;
            uint32_t remain = 0u;

            if (instance->next_us > now)
            {
                idx = instance->seq_idx;
                remain = (uint32_t)(instance->next_us - now);
            }
            else
            {
                idx = (uint16_t)((uint32_t)instance->seq_idx + 1u);
            }

            if (idx < instance->seq->count)
            {
                instance->music_seq = instance->seq;
                instance->music_idx = idx;
                instance->music_remain_us = remain;
                instance->music_speed = instance->speed;
                instance->music_suspended = 1u;
            }
            else
            {
                instance->music_suspended = 0u;
            }
        }

        Buzzer_StartSequence(instance, music, priority, speed, now);
        return 1;
    }

    /* 低优先级：报警在播时不排队，直接拒绝 */
    if (instance->playing && instance->prio == BUZZER_PRIO_HIGH)
        return 0;

    instance->music_suspended = 0u;
    Buzzer_StartSequence(instance, music, BUZZER_PRIO_LOW, speed, DWT_GetTimeUs());
    return 1;
}

void BuzzerTask(BuzzerInstance *instance)
{
    uint8_t steps;

    if (instance == NULL || !instance->playing)
        return;

    for (steps = 0u; steps < BUZZER_TASK_MAX_STEPS; steps++)
    {
        uint64_t now = DWT_GetTimeUs();

        if (now < instance->next_us)
            break;

        /* 当前脉冲结束：还有下一拍就换拍，否则整段结束 */
        if ((uint32_t)instance->seq_idx + 1u >= (uint32_t)instance->seq->count)
        {
            Buzzer_EndSequence(instance, now);
            break;
        }

        instance->seq_idx = (uint16_t)((uint32_t)instance->seq_idx + 1u);
        Buzzer_ApplyPulse(instance, now, 0u);
    }

    /* 每 1ms 重写一次占空比，形成线性衰减；EndSequence 可能已经把 playing 清掉 */
    if (instance->playing)
        Buzzer_UpdateEnvelope(instance, DWT_GetTimeUs());
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

void BuzzerStopMusic(BuzzerInstance *instance)
{
    (void)instance;
}

uint8_t BuzzerPlay(BuzzerInstance *instance, const MusicSequence_t *music, BuzzerPriority_e priority, float speed)
{
    (void)instance;
    (void)music;
    (void)priority;
    (void)speed;
    return 0;
}

void BuzzerTask(BuzzerInstance *instance)
{
    (void)instance;
}

#endif /* DRV_BUZZER_USED */
