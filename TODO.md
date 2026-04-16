# TODO

- [ ] Structured metadata with real schema (title, author, description, created, tags, etc.)
- [x] Add metadata validation helpers (type checking, length checking, required fields)
- [x] Add metadata round‑trip tests (encode -> decode -> compare)
- [ ] Add more sample files demonstrating structured metadata, palettes, masks, and EXTRA chunks
- [ ] Add tBMP versioning clarifications and rules for format evolution
- [ ] Add tBMP -> PNG exporter (for debugging + web demo)
- [x] Add tBMP -> raw RGBA dump tool
- [ ] Add optional dithering helpers for indexed formats
- [ ] Add “auto‑palette” generator for INDEX_1–8 encodings
- [ ] Add encoding heuristics helper (`tbmp_pick_best_encoding()`)
- [ ] Add fuzz tester (random images -> encode -> decode -> verify)
- [x] Add tests for malformed EXTRA chunks
- [x] Add tests for malformed MsgPack metadata
- [x] Add tests for sparse/block‑sparse edge cases

## Tools
- [x] unify all CLI things into one
- [ ] Add validator CLI (with optional strict mode) (`validate --strict`)
- [ ] Expand `conv` with metadata flags (`--title`, `--author`, `--tags`, etc.)
- [ ] Add `--meta-json` or `--meta-file` for bulk metadata injection
- [x] Add `inspect` CLI (header, metadata, EXTRA, warnings)
- [ ] Add `extract_sprites` (grid or metadata‑based sprite extraction)
- [ ] buildscripts and dev conveniences

## Web Demo (WASM, NOT Emscripten)
- [ ] Build minimal WASM decoder + metadata parser (Cheerp? WaJIC? idk)
- [ ] Write custom JS glue (no Emscripten runtime)
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
- [ ] Update `/Docs/docs/guide/metadata.md` with structured metadata schema
- [ ] Add examples for writing/reading structured metadata
- [ ] Add section on custom fields + namespacing
- [ ] Add section on EXTRA chunks (PALT, MASK, etc.)
- [ ] Add “Design Philosophy” page (why tBMP exists)
- [ ] Add “tBMP for Embedded Systems” guide (very important heheh)
- [ ] Add “tBMP for Games” guide (very important heheh)
- [ ] Add screenshots/gifs of the web demo?

## Polish and stuff
- [ ] Add a tiny pixel‑art tBMP logo (as a tBMP -> PNG, duh)
- [ ] Add badges to README (build, tests, docs, version)

--- 

## Beyond v1.0.0

- [ ] Animation support
- [ ] Tilemap support
- [ ] Embedded thumbnails?
