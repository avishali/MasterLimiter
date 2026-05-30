# Cursor Task — Slice 15b.1: meter refinements (GR timing, scale colour, 48 dB, Peak/RMS toggle)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Refinement pass on the uncommitted Slice 15b work + a small HQ enum add.**
> From avishali's audition: (1) GR meter should track attenuation timing tightly
> (faster release), (2) the I/O centre scale should be one uniform colour with
> no red above 0 dBFS, (3) add a 48 dB range to the +/- cycle, (4) add a
> user toggle so the I/O meters are sample-peak-only by default, with RMS opt-in.
> **Do NOT commit, do NOT push.**

> **Branches:** continue product `slice-15b-io-meter-features` (uncommitted).
> For the HQ enum add (item 3), make the edit in the **sibling** working copy
> `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq` on a new branch
> `slice-15b-meter-scale` off `master` — the product build consumes HQ from the
> sibling (`MELECHDSP_HQ_ROOT`), so it picks the change up directly. Submodule
> bump happens at the 15b close, not here.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product edits: `Source/ui/meters/MeterComponent.{cpp,h}`,
  `Source/ui/meters/MeterGroupComponent.{cpp,h}`, `Source/ui/MainView.{cpp,h}`,
  `Source/ui/meters/GainReductionMeter.cpp`.
- HQ edit (item 3 only): sibling `shared/mdsp_ui/include/mdsp_ui/meters/MeterRenderState.h`
  + `shared/mdsp_ui/src/meters/MeterRenderStateProvider.cpp`. No other HQ files.
- No audio/param/ADR changes. The Peak/RMS toggle and range are UI display
  prefs, not parameters.
- **Do NOT commit, do NOT push.** Leave the HQ `MCP` pointer untouched.

────────────────────────────────────────
1. GR METER — tighter, literal timing
────────────────────────────────────────

In `GainReductionMeter.cpp`, the GR live-display `MeterBallisticsConfig`:
change **`releaseMs` from ~300 to 50** (keep `attackMs ≈ 1` instant). Keep the
per-channel **peak-hold** markers as-is (`holdMs 1000`, falloff ~12 dB/s) — the
fast bar shows the exact moment/depth of each reduction; the marker still holds
the max. No other GR change.

────────────────────────────────────────
2. I/O CENTRE SCALE — uniform colour, no red zone
────────────────────────────────────────

In the shared centre dB scale (MainView) added in Slice 15b: draw **all** ticks
+ labels in **one uniform theme colour** (e.g. `theme.grid` / a neutral label
colour). **Remove any red/`theme.warning` colouring** for the 0 dB line or the
above-0 region — no red zone on the scale. (Scope: the centre scale only; do
not change the meter bars' own over/clip colouring.)

────────────────────────────────────────
3. ADD 48 dB RANGE
────────────────────────────────────────

### 3a. HQ (sibling, branch `slice-15b-meter-scale`)
- `MeterRenderState.h`: add `Top48Db` to `enum class MeterScaleMode` (append a
  new value, e.g. `Top48Db = 4` — do NOT renumber existing values).
- `MeterRenderStateProvider.cpp`:
  - `scaleMinDb`: `case MeterScaleMode::Top48Db: return -48.0f;`
  - `scaleMaxDb`: add `Top48Db` to the `return 0.0f;` group (with Top24/12/6).
  - `normaliseDb` already handles zoomed bands generically via
    `scaleMinDb`/`scaleEndDb` — no change needed, but verify Top48 maps right.

### 3b. Product
- `MeterComponent::dbToNormForScale`: add a `Top48Db` case mirroring the zoomed
  mapping (min −48, same 0.9/0.1 split as the others).
- `MainView` helpers: add `Top48Db` to `scaleLabel` (→ "48"), and update the
  **cycle order** so +/- steps **Full → 48 → 24 → 12 → 6** (5 modes):
  `scaleFromIndex`/`scaleIndex` map cycle position 0..4
  (0=Full,1=Top48,2=Top24,3=Top12,4=Top6); update the +/- clamp bounds to 0..4.
- `appendScaleTicks`: add a `Top48Db` case with sensible ticks
  (e.g. 0 / −12 / −24 / −36 / −48). Ensure the centre-scale ticks still align
  with the bars at this range.

────────────────────────────────────────
4. PEAK / RMS USER TOGGLE (default = peak only)
────────────────────────────────────────

- Add a shared **"RMS" toggle** button in MainView near the I/O meters (style
  with the LookAndFeel, like the +/- buttons). **Default OFF** (sample-peak
  only).
- Add `void setShowRms (bool)` to `MeterGroupComponent` → `MeterComponent`
  controlling whether the **RMS fill + RMS numeric** are drawn. When OFF: show
  only the peak bar/cap + peak numeric (clean classic meter). When ON: add the
  RMS fill + RMS numeric (the Slice 15b behaviour).
- The toggle drives **both** I/O meters together (shared, like range).
- RMS is still measured in the engine regardless (cheap); the toggle only
  controls display. Persistence optional (default OFF each session is fine).

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq && git checkout -b slice-15b-meter-scale   # HQ enum add lives here
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j      # builds against the sibling HQ (MELECHDSP_HQ_ROOT) → picks up Top48Db
cmake --build build-release -j
```
- Zero new warnings.
- No audio change → Slice 3/4/5 quick unchanged (run as sanity).
- In a host: GR bar now snaps and falls fast (you can read each limiting event),
  peak marker still holds. Centre scale is one colour, no red. +/- cycles
  Full/48/24/12/6 (both meters together), 48 dB ticks align. RMS toggle OFF by
  default (peak only); enabling it adds RMS fill + numeric on both meters.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Diffs: GR ballistics release; centre-scale colour; HQ Top48Db (enum +
   provider) + product 48 dB wiring; Peak/RMS toggle.
3. Build summary (Debug + Release, warnings).
4. Slice 3/4/5 quick: unchanged.
5. `git status --short` on product (slice-15b branch) AND sibling HQ
   (slice-15b-meter-scale branch) — both uncommitted.
6. Open questions (button placement for the RMS toggle, 48 dB tick choices,
   GR release feel at 50 ms).

────────────────────────────────────────
7. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali re-auditions: GR timing reads the limiting clearly; scale colour; 48 dB
range; Peak/RMS toggle. On approval → architect writes the Slice 15b
consolidated close (product + HQ commit + submodule bump + PROGRESS/PLAN +
prompt archive). **Do not self-close.**
