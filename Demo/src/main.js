import {
  createInitialAppState,
  setAppState,
  resetAppStateForFile,
} from "./state.js";
import { q, qa } from "./dom.js";
import { startWasm, readJson } from "./wasm-helpers.js";
import { renderAll, renderPanelCollapseStates } from "./render.js";
import { drawCanvas, applyTransform, resetView, syncZoomUI } from "./canvas.js";
import { listExampleFiles, fetchExampleFile } from "./examples.js";

let appState = createInitialAppState();
const view = { offsetX: 0, offsetY: 0, scale: 1 };

function updateAppState(partial) {
  appState = setAppState(partial, appState);
}

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
  updateAppState({
    pixelFormat,
    encoding,
    meta,
    palette,
    masks,
    collapsedPanels,
  });
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
  appState = resetAppStateForFile(file.name);

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
    renderAll(appState);
    return;
  }

  const width = mod.ccall("tbmp_width", "number", []);
  const height = mod.ccall("tbmp_height", "number", []);
  const pxPtr = mod.ccall("tbmp_pixels_ptr", "number", []);
  const pxLen = mod.ccall("tbmp_pixels_len", "number", []);
  const rgba = HEAP.slice(pxPtr, pxPtr + pxLen);
  appState = setAppState({ width, height, rgba }, appState);

  // Pull all derived data into state
  loadDerivedData(mod);

  // Now render from state
  q("#dims").textContent = `${width} × ${height}`;
  q("#dims").classList.remove("empty-state");

  drawCanvas(appState, view);
  resetView(appState, view);
  renderAll(appState);
}

// Event binding
function mount() {
  // Example loader
  const exampleBtn = q("#loadExampleBtn");
  const exampleSelect = q("#exampleSelect");
  if (exampleBtn && exampleSelect) {
    // Style: put select next to button
    exampleBtn.style.marginRight = "6px";
    exampleSelect.style.marginLeft = "0";
    exampleSelect.style.marginTop = "6px";
    exampleSelect.style.width = "100%";

    // Populate dropdown
    listExampleFiles().then((files) => {
      exampleSelect.innerHTML = files
        .map((f) => `<option value="${f}">${f}</option>`)
        .join("");
    });
    // Button loads selected file
    exampleBtn.addEventListener("click", async () => {
      const filename = exampleSelect.value;
      if (!filename) return;
      exampleBtn.disabled = true;
      exampleBtn.textContent = "Loading…";
      try {
        const blob = await fetchExampleFile(filename);
        const file = new File([blob], filename, {
          type: blob.type || "application/octet-stream",
        });
        await handleFile(file);
      } catch (e) {
        alert("Failed to load example: " + e.message);
      } finally {
        exampleBtn.disabled = false;
        exampleBtn.textContent = "Load example…";
      }
    });
  }
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
  // Panel collapse
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
      renderPanelCollapseStates(appState);
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
    syncZoomUI(view);
    applyTransform(view);
  });
  // Display toggles
  q("#gridToggle")?.addEventListener("change", (e) => {
    updateAppState({ showGrid: e.target.checked });
    drawCanvas(appState, view);
  });
  q("#checkerToggle")?.addEventListener("change", (e) => {
    updateAppState({ showChecker: e.target.checked });
    q("#viewerViewport").classList.toggle("no-checker", !e.target.checked);
  });
  q("#alphaToggle")?.addEventListener("change", (e) => {
    updateAppState({ showAlpha: e.target.checked });
    q("#viewerViewport").classList.toggle("no-checker", e.target.checked);
    drawCanvas(appState, view);
  });
  // Viewport interactions
  const vp = q("#viewerViewport");
  if (!vp) return;
  // Wheel zoom
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
      syncZoomUI(view);
      applyTransform(view);
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
    applyTransform(view);
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
        applyTransform(view);
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
        syncZoomUI(view);
        applyTransform(view);
      }
      lastTouches = touches;
    },
    { passive: false },
  );
  window.addEventListener("resize", () => {
    if (appState.rgba) resetView(appState, view);
  });
}

mount();
