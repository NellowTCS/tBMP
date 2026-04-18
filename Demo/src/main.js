import initWasm from "../public/wasm/tbmp_wasm.js";

let appState = createInitialAppState();

function createInitialAppState() {
  return {
    // File/image data
    fileName: "",
    rgba: null,
    width: 0,
    height: 0,

    // Display state
    showGrid: true,
    showAlpha: false,
    showChecker: true,

    // WASM-derived data, populated once per file load
    pixelFormat: 0,
    encoding: 0,
    meta: null,
    palette: null,
    masks: null,

    // UI state
    collapsedPanels: new Set(),
  };
}

function setAppState(partial) {
  // Shallow merge, but replace collapsedPanels if present
  if (partial.collapsedPanels) {
    appState = {
      ...appState,
      ...partial,
      collapsedPanels: new Set(partial.collapsedPanels),
    };
  } else {
    appState = { ...appState, ...partial };
  }
}

function resetAppStateForFile(fileName) {
  appState = createInitialAppState();
  appState.fileName = fileName;
  // Panels start collapsed until data is loaded
  appState.collapsedPanels.add("paletteInfo");
  appState.collapsedPanels.add("maskInfo");
}

const view = {
  offsetX: 0,
  offsetY: 0,
  scale: 1,
};

let wasmModule = null;

function q(sel) {
  return document.querySelector(sel);
}
function qa(sel) {
  return [...document.querySelectorAll(sel)];
}

function escapeHtml(s) {
  if (!s) return "";
  return String(s)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");
}

// WASM helpers

async function startWasm() {
  if (wasmModule) return wasmModule;
  wasmModule = await initWasm();
  await new Promise((resolve) => {
    if (wasmModule.calledRun) resolve();
    else wasmModule.onRuntimeInitialized = resolve;
  });
  return wasmModule;
}

function readJson(mod, writeFn) {
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

// Data loading (pure side-effects on appState)

function loadDerivedData(mod) {
  const pixelFormat = mod.ccall("tbmp_pixel_format", "number", []);
  const encoding = mod.ccall("tbmp_encoding", "number", []);
  const meta = readJson(mod, (buf, len) =>
    mod.ccall("tbmp_meta_json", "number", ["number", "number"], [buf, len]),
  );
  let palette = readJson(mod, (buf, len) =>
    mod.ccall("tbmp_palette_json", "number", ["number", "number"], [buf, len]),
  );
  if (!Array.isArray(palette)) palette = [];
  const masks = readJson(mod, (buf, len) =>
    mod.ccall("tbmp_masks_json", "number", ["number", "number"], [buf, len]),
  );

  // Auto-collapse panels that have no data
  const hasPalette = palette.length > 0;
  const hasMasks = masks && masks.r + masks.g + masks.b + masks.a > 0;
  const collapsedPanels = new Set(appState.collapsedPanels);
  if (!hasPalette) collapsedPanels.add("paletteInfo");
  if (!masks || !hasMasks) collapsedPanels.add("maskInfo");

  setAppState({ pixelFormat, encoding, meta, palette, masks, collapsedPanels });
}

// Renderers (pure: read appState, write DOM)

const PF_NAMES = [
  "INDEX1",
  "INDEX2",
  "INDEX4",
  "INDEX8",
  "RGB565",
  "RGB555",
  "RGB444",
  "RGB332",
  "RGB888",
  "RGBA8888",
  "CUSTOM",
];
const ENC_NAMES = ["RAW", "ZERORANGE", "RLE", "SPAN", "SPARSE", "BLOCK-SPARSE"];

function renderFileInfo() {
  const { fileName, width, height, rgba, pixelFormat, encoding } = appState;
  if (!fileName) {
    q("#fileInfo").innerHTML = `<p class="empty-state">—</p>`;
    return;
  }
  q("#fileInfo").innerHTML = `<table class="info-table">
        <tr><td>File</td><td>${escapeHtml(fileName)}</td></tr>
        <tr><td>Dimensions</td><td>${width} × ${height}</td></tr>
        <tr><td>Format</td><td>${PF_NAMES[pixelFormat] ?? "?"}</td></tr>
        <tr><td>Encoding</td><td>${ENC_NAMES[encoding] ?? "?"}</td></tr>
        <tr><td>Size</td><td>${rgba?.length ?? 0} bytes</td></tr>
    </table>`;
}

function renderKeyValueTable(elId, obj) {
  const el = q(`#${elId}`);
  if (!obj || typeof obj !== "object") {
    el.innerHTML = "<p class='empty-state'>No data.</p>";
    return;
  }
  const rows = Object.entries(obj)
    .map(([k, v]) => {
      const label = k.replace(/_/g, " ");
      const val =
        v === null
          ? "<em>null</em>"
          : Array.isArray(v)
            ? v.length === 0
              ? "<em>empty</em>"
              : escapeHtml(String(v))
            : typeof v === "object"
              ? `<pre>${escapeHtml(JSON.stringify(v, null, 2))}</pre>`
              : escapeHtml(String(v));
      return `<tr><td>${escapeHtml(label)}</td><td>${val}</td></tr>`;
    })
    .join("");
  el.innerHTML = rows
    ? `<table class="info-table">${rows}</table>`
    : "<p class='empty-state'>No data.</p>";
}

function renderPalette() {
  const el = q("#paletteInfo");
  const countEl = q("#paletteCount");
  const palette = Array.isArray(appState.palette) ? appState.palette : [];

  if (!palette.length) {
    el.innerHTML = "<p class='empty-state'>No palette</p>";
    if (countEl) countEl.textContent = "";
    return;
  }

  // Build swatches, batch into a fragment-friendly string
  const swatches = palette
    .slice(0, 256)
    .map(
      (e) =>
        `<div class="palette-swatch" title="rgba(${e.r},${e.g},${e.b},${e.a})" style="background:rgba(${e.r},${e.g},${e.b},${e.a / 255})"></div>`,
    )
    .join("");

  el.innerHTML = `<div class="palette-grid">${swatches}</div>`;
  if (countEl) countEl.textContent = `${palette.length} colors`;
}

function renderMasks() {
  const el = q("#maskInfo");
  const masks = appState.masks;

  if (!masks || typeof masks !== "object") {
    el.innerHTML = "<p class='empty-state'>No masks</p>";
    return;
  }

  const channel = (name, color, bits) => {
    const cells = Array.from(
      { length: 16 },
      (_, i) =>
        `<span style="width:12px;height:12px;display:inline-block;background:${i < bits ? color : "var(--color-border)"};border:1px solid var(--color-border-strong)"></span>`,
    ).join("");
    return `<div style="flex:1">
            <div style="font-size:0.75rem;font-weight:600;color:${color};margin-bottom:4px">${name}</div>
            <div style="font-size:0.625rem;color:var(--color-text-muted);margin-bottom:4px">${bits} bits</div>
            <div style="display:grid;grid-template-columns:repeat(4,1fr);gap:1px">${cells}</div>
        </div>`;
  };

  el.innerHTML = `<div style="display:flex;gap:8px;flex-wrap:wrap">
        ${channel("Red", "#ef4444", masks.r)}
        ${channel("Green", "#22c55e", masks.g)}
        ${channel("Blue", "#3b82f6", masks.b)}
        ${channel("Alpha", "#a855f7", masks.a)}
    </div>`;
}

function renderPanelCollapseStates() {
  qa(".panel-card").forEach((card) => {
    const body = card.querySelector(".panel-card-body");
    if (!body) return;
    const id = body.id;
    const collapsed = appState.collapsedPanels.has(id);
    card.classList.toggle("collapsed", collapsed);
  });
}

function renderAll() {
  renderFileInfo();
  renderKeyValueTable("metaInfo", appState.meta);
  renderPalette();
  renderMasks();

  // After rendering content, expand panels that HAVE data
  // This supersedes any collapsed preference from before load
  if (
    appState.palette &&
    Array.isArray(appState.palette) &&
    appState.palette.length > 0
  ) {
    appState.collapsedPanels.delete("paletteInfo");
  }
  if (
    appState.masks &&
    appState.masks.r + appState.masks.g + appState.masks.b + appState.masks.a >
      0
  ) {
    appState.collapsedPanels.delete("maskInfo");
  }

  renderPanelCollapseStates();
}

// Canvas drawing

function drawCanvas() {
  const canvas = q("#viewerCanvas");
  if (!canvas) return;
  const { width: w, height: h, rgba, showGrid, showAlpha } = appState;
  if (!rgba || !w || !h) return;

  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
  }

  const pixels = new Uint8ClampedArray(rgba);
  if (showAlpha) {
    for (let i = 0; i < pixels.length; i += 4) {
      const a = pixels[i + 3];
      pixels[i] = a;
      pixels[i + 1] = a;
      pixels[i + 2] = a;
      pixels[i + 3] = 255;
    }
  }
  canvas.getContext("2d").putImageData(new ImageData(pixels, w, h), 0, 0);

  if (showGrid && view.scale >= 4) {
    const ctx = canvas.getContext("2d");
    ctx.strokeStyle = "rgba(0,0,0,0.25)";
    ctx.lineWidth = 0.5 / view.scale;
    for (let y = 0; y <= h; y++) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    }
    for (let x = 0; x <= w; x++) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    }
  }
}

// View transforms

function applyTransform() {
  const canvas = q("#viewerCanvas");
  if (canvas)
    canvas.style.transform = `translate(${view.offsetX}px, ${view.offsetY}px) scale(${view.scale})`;
}

function resetView() {
  const vp = q("#viewerViewport");
  const { width: w, height: h } = appState;
  if (!vp || !w || !h) return;
  const fit = Math.min(vp.clientWidth / w, vp.clientHeight / h, 1);
  view.scale = fit;
  view.offsetX = (vp.clientWidth - w * fit) / 2;
  view.offsetY = (vp.clientHeight - h * fit) / 2;
  syncZoomUI();
  applyTransform();
}

function syncZoomUI() {
  q("#zoomDisplay").textContent = Math.round(view.scale * 100) + "%";
  q("#zoomRange").value = Math.round(Math.max(1, Math.min(32, view.scale * 8)));
}

// File handling

async function handleFile(file) {
  const mod = await startWasm();
  const HEAP = mod.HEAPU8;
  if (!HEAP) {
    console.error("No HEAP");
    return;
  }

  // Call WASM reset before loading new file
  mod._tbmp_reset();

  const data = new Uint8Array(await file.arrayBuffer());

  // Reset per-file state
  resetAppStateForFile(file.name);

  // Reset UI controls to match state
  q("#alphaToggle").checked = false;
  q("#checkerToggle").checked = true;
  q("#viewerViewport").classList.remove("no-checker");
  q("#statusText").textContent = file.name;
  q("#statusText").classList.remove("empty-state");

  const dataPtr = mod._malloc(data.length);
  HEAP.set(data, dataPtr);
  const err = mod.ccall(
    "tbmp_load",
    "number",
    ["number", "number"],
    [dataPtr, data.length],
  );
  mod._free(dataPtr);

  if (err !== 0) {
    const errBuf = mod._malloc(256);
    mod.ccall(
      "tbmp_error_string",
      "number",
      ["number", "number", "number"],
      [err, errBuf, 256],
    );
    const errText = mod.UTF8ToString(errBuf);
    mod._free(errBuf);
    alert(`Decode failed: ${errText}`);
    renderAll();
    return;
  }

  const width = mod.ccall("tbmp_width", "number", []);
  const height = mod.ccall("tbmp_height", "number", []);
  const pxPtr = mod.ccall("tbmp_pixels_ptr", "number", []);
  const pxLen = mod.ccall("tbmp_pixels_len", "number", []);
  const rgba = HEAP.slice(pxPtr, pxPtr + pxLen);
  setAppState({ width, height, rgba });

  // Pull all derived data into state
  loadDerivedData(mod);

  // Now render from state
  q("#dims").textContent = `${width} × ${height}`;
  q("#dims").classList.remove("empty-state");

  drawCanvas();
  resetView();
  renderAll();
}

// Event binding

function mount() {
  // Theme
  q("#themeToggle")?.addEventListener("click", () => {
    q("body").setAttribute(
      "data-theme",
      q("body").getAttribute("data-theme") === "dark" ? "light" : "dark",
    );
  });

  // File input
  q("#fileInput")?.addEventListener("change", (e) => {
    if (e.target.files?.[0]) handleFile(e.target.files[0]);
  });

  // Drag & drop
  const dropZone = q("#dropZone");
  if (dropZone) {
    dropZone.addEventListener("dragover", (e) => {
      e.preventDefault();
      dropZone.classList.add("dragging");
    });
    dropZone.addEventListener("dragleave", () =>
      dropZone.classList.remove("dragging"),
    );
    dropZone.addEventListener("drop", (e) => {
      e.preventDefault();
      dropZone.classList.remove("dragging");
      if (e.dataTransfer?.files?.[0]) handleFile(e.dataTransfer.files[0]);
    });
  }

  // Panel collapse, toggle in state, then re-render collapse
  qa(".panel-card-header").forEach((header) => {
    header.addEventListener("click", () => {
      const body = header
        .closest(".panel-card")
        ?.querySelector(".panel-card-body");
      if (!body?.id) return;
      const id = body.id;
      if (appState.collapsedPanels.has(id)) {
        appState.collapsedPanels.delete(id);
      } else {
        appState.collapsedPanels.add(id);
      }
      renderPanelCollapseStates();
    });
  });

  // Zoom slider
  q("#zoomRange")?.addEventListener("input", (e) => {
    const newScale = Number(e.target.value) / 8;
    const vp = q("#viewerViewport");
    const cx = vp.clientWidth / 2;
    const cy = vp.clientHeight / 2;
    const ratio = newScale / view.scale;
    view.offsetX = cx - ratio * (cx - view.offsetX);
    view.offsetY = cy - ratio * (cy - view.offsetY);
    view.scale = newScale;
    syncZoomUI();
    applyTransform();
  });

  // Display toggles, update state, then side-effect
  q("#gridToggle")?.addEventListener("change", (e) => {
    appState.showGrid = e.target.checked;
    drawCanvas();
  });
  q("#checkerToggle")?.addEventListener("change", (e) => {
    appState.showChecker = e.target.checked;
    q("#viewerViewport").classList.toggle("no-checker", !e.target.checked);
  });
  q("#alphaToggle")?.addEventListener("change", (e) => {
    appState.showAlpha = e.target.checked;
    q("#viewerViewport").classList.toggle("no-checker", e.target.checked);
    drawCanvas();
  });

  // Viewport interactions
  const vp = q("#viewerViewport");
  if (!vp) return;

  // Wheel zoom, cursor-centred
  let wheelAccum = 0;
  vp.addEventListener(
    "wheel",
    (e) => {
      e.preventDefault();
      let delta = e.deltaY;
      if (e.deltaMode === 1) delta *= 20;
      if (e.deltaMode === 2) delta *= 300;
      wheelAccum += delta;
      if (Math.abs(wheelAccum) < 60) return;
      const steps = wheelAccum / 60;
      wheelAccum = 0;
      const ratio =
        steps < 0 ? Math.pow(1.12, -steps) : Math.pow(1 / 1.12, steps);
      const newScale = Math.max(0.5, Math.min(64, view.scale * ratio));
      const actualRatio = newScale / view.scale;
      const rect = vp.getBoundingClientRect();
      const cx = e.clientX - rect.left;
      const cy = e.clientY - rect.top;
      view.offsetX = cx - actualRatio * (cx - view.offsetX);
      view.offsetY = cy - actualRatio * (cy - view.offsetY);
      view.scale = newScale;
      syncZoomUI();
      applyTransform();
    },
    { passive: false },
  );

  // Mouse drag pan
  let dragging = false,
    lastX = 0,
    lastY = 0;
  vp.addEventListener("mousedown", (e) => {
    if (e.button !== 0) return;
    dragging = true;
    lastX = e.clientX;
    lastY = e.clientY;
    vp.style.cursor = "grabbing";
  });
  window.addEventListener("mousemove", (e) => {
    if (!dragging) return;
    view.offsetX += e.clientX - lastX;
    view.offsetY += e.clientY - lastY;
    lastX = e.clientX;
    lastY = e.clientY;
    applyTransform();
  });
  window.addEventListener("mouseup", () => {
    dragging = false;
    vp.style.cursor = "grab";
  });

  // Touch pan & pinch zoom
  let lastTouches = [];
  vp.addEventListener(
    "touchstart",
    (e) => {
      lastTouches = [...e.touches];
    },
    { passive: true },
  );
  vp.addEventListener(
    "touchmove",
    (e) => {
      e.preventDefault();
      const touches = [...e.touches];
      if (touches.length === 1 && lastTouches.length === 1) {
        view.offsetX += touches[0].clientX - lastTouches[0].clientX;
        view.offsetY += touches[0].clientY - lastTouches[0].clientY;
        applyTransform();
      } else if (touches.length === 2 && lastTouches.length === 2) {
        const rect = vp.getBoundingClientRect();
        const prevDist = Math.hypot(
          lastTouches[0].clientX - lastTouches[1].clientX,
          lastTouches[0].clientY - lastTouches[1].clientY,
        );
        const currDist = Math.hypot(
          touches[0].clientX - touches[1].clientX,
          touches[0].clientY - touches[1].clientY,
        );
        const ratio = currDist / prevDist;
        const cx = (touches[0].clientX + touches[1].clientX) / 2 - rect.left;
        const cy = (touches[0].clientY + touches[1].clientY) / 2 - rect.top;
        const newScale = Math.max(0.5, Math.min(64, view.scale * ratio));
        const actualRatio = newScale / view.scale;
        view.offsetX = cx - actualRatio * (cx - view.offsetX);
        view.offsetY = cy - actualRatio * (cy - view.offsetY);
        view.scale = newScale;
        syncZoomUI();
        applyTransform();
      }
      lastTouches = touches;
    },
    { passive: false },
  );

  window.addEventListener("resize", () => {
    if (appState.rgba) resetView();
  });
}

mount();
