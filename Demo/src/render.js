import { q, qa, escapeHtml } from "./dom.js";

export const PF_NAMES = [
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
export const ENC_NAMES = [
  "RAW",
  "ZERORANGE",
  "RLE",
  "SPAN",
  "SPARSE",
  "BLOCK-SPARSE",
];

export function renderFileInfo(appState) {
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

export function renderKeyValueTable(elId, obj) {
  const el = q(`#${elId}`);
  if (!obj || typeof obj !== "object") {
    el.innerHTML = "<p class='empty-state'>No data.</p>";
    return;
  }
  const rows = Object.entries(obj)
    .map(([k, v]) => {
      const label = k.replace(/_/g, " ");
      let val;
      if (v === null) {
        val = "<em>null</em>";
      } else if (Array.isArray(v)) {
        if (v.length === 0) {
          val = "<em>empty</em>";
        } else if (typeof v[0] === "object" && v[0] !== null) {
          // Render array of objects as a small nested table
          val = v.map((item, idx) => {
            if (typeof item !== "object" || item === null) return `<div>[${escapeHtml(String(item))}]</div>`;
            const innerRows = Object.entries(item)
              .map(([ik, iv]) => `<tr><td>${escapeHtml(ik)}</td><td>${escapeHtml(String(iv))}</td></tr>`)
              .join("");
            return `<table class=\"info-table info-table-nested\" style=\"margin:0.25em 0;max-width:100%;font-size:0.95em;\"><caption style=\"caption-side:bottom;text-align:left;font-size:0.9em;opacity:0.7;\">custom[${idx}]</caption>${innerRows}</table>`;
          }).join("");
        } else {
          val = escapeHtml(String(v));
        }
      } else if (typeof v === "object") {
        val = `<pre>${escapeHtml(JSON.stringify(v, null, 2))}</pre>`;
      } else {
        val = escapeHtml(String(v));
      }
      return `<tr><td>${escapeHtml(label)}</td><td>${val}</td></tr>`;
    })
    .join("");
  el.innerHTML = rows
    ? `<table class="info-table">${rows}</table>`
    : "<p class='empty-state'>No data.</p>";
}

export function renderPalette(appState) {
  const el = q("#paletteInfo");
  const countEl = q("#paletteCount");
  const palette = Array.isArray(appState.palette) ? appState.palette : [];

  if (!palette.length) {
    el.innerHTML = "<p class='empty-state'>No palette</p>";
    if (countEl) countEl.textContent = "";
    return;
  }

  // Add data-index for event delegation
  const swatches = palette
    .slice(0, 256)
    .map(
      (e, i) =>
        `<div class="palette-swatch" data-index="${i}" title="rgba(${e.r},${e.g},${e.b},${e.a})" style="background:rgba(${e.r},${e.g},${e.b},${e.a / 255})"></div>`,
    )
    .join("");

  el.innerHTML = `<div class="palette-grid">${swatches}</div>`;
  if (countEl) countEl.textContent = `${palette.length} colors`;

  // Add click-to-copy with toast
  const grid = el.querySelector(".palette-grid");
  if (grid) {
    grid.onclick = async (e) => {
      const swatch = e.target.closest(".palette-swatch");
      if (!swatch) return;
      const idx = Number(swatch.getAttribute("data-index"));
      const color = palette[idx];
      if (!color) return;
      const rgbaStr = `rgba(${color.r},${color.g},${color.b},${(color.a / 255).toFixed(3)})`;
      try {
        await navigator.clipboard.writeText(rgbaStr);
        showToast(`Copied: ${rgbaStr}`);
      } catch {
        showToast("Copy failed");
      }
    };
  }
}

// Toast logic
let toastTimeout;
function showToast(msg) {
  let toast = document.getElementById("palette-toast");
  if (!toast) {
    toast = document.createElement("div");
    toast.id = "palette-toast";
    toast.className = "palette-toast";
    document.body.appendChild(toast);
  }
  toast.textContent = msg;
  toast.style.opacity = "1";
  clearTimeout(toastTimeout);
  toastTimeout = setTimeout(() => {
    toast.style.opacity = "0";
  }, 1600);
}

export function renderMasks(appState) {
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

export function renderPanelCollapseStates(appState) {
  qa(".panel-card").forEach((card) => {
    const body = card.querySelector(".panel-card-body");
    if (!body) return;
    const id = body.id;
    const collapsed = appState.collapsedPanels.has(id);
    card.classList.toggle("collapsed", collapsed);
  });
}

export function renderAll(appState) {
  renderFileInfo(appState);
  renderKeyValueTable("metaInfo", appState.meta);
  renderPalette(appState);
  renderMasks(appState);

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

  renderPanelCollapseStates(appState);
}
