  betarobot 与 xrobot USB 协议栈差异（除已实现 6/7/8/9 与未实现的类之外）

  先说结论：EP0 状态机主体（分块收发、STATUS OUT gate、SET_ADDRESS 延迟、ZLP 补发）是逐行同构的忠实移植，机制无差异。真正的差异集中在下面几层。

  A. 影响主机枚举 / Windows 兼容性的（优先级最高）

  ┌────────────────────┬────────────────────────────────────────────────────────────┬────────────────────────────────────────────────┬─────────────────────────────────────────┐
  │        差异        │                           xrobot                           │                   betarobot                    │                  影响                   │
  ├────────────────────┼────────────────────────────────────────────────────────────┼────────────────────────────────────────────────┼─────────────────────────────────────────┤
  │ BOS 描述符         │ GET_DESCRIPTOR(0x0F) 返回 BOS，无 capability 时自动补 7B   │ 无                                             │ 无法支撑 WebUSB / MS OS 2.0             │
  │                    │ USB 2.0 Extension（bos.hpp:177）                           │                                                │                                         │
  ├────────────────────┼────────────────────────────────────────────────────────────┼────────────────────────────────────────────────┼─────────────────────────────────────────┤
  │ bcdUSB             │ 可到 0x0210（USB 2.1），STM32 默认                         │ 硬编码 0x0200（bsp_usb_desc_dev.c:22）         │ 主机只在 bcdUSB≥2.1 时才请求            │
  │                    │ 2.1（stm32_usb_dev.hpp:34）                                │                                                │ BOS/平台能力                            │
  ├────────────────────┼────────────────────────────────────────────────────────────┼────────────────────────────────────────────────┼─────────────────────────────────────────┤
  │                    │ Platform Capability（UUID D8DD60DF…）+ 0xEE 描述符 +       │                                                │ Windows 免驱动加载                      │
  │ WinUSB MS OS 2.0   │ vendor 请求 0x0007/0x0008 + WINUSB 兼容 ID                 │ 无                                             │ WinUSB（用户态驱动）的关键              │
  │                    │ 描述符集（winusb_msos20.hpp:314）                          │                                                │                                         │
  ├────────────────────┼────────────────────────────────────────────────────────────┼────────────────────────────────────────────────┼─────────────────────────────────────────┤
  │ WebUSB             │ Platform Capability + URL 描述符 + GET_URL vendor          │ 无                                             │ 浏览器直连设备                          │
  │                    │ 请求（webusb.hpp:164）                                     │                                                │                                         │
  ├────────────────────┼────────────────────────────────────────────────────────────┼────────────────────────────────────────────────┼─────────────────────────────────────────┤
  │ 接口字符串编码     │ 完整 UTF-8 1/2/3 字节解码转                                │ 仅 ASCII 低字节映射（bsp_usb_desc_str.c:45）   │ 非 ASCII 接口字符串在 betarobot 会编错  │
  │                    │ UTF-16LE（device_composition.cpp:97）                      │                                                │                                         │
  ├────────────────────┼────────────────────────────────────────────────────────────┼────────────────────────────────────────────────┼─────────────────────────────────────────┤
  │                    │ 返回 bmAttributes 的                                       │                                                │ Windows                                 │
  │ GET_STATUS(device) │ SELF_POWERED(0x01)/REMOTE_WAKEUP(0x02)                     │ 恒返回 0（bsp_usb_core.c:343）                 │ 枚举时看不到设备自供电/唤醒能力位       │
  │                    │ 位（dev_core.cpp:587）                                     │                                                │                                         │
  ├────────────────────┼────────────────────────────────────────────────────────────┼────────────────────────────────────────────────┼─────────────────────────────────────────┤
  │ 设备级 vendor 分发 │ 设备级 vendor 请求只走 BOS capability                      │ 相反：设备级 vendor 遍历类逐个试               │ 两条路互斥——betarobot 没有 BOS          │
  │                    │ 链，类不接收（dev_core.cpp:1043）                          │ on_vendor_request（bsp_usb_core.c:802）        │ 链，xrobot 类收不到设备级请求           │
  └────────────────────┴────────────────────────────────────────────────────────────┴────────────────────────────────────────────────┴─────────────────────────────────────────┘

  B. 电源 / 远程唤醒

  - Remote wakeup：xrobot 有 Enable/DisableRemoteWakeup 虚钩子槽位（dev_core.hpp:122，虽全库无实现、实际仍是仅置位），且 GET_STATUS 反映配置声明位；betarobot 连槽位都没有，纯
  ACK。
  - Suspend/Resume：xrobot Suspend→Deinit(true) 整栈下电、Resume→Init(true) 重建（stm32_usb_dev.cpp:69）；betarobot Suspend 仅 configured=0、Resume
  空实现（bsp_usb.c:267）。行为上两者都不发唤醒信号，但 xrobot 重建粒度更彻底。

  C. EP0 / 请求分发（类回调能力）

  - 类回调载荷 + in_isr 贯穿：xrobot OnClassData(in_isr, bRequest, data) 带数据阶段实际内容，全部类回调首参都是 bool in_isr（ISR 上下文感知，dev_core.cpp:264）；betarobot
  on_class_data(ctx, bRequest) 无载荷、全栈无 in_isr。
  - SET_DESCRIPTOR：xrobot 不 STALL 不 ACK（EP0 悬挂，dev_core.cpp:490）；betarobot 干净 STALL（bsp_usb_core.c:585）。betarobot 行为更规范。
  - GET_DESCRIPTOR(STRING) recipient：xrobot 任意 recipient 都回；betarobot 要求 DEVICE 否则 STALL（bsp_usb_core.c:477）。betarobot 更严，实际无影响。

  D. 端点 / 传输层

  - 无锁端点池：xrobot LockFreePool + atomic CAS，可在 ISR 中并发 Get/Release（ep_pool.cpp:5）；betarobot 静态位图 + 预分配数组（bsp_usb_ep.h:38），非 ISR 并发安全。
  - FindEndpoint：xrobot 按端点地址反查端点对象（标准请求 ENDPOINT recipient 用）；betarobot 直接按 epnum 索引，无此抽象。
  - multi-bulk 链式传输：xrobot 端点层 TransferMultiBulk 自动分包续传；betarobot 分包状态机手工搬在 CDC 层。
  - ISO 端点：xrobot 完整桥接（mult 高带宽、1023/1024 包长、ISOIncomplete 回调）；betarobot 只有枚举值，无实际支持。
  - DMA + DCache：xrobot 设 dma_addr + DCache clean/invalidate（stm32_usb_ep.cpp:216）；betarobot 无——若目标 MCU 使能 D-Cache（如 H7）会数据不一致。
  - SET_ADDRESS 生效时机：xrobot 按 OTG/FSDEV 控制器适配（stm32_usb_dev.cpp:173）；betarobot 单一路径。

  E. CDC 类层细节

  - UART 抽象：xrobot CDC 是 ReadPort/WritePort + UART 抽象，可经 CDCToUart 桥接真实串口做成搬运泵（cdc_to_uart.hpp:43）；betarobot 是纯数据环 + 背压。
  - RX 满时 pending 重注入：xrobot 队列满时挂起 pending、空间恢复后重注入（cdc_uart.hpp:639）；betarobot 是暂停 rearm 等应用取走——功能等价，恢复路径不同。
  - TX 双队列 + 预写流水：xrobot info/data 双队列、单 op 不跨界、多段预写、新数据到达即取消 pending ZLP（cdc_uart.hpp:694）；betarobot 单 op 环 + 简单状态机。ZLP 取消这个细节
  betarobot 没有（空 op 会先发 ZLP 再发新数据）。
  - SET_LINE_CODING 回调：xrobot 转成 UART::Configuration 再回调（含停止位/校验转换，1.5 位 TODO）；betarobot 有 line_coding_cb 但直接透传 USB_LineCoding_t，无 UART 配置语义。

  F. 类实现（你已知，仅列清单）

  HID（键盘/鼠标/手柄，六请求全集 + boot/report + LED OUT）、DFU（完整状态机 + Flash 后端 seal + runtime）、GSUSB（CAN 转 USB）、UAC（等时麦克风 + alt
  切换流开关）、DAP（DAPLink）。

  ---
  建议的补全优先级

  1. WinUSB MS OS 2.0（A 组）——唯一能改变 Windows 设备管理器行为（免驱动）的机制，若做自定义 HID/DFU 或希望用户态应用访问，价值最高。依赖 BOS + bcdUSB 2.1。
  2. UTF-8 接口字符串（A 组）——改动最小（bsp_usb_desc_str.c 一个函数），纯正确性。
  3. GET_STATUS 唤醒位 + Suspend/Resume 重建（A/B 组）——枚举信息正确性 + 低功耗恢复健壮性。
  4. DCache/DMA（D 组）——若目标板是 H7 且可能使能 D-Cache，属于隐性坑。
  5. in_isr、无锁池、multi-bulk、ISO、多平台——属架构扩展，无当前需求则不必动。

  需要我把其中某几项（比如 UTF-8 接口字符串、GET_STATUS 唤醒位）按前面的方式直接实现到 betarobot 吗？