#!/usr/bin/env bash
# Format all JS/TS files using Prettier and all C/C++ files using clang-format

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Format JS/TS in Demo/ and Docs/ using npm script
echo "Formatting JS/TS files with Prettier (via npm)..."
(cd "${ROOT_DIR}/Demo" && npm run format)

# Format C/C++ in src/, include/, tools/, and tests/
echo "Formatting C/C++ files with clang-format..."
find "${ROOT_DIR}/src" "${ROOT_DIR}/include" "${ROOT_DIR}/tools" "${ROOT_DIR}/tests" \
    -type f \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) \
    -exec clang-format -i {} +

echo "Formatting complete."
