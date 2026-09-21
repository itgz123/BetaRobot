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
当前drv很多不符合这个架构，慢慢修改  
先用drvs前缀，避免重名，等app全部依赖drvs/drvlib之后，删除原来的drv，改drvs为drv

---

### 当前drv耦合模块之后分层

#### drvlib

motor(别的用途可以用一下)

#### drv

motor

#### lib

|           |                                                        |
| --------- | ------------------------------------------------------ |
| axis      | 离心力，科里奥利力                                     |
| axis_lite | 摩擦，重力，惯量                                       |
| gimbal    | 综合axis                                               |
| shoot     | 热量计算，射速，射频，子弹数量剩余，异常处理（退弹等） |
| 里程计    |                                                        |
| mit       |                                                        |
| pid       |                                                        |
| planner   |                                                        |
| dh_matrix |                                                        |

底盘：运动学正逆解，功率控制有无，类型，位控/力控

```
drv_chassis/drv_chassis_<position/force>_<type> // 有功率控制
drv_chassis_lite/drv_chassis_lite_<position/force>_<type> // 无功率控制
```
