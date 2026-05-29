# Cursor Task — Slice 7.1: 2-channel GR meter + latched-max readouts (GR + Clip)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-only UI/processor update following Slice 7. No DSP behaviour
> change. No HQ source change. No bench impact (default link = 100 →
> envelope_L = envelope_R per fast-path → both GR atomics carry the same
> value as today's single `currentGrDb_`).**
>
> Slice 7 introduced per-channel parallel envelopes; the GR meter still
> shows a single bar / single readout, losing the per-channel
> information that is precisely what users dial Link to control.
>
> avishali asks for an Ozone-Maximizer-style GR meter: two thin
> vertical bars L | R side-by-side, with the existing current-GR
> readout (max of L/R, what the bench reports) AND a new latched
> max-GR-since-reset readout below it (click to reset, matching the
> Slice 11b1 peak-meter latched-max pattern).
>
> Same latched-max addition for the Clipper indicator: existing Clip dB
> readout stays, new latched max-Clip-since-reset readout added (click
> to reset).
>
> One product commit. No push from this slice — Slice 7 close prompt
> handles both 7 and 7.1 together once 7.1 ships.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. Product
files only this slice. No HQ source edits. No bench edits. No product
push. No amend of `f05c062` (Slice 7 product commit) — this slice
stacks on top.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

Slice 7 added `envelope_R_` alongside `envelope_L_` and gain blending
across `stereo_link_pct`. The product currently stores
`currentGrDb_ = max(envelope_L_.getLastBlockMaxGrDb(),
                    envelope_R_.getLastBlockMaxGrDb())`
and the GR meter shows this single value. When link < 100, L and R
envelopes can diverge — but the meter hides this divergence, defeating
the visual feedback users need while dialing Link.

The Clipper indicator added in Slice 9.3a shows current per-block clip
dB excess and an LED. It lacks a latched max-since-reset readout — a
useful feature when you set up a session and want to know after a
song play "how much did the clipper hit at its peak?"

Both gaps closed by this slice. Pattern reuses the latched-max-with-
click-to-reset behaviour already proven in `MeterGroupComponent.cpp`
(Slice 11b1 input/output peak meters).

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (product only):**
- `Source/PluginProcessor.h` — add per-channel GR atomics and latched-
  max atomics; add getters and reset methods.
- `Source/PluginProcessor.cpp` — `processBlock` writes per-channel GR
  and updates latched-max atomics for GR and Clip.
- `Source/ui/meters/GainReductionMeter.h/.cpp` — redesign for two L|R
  bars + current readout + latched-max readout + click-to-reset.
- `Source/ui/MainView.cpp` (or whichever component owns the Clipper
  knob row from Slice 9.3a) — add a small "Max" readout below the
  existing "Clip" readout; both clickable to reset.

**DO NOT TOUCH:**
- `shared/mdsp_dsp/...` — HQ DSP source.
- Bench files. UI/atomic-only change at default link = 100 sees no
  data difference vs Slice 7 (envelope_L == envelope_R under the
  fast-path).
- Any param ID, range, default. No new params.
- Input / output peak meters (already 2ch per earlier work).
- LoudnessAnalyzer / LUFS readouts.
- Layout / panel resize. Fit the new readouts inside existing slots.

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

### 3.1 PluginProcessor.h — atomics + getters + reset methods

Alongside the existing `currentGrDb_`:

```cpp
std::atomic<float> currentGrLDb_      { 0.0f };
std::atomic<float> currentGrRDb_      { 0.0f };
std::atomic<float> maxGrSinceResetDb_ { 0.0f };
std::atomic<float> maxClipSinceResetDb_ { 0.0f };
```

`currentGrDb_` stays — it's the existing "max-of-channels" used by
bench and any other reader. New atomics ADD to the surface, don't
replace.

Public getters (alongside existing `getCurrentGrDb()` and
`getCurrentClipDb()`):

```cpp
float getCurrentGrLDb()       const noexcept { return currentGrLDb_.load (std::memory_order_relaxed); }
float getCurrentGrRDb()       const noexcept { return currentGrRDb_.load (std::memory_order_relaxed); }
float getMaxGrSinceResetDb()  const noexcept { return maxGrSinceResetDb_.load (std::memory_order_relaxed); }
float getMaxClipSinceResetDb()const noexcept { return maxClipSinceResetDb_.load (std::memory_order_relaxed); }

void resetMaxGr()   noexcept { maxGrSinceResetDb_.store   (0.0f, std::memory_order_relaxed); }
void resetMaxClip() noexcept { maxClipSinceResetDb_.store (0.0f, std::memory_order_relaxed); }
```

### 3.2 PluginProcessor.cpp — processBlock writes

Inside `if (limiterActive_->get())`, after both envelopes have run
(either path: fast-path or full path), write per-channel atomics:

**Fast-path branch** (link ≥ 99.95, single envelope on max):

```cpp
const float gr = envelope_L_.getLastBlockMaxGrDb();
currentGrDb_.store  (gr, std::memory_order_relaxed);
currentGrLDb_.store (gr, std::memory_order_relaxed);
currentGrRDb_.store (gr, std::memory_order_relaxed);
```

L and R get the same value because the fast-path runs only one
envelope and applies its gain to both channels — identical reduction
by definition.

**Full-path branch** (link < 99.95, per-channel envelopes):

```cpp
const float grL = envelope_L_.getLastBlockMaxGrDb();
const float grR = envelope_R_.getLastBlockMaxGrDb();
currentGrLDb_.store (grL, std::memory_order_relaxed);
currentGrRDb_.store (grR, std::memory_order_relaxed);
currentGrDb_.store  (std::max (grL, grR), std::memory_order_relaxed);
```

After either branch, update the latched max:

```cpp
const float frameMaxGr = std::max (currentGrLDb_.load (std::memory_order_relaxed),
                                   currentGrRDb_.load (std::memory_order_relaxed));
const float prevMaxGr  = maxGrSinceResetDb_.load (std::memory_order_relaxed);
if (frameMaxGr > prevMaxGr)
    maxGrSinceResetDb_.store (frameMaxGr, std::memory_order_relaxed);
```

In the Clipper stage (Slice 9.3a code), after storing
`currentClipDb_`:

```cpp
const float frameClip = currentClipDb_.load (std::memory_order_relaxed);
const float prevMaxClip = maxClipSinceResetDb_.load (std::memory_order_relaxed);
if (frameClip > prevMaxClip)
    maxClipSinceResetDb_.store (frameClip, std::memory_order_relaxed);
```

In the `limiterActive == false` branch (and the clipper-disabled
branch), do NOT zero the latched max atomics — they latch across
on/off toggles by design (user resets explicitly via click). Only the
non-latched `currentGrDb_` / `currentGrLDb_` / `currentGrRDb_` /
`currentClipDb_` zero in those branches.

### 3.3 GainReductionMeter.h/.cpp — 2 bars + compact dual readout + click-to-reset

Current state (read from existing file): single vertical bar with a
"GR" label at top and one numeric readout below.

New layout per avishali screenshot — two thin bars in a dark inset
slot, single compact readout box at the bottom showing both current
and latched max in one line. No "GR" label inside the meter component
(parent panel header handles labeling, or it's removed entirely).

```
+-----------+
|  ===  === |   meter area: two thin bars L | R inside a dark inset
|  ===  === |   slot with subtle border. Top-down fill (warning colour
|  ===  === |   grows from top as reduction engages). Always-visible
|  ===  === |   background bar in theme.panel/darker so the bars are
|  ===  === |   visible at idle (matches screenshot's filled look).
+-----------+
| 3.2 / 5.1 |   single readout box: current / max, compact dual format
+-----------+
```

Layout sketch in code:

```cpp
auto bounds      = getLocalBounds();
auto readoutArea = bounds.removeFromBottom (18);    // single compact slot
auto meterArea   = bounds.reduced (4, 4);           // dark inset slot

// meter area split into two thin bars with a small gap:
const int halfW = (meterArea.getWidth() - 3) / 2;
auto leftBar  = meterArea.removeFromLeft (halfW);
meterArea.removeFromLeft (3);                       // gap
auto rightBar = meterArea;

// Draw dark inset background for the whole meter slot first (rounded
// rect, theme.background.darker, subtle border in theme.grid).
// Then draw each bar's background (theme.panel or similar — always
// visible) within leftBar / rightBar.
// Then draw the reduction overlay (theme.warning) top-down on top:
//   const float h = normaliseGr (displayGrLDb_) * bar.getHeight();
//   g.fillRect (Rectangle<float> (bar.getX(), bar.getY(), bar.getWidth(), h));
// (top-down: y starts at top, height grows with GR depth)
```

Top-down fill direction means: at GR = 0 the warning overlay has
zero height (only background bar visible); at GR = 10 dB the overlay
fills the full bar height from top. Match the existing
`normaliseGr()` helper for the scale mapping (0..-10 dB).

Each bar uses the same warning colour (`theme.warning`) for the
reduction overlay. Background bars use a colour matching the
screenshot's muted teal-grey (try `theme.panel` first; if that's too
light against the inset slot, use `theme.background.brighter (0.05f)`
or a similar low-contrast value).

**Compact dual readout** in the single bottom slot:

```cpp
const auto current = formatDbBare (displayCurrentGrDb_);  // "3.2"
const auto maxVal  = formatDbBare (displayMaxGrDb_);      // "5.1"
const auto readoutText = current + " / " + maxVal;

g.setColour (theme.textMuted.withAlpha (0.85f));
g.setFont (type.labelFont().withHeight (11.0f));
g.drawText (readoutText, readoutArea, juce::Justification::centred, true);
```

Use a `formatDbBare` helper (no " dB" suffix to keep it compact) —
follows the same naming pattern from the Slice 8.1 meter-readout
truncation fix. If the current value is 0 and max is 0, show
`"— / —"` instead of `"0.0 / 0.0"`.

Optional subtle styling: draw the current value slightly brighter
than the max (`theme.text` vs `theme.textMuted`) to disambiguate at a
glance.

**Click-to-reset** — replicate Slice 11b1's `MeterGroupComponent::
mouseDown` pattern. The entire readout slot is the reset hit region
(simpler than splitting by horizontal position when the values are
that close together):

```cpp
void mouseDown (const juce::MouseEvent& e) override
{
    if (readoutBounds_.contains (e.getPosition()))
        processor_.resetMaxGr();
}
```

Store `readoutBounds_` as a member updated in `resized()`. Set
`setMouseCursor(juce::MouseCursor::PointingHandCursor)` on the
readout subregion via a child component or hit-test override —
helps UX discoverability that the readout is clickable.

The sync function (`sync(dtSec)` from Slice 8 timer pattern) now
reads four atomics:

```cpp
const float rawL    = std::abs (processor_.getCurrentGrLDb());
const float rawR    = std::abs (processor_.getCurrentGrRDb());
const float rawMax  = std::abs (processor_.getMaxGrSinceResetDb());

displayGrLDb_       += (rawL - displayGrLDb_) * a;
displayGrRDb_       += (rawR - displayGrRDb_) * a;
displayCurrentGrDb_  = std::max (displayGrLDb_, displayGrRDb_);
displayMaxGrDb_      = rawMax;   // latched, no smoothing

repaint();
```

`displayGrLDb_` / `displayGrRDb_` drive the two bar overlays.
`displayCurrentGrDb_` is the left value in the dual readout (max of
L/R, smoothed). `displayMaxGrDb_` is the right value (latched).

`resetPeakHolds()` already exists — extend it to also call
`processor_.resetMaxGr()` so external "reset all peaks" gestures
clear the latched max too.

### 3.4 Clipper indicator — compact dual readout to match GR meter

Locate the Clipper Drive knob's UI block (added in Slice 9.3a). It
currently has:
- "Clip: X.X dB" or "Clip: —" numeric readout
- 8 px round LED with `theme.warning`, decays over ~200 ms

Restyle the readout to match the new GR meter's compact dual format:

```
Clip 3.2 / 5.1   [LED]
```

Where `3.2` is current pre-clip excess (existing) and `5.1` is the
new latched max-clip-since-reset. Same `formatDbBare` helper, same
" / " separator. "Clip" label retained inline on the left so the user
knows what they're reading.

When both current and max are 0, draw `"Clip — / —"` instead of zeros.

LED placement and decay behaviour unchanged.

**Click-to-reset** on the readout text area → `processor_.resetMaxClip()`.
Same `setMouseCursor(PointingHandCursor)` hint as the GR readout. The
LED is non-interactive (no click target conflict).

Maintain the same font and size as the existing Clip readout so the
total height of the indicator block doesn't change — important for
not disturbing the Maximizer panel layout.

### 3.5 Optional: "Reset all" gesture

If a global "reset all peaks" action exists elsewhere (the Slice
11b1 input/output peak meters had click-to-reset; check whether they
expose a "reset all" via double-click or a header gesture), wire it
to also call `resetMaxGr()` and `resetMaxClip()` so a single user
gesture clears every latched value.

If no such global gesture exists, leave it — per-meter clicks work.

────────────────────────────────────────
4. BUILD
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean. No new warnings.

────────────────────────────────────────
5. BENCH (no run, no recalibration)
────────────────────────────────────────

UI/atomic-only change. At default link = 100 the fast-path runs as
today, all GR atomics carry the same value, `currentGrDb_` is
unchanged from Slice 7. Slice 3/4/5 bench remains bit-exact PASS;
no rerun required this slice.

If Cursor wants to spot-check, a single Slice 3 quick run is
sufficient — must PASS 13/13.

────────────────────────────────────────
6. PRODUCT COMMIT (separate, no push, no amend)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect:
#    M Source/PluginProcessor.h
#    M Source/PluginProcessor.cpp
#    M Source/ui/meters/GainReductionMeter.h
#    M Source/ui/meters/GainReductionMeter.cpp
#    M Source/ui/MainView.cpp           (Clipper Max readout)

git add Source/PluginProcessor.h Source/PluginProcessor.cpp \
        Source/ui/meters/GainReductionMeter.* \
        Source/ui/MainView.cpp

git commit -m "$(cat <<'EOF'
MasterLimiter Slice 7.1: 2ch GR meter + latched-max readouts (GR + Clip)

Closes the per-channel GR visibility gap left by Slice 7. The single
GR bar hid envelope_L vs envelope_R divergence under link < 100,
which is exactly the feedback users need when dialing Link.

Processor:
- New atomics: currentGrLDb_, currentGrRDb_, maxGrSinceResetDb_,
  maxClipSinceResetDb_.
- Existing currentGrDb_ retained as "max-of-channels" for bench and
  other consumers; new per-channel atomics ADD to the surface.
- processBlock writes per-channel GR in both fast-path (link >= 99.95;
  L == R since one envelope drives both channels) and full-path
  branches. Latched max updates on every frame; persists across
  limiter on/off toggles by design (user resets via click).
- Getters: getCurrentGrLDb / getCurrentGrRDb / getMaxGrSinceResetDb /
  getMaxClipSinceResetDb. Reset methods: resetMaxGr / resetMaxClip.

GainReductionMeter UI:
- Two thin vertical bars (L | R) replace the single bar. Same 0..-10
  dB scale and warning fill style as today.
- Current GR readout (max of L/R, smoothed) retained below the bars.
- New latched "Max X.X dB" readout below the current readout. Click
  on the Max readout area resets to 0 dB. Hand cursor on hover.

Clipper indicator:
- Existing "Clip: X.X dB" readout and LED unchanged in placement.
- New "Max" readout added below the Clip readout, latched, click to
  reset. "Max -" when latched value is 0.

Pattern matches Slice 11b1's input/output peak meters (latched
max-since-reset with click-to-reset). No new params, no bench
change, no DSP behaviour change. Default link = 100 produces
identical bench results to Slice 7 (envelope_L == envelope_R under
fast-path).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

**Do NOT push.** Slice 7 close prompt will be revised to cover Slice
7 + 7.1 together in a single PROGRESS entry.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Header diff: new atomics + getters + reset methods.
2. processBlock diff: per-channel GR writes (both branches) + latched
   max updates.
3. GainReductionMeter diff summary: layout change, two-bar drawing,
   two-readout drawing, click-to-reset wiring.
4. Clipper UI diff summary: Max readout placement, click-to-reset.
5. Build success lines for Release and Debug.
6. Product `git log --oneline -3` showing the new Slice 7.1 commit on
   top of `f05c062`.
7. Confirmation: HQ NOT touched; product NOT pushed; bench NOT run;
   no parameters added or modified.
8. Brief visual description of how the new meter looks at idle (both
   bars empty, "0.0 dB" current, "Max —" latched) vs under heavy
   limiting (asymmetric L|R bars at link < 100, current shows max,
   Max shows historical peak).
9. Open questions.

────────────────────────────────────────
8. AFTER THIS
────────────────────────────────────────

avishali auditions in Live (Release VST3):

- **Default link = 100**: both bars should track identically (since
  envelope_L == envelope_R under fast-path). Current readout matches
  Slice 7 behaviour exactly. Max readout latches deepest GR seen,
  persists across playback stops.
- **Sweep Link 100 → 0**: L|R bars start diverging as link drops.
  Asymmetric stereo material clearly shows which channel is taking
  reduction. Current readout follows the deeper channel.
- **Click on "Max" GR readout**: resets to 0.0 dB. Re-engages on next
  reduction.
- **Clipper push**: Clip readout tracks current pre-clip excess, Max
  latches the peak excess seen in the session. Click "Max" to reset.
- **Layout fit**: no panel resize, both readouts fit in existing
  meter slot. Font readable at standard plugin scale.

On audition pass → **revised Slice 7 close** prompt runs, covering
both Slice 7 and Slice 7.1 in one PROGRESS entry, one PLAN update,
one prompts-archive commit, two pushes (HQ ADR-0008 + product Slice
7 + 7.1).

On partial pass (layout issue, click-target too small, readout
overlap) → small UI tweak slice within 7.1 before close.
