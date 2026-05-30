# Cursor Task — Slice 16.1: Ozone-style type-in + clipper toggle + layout moves

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **UI refinement on the uncommitted Slice 16 branch** (`slice-16-ui-interaction`).
> From avishali's audition: (1) replace the always-on editable text boxes with
> Ozone-style click-to-edit, (2) Clipper Hard/Soft as a toggle not a dropdown,
> (3) several layout moves. No DSP/param change (clipper_mode stays the same
> Choice param, just presented as a toggle). **Do NOT commit, do NOT push.**

> Button *look* and palette/type are a separate visual pass (16b) — here, just
> get the structure/interaction right; styling polish follows.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product UI edits only: `Source/ui/MainView.{cpp,h}` and, if needed for the
  type-in interaction or the toggle, `Source/ui/MasterLimiterLookAndFeel.{cpp,h}`.
- No parameter changes, no DSP, no HQ. `clipper_mode` stays an
  `AudioParameterChoice` (Hard/Soft) — only its UI presentation changes.
- Continue branch `slice-16-ui-interaction`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. TYPE-IN → OZONE-STYLE (click-to-edit, not always-on boxes)
────────────────────────────────────────

The Slice 16 always-editable text boxes read cluttered. Change to Ozone-style:

- The knob's value shows as a **clean read-only value label** (no form-field
  look) — part of the knob, using the existing suffix formatting.
- **Clicking the value (knob centre) enters edit mode**: an inline editor
  appears in place showing the current value; the user types, commits on
  Enter / focus-out, cancels on Esc. Parse units + clamp to range as in
  Slice 16.
- Otherwise the editor is hidden — no persistent text box.
- Apply consistently to all value knobs/faders that got type-in in Slice 16.
- Implement via the cleanest JUCE route (e.g. read-only `Slider` text box +
  a click handler that shows a transient `TextEditor` centred on the knob, or
  a small reusable helper in the LookAndFeel). Keep it consistent across
  controls.

────────────────────────────────────────
2. CLIPPER HARD/SOFT → TOGGLE
────────────────────────────────────────

Replace the Clipper mode **dropdown/ComboBox** with a **2-state toggle /
segmented control** (Hard | Soft), bound to the existing `clipper_mode` Choice
parameter (index 0 = Hard, 1 = Soft). Match the interaction style of the
existing Stereo-Mode control. No param change — only the widget + its
attachment (use a parameter attachment appropriate for a 2-state toggle).

────────────────────────────────────────
3. LAYOUT MOVES (audit placement with avishali)
────────────────────────────────────────

Reposition (find the members by their labels/params; retrieve first):

- **Imaging / Stereo-Mode (L/R ↔ M/S)** → move to **center** of its cluster.
- **Gain-Match (Auto/Track)** → move **a little down and centered**.
- **Color** (`band_color`) → place **directly below the Clipper** control.
- **SP/TP (`ceiling_mode`) + Gain⇄Ceiling Link (`gain_ceiling_link`) +
  Limiter On (`limiter_active`)** → relocate into the **space between the Gain
  (Drive) and Ceiling knobs**, and **size them so all three fit nicely** in
  that gap (consistent button sizing, aligned).

Keep the window size and clean/dark/teal aesthetic; layout stays tidy on
resize. These are directional — avishali audits exact pixels in-host.

────────────────────────────────────────
4. TRINITY / RETRIEVAL
────────────────────────────────────────

Read `Source/ui/MainView.{cpp,h}` to map the controls to members:
the Drive/Ceiling knobs, `ceiling_mode` (SP/TP), `gain_ceiling_link`,
`limiter_active`, `band_color` (Color), the Stereo-Mode control, the
Gain-Match (Auto/Track) control, the Clipper mode control, and the value
sliders that got type-in in Slice 16. Output the retrieval log.

────────────────────────────────────────
5. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings.
- UI-only → Slice 3/4/5 unchanged (optional).
- In a host: knob values show as clean labels, clicking the centre opens an
  inline editor (type + Enter commits, Esc cancels, units parse, clamps);
  Clipper is a Hard|Soft toggle; the moved controls sit where described
  (Imaging centered, Gain-Match down+centered, Color below Clipper, and
  SP/TP + Gain⇄Ceiling Link + Limiter On fit neatly between the Gain & Ceiling
  knobs).

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log (control → member map).
2. Diffs: type-in interaction change; clipper toggle; the layout moves.
3. Build summary (Debug + Release, warnings).
4. `git status --short` (slice-16 branch, no commit).
5. Open questions (placement specifics for avishali's audit).

────────────────────────────────────────
7. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali audits the type-in feel, the clipper toggle, and the new placement.
Iterate placement by eye. Then Slice 16 (incl. 16.1) closes, and **Slice 16b**
(visual: button restyle + palette + typography + contrast + spacing) follows
per the architect's spec. **Do not self-close.**
