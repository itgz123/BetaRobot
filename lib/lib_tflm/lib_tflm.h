/**
 * @file lib_tflm.h
 * @brief lib_tflm 对外接口：TFLM（TensorFlow Lite for Microcontrollers）+ CMSIS-NN
 *
 * @note 本模块**不做封装**：只把 TFLM 的原生 C++ 接口聚到一处并接进构建，
 *       算子选择、张量搬运、内存 arena 全按 TFLM 原始用法写，见官方
 *       tensorflow/lite/micro/ 下的头文件与示例。因此引用本模块的 app 用
 *       **.cpp** 写（TFLM 是 C++ 库，不能从 .c 直接调）。
 *
 * @note TFLM 是**运行时库**（解释器 + 算子内核）随固件一起编译；模型是数据，
 *       不是被生成的代码 —— 用 xxd/convert 把 .tflite 变成 C 数组即可。
 *       （与本仓库另一条路线 X-CUBE-AI 的"生成 C 代码"不同。）
 *
 * ---- 使用前提（缺一不可）----
 *
 * 1. 拉子模块（TFLM / CMSIS-NN / flatbuffers 三者版本需配套）：
 *        git submodule update --init --recursive
 * 2. 在 **app/<app>/app_cfg.h** 里加开关（只影响该 app，别的 app 不受影响）：
 *        #define LIB_TFLM_USED
 *    未定义时 lib_tflm/CMakeLists.txt 立刻 return，**完全不碰第三方路径**，
 *    所以不用 TFLM、也没拉子模块的人照样能构建，不会报错。
 *
 * ---- 典型流程 ----
 *
 * @code
 *   #include "app_cfg.h"
 *   #include "lib_tflm.h"
 *
 *   // 1) 模型：xxd -i model.tflite > model_data.cc，得到 model_tflite[] / model_tflite_len
 *   const tflite::Model* model = tflite::GetModel(model_tflite);
 *
 *   // 2) 算子表：只注册模型真正用到的算子（注册越多，固件越大）
 *   static tflite::MicroMutableOpResolver<4> resolver;
 *   resolver.AddConv2D();
 *   resolver.AddFullyConnected();
 *   resolver.AddSoftmax();
 *   resolver.AddReshape();
 *
 *   // 3) arena：推理用工作内存，大小需自行试（TFLM 会在不够时报错告知）
 *   static uint8_t tensor_arena[16 * 1024];
 *
 *   static tflite::MicroInterpreter interpreter(model, resolver, tensor_arena,
 *                                               sizeof(tensor_arena));
 *   interpreter.AllocateTensors();
 *
 *   // 4) 推理：input(0) → Invoke() → output(0)，输入输出都是量化 int8 张量
 *   interpreter.input(0)->data.int8[...] = ...;
 *   interpreter.Invoke();
 *   ... = interpreter.output(0)->data.int8[...];
 * @endcode
 *
 * ---- 日志 ----
 *
 * TFLM 的错误/告警走 DebugLog，默认没有接收者（**静默**）。建议在建解释器前
 * 挂上，排查模型/arena 问题时才有输出：
 *
 * @code
 *   static void TflmLog(const char* s) {
 *       BSPLOG(&log_uart, BSP_LOG_LVL_ERROR, "%s", s);   // 实例与等级按 app 定
 *   }
 *   RegisterDebugLogCallback(TflmLog);
 * @endcode
 *
 * ---- 时间源 ----
 *
 * MicroProfiler 的耗时统计由 lib_tflm_port.cc 提供（接 bsp_dwt，需 DWT_Init 已跑）。
 * 不用 profiling 时不必传 profiler 给 MicroInterpreter。
 */

#ifndef __LIB_TFLM_H
#define __LIB_TFLM_H

#include "app_cfg.h"

#ifdef LIB_TFLM_USED

/*------------- TFLM 原生接口（含它需要的 flatbuffers schema） -------------*/

/* 解释器与算子表 */
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_resource_variable.h"

/* 模型 flatbuffer */
#include "tensorflow/lite/schema/schema_generated.h"

/* 平台钩子与日志 */
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/debug_log.h"
#include "tensorflow/lite/micro/cortex_m_generic/debug_log_callback.h"
#include "tensorflow/lite/micro/system_setup.h"

/* 性能统计（时间源见 lib_tflm_port.cc） */
#include "tensorflow/lite/micro/micro_profiler.h"

/* 注意：CMSIS-NN 的加速内核已编进本模块（见 CMakeLists.txt 的源清单），
   经 OPERATOR 注册即可生效，不需要在这里 include arm_nnfunctions.h。 */

#else

#error "引用了 lib_tflm.h，但 app_cfg.h 里没有 #define LIB_TFLM_USED"

#endif /* LIB_TFLM_USED */

#endif /* __LIB_TFLM_H */
