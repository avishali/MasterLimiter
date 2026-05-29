# Cursor Task — Slice 9.3a: limiter feel rework (min→max), faster release default, Clipper meter+LED

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Audition-only slice. No bench run (will be recalibrated in 9.3b after
> avishali approves the new feel).** Three orthogonal fixes bundled because
> they're all part of "make the limiter sound like a real limiter and tell
> the user when the clipper is doing work":
>
> 1. Flip the T/S split combinator in `LimiterEnvelope` from `min(s2, s2s)`
>    to `max(s2, s2s)`. Under the existing parallel-cascade topology,
>    `min` forces effective release = slow envelope τ (release_ms × ratio
>    = 400 ms by default), which is why Clean feels like a slow compressor.
>    `max` makes the fast envelope dominate; release_ms actually drives
>    release. `release_sustain_ratio` becomes inert under `max` — kept as
>    a frozen-ID param, reserved for a future real T/S split slice.
> 2. Lower `release_ms` default from `100.0` → `30.0` ms. Range unchanged.
>    ID unchanged. Brickwall-style default.
> 3. Add Clipper indication: per-block pre-clip peak excess in dB as a
>    numeric readout, plus a red LED that lights when any sample clipped
>    this block and decays in UI over ~200 ms.
>
> **HQ + product both touched. Two separate commits. No push. No amend.
> No bench run this slice — bench will be recalibrated as Slice 9.3b
> after avishali signs off on audition.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. HQ commit
first, product commit second. No HQ push. No amend of `763604e`, `3dfee43`,
`bbd4293`, or any earlier HQ commit. No product push.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

ADR-0005 specified the T/S split as `min(s_fast, s_slow)` over two
parallel cascaded one-poles, intending Pro-L-Modern-style behaviour.
Walking through the algebra:

- Both envelopes snap to `inp` on falling edge (`if inp < s2 - eps`).
- On release (`inp > s2`), both rise exponentially: fast with `alpha_`,
  slow with `alphaSlow_` where `alphaSlow_` corresponds to
  `release_ms × release_sustain_ratio`.
- After a peak both start at the same reduced value; `s_fast` rises
  faster, so `s_fast > s_slow` for the entire release transient until
  they reconverge near 1.0.
- `min(s_fast, s_slow) = s_slow` for the whole release.

Result: effective release τ is always the slow envelope's. With defaults
`release_ms = 100 ms`, `release_sustain_ratio = 4.0`, the audible release
is ~400 ms. The release knob in the UI reads 100 ms; the ear hears 400 ms;
the ratio knob silently drives the whole thing. avishali's audition
confirms: Clean feels like a slow compressor, Tight (which skips the T/S
branch but inherits the cascade smoothing) still pumps because of the
two-stage cascade and the 100 ms default.

Slice 9.2 fixed the stuck-release `ext_` propagation bug. That was
necessary but not sufficient — the smoothing-stage operator is also
producing slow-compressor character.

Switching to `max(s_fast, s_slow)`: fast envelope dominates always (since
it's always greater during release). Effective release = `release_ms`.
`release_sustain_ratio` no longer affects output (slow envelope is
mathematically dead under `max`). We accept that — the param stays as a
frozen ID with no audible effect, reserved for a future real T/S split
slice where program-dependent transient detection actually differentiates
sustained from transient reduction. The current parallel-cascade topology
cannot do that with `min` or `max`; neither is a real T/S split.

Lowering `release_ms` default 100 → 30 ms gives a brickwall feel out of
the box. Tight/Aggressive still skip T/S in the envelope (Slice 9 wiring)
and now also get the faster default.

Clipper indication: avishali wants GR-meter-style feedback for how hard
the Clipper Drive is working. Per-block max(|sample × driveGain|) before
clipping, converted to dB-above-0-dBFS, gives an honest "amount being
clipped" number in the same units as GR. A red LED that latches on any
clip and decays in UI matches the rest of the meter aesthetic.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (HQ):**
- `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp` — `process()`: change
  the gain combination from `std::min (s2, s2s)` to `std::max (s2, s2s)`
  in the `useTransientSustainSplit ? ... : s2` ternary. **One line.**
  Everything else stays. Header untouched.

**MODIFY (product):**
- `Source/parameters/Parameters.cpp` — `release_ms` default value
  `100.0f` → `30.0f`. ID, range, step, label unchanged.
- `Source/PluginProcessor.h` — add
  `std::atomic<float> currentClipDb_ { 0.0f };` and a getter
  `float getCurrentClipDb() const noexcept { return currentClipDb_.load (std::memory_order_relaxed); }`.
- `Source/PluginProcessor.cpp` — in the Clipper stage block (when
  `clipperDriveDb > 0.0f`), track `maxPreClipAbs` across the per-channel
  per-sample loop. After the loop, store
  `currentClipDb_.store (maxPreClipAbs > 1.0f ? juce::Decibels::gainToDecibels (maxPreClipAbs) : 0.0f, std::memory_order_relaxed)`.
  When `clipperDriveDb <= 0.0f` OR `limiterActive_->get() == false`,
  store `0.0f`.
- `Source/ui/MainView.{h,cpp}` (or wherever the Clipper Drive knob lives —
  Cursor finds it; this is the existing knob added in Slice 9) — add
  underneath/next to the Clipper Drive knob:
  - A small numeric label `"Clip: X.X dB"` (1 decimal place), or `"Clip: —"`
    when `currentClipDb == 0`. Same font/size as the other meter readouts.
  - A small round red LED (e.g. 8 px) that lights opaque when
    `currentClipDb > 0` in this UI frame, and fades to fully transparent
    over ~200 ms using a held-and-decayed local float in the component.
  - Both update via the existing UI timer (whatever drives the GR meter).

**DO NOT TOUCH:**
- `shared/mdsp_dsp/include/mdsp_dsp/dynamics/LimiterEnvelope.h`
- Any bench file
- Any parameter range, ID, step, label, or skew for `release_ms`
- `release_sustain_ratio` parameter — keep declared, keep in APVTS,
  keep wired through `envelope_.setReleaseSustainRatio(...)`. Leave the
  UI slider as-is. We are not removing or hiding it in this slice; only
  documenting it as inert in commit messages.
- Any other DSP component, any other UI panel, any other param

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

### 3.1 HQ — flip the combinator

In `shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp`, in `process()`,
locate:

```cpp
const float g = std::clamp (useTransientSustainSplit ? std::min (s2, s2s) : s2, 1.0e-12f, 1.0f);
```

Replace with:

```cpp
const float g = std::clamp (useTransientSustainSplit ? std::max (s2, s2s) : s2, 1.0e-12f, 1.0f);
```

That is the entire HQ change. No other edits in the file.

### 3.2 Product — release_ms default

In `Source/parameters/Parameters.cpp`, find the `release_ms` parameter
declaration. Change the default from `100.0f` to `30.0f`. Leave range,
step, skew, label, and ID exactly as they are.

### 3.3 Product — Clip dB tracking in processBlock

In `Source/PluginProcessor.h`, alongside `currentGrDb_` / `currentTpTrimDb_`:

```cpp
std::atomic<float> currentClipDb_ { 0.0f };
```

And alongside `getCurrentGrDb()`:

```cpp
float getCurrentClipDb() const noexcept
{
    return currentClipDb_.load (std::memory_order_relaxed);
}
```

In `Source/PluginProcessor.cpp`, in `processBlock`, modify the existing
Clipper stage to track the pre-clip peak. The current block is:

```cpp
const float clipperDriveDb = clipperDriveDb_->load (std::memory_order_relaxed);
if (clipperDriveDb > 0.0f)
{
    const float driveGain = juce::Decibels::decibelsToGain (clipperDriveDb);
    for (int ch = 0; ch < nch; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            d[i] = juce::jlimit (-1.0f, 1.0f, d[i] * driveGain);
    }
}
```

Replace with:

```cpp
const float clipperDriveDb = clipperDriveDb_->load (std::memory_order_relaxed);
if (clipperDriveDb > 0.0f)
{
    const float driveGain = juce::Decibels::decibelsToGain (clipperDriveDb);
    float maxPreClipAbs = 0.0f;
    for (int ch = 0; ch < nch; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const float scaled = d[i] * driveGain;
            const float a = std::abs (scaled);
            if (a > maxPreClipAbs)
                maxPreClipAbs = a;
            d[i] = juce::jlimit (-1.0f, 1.0f, scaled);
        }
    }
    currentClipDb_.store (maxPreClipAbs > 1.0f
                              ? juce::Decibels::gainToDecibels (maxPreClipAbs)
                              : 0.0f,
                          std::memory_order_relaxed);
}
else
{
    currentClipDb_.store (0.0f, std::memory_order_relaxed);
}
```

And in the `else` branch of `if (limiterActive_->get())` (the branch
that runs when the limiter is off), add:

```cpp
currentClipDb_.store (0.0f, std::memory_order_relaxed);
```

next to the existing `currentGrDb_.store (0.0f, ...)` and
`currentTpTrimDb_.store (0.0f, ...)` lines.

### 3.4 Product — Clip readout + LED in UI

In whichever component owns the Clipper Drive knob (MainView or a child
panel — locate by searching for `"clipper_drive_db"` attachment), add
two small children next to/under the knob:

(a) **Clip dB readout** — a `juce::Label` (or paint() text, matching the
existing readout style for I/O / Drive / Ceiling). Format with one
decimal place. When `processor_.getCurrentClipDb() <= 0.0f`, draw
`"Clip: —"`. When > 0, draw `"Clip: X.X dB"`. Update from the same UI
timer as the GR meter.

(b) **Clip LED** — an 8 px round indicator drawn in paint(). Maintain a
local `float clipLedLevel_ = 0.0f;` member in the component. On each
UI tick:

```cpp
const float dt = ...;        // same dt the other meters use
const float clipDb = processor_.getCurrentClipDb();
if (clipDb > 0.0f)
    clipLedLevel_ = 1.0f;
else
    clipLedLevel_ *= std::exp (-dt / 0.2f);   // ~200 ms decay
```

Then paint a filled circle in `theme.warning` (red/orange already in the
palette) with alpha = `clipLedLevel_`. Placement: directly to the right
of, or above, the Clip dB readout. Cursor picks the cleanest position
inside the existing Clipper knob slot — must not change the panel's
overall layout / aspect.

────────────────────────────────────────
4. BUILD (no bench)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j     # Debug, existing dir
```

Both must build clean, no new warnings.

**Do NOT run the bench this slice.** Slice 5 criteria were calibrated
against the previous `min()` behaviour and the previous release_ms
default; they will fail under the new combinator and default. Bench
recalibration is Slice 9.3b after avishali signs off on audition.

────────────────────────────────────────
5. COMMITS — two separate, no push, no amend
────────────────────────────────────────

### 5.1 HQ commit

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# expect ONLY:
#    M shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp

git add shared/mdsp_dsp/src/dynamics/LimiterEnvelope.cpp

git commit -m "$(cat <<'EOF'
LimiterEnvelope: T/S combine min -> max (fast release dominates)

ADR-0005 specified the T/S split as min(s_fast, s_slow) over two
parallel cascaded one-poles. Under that operator the slow envelope wins
the entire release transient (it is always lower than the faster-rising
fast envelope), so the effective release time is release_ms ×
release_sustain_ratio, not release_ms. With the defaults of 100 ms and
ratio 4.0 the audible release is ~400 ms regardless of the release knob
position, which avishali's audition characterises as "slow compressor,
not a limiter". The release_ms parameter has no audible effect under
min(); release_sustain_ratio silently drives the whole envelope.

Switching to max(s_fast, s_slow) makes the fast envelope dominate
during release (it is always greater than the slow one as both rise
from the snap-on-attack point). Effective release is now release_ms as
the knob says. release_sustain_ratio becomes inaudible under max() and
is mathematically dead in the audio path; the parameter remains in the
APVTS with its frozen ID for compatibility and is reserved for a future
real T/S split slice where program-dependent transient detection
actually differentiates sustained from transient reduction. The current
parallel-cascade topology cannot deliver T/S character with min or max
- neither is a real T/S split.

Single-line operator change in the smoothing stage. Tight/Aggressive
already skip the T/S branch (useTransientSustainSplit = mode_ ==
Mode::Clean) and so are unchanged by this commit; their release
character improves separately through the product-side release_ms
default lowered from 100 to 30 ms (next product commit) and through
the Slice 9.2 stuck-release ext_ fix already in place.

Slice 3/4/5 bench will not be re-run this slice; criteria were
calibrated to the previous min() behaviour and need recalibration in
Slice 9.3b once avishali signs off on the audition.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

### 5.2 Product commit

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect ONLY:
#    M Source/parameters/Parameters.cpp
#    M Source/PluginProcessor.h
#    M Source/PluginProcessor.cpp
#    M Source/ui/<MainView or child component for Clipper knob>
# (and possibly .h companion for that UI component)

git add Source/parameters/Parameters.cpp Source/PluginProcessor.h Source/PluginProcessor.cpp Source/ui/*

git commit -m "$(cat <<'EOF'
release_ms default 100 -> 30 ms; Clipper dB readout + LED indicator

Brickwall-feel default for release_ms: lowered from 100 ms to 30 ms,
matching Pro-L Modern-style defaults and the new "Tight" character
mode's intended character. Range, step, skew, label, and ID
unchanged. Pairs with the HQ commit flipping the T/S combinator from
min to max so that release_ms actually drives release time (under
min, the slow envelope dominated and the knob had no audible effect).

Clipper Drive indication: avishali wants GR-meter-style feedback for
how much the clipper is cutting. Added per-block pre-clip peak
tracking in processBlock — when clipper_drive_db > 0 and any sample
exceeds 0 dBFS post-Drive-pre-clip, the excess is converted to dB and
stored in currentClipDb_ (atomic). UI exposes two child elements
under the Clipper Drive knob:
  - "Clip: X.X dB" numeric readout (or "Clip: —" when nothing is
    being clipped), updated from the existing UI timer.
  - 8 px red LED that lights on any clip activity in the current
    block and decays in UI over ~200 ms (held-and-decayed local
    float, exp(-dt/0.2) per tick).

Soft-knee and oversampling on the clipper deferred to a later slice.
For now the clipper is hard at +/- 1.0 and the readout/LED let the
user see when they're pushing it into distortion.

No bench this slice; bench recalibration in 9.3b after avishali
audition pass on the new envelope feel.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Exact one-line HQ change (before/after of the `std::clamp (... ? ... : s2, ...)` line).
2. Exact product default-value change line for `release_ms`.
3. Brief diff summary for the Clipper meter wiring (which file got
   the atomic, which UI component got the readout + LED, where they were
   placed visually).
4. Build success lines for Release and Debug.
5. HQ `git log --oneline -5`, product `git log --oneline -5`.
6. Confirmation: HQ NOT pushed; product NOT pushed; bench NOT run; no
   other files modified.
7. One screenshot or terse description of how the Clip readout + LED
   look at idle (Clip: —, LED off) and when Clipper Drive is pushed
   into clipping (Clip: ~3 dB, LED solid red).
8. Open questions.

────────────────────────────────────────
7. AFTER THIS
────────────────────────────────────────

avishali auditions in Live (Release VST3):
- Default character (Tight) with default release_ms = 30 ms should
  feel like a snappy brickwall, not a compressor.
- Switch to Clean: release follows the release_ms knob directly; T/S
  ratio knob should now have no audible effect (we'll de-emphasize or
  hide it in a future UI pass).
- Push Clipper Drive: readout reads pre-clip excess in dB, LED flashes
  red and decays; distortion at high drives is expected (no soft-knee
  / no oversampling yet).
- Compare to Pro-L 2 or another reference limiter on the same dense
  source — should be in the same character ballpark now, not
  "compressor-with-a-lookahead".

On pass → Slice 9.3b: bench recalibration. Slice 5 criteria assumed
the slow-release behaviour; we need to re-derive the expected
null-residual / IMD / loudness numbers under the new release character
and update `criteria.py` for Slice 5 (and check Slice 3/4 — they may
still pass since Clean's effective release was the only thing that
changed and the bench's pink-noise / drum-loop targets may be release-
time-tolerant).

On fail → we revisit. The next logical step would be Option B
formally: drop the slow-cascade path entirely and re-introduce real
T/S via program-dependent release detection in a dedicated slice.
