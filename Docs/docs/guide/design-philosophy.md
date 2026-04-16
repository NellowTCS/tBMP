---
title: "Design Philosophy"
description: "Why tBMP exists and the principles that guide its evolution."
---

## Why tBMP Exists

tBMP is for cases where common image formats are heavier than the job:

- Embedded assets where every kilobyte matters.
- Small game sprites and UI textures.
- Data pipelines that need predictable decode behavior.

The goal is not "best compression at any cost". The goal is small, explicit, and easy to integrate.

## Principles

### Predictable Over Clever

tBMP favors straightforward structures over clever tricks.

- Fixed header.
- Explicit section sizes.
- Known decode paths.

This makes debugging, profiling, and long-term maintenance much easier.

### Zero-Dependency Core

The core library should be easy to drop into constrained builds.

- Plain C.
- No runtime dependency chain.
- Caller-owned buffers and clear memory ownership.

### Practical Compression Choices

Not every image needs the same strategy.

tBMP gives multiple encodings so you can pick based on data shape and performance goals.

### Extensible Without Fragility

Evolution should not constantly break tooling.

- EXTRA chunks carry optional feature payloads.
- META carries structured metadata.
- Unknown additions should be skippable where possible.

### Fast Failure Beats Silent Corruption

Validation is explicit and strict mode is available.

If something is malformed, fail loudly instead of guessing.

## Non-Goals

tBMP is not trying to replace PNG or JPEG for web photos or archival interchange.

It is intentionally focused on tiny bitmap workflows where deterministic behavior matters.

## Trade-Offs

- File size may be larger than heavyweight codecs on some images.
- Some features are explicit instead of automatic.
- Clarity and control are prioritized over absolute compression ratio.

## A Simple Decision Test

When evaluating a new feature, ask:

1. Does this keep decode behavior predictable?
2. Can constrained targets still implement it cleanly?
3. Does it preserve backward compatibility where possible?
4. Is complexity justified by concrete user value?

If the answer is mostly yes, it probably belongs in tBMP.
