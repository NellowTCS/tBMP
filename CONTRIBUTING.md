# Contributing to tBMP

Thanks for contributing.

This project is intentionally small, explicit, and systems-friendly. The design philosophy is not just docs text, it is the bar for changes.

## Development Setup

Build:

```bash
cmake -B build
cmake --build build
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

CLI validation smoke test:

```bash
./build/tbmp validate Examples/colorP6File.tbmp --strict
```

## Change Checklist

Before opening a PR, verify the following.

### Predictable Over Clever

- Wire layout behavior is clear and documented.
- Changes avoid hidden behavior or surprising side effects.
- Error paths are explicit and testable.

### Zero-Dependency Core

- Core library changes do not add new runtime dependencies.
- Memory ownership remains clear (caller-owned buffers where expected).

### Practical Compression Choices

- Encoding-related changes include rationale for when to use them.
- Performance/size trade-offs are called out in docs or PR notes.

### Extensible Without Fragility

- New format features are additive when possible.
- Unknown/optional data remains safely skippable.
- Versioning impact is explained for wire-level changes.

### Fast Failure Over Silent Corruption

- Invalid input paths fail loudly and safely.
- New parser/decoder behavior includes negative tests.
- `tbmp validate --strict` behavior remains coherent.

## Documentation Expectations

If behavior changes, update docs in the same PR:

- guide pages under `Docs/docs/guide/`
- CLI usage in `Docs/docs/guide/cli.md` and `README.md` (if relevant)
- any compatibility notes in `Docs/docs/guide/versioning.md`

## Test Expectations

At minimum:

- Existing test suite passes.
- New behavior has targeted tests.
- If the change touches format behavior, add/adjust sample assets when useful.

## Pull Request Tips

A good PR description includes:

- What changed
- Why it changed
- Compatibility impact
- How it was tested

Use the PR template checklist to speed review.
