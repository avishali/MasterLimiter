# Cursor Task — Slice 10.3: Maximizer UI refinement #3 (GR meter rescale, knob sizing, SP/TP toggle, Learn button, Character slider)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. UI/editor-thread only — NO DSP, NO new APVTS params,
> NO param range/default/ID changes.** Continues the uncommitted work on
> branch `slice-10-maximizer-ui-shell`. New controls remain placeholders.
> Build, **do NOT commit, do NOT push.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:
- Files in §3 only. No HQ edits, no AnalyzerPro edits.
- No DSP, no new APVTS params, no range/default/ID changes to existing params.
- RT-safety: audio thread untouched.
- Stay on branch `slice-10-maximizer-ui-shell`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. WHAT TO FIX (audition feedback)
────────────────────────────────────────

Reference `docs/mockups/SLICE10_ui_layout.svg`.

### 1.1 GR meter — single bar, finer scale
- Make the Gain-Reduction meter a **single bar with a single dB readout**
  (remove the second/duplicate bar — true L/R reduction is deferred to the
  stereo-link DSP slice and isn't available yet).
- **Rescale finer for low reduction: 0 to −10 dB**, with clear ticks at
  0 / −2 / −4 / −6 / −8 / −10. Still top-down (0 at top), no clip LED.
- Goal: small amounts of GR are clearly readable.

### 1.2 Gain & Output Level knobs — equal size
- Make the **Gain** knob and the **Output Level** knob the **same size**
  (pick a consistent large knob size for both).

### 1.3 SP / TP — toggle button, no dropdown
- Replace the ceiling-mode **ComboBox** with a **2-state toggle button**
  (Sample Peak ⇄ True Peak), since there are only two options.
- Keep it **wired to `ceiling_mode`** (use a `ButtonAttachment` to the
  existing choice parameter, or toggle its index on click). The button label
  reflects the active mode ("SP" / "TP" or "Sample Peak" / "True Peak").
- No parameter changes — same `ceiling_mode` IDs/values.

### 1.4 Learn Input Gain — small, unobtrusive
- The **Learn Input Gain** control is far too large. Make it a **small
  button** that does not compete with the I/O meters/faders.
- In the I/O panel, **meters + faders are the priority visuals** — make sure
  Learn (and the LUFS readout, and anything else) sits compactly around them
  without crowding. Meters/faders get the space.

### 1.5 Character — slider, not a block
- Replace the Character **ComboBox block** with a **horizontal slider**
  (per the SVG: "Clean" ↔ "Fast & Loud"), styled with the product LookAndFeel.
- Keep it **wired to the existing `character` parameter** (a `SliderAttachment`
  over the choice/stepped param). No parameter changes.

### 1.6 Gain/Ceiling Link restyle — DEFERRED
- Do **NOT** restyle the Gain/Ceiling Link control this round. Leave it as-is
  (just not overlapping). A nicer mockup-style control comes in a later
  UI-graphics pass.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (as needed):**
- `Source/ui/meters/GainReductionMeter.{h,cpp}` — single bar, 0..−10 scale.
- `Source/ui/MainView.{h,cpp}` — knob sizing, SP/TP toggle, Learn button size,
  Character slider, I/O panel spacing (meters/faders priority).
- `Source/ui/MasterLimiterLookAndFeel.{h,cpp}` — only if needed for the
  toggle / slider styling.

Do NOT touch `PluginProcessor`, `Parameters`, `ParameterIDs`, CMake (no new
files expected), HQ, or AnalyzerPro.

NON-GOALS: no param changes, no DSP, no Gain/Ceiling-Link restyle, faders
stay inert placeholders.

────────────────────────────────────────
3. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-10-maximizer-ui-shell
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings. No bench run (editor-only).
- Resize still clean.

────────────────────────────────────────
4. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Files modified, one-line purpose each.
3. Confirmation of each item: GR single-bar 0..−10, equal knob sizes,
   SP/TP toggle wired to `ceiling_mode`, Learn button small, Character slider
   wired to `character`, Link control left as-is.
4. Build summary (Debug + Release, warnings).
5. Confirmation: branch unchanged, NO commit, NO push.
6. Open questions.

────────────────────────────────────────
5. ARCHITECT AUDITION
────────────────────────────────────────

avishali re-auditions in Live: GR meter fine/clear at low reduction, equal
knob sizes, SP/TP toggles cleanly, Learn unobtrusive with meters/faders
dominant, Character reads as a slider. Once the look is approved, the whole
Slice 10 (shell + polish) gets committed, then ADR + Slice 11 (real I/O
gains, Gain-Match, Ceiling 0..−24).
