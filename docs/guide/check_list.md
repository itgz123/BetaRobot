---
---

# 代码要求检查清单（check_list）

## 编码要求

1. 读取直接读结构体，写参数必须用函数
2. 配置宏、参数宏、共用结构体、枚举的放置位置规范
3. 检查所有回调函数签名
4. DRV/BSP 所有功能都要可以通过宏开启或关闭，减少 Flash/RAM 占用
5. DRV/BSP 所有实例用 `*_INSTANCE_DEF` 宏来定义
6. `*_INSTANCE_DEF` 内部自带 `static`；协议缓冲区加 `const`；要用 DMA 加 `DMA_RAM`
7. 实例结构体 set 封装函数，get 直接通过结构体获取
8. 注册、初始化函数需要检查 cfg 结构体参数
9. ITCM 放置：FreeRTOS 内核代码；创建的 FreeRTOS 任务代码；Kalman 等高算力代码；中断代码
10. 检查 `const`、`extern`、`register`、`static`、`volatile`
11. 函数签名：BSP 用 register，DRV 用 init
12. 检查每个 DRV/BSP 的所有 static 函数，不要每个 DRV 自己定义一个 clamp 限幅函数
