#!/bin/bash
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
MASTER_YAML="${SCRIPT_DIR}/cli/config/master.yaml"
BUILD_SH="${SCRIPT_DIR}/build.sh"
CLI_BIN="${SCRIPT_DIR}/cli/build/ecat_cli"

# 网口不做自动探测，来源是 cli/config/master.yaml 的 master.interface；
# 由应用层（ecat_cli）通过命令行参数接收，创建 master 时绑定
IFNAME=$(grep -m1 -E '^[[:space:]]*interface:' "${MASTER_YAML}" \
    | sed -E 's/^[[:space:]]*interface:[[:space:]]*"?([^"[:space:]]+)"?.*/\1/')

if [ -z "${IFNAME}" ]; then
    echo "Error: master.interface not found in ${MASTER_YAML}"
    exit 1
fi

echo "Interface from config: ${IFNAME} (${MASTER_YAML})"
echo ""

# 1. 先构建
"${BUILD_SH}"

echo ""
echo "Starting ecat_cli on interface: ${IFNAME} ..."
echo "(Ctrl+C to stop)"
echo ""

# 2. 直接运行程序；原始套接字需要 root 权限
sudo "${CLI_BIN}" "${IFNAME}"
