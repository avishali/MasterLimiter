# Cursor Task — Slice 14.6: user-facing "Color" control (band link as a parameter)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product feature slice, on the existing Slice 14 branches.** The 2-band
> multiband auditioned well vs Ozone; avishali wants the band-decoupling
> amount exposed as a user control rather than frozen to one value. This
> slice turns the internal `kBandLink` constant into a new frozen
> `AudioParameterFloat` "Color" (0–100%, default 50%), smoothed, driving the
> existing band-gain blend, plus a UI knob with detents at 0/50/100%. Headroom
> stays fixed at 2 dB (not exposed this slice). **Do NOT commit, do NOT push.**

> **Continue on the existing Slice 14 branches**: product
> `slice-14-multiband-2band`, HQ `slice-14-multiband-bench`. No new branches.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product edits permitted: `ParameterIDs.h`, `Parameters.cpp`,
  `PluginProcessor.{h,cpp}`, `ui/MainView.{h,cpp}` (and the LookAndFeel only if
  needed for the knob detents).
- HQ `shared/dsp_bench/` edits permitted only for the default-config recal
  (§5). No `mdsp_dsp` source edits. No ADR edits (architect handles the
  ADR-0009 §6/R revision at close).
- **New frozen parameter ID** `band_color` — once shipped it cannot be removed
  or renamed (system rules). Implement exactly as specified.
- RT-safety: the param is read per block and **smoothed per sample**
  (`juce::LinearSmoothedValue`) to avoid zipper noise; no audio-thread
  allocation/logging/locks.
- **Do NOT commit, do NOT push.** `MCP` stays dirty.

────────────────────────────────────────
1. TRINITY (retrieval)
────────────────────────────────────────

Read and log:
- `Source/parameters/ParameterIDs.h` — the frozen-ID convention; how existing
  IDs (e.g. `stereo_link_pct`, `character`, `clipper_mode`) are declared.
- `Source/parameters/Parameters.cpp` — how `stereo_link_pct`
  (`AudioParameterFloat`, %-formatted) and the choice params are constructed +
  default/range/attributes.
- `Source/PluginProcessor.{h,cpp}` — the `MDSP_BAND_LINK` macro + `kBandLink`
  constant (header ~14–15, 105); the band-gain blend
  (`gLowOut`/`gHighOut`, ~637–642); how other params are cached/read in
  `processBlock` (e.g. `stereoLinkPct_`).
- `Source/ui/MainView.{h,cpp}` — how existing knobs (Drive/Ceiling) and the
  stereo Link knob + Character selector are laid out and attached
  (`SliderAttachment` / `ComboBoxAttachment`), and the LookAndFeel.

Output the retrieval log.

────────────────────────────────────────
2. PARAMETER (frozen ID — exact spec)
────────────────────────────────────────

Add ONE new frozen parameter:

- **ID:** `band_color`
- **Type:** `juce::AudioParameterFloat`
- **Display name:** `"Color"`
- **Range:** `0.0 .. 100.0`, step `0.1`
- **Default:** `50.0`
- **Unit/suffix:** `"%"`

Declare the ID in `ParameterIDs.h` next to the other frozen IDs (same
convention), and construct it in `Parameters.cpp` alongside `stereo_link_pct`
(mirror its float/percentage attributes).

────────────────────────────────────────
3. DSP — Color drives kBandLink (smoothed)
────────────────────────────────────────

### 3.1 Mapping

```
kBandLink = 0.5f * (1.0f - color / 100.0f)
// Color 100% -> link 0.00 (Open / brightest, most de-pump)
// Color  50% -> link 0.25 (Balanced, default)
// Color   0% -> link 0.50 (Glued / warmest)
```

### 3.2 Replace the constant with a smoothed param read

- Remove the `static constexpr float kBandLink` (header) and the
  `MDSP_BAND_LINK` macro guard — `kBandLink` is now runtime. **Keep**
  `MDSP_BAND_HEADROOM_DB` / `kBandHeadroomDb` as-is (headroom stays fixed at
  2.0; not exposed this slice).
- Add a cached pointer `bandColor_` (the `std::atomic<float>*` raw value, like
  `stereoLinkPct_`) and a `juce::LinearSmoothedValue<float> bandLinkSmoothed_`.
- In `prepareToPlay`: `bandLinkSmoothed_.reset (osSampleRate, 0.02);` (≈20 ms
  ramp) and initialise it to the current mapped value.
- In `processBlock`, once per block: read `band_color`, map to target
  `kBandLink` per §3.1, `bandLinkSmoothed_.setTargetValue (targetLink);`.
- In the band-gain blend loop, take `const float bl = bandLinkSmoothed_.getNextValue();`
  **per sample** and use it in place of the old constant:
  ```cpp
  gLowOut[i]  = bl * gLinked + (1.0f - bl) * gainLow[i];
  gHighOut[i] = bl * gLinked + (1.0f - bl) * gainHigh[i];
  ```
  (The smoothed value advances once per output sample; ensure exactly one
  `getNextValue()` per sample so the ramp tracks correctly.)

Everything else in the multiband chain (serial two-lookahead, headroom, the
wideband brickwall) is unchanged.

────────────────────────────────────────
4. UI — "Color" knob with detents
────────────────────────────────────────

- Add a rotary "Color" knob to `MainView`, styled with the existing
  LookAndFeel, placed in the tone-shaping cluster near the Character selector
  / stereo Link knob (architect + avishali will audit exact placement — keep
  it consistent with the clean/dark/teal aesthetic; the layout must stay clean
  on resize).
- Attach via `juce::AudioProcessorValueTreeState::SliderAttachment` to
  `band_color`.
- **Detents at 0 / 50 / 100%**: the param stays continuous, but the slider
  snaps to those three marks when released near them (small snap window, e.g.
  ±2–3%), with free movement elsewhere; fine control via double-click-to-type
  or modifier-drag. Draw subtle tick marks at 0/50/100 with small labels
  (e.g. `Glued` / `Balanced` / `Open`, or just ticks if labels crowd the
  layout). **Priority:** continuous behaviour + the three visible detents;
  elaborate snapping is nice-to-have, not required.
- Readout shows the percentage (e.g. `Color 50%`).

No other UI changes.

────────────────────────────────────────
5. BENCH — gate at the DEFAULT (Color 50% = link 0.25, headroom 2)
────────────────────────────────────────

The default Color (50% → link 0.25, headroom 2.0) is the shipped recall-zero
state, so it is what Slice 3/4/5 gate against. Note this exact config
(link 0.25 / headroom 2.0) was NOT in the 14.5 grid — measure it now.

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j      # default build: Color defaults to 50%
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE14_6_REGRESSION_S$SLICE"
done
```

- Re-tighten the Slice 14 threshold loosenings to the **smallest that pass at
  the default Color**. The always-on crossover legitimately raises null
  residual (allpass) and steady-state IMD / SP-over count (2-band character) —
  loosen those treatment-B with `# ADR-0009: 2-band default (Color 50%)`
  comments, minimally. IMD/THD direction must be honest (do not loosen beyond
  what the default config actually needs). Report before→after for every
  threshold.
- **Sanity at the extremes** (no full gate, just confirm nothing breaks): build
  Color=0 and Color=100 variants (override the param default via a temp build
  flag, or run the bench with the param set if the harness supports it) and
  confirm Slice 3 SP overs stay bounded (no ceiling blow-up) at both ends.
- Optional: re-run the pumping diagnostic at the default to record the shipped
  HF-ducking number for PROGRESS.

────────────────────────────────────────
6. BUILD & VERIFY
────────────────────────────────────────

- Debug + Release build clean, zero new `Source/` warnings.
- Load in a host: the "Color" knob appears, defaults to 50%, automates,
  recalls with the session, and audibly changes brightness/de-pump as moved
  (no zipper noise when sweeping — confirms the smoothing).
- Latency unchanged from 14.4 (541 samples; the param does not affect latency).

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Param diff (`ParameterIDs.h`, `Parameters.cpp`) — the frozen `band_color`.
3. DSP diff — smoothed `kBandLink` from `band_color`; macro/constant removal.
4. UI diff — the Color knob + attachment + detents.
5. Build summary (Debug + Release, warnings).
6. Slice 3/4/5 at default Color: before→after thresholds, the recal comments,
   confirmation overs are bounded; extremes sanity (Color 0 / 100).
7. Shipped default HF-ducking number (optional).
8. `git status --short` product + HQ.
9. Open questions (incl. any concern with the `band_color` name before it
   freezes, and proposed UI placement).

────────────────────────────────────────
8. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

Architect reviews scope/RT-safety/frozen-ID. avishali auditions the Color
knob across its range on real material (brightness/de-pump sweep, no zipper,
UI placement clean). On approval → architect revises ADR-0009 (§6 was
"automatic, zero params" → now exposes Color; + the §5 latency revision) and
writes the Slice 14 consolidated close, including **cleanup of all audition
scaffolding**: the `build-rel-*` / `build-coexist-*` dirs and the extra
installed `MasterLimiter L025/L050.vst3` bundles. **Do not self-close, do not
edit ADR-0009.**
