#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUT_DIR="${ROOT_DIR}/npm"
OUT_DIR_DEMO="${ROOT_DIR}/Demo/public/wasm"
OBJ_DIR="${ROOT_DIR}/build/wasm-obj"
EMSDK_DIR="${EMSDK_DIR:-/tmp/emsdk}"

find_emsdk() {
    if command -v emcc >/dev/null 2>&1; then
        return 0
    fi
    if [[ -x "${EMSDK_DIR}/emsdk_env.sh" ]]; then
        return 0
    fi
    return 1
}

ensure_emsdk() {
    if find_emsdk; then
        if [[ -x "${EMSDK_DIR}/emsdk_env.sh" ]]; then
            source "${EMSDK_DIR}/emsdk_env.sh" 2>/dev/null || true
        fi
        return 0
    fi
    if [[ -d "${EMSDK_DIR}" && ! -x "${EMSDK_DIR}/emsdk_env.sh" ]]; then
        echo "==> Removing invalid ${EMSDK_DIR} directory..."
        rm -rf "${EMSDK_DIR}"
    fi
    echo "==> Installing Emscripten..."
    git clone --depth 1 https://github.com/emscripten-core/emsdk.git "${EMSDK_DIR}"
    cd "${EMSDK_DIR}"
    ./emsdk install latest
    ./emsdk activate latest
    source ./emsdk_env.sh
    echo "==> Emscripten installed"
}

ensure_typescript() {
    if command -v tsc >/dev/null 2>&1; then
        tsc_version=$(tsc --version 2>/dev/null) || tsc_version=""
        if [[ -n "${tsc_version}" ]]; then
            # Output is "Version X.Y.Z", extract major number after "Version "
            major=$(echo "${tsc_version}" | sed 's/Version //' | cut -d. -f1)
            # TypeScript 6+ deprecated outFile, so only allow 5.x
            if [[ "${major}" -eq 5 ]] 2>/dev/null; then
                return 0
            fi
        fi
    fi
    echo "==> Installing TypeScript for d.ts generation..."
    npm install -g typescript@5.5
}

build_wasm() {
    source "${EMSDK_DIR}/emsdk_env.sh" 2>/dev/null || true

    mkdir -p "${OUT_DIR}" "${OUT_DIR_DEMO}" "${OBJ_DIR}"

    echo "==> Compiling tBMP for WASM with Emscripten"
    emcc -Os -s WASM=1 \
        -s NO_EXIT_RUNTIME=1 \
        -s EXPORT_ES6=1 \
        -s MODULARIZE=1 \
        -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","allocate","HEAPU8"]' \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s INITIAL_MEMORY=33554432 \
        -s EXPORTED_FUNCTIONS='["_malloc","_free","_tbmp_load","_tbmp_reset","_tbmp_cleanup","_tbmp_width","_tbmp_height","_tbmp_pixel_format","_tbmp_encoding","_tbmp_bit_depth","_tbmp_error","_tbmp_has_palette","_tbmp_has_masks","_tbmp_has_extra","_tbmp_has_meta","_tbmp_pixels_ptr","_tbmp_pixels_len","_tbmp_image_info_json","_tbmp_meta_json","_tbmp_palette_json","_tbmp_masks_json","_tbmp_extra_json","_tbmp_error_string","_tbmp_pixels_copy","_tbmp_encode","_tbmp_write_needed","_tbmp_rotate_90","_tbmp_rotate_180","_tbmp_rotate_270","_tbmp_rotate_any","_tbmp_rotate_output_size","_tbmp_convert_pixel","_tbmp_dither","_tbmp_validate"]' \
        -s ALLOW_MEMORY_GROWTH=1 \
        --emit-tsd "${OUT_DIR}/tbmp_wasm.d.ts" \
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

    cp "${OUT_DIR}/tbmp_wasm.js" "${OUT_DIR_DEMO}/"
    cp "${OUT_DIR}/tbmp_wasm.wasm" "${OUT_DIR_DEMO}/"
    cp "${OUT_DIR}/tbmp_wasm.d.ts" "${OUT_DIR_DEMO}/"

    echo "==> Done"
    echo "Generated: ${OUT_DIR}/tbmp_wasm.js"
    echo "Generated: ${OUT_DIR}/tbmp_wasm.wasm"
    echo "Generated: ${OUT_DIR}/tbmp_wasm.d.ts"
    echo "Generated: ${OUT_DIR_DEMO}/tbmp_wasm.js"
    echo "Generated: ${OUT_DIR_DEMO}/tbmp_wasm.wasm"
    echo "Generated: ${OUT_DIR_DEMO}/tbmp_wasm.d.ts"
}

main() {
    ensure_emsdk
    ensure_typescript
    build_wasm
}

main "$@"