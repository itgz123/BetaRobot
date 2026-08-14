# 二：容器开发（Linux）—— VSCode Dev Container（推荐）

> 适用 Linux 桌面：Docker + VSCode Dev Container 提供开箱即用的交叉编译/烧录/调试环境，
> 无需手动装 msys2、工具链、openocd。镜像基于 Debian 13，工具链自动配置为
> `arm-gnu-toolchain-15.3.rel1-x86_64-arm-none-eabi`。

### 前置条件

- Linux（Debian/Ubuntu/Fedora 均可）、已安装 Docker Engine
- VSCode 安装 "Dev Containers" 扩展（`ms-vscode-remote.remote-containers`）
- 本机 SSH 已配置可访问 GitHub（用于拉取 app 子模块），或已启动 ssh-agent 并加载密钥

### 使用步骤

1. 克隆仓库：`git clone --recurse-submodules git@github.com:itgz123/BetaRobot.git`
2. VSCode 打开仓库根目录 → 右下角弹窗选 "Reopen in Container"（或命令面板：Dev Containers: Reopen in Container）
3. 首次构建自动完成：apt 安装工具 + 下载 ARM 工具链 15.3.rel1 到 `/opt/arm-gnu-toolchain`（需联网，约 300~400 MB）
4. 自动初始化：复制 `user_cfg.h.example` → `user_cfg.h`，并执行 `git submodule update --init --recursive`
5. 之后操作与 Windows 完全一致：`Ctrl+Shift+B` 编译；任务面板选 "download dap" / "DAP-link RTT"；`F5` 调试

### 容器内常用命令

```bash
arm-none-eabi-gcc --version        # 工具链版本
cmake --preset Debug               # 配置
cmake --build build/Debug -j24     # 编译（生成 BetaRobot.elf/.hex/.bin）
cmake --build build/Debug --target download_dap   # 用 DAPlink 烧录
cmake --build build/Debug --target rtt_connect    # 启动 RTT 日志（telnet 8888）
```

### USB 透传（DAPlink）

- 容器以 `--privileged` 运行，可访问宿主 USB 设备；每次启动自动执行 `chmod 666 /dev/bus/usb/*` 与 `/dev/hidraw*`
- 运行中插入 DAPlink 时，在容器终端手动执行一次：
  `sudo chmod 666 /dev/bus/usb/*/* /dev/hidraw*`
- 验证：`lsusb` 应能看到 CMSIS-DAP 设备；`ls -l /dev/hidraw*` 应显示 `crw-rw-rw-`（666）

### 常见问题

- 容器内 CMake 报 `CMakeCache.txt ... is different than the directory ...`：
  这是宿主机（或其他环境）已用不同路径构建过 `build/` 目录导致。执行
  `cmake -E rm -rf build/Debug` 后重新 `cmake --preset Debug` 即可
- 子模块拉取失败（ssh-agent 未转发）：容器内执行
  `git config --global url."https://github.com/".insteadOf "git@github.com:"` 后重新执行
  `git submodule update --init --recursive`
- 想用文档站点任务：容器内 `sudo apt-get install -y nodejs npm` 后运行 "start docs"
- 非 1000 宿主 UID：改 `.devcontainer/Dockerfile` 顶部 `USER_UID`/`USER_GID`（或依赖 `updateRemoteUserUID` 自动对齐）
