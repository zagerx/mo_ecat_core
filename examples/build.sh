#!/bin/bash
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)

CORE_BUILD_DIR="${SCRIPT_DIR}/build"
CLI_BUILD_DIR="${SCRIPT_DIR}/cli/build"

echo "Building mo_ecat_core..."
cmake -B "${CORE_BUILD_DIR}" "${PROJECT_ROOT}"
cmake --build "${CORE_BUILD_DIR}" -j"$(nproc)"

echo "Building ecat_cli..."
cmake -B "${CLI_BUILD_DIR}" -DCMAKE_PREFIX_PATH="${CORE_BUILD_DIR}" "${SCRIPT_DIR}/cli"
cmake --build "${CLI_BUILD_DIR}" -j"$(nproc)"

echo ""
echo "Build finished."
echo "  Core library: ${CORE_BUILD_DIR}/libmo_ecat_core.a"
echo "  CLI binary:   ${CLI_BUILD_DIR}/ecat_cli"
