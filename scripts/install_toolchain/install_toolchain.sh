#!/usr/bin/env bash
# =============================================================================
# install_toolchain.sh
#
# 在任意 Linux 发行版上装齐 BetaRobot 的构建 / 调试工具链。
#
# 装什么
#   git / cmake / ninja / openocd —— 四项必需，全部走发行版包管理器。
#   另加若干可选件（clang-format、curl/xz/ca-certificates、ssh、usbutils、file）。
#
# 不装什么
#   arm-none-eabi 交叉工具链只检测、不安装。它是 ARM 官方发布的独立压缩包
#   （可执行文件前缀 arm-none-eabi-），发行版仓库里的同名包版本通常偏旧。
#   缺失时打印下载与 PATH 配置指引，与 .devcontainer/Dockerfile 的来源一致。
#
# 行为约定
#   1. 每个依赖先检测、只装缺的，已存在的一律跳过（幂等，可反复执行）。
#   2. 可选件安装失败只警告，不影响后续构建。
#   3. 只管系统包。Python / 编辑器插件不在范围内。
#   4. 本脚本只针对 Linux。Windows 走 msys2，见 docs/user_docs/使用文档.md。
#
# 用法
#   bash scripts/install_toolchain/install_toolchain.sh [选项]
#
# 选项
#   -n, --check-only   只检测，不安装任何东西
#   -y, --yes          不询问，直接安装
#   -h, --help         显示本帮助
#
# 支持的发行版族
#   debian  Debian / Ubuntu / Mint / Pop!_OS / Raspberry Pi OS
#   fedora  Fedora / RHEL / CentOS / Rocky / AlmaLinux
#   arch    Arch / Manjaro / EndeavourOS
#   suse    openSUSE / SLES
#   alpine  Alpine
#
# 注意：可选件的包名各发行版差异最大（尤其 clang-format、ssh），表格里是
#       尽力而为的写法。装不上只警告，请按发行版实际包名手动补。
# =============================================================================

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

ASSUME_YES=0
CHECK_ONLY=0

# CMakePresets.json 是 schema v6、cmake_minimum_required(VERSION 3.23)，
# 低于这个版本的 cmake 会直接拒绝配置。
CMAKE_MIN_MAJOR=3
CMAKE_MIN_MINOR=23

# ARM 交叉工具链的官方下载页。2026-10 起从 developer.arm.com 迁到 GitLab，
# 旧地址 .../downloads/-/arm-gnu-toolchain-downloads 现在只做 301 跳转。
# 各版本放在 releases/<版本> 分支下，产物直链形如下方 URL（命名规则没变）。
ARM_GCC_PAGE="https://gitlab.arm.com/tooling/gnu-toolchains-for-arm"
ARM_GCC_UPSTREAM_VERSION="${ARM_GCC_UPSTREAM_VERSION:-15.3.rel2}"
ARM_GCC_UPSTREAM_URL="https://gitlab.arm.com/api/v4/projects/tooling%2Fgnu-toolchains-for-arm/packages/generic/gnu-toolchain/${ARM_GCC_UPSTREAM_VERSION}/arm-gnu-toolchain-${ARM_GCC_UPSTREAM_VERSION}-x86_64-arm-none-eabi.tar.xz"

# 本仓库 devcontainer 用的 ARM 工具链包（Debian 13 上验证过，见 Dockerfile）
ARM_GCC_VERSION=15.3.rel1
ARM_GCC_URL="https://github.com/itgz123/BetaRobot/releases/download/betarobot_docker/arm-gnu-toolchain-${ARM_GCC_VERSION}-x86_64-arm-none-eabi.tar.xz"

# -----------------------------------------------------------------------------
# 日志
# -----------------------------------------------------------------------------
if [ -t 1 ]; then
	C_RED=$(printf '\033[31m')
	C_GRN=$(printf '\033[32m')
	C_YEL=$(printf '\033[33m')
	C_CYA=$(printf '\033[36m')
	C_BLD=$(printf '\033[1m')
	C_RST=$(printf '\033[0m')
else
	C_RED='' ; C_GRN='' ; C_YEL='' ; C_CYA='' ; C_BLD='' ; C_RST=''
fi

info()  { printf '%s[INFO]%s %s\n' "$C_CYA" "$C_RST" "$*" ; }
ok()    { printf '%s[ OK ]%s %s\n' "$C_GRN" "$C_RST" "$*" ; }
skip()  { printf '%s[SKIP]%s %s\n' "$C_GRN" "$C_RST" "$*" ; }
warn()  { printf '%s[WARN]%s %s\n' "$C_YEL" "$C_RST" "$*" >&2 ; }
err()   { printf '%s[FAIL]%s %s\n' "$C_RED" "$C_RST" "$*" >&2 ; }
die()   { err "$*" ; exit 1 ; }
title() { printf '\n%s%s==> %s%s\n' "$C_BLD" "$C_CYA" "$*" "$C_RST" ; }

usage() {
	# 打印脚本头部的注释块（跳过 shebang，遇到第一条非注释即停）
	awk 'NR==1{next} /^#/{sub(/^# ?/,""); print; next} {exit}' "$0"
	exit 0
}

# -----------------------------------------------------------------------------
# 参数解析
# -----------------------------------------------------------------------------
while [ $# -gt 0 ]; do
	case "$1" in
		-n|--check-only)  CHECK_ONLY=1 ;;
		-y|--yes)         ASSUME_YES=1 ;;
		-h|--help)        usage ;;
		*)                die "未知参数: $1（用 --help 查看用法）" ;;
	esac
	shift
done

confirm() {
	[ "$ASSUME_YES" -eq 1 ] && return 0
	[ -t 0 ] || return 0          # 非交互环境直接放行
	printf '%s%s [y/N]%s ' "$C_BLD" "$1" "$C_RST"
	read -r _ans || return 1
	case "$_ans" in
		[yY]|[yY][eE][sS]) return 0 ;;
		*) return 1 ;;
	esac
}

# -----------------------------------------------------------------------------
# 发行版 / 包管理器探测
# -----------------------------------------------------------------------------
FAMILY=unknown
PM=''
SUDO=''
ARCH=$(uname -m)

detect_family() {
	_d_id='' ; _d_like=''
	if [ -r /etc/os-release ]; then
		_d_id=$(    . /etc/os-release 2>/dev/null; printf '%s' "${ID:-}" )
		_d_like=$(  . /etc/os-release 2>/dev/null; printf '%s' "${ID_LIKE:-}" )
	fi
	_keys=" $_d_id $_d_like "

	case "$_keys" in
		*" debian "*|*" ubuntu "*|*" linuxmint "*|*" raspbian "*|*" pop "*) FAMILY=debian ;;
		*" fedora "*|*" rhel "*|*" centos "*|*" rocky "*|*" almalinux "*)   FAMILY=fedora ;;
		*" arch "*|*" manjaro "*|*" endeavouros "*)                         FAMILY=arch ;;
		*" opensuse "*|*" suse "*|*" sles "*)                               FAMILY=suse ;;
		*" alpine "*)                                                       FAMILY=alpine ;;
		*)
			# os-release 认不出来时，退回"哪个包管理器在就用哪个"
			if   command -v apt-get >/dev/null 2>&1 ; then FAMILY=debian
			elif command -v dnf     >/dev/null 2>&1 ; then FAMILY=fedora
			elif command -v pacman  >/dev/null 2>&1 ; then FAMILY=arch
			elif command -v zypper  >/dev/null 2>&1 ; then FAMILY=suse
			elif command -v apk     >/dev/null 2>&1 ; then FAMILY=alpine
			fi
			;;
	esac
}

detect_pm() {
	case "$FAMILY" in
		debian) PM=apt-get ;;
		fedora) PM=$(command -v dnf >/dev/null 2>&1 && echo dnf || echo yum) ;;
		arch)   PM=pacman ;;
		suse)   PM=zypper ;;
		alpine) PM=apk ;;
		*)      PM='' ;;
	esac
}

detect_sudo() {
	if [ "$(id -u)" -eq 0 ]; then
		SUDO=''
	elif command -v sudo >/dev/null 2>&1; then
		SUDO='sudo'
	elif command -v doas >/dev/null 2>&1; then
		SUDO='doas'
	else
		die "需要 root 权限安装系统包，但 sudo/doas 都不可用。请以 root 运行本脚本。"
	fi
}

pm_refresh() {
	[ "$CHECK_ONLY" -eq 1 ] && return 0
	info "刷新 $PM 软件源索引"
	case "$FAMILY" in
		debian) $SUDO apt-get update ;;
		fedora) : ;;                                       # dnf 会自动按需刷新
		arch)   $SUDO pacman -Sy --noconfirm ;;            # 仅同步索引，不升级系统
		suse)   $SUDO zypper --non-interactive --gpg-auto-import-keys refresh ;;
		alpine) $SUDO apk update ;;
	esac
}

pm_install() {
	[ $# -gt 0 ] || return 0
	case "$FAMILY" in
		debian) $SUDO apt-get install -y --no-install-recommends "$@" ;;
		fedora) $SUDO "$PM" install -y "$@" ;;
		arch)   $SUDO pacman -S --needed --noconfirm "$@" ;;
		suse)   $SUDO zypper --non-interactive install --no-recommends "$@" ;;
		alpine) $SUDO apk add --no-cache "$@" ;;
		*)      return 1 ;;
	esac
}

is_pkg_installed() {
	case "$FAMILY" in
		debian) dpkg-query -W -f '${Status}' "$1" 2>/dev/null | grep -q 'install ok installed' ;;
		fedora) rpm -q "$1" >/dev/null 2>&1 ;;
		arch)   pacman -Q "$1" >/dev/null 2>&1 ;;
		suse)   rpm -q "$1" >/dev/null 2>&1 ;;
		alpine) apk info -e "$1" >/dev/null 2>&1 ;;
		*)      return 1 ;;
	esac
}

# 依赖是否已满足：优先看命令在不在，其次问包管理器
# 检查命令为 '-' 表示没有对应命令（如 ca-certificates），只看包管理器的已装查询。
# 注意不能把首字段留空：read 按 '|' 切分时首字段空会让 _pkg 也变空，整行被跳过。
dep_present() {
	if [ -n "$1" ] && [ "$1" != '-' ] && command -v "$1" >/dev/null 2>&1; then
		return 0
	fi
	is_pkg_installed "$2"
}

# 去重（输入是空格分隔的字符串）
dedupe() {
	[ $# -gt 0 ] || return 0
	printf '%s\n' "$@" | awk 'NF && !seen[$0]++' | tr '\n' ' '
}

# -----------------------------------------------------------------------------
# 依赖清单
#   每行格式：检查命令|包名|说明|级别
#   级别 req = 必需（装了才能构建/下载）；opt = 可选（失败只警告）
#   检查命令写 '-' 表示没有对应命令，改用包管理器的已装查询。
#
#   ninja 的包名：Debian/Fedora 叫 ninja-build，Arch/SUSE/Alpine 叫 ninja。
#   ARM 交叉工具链**不在**这张表里，它只检测不装，见 check_arm_toolchain。
# -----------------------------------------------------------------------------
deps_for() {
	case "$1" in
	debian)
		cat <<'EOD'
git|git|Git 版本控制（主仓库 + 嵌套的 app 仓库）|req
cmake|cmake|CMake 构建系统（preset 要求 >= 3.23）|req
ninja|ninja-build|Ninja 生成器（preset 用 Ninja Multi-Config）|req
openocd|openocd|OpenOCD（DAPlink 下载 / Cortex-Debug 调试）|req
clang-format|clang-format|clang-format（VSCode "format" 任务）|opt
ssh|openssh-client|SSH 客户端（app 仓库走 git@github.com）|opt
curl|curl|下载 ARM 工具链|opt
xz|xz-utils|解压 ARM 工具链 tar.xz|opt
lsusb|usbutils|lsusb（确认调试器被识别）|opt
file|file|file（ELF / 下载包类型校验）|opt
-|ca-certificates|CA 根证书（HTTPS 下载用）|opt
EOD
		;;
	fedora)
		cat <<'EOD'
git|git|Git 版本控制（主仓库 + 嵌套的 app 仓库）|req
cmake|cmake|CMake 构建系统（preset 要求 >= 3.23）|req
ninja|ninja-build|Ninja 生成器（preset 用 Ninja Multi-Config）|req
openocd|openocd|OpenOCD（DAPlink 下载 / Cortex-Debug 调试）|req
clang-format|clang-tools-extra|clang-format（VSCode "format" 任务）|opt
ssh|openssh-clients|SSH 客户端（app 仓库走 git@github.com）|opt
curl|curl|下载 ARM 工具链|opt
xz|xz|解压 ARM 工具链 tar.xz|opt
lsusb|usbutils|lsusb（确认调试器被识别）|opt
file|file|file（ELF / 下载包类型校验）|opt
-|ca-certificates|CA 根证书（HTTPS 下载用）|opt
EOD
		;;
	arch)
		cat <<'EOD'
git|git|Git 版本控制（主仓库 + 嵌套的 app 仓库）|req
cmake|cmake|CMake 构建系统（preset 要求 >= 3.23）|req
ninja|ninja|Ninja 生成器（preset 用 Ninja Multi-Config）|req
openocd|openocd|OpenOCD（DAPlink 下载 / Cortex-Debug 调试）|req
clang-format|clang|clang-format（VSCode "format" 任务）|opt
ssh|openssh|SSH 客户端（app 仓库走 git@github.com）|opt
curl|curl|下载 ARM 工具链|opt
xz|xz|解压 ARM 工具链 tar.xz|opt
lsusb|usbutils|lsusb（确认调试器被识别）|opt
file|file|file（ELF / 下载包类型校验）|opt
-|ca-certificates|CA 根证书（HTTPS 下载用）|opt
EOD
		;;
	suse)
		cat <<'EOD'
git|git|Git 版本控制（主仓库 + 嵌套的 app 仓库）|req
cmake|cmake|CMake 构建系统（preset 要求 >= 3.23）|req
ninja|ninja|Ninja 生成器（preset 用 Ninja Multi-Config）|req
openocd|openocd|OpenOCD（DAPlink 下载 / Cortex-Debug 调试）|req
clang-format|clang-tools|clang-format（VSCode "format" 任务）|opt
ssh|openssh-clients|SSH 客户端（app 仓库走 git@github.com）|opt
curl|curl|下载 ARM 工具链|opt
xz|xz|解压 ARM 工具链 tar.xz|opt
lsusb|usbutils|lsusb（确认调试器被识别）|opt
file|file|file（ELF / 下载包类型校验）|opt
-|ca-certificates|CA 根证书（HTTPS 下载用）|opt
EOD
		;;
	alpine)
		cat <<'EOD'
git|git|Git 版本控制（主仓库 + 嵌套的 app 仓库）|req
cmake|cmake|CMake 构建系统（preset 要求 >= 3.23）|req
ninja|ninja|Ninja 生成器（preset 用 Ninja Multi-Config）|req
openocd|openocd|OpenOCD（DAPlink 下载 / Cortex-Debug 调试）|req
clang-format|clang-extra-tools|clang-format（VSCode "format" 任务）|opt
ssh|openssh-client|SSH 客户端（app 仓库走 git@github.com）|opt
curl|curl|下载 ARM 工具链|opt
xz|xz|解压 ARM 工具链 tar.xz|opt
lsusb|usbutils|lsusb（确认调试器被识别）|opt
file|file|file（ELF / 下载包类型校验）|opt
-|ca-certificates|CA 根证书（HTTPS 下载用）|opt
EOD
		;;
	esac
}

# -----------------------------------------------------------------------------
# cmake 版本（不是"装了就行"：preset 是 schema v6，低于 3.23 直接配置失败）
# -----------------------------------------------------------------------------
cmake_ok() {
	command -v cmake >/dev/null 2>&1 || return 1
	_v=$(cmake --version 2>/dev/null | sed -n '1s/.*version[[:space:]]*//p')
	[ -n "$_v" ] || return 1
	_major=${_v%%.*}
	_rest=${_v#*.}
	_minor=${_rest%%.*}
	# 版本号里可能带 -rc1 之类的后缀；非数字时按"不确定"处理，放行
	case "$_major" in ''|*[!0-9]*) return 0 ;; esac
	case "$_minor" in ''|*[!0-9]*) return 0 ;; esac
	[ "$_major" -gt "$CMAKE_MIN_MAJOR" ] && return 0
	[ "$_major" -eq "$CMAKE_MIN_MAJOR" ] && [ "$_minor" -ge "$CMAKE_MIN_MINOR" ]
}

# -----------------------------------------------------------------------------
# ARM 交叉工具链（只检测，不安装）
# -----------------------------------------------------------------------------
find_arm_gcc() {
	# 1) PATH 里已经有了最省事
	if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
		command -v arm-none-eabi-gcc
		return 0
	fi
	# 2) 退回到常见安装位置（devcontainer 落在 /opt/arm-gnu-toolchain；
	#    官方 tar.xz 解压出来是带版本号、带 target 后缀的目录，故再配一条通配）
	for _d in /opt/arm-gnu-toolchain/bin \
	          /opt/arm-gnu-toolchain*/bin \
	          "$HOME"/arm-gnu-toolchain*/bin \
	          /usr/local/arm-gnu-toolchain*/bin \
	          "$HOME"/.local/arm-gnu-toolchain*/bin \
	          /opt/arm-none-eabi*/bin; do
		if [ -x "$_d/arm-none-eabi-gcc" ]; then
			printf '%s' "$_d/arm-none-eabi-gcc"
			return 0
		fi
	done
	return 1
}

# gdb 是 Cortex-Debug 依赖的，它常有"装了但起不来"的动态库问题，单独查一下
check_arm_gdb() {
	_gdb="$(_command_gdb "$1")"
	if [ -z "$_gdb" ]; then
		warn "没找到 arm-none-eabi-gdb —— VSCode 的 Cortex-Debug 需要它，只有 gcc 无法调试。"
		return 0
	fi
	if [ -x "$_gdb" ]; then
		_missing=$(ldd "$_gdb" 2>/dev/null | awk '/not found/{print $1}')
		if [ -n "$_missing" ]; then
			warn "arm-none-eabi-gdb 缺动态库: $(printf '%s ' $_missing)"
			warn "  官方 x86_64 包的 gdb 依赖的 so 名可能和你的发行版对不上。"
			warn "  先装齐这些运行时库（Debian 例：libncursesw6 zlib1g libpython3），"
			warn "  仍缺时把同族库软链到它要的名字上（做法见 .devcontainer/Dockerfile 第 3 步）。"
			return 0
		fi
	fi
	ok "arm-none-eabi-gdb 可用: $_gdb"
}

_command_gdb() {
	if [ -x "$1/arm-none-eabi-gdb" ]; then
		printf '%s' "$1/arm-none-eabi-gdb"
	elif command -v arm-none-eabi-gdb >/dev/null 2>&1; then
		command -v arm-none-eabi-gdb
	fi
}

check_arm_toolchain() {
	title "检查 ARM 交叉工具链（只检测，不安装）"

	if _gcc=$(find_arm_gcc); then
		_bin_dir=$(dirname "$_gcc")
		ok "已找到 ARM 工具链: $_gcc"
		info "  $("$_gcc" --version 2>/dev/null | sed -n '1p')"
		# 只有在 PATH 里才算真正可用，否则编译时 cmake 找不到 arm-none-eabi-gcc
		if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
			warn "但它不在 PATH 里，cmake 配置时会报找不到编译器。加进 shell 配置："
			warn "    export PATH=\"$_bin_dir:\$PATH\""
			warn "  （login shell 还要落到 /etc/profile.d/*.sh，见 Dockerfile 第 5 步）"
			ARM_NOT_IN_PATH=1
		fi
		check_arm_gdb "$_bin_dir"
	else
		warn "未检测到 arm-none-eabi 交叉工具链 —— 它是交叉编译器（arm-none-eabi-gcc）"
		warn "的来源，缺它无法编译任何 app。本脚本按约定不自动安装，三种做法："
		warn "  1) 官方下载（推荐，版本任意）：$ARM_GCC_PAGE"
		warn "     各版本在 releases/<版本> 分支下（如 releases/$ARM_GCC_UPSTREAM_VERSION），"
		warn "     选 \"x86_64 Linux hosted cross toolchains\" 里文件名带 arm-none-eabi 的 tar.xz，"
		warn "     直链形如：$ARM_GCC_UPSTREAM_URL"
		warn "     解压后把 bin/ 加入 PATH（与 devcontainer 的落点保持一致）："
		warn "       mkdir -p /opt/arm-gnu-toolchain"
		warn "       tar -xJf arm-gnu-toolchain-*.tar.xz --strip-components=1 -C /opt/arm-gnu-toolchain"
		warn "       export PATH=\"/opt/arm-gnu-toolchain/bin:\$PATH\""
		warn "  2) 和本仓库 devcontainer 同源的预打包（Debian 13 上验证过）："
		warn "       $ARM_GCC_URL"
		warn "  3) 发行版仓库里的同名包（版本通常偏旧，可能低于工程要求）："
		warn "       Debian/Ubuntu: gcc-arm-none-eabi   Fedora: arm-none-eabi-gcc-cs"
		warn "       Arch: arm-none-eabi-gcc            SUSE: cross-arm-none-gcc"
		warn "       Alpine: gcc-arm-none-eabi"
		ARM_MISSING=1
	fi
}

# -----------------------------------------------------------------------------
# 主流程
# -----------------------------------------------------------------------------
detect_family
detect_pm
detect_sudo

title "环境概览"
info "发行版族 : $FAMILY"
info "包管理器 : ${PM:-未知}"
info "CPU 架构 : $ARCH"
info "脚本目录 : $SCRIPT_DIR"
[ "$CHECK_ONLY" -eq 1 ] && warn "当前为 --check-only 模式，不会做任何改动"

if [ -z "$PM" ]; then
	warn "无法识别包管理器，系统依赖需要你手动安装。"
	warn "必需的四项：git / cmake(>=3.23) / ninja / openocd"
fi

# ---- 逐个检测依赖 ----------------------------------------------------------
title "检测系统依赖"

MISSING_REQ=''
MISSING_OPT=''
HAVE_CNT=0
HARD_FAIL=0

while IFS='|' read -r _check _pkg _desc _level; do
	[ -n "${_pkg:-}" ] || continue
	if dep_present "$_check" "$_pkg"; then
		HAVE_CNT=$((HAVE_CNT + 1))
		skip "$_desc  [$_pkg]"
	else
		if [ "$_level" = opt ]; then
			warn "可选缺失: $_desc  [$_pkg]"
			MISSING_OPT="$MISSING_OPT $_pkg"
		else
			warn "必需缺失: $_desc  [$_pkg]"
			MISSING_REQ="$MISSING_REQ $_pkg"
		fi
	fi
done <<EOD
$(deps_for "$FAMILY")
EOD

MISSING_REQ=$(dedupe $MISSING_REQ)
MISSING_OPT=$(dedupe $MISSING_OPT)

info "已满足 $HAVE_CNT 项；缺少必需 ${MISSING_REQ:-无}；缺少可选 ${MISSING_OPT:-无}"

# ---- 安装 ------------------------------------------------------------------
if [ "$CHECK_ONLY" -eq 1 ]; then
	info "（--check-only）跳过安装"
elif [ -z "$MISSING_REQ$MISSING_OPT" ]; then
	info "没有需要安装的包"
elif [ -z "$PM" ]; then
	err "缺少必需依赖但无法识别包管理器，请手动安装:$MISSING_REQ"
	HARD_FAIL=1
else
	title "安装缺失依赖"

	if confirm "即将用 $PM 安装上述缺失依赖，继续？"; then
		pm_refresh

		if [ -n "$MISSING_REQ" ]; then
			info "安装必需依赖:$MISSING_REQ"
			if ! pm_install $MISSING_REQ; then
				warn "批量安装失败，改为逐个安装以定位具体是哪个包"
				_failed=''
				for _p in $MISSING_REQ; do
					pm_install "$_p" || _failed="$_failed $_p"
				done
				[ -z "$_failed" ] || err "以下必需依赖安装失败:$_failed"
			fi
		fi

		if [ -n "$MISSING_OPT" ]; then
			info "安装可选依赖:$MISSING_OPT"
			for _p in $MISSING_OPT; do
				pm_install "$_p" || warn "可选依赖 $_p 安装失败，跳过（不影响构建）"
			done
		fi

		# 复核必需依赖：用 here-doc 而非管道，避免 while 落进子 shell 丢变量
		_still=''
		while IFS='|' read -r _c _p _d _l; do
			[ "$_l" = req ] || continue
			dep_present "$_c" "$_p" || _still="$_still $_p"
		done <<EOD
$(deps_for "$FAMILY")
EOD
		if [ -n "$_still" ]; then
			err "以下必需依赖仍不可用:$_still"
			err "请手动确认这些包名在你发行版上的叫法，或检查软件源。"
			HARD_FAIL=1
		else
			ok "所有必需依赖均已就绪"
		fi
	else
		info "已取消安装"
	fi
fi

# ---- cmake 版本 -------------------------------------------------------------
if command -v cmake >/dev/null 2>&1; then
	if cmake_ok; then
		ok "cmake $(cmake --version 2>/dev/null | sed -n '1s/.*version[[:space:]]*//p') 满足 >= $CMAKE_MIN_MAJOR.$CMAKE_MIN_MINOR"
	else
		err "cmake 版本低于 $CMAKE_MIN_MAJOR.$CMAKE_MIN_MINOR，CMakePresets.json（schema v6）会被拒绝。"
		err "  发行版包太旧时，请装官方新版本，或改用更高版本的仓库/backports。"
		HARD_FAIL=1
	fi
fi

# ---- ARM 工具链 -------------------------------------------------------------
ARM_MISSING=0
ARM_NOT_IN_PATH=0
check_arm_toolchain

# ---- 收尾 ------------------------------------------------------------------
title "完成情况"
if [ "$ARM_MISSING" -eq 1 ]; then
	warn "ARM 交叉工具链: 未安装（需要你手动处理，见上方提示）"
elif [ "$ARM_NOT_IN_PATH" -eq 1 ]; then
	warn "ARM 交叉工具链: 已安装但不在 PATH 里"
else
	ok "ARM 交叉工具链: 已就绪"
fi

cat <<EOF

${C_BLD}后续步骤${C_RST}（在仓库根目录执行）
  1. 编译当前 app（由 user_cfg.h 的 APP_NAME 决定）：
       cmake --preset default && cmake --build --preset Debug
  2. 下载到板子（对应 VSCode 任务 "download dap"）：
       cmake --build --preset Debug --target download_dap
  3. 调试：VSCode 的 Cortex-Debug（用 build/active/Debug/openocd_daplink.cfg）

  提示：非 root 跑 openocd 需要 udev 规则，否则每次都得 sudo。
        规则文件由 openocd 包自带（Debian/Fedora 都在 /usr/share/openocd/contrib/）：
          sudo cp /usr/share/openocd/contrib/60-openocd.rules /etc/udev/rules.d/
          sudo udevadm control --reload-rules && sudo udevadm trigger
EOF

# 有硬失败（必需依赖没装上 / cmake 太旧）则返回非 0，方便脚本化调用判断
[ "$HARD_FAIL" -eq 0 ] || exit 1
exit 0
