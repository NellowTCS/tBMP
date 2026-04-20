#!/usr/bin/env bash
# build_rust.sh - build the Rust bindings for tBMP
#
# Usage: scripts/build_rust.sh [--static]
#   --static    Build as a static library

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
RUST_DIR="${ROOT}/rust/bindings"

# Source Rust environment
if [ -f "$HOME/.cargo/env" ]; then
    . "$HOME/.cargo/env"
fi

cd "${RUST_DIR}"

if [ "${1:-}" = "--static" ]; then
    echo "==> Building Rust bindings (static)"
    cargo build --release --features static
else
    echo "==> Building Rust bindings"
    cargo build
fi

echo "==> Done: artifacts in ${RUST_DIR}/target"