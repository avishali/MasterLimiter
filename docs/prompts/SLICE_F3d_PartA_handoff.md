# Cursor handoff — F-3d Part A: N-stage `HalfbandPolyphaseOS` (SDK)

**Repo:** `melechdsp-hq` (the SDK), branch as usual. **Do NOT touch** the untracked
`StftEngine` / `spectral` / `quell` WIP — it is not part of this slice. Work only in
`shared/mdsp_dsp/filters/`.

**Scope:** SDK only. This is the BLOCKER half of slice F-3d. Part B (wrap the clipper
at 8× in the plugin) is a *separate* follow-up prompt and is out of scope here. Full
slice context: `docs/prompts/SLICE_F3d_oversampler_8x.md` (in the MasterLimiter repo).

---

## Goal

Generalize `HalfbandPolyphaseOS` from a hardcoded 2-stage (4×) cascade to an
**N-stage cascade** (`2^N×`) and **fix the reconstruction bug** that made the prior
attempt fail. We need a **1-stage (2×)** instance to wrap the clipper, and want
**3-stage (8×)** available for future blanket use.

A prior attempt generalized to a `std::vector<Stage2x>` and was **reverted** because:
- 4× (2-stage) stayed bit-identical (round-trip rel-err ~6e-6) ✅
- **2× (1-stage) and 8× (3-stage) reconstructed at only −21 dB (round-trip rel-err
  ~0.089) — even at the correct reported latency.** ❌

So the bug is **NOT latency reporting** — it is in the cascade / `Stage2x` buffer
reuse for N≠2.

---

## Files

- `shared/mdsp_dsp/include/mdsp_dsp/filters/HalfbandPolyphaseOS.h`
- `shared/mdsp_dsp/src/filters/HalfbandPolyphaseOS.cpp`
- Tests: `shared/mdsp_dsp/tests/…` (the existing `HalfbandPolyphaseOS` anti-alias /
  half-band / symmetric / round-trip tests — find them and keep them green).

## Current shape (what you're changing)

`prepare(numChannels, maxBlockSize, sampleRate, StageSpec stage1, StageSpec stage2)`
builds exactly two `Stage2x` (`stage1_`, `stage2_`). `getOversamplingFactor()` returns
a hardcoded `4`. Latency:
```
osGroupDelay4x = stage1.lat * 2 + stage2.lat
paddedOs4x     = roundUpToMultiple(osGroupDelay4x, 4)
latencySamples = paddedOs4x / 4
```
Up path: `stage1.up(input) → stage2.up(block1) → integer compensation delay`.
Down path:
```cpp
auto stage1Block = stage1_->getBufferBlock(output.getNumSamples()*2);
stage2_->processSamplesDown(stage1Block);  // reads stage2.buffer_, WRITES into stage1.buffer_
stage1_->processSamplesDown(output);        // reads stage1.buffer_ (now stage2's down-output)
```

## Target API

- Keep the existing 2-arg-spec overload (delegates to the new vector form so callers
  don't break):
  ```cpp
  void prepare(int numChannels, int maxBlockSize, double sampleRate,
               StageSpec stage1, StageSpec stage2);            // existing — delegate
  void prepare(int numChannels, int maxBlockSize, double sampleRate,
               const std::vector<StageSpec>& stages);          // NEW canonical form
  ```
- `osFactor_ = 1 << stages.size()`; `getOversamplingFactor()` returns `osFactor_`
  (no longer hardcoded 4).
- Latency = `Σ_i stage_i.lat * 2^(N-1-i)` at the OS rate, then **round UP to a
  multiple of `osFactor_`** via the existing integer `osCompensationDelay_` (that pad
  logic was correct — keep it). For N=2 this reduces to the current
  `stage1.lat*2 + stage2.lat`, so the 4× path must stay byte-for-byte identical.
- `stage1NumTaps_`/`stage2NumTaps_` getters: keep them working for N≥2 (return the
  first two stages' tap counts); fine to add a `getStageNumTaps(int)` if cleaner.
- Stages design taps via the existing `designHalfBandTaps(...)`
  (JUCE `designFIRLowpassHalfBandEquirippleMethod`). **Do NOT** switch to
  `firdesign::kaiserHalfBand`.

## ⚠️ Latency assertion — do NOT re-add the bad one

The reverted attempt asserted `latency % osFactor == 0` somewhere and/or assumed the
host latency is a multiple of the factor. **Latency may be any positive integer.** Do
not assert `lat % factor == 0`. The only requirement is that the *OS-rate* group delay
is padded up to a whole multiple of `osFactor_` (so the host-rate latency is integer) —
which the existing `roundUpToMultiple(..., osFactor_)` already does.

---

## The bug — where to look (read this before coding)

The 2-stage down path works because of a specific in-place handoff: `stage2`
**reads its own `buffer_`** and **writes its decimated output into `stage1`'s
`buffer_`** (via `stage1Block`), then `stage1` reads that and writes to `output`.
Each stage reads one buffer and writes a *different* one — no stage aliases its own
buffer within a single down pass.

`Stage2x` also carries per-stage down-path state: `stateDown_`, the polyphase
`stateDown2_` (sized `nDiv2_/2+1`), and `position_`. And it uses a single in-place
`buffer_` that `processSamplesUp` **writes** and `processSamplesDown` **reads**.

For N≠2, one of those assumptions breaks. Prime suspects:
1. **Shared in-place `buffer_` aliasing.** In a 1-stage flow, the *same* stage's up
   path wrote `buffer_` and its down path reads `buffer_` — but if the generalized
   down loop writes intermediate results back into a buffer that a later stage still
   needs to read, you corrupt it. In the 3-stage flow you must chain
   `stage2.down → stage1.buffer`, `stage1.down → stage0.buffer`, `stage0.down → output`
   in the right order, each reading the buffer the stage *above* just wrote. Verify the
   ordering and that no `getBufferBlock` sub-block size is wrong for the non-final
   stages (the `*2` factor changes per stage level).
2. **`position_` / `stateDown2_` phase** for stages that aren't the final one.

**Recommended fix:** give each `Stage2x` **separate up and down buffers** instead of
the single in-place `buffer_` shared between `processSamplesUp` and
`processSamplesDown`. That removes the aliasing class of bug entirely and makes the
cascade wiring explicit (each stage's down reads the higher stage's down-output buffer,
writes its own).

### Debug method (do this incrementally, don't guess)
1. Prove a **single** `Stage2x` up→down reconstructs in isolation (it must — it's the
   JUCE 2× algorithm). If it doesn't at N=1, the bug is purely in `Stage2x`, not the
   cascade.
2. Prove the 2-stage chain (`s0.up→s1.up→s1.down→s0.down`) still reconstructs after
   your refactor (regression guard).
3. Dump inter-stage signals to find which buffer/state differs for the 1- and 3-stage
   paths.

---

## Repro test (add temporarily, then convert into the permanent N-stage test)

```cpp
for (int ns : {1, 2, 3}) {
    std::vector<HalfbandPolyphaseOS::StageSpec> specs((size_t) ns, {0.06, 90.0});
    HalfbandPolyphaseOS os;
    os.prepare(1, 512, 48000.0, specs);
    // broadband noise through processSamplesUp() -> (no processing) -> processSamplesDown()
    // in 256-sample blocks; align output by getLatencyInSamples(); measure round-trip rel err.
    // EXPECT all < 1e-5.   Currently: ns=2 -> ~6e-6 (good); ns=1 & ns=3 -> ~0.089 (BUG).
}
```

---

## Part A gate (all must pass before you commit)

- [ ] New permanent N-stage round-trip test: `getOversamplingFactor() == (1<<N)` for
      N∈{1,2,3}; **2× and 8× round-trip rel-err < 1e-5**.
- [ ] Reported latency equals the best-alignment latency (no `lat % factor` assertion).
- [ ] **4× path byte-for-byte unchanged** — existing 4× round-trip still ~6e-6, and the
      existing anti-alias / half-band / symmetric tests still pass.
- [ ] Full `mdsp_dsp_tests` suite exit 0.
- [ ] No changes outside `shared/mdsp_dsp/filters/` + its tests. Quell/STFT WIP untouched.

## Report back

- Confirm the gate line-by-line with the actual measured round-trip rel-err for N=1/2/3.
- Report the **exact integer reported latency** for each N (this matters downstream —
  Part B aligns the clipper OS latency to a multiple of 4).
- One-line root cause: what the N≠2 bug actually was.
- Commit message: `mdsp_dsp: F-3d Part A — N-stage HalfbandPolyphaseOS (fix N≠2 reconstruction)`.
- Do NOT push until I've reviewed; SDK + plugin push together at slice close.
