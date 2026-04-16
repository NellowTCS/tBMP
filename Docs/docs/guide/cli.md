---
title: "CLI"
description: "Using the tbmp command-line toolkit"
---

## tBMP CLI

The project ships with a general CLI named `tbmp` when built with tools enabled.

By default, `tbmp` uses a styled interactive output (section headers, status
lines, and terminal colors). For CI pipelines or scripts, use `--ci` to force
plain text output.

## Global options

```bash
tbmp --help
tbmp --ci <command> ...
```

- `--help` prints usage and command overview.
- `--ci` disables styled output for deterministic logs.

## Build

From the repository root:

```bash
cmake -S . -B build -DTBMP_BUILD_TOOLS=ON
cmake --build build
```

CLI binary path:

```bash
./build/tbmp
```

## Commands

## `encode`

Convert an input image to `.tbmp`.

```bash
tbmp encode <input> <output.tbmp> [--format NAME] [--encoding NAME]
```

Optional helper flags:

- `--pick-encoding`: choose between raw and rle using a quick size heuristic.
- `--auto-palette`: generate a palette automatically for indexed formats.
- `--dither`: apply Floyd-Steinberg dithering to the selected palette.

`--auto-palette` currently requires indexed formats (`index1`, `index2`,
`index4`, `index8`). `--dither` requires a palette (for CLI usage this is
typically combined with `--auto-palette`).

Structured metadata options (all optional, but if any are provided the
required schema fields must be complete):

- `--title TEXT`
- `--author TEXT`
- `--description TEXT`
- `--created TEXT`
- `--software TEXT`
- `--license TEXT`
- `--tags tag1,tag2,...`
- `--dpi N`
- `--colorspace NAME`
- `--custom-map FILE` (repeatable; each file must be a MessagePack map blob)
- `--meta-file FILE` (bulk structured metadata MessagePack blob)

`--meta-file` loads a full structured metadata object from disk. You can still
pass individual metadata flags in the same command; flags that appear later
override fields loaded from the file.

Supported input formats:

- PBM/PGM/PPM (`P1` through `P6`)
- Formats handled by stb_image (PNG, BMP, JPEG, ...)

Format options:

- `rgba8888` (default)
- `rgb888`
- `rgb565`
- `rgb555`
- `rgb444`
- `rgb332`
- `index1`
- `index2`
- `index4`
- `index8`

Encoding options:

- `raw` (default)
- `rle`
- `zerorange`
- `span`

Example:

```bash
tbmp encode sprite.png sprite.tbmp --format rgb565 --encoding rle \
    --title "Forest Tiles" \
    --author "Nellow" \
    --description "Top-down biome tiles" \
    --created "2026-04-16T19:30:00Z" \
    --software "tbmp-cli 0.1" \
    --license "CC-BY-4.0" \
    --tags "tileset,rpg,forest" \
    --dpi 144 \
    --colorspace sRGB

tbmp encode sprite.png sprite_index4.tbmp --format index4 --auto-palette --dither --pick-encoding
```

## `decode`

Decode `.tbmp` to an image file (PPM/PGM output).

```bash
tbmp decode <input.tbmp> <output.ppm>
```

If metadata exists, it is printed to stdout.

Example:

```bash
tbmp decode sprite.tbmp sprite.ppm
```

## `export-png`

Decode `.tbmp` and write a `.png` output file.

```bash
tbmp export-png <input.tbmp> <output.png>
```

This is useful for debugging and quick previews in tools that do not read
`.tbmp` directly.

Example:

```bash
tbmp export-png sprite.tbmp sprite.png
```

## `inspect`

Print structural details for a `.tbmp` file.

```bash
tbmp inspect <input.tbmp>
tbmp --ci inspect <input.tbmp>
```

Reports:

- Header fields
- DATA/EXTRA/META presence and sizes
- Palette and masks details
- EXTRA chunk list
- Metadata entries
- Warning summary

Example:

```bash
tbmp inspect sprite.tbmp
```

## `validate`

Validate a `.tbmp` file structure and optionally enforce strict checks.

```bash
tbmp validate <input.tbmp>
tbmp validate <input.tbmp> --strict
```

Validation modes:

- Basic (default): verifies the file parses successfully.
- Strict (`--strict`): also fails on unknown or malformed `EXTRA` chunks,
  and fails if present `META` does not match the structured schema.

Example:

```bash
tbmp validate sprite.tbmp --strict
```

## `dump-rgba`

Decode and dump raw RGBA bytes (`R,G,B,A` per pixel).

```bash
tbmp dump-rgba <input.tbmp> <output.rgba>
tbmp --ci dump-rgba <input.tbmp> <output.rgba>
```

Output size is:

$$
\text{bytes} = \text{width} \times \text{height} \times 4
$$

Example:

```bash
tbmp dump-rgba sprite.tbmp sprite.rgba
```

## Notes

- The CLI is intentionally lightweight and script-friendly.
- Exit code is non-zero on errors.
- Use `tbmp inspect` first when debugging malformed files.
