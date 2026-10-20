/**
 * @file bsp_dwt.h
 * @brief DWT 功能族伞形聚合头（CYCCNT 计时 / 性能计数 / 比较器 / PC 采样）
 * @note DWT_Init() 是整个功能族的唯一初始化入口，位于 bsp_dwt_cyccnt。
 * @note 各子模块实现由 app_cfg.h 的 BSP_DWT_<SUB>_USED 单独门控；本头无条件
 *       聚合声明，关闭的模块被调用时会在链接期暴露未定义符号。
 */

#ifndef __BSP_DWT_H
#define __BSP_DWT_H

#include "bsp_dwt_cyccnt.h"
#include "bsp_dwt_perf.h"
#include "bsp_dwt_compare.h"
#include "bsp_dwt_pcsample.h"

#endif /* __BSP_DWT_H */
