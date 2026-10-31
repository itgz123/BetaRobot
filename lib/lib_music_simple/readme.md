# lib_music_simple

蜂鸣器音频数据层（纯数据，不依赖任何硬件）。驱动 `drv/drv_buzzer_simple` 消费这里的数据，
换成别的发声器件时曲谱不用改。

曲目文件会 `#include "drv_buzzer_note.h"` 取简谱换算宏——那是个纯常量头文件（不含任何硬件），
编译期就把频率算好，运行时不依赖蜂鸣器驱动。

## 结构

| 路径 | 内容 |
| --- | --- |
| `lib_music_simple.h` | `MusicPulse_t`（一拍：频率/时长/音量）、`MusicSequence_t`（一首曲目：指针 + 长度） |
| `music_custom/` | 常用提示音、5 级报警音效、开机音等（`MUSIC_EFFECT_*`） |
| `twinkle_star/` | 《小星星》，验证音高/包络用 |
| `happy_birthday/` | 《祝你生日快乐》 |
| `see_you_again/see_you_again_guitar/` | 《See You Again》吉他谱版（`MUSIC_SEE_YOU_AGAIN_GUITAR`），已参与构建 |
| `host/` | PC 端验证工具：gcc 编共享库 + Python 合成 FLAC，见 `host/readme.md` |

`lib/lib_music/`（没有 simple）预留给之后基于 DAC 的方案，目前为空。

## 一首曲目 = 一个结构体

```c
/* .c：脉冲数组只在文件内可见 */
static const MusicPulse_t s_song[] = { {523u, 600u, 0u}, /* ... */ };
const MusicSequence_t MUSIC_SONG = {s_song, MUSIC_PULSE_COUNT(s_song)};

/* 播放：数组和长度绑在一起传，不会出现"数组改了长度忘了改" */
(void)BuzzerPlay(&g_buzzer, &MUSIC_SONG, BUZZER_PRIO_LOW, 1.0f);
```

元素格式：`{freq_hz, duration_ms, volume_pct}`；`freq_hz == 0` 为休止符，
`volume_pct == 0` 表示用蜂鸣器实例的默认音量。

## 音名/简谱怎么换算

频率表在 `drv/drv_buzzer_simple/drv_buzzer_note.{h,c}`，分两套入口：

| 入口 | 用途 | 位置 |
| --- | --- | --- |
| `BUZZER_DEG_HZ(base, d)` / `_HI` / `_LO` | **编译期**算频率，曲谱数组直接用 | 宏（头文件） |
| `BuzzerNoteHz` / `BuzzerDegreeHz` | **运行时**换算/移调 | 函数 + 二维表 |

曲谱数组是 `static const`，初始化必须是常量表达式，函数调用和数组下标都不行
（C 没有 constexpr），所以曲谱里用宏。每个曲目文件先定 `1` 的基准，再起两个短别名：

```c
#include "drv_buzzer_note.h"

#define JIANPU_BASE 523u                          /* 1 = C5 */
#define J(d) BUZZER_DEG_HZ(JIANPU_BASE, (d))      /* 简谱 1~7 */
#define JH(d) BUZZER_DEG_HZ_HI(JIANPU_BASE, (d))  /* 上加点的高音 1'~7' */

static const MusicPulse_t s_song[] = {
    {J(5), 300u, 0u}, {J(5), 300u, 0u}, {J(6), 600u, 0u}, {JH(1), 600u, 0u},
};
```

数组元素就与简谱逐音对应；换调只改 `JIANPU_BASE`（例如 1=♭B4 写 466），
简谱本身不用动。宏内部用十二平均律比值 `BUZZER_RATIO_*`（1e-5 定点）做整数运算，
结果是四舍五入的 Hz，和运行时查表最多差 1 Hz。

> 音效（`music_custom/`）不是旋律，没有「1=某调」的概念，直接写 Hz 即可。

门控：需在 `app_cfg.h` 定义 `LIB_MUSIC_USED`。
