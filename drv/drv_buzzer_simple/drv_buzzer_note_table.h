/**
 * @file drv_buzzer_note_table.h
 * @brief 音名 → 半音偏移 / 绝对偏移 → Hz 的编译期宏表
 *
 * @note 由 tools/gen_note_table.py 生成，勿手改。
 *       基准：C4 = 0，A4 = +9 = 440 Hz；覆盖 C0(-48) ~ B8(+59)。
 */

#ifndef __DRV_BUZZER_NOTE_TABLE_H
#define __DRV_BUZZER_NOTE_TABLE_H

#include <stdint.h>

/* 音名 token → 相对 C4 的半音偏移（升号拼写） */
#define BUZZER_PITCH_C0 (-48)
#define BUZZER_PITCH_CS0 (-47)
#define BUZZER_PITCH_D0 (-46)
#define BUZZER_PITCH_DS0 (-45)
#define BUZZER_PITCH_E0 (-44)
#define BUZZER_PITCH_F0 (-43)
#define BUZZER_PITCH_FS0 (-42)
#define BUZZER_PITCH_G0 (-41)
#define BUZZER_PITCH_GS0 (-40)
#define BUZZER_PITCH_A0 (-39)
#define BUZZER_PITCH_AS0 (-38)
#define BUZZER_PITCH_B0 (-37)
#define BUZZER_PITCH_C1 (-36)
#define BUZZER_PITCH_CS1 (-35)
#define BUZZER_PITCH_D1 (-34)
#define BUZZER_PITCH_DS1 (-33)
#define BUZZER_PITCH_E1 (-32)
#define BUZZER_PITCH_F1 (-31)
#define BUZZER_PITCH_FS1 (-30)
#define BUZZER_PITCH_G1 (-29)
#define BUZZER_PITCH_GS1 (-28)
#define BUZZER_PITCH_A1 (-27)
#define BUZZER_PITCH_AS1 (-26)
#define BUZZER_PITCH_B1 (-25)
#define BUZZER_PITCH_C2 (-24)
#define BUZZER_PITCH_CS2 (-23)
#define BUZZER_PITCH_D2 (-22)
#define BUZZER_PITCH_DS2 (-21)
#define BUZZER_PITCH_E2 (-20)
#define BUZZER_PITCH_F2 (-19)
#define BUZZER_PITCH_FS2 (-18)
#define BUZZER_PITCH_G2 (-17)
#define BUZZER_PITCH_GS2 (-16)
#define BUZZER_PITCH_A2 (-15)
#define BUZZER_PITCH_AS2 (-14)
#define BUZZER_PITCH_B2 (-13)
#define BUZZER_PITCH_C3 (-12)
#define BUZZER_PITCH_CS3 (-11)
#define BUZZER_PITCH_D3 (-10)
#define BUZZER_PITCH_DS3 (-9)
#define BUZZER_PITCH_E3 (-8)
#define BUZZER_PITCH_F3 (-7)
#define BUZZER_PITCH_FS3 (-6)
#define BUZZER_PITCH_G3 (-5)
#define BUZZER_PITCH_GS3 (-4)
#define BUZZER_PITCH_A3 (-3)
#define BUZZER_PITCH_AS3 (-2)
#define BUZZER_PITCH_B3 (-1)
#define BUZZER_PITCH_C4 (0)
#define BUZZER_PITCH_CS4 (1)
#define BUZZER_PITCH_D4 (2)
#define BUZZER_PITCH_DS4 (3)
#define BUZZER_PITCH_E4 (4)
#define BUZZER_PITCH_F4 (5)
#define BUZZER_PITCH_FS4 (6)
#define BUZZER_PITCH_G4 (7)
#define BUZZER_PITCH_GS4 (8)
#define BUZZER_PITCH_A4 (9)
#define BUZZER_PITCH_AS4 (10)
#define BUZZER_PITCH_B4 (11)
#define BUZZER_PITCH_C5 (12)
#define BUZZER_PITCH_CS5 (13)
#define BUZZER_PITCH_D5 (14)
#define BUZZER_PITCH_DS5 (15)
#define BUZZER_PITCH_E5 (16)
#define BUZZER_PITCH_F5 (17)
#define BUZZER_PITCH_FS5 (18)
#define BUZZER_PITCH_G5 (19)
#define BUZZER_PITCH_GS5 (20)
#define BUZZER_PITCH_A5 (21)
#define BUZZER_PITCH_AS5 (22)
#define BUZZER_PITCH_B5 (23)
#define BUZZER_PITCH_C6 (24)
#define BUZZER_PITCH_CS6 (25)
#define BUZZER_PITCH_D6 (26)
#define BUZZER_PITCH_DS6 (27)
#define BUZZER_PITCH_E6 (28)
#define BUZZER_PITCH_F6 (29)
#define BUZZER_PITCH_FS6 (30)
#define BUZZER_PITCH_G6 (31)
#define BUZZER_PITCH_GS6 (32)
#define BUZZER_PITCH_A6 (33)
#define BUZZER_PITCH_AS6 (34)
#define BUZZER_PITCH_B6 (35)
#define BUZZER_PITCH_C7 (36)
#define BUZZER_PITCH_CS7 (37)
#define BUZZER_PITCH_D7 (38)
#define BUZZER_PITCH_DS7 (39)
#define BUZZER_PITCH_E7 (40)
#define BUZZER_PITCH_F7 (41)
#define BUZZER_PITCH_FS7 (42)
#define BUZZER_PITCH_G7 (43)
#define BUZZER_PITCH_GS7 (44)
#define BUZZER_PITCH_A7 (45)
#define BUZZER_PITCH_AS7 (46)
#define BUZZER_PITCH_B7 (47)
#define BUZZER_PITCH_C8 (48)
#define BUZZER_PITCH_CS8 (49)
#define BUZZER_PITCH_D8 (50)
#define BUZZER_PITCH_DS8 (51)
#define BUZZER_PITCH_E8 (52)
#define BUZZER_PITCH_F8 (53)
#define BUZZER_PITCH_FS8 (54)
#define BUZZER_PITCH_G8 (55)
#define BUZZER_PITCH_GS8 (56)
#define BUZZER_PITCH_A8 (57)
#define BUZZER_PITCH_AS8 (58)
#define BUZZER_PITCH_B8 (59)

/* 降号等音别名（♭B = A♯，以此类推） */
#define BUZZER_PITCH_DB0 BUZZER_PITCH_CS0
#define BUZZER_PITCH_EB0 BUZZER_PITCH_DS0
#define BUZZER_PITCH_GB0 BUZZER_PITCH_FS0
#define BUZZER_PITCH_AB0 BUZZER_PITCH_GS0
#define BUZZER_PITCH_BB0 BUZZER_PITCH_AS0
#define BUZZER_PITCH_DB1 BUZZER_PITCH_CS1
#define BUZZER_PITCH_EB1 BUZZER_PITCH_DS1
#define BUZZER_PITCH_GB1 BUZZER_PITCH_FS1
#define BUZZER_PITCH_AB1 BUZZER_PITCH_GS1
#define BUZZER_PITCH_BB1 BUZZER_PITCH_AS1
#define BUZZER_PITCH_DB2 BUZZER_PITCH_CS2
#define BUZZER_PITCH_EB2 BUZZER_PITCH_DS2
#define BUZZER_PITCH_GB2 BUZZER_PITCH_FS2
#define BUZZER_PITCH_AB2 BUZZER_PITCH_GS2
#define BUZZER_PITCH_BB2 BUZZER_PITCH_AS2
#define BUZZER_PITCH_DB3 BUZZER_PITCH_CS3
#define BUZZER_PITCH_EB3 BUZZER_PITCH_DS3
#define BUZZER_PITCH_GB3 BUZZER_PITCH_FS3
#define BUZZER_PITCH_AB3 BUZZER_PITCH_GS3
#define BUZZER_PITCH_BB3 BUZZER_PITCH_AS3
#define BUZZER_PITCH_DB4 BUZZER_PITCH_CS4
#define BUZZER_PITCH_EB4 BUZZER_PITCH_DS4
#define BUZZER_PITCH_GB4 BUZZER_PITCH_FS4
#define BUZZER_PITCH_AB4 BUZZER_PITCH_GS4
#define BUZZER_PITCH_BB4 BUZZER_PITCH_AS4
#define BUZZER_PITCH_DB5 BUZZER_PITCH_CS5
#define BUZZER_PITCH_EB5 BUZZER_PITCH_DS5
#define BUZZER_PITCH_GB5 BUZZER_PITCH_FS5
#define BUZZER_PITCH_AB5 BUZZER_PITCH_GS5
#define BUZZER_PITCH_BB5 BUZZER_PITCH_AS5
#define BUZZER_PITCH_DB6 BUZZER_PITCH_CS6
#define BUZZER_PITCH_EB6 BUZZER_PITCH_DS6
#define BUZZER_PITCH_GB6 BUZZER_PITCH_FS6
#define BUZZER_PITCH_AB6 BUZZER_PITCH_GS6
#define BUZZER_PITCH_BB6 BUZZER_PITCH_AS6
#define BUZZER_PITCH_DB7 BUZZER_PITCH_CS7
#define BUZZER_PITCH_EB7 BUZZER_PITCH_DS7
#define BUZZER_PITCH_GB7 BUZZER_PITCH_FS7
#define BUZZER_PITCH_AB7 BUZZER_PITCH_GS7
#define BUZZER_PITCH_BB7 BUZZER_PITCH_AS7
#define BUZZER_PITCH_DB8 BUZZER_PITCH_CS8
#define BUZZER_PITCH_EB8 BUZZER_PITCH_DS8
#define BUZZER_PITCH_GB8 BUZZER_PITCH_FS8
#define BUZZER_PITCH_AB8 BUZZER_PITCH_GS8
#define BUZZER_PITCH_BB8 BUZZER_PITCH_AS8

/* 绝对半音偏移 → Hz；越界返回 0 */
#define BUZZER_HZ_AT(off) ((uint16_t)BUZZER_HZ_AT_I((int32_t)(off)))
#define BUZZER_HZ_AT_I(off)                                                                                                                                    \
    (off) == -48   ? 16u                                                                                                                                       \
    : (off) == -47 ? 17u                                                                                                                                       \
    : (off) == -46 ? 18u                                                                                                                                       \
    : (off) == -45 ? 19u                                                                                                                                       \
    : (off) == -44 ? 21u                                                                                                                                       \
    : (off) == -43 ? 22u                                                                                                                                       \
    : (off) == -42 ? 23u                                                                                                                                       \
    : (off) == -41 ? 24u                                                                                                                                       \
    : (off) == -40 ? 26u                                                                                                                                       \
    : (off) == -39 ? 28u                                                                                                                                       \
    : (off) == -38 ? 29u                                                                                                                                       \
    : (off) == -37 ? 31u                                                                                                                                       \
    : (off) == -36 ? 33u                                                                                                                                       \
    : (off) == -35 ? 35u                                                                                                                                       \
    : (off) == -34 ? 37u                                                                                                                                       \
    : (off) == -33 ? 39u                                                                                                                                       \
    : (off) == -32 ? 41u                                                                                                                                       \
    : (off) == -31 ? 44u                                                                                                                                       \
    : (off) == -30 ? 46u                                                                                                                                       \
    : (off) == -29 ? 49u                                                                                                                                       \
    : (off) == -28 ? 52u                                                                                                                                       \
    : (off) == -27 ? 55u                                                                                                                                       \
    : (off) == -26 ? 58u                                                                                                                                       \
    : (off) == -25 ? 62u                                                                                                                                       \
    : (off) == -24 ? 65u                                                                                                                                       \
    : (off) == -23 ? 69u                                                                                                                                       \
    : (off) == -22 ? 73u                                                                                                                                       \
    : (off) == -21 ? 78u                                                                                                                                       \
    : (off) == -20 ? 82u                                                                                                                                       \
    : (off) == -19 ? 87u                                                                                                                                       \
    : (off) == -18 ? 92u                                                                                                                                       \
    : (off) == -17 ? 98u                                                                                                                                       \
    : (off) == -16 ? 104u                                                                                                                                      \
    : (off) == -15 ? 110u                                                                                                                                      \
    : (off) == -14 ? 117u                                                                                                                                      \
    : (off) == -13 ? 123u                                                                                                                                      \
    : (off) == -12 ? 131u                                                                                                                                      \
    : (off) == -11 ? 139u                                                                                                                                      \
    : (off) == -10 ? 147u                                                                                                                                      \
    : (off) == -9  ? 156u                                                                                                                                      \
    : (off) == -8  ? 165u                                                                                                                                      \
    : (off) == -7  ? 175u                                                                                                                                      \
    : (off) == -6  ? 185u                                                                                                                                      \
    : (off) == -5  ? 196u                                                                                                                                      \
    : (off) == -4  ? 208u                                                                                                                                      \
    : (off) == -3  ? 220u                                                                                                                                      \
    : (off) == -2  ? 233u                                                                                                                                      \
    : (off) == -1  ? 247u                                                                                                                                      \
    : (off) == 0   ? 262u                                                                                                                                      \
    : (off) == 1   ? 277u                                                                                                                                      \
    : (off) == 2   ? 294u                                                                                                                                      \
    : (off) == 3   ? 311u                                                                                                                                      \
    : (off) == 4   ? 330u                                                                                                                                      \
    : (off) == 5   ? 349u                                                                                                                                      \
    : (off) == 6   ? 370u                                                                                                                                      \
    : (off) == 7   ? 392u                                                                                                                                      \
    : (off) == 8   ? 415u                                                                                                                                      \
    : (off) == 9   ? 440u                                                                                                                                      \
    : (off) == 10  ? 466u                                                                                                                                      \
    : (off) == 11  ? 494u                                                                                                                                      \
    : (off) == 12  ? 523u                                                                                                                                      \
    : (off) == 13  ? 554u                                                                                                                                      \
    : (off) == 14  ? 587u                                                                                                                                      \
    : (off) == 15  ? 622u                                                                                                                                      \
    : (off) == 16  ? 659u                                                                                                                                      \
    : (off) == 17  ? 698u                                                                                                                                      \
    : (off) == 18  ? 740u                                                                                                                                      \
    : (off) == 19  ? 784u                                                                                                                                      \
    : (off) == 20  ? 831u                                                                                                                                      \
    : (off) == 21  ? 880u                                                                                                                                      \
    : (off) == 22  ? 932u                                                                                                                                      \
    : (off) == 23  ? 988u                                                                                                                                      \
    : (off) == 24  ? 1047u                                                                                                                                     \
    : (off) == 25  ? 1109u                                                                                                                                     \
    : (off) == 26  ? 1175u                                                                                                                                     \
    : (off) == 27  ? 1245u                                                                                                                                     \
    : (off) == 28  ? 1319u                                                                                                                                     \
    : (off) == 29  ? 1397u                                                                                                                                     \
    : (off) == 30  ? 1480u                                                                                                                                     \
    : (off) == 31  ? 1568u                                                                                                                                     \
    : (off) == 32  ? 1661u                                                                                                                                     \
    : (off) == 33  ? 1760u                                                                                                                                     \
    : (off) == 34  ? 1865u                                                                                                                                     \
    : (off) == 35  ? 1976u                                                                                                                                     \
    : (off) == 36  ? 2093u                                                                                                                                     \
    : (off) == 37  ? 2217u                                                                                                                                     \
    : (off) == 38  ? 2349u                                                                                                                                     \
    : (off) == 39  ? 2489u                                                                                                                                     \
    : (off) == 40  ? 2637u                                                                                                                                     \
    : (off) == 41  ? 2794u                                                                                                                                     \
    : (off) == 42  ? 2960u                                                                                                                                     \
    : (off) == 43  ? 3136u                                                                                                                                     \
    : (off) == 44  ? 3322u                                                                                                                                     \
    : (off) == 45  ? 3520u                                                                                                                                     \
    : (off) == 46  ? 3729u                                                                                                                                     \
    : (off) == 47  ? 3951u                                                                                                                                     \
    : (off) == 48  ? 4186u                                                                                                                                     \
    : (off) == 49  ? 4435u                                                                                                                                     \
    : (off) == 50  ? 4699u                                                                                                                                     \
    : (off) == 51  ? 4978u                                                                                                                                     \
    : (off) == 52  ? 5274u                                                                                                                                     \
    : (off) == 53  ? 5588u                                                                                                                                     \
    : (off) == 54  ? 5920u                                                                                                                                     \
    : (off) == 55  ? 6272u                                                                                                                                     \
    : (off) == 56  ? 6645u                                                                                                                                     \
    : (off) == 57  ? 7040u                                                                                                                                     \
    : (off) == 58  ? 7459u                                                                                                                                     \
    : (off) == 59  ? 7902u                                                                                                                                     \
                   : 0u

#endif /* __DRV_BUZZER_NOTE_TABLE_H */
