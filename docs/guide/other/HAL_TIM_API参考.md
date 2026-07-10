---
---

# STM32F4xx HAL TIM API 参考

## 一、Start/Stop 函数分类总览

### 1.1 按操作对象分类

| 操作对象           | 函数前缀                    | 说明                         |
| ------------------ | --------------------------- | ---------------------------- |
| **TIM 计数器**     | `HAL_TIM_Base_Start/Stop`   | 启动/停止整个定时器计数      |
| **Channel**        | `HAL_TIM_xxx_Start/Stop`    | 启动/停止指定通道的输出/捕获 |
| **互补 Channel N** | `HAL_TIMEx_xxxN_Start/Stop` | 启动/停止互补通道            |

### 1.2 按工作模式分类

| 后缀   | 模式              | 说明                  |
| ------ | ----------------- | --------------------- |
| 无后缀 | Polling（轮询）   | 阻塞模式，直接操作    |
| `_IT`  | Interrupt（中断） | 非阻塞，使能中断      |
| `_DMA` | DMA               | 非阻塞，使用 DMA 传输 |

---

## 二、Time Base（基础定时器）

### 2.1 函数列表

| 函数                                          | 操作对象  | 模式      | 说明                           |
| --------------------------------------------- | --------- | --------- | ------------------------------ |
| `HAL_TIM_Base_Start(htim)`                    | TIM       | Polling   | 启动定时器计数                 |
| `HAL_TIM_Base_Stop(htim)`                     | TIM       | Polling   | 停止定时器计数                 |
| `HAL_TIM_Base_Start_IT(htim)`                 | TIM       | Interrupt | 启动定时器（更新中断）         |
| `HAL_TIM_Base_Stop_IT(htim)`                  | TIM       | Interrupt | 停止定时器（禁用更新中断）     |
| `HAL_TIM_Base_Start_DMA(htim, pData, Length)` | TIM + DMA | DMA       | 启动定时器，DMA 传输数据到 ARR |
| `HAL_TIM_Base_Stop_DMA(htim)`                 | TIM + DMA | DMA       | 停止定时器 DMA                 |

### 2.2 使用场景

- 产生周期性更新中断
- 作为其他外设的时基（如 FreeRTOS 的 tick）
- DMA 方式动态修改 ARR 实现变频

---

## 三、Output Compare（输出比较）

### 3.1 函数列表

| 函数                                                 | 操作对象      | 模式      | 说明                      |
| ---------------------------------------------------- | ------------- | --------- | ------------------------- |
| `HAL_TIM_OC_Start(htim, Channel)`                    | Channel       | Polling   | 启动通道输出比较          |
| `HAL_TIM_OC_Stop(htim, Channel)`                     | Channel       | Polling   | 停止通道输出比较          |
| `HAL_TIM_OC_Start_IT(htim, Channel)`                 | Channel       | Interrupt | 启动通道（比较中断）      |
| `HAL_TIM_OC_Stop_IT(htim, Channel)`                  | Channel       | Interrupt | 停止通道（禁用比较中断）  |
| `HAL_TIM_OC_Start_DMA(htim, Channel, pData, Length)` | Channel + DMA | DMA       | 启动通道，DMA 传输 CCR 值 |
| `HAL_TIM_OC_Stop_DMA(htim, Channel)`                 | Channel + DMA | DMA       | 停止通道 DMA              |

### 3.2 互补通道（TIMEx）

| 函数                                                         | 操作对象        | 模式      |
| ------------------------------------------------------------ | --------------- | --------- |
| `HAL_TIMEx_OCN_Start/Stop(htim, Channel)`                    | Channel N       | Polling   |
| `HAL_TIMEx_OCN_Start/Stop_IT(htim, Channel)`                 | Channel N       | Interrupt |
| `HAL_TIMEx_OCN_Start/Stop_DMA(htim, Channel, pData, Length)` | Channel N + DMA | DMA       |

### 3.3 使用场景

- 产生精确的时间延迟
- 输出特定波形
- DMA 方式动态修改占空比

---

## 四、PWM（脉宽调制）

### 4.1 函数列表

| 函数                                                  | 操作对象      | 模式      | 说明                         |
| ----------------------------------------------------- | ------------- | --------- | ---------------------------- |
| `HAL_TIM_PWM_Start(htim, Channel)`                    | Channel       | Polling   | 启动通道 PWM 输出            |
| `HAL_TIM_PWM_Stop(htim, Channel)`                     | Channel       | Polling   | 停止通道 PWM 输出            |
| `HAL_TIM_PWM_Start_IT(htim, Channel)`                 | Channel       | Interrupt | 启动通道（PWM 周期完成中断） |
| `HAL_TIM_PWM_Stop_IT(htim, Channel)`                  | Channel       | Interrupt | 停止通道（禁用中断）         |
| `HAL_TIM_PWM_Start_DMA(htim, Channel, pData, Length)` | Channel + DMA | DMA       | 启动通道，DMA 传输 CCR 值    |
| `HAL_TIM_PWM_Stop_DMA(htim, Channel)`                 | Channel + DMA | DMA       | 停止通道 DMA                 |

### 4.2 互补通道（TIMEx）

| 函数                                                          | 操作对象        | 模式      |
| ------------------------------------------------------------- | --------------- | --------- |
| `HAL_TIMEx_PWMN_Start/Stop(htim, Channel)`                    | Channel N       | Polling   |
| `HAL_TIMEx_PWMN_Start/Stop_IT(htim, Channel)`                 | Channel N       | Interrupt |
| `HAL_TIMEx_PWMN_Start/Stop_DMA(htim, Channel, pData, Length)` | Channel N + DMA | DMA       |

### 4.3 使用场景

- 电机驱动（需要互补通道 + 死区）
- LED 调光
- 伺服控制

---

## 五、Input Capture（输入捕获）

### 5.1 函数列表

| 函数                                                 | 操作对象      | 模式      | 说明                     |
| ---------------------------------------------------- | ------------- | --------- | ------------------------ |
| `HAL_TIM_IC_Start(htim, Channel)`                    | Channel       | Polling   | 启动通道输入捕获         |
| `HAL_TIM_IC_Stop(htim, Channel)`                     | Channel       | Polling   | 停止通道输入捕获         |
| `HAL_TIM_IC_Start_IT(htim, Channel)`                 | Channel       | Interrupt | 启动通道（捕获中断）     |
| `HAL_TIM_IC_Stop_IT(htim, Channel)`                  | Channel       | Interrupt | 停止通道（禁用捕获中断） |
| `HAL_TIM_IC_Start_DMA(htim, Channel, pData, Length)` | Channel + DMA | DMA       | 启动通道，DMA 存储捕获值 |
| `HAL_TIM_IC_Stop_DMA(htim, Channel)`                 | Channel + DMA | DMA       | 停止通道 DMA             |

### 5.2 使用场景

- 测量脉冲宽度
- 测量频率
- 编码器信号解码（配合 Encoder 模式）

---

## 六、One Pulse（单脉冲）

### 6.1 函数列表

| 函数                                             | 操作对象 | 模式      | 说明                   |
| ------------------------------------------------ | -------- | --------- | ---------------------- |
| `HAL_TIM_OnePulse_Start(htim, OutputChannel)`    | Channel  | Polling   | 启动单脉冲输出         |
| `HAL_TIM_OnePulse_Stop(htim, OutputChannel)`     | Channel  | Polling   | 停止单脉冲输出         |
| `HAL_TIM_OnePulse_Start_IT(htim, OutputChannel)` | Channel  | Interrupt | 启动单脉冲（中断）     |
| `HAL_TIM_OnePulse_Stop_IT(htim, OutputChannel)`  | Channel  | Interrupt | 停止单脉冲（禁用中断） |

> **注意**：One Pulse 模式**无 DMA 版本**

### 6.2 互补通道（TIMEx）

| 函数                                                     | 操作对象  | 模式      |
| -------------------------------------------------------- | --------- | --------- |
| `HAL_TIMEx_OnePulseN_Start/Stop(htim, OutputChannel)`    | Channel N | Polling   |
| `HAL_TIMEx_OnePulseN_Start/Stop_IT(htim, OutputChannel)` | Channel N | Interrupt |

### 6.3 使用场景

- 产生固定宽度的单次脉冲
- 触发外部设备

---

## 七、Encoder（编码器接口）

### 7.1 函数列表

| 函数                                                               | 操作对象  | 模式      | 说明                               |
| ------------------------------------------------------------------ | --------- | --------- | ---------------------------------- |
| `HAL_TIM_Encoder_Start(htim, Channel)`                             | TIM       | Polling   | 启动编码器模式                     |
| `HAL_TIM_Encoder_Stop(htim, Channel)`                              | TIM       | Polling   | 停止编码器模式                     |
| `HAL_TIM_Encoder_Start_IT(htim, Channel)`                          | TIM       | Interrupt | 启动编码器（中断）                 |
| `HAL_TIM_Encoder_Stop_IT(htim, Channel)`                           | TIM       | Interrupt | 停止编码器（禁用中断）             |
| `HAL_TIM_Encoder_Start_DMA(htim, Channel, pData1, pData2, Length)` | TIM + DMA | DMA       | 启动编码器，DMA 存储两个通道捕获值 |
| `HAL_TIM_Encoder_Stop_DMA(htim, Channel)`                          | TIM + DMA | DMA       | 停止编码器 DMA                     |

### 7.2 使用场景

- 读取旋转编码器
- 测量电机转速和方向

---

## 八、Hall Sensor（霍尔传感器，TIMEx）

### 8.1 函数列表

| 函数                                                  | 操作对象  | 模式      | 说明                           |
| ----------------------------------------------------- | --------- | --------- | ------------------------------ |
| `HAL_TIMEx_HallSensor_Start(htim)`                    | TIM       | Polling   | 启动霍尔传感器模式             |
| `HAL_TIMEx_HallSensor_Stop(htim)`                     | TIM       | Polling   | 停止霍尔传感器模式             |
| `HAL_TIMEx_HallSensor_Start_IT(htim)`                 | TIM       | Interrupt | 启动霍尔传感器（中断）         |
| `HAL_TIMEx_HallSensor_Stop_IT(htim)`                  | TIM       | Interrupt | 停止霍尔传感器（禁用中断）     |
| `HAL_TIMEx_HallSensor_Start_DMA(htim, pData, Length)` | TIM + DMA | DMA       | 启动霍尔传感器，DMA 存储捕获值 |
| `HAL_TIMEx_HallSensor_Stop_DMA(htim)`                 | TIM + DMA | DMA       | 停止霍尔传感器 DMA             |

### 8.2 使用场景

- 无刷直流电机换相

---

## 九、DMA Burst（DMA 突发传输）

### 9.1 函数列表

| 函数                                                                                             | 说明              |
| ------------------------------------------------------------------------------------------------ | ----------------- |
| `HAL_TIM_DMABurst_WriteStart(htim, BurstBaseAddress, BurstRequestSrc, BurstBuffer, BurstLength)` | 启动 DMA 突发写入 |
| `HAL_TIM_DMABurst_WriteStop(htim, BurstRequestSrc)`                                              | 停止 DMA 突发写入 |
| `HAL_TIM_DMABurst_ReadStart(htim, BurstBaseAddress, BurstRequestSrc, BurstBuffer, BurstLength)`  | 启动 DMA 突发读取 |
| `HAL_TIM_DMABurst_ReadStop(htim, BurstRequestSrc)`                                               | 停止 DMA 突发读取 |

### 9.2 使用场景

- 批量修改多个 TIM 寄存器
- 高效更新 PWM 参数

---

## 十、Start/Stop 函数对照表

### 10.1 Base 模式（启动 TIM 计数器）

| 函数                     | 启动对象              | 是否启动 DMA |
| ------------------------ | --------------------- | ------------ |
| `HAL_TIM_Base_Start`     | TIM 计数器            | ❌           |
| `HAL_TIM_Base_Start_IT`  | TIM 计数器 + 更新中断 | ❌           |
| `HAL_TIM_Base_Start_DMA` | TIM 计数器 + DMA      | ✅           |

### 10.2 Channel 模式（启动指定通道）

| 函数                    | 启动对象          | 是否启动 DMA |
| ----------------------- | ----------------- | ------------ |
| `HAL_TIM_xxx_Start`     | Channel 输出/捕获 | ❌           |
| `HAL_TIM_xxx_Start_IT`  | Channel + 中断    | ❌           |
| `HAL_TIM_xxx_Start_DMA` | Channel + DMA     | ✅           |

> `xxx` 可以是：`OC`、`PWM`、`IC`、`Encoder`

### 10.3 互补 Channel N 模式

| 函数                       | 启动对象         | 是否启动 DMA |
| -------------------------- | ---------------- | ------------ |
| `HAL_TIMEx_xxxN_Start`     | Channel N 输出   | ❌           |
| `HAL_TIMEx_xxxN_Start_IT`  | Channel N + 中断 | ❌           |
| `HAL_TIMEx_xxxN_Start_DMA` | Channel N + DMA  | ✅           |

> `xxx` 可以是：`OC`、`PWM`、`OnePulse`

---

## 十一、常用宏定义

### 11.1 运行时修改

```c
// 设置计数器值
__HAL_TIM_SET_COUNTER(htim, value);

// 获取计数器值
uint32_t count = __HAL_TIM_GET_COUNTER(htim);

// 设置自动重装载值
__HAL_TIM_SET_AUTORELOAD(htim, value);

// 获取自动重装载值
uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);

// 设置比较值（PWM 占空比）
__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, value);

// 获取比较值
uint32_t ccr = __HAL_TIM_GET_COMPARE(htim, TIM_CHANNEL_1);
```

### 11.2 中断和标志

```c
// 使能中断
__HAL_TIM_ENABLE_IT(htim, TIM_IT_UPDATE);

// 禁用中断
__HAL_TIM_DISABLE_IT(htim, TIM_IT_UPDATE);

// 获取中断标志
if (__HAL_TIM_GET_FLAG(htim, TIM_FLAG_UPDATE)) {
    // 清除标志
    __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);
}
```

---

## 十二、BSP_TIM 设计建议

### 12.1 推荐封装模式

根据 BSP 开发指南，建议按以下模式封装：

1. **只封装常用的模式**：PWM、Base、Encoder
2. **使用回调机制**：中断回调分发到 APP 层
3. **实例注册**：通过枚举索引管理多个 TIM 实例

### 12.2 典型使用流程

```c
// PWM 输出
HAL_TIM_PWM_Start(&htim, TIM_CHANNEL_1);           // 启动通道
__HAL_TIM_SET_COMPARE(&htim, TIM_CHANNEL_1, 500);  // 设置占空比

// 定时器中断
HAL_TIM_Base_Start_IT(&htim);  // 启动定时器 + 更新中断
// 在 HAL_TIM_PeriodElapsedCallback 中处理

// 编码器
HAL_TIM_Encoder_Start(&htim, TIM_CHANNEL_ALL);     // 启动编码器
int32_t count = __HAL_TIM_GET_COUNTER(&htim);      // 读取计数值
```
