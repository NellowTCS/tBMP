# TODO

- [ ] Add more sample files demonstrating structured metadata, palettes, masks, and EXTRA chunks
- [ ] Add fuzz tests (random images -> encode -> decode -> verify)

## Tools

- [ ] Add `extract_sprites` (grid or metadata‑based sprite extraction)

## Web Demo (WASM, NOT Emscripten, maybe)

- [ ] Build minimal WASM decoder + metadata parser (Cheerp? WaJIC? idk)
- [ ] Write custom JS glue (no Emscripten runtime?)
- [ ] Add drag‑and‑drop file loader
- [ ] Add pixel viewer (canvas)
- [ ] Add zoom + pan + pixel grid
- [ ] Add palette visualizer (PALT)
- [ ] Add masks visualizer (MASK)
- [ ] Add structured metadata panel
- [ ] Add raw MsgPack tree viewer
- [ ] Add EXTRA chunk inspector

## Image Viewer (Standalone or part of demo? shrug probs latter)

- [ ] Show width, height, pixel format, encoding
- [ ] Toggle alpha / checkerboard background
- [ ] Show structured metadata
- [ ] Show custom metadata keys
- [ ] Show EXTRA chunks
- [ ] Show palette swatches
- [ ] Show masks bit layout

## Documentation

- [ ] Add screenshots/gifs of the web demo?

## Polish and stuff

- [ ] Add a tiny pixel‑art tBMP logo (as a tBMP -> PNG, duh)

---

## Beyond v1.0.0

- [ ] Animation support
- [ ] Tilemap support
- [ ] Embedded thumbnails?
