#!/bin/bash
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
BUILD_SH="${SCRIPT_DIR}/build.sh"
CLI_BIN="${SCRIPT_DIR}/cli/build/ecat_cli"

# 如果用户传了参数，直接用；否则自动检测第一个可用的有线以太网口
if [ -n "$1" ]; then
    IFNAME="$1"
else
    # 优先选状态为 UP 的 en*/eth* 接口
    IFNAME=$(ip -o link show | awk -F': ' '$2 ~ /^(en|eth)/ && /state UP/ {print $2; exit}')

    # 如果没找到 UP 的，退而求其次选任意 en*/eth* 接口
    if [ -z "${IFNAME}" ]; then
        IFNAME=$(ip -o link show | awk -F': ' '$2 ~ /^(en|eth)/ {print $2; exit}')
    fi
fi

if [ -z "${IFNAME}" ]; then
    echo "Error: No wired Ethernet interface found."
    echo "Available interfaces:"
    ip -br link show
    exit 1
fi

echo "Detected interface: ${IFNAME}"
echo ""

# 1. 先构建
"${BUILD_SH}"

echo ""
echo "Starting ecat_cli on interface: ${IFNAME} ..."
echo "(Ctrl+C to stop)"
echo ""

# 2. 直接运行程序；原始套接字需要 root 权限
sudo "${CLI_BIN}" "${IFNAME}"
