# Slice F-3d — 8× oversampling for the clipper (kill the clipper IMD aliasing)

**Status:** ◻ ready to build (design + bug-repro below). Roadmap-F item. Prior art: F-3a/b/c shipped the
custom 4× `HalfbandPolyphaseOS` (SDK `fd58a4f`, plugin `5f85292`). This was **attempted once and
reverted** — the N-stage engine generalization has a reconstruction bug (Part A) that must be fixed
*first*, with proper verification.

---

## Why (measured, current build)

The clipper, when engaged, generates IMD with two components (two-tone 11k+13k, hard, ~1.9 dB into clip):
- **−28 dB real cubic IMD** (2f1−f2 @9k, 2f2−f1 @15k) — inherent to clipping, *not* fixable by OS.
- **−34 dB aliased products** — hard-clip harmonics fold back because **4× is marginal**. **This is the F-3d target.**

8× pushes the fold-point up an octave; the −34 dB aliasing should drop well down. (The limiter's LF
harmonics are a *separate, release-driven voicing* matter — NOT this slice. See MEMORY / the diagnosis.)

**Do it targeted, not blanket.** Oversample only the *clipper* (a memoryless nonlinearity) to 8×; keep
the crossover + limiter at 4×. Blanket-8× would also run the linear partitioned-FFT crossover at 8× —
pure CPU waste (linear stages gain nothing from more OS) and ~2× the crossover cost.

---

## Part A — fix the N-stage `HalfbandPolyphaseOS` (BLOCKER, do first)

To wrap the clipper I need a **2× (1-stage)** instance; future blanket use wants **8× (3-stage)**. The
engine is currently hardcoded to exactly 2 stages (4×). Generalizing it to a `std::vector<Stage2x>`
cascade built (4× path stayed bit-identical, round-trip 6e-6) BUT **2× and 8× reconstruct at only −21 dB**
(round-trip rel err ~0.089) *even at the correct reported latency*. 4× (2-stage) is perfect.

**So the bug is in the cascade/`Stage2x` reuse for N≠2, not latency reporting.** Prime suspect: the
JUCE-derived `Stage2x` **down**-path keeps internal polyphase state (`stateDown2_`, `position_`, sized
`nDiv2_/2+1`) plus the in-place `buffer_` it both writes (up) and reads (down). In the working 2-stage
flow, the *higher* stage's `processSamplesDown` overwrites the *lower* stage's `buffer_` before the lower
stage decimates it; a 1-stage flow instead decimates the stage's own up-output, and a 3-stage flow chains
three of them. One of these paths violates an assumption baked into `Stage2x`.

**Repro (add as a temporary test, then delete):**
```cpp
for (int ns : {1,2,3}) {
  std::vector<HalfbandPolyphaseOS::StageSpec> specs((size_t)ns, {0.06,90.0});
  HalfbandPolyphaseOS os; os.prepare(1,512,48000.0,specs);
  // push broadband noise through processSamplesUp()->(no proc)->processSamplesDown() in 256-sample blocks,
  // align output by getLatencyInSamples(), measure rel err.
  // EXPECT all <1e-3. ACTUAL: ns=2 -> ~6e-6 (good); ns=1 & ns=3 -> ~0.089 (BUG).
}
```
**Debug method:** dump the inter-stage signal. First prove a *single* `Stage2x` up→down reconstructs in
isolation (it must — it's how JUCE's 2× works). Then prove `stage0.up → stage1.up → stage1.down →
stage0.down` (the 2-stage path) reconstructs. Then find which differs for the 1- and 3-stage paths —
likely the `buffer_` aliasing between up-write and down-read when no higher stage overwrites it, or the
`getBufferBlock` size / `position_` phase for the non-final stages. Consider giving each stage **separate
up and down buffers** instead of the shared in-place `buffer_`.

**Part A gate:** N-stage test — `getOversamplingFactor()==2^N`; **2× and 8× round-trip rel err < 1e-5**;
reported latency == best-alignment latency; 4× path unchanged (existing tests still 6e-6). Latency may be
any positive integer (do NOT assert `lat % factor == 0` — that was a bad assertion last time). The
existing 4× anti-alias / half-band / symmetric tests must still pass. Full `mdsp_dsp_tests` exit 0.

> Generalize `prepare` to take `const std::vector<StageSpec>&` (keep the 2-arg overload delegating to it),
> `osFactor_ = 1<<N`, latency = `Σ stage_i.lat * 2^(N-1-i)` padded up to a multiple of `osFactor_` via the
> existing integer `osCompensationDelay_` (that part was correct). Stages use JUCE equiripple taps via
> `designHalfBandTaps` (NOT `firdesign::kaiserHalfBand`).

---

## Part B — wrap the clipper at 8× (plugin)

After Part A is green:

- Add `mdsp_dsp::HalfbandPolyphaseOS clipperOversampler_` prepared as **1 stage (2×)** at the OS rate
  (`osSampleRate`, `osMaxBlockSize`), gentle spec (e.g. `{0.10, 90}`) — it only needs to suppress images
  in the top octave of the 4× band.
- In `processCore` step 2.7 (clipper, `PluginProcessor.cpp:971-1018`): replace the in-place 4× clip loop
  with **up (4×→8×) → clip per-sample at 8× → down (8×→4×)** around the existing drive/clip/un-drive math.
  Keep the clip metering (`clipEnvBuf_`, `currentClipDb_`) — compute at 8×, map index by `i*n/(2*osN)`.
- **Latency alignment (the F-2c/F-3a lesson):** the clipper OS adds latency *inside* the 4× domain. Its
  total 4×-rate latency MUST be a multiple of `osFactor` (4) so the host PDC stays integer (else a
  sub-sample residual → HF phase tilt). Pad with a small integer delay on the post-clipper `osBlock` if
  needed. Add the resulting host latency (`/4`) to `baseLatencySamples_`; keep `dryDelay_`/`lookaheadPad_`
  consistent.

**Part B gate (offline rig — use CORRECT param names: `input_gain`, `color`, `ceiling`, `clipper`,
`clipper_mode`; see [[rig-param-names]]):**
- Clipper two-tone IMD (11k+13k, hard, ~1.9 dB into clip): the **−34 dB aliased products drop ≥ ~10 dB**;
  the −28 dB real cubic IMD is unchanged (inherent).
- No regression: Color reconstruction still ±0.0000 dB; HF phase still flat (clipper latency aligned);
  true-peak ceiling exact; full `mdsp_dsp_tests` exit 0; plugin builds clean.
- **Report CPU at clip-engaged in a DAW** — confirm targeted-clipper-8× didn't blow the RT budget (should
  be small; only the clipper runs at 8×).

## Out of scope
Blanket 8× for the limiter; moving the anti-alias cut up (roadmap F). The limiter LF distortion (it's
release/voicing, not OS).
