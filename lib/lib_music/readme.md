# lib_music（预留，当前不参与构建）

本目录预留给之后**基于 DAC 的音乐播放方案**（对应 `lib_music_simple` 的"非 simple"版本）。

- 当前的蜂鸣器方案在 `lib_music_simple/`（纯数据曲谱）+ `drv/drv_buzzer_simple/`（PWM 播放）。
- 目前工程里的板卡（DJI_C、DM_MC02 等）都没有 DAC 输出，只能走 PWM 蜂鸣器，
  所以本目录暂时没有源文件，也不在 `lib/CMakeLists.txt` 中登记。
- 将来上 DAC 的板卡再在这里实现；曲谱数据仍可复用 `lib_music_simple/` 中的 `MusicSequence_t`。
