#!/usr/bin/env bash
# build.sh: configure and build the tBMP library, tests, and CLI tool
#
# Usage: scripts/build.sh [Debug|Release|RelWithDebInfo]
#
# Default build type: Release
# Tests and the CLI tool are enabled by default in this script.
# To build only the library: cmake -S . -B build

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_TYPE="${1:-Release}"
BUILD_DIR="${ROOT}/build"

echo "==> Configuring (${BUILD_TYPE}) in ${BUILD_DIR}"
cmake -S "${ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DTBMP_BUILD_TESTS=ON \
    -DTBMP_BUILD_TOOLS=ON

echo "==> Building"
cmake --build "${BUILD_DIR}" --parallel

echo "==> Done: artifacts in ${BUILD_DIR}"
