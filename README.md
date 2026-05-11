# MasterLimiter

MasterLimiter — MelechDSP mastering limiter.

**Status:** Pre-alpha. Slice 1 (skeleton).

## Documentation (HQ)

- [ADR-0004 — MasterLimiter architecture](third_party/melechdsp-hq/docs/DECISIONS/ADR-0004-master-limiter-architecture.md)
- [Product spec](third_party/melechdsp-hq/docs/products/MasterLimiter/SPEC.md)
- [Slice plan](third_party/melechdsp-hq/PROMPTS/MasterLimiter/PLAN.md)

## Build

Requires `JUCE_PATH` pointing at a JUCE 8 checkout (see `CMakePresets.json`).

```bash
export JUCE_PATH=/path/to/JUCE
cmake --preset debug
cmake --build build-debug -j
```

The `debug` preset prefers a sibling `melechdsp-hq` checkout via `MELECHDSP_HQ_ROOT`; otherwise use the `third_party/melechdsp-hq` submodule.

## Git remote

The GitHub remote will be added separately by the repository owner.
