import{_ as a,o as n,c as i,a2 as p}from"./chunks/framework.BKchhTkY.js";const E=JSON.parse('{"title":"内存与存储原理","description":"","frontmatter":{},"headers":[],"relativePath":"guide/other/内存与存储原理.md","filePath":"guide/other/内存与存储原理.md","lastUpdated":1783672098000}'),l={name:"guide/other/内存与存储原理.md"};function t(h,s,e,k,d,r){return n(),i("div",null,[...s[0]||(s[0]=[p(`<h1 id="内存与存储原理" tabindex="-1">内存与存储原理 <a class="header-anchor" href="#内存与存储原理" aria-label="Permalink to &quot;内存与存储原理&quot;">​</a></h1><p>本文档详细介绍嵌入式系统的内存模型，分为理想模型和实际模型两部分。</p><hr><h1 id="第一章-理想模型-ram与flash运行原理" tabindex="-1">第一章：理想模型 - RAM与Flash运行原理 <a class="header-anchor" href="#第一章-理想模型-ram与flash运行原理" aria-label="Permalink to &quot;第一章：理想模型 - RAM与Flash运行原理&quot;">​</a></h1><h2 id="_1-1-物理特性对比" tabindex="-1">1.1 物理特性对比 <a class="header-anchor" href="#_1-1-物理特性对比" aria-label="Permalink to &quot;1.1 物理特性对比&quot;">​</a></h2><table tabindex="0"><thead><tr><th>特性</th><th>Flash</th><th>RAM</th></tr></thead><tbody><tr><td><strong>用途</strong></td><td>存储程序代码和常量</td><td>存储运行时数据</td></tr><tr><td><strong>掉电后</strong></td><td>数据保留</td><td>数据丢失</td></tr><tr><td><strong>速度</strong></td><td>较慢（需要等待状态）</td><td>快</td></tr><tr><td><strong>写入方式</strong></td><td>需要擦除后才能写</td><td>随机读写</td></tr><tr><td><strong>生命周期</strong></td><td>有限（约10万次擦写）</td><td>无限</td></tr><tr><td><strong>成本</strong></td><td>低（每字节）</td><td>高（每字节）</td></tr></tbody></table><h2 id="_1-2-内存区域划分" tabindex="-1">1.2 内存区域划分 <a class="header-anchor" href="#_1-2-内存区域划分" aria-label="Permalink to &quot;1.2 内存区域划分&quot;">​</a></h2><h3 id="_1-2-1-整体布局" tabindex="-1">1.2.1 整体布局 <a class="header-anchor" href="#_1-2-1-整体布局" aria-label="Permalink to &quot;1.2.1 整体布局&quot;">​</a></h3><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>高地址</span></span>
<span class="line"><span>┌─────────────────────┐</span></span>
<span class="line"><span>│      栈 (Stack)     │ ← 向下增长（局部变量、函数调用）</span></span>
<span class="line"><span>├─────────────────────┤</span></span>
<span class="line"><span>│         ↓           │</span></span>
<span class="line"><span>│                     │</span></span>
<span class="line"><span>│         ↑           │</span></span>
<span class="line"><span>├─────────────────────┤</span></span>
<span class="line"><span>│      堆 (Heap)      │ ← 向上增长（malloc/free）</span></span>
<span class="line"><span>├─────────────────────┤</span></span>
<span class="line"><span>│    .bss段 (未初始化) │ ← 全局/静态变量，初始化为0</span></span>
<span class="line"><span>├─────────────────────┤</span></span>
<span class="line"><span>│    .data段 (已初始化)│ ← 全局/静态变量，有初始值</span></span>
<span class="line"><span>├─────────────────────┤</span></span>
<span class="line"><span>│   Flash中的镜像      │ ← .data的初始值存储在这里</span></span>
<span class="line"><span>├─────────────────────┤</span></span>
<span class="line"><span>│    .rodata段 (只读)  │ ← const常量，字符串字面量</span></span>
<span class="line"><span>├─────────────────────┤</span></span>
<span class="line"><span>│    .text段 (代码)    │ ← 程序代码</span></span>
<span class="line"><span>└─────────────────────┘</span></span>
<span class="line"><span>低地址</span></span></code></pre></div><h3 id="_1-2-2-各区域详解" tabindex="-1">1.2.2 各区域详解 <a class="header-anchor" href="#_1-2-2-各区域详解" aria-label="Permalink to &quot;1.2.2 各区域详解&quot;">​</a></h3><h4 id="text段-代码段" tabindex="-1">.text段（代码段） <a class="header-anchor" href="#text段-代码段" aria-label="Permalink to &quot;.text段（代码段）&quot;">​</a></h4><ul><li><strong>位置</strong>：Flash</li><li><strong>内容</strong>：程序的可执行机器码</li><li><strong>特点</strong>：只读，大小固定</li></ul><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 所有函数代码都存放在.text段</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> my_function</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) {</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">    // ...</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">int</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> main</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) {</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">    // ...</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    return</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 0</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span></span></code></pre></div><h4 id="rodata段-只读数据段" tabindex="-1">.rodata段（只读数据段） <a class="header-anchor" href="#rodata段-只读数据段" aria-label="Permalink to &quot;.rodata段（只读数据段）&quot;">​</a></h4><ul><li><strong>位置</strong>：Flash</li><li><strong>内容</strong>：常量、字符串字面量、const全局变量</li><li><strong>特点</strong>：只读，不占用RAM</li></ul><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">const</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> config_value </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 100</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">           // 全局const变量 → .rodata</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">const</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> char*</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> error_msg </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> &quot;Error&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">        // 字符串字面量 → .rodata</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">const</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> uint8_t</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;"> lookup_table</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">[</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">256</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">] </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> {...};</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 查找表 → .rodata</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 注意区别</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">char*</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> str </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> &quot;Hello&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">    // &quot;Hello&quot;在.rodata，str指针在RAM</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">char</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> str2</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">[]</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> =</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> &quot;Hello&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">  // 数组内容在RAM（从.rodata复制）</span></span></code></pre></div><h4 id="data段-已初始化数据段" tabindex="-1">.data段（已初始化数据段） <a class="header-anchor" href="#data段-已初始化数据段" aria-label="Permalink to &quot;.data段（已初始化数据段）&quot;">​</a></h4><ul><li><strong>位置</strong>：RAM（运行时），Flash（存储初始值）</li><li><strong>内容</strong>：有初始值的全局变量、静态变量</li><li><strong>特点</strong>：占用Flash+RAM双份空间</li></ul><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> g_initialized </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 100</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">            // .data（占Flash存储初始值 + RAM运行空间）</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">static</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> s_counter </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 0</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">           // .data</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">float</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;"> g_array</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">[</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">10</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">] </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> {</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">1</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">,</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">2</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">,</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">3</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">};</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">        // .data</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 启动过程：</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 1. Flash中存储初始值 [100, 0, 1,2,3,...]</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 2. 启动代码将初始值复制到RAM</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 3. 程序运行时在RAM中读写</span></span></code></pre></div><h4 id="bss段-未初始化数据段" tabindex="-1">.bss段（未初始化数据段） <a class="header-anchor" href="#bss段-未初始化数据段" aria-label="Permalink to &quot;.bss段（未初始化数据段）&quot;">​</a></h4><ul><li><strong>位置</strong>：RAM</li><li><strong>内容</strong>：未初始化或初始化为0的全局变量、静态变量</li><li><strong>特点</strong>：只占RAM，不占Flash空间</li></ul><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">int</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;"> g_buffer</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">[</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">256</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">];</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">              // .bss（只占RAM）</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">static</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> s_flag;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">              // .bss</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">double</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;"> g_matrix</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">[</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">10</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">][</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">10</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">];</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">        // .bss</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 启动过程：</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 1. 不占用Flash空间（没有初始值需要存储）</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 2. 启动代码直接在RAM中清零</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 3. 程序运行时在RAM中读写</span></span></code></pre></div><h4 id="栈-stack" tabindex="-1">栈（Stack） <a class="header-anchor" href="#栈-stack" aria-label="Permalink to &quot;栈（Stack）&quot;">​</a></h4><ul><li><strong>位置</strong>：RAM</li><li><strong>内容</strong>：局部变量、函数参数、返回地址、寄存器保存</li><li><strong>特点</strong>：自动管理，向低地址增长</li></ul><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> function</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">int</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;"> param</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">         // param在栈上</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">{</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> local_var </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 10</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">          // 局部变量 → 栈</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    char</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;"> buffer</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">[</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">64</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">];</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">             // 局部数组 → 栈</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    if</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> (condition) {</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">        int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> block_var;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">           // 块作用域变量 → 栈</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">    }</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">   // 函数返回后，栈上所有局部变量自动释放</span></span></code></pre></div><h4 id="堆-heap" tabindex="-1">堆（Heap） <a class="header-anchor" href="#堆-heap" aria-label="Permalink to &quot;堆（Heap）&quot;">​</a></h4><ul><li><strong>位置</strong>：RAM</li><li><strong>内容</strong>：动态分配的内存</li><li><strong>特点</strong>：手动管理，向高地址增长</li></ul><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> example</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) {</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    int*</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> ptr1 </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> malloc</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">100</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">     // 动态分配 → 堆</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    char*</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> ptr2 </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> calloc</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">50</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">, </span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">1</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">  // 动态分配并清零 → 堆</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">    free</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(ptr1);</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">                  // 手动释放</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">    free</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(ptr2);</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span></span></code></pre></div><h2 id="_1-3-c语言语法与内存区域对应" tabindex="-1">1.3 C语言语法与内存区域对应 <a class="header-anchor" href="#_1-3-c语言语法与内存区域对应" aria-label="Permalink to &quot;1.3 C语言语法与内存区域对应&quot;">​</a></h2><h3 id="_1-3-1-速查表" tabindex="-1">1.3.1 速查表 <a class="header-anchor" href="#_1-3-1-速查表" aria-label="Permalink to &quot;1.3.1 速查表&quot;">​</a></h3><table tabindex="0"><thead><tr><th>C语言定义</th><th>存储位置</th><th>段名</th><th>占用Flash</th><th>占用RAM</th></tr></thead><tbody><tr><td><code>int g = 1;</code></td><td>RAM</td><td>.data</td><td>✓（初始值）</td><td>✓</td></tr><tr><td><code>int g;</code></td><td>RAM</td><td>.bss</td><td>✗</td><td>✓</td></tr><tr><td><code>const int g = 1;</code></td><td>Flash</td><td>.rodata</td><td>✓</td><td>✗</td></tr><tr><td><code>static int s = 1;</code></td><td>RAM</td><td>.data</td><td>✓（初始值）</td><td>✓</td></tr><tr><td><code>static int s;</code></td><td>RAM</td><td>.bss</td><td>✗</td><td>✓</td></tr><tr><td><code>void func(){ int x; }</code></td><td>栈</td><td>-</td><td>✗</td><td>✓（临时）</td></tr><tr><td><code>malloc(100)</code></td><td>堆</td><td>-</td><td>✗</td><td>✓</td></tr><tr><td><code>&quot;string&quot;</code></td><td>Flash</td><td>.rodata</td><td>✓</td><td>✗</td></tr><tr><td><code>char arr[] = &quot;str&quot;;</code></td><td>RAM</td><td>.data</td><td>✓</td><td>✓</td></tr></tbody></table><h3 id="_1-3-2-关键字详解" tabindex="-1">1.3.2 关键字详解 <a class="header-anchor" href="#_1-3-2-关键字详解" aria-label="Permalink to &quot;1.3.2 关键字详解&quot;">​</a></h3><h4 id="static关键字" tabindex="-1">static关键字 <a class="header-anchor" href="#static关键字" aria-label="Permalink to &quot;static关键字&quot;">​</a></h4><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 全局作用域的static：限制链接可见性</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">static</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> module_private </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 10</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">  // 只在本文件可见，存放在.data</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 函数内的static：持久存储</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> counter</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) {</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    static</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> count </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 0</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">        // 存放在.data，程序生命周期</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">    count</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">++</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">                     // 值在多次调用间保持</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 静态局部变量 vs 普通局部变量</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> example</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) {</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> normal </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 0</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">              // 栈上，每次调用重新初始化</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    static</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> persistent </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 0</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">   // .data，只初始化一次</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span></span></code></pre></div><h4 id="const关键字" tabindex="-1">const关键字 <a class="header-anchor" href="#const关键字" aria-label="Permalink to &quot;const关键字&quot;">​</a></h4><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// const全局变量 → .rodata（Flash）</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">const</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> MAX_VALUE </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 100</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// const局部变量 → 栈（仍是栈上的只读变量）</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> func</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) {</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    const</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> local_const </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 10</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">  // 栈上</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// const指针的不同含义</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">const</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> int*</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> p1;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">        // 指向的内容不可变</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">int*</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> const</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> p2;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">        // 指针本身不可变</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">const</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> int*</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> const</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> p3;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">  // 都不可变</span></span></code></pre></div><h4 id="volatile关键字" tabindex="-1">volatile关键字 <a class="header-anchor" href="#volatile关键字" aria-label="Permalink to &quot;volatile关键字&quot;">​</a></h4><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// volatile不改变存储位置，只告诉编译器不要优化访问</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">volatile</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> int</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> hardware_reg;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">       // 可能是内存映射寄存器</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">volatile</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> uint32_t*</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> DMA_ADDR;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">     // DMA可能修改的地址</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 常见使用场景</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#define</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> REG_BASE</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    0x</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">40000000</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#define</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> GPIO_ODR</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">    (</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">*</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">volatile</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> uint32_t*</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)(REG_BASE </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">+</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> 0x</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">14</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">))</span></span></code></pre></div><h2 id="_1-4-程序启动初始化流程" tabindex="-1">1.4 程序启动初始化流程 <a class="header-anchor" href="#_1-4-程序启动初始化流程" aria-label="Permalink to &quot;1.4 程序启动初始化流程&quot;">​</a></h2><h3 id="_1-4-1-启动流程图" tabindex="-1">1.4.1 启动流程图 <a class="header-anchor" href="#_1-4-1-启动流程图" aria-label="Permalink to &quot;1.4.1 启动流程图&quot;">​</a></h3><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>复位</span></span>
<span class="line"><span>    │</span></span>
<span class="line"><span>    ▼</span></span>
<span class="line"><span>┌─────────────────┐</span></span>
<span class="line"><span>│ 1. 硬件初始化    │  ← 设置栈指针(SP)到RAM末尾</span></span>
<span class="line"><span>│    (启动代码)    │  ← 设置PC到Reset_Handler</span></span>
<span class="line"><span>└────────┬────────┘</span></span>
<span class="line"><span>         │</span></span>
<span class="line"><span>         ▼</span></span>
<span class="line"><span>┌─────────────────┐</span></span>
<span class="line"><span>│ 2. .data段初始化 │  ← 从Flash复制初始值到RAM</span></span>
<span class="line"><span>│                 │  memcpy(_sdata, _sidata, size)</span></span>
<span class="line"><span>└────────┬────────┘</span></span>
<span class="line"><span>         │</span></span>
<span class="line"><span>         ▼</span></span>
<span class="line"><span>┌─────────────────┐</span></span>
<span class="line"><span>│ 3. .bss段清零   │  ← 将未初始化数据区清零</span></span>
<span class="line"><span>│                 │  memset(_sbss, 0, size)</span></span>
<span class="line"><span>└────────┬────────┘</span></span>
<span class="line"><span>         │</span></span>
<span class="line"><span>         ▼</span></span>
<span class="line"><span>┌─────────────────┐</span></span>
<span class="line"><span>│ 4. 系统初始化    │  ← SystemInit()：时钟配置等</span></span>
<span class="line"><span>└────────┬────────┘</span></span>
<span class="line"><span>         │</span></span>
<span class="line"><span>         ▼</span></span>
<span class="line"><span>┌─────────────────┐</span></span>
<span class="line"><span>│ 5. C运行环境    │  ← C库初始化（如有）</span></span>
<span class="line"><span>│    初始化       │</span></span>
<span class="line"><span>└────────┬────────┘</span></span>
<span class="line"><span>         │</span></span>
<span class="line"><span>         ▼</span></span>
<span class="line"><span>┌─────────────────┐</span></span>
<span class="line"><span>│ 6. main()       │  ← 进入用户程序</span></span>
<span class="line"><span>└─────────────────┘</span></span></code></pre></div><h3 id="_1-4-2-启动代码示例" tabindex="-1">1.4.2 启动代码示例 <a class="header-anchor" href="#_1-4-2-启动代码示例" aria-label="Permalink to &quot;1.4.2 启动代码示例&quot;">​</a></h3><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 典型的启动文件逻辑（简化版）</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> Reset_Handler</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) {</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">    // 1. 复制.data段从Flash到RAM</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    uint32_t</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> *</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">src </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> &amp;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">_sidata;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">   // Flash中的数据</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    uint32_t</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> *</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">dst </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> &amp;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">_sdata;</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">    // RAM目标地址</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    while</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> (dst </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">&lt;</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> &amp;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">_edata) {</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">        *</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">dst</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">++</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> =</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> *</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">src</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">++</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">    }</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">    // 2. 清零.bss段</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">    dst </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> &amp;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">_sbss;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    while</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> (dst </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">&lt;</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> &amp;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">_ebss) {</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">        *</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">dst</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">++</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> =</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> 0</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">    }</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">    // 3. 调用系统初始化</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">    SystemInit</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">();</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">    // 4. 调用main</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">    main</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">();</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">    // 5. 不应该返回</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    while</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">1</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span></span></code></pre></div><hr><h1 id="第二章-实际模型-stm32-f4-h7内存架构" tabindex="-1">第二章：实际模型 - STM32 F4/H7内存架构 <a class="header-anchor" href="#第二章-实际模型-stm32-f4-h7内存架构" aria-label="Permalink to &quot;第二章：实际模型 - STM32 F4/H7内存架构&quot;">​</a></h1><h2 id="_2-1-stm32f407内存架构" tabindex="-1">2.1 STM32F407内存架构 <a class="header-anchor" href="#_2-1-stm32f407内存架构" aria-label="Permalink to &quot;2.1 STM32F407内存架构&quot;">​</a></h2><h3 id="_2-1-1-内存映射" tabindex="-1">2.1.1 内存映射 <a class="header-anchor" href="#_2-1-1-内存映射" aria-label="Permalink to &quot;2.1.1 内存映射&quot;">​</a></h3><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>地址空间布局</span></span>
<span class="line"><span></span></span>
<span class="line"><span>0x20000000 ┌──────────────────┐ 128KB</span></span>
<span class="line"><span>           │  SRAM1           │ ← 主RAM，DMA可访问</span></span>
<span class="line"><span>           │  (通用用途)      │</span></span>
<span class="line"><span>0x2001FFFF └──────────────────┘</span></span>
<span class="line"><span></span></span>
<span class="line"><span>0x10000000 ┌──────────────────┐ 64KB</span></span>
<span class="line"><span>           │  CCMRAM          │ ← 核心耦合内存</span></span>
<span class="line"><span>           │  (Core Coupled)  │   DMA不可访问！</span></span>
<span class="line"><span>0x1000FFFF └──────────────────┘</span></span>
<span class="line"><span></span></span>
<span class="line"><span>0x08000000 ┌──────────────────┐ 512KB</span></span>
<span class="line"><span>           │  FLASH           │ ← 程序存储</span></span>
<span class="line"><span>           │                  │</span></span>
<span class="line"><span>0x0807FFFF └──────────────────┘</span></span>
<span class="line"><span></span></span>
<span class="line"><span>0x40000000 ┌──────────────────┐</span></span>
<span class="line"><span>           │  外设寄存器区     │ ← AHB/APB外设</span></span>
<span class="line"><span>           │  (Peripheral)    │</span></span>
<span class="line"><span>0x5FFFFFFF └──────────────────┘</span></span></code></pre></div><h3 id="_2-1-2-各区域特性" tabindex="-1">2.1.2 各区域特性 <a class="header-anchor" href="#_2-1-2-各区域特性" aria-label="Permalink to &quot;2.1.2 各区域特性&quot;">​</a></h3><table tabindex="0"><thead><tr><th>区域</th><th>起始地址</th><th>大小</th><th>特点</th></tr></thead><tbody><tr><td>SRAM1</td><td>0x20000000</td><td>128KB</td><td>通用RAM，DMA可访问，默认数据区</td></tr><tr><td>CCMRAM</td><td>0x10000000</td><td>64KB</td><td>仅CPU可访问，DMA不可访问，速度快</td></tr><tr><td>FLASH</td><td>0x08000000</td><td>512KB</td><td>程序存储，支持读加速</td></tr></tbody></table><h3 id="_2-1-3-链接脚本示例-f407" tabindex="-1">2.1.3 链接脚本示例（F407） <a class="header-anchor" href="#_2-1-3-链接脚本示例-f407" aria-label="Permalink to &quot;2.1.3 链接脚本示例（F407）&quot;">​</a></h3><div class="language-txt vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">txt</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>/* 内存区域定义 */</span></span>
<span class="line"><span>MEMORY</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    RAM (xrw)    : ORIGIN = 0x20000000, LENGTH = 128K</span></span>
<span class="line"><span>    CCMRAM (xrw) : ORIGIN = 0x10000000, LENGTH = 64K</span></span>
<span class="line"><span>    FLASH (rx)   : ORIGIN = 0x08000000, LENGTH = 512K</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>/* 段定义 */</span></span>
<span class="line"><span>SECTIONS</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    /* 中断向量表 - 必须放在Flash起始位置 */</span></span>
<span class="line"><span>    .isr_vector :</span></span>
<span class="line"><span>    {</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>        KEEP(*(.isr_vector))</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>    } &gt;FLASH</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* 代码段 */</span></span>
<span class="line"><span>    .text :</span></span>
<span class="line"><span>    {</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>        *(.text)</span></span>
<span class="line"><span>        *(.text*)</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>        _etext = .;</span></span>
<span class="line"><span>    } &gt;FLASH</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* 只读数据 */</span></span>
<span class="line"><span>    .rodata :</span></span>
<span class="line"><span>    {</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>        *(.rodata)</span></span>
<span class="line"><span>        *(.rodata*)</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>    } &gt;FLASH</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* 已初始化数据 - LMA在Flash，VMA在RAM */</span></span>
<span class="line"><span>    .data :</span></span>
<span class="line"><span>    {</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>        _sdata = .;</span></span>
<span class="line"><span>        *(.data)</span></span>
<span class="line"><span>        *(.data*)</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>        _edata = .;</span></span>
<span class="line"><span>    } &gt;RAM AT&gt; FLASH</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* 未初始化数据 */</span></span>
<span class="line"><span>    .bss :</span></span>
<span class="line"><span>    {</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>        _sbss = .;</span></span>
<span class="line"><span>        *(.bss)</span></span>
<span class="line"><span>        *(.bss*)</span></span>
<span class="line"><span>        _ebss = .;</span></span>
<span class="line"><span>    } &gt;RAM</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* CCMRAM段（可选使用） */</span></span>
<span class="line"><span>    .ccmram :</span></span>
<span class="line"><span>    {</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>        _sccmram = .;</span></span>
<span class="line"><span>        *(.ccmram)</span></span>
<span class="line"><span>        *(.ccmram*)</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>        _eccmram = .;</span></span>
<span class="line"><span>    } &gt;CCMRAM AT&gt; FLASH</span></span>
<span class="line"><span>}</span></span></code></pre></div><h2 id="_2-2-stm32h7内存架构-复杂" tabindex="-1">2.2 STM32H7内存架构（复杂） <a class="header-anchor" href="#_2-2-stm32h7内存架构-复杂" aria-label="Permalink to &quot;2.2 STM32H7内存架构（复杂）&quot;">​</a></h2><h3 id="_2-2-1-内存映射" tabindex="-1">2.2.1 内存映射 <a class="header-anchor" href="#_2-2-1-内存映射" aria-label="Permalink to &quot;2.2.1 内存映射&quot;">​</a></h3><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>地址空间布局（STM32H723）</span></span>
<span class="line"><span></span></span>
<span class="line"><span>0x20000000 ┌──────────────────┐ 128KB</span></span>
<span class="line"><span>           │  DTCM-RAM        │ ← 数据TCM，CPU专用</span></span>
<span class="line"><span>           │  (Data TCM)      │   DMA1不可访问！</span></span>
<span class="line"><span>0x2001FFFF └──────────────────┘</span></span>
<span class="line"><span></span></span>
<span class="line"><span>0x00000000 ┌──────────────────┐ 64KB</span></span>
<span class="line"><span>           │  ITCM-RAM        │ ← 指令TCM</span></span>
<span class="line"><span>           │  (Instruction)   │   可存放关键代码</span></span>
<span class="line"><span>0x0000FFFF └──────────────────┘</span></span>
<span class="line"><span></span></span>
<span class="line"><span>0x24000000 ┌──────────────────┐ 512KB</span></span>
<span class="line"><span>           │  AXI SRAM        │ ← AXI总线SRAM</span></span>
<span class="line"><span>           │                  │   DMA可访问</span></span>
<span class="line"><span>0x2407FFFF └──────────────────┘</span></span>
<span class="line"><span></span></span>
<span class="line"><span>0x30000000 ┌──────────────────┐ 128KB</span></span>
<span class="line"><span>           │  SRAM1           │ ← AHB SRAM</span></span>
<span class="line"><span>0x3001FFFF ├──────────────────┤   DMA1可访问！</span></span>
<span class="line"><span>           │  SRAM2           │ 128KB</span></span>
<span class="line"><span>0x3003FFFF ├──────────────────┤</span></span>
<span class="line"><span>           │  SRAM3           │ 32KB</span></span>
<span class="line"><span>0x30047FFF ├──────────────────┤</span></span>
<span class="line"><span>           │  SRAM4           │ 16KB</span></span>
<span class="line"><span>0x3004BFFF └──────────────────┘</span></span>
<span class="line"><span></span></span>
<span class="line"><span>0x38000000 ┌──────────────────┐ 64KB</span></span>
<span class="line"><span>           │  Backup SRAM     │ ← 备份域SRAM</span></span>
<span class="line"><span>0x3800FFFF └──────────────────┘</span></span>
<span class="line"><span></span></span>
<span class="line"><span>0x08000000 ┌──────────────────┐ 1MB</span></span>
<span class="line"><span>           │  FLASH           │</span></span>
<span class="line"><span>0x080FFFFF └──────────────────┘</span></span></code></pre></div><h3 id="_2-2-2-各区域特性" tabindex="-1">2.2.2 各区域特性 <a class="header-anchor" href="#_2-2-2-各区域特性" aria-label="Permalink to &quot;2.2.2 各区域特性&quot;">​</a></h3><table tabindex="0"><thead><tr><th>区域</th><th>起始地址</th><th>大小</th><th>总线</th><th>DMA访问</th></tr></thead><tbody><tr><td>DTCM-RAM</td><td>0x20000000</td><td>128KB</td><td>TCM</td><td>DMA1不可，DMA2需BDMA</td></tr><tr><td>ITCM-RAM</td><td>0x00000000</td><td>64KB</td><td>TCM</td><td>DMA不可</td></tr><tr><td>AXI SRAM</td><td>0x24000000</td><td>512KB</td><td>AXI</td><td>MDMA可访问</td></tr><tr><td>SRAM1</td><td>0x30000000</td><td>128KB</td><td>AHB</td><td>DMA1/DMA2可访问</td></tr><tr><td>SRAM2</td><td>0x30020000</td><td>128KB</td><td>AHB</td><td>DMA1/DMA2可访问</td></tr><tr><td>SRAM3</td><td>0x30040000</td><td>32KB</td><td>AHB</td><td>DMA1/DMA2可访问</td></tr><tr><td>SRAM4</td><td>0x38000000</td><td>64KB</td><td>AHB</td><td>DMA2/BDMA可访问</td></tr></tbody></table><h3 id="_2-2-3-关键问题-dma访问限制" tabindex="-1">2.2.3 关键问题：DMA访问限制 <a class="header-anchor" href="#_2-2-3-关键问题-dma访问限制" aria-label="Permalink to &quot;2.2.3 关键问题：DMA访问限制&quot;">​</a></h3><p><strong>问题描述：</strong></p><ul><li>STM32H7默认将变量放在DTCM-RAM（0x20000000）</li><li>DTCM-RAM与DMA1控制器不在同一总线</li><li>DMA1无法访问DTCM-RAM中的数据</li></ul><p><strong>解决方案：</strong> 将DMA缓冲区放到SRAM1/2/3（0x30000000区域）</p><h3 id="_2-2-4-链接脚本示例-h7" tabindex="-1">2.2.4 链接脚本示例（H7） <a class="header-anchor" href="#_2-2-4-链接脚本示例-h7" aria-label="Permalink to &quot;2.2.4 链接脚本示例（H7）&quot;">​</a></h3><div class="language-txt vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">txt</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>/* 内存区域定义 */</span></span>
<span class="line"><span>MEMORY</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    ITCMRAM (xrw)  : ORIGIN = 0x00000000, LENGTH = 64K</span></span>
<span class="line"><span>    DTCMRAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 128K</span></span>
<span class="line"><span>    RAM_D2 (xrw)   : ORIGIN = 0x30000000, LENGTH = 288K  /* SRAM1+2+3 */</span></span>
<span class="line"><span>    RAM_D3 (xrw)   : ORIGIN = 0x38000000, LENGTH = 64K   /* SRAM4 */</span></span>
<span class="line"><span>    FLASH (rx)     : ORIGIN = 0x08000000, LENGTH = 1024K</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>/* DMA缓冲区段定义 */</span></span>
<span class="line"><span>SECTIONS</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    /* 其他段... */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* DMA缓冲区 - 放到RAM_D2（DMA1可访问） */</span></span>
<span class="line"><span>    .dma_buffer (NOLOAD) :</span></span>
<span class="line"><span>    {</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>        _sdma_buffer = .;</span></span>
<span class="line"><span>        *(.dma_buffer)</span></span>
<span class="line"><span>        *(.dma_buffer*)</span></span>
<span class="line"><span>        . = ALIGN(4);</span></span>
<span class="line"><span>        _edma_buffer = .;</span></span>
<span class="line"><span>    } &gt;RAM_D2</span></span>
<span class="line"><span>}</span></span></code></pre></div><h2 id="_2-3-链接脚本语法详解" tabindex="-1">2.3 链接脚本语法详解 <a class="header-anchor" href="#_2-3-链接脚本语法详解" aria-label="Permalink to &quot;2.3 链接脚本语法详解&quot;">​</a></h2><h3 id="_2-3-1-基本结构" tabindex="-1">2.3.1 基本结构 <a class="header-anchor" href="#_2-3-1-基本结构" aria-label="Permalink to &quot;2.3.1 基本结构&quot;">​</a></h3><div class="language-txt vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">txt</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>/* ==================== 入口点定义 ==================== */</span></span>
<span class="line"><span>ENTRY(Reset_Handler)    /* 程序开始执行的位置 */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>/* ==================== 栈和堆定义 ==================== */</span></span>
<span class="line"><span>_estack = ORIGIN(RAM) + LENGTH(RAM);  /* 栈顶 = RAM末尾 */</span></span>
<span class="line"><span>_Min_Heap_Size = 0x200;    /* 最小堆大小 */</span></span>
<span class="line"><span>_Min_Stack_Size = 0x400;   /* 最小栈大小 */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>/* ==================== 内存区域定义 ==================== */</span></span>
<span class="line"><span>MEMORY</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    /* 名称 (属性) : 起始地址 = ORIGIN, 大小 = LENGTH */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* 属性说明：</span></span>
<span class="line"><span>       R - 可读    W - 可写    X - 可执行</span></span>
<span class="line"><span>       A - 可分配  I - 可初始化</span></span>
<span class="line"><span>       rx  = 只读可执行（Flash）</span></span>
<span class="line"><span>       xrw = 可读可写可执行（RAM）</span></span>
<span class="line"><span>    */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    RAM (xrw)   : ORIGIN = 0x20000000, LENGTH = 128K</span></span>
<span class="line"><span>    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>/* ==================== 段定义 ==================== */</span></span>
<span class="line"><span>SECTIONS</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    /* 段内容... */</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="_2-3-2-段定义语法" tabindex="-1">2.3.2 段定义语法 <a class="header-anchor" href="#_2-3-2-段定义语法" aria-label="Permalink to &quot;2.3.2 段定义语法&quot;">​</a></h3><div class="language-txt vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">txt</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>SECTIONS</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    /* ========== 基本段定义 ========== */</span></span>
<span class="line"><span>    .section_name :</span></span>
<span class="line"><span>    {</span></span>
<span class="line"><span>        /* 内容 */</span></span>
<span class="line"><span>    } &gt;MEMORY_REGION</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* ========== 位置计数器 ========== */</span></span>
<span class="line"><span>    . = ALIGN(4);           /* 4字节对齐 */</span></span>
<span class="line"><span>    . = 0x08000100;         /* 设置到特定地址 */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    _symbol = .;            /* 定义符号（可用于C代码） */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* ========== 输入段匹配 ========== */</span></span>
<span class="line"><span>    *(.text)                /* 所有.text段 */</span></span>
<span class="line"><span>    *(.text*)               /* 所有.text开头的段 */</span></span>
<span class="line"><span>    *(.text .text.*)        /* 多个模式 */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* ========== KEEP指令 ========== */</span></span>
<span class="line"><span>    KEEP(*(.isr_vector))    /* 防止被链接器优化掉 */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* ========== 双地址：VMA和LMA ========== */</span></span>
<span class="line"><span>    .data :</span></span>
<span class="line"><span>    {</span></span>
<span class="line"><span>        _sdata = .;</span></span>
<span class="line"><span>        *(.data)</span></span>
<span class="line"><span>        _edata = .;</span></span>
<span class="line"><span>    } &gt;RAM AT&gt; FLASH        /* VMA=RAM, LMA=Flash */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /*</span></span>
<span class="line"><span>     * VMA (Virtual Memory Address): 运行时的地址</span></span>
<span class="line"><span>     * LMA (Load Memory Address):    加载时的地址</span></span>
<span class="line"><span>     *</span></span>
<span class="line"><span>     * .data段：</span></span>
<span class="line"><span>     *   - LMA = Flash（初始值存储位置）</span></span>
<span class="line"><span>     *   - VMA = RAM（运行时位置）</span></span>
<span class="line"><span>     *   - 启动代码负责从LMA复制到VMA</span></span>
<span class="line"><span>     */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* ========== LOADADDR函数 ========== */</span></span>
<span class="line"><span>    _sidata = LOADADDR(.data);  /* 获取段的LMA地址 */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* ========== NOLOAD属性 ========== */</span></span>
<span class="line"><span>    .bss (NOLOAD) :</span></span>
<span class="line"><span>    {</span></span>
<span class="line"><span>        _sbss = .;</span></span>
<span class="line"><span>        *(.bss)</span></span>
<span class="line"><span>        _ebss = .;</span></span>
<span class="line"><span>    } &gt;RAM</span></span>
<span class="line"><span>    /* NOLOAD: 不从Flash加载，启动代码直接清零 */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>    /* ========== 丢弃段 ========== */</span></span>
<span class="line"><span>    /DISCARD/ :</span></span>
<span class="line"><span>    {</span></span>
<span class="line"><span>        libc.a ( * )        /* 丢弃标准库中不需要的部分 */</span></span>
<span class="line"><span>        libm.a ( * )</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="_2-3-3-常用内置函数" tabindex="-1">2.3.3 常用内置函数 <a class="header-anchor" href="#_2-3-3-常用内置函数" aria-label="Permalink to &quot;2.3.3 常用内置函数&quot;">​</a></h3><table tabindex="0"><thead><tr><th>函数</th><th>说明</th><th>示例</th></tr></thead><tbody><tr><td><code>ORIGIN(memory)</code></td><td>获取内存区域起始地址</td><td><code>ORIGIN(RAM)</code></td></tr><tr><td><code>LENGTH(memory)</code></td><td>获取内存区域大小</td><td><code>LENGTH(FLASH)</code></td></tr><tr><td><code>LOADADDR(section)</code></td><td>获取段的LMA地址</td><td><code>LOADADDR(.data)</code></td></tr><tr><td><code>ADDR(section)</code></td><td>获取段的VMA地址</td><td><code>ADDR(.data)</code></td></tr><tr><td><code>SIZEOF(section)</code></td><td>获取段大小</td><td><code>SIZEOF(.text)</code></td></tr><tr><td><code>ALIGN(exp)</code></td><td>对齐到exp边界</td><td><code>ALIGN(4)</code></td></tr></tbody></table><h3 id="_2-3-4-特殊段说明" tabindex="-1">2.3.4 特殊段说明 <a class="header-anchor" href="#_2-3-4-特殊段说明" aria-label="Permalink to &quot;2.3.4 特殊段说明&quot;">​</a></h3><div class="language-txt vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">txt</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>/* ========== 中断向量表 ========== */</span></span>
<span class="line"><span>.isr_vector :</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    . = ALIGN(4);</span></span>
<span class="line"><span>    KEEP(*(.isr_vector))    /* 必须保留，且放在Flash起始位置 */</span></span>
<span class="line"><span>    . = ALIGN(4);</span></span>
<span class="line"><span>} &gt;FLASH</span></span>
<span class="line"><span></span></span>
<span class="line"><span>/* ========== ARM/Thumb转换 ========== */</span></span>
<span class="line"><span>.glue_7 :                   /* ARM调用Thumb函数的转换代码 */</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    *(.glue_7)</span></span>
<span class="line"><span>    *(.glue_7t)</span></span>
<span class="line"><span>} &gt;FLASH</span></span>
<span class="line"><span></span></span>
<span class="line"><span>/* ========== C++构造/析构函数表 ========== */</span></span>
<span class="line"><span>.preinit_array :</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    PROVIDE_HIDDEN(__preinit_array_start = .);</span></span>
<span class="line"><span>    KEEP(*(.preinit_array*))</span></span>
<span class="line"><span>    PROVIDE_HIDDEN(__preinit_array_end = .);</span></span>
<span class="line"><span>} &gt;FLASH</span></span>
<span class="line"><span></span></span>
<span class="line"><span>.init_array :</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    PROVIDE_HIDDEN(__init_array_start = .);</span></span>
<span class="line"><span>    KEEP(*(SORT(.init_array.*)))</span></span>
<span class="line"><span>    KEEP(*(.init_array*))</span></span>
<span class="line"><span>    PROVIDE_HIDDEN(__init_array_end = .);</span></span>
<span class="line"><span>} &gt;FLASH</span></span>
<span class="line"><span></span></span>
<span class="line"><span>/* ========== RAM中运行的函数 ========== */</span></span>
<span class="line"><span>.RamFunc :</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    . = ALIGN(4);</span></span>
<span class="line"><span>    *(.RamFunc)</span></span>
<span class="line"><span>    *(.RamFunc*)</span></span>
<span class="line"><span>    . = ALIGN(4);</span></span>
<span class="line"><span>} &gt;RAM AT&gt; FLASH</span></span></code></pre></div><h2 id="_2-4-自定义段的实现" tabindex="-1">2.4 自定义段的实现 <a class="header-anchor" href="#_2-4-自定义段的实现" aria-label="Permalink to &quot;2.4 自定义段的实现&quot;">​</a></h2><h3 id="_2-4-1-定义段属性宏" tabindex="-1">2.4.1 定义段属性宏 <a class="header-anchor" href="#_2-4-1-定义段属性宏" aria-label="Permalink to &quot;2.4.1 定义段属性宏&quot;">​</a></h3><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 内存属性定义宏</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#define</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> DMA_BUFFER</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">    __attribute__</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">((</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">section</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;.dma_buffer&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)))</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#define</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> CCM_RAM</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">       __attribute__</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">((</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">section</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;.ccmram&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)))</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#define</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> FAST_CODE</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">     __attribute__</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">((</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">section</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;.RamFunc&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)))</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#define</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> FLASH_CONST</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">   __attribute__</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">((</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">section</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;.rodata&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)))</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 对齐属性</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#define</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> ALIGN_4</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">       __attribute__</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">((</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">aligned</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">4</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)))</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#define</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> ALIGN_32</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">      __attribute__</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">((</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">aligned</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">32</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)))</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#define</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> ALIGN_CACHE</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">   __attribute__</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">((</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">aligned</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">32</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)))</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">  /* 缓存行对齐 */</span></span></code></pre></div><h3 id="_2-4-2-使用示例" tabindex="-1">2.4.2 使用示例 <a class="header-anchor" href="#_2-4-2-使用示例" aria-label="Permalink to &quot;2.4.2 使用示例&quot;">​</a></h3><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#include</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> &lt;stdint.h&gt;</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">/* ========== DMA缓冲区（放到DMA可访问区域） ========== */</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 对于STM32H7，放到RAM_D2（0x30000000）</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">uint8_t</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;"> uart_rx_buffer</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">[</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">256</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">] DMA_BUFFER;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">uint8_t</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;"> spi_tx_buffer</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">[</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">128</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">] DMA_BUFFER;</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">/* ========== CCMRAM变量（快速访问，DMA不可访问） ========== */</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 对于STM32F407，放到CCMRAM（0x10000000）</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">int</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;"> fast_lookup_table</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">[</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">1024</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">] CCM_RAM;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">float</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;"> pid_state</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">[</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">4</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">] CCM_RAM;</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">/* ========== 在RAM中运行的函数（加速执行） ========== */</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 适合频繁调用的关键函数</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> Critical_Control_Loop</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) FAST_CODE;</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">/* ========== Flash中的常量数据 ========== */</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">const</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> uint8_t</span><span style="--shiki-light:#E36209;--shiki-dark:#FFAB70;"> font_table</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">[</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">2048</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">] FLASH_CONST;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">const</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> char*</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> const</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> error_messages</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">[]</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> FLASH_CONST </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> {</span></span>
<span class="line"><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">    &quot;Error 1&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">,</span></span>
<span class="line"><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">    &quot;Error 2&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">,</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">};</span></span></code></pre></div><h3 id="_2-4-3-链接脚本配套定义" tabindex="-1">2.4.3 链接脚本配套定义 <a class="header-anchor" href="#_2-4-3-链接脚本配套定义" aria-label="Permalink to &quot;2.4.3 链接脚本配套定义&quot;">​</a></h3><div class="language-txt vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">txt</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>/* 在SECTIONS中添加自定义段 */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>/* DMA缓冲区段 */</span></span>
<span class="line"><span>.dma_buffer (NOLOAD) :</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    . = ALIGN(4);</span></span>
<span class="line"><span>    _sdma_buffer = .;</span></span>
<span class="line"><span>    *(.dma_buffer)</span></span>
<span class="line"><span>    *(.dma_buffer*)</span></span>
<span class="line"><span>    . = ALIGN(4);</span></span>
<span class="line"><span>    _edma_buffer = .;</span></span>
<span class="line"><span>} &gt;RAM_D2     /* STM32H7: 放到DMA1可访问的区域 */</span></span>
<span class="line"><span></span></span>
<span class="line"><span>/* CCMRAM段 */</span></span>
<span class="line"><span>.ccmram :</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    . = ALIGN(4);</span></span>
<span class="line"><span>    _sccmram = .;</span></span>
<span class="line"><span>    *(.ccmram)</span></span>
<span class="line"><span>    *(.ccmram*)</span></span>
<span class="line"><span>    . = ALIGN(4);</span></span>
<span class="line"><span>    _eccmram = .;</span></span>
<span class="line"><span>} &gt;CCMRAM AT&gt; FLASH</span></span>
<span class="line"><span></span></span>
<span class="line"><span>/* RAM函数段 */</span></span>
<span class="line"><span>.RamFunc :</span></span>
<span class="line"><span>{</span></span>
<span class="line"><span>    . = ALIGN(4);</span></span>
<span class="line"><span>    _sRamFunc = .;</span></span>
<span class="line"><span>    *(.RamFunc)</span></span>
<span class="line"><span>    *(.RamFunc*)</span></span>
<span class="line"><span>    . = ALIGN(4);</span></span>
<span class="line"><span>    _eRamFunc = .;</span></span>
<span class="line"><span>} &gt;RAM AT&gt; FLASH</span></span>
<span class="line"><span></span></span>
<span class="line"><span>_sRamFuncLoad = LOADADDR(.RamFunc);</span></span></code></pre></div><h3 id="_2-4-4-启动代码支持" tabindex="-1">2.4.4 启动代码支持 <a class="header-anchor" href="#_2-4-4-启动代码支持" aria-label="Permalink to &quot;2.4.4 启动代码支持&quot;">​</a></h3><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 如果使用自定义的初始化段，需要在启动代码中添加初始化</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 初始化CCMRAM（如果有初始值）</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> Init_CCMRAM</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) {</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    uint32_t</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> *</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">src </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> &amp;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">_sccmram_load;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    uint32_t</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> *</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">dst </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> &amp;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">_sccmram;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    while</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> (dst </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">&lt;</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> &amp;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">_eccmram) {</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">        *</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">dst</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">++</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> =</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> *</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">src</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">++</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">;</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">    }</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// 初始化RAM函数</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> Init_RamFunc</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) {</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    uint32_t</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> *</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">src </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> &amp;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">_sRamFuncLoad;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    uint32_t</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> *</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">dst </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;"> &amp;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">_sRamFunc;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    uint32_t</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> size </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> (</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">uint32_t</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">&amp;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">_eRamFunc </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">-</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> (</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">uint32_t</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">&amp;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">_sRamFunc;</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">    memcpy</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(dst, src, size);</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span></span></code></pre></div><h2 id="_2-5-内存使用分析" tabindex="-1">2.5 内存使用分析 <a class="header-anchor" href="#_2-5-内存使用分析" aria-label="Permalink to &quot;2.5 内存使用分析&quot;">​</a></h2><h3 id="_2-5-1-编译后查看内存使用" tabindex="-1">2.5.1 编译后查看内存使用 <a class="header-anchor" href="#_2-5-1-编译后查看内存使用" aria-label="Permalink to &quot;2.5.1 编译后查看内存使用&quot;">​</a></h3><div class="language-bash vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">bash</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;"># 使用arm-none-eabi-size工具</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">arm-none-eabi-size</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> output.elf</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;"># 输出示例：</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">#    text    data     bss     dec     hex filename</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">#   45678    1234    8900   55812    da04 output.elf</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;"># 字段说明：</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;"># text  = 代码 + 只读数据（Flash）</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;"># data  = 已初始化数据（Flash存储 + RAM运行）</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;"># bss   = 未初始化数据（只占RAM）</span></span></code></pre></div><h3 id="_2-5-2-内存使用计算" tabindex="-1">2.5.2 内存使用计算 <a class="header-anchor" href="#_2-5-2-内存使用计算" aria-label="Permalink to &quot;2.5.2 内存使用计算&quot;">​</a></h3><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span>Flash使用 = text + data</span></span>
<span class="line"><span>RAM使用  = data + bss + 栈 + 堆</span></span>
<span class="line"><span></span></span>
<span class="line"><span>示例：</span></span>
<span class="line"><span>Flash = 45678 + 1234 = 46912 字节 ≈ 45.8 KB</span></span>
<span class="line"><span>RAM   = 1234 + 8900 + 栈(1K) + 堆(512) ≈ 11.6 KB</span></span></code></pre></div><h3 id="_2-5-3-生成详细映射文件" tabindex="-1">2.5.3 生成详细映射文件 <a class="header-anchor" href="#_2-5-3-生成详细映射文件" aria-label="Permalink to &quot;2.5.3 生成详细映射文件&quot;">​</a></h3><div class="language-bash vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">bash</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;"># 在Makefile中添加链接选项</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">LDFLAGS</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> +=</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> -Wl,-Map=output.map</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;"># 或使用objdump</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">arm-none-eabi-objdump</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> -h</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> output.elf</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">  # 查看段信息</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">arm-none-eabi-nm</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> -S</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;"> --size-sort</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> output.elf</span><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">  # 查看符号大小</span></span></code></pre></div><hr><h2 id="附录-常见问题" tabindex="-1">附录：常见问题 <a class="header-anchor" href="#附录-常见问题" aria-label="Permalink to &quot;附录：常见问题&quot;">​</a></h2><h3 id="q1-为什么全局变量初始化为0和未初始化效果一样" tabindex="-1">Q1: 为什么全局变量初始化为0和未初始化效果一样？ <a class="header-anchor" href="#q1-为什么全局变量初始化为0和未初始化效果一样" aria-label="Permalink to &quot;Q1: 为什么全局变量初始化为0和未初始化效果一样？&quot;">​</a></h3><p>两者都放在.bss段，启动时都会被清零。但显式初始化为0更清晰。</p><h3 id="q2-ccmram有什么用" tabindex="-1">Q2: CCMRAM有什么用？ <a class="header-anchor" href="#q2-ccmram有什么用" aria-label="Permalink to &quot;Q2: CCMRAM有什么用？&quot;">​</a></h3><ul><li>存放频繁访问的数据（如PID状态）</li><li>存放不需要DMA的缓冲区</li><li>相比主RAM，访问延迟更低</li></ul><h3 id="q3-为什么字符串字面量在flash" tabindex="-1">Q3: 为什么字符串字面量在Flash？ <a class="header-anchor" href="#q3-为什么字符串字面量在flash" aria-label="Permalink to &quot;Q3: 为什么字符串字面量在Flash？&quot;">​</a></h3><p>字符串字面量是常量，放在.rodata段。这样可以节省RAM空间。</p><h3 id="q4-如何让函数在ram中运行" tabindex="-1">Q4: 如何让函数在RAM中运行？ <a class="header-anchor" href="#q4-如何让函数在ram中运行" aria-label="Permalink to &quot;Q4: 如何让函数在RAM中运行？&quot;">​</a></h3><div class="language-c vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">c</span><pre class="shiki shiki-themes github-light github-dark vp-code" tabindex="0"><code><span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">__attribute__</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">((</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">section</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;.RamFunc&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)))</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> MyFastFunction</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) {</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">    // 这个函数会被复制到RAM执行</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">    // 适合Flash访问慢或需要频繁执行的场景</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span></span></code></pre></div><h3 id="q5-h7的dma问题如何排查" tabindex="-1">Q5: H7的DMA问题如何排查？ <a class="header-anchor" href="#q5-h7的dma问题如何排查" aria-label="Permalink to &quot;Q5: H7的DMA问题如何排查？&quot;">​</a></h3><ol><li>检查缓冲区地址是否在DMA可访问区域</li><li>使用 <code>DMA_BUFFER</code> 宏将缓冲区放到正确区域</li><li>确认链接脚本中 <code>RAM_D2</code> 的定义正确</li></ol>`,100)])])}const g=a(l,[["render",t]]);export{E as __pageData,g as default};
