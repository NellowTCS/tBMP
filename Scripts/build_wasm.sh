#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUT_DIR="${ROOT_DIR}/Demo/public/wasm"
OBJ_DIR="${ROOT_DIR}/build/wasm-obj"

find_emsdk() {
    if [[ -x "/tmp/emsdk/emsdk_env.sh" ]]; then
        return 0
    fi
    if command -v emcc >/dev/null 2>&1; then
        return 0
    fi
    return 1
}

ensure_emsdk() {
    if find_emsdk; then
        source /tmp/emsdk/emsdk_env.sh 2>/dev/null || true
        return 0
    fi
    # If /tmp/emsdk exists but is not a valid emsdk install, remove it
    if [[ -d "/tmp/emsdk" && ! -x "/tmp/emsdk/emsdk_env.sh" ]]; then
        echo "==> Removing invalid /tmp/emsdk directory..."
        rm -rf /tmp/emsdk
    fi
    echo "==> Installing Emscripten..."
    git clone --depth 1 https://github.com/emscripten-core/emsdk.git /tmp/emsdk
    cd /tmp/emsdk
    ./emsdk install latest
    ./emsdk activate latest
    source ./emsdk_env.sh
    echo "==> Emscripten installed"
}

build_wasm() {
    source /tmp/emsdk/emsdk_env.sh 2>/dev/null || true
    
    mkdir -p "${OUT_DIR}" "${OBJ_DIR}"
    
    echo "==> Compiling tBMP for WASM with Emscripten"
    emcc -Os -s WASM=1 \
        -s NO_EXIT_RUNTIME=1 \
        -s EXPORT_ES6=1 \
        -s MODULARIZE=1 \
        -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","allocate","HEAPU8"]' \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s INITIAL_MEMORY=16777216 \
        -s EXPORTED_FUNCTIONS='["_malloc","_free","_tbmp_load","_tbmp_reset","_tbmp_width","_tbmp_height","_tbmp_pixel_format","_tbmp_encoding","_tbmp_error","_tbmp_has_palette","_tbmp_has_masks","_tbmp_has_extra","_tbmp_has_meta","_tbmp_pixels_ptr","_tbmp_pixels_len","_tbmp_image_info_json","_tbmp_meta_json","_tbmp_palette_json","_tbmp_masks_json","_tbmp_extra_json","_tbmp_error_string","_tbmp_pixels_copy"]' \
        -s ALLOW_MEMORY_GROWTH=1 \
        -I"${ROOT_DIR}/include" \
        "${ROOT_DIR}/src/tbmp_reader.c" \
        "${ROOT_DIR}/src/tbmp_decode.c" \
        "${ROOT_DIR}/src/tbmp_pixel.c" \
        "${ROOT_DIR}/src/tbmp_rotate.c" \
        "${ROOT_DIR}/src/tbmp_write.c" \
        "${ROOT_DIR}/src/tbmp_meta.c" \
        "${ROOT_DIR}/src/tbmp_msgpack.c" \
        "${ROOT_DIR}/src/tbmp_wasm_entry.c" \
        -o "${OUT_DIR}/tbmp_wasm.js"
    
    echo "==> Done"
    echo "Generated: ${OUT_DIR}/tbmp_wasm.js"
    echo "Generated: ${OUT_DIR}/tbmp_wasm.wasm"
}

main() {
    ensure_emsdk
    build_wasm
}

main "$@"