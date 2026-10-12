# STM32 神经网络部署工具链（TFLM）

本目录（`lib/lib_tflm/tools/`）是 **TensorFlow Lite for Microcontrollers (TFLM)** 的
**PC 侧工具链文档与依赖清单**：

- 工具链文档（本文件）
- conda 环境与 Python 依赖清单（`environment.yml` / `requirements.txt`）
- 源清单生成脚本（`gen_sources.py`）

上游源码一律按「第三方仓库 = `Middlewares/Third_Party/` 下的 git 子模块（SSH 拉取）」管理，
**不 vendor 进主仓库**。构建集成本身在上一层：`lib/lib_tflm/CMakeLists.txt`（见第三节）。

目标硬件（见 `cubemx/*/*.ioc`）：

| 板卡 | MCU | 内核 | 备注 |
| --- | --- | --- | --- |
| `DJI_A` | STM32F407IGH6 | Cortex-M4F | 有 FPU，无 NPU |
| `DJI_C` | STM32F427IIH6 | Cortex-M4F | 有 FPU，无 NPU |
| `DM_MC02` | STM32H723VGT6 | Cortex-M7 | 有 FPU，无 NPU |
| `DM_MC02_HALF_RUDDER` | STM32H723VGT6 | Cortex-M7 | 有 FPU，无 NPU |

> 这几款 MCU **都没有 NPU**，所以 ARM 侧的加速手段只有 **CMSIS-NN**（Cortex-M 的 SIMD/DSP 优化内核），**不涉及 Ethos-U / Vela / Neural-ART**。若日后换到带 NPU 的型号（如 STM32N6），才需要另加 ST Edge AI 的 NPU 链路。

---

## 一、PC 侧工具（x86_64 主机：训练 → 转换 → 量化 → 生成 C 数组）

| 分类 | 工具 | 用途 | 本仓库落点 |
| --- | --- | --- | --- |
| 环境隔离 | Miniconda / conda | 隔离 Python 工具链 | `lib/lib_tflm/tools/environment.yml` |
| 训练框架 | TensorFlow + Keras | 训练 / 微调模型（TFLite 官方路径） | conda 环境 `tflm` |
| 训练框架（可选） | PyTorch + `ai-edge-torch` | 已有 PyTorch 模型时导出为 TFLite | 可选依赖 |
| 转换 | TFLite Converter（`tf.lite.TFLiteConverter`，随 TF 提供） | `.keras`/SavedModel/`.h5` → `.tflite` | conda 环境 |
| 量化 | TF Model Optimization Toolkit | 量化感知训练 (QAT)；int8 训练后量化 (PTQ) 由 Converter 直接支持 | conda 环境 |
| 主机侧运行时（可选） | `ai-edge-litert` | 在 PC 上先跑通/比对 quantized 模型 | conda 环境 |
| 序列化 | `flatbuffers`（Python 包）、`flatc`（C++ 编译器，随子模块构建） | `.tflite` 本质是 FlatBuffer；自定义 schema 时用 | `Middlewares/Third_Party/flatbuffers` |
| 模型查看 | Netron | 可视化算子/张量/量化参数 | conda 环境 |
| 模型 → C 数组 | `xxd -i`（vim-common 提供）或 Python 脚本 | `.tflite` → `model_data.cc`（TFLM 加载的字节数组） | 见下方「流程」 |
| 构建 | CMake + Ninja | 本仓库统一构建入口 | 根目录 `CMakeLists.txt` / `.vscode/tasks.json` |
| 交叉编译 | `arm-none-eabi-gcc`（Arm GNU Toolchain） | 生成 Cortex-M 目标码 | 本机 `/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi` |
| 代码生成 | STM32CubeMX | 生成 HAL/初始化代码 | `cubemx/` |
| 烧录 / 调试 | OpenOCD / pyOCD / DAPLink | 下载与调试 | `cmake --build --preset Debug --target download_dap` |
| ST 官方替代链路（可选） | ST Edge AI Core / X-CUBE-AI | 把模型转成 ST 自有 C 库，**与 TFLM 二选一** | 不随本仓库 |

> **`xxd` 不在本机预装**（属于 `vim-common`）。装不了时可用 Python 兜底生成 C 数组：
>
> ```bash
> python -c "import sys;d=open(sys.argv[1],'rb').read();\
> print('#include <stdint.h>\nconst unsigned char model_data[] = {'+','.join(str(b) for b in d)+'};\nconst unsigned int model_data_len = sizeof(model_data);')" \
>   model.tflite > model_data.cc
> ```

---

## 二、ARM / STM32 侧工具（目标机：推理运行时）

| 分类 | 工具 | 用途 | 来源 |
| --- | --- | --- | --- |
| 推理引擎 | **TFLM**（tflite-micro） | 解释器 + 算子内核，加载 `.tflite` 字节数组推理 | 子模块 `Middlewares/Third_Party/tflite-micro` |
| 加速内核 | **CMSIS-NN** | int8 卷积/全连接/池化等的 Cortex-M4/M7 优化实现（`OPTIMIZED_KERNEL_DIR=cmsis_nn`） | 子模块 `Middlewares/Third_Party/CMSIS-NN` |
| 内核头 | **CMSIS_6**（Core） | 供 CMSIS-NN 的 float/f16 路径用；**int8 路径不需要它**，故当前不加入 include 路径（详见第四节末） | 子模块 `Middlewares/Third_Party/CMSIS_6`；CubeMX 另自带一套 CMSIS 5 给 HAL |
| 序列化运行时 | **FlatBuffers** | `.tflite` schema 的读取代码 | 子模块 `Middlewares/Third_Party/flatbuffers` |
| 交叉编译器 | `arm-none-eabi-gcc` | 编译上述 C/C++ 源码 | 主机侧工具链 |
| 可选：DSP | CMSIS-DSP | 仅当启用 CMSIS-NN 的 float/f16 内核（`ARM_NN_ENABLE_F32/F16=1`）或模型含 DSP 算子 | 未默认拉取，见第八节 |
| 可选：FFT | KissFFT (v130) | 模型含 FFT 算子时 | 未默认拉取，见第八节 |
| 可选：调试打印 | eyalroz/printf | TFLM 的 debug log 回调输出 | 未默认拉取，见第八节 |

**不适用**（本仓库 MCU 无 NPU）：Ethos-U 驱动、Ethos-U 平台、Vela 编译器、Corstone 平台。

---

## 三、接入方式（lib_tflm）

构建集成写在 `lib/lib_tflm/CMakeLists.txt`，由 `lib/CMakeLists.txt` include 进来。
门控沿用本仓库的 `LIB_XXX_USED` 惯例，**只影响定义它的那个 app**：

```c
// app/<app>/app_cfg.h
#define LIB_TFLM_USED
```

- **不定义**（默认）：`lib_tflm/CMakeLists.txt` 立刻 `return()`，**完全不触碰第三方路径**。
  不用 TFLM、也没拉子模块的人照样能 configure/build，**不会报错**。
- **定义**：把 TFLM + CMSIS-NN 的源码纳入该 app 的 target；此时才检查子模块是否存在，
  缺了直接 `FATAL_ERROR` 并提示 `git submodule update --init --recursive`。

源清单**不是 GLOB**，而是由 `tools/gen_sources.py` 从上游 `sources.inc` 逐条翻译
（含 `kernels/*.cc → kernels/cmsis_nn/*.cc`、`micro/*.cc → cortex_m_generic/*.cc`
两级同名替换，以及测试/基准过滤；GLOB 会带上测试且拿不到替换规则，导致重复符号）。
TFLM 升级后重跑：

```bash
python3 lib/lib_tflm/tools/gen_sources.py \
  --tflm-root Middlewares/Third_Party/tflite-micro \
  --cmsisnn-root Middlewares/Third_Party/CMSIS-NN
```

CMake 侧另外三处（在 CMakeLists 与顶层 `CMakeLists.txt` 里都有注释）：

| 项 | 内容 | 为什么 |
| --- | --- | --- |
| 编译宏 | `CMSIS_NN`、`TF_LITE_MCU_DEBUG_LOG` | TFLM 的 `kernels/*.h` 按 `CMSIS_NN` 切换「声明加速注册函数」还是「给 inline 回落版」——**app 侧 include `lib_tflm.h` 后会读到同一批头，两边必须一致**，否则注册的符号对不上。宏由模块自己 `set(... PARENT_SCOPE)` 报给 `build_app()` |
| C++ | 顶层打开 `CXX` + `C++17` + `-fno-rtti -fno-exceptions -fno-threadsafe-statics -fno-unwind-tables -fno-use-cxa-atexit` | TFLM 是 C++ 库；不关这些会把 libstdc++ 的异常/RTTI 支持拖进固件 |
| 平台钩子 | `lib_tflm_port.cc` 提供 `micro_time`（接 `bsp_dwt`） | 上游 cortex_m_generic 版要走 DWT + 器件头宏（随开发板变化），这里换成本工程的 bsp_dwt；不做 profiling 时该文件可删（见文件头注释） |

**实测占用**（DM_MC02，Debug，CMSIS-NN 已生效）：只注册 Conv2D + FullyConnected +
Softmax + Reshape 四个算子时，解释器 + 内核 + 注册表约 **+68 KB Flash / +0.7 KB RAM**
（75 808 B → 144 232 B Flash、21 568 B → 22 304 B DTCM）。不引用 TFLM 符号时
`--gc-sections` 会把它整体丢掉，**占 0**。张量 arena 是 app 自己开的静态数组，不含在上面。

**app 侧怎么用**：头注释见 `lib/lib_tflm/lib_tflm.h`（本模块不封装，暴露 TFLM 原生
C++ 接口，故 app 相关文件用 `.cpp`）。

---

## 四、依赖来源与固定版本

子模块 revision 对齐 **TFLM 官方 `tools/make/ext_libs/*_download.sh`** 钉死的版本，便于日后与上游比对：

| 依赖 | 类型 | 版本 / revision | 来源 |
| --- | --- | --- | --- |
| tflite-micro | 子模块（SSH） | `230c9d95` | `git@github.com:tensorflow/tflite-micro.git` |
| flatbuffers | 子模块（SSH） | tag `v25.9.23` | `git@github.com:google/flatbuffers.git` |
| CMSIS_6 | 子模块（SSH） | `5782d6f8` | `git@github.com:ARM-software/CMSIS_6.git` |
| CMSIS-NN | 子模块（SSH） | `4ab83cc3` | `git@github.com:ARM-software/CMSIS-NN.git` |
| TinyMPC | 子模块（SSH） | `023f36bd` | `git@github.com:TinyMPC/TinyMPC.git` |
| Python 依赖 | conda / pip | 见 `environment.yml` / `requirements.txt` | PyPI / conda-forge |
| arm-none-eabi-gcc | 系统工具 | 本机 15.2.rel1（TFLM 官方脚本拉 14.3.rel1） | Arm GNU Toolchain |
| xxd | 系统工具（可选） | vim-common | 发行版包管理器 |

子模块初始化（新克隆仓库后执行一次）：

```bash
git submodule update --init --recursive
# 若子模块地址还是 HTTPS/相对路径，可统一切成 SSH：
# git config --file .gitmodules submodule.<name>.url git@github.com:<org>/<repo>.git
```

---

## 五、目录结构

```
lib/lib_tflm/                      # 集成层（被 lib/CMakeLists.txt include）
├── CMakeLists.txt                 # 门控 + include 路径 + 编译宏 + 源清单
├── lib_tflm.h                     # 对外接口（TFLM 原生 C++ 头 + 用法说明）
├── lib_tflm_port.cc               # 平台钩子：micro_time ← bsp_dwt
└── tools/                         # PC 侧
    ├── README.md                  # 本文件
    ├── environment.yml            # conda 环境定义
    ├── requirements.txt           # pip 依赖
    └── gen_sources.py             # 从上游 sources.inc 生成源清单

Middlewares/Third_Party/           # 全部第三方仓库（git 子模块，SSH）
├── tflite-micro/                  # TFLM 上游
├── flatbuffers/                   # .tflite schema / 运行时
├── CMSIS-NN/                      # int8 优化内核（Include / Source）
└── CMSIS_6/                       # CMSIS Core 头（当前不加入 include 路径，见下）
```

> **CMSIS_6 当前不进 include 路径**：CMSIS-NN 的整数路径（本项目编进来的就是这些，
> f16/f32 已排除）自带 `arm_nn_math_types.h`，全仓不 include 任何 CMSIS Core 头。
> 而上游 Makefile 加它，是为了 float 路径的 `arm_math.h` 与 Cortex_DFP 器件头。
> 更要紧的是：本工程 CubeMX 自带一套 CMSIS 5，而 lib 的 `-I` 排在 HAL 之前，
> 全局加 CMSIS 6 会让所有 HAL/FreeRTOS 源改用 CMSIS 6 的头 —— 不必要地动摇 HAL。
> 将来要开 float 内核再加，并用 `set_source_files_properties` 限定到本模块的源。

---

## 六、conda 环境

```bash
# 创建（在 lib/lib_tflm/tools 下执行）
conda env create -f environment.yml

# 激活
conda activate tflm

# 更新（改了 requirements.txt 后）
conda env update -f environment.yml --prune
```

依赖清单也支持裸 pip（不装 conda 时）：

```bash
python -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt
```

---

## 七、从模型到 MCU 的流程

```
        PC (conda env: tflm)                                STM32
┌────────────────────────────────┐                ┌──────────────────────────┐
│ 1. 训练 (TF/Keras 或 PyTorch)   │                │                          │
│ 2. 导出 .tflite                 │                │  TFLM 解释器             │
│    · PTQ: 代表性数据集 → int8   │                │   + CMSIS-NN 内核        │
│    · QAT: TF-MOT 包装后重训     │                │                          │
│ 3. xxd -i / 脚本 → model_data.cc│                │  MicroInterpreter        │
│ 4. 放进 app/ 并开 LIB_TFLM_USED      │  ──────►  │  .Invoke() 跑推理        │
└────────────────────────────────┘   arm-none-eabi │                          │
                                     交叉编译+烧录  └──────────────────────────┘
```

要点：

1. **量化优先 int8**：Cortex-M4F/M7 上 int8 + CMSIS-NN 比 float32 快数倍且省 RAM；TFLM 的默认内核也是 int8。
2. **模型即常量数组**：`.tflite` 字节被编译进 Flash，`MicroInterpreter` 直接指向它，无需文件系统。
3. **张量内存池**：TFLM 用一块静态 `tensor_arena`（`MicroMutableOpResolver` + `AllOpsResolver` 二选一），大小用 `Interpreter` 的 arena 用量实测后再定。
4. **算子裁剪**：为省 Flash，用 `MicroMutableOpResolver` 只注册模型真正用到的算子，而不是 `AllOpsResolver`。

---

## 八、可选第三方库（未默认拉取）

按需再以子模块方式加入，**都用 SSH**，并沿用 TFLM 官方钉死的 revision：

```bash
# CMSIS-DSP：启用 CMSIS-NN 的 float/f16 内核时需要
git submodule add git@github.com:ARM-software/CMSIS-DSP.git Middlewares/Third_Party/CMSIS-DSP

# KissFFT v130：模型含 FFT 算子时
git submodule add git@github.com:mborgerding/kissfft.git Middlewares/Third_Party/kissfft
git -C Middlewares/Third_Party/kissfft checkout v130

# Cortex_DFP c2c70a97：仅在仓库内直接构建 TFLM 自带的 cortex_m_generic target 时需要
#（集成进 CubeMX 应用时不需要，设备支持由 cubemx/ 提供）
git submodule add git@github.com:ARM-software/Cortex_DFP.git Middlewares/Third_Party/Cortex_DFP
git -C Middlewares/Third_Party/Cortex_DFP checkout c2c70a97a20fb355815e2ead3d4a40e35a4a3cdf

# eyalroz/printf f8ed5a9b：TFLM debug log 走 printf 时
git submodule add git@github.com:eyalroz/printf.git Middlewares/Third_Party/printf
git -C Middlewares/Third_Party/printf checkout f8ed5a9bd9fa8384430973465e94aa14c925872d
```
