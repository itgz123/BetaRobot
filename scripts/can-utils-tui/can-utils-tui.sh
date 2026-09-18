#!/usr/bin/env bash
#
# can-utils-tui.sh —— USB-CAN 调试用极简 TUI（只依赖 bash + can-utils + iproute2）
#
# 流程：
#   0. 检查依赖（can-utils / iproute2），缺了提示安装
#   1. 从内核日志里找 CAN 相关记录（dmesg / journalctl -k），按时间排序列出
#   2. 选接口
#   3. 输波特率 → sudo ip link set <if> up type can bitrate <xxx>
#   4. 进 TUI：滚动显示 + 显示过滤 + 发送 + 保存
#
# 用法：
#   ./can-utils-tui.sh                     # 全交互
#   ./can-utils-tui.sh can0                # 跳过接口选择
#   ./can-utils-tui.sh can0 1000000        # 跳过接口选择和波特率
#   ./can-utils-tui.sh can0 1000000 5000   # 顺带指定显示缓冲帧数
#
# 接口、波特率、显示缓冲都在启动时问；给了参数（或设了 CAN_TUI_BUF）的那一项就不问了。
#
# 环境变量：
#   CAN_TUI_BUF       显示缓冲帧数，默认 2000（进 TUI 后按 b 也能改，最小 10）
#   CAN_TUI_LOG_DIR   日志目录，默认 ~/can-logs
#   CAN_TUI_NO_UP=1   跳过 ip link set（接口已经配好了）
#   CAN_TUI_RCVBUF    内核收包缓冲字节数，默认取 /proc/sys/net/core/rmem_max
#                     （这个只影响启动时的 candump，运行时改不了）
#
# 已知取舍：帧解析、过滤、刷新全在 bash 里做，几千帧/秒以上会跟不上；
#           日志按「收到的每一帧」原样落盘（不受显示过滤影响）；
#           负载按「总线上收到的全部帧」估算（同样不受显示过滤影响）。
set -u

# ---------------------------------------------------------------- 常量

FRAME_RE='^\(([0-9]+)\.([0-9]+)\)[[:space:]]+([^[:space:]]+)[[:space:]]+([0-9A-Fa-f]{1,8})#(.*)$'
# 只留和 CAN 有关的行，挡掉 dmesg 里 cannot / scan 之类的噪音
CAN_KLOG_RE='can[0-9]+|slcan|vcan|vxcan|gs_usb|kvaser|peak_usb|pcan|mcp25[0-9x]|canable|es58x|f81601|can[ _-]?(device|driver|controller|bus|interface)'
# candump 的诊断输出（stderr 也接进来了），这类行才值得显示
DIAG_RE='candump|error|Error|ERROR|drop|Drop|warn|Warn|No such|bus-off|busoff'

BUF_DEFAULT=2000            # 显示缓冲默认帧数（第 3 个参数 / CAN_TUI_BUF / b 键都能改）
BUF_CAP="${CAN_TUI_BUF:-$BUF_DEFAULT}"
KLOG_SHOW=20                # 参考用的内核日志最多列多少条
LOG_DIR="${CAN_TUI_LOG_DIR:-$HOME/can-logs}"
# 内核收包缓冲：默认顶着 rmem_max 要，省得 setsockopt 被内核拒掉
if [ -n "${CAN_TUI_RCVBUF-}" ]; then
  RCVBUF="$CAN_TUI_RCVBUF"
else
  RCVBUF=$(cat /proc/sys/net/core/rmem_max 2>/dev/null || echo 1048576)
  [[ $RCVBUF =~ ^[0-9]+$ ]] || RCVBUF=1048576
fi
DRAW_MS=100                 # 重绘间隔
MAX_BYTES=8                 # 数据区最多显示多少字节

# 颜色（非 tty 输出时留空）
if [ -t 1 ]; then
  C_RST=$'\033[0m'; C_DIM=$'\033[2m'; C_BLD=$'\033[1m'; C_REV=$'\033[7m'
  C_YEL=$'\033[33m'; C_RED=$'\033[31m'; C_GRN=$'\033[32m'
else
  C_RST=""; C_DIM=""; C_BLD=""; C_REV=""; C_YEL=""; C_RED=""; C_GRN=""
fi

# ---------------------------------------------------------------- 依赖检查

pkg_install() {
  local id=""
  [ -r /etc/os-release ] && . /etc/os-release && id="${ID-}"
  case "$id" in
    fedora | rhel | centos | rocky | almalinux) sudo dnf install -y can-utils iproute ;;
    debian | ubuntu | raspbian | linuxmint)     sudo apt-get install -y can-utils iproute2 ;;
    arch | manjaro | endeavouros)               sudo pacman -S --noconfirm can-utils iproute2 ;;
    opensuse* | sles)                           sudo zypper install -y can-utils iproute2 ;;
    *) return 1 ;;
  esac
}

check_deps() {
  local missing=() c
  for c in ip candump cansend; do
    command -v "$c" >/dev/null 2>&1 || missing+=("$c")
  done
  ((${#missing[@]} == 0)) && return 0

  echo "${C_YEL}缺少命令：${missing[*]}${C_RST}"
  echo "  candump/cansend 来自 can-utils，ip 来自 iproute2"
  echo "  Fedora:  sudo dnf install -y can-utils iproute"
  echo "  Debian:  sudo apt install -y can-utils iproute2"
  echo "  Arch:    sudo pacman -S can-utils iproute2"

  if [ -t 0 ]; then
    local ans=""
    read -r -p "现在自动安装？[y/N] " ans
    case "$ans" in
      y | Y)
        if ! pkg_install; then
          echo "${C_RED}不认识这个发行版，请手动安装${C_RST}" >&2
          exit 1
        fi
        ;;
      *) echo "请先安装再运行" >&2; exit 1 ;;
    esac
  else
    exit 1
  fi

  for c in ip candump cansend; do
    command -v "$c" >/dev/null 2>&1 || { echo "${C_RED}仍未找到 $c${C_RST}" >&2; exit 1; }
  done
  echo "${C_GRN}依赖已就绪${C_RST}"
}

# ---------------------------------------------------------------- 1. 找设备

# 内核日志：dmesg 要权限就用 journalctl，再不行才 sudo
read_klog() {
  local f=$1
  dmesg >"$f" 2>/dev/null && [ -s "$f" ] && return 0
  command -v journalctl >/dev/null 2>&1 &&
    journalctl -k -b -o short-monotonic >"$f" 2>/dev/null && [ -s "$f" ] && return 0
  echo "${C_YEL}读取内核日志需要 root，改用 sudo...${C_RST}" >&2
  sudo dmesg >"$f" 2>/dev/null && [ -s "$f" ] && return 0
  return 1
}

# 填 DEV_* 数组：DEV_TS / DEV_IF / DEV_MSG
detect_devices() {
  DEV_TS=(); DEV_IF=(); DEV_MSG=(); LIVE_IF=()
  local klog entry ts iface msg line
  init_boot_epoch

  klog=$(mktemp) || return 1
  if read_klog "$klog"; then
    # 只留 CAN 相关行；一条都没有就退回用户说的原始 grep -i can
    if ! grep -Eiq "$CAN_KLOG_RE" "$klog"; then
      grep -i -- 'can' "$klog" >"$klog.f" || : >"$klog.f"
    else
      grep -Ei "$CAN_KLOG_RE" "$klog" >"$klog.f"
    fi
    # 时间戳提出来当排序键；journalctl 的 hostname kernel: 前缀顺手去掉
    while IFS= read -r entry; do
      [ -n "${entry//[[:space:]]/}" ] || continue
      DEV_TS+=("${entry%%$'\t'*}")
      entry=${entry#*$'\t'}
      DEV_IF+=("${entry%%$'\t'*}")
      DEV_MSG+=("${entry#*$'\t'}")
    done < <(
      sed -E 's/^(\[[[:space:]]*[0-9]+\.[0-9]+\]) [^ ]+ kernel: /\1 /' "$klog.f" |
        while IFS= read -r line; do
          ts="0"; msg="$line"
          if [[ $line =~ ^\[[[:space:]]*([0-9]+\.[0-9]+)\] ]]; then ts="${BASH_REMATCH[1]}"; fi
          msg=${line#*]}; msg=${msg# }
          iface=""
          if [[ $msg =~ (can|slcan|vcan|vxcan)[0-9]+ ]]; then iface="${BASH_REMATCH[0]}"; fi
          printf '%s\t%s\t%s\n' "$ts" "$iface" "$msg"
        done | LC_ALL=C sort -k1,1g
    )
    rm -f "$klog.f"
  fi
  rm -f "$klog"

  # 系统里实际存在的 CAN 接口，作为补充（dmesg 里可能查不到）
  while IFS= read -r line; do
    [ -n "$line" ] && LIVE_IF+=("$line")
  done < <(ip -o link show 2>/dev/null |
    awk -F': ' '$2 ~ /^(can|slcan|vcan|vxcan)[0-9]+$/ {print $2}')

  build_selectable
}

# _sel_add <接口名> <是否当前存在>：加进候选表，并带上它最近一条内核日志
_sel_add() {
  local i ts="" msg=""
  # DEV_* 已按时间升序，倒着找第一条就是这个接口最近一次出现
  for ((i = ${#DEV_IF[@]} - 1; i >= 0; i--)); do
    if [ "${DEV_IF[$i]}" = "$1" ]; then ts="${DEV_TS[$i]}"; msg="${DEV_MSG[$i]}"; break; fi
  done
  SEL_IF+=("$1"); SEL_LIVE+=("$2"); SEL_TS+=("$ts"); SEL_MSG+=("$msg")
}

# 把内核日志 + ip link 合成一份候选表：同名只留一条，当前存在的排前面。
# （以前两段各编各的号，同一个 can0 会占两个编号，看着莫名其妙）
build_selectable() {
  SEL_IF=(); SEL_LIVE=(); SEL_TS=(); SEL_MSG=()
  local i j
  for ((i = 0; i < ${#LIVE_IF[@]}; i++)); do
    _sel_add "${LIVE_IF[$i]}" 1
  done
  for ((i = 0; i < ${#DEV_IF[@]}; i++)); do
    [ -n "${DEV_IF[$i]}" ] || continue
    for ((j = 0; j < ${#SEL_IF[@]}; j++)); do
      [ "${SEL_IF[$j]}" = "${DEV_IF[$i]}" ] && continue 2
    done
    _sel_add "${DEV_IF[$i]}" 0
  done
}

print_devices() {
  local i n=${#SEL_IF[@]}

  echo
  echo "${C_BLD}==> 可选 CAN 接口${C_RST}"
  if ((n == 0)); then
    echo "  （没有找到）"
  else
    local name desc
    # 中文是双宽字符，表头手写空格，跟下面 %-8s 的列对齐
    printf '    #  %s     %s\n' '接口' '说明'
    for ((i = 0; i < n; i++)); do
      name=${SEL_IF[$i]}
      if ((SEL_LIVE[i])); then
        case "$name" in
          vcan* | vxcan*) desc="当前存在（虚拟接口，不是 USB-CAN）" ;;
          *) desc="当前存在" ;;
        esac
      else
        desc="日志里出现过，当前不在（拔了？）"
      fi
      if [ -n "${SEL_MSG[$i]}" ]; then
        fmt_ktime "${SEL_TS[$i]}" '%(%m-%d %H:%M:%S)T'
        desc+="；最后一条记录 $_KT ${SEL_MSG[$i]}"
      else
        desc+="；内核日志里没有记录"
      fi
      printf '  [%d]  %-8s %s\n' "$((i + 1))" "$name" "$desc"
    done
  fi

  # 内核日志原文：仅供参考，不参与选择
  local m=${#DEV_MSG[@]} from=0
  if ((m > 0)); then
    ((m > KLOG_SHOW)) && from=$((m - KLOG_SHOW))
    echo
    echo "${C_DIM}==> 内核日志里的 CAN 记录（按时间排序，仅供参考）${C_RST}"
    ((from > 0)) && echo "  ${C_DIM}（共 $m 条，这里只显示最近 $((m - from)) 条）${C_RST}"
    for ((i = from; i < m; i++)); do
      fmt_ktime "${DEV_TS[$i]}" '%(%F %T)T'
      printf '  %s%s%s  %s\n' "$C_DIM" "$_KT" "$C_RST" "${DEV_MSG[$i]}"
    done
  fi

  if ((n == 0)); then
    echo
    echo "${C_YEL}没找到 CAN 设备。检查一下：${C_RST}"
    echo "  - USB-CAN 是否插好（lsusb）"
    echo "  - 驱动是否加载（gs_usb / slcan / peak_usb ...）"
    echo "  - 也可以直接手动输入接口名（如 can0、slcan0）"
  fi
}

choose_iface() {
  local n=${#SEL_IF[@]}

  if [ -n "${1-}" ]; then
    IFACE="$1"
    echo "${C_GRN}使用命令行指定的接口：$IFACE${C_RST}"
  elif ((n == 0)); then
    read -r -p "输入接口名（如 can0）: " IFACE
  else
    print_devices
    echo
    read -r -p "选择序号，或直接输入接口名 [1]: " IFACE
    IFACE=${IFACE:-1}
    if [[ $IFACE =~ ^[0-9]+$ ]]; then
      if ((IFACE >= 1 && IFACE <= n)); then
        IFACE="${SEL_IF[$((IFACE - 1))]}"
      else
        echo "${C_RED}序号超范围（可选 1-$n）${C_RST}" >&2
        exit 1
      fi
    fi
  fi

  [ -n "$IFACE" ] || { echo "${C_RED}没有接口，退出${C_RST}" >&2; exit 1; }

  if [ ! -e "/sys/class/net/$IFACE" ]; then
    if skip_up; then
      echo "${C_YEL}警告：/sys/class/net/$IFACE 不存在，按 CAN_TUI_NO_UP 继续${C_RST}"
    else
      echo "${C_RED}接口 $IFACE 不存在${C_RST}" >&2
      local i
      for ((i = ${#DEV_IF[@]} - 1; i >= 0; i--)); do
        [ "${DEV_IF[$i]}" = "$IFACE" ] || continue
        fmt_ktime "${DEV_TS[$i]}" '%(%F %T)T'
        echo "  内核日志里最后一条：$_KT ${DEV_MSG[$i]}（USB-CAN 被拔了？）" >&2
        break
      done
      exit 1
    fi
  fi
}

# ---------------------------------------------------------------- 3. 波特率 / 拉起

current_bitrate() {
  local b
  b=$(ip -details link show "$IFACE" 2>/dev/null |
    grep -o 'bitrate [0-9]*' | head -1 | awk '{print $2}')
  printf '%s' "${b:-}"
}

choose_bitrate() {
  local cur
  cur=$(current_bitrate)
  echo
  echo "${C_BLD}常见波特率${C_RST}  1) 125000  2) 250000  3) 500000  4) 1000000  5) 自定义"
  [ -n "$cur" ] && echo "  （$IFACE 当前是 $cur）"
  local ans=""
  read -r -p "选择波特率 [4]: " ans
  case "${ans:-4}" in
    1) BITRATE=125000 ;;
    2) BITRATE=250000 ;;
    3) BITRATE=500000 ;;
    4) BITRATE=1000000 ;;
    5) read -r -p "输入波特率: " BITRATE ;;
    *) BITRATE="$ans" ;;
  esac
  [[ $BITRATE =~ ^[0-9]+$ ]] || { echo "${C_RED}波特率必须是非负整数${C_RST}" >&2; exit 1; }
}

# 和选接口、选波特率一样的问法；命令行第 3 个参数或 CAN_TUI_BUF 给了就不问
choose_buf() {
  echo
  echo "${C_BLD}显示缓冲区${C_RST}  显示区最多留多少帧（进 TUI 后按 b 也能改，最小 10）"
  local ans=""
  while :; do
    read -r -p "输入帧数 [$BUF_CAP]: " ans
    ans=${ans:-$BUF_CAP}
    [[ $ans =~ ^[0-9]+$ ]] && ((10#$ans >= 10)) && break
    echo "${C_YEL}要 ≥10 的整数${C_RST}"
  done
  set_bufcap "$((10#$ans))"
}

bring_up() {
  echo
  echo "==> sudo ip link set $IFACE up type can bitrate $BITRATE"
  sudo ip link set "$IFACE" down 2>/dev/null
  if sudo ip link set "$IFACE" up type can bitrate "$BITRATE"; then
    :
  else
    # 有些驱动要先 down 设 bitrate 再 up
    echo "${C_YEL}一次性设置失败，换成 down → type can bitrate → up${C_RST}" >&2
    sudo ip link set "$IFACE" down 2>/dev/null
    if ! sudo ip link set "$IFACE" type can bitrate "$BITRATE"; then
      echo "${C_RED}设置波特率失败${C_RST}" >&2
      case "$IFACE" in
        slcan*)
          echo "${C_YEL}slcan 需要先起 slcand，例如：${C_RST}" >&2
          echo "  sudo slcand -o -c -s4 /dev/ttyACM0 $IFACE" >&2
          ;;
      esac
      exit 1
    fi
    sudo ip link set "$IFACE" up || { echo "${C_RED}开启接口失败${C_RST}" >&2; exit 1; }
  fi

  local state
  state=$(ip -br link show "$IFACE" 2>/dev/null | awk '{print $2}')
  if [ "$state" != "UP" ]; then
    echo "${C_YEL}接口状态是 $state，可能没起来${C_RST}" >&2
  fi
  echo "${C_GRN}$IFACE 已就绪（bitrate $BITRATE）${C_RST}"
}

# ---------------------------------------------------------------- TUI 状态

IFACE=""
BITRATE=""
LOGFILE=""
LOGDIR_OK=0
ERRFILE=""

# 设备扫描结果（detect_devices 填充；这里先声明，免得跳过扫描时被 set -u 拦下）
declare -a DEV_TS=() DEV_IF=() DEV_MSG=() LIVE_IF=()
# 候选接口表（detect_devices → build_selectable 填充）
declare -a SEL_IF=() SEL_LIVE=() SEL_TS=() SEL_MSG=()
BOOT_EPOCH=0                          # 开机时刻（epoch），用来把内核日志的秒数换成人看的

declare -a RING=()                    # 显示缓冲（环形）：只放通过过滤的原始行
RING_HEAD=0 RING_CNT=0
declare -a R_PRE=() R_TXT=() R_TYPE=() R_A=() R_B=()   # 过滤规则
TOTAL=0 SHOWN=0                       # 收到总帧数 / 通过过滤的帧数
TOTAL_PREV=0 FPS=0                    # 上周期帧数 / 本周期帧率（TUI 的处理速度）
BITS_ACC=0 LOAD_X10=0                 # 本周期累计 bit 数 / 平滑负载(千分之一)
LOG_PENDING=""
PAUSED=0 QUIT=0
STATUS="就绪"
INPUT_MODE=0 INPUT="" INPUT_PROMPT="" INPUT_ACTION=""
DUMP_FD="" DUMP_PID=""
STTY_SAVED=""
NOW_MS=0 LAST_DRAW=0
TZ_OFF=0
BAR_FULL="" BAR_EMPTY="" SEP=""
_R="" _T="" _H="" _KT=""

# 时间：EPOCHREALTIME 免 fork；没有就退回 date
if [ -n "${EPOCHREALTIME-}" ]; then
  now_ms() { local t=${EPOCHREALTIME//[.,]/}; NOW_MS=$((10#${t:0:13})); }
else
  now_ms() { NOW_MS=$(date +%s%3N); }
fi

# 内核日志给的是「开机以来的秒数」，得减去开机时刻才是人看的日期。
# BOOT_EPOCH = 现在 - uptime，误差不到 1 秒，够用
init_boot_epoch() {
  local up="" rest=""
  read -r up rest </proc/uptime 2>/dev/null || return 1
  [[ $up =~ ^[0-9]+ ]] || return 1
  BOOT_EPOCH=$(($(date +%s) - 10#${BASH_REMATCH[0]}))
  ((BOOT_EPOCH > 0))
}

# fmt_ktime <单调秒> <strftime 格式> → _KT；算不出来就退回原样的单调秒
fmt_ktime() {
  local s=${1%%.*}
  if ((BOOT_EPOCH > 0)) && [[ $s =~ ^[0-9]+$ ]]; then
    printf -v _KT "$2" "$((BOOT_EPOCH + 10#$s))"
  else
    _KT="[$1]"
  fi
}

# 字符画进度条；非 UTF-8 环境退回 ASCII
init_bar() {
  BAR_FULL=$(printf '█%.0s' {1..60})
  BAR_EMPTY=$(printf '░%.0s' {1..60})
  if ((${#BAR_FULL} != 60)); then
    BAR_FULL=$(printf '#%.0s' {1..60})
    BAR_EMPTY=$(printf -- '-%.0s' {1..60})
  fi
  SEP=$(printf '─%.0s' {1..300})
  ((${#SEP} != 300)) && SEP=$(printf -- '-%.0s' {1..300})
}
# bar <已填充> <总宽> → _BAR
bar() { printf -v _BAR '%s%s' "${BAR_FULL:0:$1}" "${BAR_EMPTY:0:$(( $2 - $1 ))}"; }

# 改显示缓冲大小，保留最近 min(现有帧数, 新容量) 帧
set_bufcap() {
  local new=$1 old=$BUF_CAP keep=$RING_CNT i idx
  ((keep > new)) && keep=$new
  local -a tmp=()
  for ((i = 0; i < keep; i++)); do
    idx=$(((RING_HEAD + RING_CNT - keep + i) % old))
    tmp+=("${RING[$idx]}")
  done
  if ((keep > 0)); then RING=("${tmp[@]}"); else RING=(); fi
  BUF_CAP=$new
  RING_HEAD=0
  RING_CNT=$keep
}

# 启动时定缓冲区大小：非数字或太小就退回默认（留下旧帧没意义，反正这时还是空的）
apply_bufcap() {
  local v=$1
  if [[ ! $v =~ ^[0-9]+$ ]] || ((10#$v < 10)); then
    echo "${C_YEL}显示缓冲 $v 不合法（要 ≥10 的整数），改用 $BUF_DEFAULT${C_RST}" >&2
    set_bufcap "$BUF_DEFAULT"
  else
    set_bufcap "$((10#$v))"
  fi
}

# ---------------------------------------------------------------- 过滤

# 把 12 34 56 换成分隔写法，避免每帧 fork
spaced_hex() {
  local s=$1 i n=${#1} max=$MAX_BYTES out=""
  for ((i = 0; i < n && i / 2 < max; i += 2)); do out+="${s:i:2} "; done
  _H=${out% }
}

rule_hit() { # $1=规则下标 $2=数值 ID
  local t=${R_TYPE[$1]} a=${R_A[$1]} b=${R_B[$1]}
  case "$t" in
    E) ((id == a)) ;;
    R) ((id >= a && id <= b)) ;;
    M) (((id & b) == (a & b))) ;;
    *) return 1 ;;
  esac
}

# 0 = 显示。排除优先；有 + 规则时白名单生效，否则全放行
filter_ok() {
  local id=$1 i n=${#R_TYPE[@]}
  ((n == 0)) && return 0
  for ((i = 0; i < n; i++)); do
    [ "${R_PRE[$i]}" = "-" ] || continue
    rule_hit "$i" "$id" && return 1
  done
  local inc=0
  for ((i = 0; i < n; i++)); do
    [ "${R_PRE[$i]}" = "+" ] || continue
    inc=1
    rule_hit "$i" "$id" && return 0
  done
  ((inc)) && return 1
  return 0
}

is_hex() { [[ $1 =~ ^[0-9A-Fa-f]{1,8}$ ]]; }

# CAN_TUI_NO_UP=1/yes/true 时跳过 ip link set
skip_up() {
  case "${CAN_TUI_NO_UP-}" in
    '' | 0 | no | false) return 1 ;;
    *) return 0 ;;
  esac
}

filter_add() { # $1=+/-  $2=规则本体
  local pre=$1 spec=$2 type="" a=0 b=0 body=""
  case "$spec" in
    '~'*)
      body=${spec:1}
      is_hex "$body" || { STATUS="ID 非法: $body"; return 1; }
      type=E; a=$((16#$body))
      ;;
    *:*)
      a=${spec%%:*}; b=${spec#*:}
      is_hex "$a" && is_hex "$b" || { STATUS="掩码格式应为 ID:MASK（都是十六进制）"; return 1; }
      type=M; a=$((16#$a)); b=$((16#$b))
      ;;
    *-*)
      a=${spec%%-*}; b=${spec#*-}
      is_hex "$a" && is_hex "$b" || { STATUS="范围格式应为 起始ID-结束ID（都是十六进制）"; return 1; }
      type=R; a=$((16#$a)); b=$((16#$b))
      ;;
    *)
      is_hex "$spec" || { STATUS="规则非法: $spec"; return 1; }
      type=E; a=$((16#$spec))
      ;;
  esac
  R_PRE+=("$pre"); R_TXT+=("$pre$spec"); R_TYPE+=("$type"); R_A+=("$a"); R_B+=("$b")
  RING_HEAD=0; RING_CNT=0 # 缓冲里存的是按旧规则筛过的，清掉免得误导
  STATUS="已添加 $pre$spec（显示缓冲已清空）"
  return 0
}

filter_del() { # $1=序号(从 1 开始)
  local i=$1
  if ((i < 1 || i > ${#R_TYPE[@]})); then STATUS="没有第 $i 条规则"; return 1; fi
  local j
  for ((j = i - 1; j < ${#R_TYPE[@]} - 1; j++)); do
    R_PRE[j]=${R_PRE[$((j + 1))]}; R_TXT[j]=${R_TXT[$((j + 1))]}
    R_TYPE[j]=${R_TYPE[$((j + 1))]}; R_A[j]=${R_A[$((j + 1))]}; R_B[j]=${R_B[$((j + 1))]}
  done
  unset 'R_PRE[-1]' 'R_TXT[-1]' 'R_TYPE[-1]' 'R_A[-1]' 'R_B[-1]'
  RING_HEAD=0; RING_CNT=0
  STATUS="已删除第 $i 条规则"
}

# ---------------------------------------------------------------- 收帧

flush_log() {
  if [ -n "$LOG_PENDING" ] && ((LOGDIR_OK)); then
    printf '%s' "$LOG_PENDING" >>"$LOGFILE"
  fi
  LOG_PENDING=""
}

process_frame() {
  local line=$1
  if [[ $line =~ $FRAME_RE ]]; then
    local fid=${BASH_REMATCH[4]} data=${BASH_REMATCH[5]}
    local idv=$((16#$fid)) dlc=$((${#data} / 2))
    # 总帧数、日志、负载统计的都是「总线上收到的帧」，必须放在 PAUSED / 过滤
    # 判断之前——挪到后面的话，一加显示过滤负载就假降
    TOTAL=$((TOTAL + 1))
    LOG_PENDING+="$line"$'\n'
    # 位计数：标准帧 47+8*dlc，扩展帧 67+8*dlc（不含填充位，只作负载估算）
    if ((idv > 0x7FF)); then BITS_ACC=$((BITS_ACC + 67 + 8 * dlc)); else
      BITS_ACC=$((BITS_ACC + 47 + 8 * dlc))
    fi
    ((PAUSED)) && return
    filter_ok "$idv" || return
    SHOWN=$((SHOWN + 1))
    local idx=$(((RING_HEAD + RING_CNT) % BUF_CAP))
    RING[idx]="$line"
    if ((RING_CNT < BUF_CAP)); then
      RING_CNT=$((RING_CNT + 1))
    else
      RING_HEAD=$(((RING_HEAD + 1) % BUF_CAP))
    fi
  else
    # 不像帧：只留 candump 的报错/丢帧提示，其余（比如 read -t 超时切出来的半行）丢掉
    [[ $line =~ $DIAG_RE ]] || return
    ((PAUSED)) && return
    local idx=$(((RING_HEAD + RING_CNT) % BUF_CAP))
    RING[idx]="${C_DIM}! ${line}${C_RST}"
    if ((RING_CNT < BUF_CAP)); then RING_CNT=$((RING_CNT + 1)); else
      RING_HEAD=$(((RING_HEAD + 1) % BUF_CAP))
    fi
  fi
}

# ---------------------------------------------------------------- 绘制

render_frame() {
  local line=$1
  if [[ $line =~ $FRAME_RE ]]; then
    local ts=$((10#${BASH_REMATCH[1]})) us=${BASH_REMATCH[2]}
    local fid=${BASH_REMATCH[4]} data=${BASH_REMATCH[5]}
    local t=$(((ts + TZ_OFF) % 86400))
    ((t < 0)) && t=$((t + 86400))
    local c=$(((16#$fid) % 6))
    local col
    case "$c" in
      0) col=36 ;; 1) col=33 ;; 2) col=32 ;;
      3) col=35 ;; 4) col=34 ;; *) col=31 ;;
    esac
    spaced_hex "$data"
    printf -v _R '%02d:%02d:%02d.%03d  \033[%dm%8s\033[0m  %s' \
      "$((t / 3600))" "$((t % 3600 / 60))" "$((t % 60))" "$((10#${us:0:3}))" \
      "$col" "${fid^^}" "$_H"
  else
    _R=$line
  fi
}

draw() {
  local rows=${LINES:-0} cols=${COLUMNS:-0}
  ((rows == 0)) && rows=$(tput lines 2>/dev/null || echo 24)
  ((cols == 0)) && cols=$(tput cols 2>/dev/null || echo 80)
  if ((rows < 12 || cols < 40)); then return; fi

  local h=$((rows - 8)) # 数据区行数
  local out=$'\033[H'
  local s i

  # 1 状态行
  printf -v s ' %s%s%s  %s bit/s  负载 %d.%d%%%s   收到 %d 帧(%d/s)   显示 %d' \
    "$C_BLD" "$IFACE" "$C_RST" "$BITRATE" "$((LOAD_X10 / 10))" "$((LOAD_X10 % 10))" \
    "$C_DIM" "$TOTAL" "$FPS" "$SHOWN"
  printf -v _T '\033[1;1H\033[K%s' "${s:0:$cols}"; out+="$_T"

  # 2 缓冲区 + 进度条
  local pct=0
  ((BUF_CAP > 0)) && pct=$((RING_CNT * 100 / BUF_CAP))
  local w=24
  ((cols < 60)) && w=12
  bar "$((pct * w / 100))" "$w"
  printf -v s ' 缓冲区 %d/%d [%s] %d%%   日志 %s' \
    "$RING_CNT" "$BUF_CAP" "$_BAR" "$pct" "$(basename "$LOGFILE")"
  printf -v _T '\033[2;1H\033[K%s' "${s:0:$cols}"; out+="$_T"

  # 3 过滤规则
  if ((${#R_TXT[@]} == 0)); then
    s=' 过滤 (无：显示全部)'
  else
    s=' 过滤'
    for ((i = 0; i < ${#R_TXT[@]}; i++)); do s+=" [$((i + 1))]${R_TXT[$i]}"; done
  fi
  printf -v _T '\033[3;1H\033[K%s' "${s:0:$cols}"; out+="$_T"

  # 4 分隔
  printf -v _T '\033[4;1H\033[K%s%.*s%s' "$C_DIM" "$cols" "$SEP" "$C_RST"
  out+="$_T"

  # 5.. 数据区（新帧在最下面）
  local n=$RING_CNT
  ((n > h)) && n=$h
  local blank=$((h - n)) idx
  for ((i = 0; i < h; i++)); do
    if ((i < blank)); then
      printf -v _T '\033[%d;1H\033[K' "$((5 + i))"
      out+="$_T"
    else
      idx=$(((RING_HEAD + RING_CNT - n + (i - blank)) % BUF_CAP))
      render_frame "${RING[$idx]}"
      printf -v _T '\033[%d;1H\033[K%s' "$((5 + i))" "${_R:0:$cols}"
      out+="$_T"
    fi
  done

  # rows-3 消息/问题，rows-2 输入行
  if ((INPUT_MODE)); then
    printf -v _T '\033[%d;1H\033[K%s%s%s' "$((rows - 3))" "$C_YEL" "$INPUT_PROMPT" "$C_RST"
    out+="$_T"
    printf -v _T '\033[%d;1H\033[K> %s' "$((rows - 2))" "$INPUT"
    out+="$_T"
  else
    printf -v _T '\033[%d;1H\033[K%s' "$((rows - 3))" " ${STATUS:0:$((cols - 1))}"
    out+="$_T"
    printf -v _T '\033[%d;1H\033[K%s' "$((rows - 2))" " 日志(全部帧): $LOGFILE"
    out+="$_T"
  fi

  # rows-1 分隔（少一个字符，避免顶到右下角触发终端换行抖动）
  printf -v _T '\033[%d;1H\033[K%s%s%s' "$((rows - 1))" "$C_DIM" "${SEP:0:$((cols - 1))}" "$C_RST"
  out+="$_T"

  # rows 底栏
  local foot=' t 发送  f 过滤  s 保存  x 退出  b 缓冲  p 暂停  c 清屏  ? 帮助'
  ((PAUSED)) && foot=' t 发送  f 过滤  s 保存  x 退出  b 缓冲  p 继续  c 清屏  ? 帮助  [已暂停显示]'
  printf -v _T '\033[%d;1H\033[K%s%s%s' "$rows" "$C_REV" "${foot:0:$cols}" "$C_RST"
  out+="$_T"

  # 光标：输入时停在输入行末尾，否则藏起来
  if ((INPUT_MODE)); then
    printf -v _T '\033[%d;%dH\033[?25h' "$((rows - 2))" "$((3 + ${#INPUT}))"
  else
    printf -v _T '\033[?25l'
  fi
  out+="$_T"
  printf '%s' "$out"
}

# ---------------------------------------------------------------- 交互

begin_input() { # $1=问题 $2=处理函数名
  INPUT_MODE=1 INPUT="" INPUT_PROMPT=$1 INPUT_ACTION=$2
}

input_key() {
  case "$1" in
    # 注意：read -n1 会把行分隔符剥掉，回车拿到的是空串
    '' | $'\r' | $'\n')
      INPUT_MODE=0
      local text=$INPUT act=$INPUT_ACTION
      INPUT_ACTION=""
      "$act" "$text"
      ;;
    $'\177' | $'\b') INPUT=${INPUT%?} ;;
    $'\033') INPUT_MODE=0 INPUT_ACTION="" STATUS="已取消" ;;
    *) INPUT+="$1" ;;
  esac
}

act_send() {
  local f=${1// /}
  [ -z "$f" ] && { STATUS="已取消发送"; return; }
  if [[ ! $f =~ ^[0-9A-Fa-f]{1,8}#(#?[0-9A-Fa-f]*|R|r)$ ]]; then
    STATUS="格式错误: $f（例 123#DEADBEEF / 123#R）"
    return
  fi
  local msg
  if msg=$(cansend "$IFACE" "$f" 2>&1); then
    STATUS="已发送 $f"
  else
    STATUS="发送失败: ${msg:-未知错误}"
  fi
}

act_filter() {
  local r=${1// /}
  [ -z "$r" ] && { STATUS="已取消"; return; }
  case "$r" in
    c | C) R_PRE=() R_TXT=() R_TYPE=() R_A=() R_B=(); RING_HEAD=0; RING_CNT=0; STATUS="过滤规则已清空" ;;
    '!'*) filter_del "${r:1}" ;;
    '+'* | '-'*) filter_add "${r:0:1}" "${r:1}" ;;
    *) STATUS="规则要带 +/- 前缀（+ 显示 / - 排除）" ;;
  esac
}

act_buf() {
  local v=${1// /}
  [ -z "$v" ] && { STATUS="已取消"; return; }
  if [[ ! $v =~ ^[0-9]+$ ]] || ((10#$v < 10)); then
    STATUS="缓冲区帧数要 ≥10 的整数（当前 $BUF_CAP）"
    return
  fi
  v=$((10#$v))
  ((v == BUF_CAP)) && { STATUS="缓冲区还是 $BUF_CAP 帧"; return; }
  set_bufcap "$v"
  STATUS="缓冲区改为 $BUF_CAP 帧（保留最近 $RING_CNT 帧）"
}

act_save() {
  local f="$LOG_DIR/snapshot_${IFACE}_$(date +%Y%m%d_%H%M%S).log"
  local i idx n=0
  {
    printf '# can-utils-tui 快照  接口=%s bitrate=%s  %s\n' "$IFACE" "$BITRATE" "$(date '+%F %T')"
    printf '# 过滤规则: %s\n' "${R_TXT[*]:-(无)}"
    printf '# 这里是「已过滤」的显示缓冲；完整日志见 %s\n' "$LOGFILE"
    for ((i = 0; i < RING_CNT; i++)); do
      idx=$(((RING_HEAD + i) % BUF_CAP))
      printf '%s\n' "${RING[$idx]}"
      n=$((n + 1))
    done
  } >"$f"
  STATUS="已保存 $n 帧(过滤后) → $f"
}

show_help() {
  tput clear 2>/dev/null || printf '\033[2J\033[H'
  cat <<'EOF'

  数据区：本地时间 | CAN ID | 数据字节。只显示通过过滤的帧；
          日志文件始终记录全部帧（不受过滤影响）。

  发送（按 t）  <ID>#<数据>，全十六进制
      123#DEADBEEF      标准帧 123，8 字节
      123#1A2B          标准帧 123，2 字节
      123#R             远程帧 RTR
      12345678#11       扩展帧（8 位 ID）
      123##1AABBCC      CAN FD（双 #）

  过滤（按 f）  只影响显示，日志不受影响
      +~123      只显示 ID=123（精确）
      +200-2FF   显示 200..2FF（范围）
      +123:7FF   显示 (ID & 7FF)==(123 & 7FF)（掩码）
      -~456      排除 ID=456（排除优先级最高）
      +123       等价于 +~123
      !2         删除第 2 条规则
      c          清空全部规则
      没有 + 规则时＝全部显示（只受 - 规则影响）

  保存（按 s）  把当前显示缓冲（已过滤）存成快照文件

  缓冲（按 b）  改显示缓冲的帧数（只丢最老的帧，最小的 10）。
                启动时也能定：./can-utils-tui.sh can0 1000000 5000
                或 CAN_TUI_BUF=5000 ./can-utils-tui.sh

  负载          按「总线上收到的全部帧」估算，跟显示过滤无关。
                括号里的 n/s 是 TUI 实际每秒处理的帧数；它明显低于总线
                真实帧率时，说明 TUI 跟不上、帧丢了，负载读数会跟着偏低。

  其它          p 暂停/继续显示（暂停时仍在记日志）  c 清屏  x 退出

  （键盘上 1/2/3/4 和上面几个键等效，习惯数字的照旧能用）

  按任意键返回...
EOF
  local k="" f="" i=0
  while :; do
    # 帮助页停留期间继续收帧，别把管道憋住；每轮最多 100 帧或 2ms，保证按键跟手
    i=0
    while ((i < 100)) && read -t 0.002 -u "$DUMP_FD" f; do
      process_frame "$f"
      i=$((i + 1))
    done
    if read -t 0.02 -rsn1 -u 0 k; then break; fi
  done
  tput clear 2>/dev/null || printf '\033[2J\033[H'
}

main_key() {
  case "$1" in
    t | T | 1) begin_input "发送帧：<ID>#<数据>（如 123#DEADBEEF，123#R 远程帧，ESC 取消）" act_send ;;
    f | F | 2) begin_input "过滤：+显示 / -排除 / !序号 删除 / c 清空；~ID 精确，A-B 范围，ID:MASK 掩码（ESC 取消）" act_filter ;;
    s | S | 3) act_save ;;
    x | X | q | Q | 4) QUIT=1 ;;
    b | B) begin_input "缓冲区帧数（当前 $BUF_CAP，最小 10；ESC 取消）" act_buf ;;
    p | P)
      if ((PAUSED)); then PAUSED=0 STATUS="继续显示"; else
        PAUSED=1 STATUS="显示已暂停（日志仍在记录）"
      fi
      ;;
    c | C) RING_HEAD=0 RING_CNT=0 STATUS="显示缓冲已清空" ;;
    '?' | h | H) show_help ;;
  esac
}

# ---------------------------------------------------------------- TUI 主循环

cleanup() {
  trap - EXIT INT TERM
  local rc=${1:-0}
  flush_log
  [ -n "$DUMP_PID" ] && kill "$DUMP_PID" 2>/dev/null
  [ -n "$DUMP_FD" ] && eval "exec $DUMP_FD<&-" 2>/dev/null
  [ -n "$STTY_SAVED" ] && stty "$STTY_SAVED" 2>/dev/null
  tput cnorm 2>/dev/null
  tput rmcup 2>/dev/null
  [ -n "$ERRFILE" ] && rm -f "$ERRFILE"
  echo
  echo "退出。共收到 $TOTAL 帧，显示 $SHOWN 帧"
  [ -n "$LOGFILE" ] && echo "全部帧日志: $LOGFILE"
  exit "$rc"
}

tui() {
  STTY_SAVED=$(stty -g 2>/dev/null) || {
    echo "需要能控制终端（stty）" >&2
    exit 1
  }
  mkdir -p "$LOG_DIR" || {
    echo "无法创建日志目录 $LOG_DIR" >&2
    exit 1
  }
  LOGDIR_OK=1
  LOGFILE="$LOG_DIR/${IFACE}_$(date +%Y%m%d_%H%M%S).log"
  : >"$LOGFILE"
  ERRFILE=$(mktemp)

  # 本地时区偏移（用于把 epoch 时间戳显示成 HH:MM:SS）
  local tz
  tz=$(date +%z)
  if [[ $tz =~ ^[+-][0-9]{4}$ ]]; then
    TZ_OFF=$((10#${tz:1:2} * 3600 + 10#${tz:3:2} * 60))
    [ "${tz:0:1}" = '-' ] && TZ_OFF=$((-TZ_OFF))
  fi
  init_bar

  # 起 candump：-L 输出日志格式，stdbuf 强制行缓冲，否则管道里会攒够 4K 才吐
  local pre=()
  command -v stdbuf >/dev/null 2>&1 && pre=(stdbuf -oL)
  coproc DUMP { "${pre[@]}" candump -L -r "$RCVBUF" "$IFACE" 2>&1; }
  DUMP_FD=${DUMP[0]}

  tput smcup 2>/dev/null
  tput clear 2>/dev/null || printf '\033[2J\033[H'
  trap 'cleanup 0' EXIT
  trap 'cleanup 130' INT TERM
  tput civis 2>/dev/null
  stty -echo -icanon min 1 time 0

  STATUS="监控 $IFACE 中（? 看帮助）"

  local line="" key="" rc=0 last_key=0 got=0
  now_ms LAST_DRAW=$NOW_MS
  while ((!QUIT)); do
    # 收帧：先等一帧（总线闲时就在这儿睡 20ms，不空转），等到了就把管道里积压的
    # 一起读干净（上限 500 行，免得键盘和重绘被饿死）。读慢了会在内核缓冲里丢帧，
    # 丢掉的帧不计入 bit 数，负载读数就偏低。
    got=0
    if read -t 0.02 -u "$DUMP_FD" line; then
      process_frame "$line"
      got=1
      while ((got < 500)) && read -t 0.001 -u "$DUMP_FD" line; do
        process_frame "$line"
        got=$((got + 1))
      done
    else
      rc=$?
    fi
    # read 返回 >128 是超时（正常，一时没帧）；否则是 EOF，说明 candump 没了
    if ((got == 0 && rc != 0 && rc <= 128)); then
      STATUS="candump 已退出（接口 down 了？）"
      QUIT=1
    fi
    # 键盘：输入时每次都探（要跟手感），否则最多每 30ms 探一次（高帧率时别拖慢收帧）
    now_ms
    if ((INPUT_MODE)) || ((NOW_MS - last_key >= 30)); then
      last_key=$NOW_MS
      if read -t 0.001 -rsn1 -u 0 key; then
        if ((INPUT_MODE)); then input_key "$key"; else main_key "$key"; fi
      fi
    fi
    # 定时刷新
    now_ms
    if ((NOW_MS - LAST_DRAW >= DRAW_MS)); then
      local dt=$((NOW_MS - LAST_DRAW))
      ((dt <= 0)) && dt=1
      # 实际处理帧率：明显低于总线帧率就是 TUI 跟不上，负载读数会跟着偏低
      FPS=$(((TOTAL - TOTAL_PREV) * 1000 / dt))
      TOTAL_PREV=$TOTAL
      # 负载(千分之一)：bit 数 /(时长ms × 波特率) → ×10^6，再四周期平滑
      if [[ ${BITRATE:-0} =~ ^[1-9][0-9]*$ ]]; then
        local l=$((BITS_ACC * 1000000 / (dt * BITRATE)))
        LOAD_X10=$(((LOAD_X10 * 3 + l) / 4))
      fi
      BITS_ACC=0
      draw
      LAST_DRAW=$NOW_MS
      flush_log
    fi
  done
  cleanup 0
}

# ---------------------------------------------------------------- 入口

main() {
  check_deps
  IFACE="${1-}"
  BITRATE="${2-}"
  [ -n "${3-}" ] && BUF_CAP="$3"   # 第 3 个参数：显示缓冲帧数
  apply_bufcap "$BUF_CAP"
  if [ -n "$IFACE" ]; then
    choose_iface "$IFACE" # 命令行给了就跳过扫描（省一次 sudo 读日志）
  else
    detect_devices
    choose_iface ""
  fi

  if skip_up; then
    [ -n "$BITRATE" ] || BITRATE=$(current_bitrate)
    [ -n "$BITRATE" ] || BITRATE=0
    echo "${C_YEL}CAN_TUI_NO_UP 已设置，跳过 ip link set${C_RST}"
  else
    [ -n "$BITRATE" ] || choose_bitrate
    [[ $BITRATE =~ ^[0-9]+$ ]] || { echo "${C_RED}波特率非法${C_RST}" >&2; exit 1; }
    bring_up
  fi

  if [ -z "${3-}" ] && [ -z "${CAN_TUI_BUF-}" ]; then
    choose_buf
  fi

  tui
}

main "${1-}" "${2-}" "${3-}"
