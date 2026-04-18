export function q(sel) {
  return document.querySelector(sel);
}

export function qa(sel) {
  return [...document.querySelectorAll(sel)];
}

export function escapeHtml(s) {
  if (!s) return "";
  return String(s)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");
}
