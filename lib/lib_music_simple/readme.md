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

频率换算全在 `drv/drv_buzzer_simple/drv_buzzer_note.h`（纯编译期宏，没有 `.c`）：

| 入口 | 用途 | 位置 |
| --- | --- | --- |
| `BUZZER_NOTE_HZ(SONG_BASE, d, oct)` | **编译期**：三个输入 → Hz，曲谱数组直接用 | 宏 + 生成表 `drv_buzzer_note_table.h` |
| `BUZZER_NOTE_MS(bpm, num, den)` | **编译期**：几分音符 → ms | 宏（头文件） |

曲谱数组是 `static const`，初始化必须是常量表达式，所以曲谱里用宏。
`BUZZER_NOTE_HZ` 的三个输入是：

- `base`：简谱「1」的**实际发声音名 token**（`BB4`、`C5`、`A4`…），不是频率；
- `d`：简谱数字 1~7；临时记号可改用 `BUZZER_NOTE_HZ_SEMI(base, semi, oct)`；
- `oct`：上下加点，`+1` 高八度、`-1` 低八度、`+2` 两点。

内部先把音名映射成相对 C4 的半音偏移（C4 = 0、A4 = +9 = 440 Hz），再加上度数
和八度，最后查 `drv_buzzer_note_table.h` 里的宏表。音名 → 偏移用 `##` 拼接：
`BUZZER_PITCH(BB4)` 展开为 10。表由 `drv/drv_buzzer_simple/tools/gen_note_table.py`
生成，勿手改。

每个曲目文件先定基准音名，再起短别名；时值也用「几分音符」宏：

```c
#include "drv_buzzer_note.h"

#define BPM 80u
#define SONG_BASE BB4                    /* 1 = ♭B4 */
#define J(d)  BUZZER_NOTE_HZ(SONG_BASE, (d), 0)
#define JH(d) BUZZER_NOTE_HZ(SONG_BASE, (d), +1)
#define CELLS(n) BUZZER_NOTE_MS(BPM, (n), 16u) /* n 个 16 分音符 */

static const MusicPulse_t s_song[] = {
    {J(5), CELLS(2), 0u}, {J(5), CELLS(4), 0u}, {JH(1), CELLS(1), 0u},
};
```

数组元素就与简谱逐音对应；换调只改 `SONG_BASE`（例如 1=♭B4 写 `BB4`），
简谱本身不用动。生成表按十二平均律四舍五入公式得到，脚本带外部锚点自检。

常用时值宏：`BUZZER_QUARTER_MS`、`BUZZER_EIGHTH_MS`、`BUZZER_SIXTEENTH_MS`、
`BUZZER_HALF_MS`、`BUZZER_DOTTED_HALF_MS`、`BUZZER_DOTTED_QUARTER_MS`、
`BUZZER_TRIPLET_EIGHTH_MS`；通用式是 `BUZZER_NOTE_MS(bpm, num, den)`，
其中 `num/den` 是相对全音符的比例。

> 音效（`music_custom/`）不是旋律，没有「1=某调」的概念，直接写 Hz 即可。

门控：需在 `app_cfg.h` 定义 `LIB_MUSIC_USED`。
