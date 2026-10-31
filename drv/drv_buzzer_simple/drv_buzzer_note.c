/**
 * @file drv_buzzer_note.c
 * @brief 音名 / 简谱 → 频率（查表实现）
 */

#include "drv_buzzer_note.h"

#include "app_cfg.h"

#ifdef DRV_BUZZER_USED

/* 十二平均律频率表：行 = 八度 0~8，列 = 音名 C..B，单位 Hz（四舍五入）
 * f = 440 * 2^((midi - 69) / 12)，midi = (octave + 1) * 12 + note */
static const uint16_t s_note_hz[BUZZER_NOTE_OCTAVE_MAX + 1u][BUZZER_NOTE_NUM] = {
    /* C     C♯    D     D♯    E     F     F♯    G     G♯    A     A♯    B   */
    {16u, 17u, 18u, 19u, 21u, 22u, 23u, 24u, 26u, 28u, 29u, 31u},                         /* 0 */
    {33u, 35u, 37u, 39u, 41u, 44u, 46u, 49u, 52u, 55u, 58u, 62u},                         /* 1 */
    {65u, 69u, 73u, 78u, 82u, 87u, 92u, 98u, 104u, 110u, 117u, 123u},                     /* 2 */
    {131u, 139u, 147u, 156u, 165u, 175u, 185u, 196u, 208u, 220u, 233u, 247u},             /* 3 */
    {262u, 277u, 294u, 311u, 330u, 349u, 370u, 392u, 415u, 440u, 466u, 494u},             /* 4 */
    {523u, 554u, 587u, 622u, 659u, 698u, 740u, 784u, 831u, 880u, 932u, 988u},             /* 5 */
    {1047u, 1109u, 1175u, 1245u, 1319u, 1397u, 1480u, 1568u, 1661u, 1760u, 1865u, 1976u}, /* 6 */
    {2093u, 2217u, 2349u, 2489u, 2637u, 2794u, 2960u, 3136u, 3322u, 3520u, 3729u, 3951u}, /* 7 */
    {4186u, 4435u, 4699u, 4978u, 5274u, 5588u, 5920u, 6272u, 6645u, 7040u, 7459u, 7902u}, /* 8 */
};

/* 与编译期宏 BUZZER_RATIO_* 同源：半音数 → 频率比（1e-5） */
static const uint32_t s_ratio_q5[12] = {
    BUZZER_RATIO_0, BUZZER_RATIO_1, BUZZER_RATIO_2, BUZZER_RATIO_3, BUZZER_RATIO_4,  BUZZER_RATIO_5,
    BUZZER_RATIO_6, BUZZER_RATIO_7, BUZZER_RATIO_8, BUZZER_RATIO_9, BUZZER_RATIO_10, BUZZER_RATIO_11,
};

uint16_t BuzzerNoteHz(BuzzerNote_e note, uint8_t octave)
{
    if ((uint8_t)note >= (uint8_t)BUZZER_NOTE_NUM || octave > BUZZER_NOTE_OCTAVE_MAX)
        return 0u;
    return s_note_hz[octave][(uint8_t)note];
}

int8_t BuzzerDegreeSemitone(uint8_t degree)
{
    if (degree < 1u || degree > 7u)
        return -1;
    return (int8_t)BUZZER_DEG_SEMITONE(degree);
}

uint16_t BuzzerDegreeHz(BuzzerNote_e tonic_note, uint8_t tonic_octave, uint8_t degree, int8_t octave_shift)
{
    int8_t semitone = BuzzerDegreeSemitone(degree);
    uint16_t tonic_hz;
    uint32_t freq;

    if (semitone < 0)
        return 0u;

    /* 翻八度最多 8 次；同时挡住 int8 取负溢出 */
    if (octave_shift > 8 || octave_shift < -8)
        return 0u;

    tonic_hz = BuzzerNoteHz(tonic_note, tonic_octave);
    if (tonic_hz == 0u)
        return 0u;

    /* 与曲谱宏 BUZZER_DEG_HZ 同一套比值和四舍五入 */
    freq = ((uint32_t)tonic_hz * s_ratio_q5[(uint8_t)semitone] + 50000u) / 100000u;

    if (octave_shift > 0)
    {
        freq <<= (uint8_t)octave_shift;
    }
    else if (octave_shift < 0)
    {
        uint32_t down = (uint32_t)(-octave_shift);
        freq = (freq + (1u << (down - 1u))) >> down;
    }

    return (freq > 0xFFFFu) ? 0u : (uint16_t)freq;
}

#endif /* DRV_BUZZER_USED */
