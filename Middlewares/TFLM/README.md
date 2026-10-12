# STM32 神经网络部署工具链（TFLM）

本目录是 **TensorFlow Lite for Microcontrollers (TFLM)** 在本仓库的集成层：

- 工具链文档（本文件）
- conda 环境与 Python 依赖清单（`environment.yml` / `requirements.txt`）
- 上游源码一律按「第三方仓库 = `Middlewares/Third_Party/` 下的 git 子模块（SSH 拉取）」管理，**不 vendor 进主仓库**

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
| 环境隔离 | Miniconda / conda | 隔离 Python 工具链 | `Middlewares/TFLM/environment.yml` |
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
| 内核头 / 启动 | **CMSIS_6**（Core） | `core_cm4.h`/`core_cm7.h`、编译器抽象等 | 子模块 `Middlewares/Third_Party/CMSIS_6`（或复用 CubeMX 自带 `cubemx/*/Drivers/CMSIS`） |
| 序列化运行时 | **FlatBuffers** | `.tflite` schema 的读取代码 | 子模块 `Middlewares/Third_Party/flatbuffers` |
| 交叉编译器 | `arm-none-eabi-gcc` | 编译上述 C/C++ 源码 | 主机侧工具链 |
| 可选：DSP | CMSIS-DSP | 仅当启用 CMSIS-NN 的 float/f16 内核（`ARM_NN_ENABLE_F32/F16=1`）或模型含 DSP 算子 | 未默认拉取，见第七节 |
| 可选：FFT | KissFFT (v130) | 模型含 FFT 算子时 | 未默认拉取，见第七节 |
| 可选：调试打印 | eyalroz/printf | TFLM 的 debug log 回调输出 | 未默认拉取，见第七节 |

**不适用**（本仓库 MCU 无 NPU）：Ethos-U 驱动、Ethos-U 平台、Vela 编译器、Corstone 平台。

---

## 三、依赖来源与固定版本

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

## 四、目录结构

```
Middlewares/
├── TFLM/                          # 本集成层：文档 + conda 环境 + 依赖清单
│   ├── README.md
│   ├── environment.yml            # conda 环境定义
│   └── requirements.txt           # pip 依赖
└── Third_Party/                   # 全部第三方仓库（git 子模块，SSH）
    ├── tflite-micro/              # TFLM 上游
    ├── flatbuffers/               # .tflite schema / 运行时
    ├── CMSIS_6/                   # CMSIS Core 头（CMSIS/Core/Include）
    └── CMSIS-NN/                  # int8 优化内核（Include / Source）
```

> `CMSIS_6` 与 CubeMX 生成的 `cubemx/*/Drivers/CMSIS` 内容重叠。CMake 里**只保留一份** `CMSIS/Core/Include` 到 include 路径，避免头文件重定义。

---

## 五、conda 环境

```bash
# 创建（在 Middlewares/TFLM 下执行）
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

## 六、从模型到 MCU 的流程

```
        PC (conda env: tflm)                                STM32
┌────────────────────────────────┐                ┌──────────────────────────┐
│ 1. 训练 (TF/Keras 或 PyTorch)   │                │                          │
│ 2. 导出 .tflite                 │                │  TFLM 解释器             │
│    · PTQ: 代表性数据集 → int8   │                │   + CMSIS-NN 内核        │
│    · QAT: TF-MOT 包装后重训     │                │                          │
│ 3. xxd -i / 脚本 → model_data.cc│                │  MicroInterpreter        │
│ 4. 放进 app/ 源码并 add_subdirectory │  ──────►  │  .Invoke() 跑推理        │
└────────────────────────────────┘   arm-none-eabi │                          │
                                     交叉编译+烧录  └──────────────────────────┘
```

要点：

1. **量化优先 int8**：Cortex-M4F/M7 上 int8 + CMSIS-NN 比 float32 快数倍且省 RAM；TFLM 的默认内核也是 int8。
2. **模型即常量数组**：`.tflite` 字节被编译进 Flash，`MicroInterpreter` 直接指向它，无需文件系统。
3. **张量内存池**：TFLM 用一块静态 `tensor_arena`（`MicroMutableOpResolver` + `AllOpsResolver` 二选一），大小用 `Interpreter` 的 arena 用量实测后再定。
4. **算子裁剪**：为省 Flash，用 `MicroMutableOpResolver` 只注册模型真正用到的算子，而不是 `AllOpsResolver`。

---

## 七、可选第三方库（未默认拉取）

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
