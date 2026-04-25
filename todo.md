1. freertos考虑使用mpu
2. uart和spi超时时间
3. 协议bsp。eg:uart，spi
4. 所有原始数据都需要时间戳（sbus通过dma接收到的原始25字节，bmi通过dma接收到的14字节），sbus，bmi
5. 中断和dma优先级
6. 给所有bsp的中断回调函数添加时间戳，回调函数两个变量，实例，时间戳（不需要，drv层使用时自己写读取时间戳）
