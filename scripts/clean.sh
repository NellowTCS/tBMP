#!/usr/bin/env bash
# clean.sh - remove the CMake build directory
#
# Usage: scripts/clean.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT}/build"

if [ -d "${BUILD_DIR}" ]; then
    echo "==> Removing ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
    echo "==> Done"
else
    echo "==> Nothing to clean (${BUILD_DIR} does not exist)"
fi
