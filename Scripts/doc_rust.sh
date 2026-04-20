#!/usr/bin/env bash
# doc_rust.sh - generate Rust bindings documentation
#
# Usage: scripts/doc_rust.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
RUST_DIR="${ROOT}/rust/bindings"

. "$HOME/.cargo/env"

cd "${RUST_DIR}"

echo "==> Generating Rust documentation"
cargo doc --no-deps

echo "==> Done: docs in ${RUST_DIR}/target/doc"