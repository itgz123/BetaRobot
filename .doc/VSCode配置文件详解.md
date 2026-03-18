# VSCode .vscode 配置文件详解

本文档详细介绍 Visual Studio Code 中 `.vscode` 文件夹下三个核心配置文件的配置项及其功能。

---

## 目录

1. [c_cpp_properties.json - C/C++ IntelliSense 配置](#1-c_cpp_propertiesjson---cc-intellisense-配置)
2. [tasks.json - 任务配置](#2-tasksjson---任务配置)
3. [launch.json - 调试配置](#3-launchjson---调试配置)

---

## 1. c_cpp_properties.json - C/C++ IntelliSense 配置

### 文件概述

`c_cpp_properties.json` 用于配置 C/C++ 扩展的 IntelliSense 功能，包括代码补全、错误检测、导航等功能。文件位于 `.vscode/c_cpp_properties.json`。

### 示例配置

```json
{
  "env": {
    "myIncludePath": ["${workspaceFolder}/include", "${workspaceFolder}/src"],
    "myDefines": ["DEBUG", "MY_FEATURE=1"]
  },
  "configurations": [
    {
      "name": "Win32",
      "compilerPath": "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.30.30705/bin/Hostx64/x64/cl.exe",
      "compilerArgs": [],
      "intelliSenseMode": "windows-msvc-x64",
      "includePath": ["${workspaceFolder}/**"],
      "defines": ["_DEBUG", "UNICODE", "_UNICODE"],
      "cStandard": "c17",
      "cppStandard": "c++20",
      "windowsSdkVersion": "10.0.19041.0"
    }
  ],
  "version": 4
}
```

### 顶层属性

| 属性名 | 类型 | 功能说明 | 示例值 |
|--------|------|----------|--------|
| `env` | Object | 定义用户自定义变量，可在 configurations 中通过 `${<var>}` 引用 | `{"myPath": ["./include"]}` |
| `configurations` | Array | 配置对象数组，为 IntelliSense 提供项目和首选项信息 | 见下方详细说明 |
| `version` | Number | 配置文件版本号，建议不要手动修改 | `4` |
| `enableConfigurationSquiggles` | Boolean | 是否在 c_cpp_properties.json 文件中报告检测到的错误 | `true` / `false` |

### configurations 配置属性

| 属性名 | 类型 | 必填 | 功能说明 | 示例值 |
|--------|------|------|----------|--------|
| `name` | String | 是 | 配置名称，`Linux`、`Mac`、`Win32` 是特殊标识符，会根据平台自动选择 | `"Win32"` |
| `compilerPath` | String | 否 | 编译器完整路径，用于更准确的 IntelliSense。设为空字符串跳过编译器查询 | `"/usr/bin/gcc"` 或 `"C:/Program Files/.../cl.exe"` |
| `compilerArgs` | Array | 否 | 传递给编译器的参数，用于修改 includes 或 defines | `["-m32", "-nostdinc++"]` |
| `intelliSenseMode` | String | 否 | IntelliSense 模式，映射到特定架构的编译器变体 | `"windows-msvc-x64"` |
| `includePath` | Array | 否 | 头文件搜索路径列表。使用 `**` 表示递归搜索 | `["${workspaceFolder}/**"]` |
| `defines` | Array | 否 | 预处理器定义列表，可用 `=` 设置值 | `["DEBUG", "VERSION=1"]` |
| `cStandard` | String | 否 | C 语言标准版本 | `"c17"`, `"gnu11"`, `"${default}"` |
| `cppStandard` | String | 否 | C++ 语言标准版本 | `"c++20"`, `"gnu++17"`, `"${default}"` |
| `configurationProvider` | String | 否 | 提供 IntelliSense 配置信息的扩展 ID | `"ms-vscode.cmake-tools"` |
| `windowsSdkVersion` | String | 否 | Windows SDK 版本（仅 Windows） | `"10.0.19041.0"` |
| `macFrameworkPath` | Array | 否 | Mac framework 头文件搜索路径（仅 macOS） | `["/System/Library/Frameworks"]` |
| `forcedInclude` | Array | 否 | 在处理任何其他字符之前强制包含的文件列表 | `["${workspaceFolder}/common.h"]` |
| `compileCommands` | String | 否 | compile_commands.json 文件的完整路径 | `"${workspaceFolder}/build/compile_commands.json"` |
| `dotConfig` | String | 否 | Kconfig 系统生成的 .config 文件路径 | `"${workspaceFolder}/.config"` |
| `mergeConfigurations` | Boolean | 否 | 是否将 include 路径、defines 和 forced includes 与配置提供程序合并 | `true` |
| `customConfigurationVariables` | Object | 否 | 自定义变量，可通过 `${cpptools:activeConfigCustomVariable}` 命令查询 | `{"myVar": "myvalue"}` |
| `browse` | Object | 否 | Tag Parser 浏览引擎配置 | 见下方 browse 属性 |

### intelliSenseMode 可选值

| 平台 | 可选值 |
|------|--------|
| Windows | `windows-msvc-x86`, `windows-msvc-x64`, `windows-msvc-arm`, `windows-msvc-arm64`, `windows-gcc-x86`, `windows-gcc-x64`, `windows-gcc-arm`, `windows-gcc-arm64`, `windows-clang-x86`, `windows-clang-x64`, `windows-clang-arm`, `windows-clang-arm64` |
| Linux | `linux-gcc-x86`, `linux-gcc-x64`, `linux-gcc-arm`, `linux-gcc-arm64`, `linux-clang-x86`, `linux-clang-x64`, `linux-clang-arm`, `linux-clang-arm64` |
| macOS | `macos-clang-x86`, `macos-clang-x64`, `macos-clang-arm`, `macos-clang-arm64`, `macos-gcc-x86`, `macos-gcc-x64`, `macos-gcc-arm`, `macos-gcc-arm64` |

### browse 属性

| 属性名 | 类型 | 功能说明 | 示例值 |
|--------|------|----------|--------|
| `path` | Array | Tag Parser 搜索头文件的路径列表，默认递归搜索，使用 `*` 表示非递归 | `["${workspaceFolder}/**"]` |
| `limitSymbolsToIncludedHeaders` | Boolean | 为 true 时，Tag Parser 只解析被源文件直接或间接包含的代码文件 | `true` |
| `databaseFilename` | String | 生成的符号数据库路径 | `"${workspaceFolder}/.vscode/browse.vc.db"` |

---

## 2. tasks.json - 任务配置

### 文件概述

`tasks.json` 用于定义和配置自动化任务，如构建、测试、运行脚本等。文件位于 `.vscode/tasks.json`。

### 示例配置

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "build",
      "type": "shell",
      "command": "gcc",
      "args": ["-g", "${file}", "-o", "${fileDirname}/${fileBasenameNoExtension}.exe"],
      "group": {
        "kind": "build",
        "isDefault": true
      },
      "problemMatcher": ["$gcc"],
      "presentation": {
        "reveal": "always",
        "panel": "shared"
      }
    }
  ]
}
```

### 顶层属性

| 属性名 | 类型 | 功能说明 | 示例值 |
|--------|------|----------|--------|
| `version` | String | 配置版本号 | `"2.0.0"` |
| `tasks` | Array | 任务描述数组 | 见下方任务属性 |
| `windows` | Object | Windows 特定任务配置 | 覆盖默认配置 |
| `osx` | Object | macOS 特定任务配置 | 覆盖默认配置 |
| `linux` | Object | Linux 特定任务配置 | 覆盖默认配置 |
| `options` | Object | 全局命令选项 | 见 CommandOptions |

### 任务属性 (TaskDescription)

| 属性名 | 类型 | 必填 | 功能说明 | 示例值 |
|--------|------|------|----------|--------|
| `label` | String | 是 | 任务名称/标签 | `"build"` |
| `type` | String | 是 | 任务类型。`shell` 在 shell 中执行，`process` 作为进程执行 | `"shell"` 或 `"process"` |
| `command` | String | 是 | 要执行的命令 | `"gcc"`, `"npm"`, `"make"` |
| `args` | Array | 否 | 传递给命令的参数 | `["-g", "main.c", "-o", "main"]` |
| `group` | String/Object | 否 | 任务所属组，可标记为默认任务 | `"build"`, `"test"` 或 `{"kind": "build", "isDefault": true}` |
| `isBackground` | Boolean | 否 | 是否为后台任务（持续运行的任务如 watch 模式） | `true` |
| `dependsOn` | String/Array | 否 | 此任务依赖的其他任务 | `"preTask"` 或 `["task1", "task2"]` |
| `dependsOrder` | String | 否 | 依赖任务的执行顺序 | `"sequence"` (顺序), `"parallel"` (并行) |
| `detail` | String | 否 | 任务的详细描述 | `"Compile the project"` |
| `icon` | Object | 否 | 任务图标配置 | `{"id": "gear", "color": "terminal.ansiBlue"}` |
| `hide` | Boolean | 否 | 是否在任务列表中隐藏此任务 | `true` |
| `presentation` | Object | 否 | 输出呈现选项 | 见 PresentationOptions |
| `problemMatcher` | String/Array/Object | 否 | 问题匹配器，用于捕获任务输出中的错误和警告 | `"$gcc"`, `"$eslint"`, `["$tsc"]` |
| `options` | Object | 否 | 命令执行选项 | 见 CommandOptions |
| `runOptions` | Object | 否 | 任务运行选项 | 见 RunOptions |
| `inputs` | Array | 否 | 任务输入变量定义 | 见下方说明 |

### CommandOptions

| 属性名 | 类型 | 功能说明 | 示例值 |
|--------|------|----------|--------|
| `cwd` | String | 执行命令的工作目录，默认为工作区根目录 | `"${workspaceFolder}/build"` |
| `env` | Object | 环境变量 | `{"PATH": "/usr/local/bin", "NODE_ENV": "development"}` |
| `shell` | Object | Shell 配置（当 type 为 shell 时） | 见下方说明 |

### shell 配置

| 属性名 | 类型 | 功能说明 | 示例值 |
|--------|------|----------|--------|
| `executable` | String | 使用的 shell | `"bash"`, `"cmd.exe"`, `"powershell.exe"` |
| `args` | Array | 传递给 shell 的参数 | `["-c"]` (bash), `["/S", "/C"]` (cmd) |

### PresentationOptions

| 属性名 | 类型 | 功能说明 | 可选值/示例 |
|--------|------|----------|-------------|
| `reveal` | String | 控制是否在界面中显示任务输出 | `"always"`, `"silent"`, `"never"` |
| `echo` | Boolean | 是否在输出中显示命令本身 | `true` / `false` |
| `focus` | Boolean | 任务面板是否获取焦点 | `true` / `false` |
| `panel` | String | 面板使用方式 | `"shared"` (共享), `"dedicated"` (专用), `"new"` (新建) |
| `showReuseMessage` | Boolean | 是否显示"终端将被重用"消息 | `true` / `false` |
| `clear` | Boolean | 运行任务前是否清空终端 | `true` / `false` |
| `group` | String | 终端分组，同组任务使用分割终端 | `"myGroup"` |

### RunOptions

| 属性名 | 类型 | 功能说明 | 可选值/示例 |
|--------|------|----------|-------------|
| `reevaluateOnRerun` | Boolean | 重新运行任务时是否重新评估变量 | `true` / `false` |
| `runOn` | String | 任务运行时机 | `"default"` (手动运行), `"folderOpen"` (打开文件夹时) |
| `instanceLimit` | Number | 同时运行的任务实例数限制 | `1`, `2` |

### ProblemMatcher

| 属性名 | 类型 | 功能说明 | 示例值 |
|--------|------|----------|--------|
| `base` | String | 基础问题匹配器名称 | `"$gcc-compile"` |
| `owner` | String | 问题所有者，通常是语言服务标识符 | `"cpp"`, `"external"` |
| `source` | String | 问题来源描述 | `"typescript"`, `"eslint"` |
| `severity` | String | 默认严重级别 | `"error"`, `"warning"`, `"info"` |
| `fileLocation` | String/Array | 文件路径解释方式 | `"absolute"`, `"relative"`, `"autodetect"` |
| `pattern` | Object/Array | 问题模式匹配定义 | 见 ProblemPattern |
| `background` | Object | 后台任务检测配置 | 见 BackgroundMatcher |

### 常用预定义 ProblemMatcher

| 匹配器名称 | 用途 |
|-----------|------|
| `$gcc` | GCC 编译器错误 |
| `$gcc-compile` | GCC 编译错误（更详细） |
| `$tsc` | TypeScript 编译器 |
| `$go` | Go 编译器 |
| `$msCompile` | MSBuild/Visual Studio 编译器 |
| `$eslint-compact` | ESLint 紧凑格式 |
| `$eslint-stylish` | ESLint stylish 格式 |
| `$python` | Python 错误 |

### inputs 输入变量

```json
{
  "version": "2.0.0",
  "inputs": [
    {
      "id": "buildMode",
      "type": "pickString",
      "description": "选择构建模式",
      "options": ["debug", "release"],
      "default": "debug"
    }
  ],
  "tasks": [
    {
      "label": "build",
      "command": "cmake",
      "args": ["--build", ".", "--config", "${input:buildMode}"]
    }
  ]
}
```

| 属性名 | 类型 | 功能说明 |
|--------|------|----------|
| `id` | String | 输入变量标识符 |
| `type` | String | 输入类型：`promptString`（字符串输入）、`pickString`（选择列表）、`command`（命令结果） |
| `description` | String | 输入提示描述 |
| `default` | String | 默认值 |
| `options` | Array | 选项列表（pickString 类型） |
| `password` | Boolean | 是否为密码输入（promptString 类型） |

---

## 3. launch.json - 调试配置

### 文件概述

`launch.json` 用于配置调试器，定义如何启动和调试程序。文件位于 `.vscode/launch.json`。

### 示例配置

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug C++",
      "type": "cppdbg",
      "request": "launch",
      "program": "${fileDirname}/${fileBasenameNoExtension}.exe",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "environment": [],
      "externalConsole": false,
      "MIMode": "gdb",
      "miDebuggerPath": "C:/mingw64/bin/gdb.exe",
      "preLaunchTask": "build",
      "setupCommands": [
        {
          "description": "为 gdb 启用整齐打印",
          "text": "-enable-pretty-printing",
          "ignoreFailures": true
        }
      ]
    }
  ]
}
```

### 顶层属性

| 属性名 | 类型 | 功能说明 | 示例值 |
|--------|------|----------|--------|
| `version` | String | 配置版本号 | `"0.2.0"` |
| `configurations` | Array | 调试配置数组 | 见下方配置属性 |
| `compounds` | Array | 复合启动配置（多目标调试） | 见下方说明 |

### 基础配置属性（所有调试器通用）

| 属性名 | 类型 | 必填 | 功能说明 | 示例值 |
|--------|------|------|----------|--------|
| `name` | String | 是 | 配置名称，显示在调试下拉菜单中 | `"Debug C++"` |
| `type` | String | 是 | 调试器类型 | `"cppdbg"`, `"node"`, `"python"`, `"go"` |
| `request` | String | 是 | 请求类型：`launch`（启动）或 `attach`（附加） | `"launch"` / `"attach"` |
| `preLaunchTask` | String | 否 | 调试前运行的任务（tasks.json 中的任务标签） | `"build"` |
| `postDebugTask` | String | 否 | 调试结束后运行的任务 | `"cleanup"` |
| `internalConsoleOptions` | String | 否 | 调试控制台选项 | `"neverOpen"`, `"openOnSessionStart"`, `"openOnFirstSessionStart"` |

### launch 模式属性

| 属性名 | 类型 | 功能说明 | 示例值 |
|--------|------|----------|--------|
| `program` | String | 要调试的程序路径（绝对路径） | `"${workspaceFolder}/main.exe"` |
| `args` | Array | 传递给程序的命令行参数 | `["--verbose", "input.txt"]` |
| `cwd` | String | 程序工作目录 | `"${workspaceFolder}"` |
| `env` | Object | 环境变量 | `{"NODE_ENV": "development", "PORT": "3000"}` |
| `envFile` | String | 包含环境变量的文件路径 | `"${workspaceFolder}/.env"` |
| `stopOnEntry` | Boolean | 程序启动时是否立即暂停 | `true` / `false` |
| `console` | String | 控制台类型 | `"internalConsole"`, `"integratedTerminal"`, `"externalTerminal"` |
| `runtimeExecutable` | String | 运行时可执行文件路径 | `"npm"`, `"node"` |
| `runtimeArgs` | Array | 传递给运行时的参数 | `["run-script", "start"]` |

### attach 模式属性

| 属性名 | 类型 | 功能说明 | 示例值 |
|--------|------|----------|--------|
| `processId` | String/Number | 要附加的进程 ID，可使用 `${command:PickProcess}` 选择 | `"${command:PickProcess}"` |
| `port` | Number | 调试端口 | `9229` (Node.js 默认) |
| `address` | String | 调试端口地址 | `"localhost"`, `"192.168.1.100"` |
| `restart` | Boolean/Number | 是否在连接断开后自动重启 | `true`, `5` (重试次数) |
| `continueOnAttach` | Boolean | 如果进程在附加时暂停，是否继续执行 | `true` / `false` |

### C/C++ 调试专用属性 (cppdbg)

| 属性名 | 类型 | 功能说明 | 示例值 |
|--------|------|----------|--------|
| `MIMode` | String | MI 调试器模式 | `"gdb"`, `"lldb"` |
| `miDebuggerPath` | String | MI 调试器路径 | `"C:/mingw64/bin/gdb.exe"` |
| `miDebuggerArgs` | String | 传递给 MI 调试器的参数 | `"--quiet"` |
| `setupCommands` | Array | 启动时发送给调试器的命令 | 见示例 |
| `externalConsole` | Boolean | 是否使用外部控制台 | `true` / `false` |
| `visualizerFile` | String | .natvis 文件路径（自定义数据类型可视化） | `"${workspaceFolder}/my.natvis"` |
| `showDisplayString` | Boolean | 是否显示自定义显示字符串 | `true` |
| `symbolLoadPath` | String | 符号文件加载路径 | `"${workspaceFolder}/symbols"` |
| `sourceFileMap` | Object | 源文件路径映射 | `{"/build": "${workspaceFolder}"}` |

### Node.js 调试专用属性

| 属性名 | 类型 | 功能说明 | 示例值 |
|--------|------|----------|--------|
| `outputCapture` | String | 输出捕获方式 | `"std"` (捕获 stdout/stderr) |
| `runtimeVersion` | String | Node.js 版本（使用 nvm/nvs 管理时） | `"14"`, `"16.14.0"` |
| `sourceMaps` | Boolean | 是否启用 source maps | `true` (默认) |
| `outFiles` | Array | 生成的 JavaScript 文件 glob 模式 | `["${workspaceFolder}/dist/**/*.js"]` |
| `resolveSourceMapLocations` | Array | 解析 source map 的位置 | `["${workspaceFolder}/**"]` |
| `smartStep` | Boolean | 自动跳过未映射到源文件的代码 | `true` |
| `skipFiles` | Array | 调试时跳过的文件 | `["<node_internals>/**"]` |
| `localRoot` | String | 本地代码根目录（远程调试） | `"${workspaceFolder}"` |
| `remoteRoot` | String | 远程代码根目录（远程调试） | `"/home/user/project"` |
| `trace` | Boolean/String | 启用诊断输出 | `true`, `"verbose"` |

### Python 调试专用属性

| 属性名 | 类型 | 功能说明 | 示例值 |
|--------|------|----------|--------|
| `pythonPath` | String | Python 解释器路径 | `"${command:python.interpreterPath}"` |
| `module` | String | 要运行的模块名 | `"mymodule"` |
| `django` | Boolean | 是否启用 Django 调试 | `true` |
| `jinja` | Boolean | 是否启用 Jinja 模板调试 | `true` |
| `justMyCode` | Boolean | 是否只调试用户代码 | `true` |
| `subProcess` | Boolean | 是否启用子进程调试 | `true` |

### compound 复合配置

用于同时启动多个调试配置（多目标调试）：

```json
{
  "version": "0.2.0",
  "compounds": [
    {
      "name": "Server + Client",
      "configurations": ["Server", "Client"],
      "preLaunchTask": "buildAll",
      "stopAll": true
    }
  ],
  "configurations": [
    {
      "name": "Server",
      "type": "node",
      "request": "launch",
      "program": "${workspaceFolder}/server.js"
    },
    {
      "name": "Client",
      "type": "chrome",
      "request": "launch",
      "url": "http://localhost:3000"
    }
  ]
}
```

| 属性名 | 类型 | 功能说明 |
|--------|------|----------|
| `name` | String | 复合配置名称 |
| `configurations` | Array | 要同时启动的配置名称数组 |
| `preLaunchTask` | String | 启动前运行的任务 |
| `stopAll` | Boolean | 停止时是否停止所有配置 |

---

## 常用变量参考

以下变量可在 `tasks.json` 和 `launch.json` 中使用：

| 变量 | 说明 |
|------|------|
| `${workspaceFolder}` | 工作区文件夹路径 |
| `${workspaceFolderBasename}` | 工作区文件夹名称 |
| `${file}` | 当前文件完整路径 |
| `${fileWorkspaceFolder}` | 当前文件所在的工作区文件夹 |
| `${relativeFile}` | 相对于工作区的当前文件路径 |
| `${relativeFileDirname}` | 当前文件所在目录的相对路径 |
| `${fileBasename}` | 当前文件完整名称（含扩展名） |
| `${fileBasenameNoExtension}` | 当前文件名称（不含扩展名） |
| `${fileDirname}` | 当前文件所在目录的完整路径 |
| `${fileExtname}` | 当前文件扩展名 |
| `${cwd}` | 任务启动时的工作目录 |
| `${lineNumber}` | 当前选中行号 |
| `${selectedText}` | 当前选中文本 |
| `${execPath}` | VS Code 可执行文件路径 |
| `${defaultBuildTask}` | 默认构建任务名称 |
| `${pathSeparator}` | 路径分隔符（/ 或 \\） |
| `${input:<id>}` | 引用 inputs 中定义的输入变量 |
| `${command:<commandId>}` | 引用命令结果 |

---

## 参考链接

- [VS Code C/C++ Extension Documentation](https://code.visualstudio.com/docs/cpp/c-cpp-properties-schema-reference)
- [VS Code Tasks Documentation](https://code.visualstudio.com/docs/editor/tasks)
- [VS Code Debugging Documentation](https://code.visualstudio.com/docs/editor/debugging)
- [Node.js Debugging in VS Code](https://code.visualstudio.com/docs/nodejs/nodejs-debugging)
