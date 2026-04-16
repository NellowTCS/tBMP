---
title: "Versioning and Evolution"
description: "Compatibility rules and format-evolution policy for tBMP files."
---

## Why This Matters

Versioning should let the format grow without breaking old tooling every few months.

The short version:

- Keep the header stable.
- Add new capabilities through optional sections.
- Only do a major bump when old readers cannot safely parse anymore.

## How Version Is Stored

tBMP stores version as a 16-bit field in major.minor byte form.

- `0x0100` means `1.0`
- `0x0101` means `1.1`

Current baseline is `1.0`.

## When to Bump Minor vs Major

### Minor (`1.x -> 1.y`)

Use a minor bump for additive, backward-friendly changes.

Good examples:

- New optional EXTRA chunk tags
- New optional metadata fields
- Encoder improvements that do not change wire layout

### Major (`1.x -> 2.0`)

Use a major bump for wire-level breaking changes.

Examples:

- Header layout changes
- Reinterpreting existing required fields
- New mandatory semantics old readers cannot ignore

## EXTRA Compatibility Rule

EXTRA is the main extension path. Keep it skippable.

Each chunk is:

```
[tag:4][length:4][body:length]
```

Unknown tags should be skipped by length in normal parse mode.

## META Compatibility Rule

For structured metadata:

- Keep required keys stable in a major line
- Add new keys as optional first
- Prefer a new key name instead of changing old key meaning

Strict mode can reject unknown keys by design. Non-strict mode can be more permissive.

## Recommended Decoder Behavior

- Basic parsing:
  - accept known major versions
  - skip unknown EXTRA chunks
- Strict validation:
  - reject malformed EXTRA layout
  - reject unknown EXTRA chunks (if strict policy says so)
  - enforce structured META schema

## Quick Checklist for New Format Features

Before adding a feature:

1. Can old readers skip it safely?
2. Does it change header meaning or required wire semantics?
3. Is strict-mode behavior documented?
4. Are tests and sample files added?

If old readers cannot parse safely, it is a major bump.
