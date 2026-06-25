#!/bin/bash
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
CLI_DIR="${SCRIPT_DIR}/cli"
BUILD_DIR="${CLI_DIR}/build"

echo "Building mo_ecat_core + ecat_cli in ${BUILD_DIR}..."
cmake -B "${BUILD_DIR}" "${CLI_DIR}"
cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo ""
echo "Build finished."
echo "  Core library: ${BUILD_DIR}/mo_ecat_core/libmo_ecat_core.a"
echo "  CLI binary:   ${BUILD_DIR}/ecat_cli"
