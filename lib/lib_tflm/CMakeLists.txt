# lib_tflm —— TFLM（TensorFlow Lite for Microcontrollers）+ CMSIS-NN 源码接入
#
# 门控：只有 app/<app>/app_cfg.h 里定义了 LIB_TFLM_USED，才把 TFLM / CMSIS-NN
#       的源码纳入构建。未定义（默认）时本文件立刻 return，**不触碰任何第三方
#       路径** —— 所以不用 TFLM、也没拉子模块的人照样能构建，不会报错。
#
# 本文件由 lib/CMakeLists.txt include()，运行在 import_module() 的作用域里，
# 因此能读到 build_app() 设置的 ${app_dir}（逐 app 判定，各 app 互不影响）。
#
# 注意：TFLM 是**运行时库**（C++ 解释器 + 算子内核），随固件一起编译；模型是
# 数据（.tflite 字节 → C 数组）不是代码。上游 .cc 不能加 #ifdef，所以门控只能
# 在 CMake 层做（与 lib_crc 那种"文件体 #ifdef"不同）。
#
# 源清单由 tools/gen_sources.py 从上游 sources.inc 翻译而来（含 cmsis_nn /
# cortex_m_generic 两级同名替换）。TFLM 版本升级后，重跑该脚本重新生成。

if(NOT DEFINED app_dir)
    return() # 脱离 app 构建上下文（例如裸 include 本文件）时不做任何事
endif()

check_define(LIB_TFLM_ON "${app_dir}/app_cfg.h" LIB_TFLM_USED)
if(NOT LIB_TFLM_ON)
    return()
endif()

######################################
# 第三方子模块（仅在启用 LIB_TFLM_USED 时才要求存在）
######################################
set(_tp "${CMAKE_SOURCE_DIR}/Middlewares/Third_Party")
# 只列真正会用到的：CMSIS_6 没进 include 路径（原因见下方），故不做要求
foreach(_dep IN ITEMS
        "${_tp}/tflite-micro"
        "${_tp}/CMSIS-NN"
        "${_tp}/flatbuffers")
    if(NOT IS_DIRECTORY "${_dep}")
        message(FATAL_ERROR
            "LIB_TFLM_USED 已启用，但第三方子模块缺失：\n  ${_dep}\n"
            "请先拉取：git submodule update --init --recursive")
    endif()
endforeach()

######################################
# 编译宏（PARENT_SCOPE → build_app()，见顶层 CMakeLists.txt 的说明）
######################################
# 这两个必须对整个 target 生效，不能只给本模块的源：
#  - CMSIS_NN：TFLM 的 kernels/*.h 用它切换"是否有加速内核"——有则声明
#    Register_CONV_2D_INT8 等，没有则给 inline 回落版。app 侧 include lib_tflm.h
#    后会经 micro_mutable_op_resolver.h 读到同一批头，两边必须一致，否则注册的
#    符号对不上（上游 project_generation/Makefile 也把它放进 COMMON_FLAGS）。
#  - TF_LITE_MCU_DEBUG_LOG：compatibility.h 借此把 TFLITE_ABORT 从 libc abort()
#    换成"打印 HALTED 后自旋"，与上游 cortex_m_generic 的 PLATFORM_FLAGS 一致。
# 用 list(APPEND) 而不是 set("${MODULE_COMPILE_DEFINITIONS};...")：build_app() 把它
# 初始化成空串，后者会拼出个空元素（"…;;CMSIS_NN"）。list(APPEND) 读到的正是
# build_app() 作用域里的当前值（函数作用域对未定义的局部名回落到调用者），
# 于是多个模块各自追加也不会互相覆盖。
list(APPEND MODULE_COMPILE_DEFINITIONS CMSIS_NN TF_LITE_MCU_DEBUG_LOG)
set(MODULE_COMPILE_DEFINITIONS "${MODULE_COMPILE_DEFINITIONS}" PARENT_SCOPE)

######################################
# 头文件路径（相对 lib/，import_module() 会补 lib/ 前缀）
######################################
list(APPEND MODULE_INCLUDE_DIRS
    # 本模块自己的适配头（lib_tflm.h），app 侧 #include "lib_tflm.h"
    lib_tflm
    # tensorflow/lite/... 与 signal/... 的根
    ../Middlewares/Third_Party/tflite-micro
    # CMSIS-NN：Include/ 给 CMSIS-NN 自己（arm_nnfunctions.h、Internal/…）；
    # 仓库根给 TFLM 的 cmsis_nn 内核 —— 它们写的是 "Include/arm_nnfunctions.h"，
    # 与上游 ext_libs/cmsis_nn.inc 的两个 -I 一一对应
    ../Middlewares/Third_Party/CMSIS-NN
    ../Middlewares/Third_Party/CMSIS-NN/Include
    # flatbuffers 运行时（schema_generated.h 依赖）
    ../Middlewares/Third_Party/flatbuffers/include
)

# 注意这里**不加** CMSIS_6/CMSIS/Core/Include：CMSIS-NN 的整数路径（我们全部
# 编进来的就是这些，f16/f32 已排除）自带 arm_nn_math_types.h，不 include 任何
# CMSIS Core 头（全仓 grep 无 cmsis_compiler.h / core_cm*.h）。而上游 Makefile 加
# 它，是为了 float 路径的 arm_math.h 与 Cortex_DFP 的器件头（micro_time 用 DWT）。
# 更要紧的是本工程 CubeMX 自带一套 CMSIS 5 Core，而 lib 的 -I 排在 HAL 之前，
# 若全局加 CMSIS 6 会把所有 HAL/FreeRTOS 源也换成 CMSIS 6 的头 —— 不必要地动
# 摇 HAL。将来要开 float 内核，再按需加（并用 set_source_files_properties
# 限定到本模块的源，别进 target 的全局 -I）。

######################################
# 源文件（相对 lib/；由 tools/gen_sources.py 生成，勿手改）
######################################
set(_lib_tflm_sources
    # 265 个源文件（由 tools/gen_sources.py 生成，勿手改）
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/cortex_m_generic/debug_log.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/fake_micro_context.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/flatbuffer_conversions.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/flatbuffer_utils.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/hexdump.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/memory_helpers.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/micro_allocation_info.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/micro_allocator.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/micro_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/micro_context.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/micro_interpreter.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/micro_interpreter_context.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/micro_interpreter_graph.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/micro_log.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/micro_op_resolver.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/micro_profiler.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/micro_resource_variable.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/micro_utils.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/mock_micro_graph.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/recording_micro_allocator.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/system_setup.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/test_helper_custom_ops.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/test_helpers.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/arena_allocator/non_persistent_arena_buffer_allocator.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/arena_allocator/persistent_arena_buffer_allocator.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/arena_allocator/recording_single_arena_buffer_allocator.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/arena_allocator/single_arena_buffer_allocator.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/memory_planner/greedy_memory_planner.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/memory_planner/linear_memory_planner.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/memory_planner/non_persistent_buffer_planner_shim.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/internal/common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/internal/portable_tensor_utils.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/internal/quantization_util.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/internal/runtime_shape.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/internal/tensor_ctypes.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/activations.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/activations_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/add.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/add_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/add_n.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/arg_min_max.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/assign_variable.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/basic_classifier.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/batch_matmul.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/batch_matmul_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/batch_to_space_nd.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/broadcast_args.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/broadcast_to.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/call_once.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cast.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/ceil.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/circular_buffer.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/circular_buffer_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/comparisons.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/concatenation.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/conv.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/conv_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cumsum.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/decode.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/decode_state.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/decode_state_huffman.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/decode_state_lut.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/decode_state_prune.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/decompress.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/decompress_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/depth_to_space.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/depthwise_conv.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/depthwise_conv_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/dequantize.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/dequantize_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/detection_postprocess.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/div.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/dynamic_update_slice.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/elementwise.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/elu.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/embedding_lookup.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/ethosu.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/exp.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/expand_dims.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/fill.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/floor.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/floor_div.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/floor_mod.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/fully_connected.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/fully_connected_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/gather.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/gather_nd.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/hard_swish.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/hard_swish_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/if.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/kernel_runner.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/kernel_util.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/l2norm.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/l2_pool_2d.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/leaky_relu.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/leaky_relu_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/logical.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/logical_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/logistic.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/logistic_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/log_softmax.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/lstm_eval.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/lstm_eval_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/maximum_minimum.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/mirror_pad.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/mul.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/mul_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/neg.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/pack.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/pad.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/pad_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/pooling.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/pooling_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/prelu.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/prelu_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/quantize.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/quantize_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/read_variable.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/reduce.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/reduce_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/reshape.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/reshape_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/resize_bilinear.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/resize_nearest_neighbor.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/reverse.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/round.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/select.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/shape.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/slice.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/softmax.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/softmax_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/space_to_batch_nd.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/space_to_depth.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/split.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/split_v.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/squared_difference.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/squeeze.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/strided_slice.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/strided_slice_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/sub.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/sub_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/svdf.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/svdf_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/tanh.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/transpose.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/transpose_common.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/transpose_conv.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/cmsis_nn/unidirectional_sequence_lstm.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/unpack.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/var_handle.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/while.cc
    ../Middlewares/Third_Party/tflite-micro/tensorflow/lite/micro/kernels/zeros_like.cc
    ../Middlewares/Third_Party/CMSIS-NN/Source/ActivationFunctions/arm_nn_activation_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ActivationFunctions/arm_relu6_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ActivationFunctions/arm_relu_q15.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ActivationFunctions/arm_relu_q7.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/BasicMathFunctions/arm_elementwise_add_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/BasicMathFunctions/arm_elementwise_add_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/BasicMathFunctions/arm_elementwise_mul_acc_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/BasicMathFunctions/arm_elementwise_mul_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/BasicMathFunctions/arm_elementwise_mul_s16_batch_offset.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/BasicMathFunctions/arm_elementwise_mul_s16_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/BasicMathFunctions/arm_elementwise_mul_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/BasicMathFunctions/arm_maximum_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/BasicMathFunctions/arm_minimum_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConcatenationFunctions/arm_concatenation_s8_w.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConcatenationFunctions/arm_concatenation_s8_x.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConcatenationFunctions/arm_concatenation_s8_y.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConcatenationFunctions/arm_concatenation_s8_z.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_1_x_n_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_1_x_n_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_1x1_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_1x1_s4_fast.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_1x1_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_1x1_s8_fast.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_even_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_get_buffer_sizes_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_get_buffer_sizes_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_get_buffer_sizes_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_wrapper_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_wrapper_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_wrapper_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_3x3_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_fast_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_get_buffer_sizes_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_get_buffer_sizes_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_get_buffer_sizes_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_s4_opt.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_s8_opt.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_wrapper_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_wrapper_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_wrapper_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_nn_depthwise_conv_s8_core.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_nn_mat_mult_kernel_row_offset_s8_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_nn_mat_mult_kernel_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_nn_mat_mult_kernel_s4_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_nn_mat_mult_kernel_s8_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_nn_mat_mult_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_transpose_conv_get_buffer_sizes_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_transpose_conv_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ConvolutionFunctions/arm_transpose_conv_wrapper_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/FullyConnectedFunctions/arm_batch_matmul_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/FullyConnectedFunctions/arm_batch_matmul_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/FullyConnectedFunctions/arm_fully_connected_get_buffer_sizes_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/FullyConnectedFunctions/arm_fully_connected_get_buffer_sizes_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/FullyConnectedFunctions/arm_fully_connected_per_channel_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/FullyConnectedFunctions/arm_fully_connected_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/FullyConnectedFunctions/arm_fully_connected_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/FullyConnectedFunctions/arm_fully_connected_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/FullyConnectedFunctions/arm_fully_connected_wrapper_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/FullyConnectedFunctions/arm_vector_sum_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/FullyConnectedFunctions/arm_vector_sum_s8_s64.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/LSTMFunctions/arm_lstm_unidirectional_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/LSTMFunctions/arm_lstm_unidirectional_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_depthwise_conv_nt_t_padded_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_depthwise_conv_nt_t_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_depthwise_conv_nt_t_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_depthwise_conv_nt_t_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_lstm_calculate_gate_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_lstm_calculate_gate_s8_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_lstm_step_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_lstm_step_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_mat_mul_core_1x_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_mat_mul_core_1x_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_mat_mul_core_4x_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_mat_mult_nt_interleaved_t_even_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_mat_mult_nt_t_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_mat_mult_nt_t_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_mat_mult_nt_t_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_mat_mult_nt_t_s8_s32.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_transpose_conv_row_s8_s32.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_vec_mat_mul_result_acc_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_vec_mat_mul_result_acc_s8_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_vec_mat_mult_t_per_ch_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_vec_mat_mult_t_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_vec_mat_mult_t_s16_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_vec_mat_mult_t_s4.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_vec_mat_mult_t_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nn_vec_mat_mult_t_svdf_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_nntables.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_q7_to_q15_with_offset.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/NNSupportFunctions/arm_s8_to_s16_unordered_with_offset.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/PadFunctions/arm_pad_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/PoolingFunctions/arm_avgpool_get_buffer_sizes_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/PoolingFunctions/arm_avgpool_get_buffer_sizes_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/PoolingFunctions/arm_avgpool_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/PoolingFunctions/arm_avgpool_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/PoolingFunctions/arm_max_pool_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/PoolingFunctions/arm_max_pool_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/ReshapeFunctions/arm_reshape_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/SVDFunctions/arm_svdf_get_buffer_sizes_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/SVDFunctions/arm_svdf_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/SVDFunctions/arm_svdf_state_s16_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/SoftmaxFunctions/arm_nn_softmax_common_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/SoftmaxFunctions/arm_softmax_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/SoftmaxFunctions/arm_softmax_s8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/SoftmaxFunctions/arm_softmax_s8_s16.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/SoftmaxFunctions/arm_softmax_u8.c
    ../Middlewares/Third_Party/CMSIS-NN/Source/TransposeFunctions/arm_transpose_s8.c
)

######################################
# 手写适配层（不在上面生成清单里）
######################################
# TFLM 把若干"平台钩子"设计成由目标自行提供（micro_time.h 的注释即此意）。
# 上游 cortex_m_generic 版用 DWT 循环计数器，要额外定义器件头宏且随开发板变化；
# 我们改接本工程的 bsp_dwt，于是把上游那份排除掉（见 tools/gen_sources.py 的
# DROP_BASENAMES），在这里补上自己的实现。
list(APPEND _lib_tflm_sources
    lib_tflm/lib_tflm_port.cc
)

list(APPEND MODULE_SOURCES ${_lib_tflm_sources})
