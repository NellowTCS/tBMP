---
title: "CLI"
description: "Using the tbmp command-line toolkit"
---

# tBMP CLI

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

Encoding options:
- `raw` (default)
- `rle`
- `zerorange`
- `span`

Example:

```bash
tbmp encode sprite.png sprite.tbmp --format rgb565 --encoding rle
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
