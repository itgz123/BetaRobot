# lib_music_simple/host

PC 主机端的曲谱验证工具：**gcc 把真实的曲谱源码编成共享库 → Python(ctypes) 读取数组 → 模拟蜂鸣器合成为 FLAC**。
验证的就是固件里那份 `MusicSequence_t`，不做任何二次转写。

本目录不参与固件构建。

## 文件

| 文件 | 作用 |
| --- | --- |
| `app_cfg.h` | PC 编译用的桩，只定义 `LIB_MUSIC_USED`（不要加进固件 include 路径） |
| `music_host.{c,h}` | 把各 `MUSIC_*` 序列登记成名称 → `MusicSequence_t*` 的清单 |
| `build.sh` | `gcc -shared` 编译成 `build/libmusic_host.so` |
| `render_music.py` | ctypes 读数组 + 合成蜂鸣器波形 + 写 FLAC |
| `requirements.txt` | Python 依赖（numpy、soundfile） |
| `.python-version` | pyenv 版本固定为 3.12.6 |

## 环境

使用 pyenv 选中的 Python（同目录 `.python-version` 已固定 3.12.6），**不建 venv、不加 `--user`**：

```sh
python3 -m pip install -r requirements.txt
```

依赖会装进 `~/.pyenv/versions/3.12.6/lib/python3.12/site-packages`。
`soundfile` 0.12+ 的 manylinux wheel 自带 libsndfile，不需要另装系统库（本机也有系统 libsndfile）。

## 使用

```sh
cd lib/lib_music_simple/host

# 列出所有曲目/音效（脉冲数、总时长、音域）
python3 render_music.py --list

# 渲染《See You Again》吉他谱版（缺库或源码更新时自动 build）
python3 render_music.py --sequence MUSIC_SEE_YOU_AGAIN_GUITAR

# 渲染小星星；关闭谐振、输出到指定路径
python3 render_music.py -s twinkle_star --no-resonance -o /tmp/twinkle_dry.flac

# 对比混叠：原始采样方波（会把高次谐波折到 4 kHz 附近）
python3 render_music.py -s twinkle_star --bandlimit 0 -o /tmp/twinkle_alias.flac

# 全部渲染；打印每个脉冲，核对数组
python3 render_music.py --all --dump

# 指定输出目录 / 衰减 / 谐振参数
python3 render_music.py -s happy_birthday \
    --out-dir /tmp/music --decay-ms 300 --resonance 4000 --q 6
```

默认输出到 `build/audio/<序列名>.flac`（`build/` 被根 `.gitignore` 忽略）。
直接播放即可听辨旋律；和原简谱对照重点听音高走向、连音/休止的位置。

## 蜂鸣器模型

按 `ignore/计划.md` 实现，尽量贴近 `drv/drv_buzzer_simple` + 4 kHz 无源蜂鸣器：

1. **带限方波**：按傅里叶级数合成到 `--bandlimit`（默认 20 kHz），高电平在每个周期开头，
   占空比 = `volume * 0.5 * env`。这样不会像 48 kHz 直接采样的方波那样把高次谐波折叠到
   4 kHz 附近；`--bandlimit 0` 回退到原始采样方波（可用来对比混叠）。
   `volume_pct == 0` 用 `--default-volume`（默认 1.0），音量 0~1 → 占空比 0~0.5。
2. **线性衰减包络**：每个发声脉冲满音量起音，在 `min(decay_ms, 本拍时长)` 内线性衰减到 0；
   `--decay-ms 0` 退化为恒定音量（与固件一致，相邻同频会连成一声）。
   默认 `500 ms`，与两个 app 的 `BuzzerConfig.decay_ms` 对齐。
3. **淡入淡出默认关闭**（`--fade-ms 0`）：`decay_ms > 0` 时每个音的包络在音尾已经到 0，
   通常不需要额外渐变；如果设了 `--decay-ms 0`，可加 `--fade-ms 5` 避免频率切换“咔哒”。
4. **4 kHz 二阶谐振带通**：RBJ band-pass，`--q 6`（真实蜂鸣器约 3~10）。
   低频基频被压、靠近 4 kHz 的谐波被放大，听感会“变尖、音高感偏移”——这是真实器件的特性。
   `--no-resonance` 关闭，听原始方波。
5. **归一化**：默认峰值归一化到 0.95 后写 `PCM_16` FLAC；`--no-normalize` 关闭。

二阶 IIR 用“冲激响应 + FFT 卷积”实现（numpy-only），不依赖 scipy。

## 注意

- 曲谱源码里有 `static const` 数组和 `LIB_MUSIC_USED` 门控；`build.sh` 靠 `-I host`
  命中桩 `app_cfg.h`，并把 `lib/lib_music_simple`、`drv/drv_buzzer_simple` 加入头文件路径。
- 新增曲目时，在 `music_host.c` 的表里加一行即可被 Python 发现。
- `see_you_again_guitar` 已参与固件构建（`lib/CMakeLists.txt`），本工具也一并编译它。
