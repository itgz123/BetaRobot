# STM32 神经网络部署工具链（NNoM）

本目录（`lib/lib_nnom/tools/`）是 **NNoM（Neural Network on Microcontroller）**
的 **PC 侧工具链文档与依赖清单**：

- 工具链文档（本文件）
- conda 环境与 Python 依赖清单（`environment.yml` / `requirements.txt`）

上游源码一律按「第三方仓库 = `Middlewares/Third_Party/` 下的 git 子模块（SSH 拉取）」
管理，**不 vendor 进主仓库**。

目标硬件（见 `cubemx/*/*.ioc`）：

| 板卡 | MCU | 内核 | 备注 |
| --- | --- | --- | --- |
| `DJI_A` | STM32F407IGH6 | Cortex-M4F | 有 FPU，无 NPU |
| `DJI_C` | STM32F427IIH6 | Cortex-M4F | 有 FPU，无 NPU |
| `DM_MC02` | STM32H723VGT6 | Cortex-M7 | 有 FPU，无 NPU |
| `DM_MC02_HALF_RUDDER` | STM32H723VGT6 | Cortex-M7 | 有 FPU，无 NPU |

> 这几款 MCU **都没有 NPU**，所以 ARM 侧的加速手段只有 **CMSIS-NN**（Cortex-M 的
> SIMD/DSP 优化内核），**不涉及 Ethos-U / Vela / Neural-ART**。

---

## 一、PC 侧工具（x86_64 主机：训练 → 量化 → 生成 weights.h）

NNoM 与 TFLM 最大的差别在于**转换环节**：TFLM 用官方 `TFLiteConverter` 导出
`.tflite`（FlatBuffer），而 NNoM 用**自己的 Python 建模接口 + 量化器**直接把模型
编译成 C 头文件 `weights.h`（一张张量化后的常量数组 + 结构描述）。因此 PC 侧的
核心是 nnom 这个 Python 包本身。

| 分类 | 工具 | 用途 | 本仓库落点 |
| --- | --- | --- | --- |
| 环境隔离 | Miniconda / conda | 隔离 Python 工具链 | `lib/lib_nnom/tools/install/environment.yml` |
| 训练框架 | TensorFlow 2.14 + `tf.keras` | 训练 / 微调模型（nnom 只吃 TF 2.x / Keras 2） | conda 环境 `nnom` |
| 建模 / 量化 / 代码生成 | **nnom**（上游 `scripts/`） | 结构化建模 API + int8 量化 + 生成 `weights.h` | 子模块 `Middlewares/Third_Party/nnom`，`pip install` 后 `from nnom import *` |
| 旧版分层接口 | nnom `nnom_utils.py` | 0.4.0 之前的 Layer Interface（现在仍可用） | 同上，`from nnom import *` 也导出 |
| 逐层量化校准 | nnom `layers_output_ranges` / `quantize_method` | max_min / KLD 两种量化标定 | 同上 |
| 权重可视化 | Netron / `show_weights` | 查看算子 / 张量 / 量化参数 | conda 环境 |
| 构建 | CMake + Ninja | 本仓库统一构建入口 | 根目录 `CMakeLists.txt` / `.vscode/tasks.json` |
| 交叉编译 | `arm-none-eabi-gcc`（Arm GNU Toolchain） | 生成 Cortex-M 目标码 | 本机 `/opt/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi` |
| 代码生成（HAL） | STM32CubeMX | 生成 HAL/初始化代码 | `cubemx/` |
| 烧录 / 调试 | OpenOCD / pyOCD / DAPLink | 下载与调试 | `cmake --build --preset Debug --target download_dap` |
| ST 官方替代链路（可选） | ST Edge AI Core / X-CUBE-AI | 把模型转成 ST 自有 C 库，**与 NNoM / TFLM 三选一** | 不随本仓库 |

> **nnom 的 Python 包要单独安装**（不在 `requirements.txt` 里，因为它是本地子模块
> 路径而非 PyPI 包）。建好 conda 环境后从子模块就地安装，锁定同一次 pin：
>
> ```bash
> # 在仓库根目录执行
> conda run -n nnom pip install ./Middlewares/Third_Party/nnom
> # 或开发模式（改上游脚本后免重装）
> conda run -n nnom pip install -e ./Middlewares/Third_Party/nnom
> ```
>
> 安装后，C 库源码位置可用 `python -c "import nnom_core; print(nnom_core.__path__[0])"` 查到
> —— 这就是 MCU 侧要编译的那份 `inc/` + `src/` + `port/`。

---

## 二、ARM / STM32 侧工具（目标机：推理运行时）

| 分类 | 工具 | 用途 | 来源 |
| --- | --- | --- | --- |
| 推理引擎 | **NNoM**（纯 C，C99） | 解释执行 `weights.h` 描述的模型（`nnom_model_t` + 逐层 run） | 子模块 `Middlewares/Third_Party/nnom` 的 `src/` |
| 内核头 | NNoM `inc/` | `nnom.h` 等公共头 | 同上 |
| 平台移植头 | NNoM `port/nnom_port.h` | **必须由集成方提供**：内存 / 时间戳 / 日志 / 后端开关 | 上游给模板，本项目需适配（见第三节） |
| 加速内核（可选） | **CMSIS-NN** | 开 `NNOM_USING_CMSIS_NN` 时，卷积/全连接/池化走 Cortex-M4/M7 优化实现 | 子模块 `Middlewares/Third_Party/CMSIS-NN` |
| 交叉编译器 | `arm-none-eabi-gcc` | 编译上述 C 源码 | 主机侧工具链 |

**不适用**（本仓库 MCU 无 NPU）：Ethos-U 驱动、Ethos-U 平台、Vela 编译器、Corstone 平台。

---

## 三、接入状态（重要）

本次只完成「上游子模块 + PC 侧工具链文档/依赖」两件事。**构建接线尚未做**，
也就是说：`lib/lib_nnom/` 目前**只有 `tools/` 文档**，还没有 `CMakeLists.txt`，
`lib/CMakeLists.txt` 也**尚未 include 它**。要在某个 app 里用 NNoM，至少还需要：

1. **`lib/lib_nnom/CMakeLists.txt`**（对齐 `lib/lib_tflm/CMakeLists.txt` 的门控写法）：
   - 用 `check_define(... LIB_NNOM_USED)` 逐 app 判定；不定义时立即 `return()`，
     **完全不触碰第三方路径**，不用 NNoM 的人照样能构建；
   - 定义时才检查子模块存在，并把 `Middlewares/Third_Party/nnom/{inc,port,src}` 加入
     源清单 / include 路径；
   - NNoM 的源清单**可以用 `file(GLOB)`**（`src/core/*.c`、`src/layers/*.c`、
     `src/backends/*.c`，与上游 `SConscript` 一致），不像 TFLM 那样需要脚本生成
     —— 因为它不带测试、也没有同名替换规则；
   - 在 `lib/CMakeLists.txt` 末尾 `include("${CMAKE_CURRENT_LIST_DIR}/lib_nnom/CMakeLists.txt")`。
2. **`lib/lib_nnom/nnom_port.h`**（NNoM 强制要求 `#include "nnom_port.h"` 可解析）：
   - 上游 `port/nnom_port.h` 只是模板，直接用它会把 `nnom_malloc/free/memset/memcpy`
     绑到 libc，且 `nnom_us_get()/ms_get()` 返回 0、`NNOM_LOG` 走 `printf`；
   - 本项目应改接 `bsp`（时间戳接 `bsp_dwt`，日志接 `bsp_log`），并**用静态内存**
     （`NNOM_USING_STATIC_MEMORY` + `nnom_set_static_buf()`）以避免在 MCU 上引入堆
     —— 本仓库对固件有**无堆约束**：`cmake/check_no_heap.cmake` 在链接后用 `nm`
     断言（见顶层 `CMakeLists.txt` 的说明），接入时要一并对齐。默认模板里的
     `nnom_malloc/free` → libc `malloc/free` **必须换掉**。

**后端选择：先纯 C**（本仓库当前决定）。`nnom_port.h` 里
`NNOM_USING_CMSIS_NN` **保持注释掉**：不依赖 CMSIS-NN，任何 app 都能编、好定位问题。
跑通、量过性能后，再按需打开 CMSIS-NN 加速（届时确认 nnom 与所 pin 的 CMSIS-NN
版本兼容，并把 `Middlewares/Third_Party/CMSIS-NN/Include` 加进 `-I`）。

> **CHW / HWC**：NNoM 默认 HWC，与 CMSIS-NN 兼容；`NNOM_USING_CHW` 仅用于带硬件加速器
> 的平台（如 K210 的 KPU），**与本仓库无关，不要开**。

---

## 四、依赖来源与固定版本

| 依赖 | 类型 | 版本 / revision | 来源 |
| --- | --- | --- | --- |
| nnom | 子模块（SSH） | `4d1d8fc0`（`v0.4.3-56-g4d1d8fc`，仓库内版本 `0.5.0a1`） | `git@github.com:majianjia/nnom.git` |
| tensorflow-cpu | pip | `==2.14.1` | PyPI |
| keras | pip | `>=2.14,<3.0.0` | PyPI |
| numpy / scipy / scikit-learn / matplotlib | pip | 见 `requirements.txt` | PyPI / conda-forge |
| arm-none-eabi-gcc | 系统工具 | 本机 15.2.rel1 | Arm GNU Toolchain |
| CMSIS-NN | 子模块（SSH） | 见 `lib/lib_tflm/tools/install/README.md`（**仅开加速后端时需要**） | `git@github.com:ARM-software/CMSIS-NN.git` |

> 上游有发布 tag（最新 `v0.4.3`）。本次 pin 的是**默认分支的 HEAD**（含 0.5.0a1 的
> 目录重构：`src/{core,layers,backends}` + `port/`）。若要换到稳定 tag：
>
> ```bash
> git -C Middlewares/Third_Party/nnom checkout v0.4.3
> ```
> 换完记得重跑一次 PC 端 `pip install`，并同步更新本表与第三节的路径说明。

子模块初始化（新克隆仓库后执行一次）：

```bash
git submodule update --init --recursive
# 若子模块地址还是 HTTPS/相对路径，可统一切成 SSH：
# git config --file .gitmodules submodule.<name>.url git@github.com:<org>/<repo>.git
```

---

## 五、目录结构

```
lib/lib_nnom/                      # 集成层（目前只有 PC 侧文档，见第三节「接入状态」）
└── tools/
    └── install/
        ├── README.md              # 本文件
        ├── environment.yml        # conda 环境定义（name: nnom）
        └── requirements.txt       # pip 依赖

Middlewares/Third_Party/nnom/      # 上游（git 子模块，SSH）
├── inc/                           # nnom.h / nnom_layers.h / nnom_tensor.h / ...
├── src/                           # 纯 C 实现
│   ├── core/                      #   nnom.c / nnom_layers.c / nnom_tensor.c / nnom_utils.c
│   ├── layers/                    #   逐层实现（conv2d / dense / rnn / softmax / ...）
│   └── backends/                  #   nnom_local.c（纯 C）/ nnom_local_q15.c
├── port/nnom_port.h               # 平台移植模板（需本项目适配，见第三节）
├── scripts/                       # PC 端 Python：nnom.py（结构化）/ nnom_utils.py（分层）
└── examples/                      # 上游示例（mnist / kws / uci-har …）
```

---

## 六、conda 环境

```bash
# 创建（在 lib/lib_nnom/tools/install 下执行）
conda env create -f environment.yml

# 激活
conda activate nnom

# 安装 nnom 本体（本地子模块，非 PyPI 包）—— 见第一节末的说明
pip install ./Middlewares/Third_Party/nnom    # 在仓库根执行

# 更新（改了 requirements.txt 后）
conda env update -f environment.yml --prune
```

依赖清单也支持裸 pip（不装 conda 时）：

```bash
python -m venv .venv && . .venv/bin/activate   # 需 Python 3.11（TF 2.14 不支持 3.12）
pip install -r requirements.txt
pip install ./Middlewares/Third_Party/nnom
```

> **不要和 tflm 环境混用**：nnom 要 Keras 2 / TF ≤2.14，tflm 要 Keras 3 / TF ≥2.16，
> 两者互斥。两个环境名分别是 `nnom` 与 `tflm`，按需 `conda activate` 切换。

---

## 七、从模型到 MCU 的流程

```
        PC (conda env: nnom)                                 STM32
┌──────────────────────────────────────┐            ┌──────────────────────────┐
│ 1. 用 tf.keras 建模/训练               │            │  NNoM 运行时（纯 C）      │
│    (from nnom import * 提供结构化 API) │            │   nnom_model_t            │
│ 2. int8 量化 + 逐层校准                │            │   nnom_model_run()        │
│    generate_model(model, x_test,       │            │                          │
│                   name='weights.h')    │            │  weights.h 里的常量数组   │
│ 3. 产出 weights.h（量化权重 + 结构）    │  ───────►  │  被直接编进 Flash         │
│ 4. 放进 app/（或 lib/lib_nnom/）       │  arm-none- │                          │
│    并开 LIB_NNOM_USED（待接线）        │  eabi 编译 │                          │
└──────────────────────────────────────┘   +烧录     └──────────────────────────┘
```

要点：

1. **量化优先 int8**：Cortex-M4F/M7 上 int8 比 float32 快数倍且省 RAM。nnom 的
   `generate_model` 默认就做 per-tensor（可开 per-channel）int8 量化。
2. **模型即 C 数组**：`weights.h` 被编译进 Flash，NNoM 直接指向它，无需文件系统。
3. **内存由 NNoM 管理**：`nnom_model_create()` 按 `weights.h` 的结构描述一次性规划
   张量内存（推荐配 `NNOM_USING_STATIC_MEMORY`，见第三节）。
4. **nnom 结构化接口的两套脚本**：新代码用 `nnom.py`（`generate_model`，支持 per-channel
   量化与膨胀卷积），旧代码用 `nnom_utils.py`（分层 `generate_weights`）。上游 0.4.0 起
   以结构化接口为默认。

---

## 八、NNoM 与 TFLM 怎么选

两条链路在本仓库**并存**（各自独立子模块与 conda 环境，互不依赖）：

| 维度 | NNoM | TFLM |
| --- | --- | --- |
| 语言 / 运行时 | 纯 C（C99），无 C++ | C++17 解释器 |
| 模型载体 | 生成的 `weights.h`（C 数组） | `.tflite`（FlatBuffer） |
| 转换方式 | nnom 自有量化器 → 直接出 C 头 | TFLiteConverter → `.tflite` → xxd/脚本转 C |
| 算子覆盖 | 少而精（卷积/全连接/RNN/池化等常见层） | 全（需自行裁剪注册） |
| Flash 占用 | 小（按用到的层链接） | 较大（解释器 + 内核，实测 4 算子约 +68 KB） |
| 上手成本 | 低（Python 建模像 Keras） | 中（要处理 FlatBuffer / arena / 注册表） |
| 适用 | 中小模型、快速部署 | 复杂/大模型、需与 TF 生态对齐 |

---

## 九、可选：开 CMSIS-NN 后端

本项目**当前默认纯 C**（第三节）。要用 CMSIS-NN 加速时：

1. 在适配后的 `nnom_port.h` 里 `#define NNOM_USING_CMSIS_NN`；
2. 构建时把 `Middlewares/Third_Party/CMSIS-NN/Include` 与 `.../CMSIS-NN` 加入 `-I`
   （CMSIS-NN 的整数路径自带 `arm_nn_math_types.h`，**不需要 CMSIS_6 Core 头**，
   理由同 `lib/lib_tflm/tools/install/README.md` 第四节的说明）；
3. 确认 nnom 的 backend 与所 pin 的 CMSIS-NN 版本兼容（不一致时以能编过、结果对齐为准）。

> **CHW 与 CMSIS-NN 不兼容**：开了 CMSIS-NN 就必须是 HWC（默认值），不要同时开
> `NNOM_USING_CHW`。
