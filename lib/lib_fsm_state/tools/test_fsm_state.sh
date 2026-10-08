#!/usr/bin/env bash
# test_fsm_state.sh — PC 端 状态结构体+虚表状态机 检验（gcc 编译，Git Bash 下可运行）
#
# 用法：./test_fsm_state.sh
#   跑两遍，共用同一份测试源码（轨迹断言用 ORD(...) 适配两种执行顺序）：
#     1) 默认开关            -> test_fsm_state.log
#     2) PARANOID+UML+TRACE  -> test_fsm_state_extras.log（自检/断言、UML 序、调试打印全开）
#   开关说明：
#     -DLIB_FSM_STATE_STANDALONE 跳过 app_cfg.h（PC 端不引入工程配置）
#     -DLIB_FSM_STATE_USED       打开模块实现（与固件 app_cfg.h 同一开关）
# 退出码 0=PASS / 非 0=FAIL（任一编失败或任一跑挂都算 FAIL）。
set -u
cd "$(dirname "$0")" || exit 1

rc_all=0

run_one() {
    log=$1
    bin=$2
    shift 2

    gcc -O2 -Wall -Wextra -std=c11 \
        -DLIB_FSM_STATE_USED -DLIB_FSM_STATE_STANDALONE "$@" \
        -I .. -o "$bin" test_fsm_state.c ../lib_fsm_state.c
    if [ $? -ne 0 ]; then
        echo "[FAIL] $log 编译失败"
        rc_all=1
        return
    fi

    "./$bin" > "$log" 2>&1
    rc=$?
    rm -f "./$bin"            # 目录里只留源码 + 日志（对齐 lib_math/lib_hamming tools 约定）
    grep -E '^checks=' "$log" # 日志末尾就是结论行（TRACE 开着时前面有一堆 [fsm] 行）
    if [ $rc -ne 0 ]; then
        rc_all=1
    fi
}

run_one test_fsm_state.log test_fsm_state
run_one test_fsm_state_extras.log test_fsm_state_extras \
    -DLIB_FSM_STATE_PARANOID -DLIB_FSM_STATE_UML_ORDER -DLIB_FSM_STATE_TRACE

if [ $rc_all -eq 0 ]; then
    echo "PASS"
else
    echo "FAIL"
fi
exit $rc_all
