import { q } from "./dom.js";

export function drawCanvas(appState, view) {
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

export function applyTransform(view) {
  const canvas = q("#viewerCanvas");
  if (canvas)
    canvas.style.transform = `translate(${view.offsetX}px, ${view.offsetY}px) scale(${view.scale})`;
}

export function resetView(appState, view) {
  const vp = q("#viewerViewport");
  const { width: w, height: h } = appState;
  if (!vp || !w || !h) return;
  const fit = Math.min(vp.clientWidth / w, vp.clientHeight / h, 1);
  view.scale = fit;
  view.offsetX = (vp.clientWidth - w * fit) / 2;
  view.offsetY = (vp.clientHeight - h * fit) / 2;
  syncZoomUI(view);
  applyTransform(view);
}

export function syncZoomUI(view) {
  q("#zoomDisplay").textContent = Math.round(view.scale * 100) + "%";
  q("#zoomRange").value = Math.round(Math.max(1, Math.min(32, view.scale * 8)));
}
