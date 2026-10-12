/**
 * @file lib_tflm_port.cc
 * @brief lib_tflm 的平台适配：给 TFLM 提供时间源
 *
 * @note TFLM 把少量"平台钩子"设计成由目标自行提供（micro_time.h 的注释即此意，
 *       上游每个平台各有一份实现：bluepill/、cortex_m_corstone_300/ 等）。
 *       时间源只被 MicroProfiler 用来统计算子耗时，**不参与推理正确性**。
 *
 * @note 上游 cortex_m_generic/micro_time.cc 也走 DWT 循环计数器，但要额外定义
 *       CMSIS_DEVICE_ARM_CORTEX_M_XX_HEADER_FILE（器件头文件名随开发板变化）与
 *       ARMCMx 宏。这里直接用本工程的 bsp_dwt（已封装 DWT，且不受中断开关影响），
 *       不必按开发板映射头文件名。上游那份已在 tools/gen_sources.py 的
 *       DROP_BASENAMES 里排除，二者不会重定义。
 *
 * @warning 本文件是 lib_tflm 中**唯一**与平台耦合的地方（lib 层其余模块保持
 *          硬件无关）。若不需要 profiling，可删掉它并撤销 gen_sources.py 里的
 *          排除项，改回上游 micro/micro_time.cc 的参考实现（恒返回 0）。
 *
 * @warning 依赖 DWT_Init() 已完成（BSP_DWT_USED 由 bsp 初始化，早于任何推理）。
 *          未初始化时 DWT_GetTimeUs 读到的计数器恒 0，只影响耗时统计。
 */

#include "app_cfg.h"

#ifdef LIB_TFLM_USED

#include "bsp_dwt.h" /* DWT_GetTimeUs */

#include "tensorflow/lite/micro/micro_time.h"

namespace tflite
{
    namespace micro
    {

        /*!
         * @brief 时间源频率：DWT 计数器按 1 µs 递增（DWT_GetTimeUs 的刻度）
         */
        uint32_t ticks_per_second()
        {
            return 1000000;
        }

        /*!
         * @brief 当前时间（µs）
         * @note 截断到 32 位：回绕周期约 71.6 min。TFLM 内部按无符号相减取差值，
         *       单次推理远小于回绕周期，故不做回绕处理
         */
        uint32_t GetCurrentTimeTicks()
        {
            return static_cast<uint32_t>(DWT_GetTimeUs());
        }

    } // namespace micro
} // namespace tflite

#endif /* LIB_TFLM_USED */
