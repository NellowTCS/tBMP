#!/usr/bin/env bash
# format_rust.sh - format and check Rust bindings code
#
# Usage: scripts/format_rust.sh [--check]
#   --check    Only check formatting, don't modify

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
RUST_DIR="${ROOT}/rust/bindings"

cd "${RUST_DIR}"

if [ "${1:-}" = "--check" ]; then
    echo "==> Checking Rust formatting"
    cargo fmt --check
else
    echo "==> Formatting Rust code"
    cargo fmt
fi

echo "==> Done"