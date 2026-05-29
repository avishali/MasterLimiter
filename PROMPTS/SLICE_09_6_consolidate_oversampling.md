# Cursor Task — Slice 9.6: consolidate oversampling (remove ispTrim, TP headroom in envelope)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-side architectural cleanup. No HQ DSP source change. No bench
> recalibration in this slice — Slice 9.5 re-runs after this lands to
> measure the consolidated chain.**
>
> Post-9.4 the product has TWO cascaded 4× OS round-trips: limiterOS
> (Clipper + Envelope + Gain) followed by ispTrim's own 4× soft-knee TP
> trim. Each downsample FIR introduces some Gibbs-style ringing; stacked,
> they compound to produce ~4000 TP overs on dense pink (Slice 9.5
> baseline showed 4054 vs expected 500). ispTrim's soft-knee
> `1 − exp(−over)` asymptotes toward ceiling at OS rate (always strictly
> below), but its own subsequent downsample re-introduces ringing above
> that asymptote.
>
> Pre-9.4 ispTrim was essential because the 1× envelope didn't see
> intersample peaks. Post-9.4 the 4× envelope already does TP-aware
> limiting at OS rate. ispTrim's role has shrunk to "clean up ringing
> that its own preprocessing partly created." Net effect: cascading two
> OS rounds creates worse TP overs than either alone would.
>
> Fix: remove ispTrim from the audio path entirely. The limiter's own
> downsample FIR still produces some 1× ringing above ceiling. Cover
> that with a small **envelope-side TP headroom**: in TP mode, target
> threshold = `ceiling × 0.965` (≈0.3 dB below ceiling) so the FIR
> ringing pushes back up to ~ceiling without exceeding. SP mode
> unchanged (targets full ceiling, accepts the small SP overs from
> ringing).
>
> Latency in TP mode drops from 362 → 301 samples at 48k (matches SP
> now). PDC re-reports correctly.
>
> **Product-only. No HQ touch. No push. No amend.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. Product
files only this slice. No HQ source edits. No bench edits. No product
push. No amend of any prior product commit.

────────────────────────────────────────
1. WHY (full math)
────────────────────────────────────────

Current chain (post-9.4, pre-9.6):

```
io_input → metering → Drive (1×)
  → limiterOS upsample to 4×
  → Clipper at 4× (if driven)
  → PeakDetector at 4×
  → LimiterEnvelope at 4× → gain to ceiling
  → Lookahead delay + gain multiply at 4×
  → limiterOS downsample to 1×                 [FIR ringing #1: small 1× overs]
  → ispTrim (TP mode only)
      → ispTrim upsample to 4×
      → soft-knee asymptote toward ceiling
      → ispTrim downsample to 1×               [FIR ringing #2: pushes back above asymptote]
  → io_output → metering → loudness
```

`IspTrimStage.cpp` soft-knee math (lines 65–66):
```cpp
const float over = (absX - kneeStart) * invKneeRange;
const float absY = kneeStart + kneeRange * (1.0f - std::exp (-over));
```
`absY → ceiling` only as `over → ∞`. Always strictly below ceiling at
OS rate. Its downsample then re-introduces ringing above that
asymptote.

Replacement chain (post-9.6):

```
io_input → metering → Drive (1×)
  → limiterOS upsample to 4×
  → Clipper at 4× (if driven)
  → PeakDetector at 4×
  → LimiterEnvelope at 4× → gain to (ceiling × 0.965 in TP, ceiling in SP)
  → Lookahead delay + gain multiply at 4×
  → limiterOS downsample to 1×                 [FIR ringing #1: small overs in SP,
                                                                 absorbed by headroom in TP]
  → io_output → metering → loudness
```

In TP mode, the 0.3 dB envelope-side headroom (`× 0.965` ≈ `-0.31 dB`)
gives the downsample FIR ringing room to push back up to ~ceiling
without exceeding. Typical halfband FIR equiripple ringing on a hard-
limited OS signal is 1–3% overshoot at 1× output. For ceiling = −1
dBTP (linear 0.891), a 2% overshoot from envelope target of 0.860
(= 0.891 × 0.965) produces 0.878 = −1.13 dBTP, just below ceiling.

In SP mode, no headroom: the envelope targets full ceiling, and the
downsample ringing produces small 1× SP overs. That's the SP/TP
contract — SP guarantees sample-peak ≤ ceiling at the OS limiting
stage but tolerates inter-sample (which is what TP is for).

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (product only):**
- `Source/PluginProcessor.h`:
  - Remove `mdsp_dsp::IspTrimStage ispTrim_;` field.
  - Remove `int osLatencySamples_` field.
  - Remove `std::atomic<float> currentTpTrimDb_` field and its
    `getCurrentTpTrimDb()` getter.
  - `cachedCeilingMode_` stays (still used to detect SP↔TP transition
    for latency re-report and for threshold computation).
- `Source/PluginProcessor.cpp`:
  - `prepareToPlay`: remove `ispTrim_.prepare(...)` call and the
    `osLatencySamples_` capture. `baseLatencySamples_` and the
    `setLatencySamples` call drop the ispTrim contribution — TP and SP
    now report the same latency (= `baseLookahead +
    limiterOsLatencySamples_`).
  - `processBlock`:
    - Remove the entire `if (modeIdx == 1) { ispTrim_.setCeilingLinear
      (thresholdLin); ispTrim_.process (buffer); ... }` block, including
      both `currentTpTrimDb_.store(...)` calls.
    - In the `limiterActive == true` branch, where `thresholdLin` is
      computed, replace the single-line `thresholdLin = decibelsToGain
      (ceilingDbParam_->get())` with:
      ```cpp
      const float ceilingLin = juce::Decibels::decibelsToGain (ceilingDbParam_->get());
      const float thresholdLin = (modeIdx == 1) ? (ceilingLin * 0.965f) : ceilingLin;
      ```
      where `modeIdx = ceilingMode_->getIndex()` (already captured in
      this function — confirm and reuse).
    - The `cachedCeilingMode_` SP↔TP transition branch still triggers a
      `setLatencySamples` call, but the new value is the same in both
      modes (= `baseLatencySamples_`). Either:
      (a) keep the branch and let setLatencySamples be a no-op idempotent
          re-set, OR
      (b) remove the latency-update logic from that branch entirely.
      Cursor picks the cleaner option; both are functionally
      equivalent. The branch still serves to reset internal state on
      mode change if needed.
- `Source/PluginProcessor.cpp`: anywhere `currentTpTrimDb_` is
  referenced (UI tick path probably reads it via getter) — remove.
- `Source/ui/*` (if any UI component reads `getCurrentTpTrimDb()` for a
  TP-trim readout): remove the readout, or replace with the standard
  GR meter only. Cursor locates by searching for `getCurrentTpTrimDb`.

**DO NOT TOUCH:**
- `shared/mdsp_dsp/IspTrimStage.h/.cpp` — HQ source stays. The component
  remains usable for any future product that wants it; we're just
  unhooking from our processBlock.
- `Source/parameters/Parameters.cpp` — `ceiling_mode` parameter
  (frozen ID, choice [SP, TP]) stays. SP and TP still have different
  semantics in the envelope-threshold computation — SP targets full
  ceiling, TP targets ceiling × 0.965.
- Any other parameter, ID, range, or default.
- HQ DSP source.
- Bench files (separate slice for recalibration).

────────────────────────────────────────
3. WHAT TO DO — specific edits
────────────────────────────────────────

### 3.1 PluginProcessor.h

Remove from the private member section:

```cpp
mdsp_dsp::IspTrimStage ispTrim_;
int osLatencySamples_ = 0;
std::atomic<float> currentTpTrimDb_ { 0.0f };
```

Remove from the public getter section:

```cpp
float getCurrentTpTrimDb() const noexcept { return currentTpTrimDb_.load (std::memory_order_relaxed); }
```

Keep:
- `cachedCeilingMode_` (still used for mode-change detection)
- `baseLatencySamples_` (still used; now means SP=TP latency)
- `limiterOversampler_` and `limiterOsLatencySamples_` (unchanged)
- `currentClipDb_`, `currentGrDb_` and their getters

### 3.2 PluginProcessor.cpp — prepareToPlay

Remove the `ispTrim_.prepare(...)` call. Remove the
`osLatencySamples_ = ...` line.

`baseLatencySamples_` becomes simply:
```cpp
baseLatencySamples_ = baseLookaheadSamples_ + limiterOsLatencySamples_;
```

`setLatencySamples (baseLatencySamples_);` — no SP/TP branch needed in
prepareToPlay (the cached-mode branch in processBlock still re-reports
but the value is the same either way).

### 3.3 PluginProcessor.cpp — processBlock

Inside `if (limiterActive_->get())`:

```cpp
const float ceilingLin   = juce::Decibels::decibelsToGain (ceilingDbParam_->get());
const float thresholdLin = (modeIdx == 1) ? (ceilingLin * 0.965f) : ceilingLin;
envelope_.setThresholdLinear (thresholdLin);
// ... rest unchanged
```

Where `modeIdx` is already `ceilingMode_->getIndex()` from earlier in
the function.

Remove the trailing block:

```cpp
if (modeIdx == 1)
{
    ispTrim_.setCeilingLinear (thresholdLin);
    ispTrim_.process (buffer);
    currentTpTrimDb_.store (ispTrim_.getLastBlockMaxTpDbReduction(), std::memory_order_relaxed);
}
else
{
    currentTpTrimDb_.store (0.0f, std::memory_order_relaxed);
}
```

And the `currentTpTrimDb_.store (0.0f, ...)` in the
`limiterActive == false` branch.

### 3.4 PluginProcessor.cpp — cachedCeilingMode_ branch

Existing:
```cpp
if (modeIdx != cachedCeilingMode_)
{
    const int newLatency = (modeIdx == 1) ? (baseLatencySamples_ + osLatencySamples_)
                                          : baseLatencySamples_;
    setLatencySamples (newLatency);
    ispTrim_.reset();
    cachedCeilingMode_ = modeIdx;
}
```

Replace with:
```cpp
if (modeIdx != cachedCeilingMode_)
{
    // Latency same in SP and TP after 9.6 OS consolidation; setLatencySamples
    // call kept (idempotent) so any future re-divergence flows through here.
    setLatencySamples (baseLatencySamples_);
    cachedCeilingMode_ = modeIdx;
}
```

`ispTrim_.reset()` removed (no field). The setLatencySamples call is a
no-op but documents the intent.

### 3.5 UI cleanup

Search `Source/ui/` for any reference to `getCurrentTpTrimDb` or
`currentTpTrimDb`. If a TP-trim readout exists in the GR meter
component, remove the readout line. The standard GR meter (from
`getCurrentGrDb`) is sufficient — the ispTrim contribution was a
separate "TP trim dB" readout, now obsolete.

If no UI references found, nothing to do in ui/.

────────────────────────────────────────
4. BUILD (no bench in this slice)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean. Verify reported latency at 48k:
- SP mode: 301 samples
- TP mode: **301 samples** (was 362)

If TP latency still reads 362, the ispTrim contribution was not fully
removed from latency reporting — re-check §3.2 / §3.4.

────────────────────────────────────────
5. PRODUCT COMMIT (separate, no push, no amend)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect:
#    M Source/PluginProcessor.h
#    M Source/PluginProcessor.cpp
#    M Source/ui/<one file>   (if UI readout was wired to TP trim dB)

git add Source/PluginProcessor.h Source/PluginProcessor.cpp
# git add Source/ui/<file>   if modified

git commit -m "$(cat <<'EOF'
MasterLimiter: consolidate oversampling - remove ispTrim, TP headroom in envelope

Slice 9.4 introduced 4x oversampling around the limiter chain
(Clipper + Envelope + Gain). With ispTrim still in place as a separate
post-envelope 4x soft-knee TP trim, the audio path went through TWO
cascaded OS round-trips. Each downsample FIR introduces small Gibbs-
style ringing; stacked, they compound: Slice 9.5 baseline showed 4054
TP overs vs expected 500 on dense pink (a known direction worsening
from cascaded FIR, not a wiring bug).

Pre-9.4 ispTrim was essential because the 1x envelope didn't see
intersample peaks. Post-9.4 the 4x envelope already performs TP-aware
limiting at OS rate. ispTrim's role had shrunk to cleaning up ringing
that its own preprocessing partly produced. Net effect: cascading two
OS rounds creates worse TP overs than either alone would.

Architectural change: remove ispTrim from the audio path entirely. The
limiter's own 4x oversampling does all the TP-aware reduction; the
post-downsample 1x output has only the limiterOS's single FIR-ringing
contribution (small).

In TP mode, the envelope's effective threshold is now ceiling x 0.965
(~0.3 dB headroom) so the downsample FIR ringing pushes 1x peaks back
up to approximately ceiling without exceeding. For ceiling -1 dBTP
(linear 0.891), envelope targets 0.860; 2% downsample overshoot
produces 0.878 = -1.13 dBTP, below ceiling.

In SP mode, envelope targets full ceiling without headroom. Small 1x
SP overs from downsample ringing are accepted per the SP/TP contract.

Latency consequence: TP latency drops 362 -> 301 samples at 48k
(matches SP). The 61-sample ispTrim OS round-trip is gone.

Removed:
- ispTrim_ field in PluginProcessor (HQ component remains usable for
  future product that wants it).
- osLatencySamples_ field.
- currentTpTrimDb_ atomic + getter.
- UI TP-trim readout (standard GR meter sufficient).
- cachedCeilingMode_ latency-divergence branch logic; setLatencySamples
  call retained as idempotent placeholder for any future re-divergence.

Frozen-ID params untouched: ceiling_mode still toggles SP vs TP, but
the audible difference is now the envelope-side headroom only, not the
ispTrim soft-knee.

Slice 3/4/5 bench recalibration runs as Slice 9.5 re-pass after this
slice. Expected impact: TP overs drop substantially (single OS pass
worth of ringing only), TP latency reduces. SP path unchanged.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

**Do NOT push product. Do NOT amend any prior product commit.**

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Header diff: removed fields and getter, retained `cachedCeilingMode_`,
   `baseLatencySamples_`, `limiterOversampler_`, `limiterOsLatencySamples_`,
   `currentGrDb_`, `currentClipDb_`.
2. `prepareToPlay` diff: `ispTrim_.prepare` and `osLatencySamples_`
   removed; new `baseLatencySamples_` computation.
3. `processBlock` diff: new `thresholdLin` ternary, ispTrim block
   removed, cachedCeilingMode branch simplified.
4. UI diff (if any).
5. Reported latency at 48k for both SP and TP modes — confirm both = 301.
6. Build success lines for Release and Debug.
7. Product `git log --oneline -4`.
8. Confirmation: HQ NOT touched; product NOT pushed; bench NOT run;
   no other files modified.
9. Open questions.

────────────────────────────────────────
7. AFTER THIS
────────────────────────────────────────

Two parallel paths once 9.6 is committed:

**A. avishali auditions in Live (Release VST3):**
- TP mode A/B vs SP mode at heavy GR: TP should still produce clean
  TP-bounded output (no audible clicks/overs at the ceiling) while
  losing the 61-sample latency. SP mode unchanged from 9.4.
- General feel: should be substantially identical to 9.4 in SP mode and
  cleaner in TP (one less OS pass = less cumulative artifact).

**B. Slice 9.5 re-run** (the consolidated bench recalibration prompt
already written; re-run cold against the new chain):
- TP overs: expect dramatic drop from ~4000 toward the pre-9.4 range
  (a few hundred). If TP overs are now under ~500, this confirms the
  cascaded-OS root-cause and we can treatment-(B) the residual SP-overs
  and IMD as fast-release inherent.
- SP overs: probably unchanged (still limiterOS downsample ringing).
  Same treatment (B) as before.
- IMD: might drop measurably (one fewer FIR pass). If it falls into
  the 1–4% range, we're in family with reference. If still 8%+, the
  IMD investigation kicks in:
    - Bench Ozone 11 Maximizer on the same pink corpus for a reference
      number (avishali has Ozone 11 available).
    - If reference is also 3–8% on this corpus, our numbers are
      synthetic-test-inherent and the bench threshold relaxes.
    - If reference is 1–2%, we have a real artifact source we'd address
      via Slice 9.6b (envelope soft-knee / snap-event smoother).

On 9.5 re-pass → Slice 9 close, push HQ + product, FF main.
On 9.5 still-fail (TP overs not dropping) → STOP and investigate; 9.6
didn't deliver what the model predicted and we re-examine.
