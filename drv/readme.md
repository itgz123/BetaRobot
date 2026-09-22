## 分层架构

**当前主要用stm32，所以bsp和hal写得不是很好**

### bsp

把stm32或其他芯片封装为统一接口。之后得改为每种芯片一个文件。

### hal

直接调用stm32hal库或ll库解初始化/初始化，修改一些配置

### drv

实现常见其他传感器，电机，遥控等的协议

### lib

提供一些独立的计算库，不依赖于bsp等

### drvlib

联合drv和lib，让rm情况下更方便

### app

每个人自己写的应用代码

---

以上复制来自docs  
当前drv很多不符合这个架构，慢慢修改（drv_axis_lite,drv_motor）  
先用drvs前缀，避免重名，等app全部依赖drvs/drvlib之后，删除原来的drv，改drvs为drv

---

## 当前drv耦合模块之后分层

### drv

motor

### lib

##### 1. lib_mit

##### 2. lib_pid

##### 3. lib_planner

##### 4. lib_dh_matrix

运动学正逆解

##### 5. lib_chassis

运动学正逆解

### drvlib/appdrvlib

##### 1. drvlib_axis

**输入位置（速度，加速度），输出力矩**

1. 反馈：pid/mit
2. 前馈：摩擦，重力，惯量（离心力，科里奥利力）
3. 命名：

```
drvlib_axis_[pid/mit]_[lite/无]
```

##### 2. drvlib_gimbal

综合axis，使用dh_matrix简化运动学计算

##### 3. drvlib_shoot

功能：热量计算，射速，射频，子弹数量剩余，异常处理（退弹等）

##### 4. drvlib_chassis

**输入vx,vy,w，输出所有电机速度（舵向位置）**
**输入vx,vy,w，输出所有电机力矩**
**输入2个通道值，输出所有电机力矩**

功能：功率控制，pid/mit，pid位控/pid+力控前馈（摩擦，惯量，云台外力），里程计，姿态闭环，摩擦检测

##### 5. drvlib_motor

支持3个环pid(别的用途可以用一下)
