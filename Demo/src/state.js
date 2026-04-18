export function createInitialAppState() {
  return {
    fileName: "",
    rgba: null,
    width: 0,
    height: 0,
    showGrid: true,
    showAlpha: false,
    showChecker: true,
    pixelFormat: 0,
    encoding: 0,
    meta: null,
    palette: null,
    masks: null,
    collapsedPanels: new Set(),
  };
}

export function setAppState(partial, appState) {
  if (partial.collapsedPanels) {
    return {
      ...appState,
      ...partial,
      collapsedPanels: new Set(partial.collapsedPanels),
    };
  } else {
    return { ...appState, ...partial };
  }
}

export function resetAppStateForFile(fileName) {
  const state = createInitialAppState();
  state.fileName = fileName;
  state.collapsedPanels.add("paletteInfo");
  state.collapsedPanels.add("maskInfo");
  return state;
}
