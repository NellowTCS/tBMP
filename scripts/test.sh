#!/usr/bin/env bash
# test.sh - build (if needed) and run the tBMP test suite via CTest
#
# Usage: scripts/test.sh [Debug|Release|RelWithDebInfo]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_TYPE="${1:-Release}"
BUILD_DIR="${ROOT}/build"

# Build first (idempotent if nothing changed)
"${SCRIPT_DIR}/build.sh" "${BUILD_TYPE}"

echo "==> Running tests"
cd "${BUILD_DIR}"
ctest --output-on-failure
