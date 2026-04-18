import initWasm from "../public/wasm/tbmp_wasm.js";

let wasmModule = null;

export async function startWasm() {
  if (wasmModule) return wasmModule;
  wasmModule = await initWasm();
  await new Promise((resolve) => {
    if (wasmModule.calledRun) resolve();
    else wasmModule.onRuntimeInitialized = resolve;
  });
  return wasmModule;
}

export function readJson(mod, writeFn) {
  if (!mod?.HEAPU8) return null;
  const buf = mod._malloc(8192);
  const len = writeFn(buf, 8192);
  if (len <= 0) {
    mod._free(buf);
    return null;
  }
  const json = mod.UTF8ToString(buf);
  mod._free(buf);
  try {
    return JSON.parse(json);
  } catch {
    return null;
  }
}
