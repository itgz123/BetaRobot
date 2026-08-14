#
# BetaRobot Dev Container — Debian 13 (trixie) + ARM GNU 工具链 + OpenOCD
# 用法：VSCode 打开仓库根目录 → 右下角 "Reopen in Container"
# 首次构建会下载 ARM 工具链（约 300~400 MB），需要联网。

FROM debian:13

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8

# ---------- 1. 系统包 ----------
# git / cmake / ninja-build : 构建（cmake 需 >= 3.20，Debian 13 自带 3.28+）
# openssh-client / gnupg   : git 的 SSH 传输（app 子模块是 git@github.com SSH 协议）
#                            + gpg-agent 转发 / git commit 签名
# openocd                 : 烧录 / 调试 / RTT（官方包内置 cmsis-dap、swd、stm32h7x/f4x 脚本）
# telnet                  : rtt_connect 任务连接 127.0.0.1:8888
# curl / xz-utils / ca-certificates : 下载并解压 ARM 工具链 tar.xz
# python3 / libncursesw6 / zlib1g   : 工具链自带 gdb 的运行时库（见第 3 步 soname 修复）
# sudo / usbutils / file  : 非 root 授权、lsusb 验证、ELF 校验
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        git \
        openssh-client \
        gnupg \
        cmake \
        ninja-build \
        openocd \
        telnet \
        curl \
        xz-utils \
        ca-certificates \
        python3 \
        libncursesw6 \
        zlib1g \
        sudo \
        usbutils \
        file \
    && rm -rf /var/lib/apt/lists/*

# ---------- 2. ARM GNU 交叉编译工具链 ----------
# 由仓库 Release 托管（tar.xz 顶层为单一目录，--strip-components=1 后进入 /opt/arm-gnu-toolchain）
ARG ARM_GCC_VERSION=15.3.rel1
ARG ARM_GCC_URL=https://github.com/itgz123/BetaRobot/releases/download/betarobot_docker/arm-gnu-toolchain-${ARM_GCC_VERSION}-x86_64-arm-none-eabi.tar.xz

RUN set -eux; \
    curl -fsSL --retry 3 --retry-delay 5 --retry-all-errors -o /tmp/arm-gcc.tar.xz "${ARM_GCC_URL}"; \
    mkdir -p /opt/arm-gnu-toolchain; \
    tar -xJf /tmp/arm-gcc.tar.xz --strip-components=1 -C /opt/arm-gnu-toolchain; \
    rm -f /tmp/arm-gcc.tar.xz; \
    test -x /opt/arm-gnu-toolchain/bin/arm-none-eabi-gcc

ENV PATH="/opt/arm-gnu-toolchain/bin:${PATH}"

# ---------- 3. 修复工具链 gdb 的动态库 soname ----------
# 官方 x86_64 工具链的 gdb 依赖宿主的 libpython/libncurses/libreadline 特定 soname，
# 与 Debian 13 版本可能不一致（如 libpython3.12 vs 3.13）。用 ldd 探测缺失项并软链到
# 系统同族库，保证 arm-none-eabi-gdb 可启动（Cortex-Debug 依赖它）。
RUN set -eux; \
    for bin in /opt/arm-gnu-toolchain/bin/arm-none-eabi-gdb /opt/arm-gnu-toolchain/bin/arm-none-eabi-gdb-py; do \
        if [ -x "$bin" ]; then \
            ldd "$bin" 2>/dev/null | awk '/not found/{print $1}' | while read -r lib; do \
                case "$lib" in \
                    libpython3.*) \
                        src="$(ls /usr/lib/x86_64-linux-gnu/libpython3.*.so.1.0 2>/dev/null | head -n1 || true)" ;; \
                    *) \
                        pfx="$(printf '%s' "$lib" | sed -E 's/^((lib)?[A-Za-z0-9_+.-]+)\.so.*/\1/')"; \
                        src="$(ls /usr/lib/x86_64-linux-gnu/${pfx}.so.* 2>/dev/null | head -n1 || true)" ;; \
                esac; \
                if [ -n "$src" ] && [ ! -e "/usr/lib/x86_64-linux-gnu/$lib" ]; then \
                    ln -s "$(basename "$src")" "/usr/lib/x86_64-linux-gnu/$lib"; \
                    echo "linked $lib -> $(basename "$src")"; \
                fi; \
            done; \
        fi; \
    done

# ---------- 4. 非 root 开发用户 ----------
# vscode：默认 UID/GID 1000。VSCode devcontainer 的 updateRemoteUserUID:auto 会自动把
# 该用户 UID/GID 对齐到宿主用户，使容器内写入的文件归宿主所有（避免 .git/build 变 root 属主）。
# 授权 NOPASSWD sudo，供 postStartCommand 执行 chmod 放开 USB 设备节点。
ARG USER_UID=1000
ARG USER_GID=1000
RUN set -eux; \
    groupadd --gid "${USER_GID}" vscode; \
    useradd --uid "${USER_UID}" --gid "${USER_GID}" -m -s /bin/bash vscode; \
    echo "vscode ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers; \
    chown -R "${USER_UID}:${USER_GID}" /home/vscode

# ---------- 5. login shell 保留工具链 PATH ----------
# Debian 13 的 /etc/profile 会无条件重置 PATH，覆盖 Dockerfile 的 ENV PATH，
# 导致 login shell（容器终端 / ssh / devcontainer 任务）找不到 arm-none-eabi-gcc。
# /etc/profile 末尾会 source /etc/profile.d/*.sh，因此在此追加工具链路径。
RUN echo 'export PATH="/opt/arm-gnu-toolchain/bin:$PATH"' > /etc/profile.d/arm-gnu-toolchain.sh

# 构建期快速校验（失败即中止）
RUN arm-none-eabi-gcc --version \
    && arm-none-eabi-gdb --version \
    && openocd --version

WORKDIR /workspaces
