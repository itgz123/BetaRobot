# BetaRobot

### 特点：

1. 多开发版
2. 多app，可以保证底层更新时所有使用这个框架的人可以直接更新
3. 静态内存分配
4. 面对对象尤其motor
5. 分层架构
6. 底层展示，虽然多app，bsp自动选择，但是底层实现所有人都看得到能够理解，debug（esp的调试是反例）

### 使用方法（架构说明）：

bsp＝Windows系统。hal＝硬件驱动。app＝软件。drv＝系统自带软件

### 文档：

##### todo

1. 根目录makefile把`-I$(HAL_DIR)/`和`$(HAL_DIR)/`放到bsp_math的makefile中。
2. 把cubemx生成的core在Makefile排除，复制到app中,.mxproject,ioc???。。添加app/<app_name>/core文件夹，makefile同步
3. core使用子模块？加入app？
