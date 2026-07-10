import{_ as a,o as n,c as t,a2 as p}from"./chunks/framework.BKchhTkY.js";const k=JSON.parse('{"title":"项目架构设计方案","description":"","frontmatter":{"outline":"deep"},"headers":[],"relativePath":"guide/项目架构设计方案.md","filePath":"guide/项目架构设计方案.md","lastUpdated":null}'),i={name:"guide/项目架构设计方案.md"};function e(l,s,d,h,r,c){return n(),t("div",null,[...s[0]||(s[0]=[p(`<h1 id="项目架构设计方案" tabindex="-1">项目架构设计方案 <a class="header-anchor" href="#项目架构设计方案" aria-label="Permalink to &quot;项目架构设计方案&quot;">​</a></h1><p>本文档描述 test_my_frame 项目的分层架构设计原则、数据流机制和封装规范，适用于跨层设计参考。</p><hr><h2 id="一、整体架构" tabindex="-1">一、整体架构 <a class="header-anchor" href="#一、整体架构" aria-label="Permalink to &quot;一、整体架构&quot;">​</a></h2><h3 id="_1-1-分层结构" tabindex="-1">1.1 分层结构 <a class="header-anchor" href="#_1-1-分层结构" aria-label="Permalink to &quot;1.1 分层结构&quot;">​</a></h3><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>┌─────────────────────────────────────────────────────────────┐</span></span>
<span class="line"><span>│                     APP 层（应用层）                         │</span></span>
<span class="line"><span>│  - 业务逻辑                                                  │</span></span>
<span class="line"><span>│  - 任务调度                                                  │</span></span>
<span class="line"><span>│  - 消息处理                                                  │</span></span>
<span class="line"><span>│  - FreeRTOS 对象管理（队列、互斥量、信号量）                 │</span></span>
<span class="line"><span>└─────────────────────────────────────────────────────────────┘</span></span>
<span class="line"><span>                          ↓ 调用 DRV 接口</span></span>
<span class="line"><span>┌─────────────────────────────────────────────────────────────┐</span></span>
<span class="line"><span>│                     DRV 层（驱动层）                         │</span></span>
<span class="line"><span>│  - 外部模块驱动（电机、IMU、遥控器等）                       │</span></span>
<span class="line"><span>│  - 协议解析                                                  │</span></span>
<span class="line"><span>│  - 数据处理                                                  │</span></span>
<span class="line"><span>│  - 回调传递                                                  │</span></span>
<span class="line"><span>└─────────────────────────────────────────────────────────────┘</span></span>
<span class="line"><span>                          ↓ 调用 BSP 接口</span></span>
<span class="line"><span>┌─────────────────────────────────────────────────────────────┐</span></span>
<span class="line"><span>│                     BSP 层（板级支持层）                     │</span></span>
<span class="line"><span>│  - MCU 外设封装（CAN、UART、SPI、GPIO 等）                   │</span></span>
<span class="line"><span>│  - 中断回调分发                                              │</span></span>
<span class="line"><span>│  - 实例管理                                                  │</span></span>
<span class="line"><span>└─────────────────────────────────────────────────────────────┘</span></span>
<span class="line"><span>                          ↓ 调用 HAL 库</span></span>
<span class="line"><span>┌─────────────────────────────────────────────────────────────┐</span></span>
<span class="line"><span>│              HAL 层 + SEGGER RTT + FreeRTOS                  │</span></span>
<span class="line"><span>└─────────────────────────────────────────────────────────────┘</span></span></code></pre></div><h3 id="_1-2-层级调用原则" tabindex="-1">1.2 层级调用原则 <a class="header-anchor" href="#_1-2-层级调用原则" aria-label="Permalink to &quot;1.2 层级调用原则&quot;">​</a></h3><table tabindex="0"><thead><tr><th>规则</th><th>说明</th></tr></thead><tbody><tr><td><strong>单向依赖</strong></td><td>上层调用下层，下层不能调用上层</td></tr><tr><td><strong>禁止跨层</strong></td><td>APP 不能直接调用 HAL，必须通过 DRV → BSP</td></tr><tr><td><strong>接口隔离</strong></td><td>每层只暴露必要的接口，隐藏实现细节</td></tr><tr><td><strong>配置向下</strong></td><td>上层配置驱动下层，下层不依赖上层配置文件</td></tr></tbody></table><h3 id="_1-3-各层职责" tabindex="-1">1.3 各层职责 <a class="header-anchor" href="#_1-3-各层职责" aria-label="Permalink to &quot;1.3 各层职责&quot;">​</a></h3><table tabindex="0"><thead><tr><th>层级</th><th>职责</th><th>关键点</th></tr></thead><tbody><tr><td>APP</td><td>业务逻辑、任务调度</td><td>调用 DRV 接口，管理 RTOS 对象，使用DEF宏定义实例</td></tr><tr><td>DRV</td><td>模块驱动、协议解析</td><td>封装外部模块，提供解析函数，值拷贝BSP实例</td></tr><tr><td>BSP</td><td>外设封装、中断分发</td><td>定义DEF宏，管理实例数组，提供板级映射</td></tr><tr><td>HAL</td><td>硬件抽象层</td><td>包含多开发板配置，CubeMX 生成的基础代码</td></tr></tbody></table><hr><h2 id="二、信息流设计" tabindex="-1">二、信息流设计 <a class="header-anchor" href="#二、信息流设计" aria-label="Permalink to &quot;二、信息流设计&quot;">​</a></h2><h3 id="_2-1-中断数据流" tabindex="-1">2.1 中断数据流 <a class="header-anchor" href="#_2-1-中断数据流" aria-label="Permalink to &quot;2.1 中断数据流&quot;">​</a></h3><p>中断发生时的数据流向（必须先注册，才能回调）：</p><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>[MPU] interrupt</span></span>
<span class="line"><span>   │</span></span>
<span class="line"><span>   ↓ HAL 回调函数</span></span>
<span class="line"><span>   │</span></span>
<span class="line"><span>[BSP] hal_callback</span></span>
<span class="line"><span>   │    └─ 拷贝原始数据，查找实例，调用其回调</span></span>
<span class="line"><span>   ↓</span></span>
<span class="line"><span>[DRV] bsp_callback</span></span>
<span class="line"><span>   │    └─ 继续回调到 APP 层</span></span>
<span class="line"><span>   ↓</span></span>
<span class="line"><span>[APP] drv_callback</span></span>
<span class="line"><span>   │    └─ 拷贝原始消息，塞入队列（不做解析！）</span></span>
<span class="line"><span>   ↓</span></span>
<span class="line"><span>(interrupt finish) 中断结束</span></span></code></pre></div><p><strong>重要原则：中断中不解析数据，只拷贝和传递</strong></p><h3 id="_2-2-任务处理流" tabindex="-1">2.2 任务处理流 <a class="header-anchor" href="#_2-2-任务处理流" aria-label="Permalink to &quot;2.2 任务处理流&quot;">​</a></h3><p>消息处理线程中的数据流向：</p><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>[APP] 从队列获取原始消息</span></span>
<span class="line"><span>   │</span></span>
<span class="line"><span>   ↓ 解析数据（协议解析在此进行）</span></span>
<span class="line"><span>   │</span></span>
<span class="line"><span>[APP] 更新模块状态，执行业务逻辑</span></span>
<span class="line"><span>   │</span></span>
<span class="line"><span>   ↓ 调用 DRV 层接口</span></span>
<span class="line"><span>   │</span></span>
<span class="line"><span>[DRV] 调用 BSP 层接口</span></span>
<span class="line"><span>   │</span></span>
<span class="line"><span>   ↓</span></span>
<span class="line"><span>[BSP] 调用 HAL 库操作硬件</span></span></code></pre></div><hr><h2 id="三、面向对象封装原则" tabindex="-1">三、面向对象封装原则 <a class="header-anchor" href="#三、面向对象封装原则" aria-label="Permalink to &quot;三、面向对象封装原则&quot;">​</a></h2><h3 id="_3-1-核心原则" tabindex="-1">3.1 核心原则 <a class="header-anchor" href="#_3-1-核心原则" aria-label="Permalink to &quot;3.1 核心原则&quot;">​</a></h3><table tabindex="0"><thead><tr><th>原则</th><th>实现方式</th><th>目的</th></tr></thead><tbody><tr><td><strong>接口暴露最小化</strong></td><td>只在 .h 中声明必要的公开函数</td><td>隐藏实现细节</td></tr><tr><td><strong>禁止全局变量</strong></td><td>使用 static 限定文件作用域</td><td>减少耦合</td></tr><tr><td><strong>线程安全通信</strong></td><td>使用队列、信号量、互斥量</td><td>避免竞态条件</td></tr></tbody></table><h3 id="_3-2-文件组织规范" tabindex="-1">3.2 文件组织规范 <a class="header-anchor" href="#_3-2-文件组织规范" aria-label="Permalink to &quot;3.2 文件组织规范&quot;">​</a></h3><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>模块目录/</span></span>
<span class="line"><span>├── module.h          # 公开接口声明 + 结构体定义</span></span>
<span class="line"><span>├── module.c          # 实现 + 静态函数</span></span>
<span class="line"><span>└── module_def.h      # 类型定义（可选，供内部使用）</span></span></code></pre></div><hr><h2 id="四、实例注册机制" tabindex="-1">四、实例注册机制 <a class="header-anchor" href="#四、实例注册机制" aria-label="Permalink to &quot;四、实例注册机制&quot;">​</a></h2><p>采用&quot;DEF宏定义实例 + 注册函数&quot;模式，实现静态分配：</p><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// APP层：使用DEF宏定义实例</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">DEF_MOTOR</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(motor1, CAN_ID_0x201);</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 注册到BSP层</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">MotorRegister</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">&amp;</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;">motor1</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 直接访问成员</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">motor1.ref </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 100.0</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">f</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">float</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> speed </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> motor1.speed;</span></span></code></pre></div><hr><h2 id="五、配置传递机制" tabindex="-1">五、配置传递机制 <a class="header-anchor" href="#五、配置传递机制" aria-label="Permalink to &quot;五、配置传递机制&quot;">​</a></h2><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>┌─────────────────────────────────────────────────────────────┐</span></span>
<span class="line"><span>│                     APP 层                                   │</span></span>
<span class="line"><span>│  - app_cfg.h 定义开发板选择和应用参数                        │</span></span>
<span class="line"><span>│  - 通过初始化参数传递配置到下层                              │</span></span>
<span class="line"><span>└─────────────────────────────────────────────────────────────┘</span></span>
<span class="line"><span>                          ↓ 包含 app_cfg.h</span></span>
<span class="line"><span>┌─────────────────────────────────────────────────────────────┐</span></span>
<span class="line"><span>│                     BSP 层                                   │</span></span>
<span class="line"><span>│  - bsp_cfg.h 根据 DEVELOPMENT_BOARD 自动适配                 │</span></span>
<span class="line"><span>│  - 定义板载资源枚举、硬件映射、实例数量                       │</span></span>
<span class="line"><span>└─────────────────────────────────────────────────────────────┘</span></span>
<span class="line"><span>                          ↓ 无依赖</span></span>
<span class="line"><span>┌─────────────────────────────────────────────────────────────┐</span></span>
<span class="line"><span>│                     DRV 层                                   │</span></span>
<span class="line"><span>│  - drv_types.h 定义跨层通用类型                              │</span></span>
<span class="line"><span>│  - 不涉及硬件配置                                            │</span></span>
<span class="line"><span>└─────────────────────────────────────────────────────────────┘</span></span></code></pre></div><hr><h2 id="六、rtos-接口规范" tabindex="-1">六、RTOS 接口规范 <a class="header-anchor" href="#六、rtos-接口规范" aria-label="Permalink to &quot;六、RTOS 接口规范&quot;">​</a></h2><p><strong>APP 层统一使用 FreeRTOS 原生 API，不使用 CMSIS-RTOS 封装层</strong></p><table tabindex="0"><thead><tr><th>功能</th><th>FreeRTOS 原生 API</th></tr></thead><tbody><tr><td>创建任务（静态）</td><td><code>xTaskCreateStatic()</code></td></tr><tr><td>删除任务</td><td><code>vTaskDelete()</code></td></tr><tr><td>延时</td><td><code>vTaskDelay(pdMS_TO_TICKS(ms))</code></td></tr><tr><td>创建队列（静态）</td><td><code>xQueueCreateStatic()</code></td></tr><tr><td>发送队列</td><td><code>xQueueSend()</code></td></tr><tr><td>接收队列</td><td><code>xQueueReceive()</code></td></tr><tr><td>创建互斥量（静态）</td><td><code>xSemaphoreCreateMutexStatic()</code></td></tr><tr><td>获取互斥量</td><td><code>xSemaphoreTake()</code></td></tr><tr><td>释放互斥量</td><td><code>xSemaphoreGive()</code></td></tr></tbody></table><hr><h2 id="七、def宏定义规范" tabindex="-1">七、DEF宏定义规范 <a class="header-anchor" href="#七、def宏定义规范" aria-label="Permalink to &quot;七、DEF宏定义规范&quot;">​</a></h2><p><strong>DEF宏定义在底层（BSP层），APP层调用宏创建实例</strong></p><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// BSP层定义宏</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#define</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> DEF_GPIO</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;">name</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">, </span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;">port</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">, </span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;">pin</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)              </span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">\\</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    static</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> GPIOInstance name </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> {               </span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">\\</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">        .GPIOx </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> port,                         </span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">\\</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">        .GPIO_Pin </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> pin,                       </span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">\\</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">        .state </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> GPIO_PIN_RESET,               </span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">\\</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">    };</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// APP层使用宏</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">DEF_GPIO</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(motor_in1, GPIOB, GPIO_PIN_12);</span></span></code></pre></div><hr><h2 id="八、drv访问bsp方式" tabindex="-1">八、DRV访问BSP方式 <a class="header-anchor" href="#八、drv访问bsp方式" aria-label="Permalink to &quot;八、DRV访问BSP方式&quot;">​</a></h2><p><strong>DRV结构体直接包含BSP结构体（值拷贝），不使用指针</strong></p><table tabindex="0"><thead><tr><th>方式</th><th>定义</th><th>生命周期</th><th>内存安全</th></tr></thead><tbody><tr><td>指针方式 ❌</td><td><code>GPIOInstance *gpio;</code></td><td>需手动管理</td><td>悬空风险</td></tr><tr><td>值拷贝方式 ✅</td><td><code>GPIOInstance gpio;</code></td><td>跟随DRV实例</td><td>安全</td></tr></tbody></table><hr><h2 id="九、回调注册与分发" tabindex="-1">九、回调注册与分发 <a class="header-anchor" href="#九、回调注册与分发" aria-label="Permalink to &quot;九、回调注册与分发&quot;">​</a></h2><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>APP 调用 DRV 注册函数</span></span>
<span class="line"><span>       │</span></span>
<span class="line"><span>       ↓</span></span>
<span class="line"><span>DRV 调用 BSP 注册函数，传入回调</span></span>
<span class="line"><span>       │</span></span>
<span class="line"><span>       ↓</span></span>
<span class="line"><span>BSP 保存实例和回调指针</span></span>
<span class="line"><span>       │</span></span>
<span class="line"><span>       ↓</span></span>
<span class="line"><span>中断发生时，BSP 查找实例并调用回调</span></span></code></pre></div><hr><h2 id="十、线程安全通信" tabindex="-1">十、线程安全通信 <a class="header-anchor" href="#十、线程安全通信" aria-label="Permalink to &quot;十、线程安全通信&quot;">​</a></h2><table tabindex="0"><thead><tr><th>队列类型</th><th>创建位置</th><th>说明</th></tr></thead><tbody><tr><td>线程间通信队列</td><td><code>app/Src/robot.c</code></td><td>用于任务间传递消息</td></tr><tr><td>中断到自己的队列</td><td>任务自己文件中</td><td>用于中断向本任务传递数据</td></tr></tbody></table><p><strong>队列长度统一设置为 1</strong></p><hr><h2 id="十一、通信方式选择" tabindex="-1">十一、通信方式选择 <a class="header-anchor" href="#十一、通信方式选择" aria-label="Permalink to &quot;十一、通信方式选择&quot;">​</a></h2><table tabindex="0"><thead><tr><th>通信方式</th><th>核心功能</th><th>关键特点</th><th>适用场景</th></tr></thead><tbody><tr><td><strong>队列</strong></td><td>任务间传递消息</td><td>数据拷贝，FIFO</td><td>中断到任务，大量数据</td></tr><tr><td><strong>互斥量</strong></td><td>保护共享资源</td><td>优先级继承</td><td>共享数据访问</td></tr><tr><td><strong>二值信号量</strong></td><td>任务同步</td><td>轻量通知</td><td>事件通知</td></tr><tr><td><strong>计数信号量</strong></td><td>资源计数</td><td>多资源管理</td><td>资源池</td></tr><tr><td><strong>任务通知</strong></td><td>直接通信</td><td>最轻量</td><td>任务间简单同步</td></tr></tbody></table><hr><p><em>本文档描述跨层设计原则，各层具体开发规范请参考对应层的开发指南。</em></p>`,56)])])}const g=a(i,[["render",e]]);export{k as __pageData,g as default};
