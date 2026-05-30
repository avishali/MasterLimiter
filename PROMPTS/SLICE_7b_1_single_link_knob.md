# Cursor Task — Slice 7b.1: collapse to ONE Link knob + mode toggle

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **UI-only refinement on the uncommitted Slice 7b branch.** Replace the two
> link knobs (Stereo Link + M/S Link) with a **single "Link" knob** that the
> Stereo/M-S mode toggle re-targets: in Stereo mode it drives `stereo_link_pct`,
> in M/S mode it drives `ms_link_pct`. Both frozen params stay (each keeps its
> own value, recalled on mode switch). No DSP/param/HQ change. **Do NOT
> commit, do NOT push.**

> Continue product branch `slice-7b-ms-link`.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product UI edit only: `Source/ui/MainView.{cpp,h}`. No DSP, no params, no HQ,
  no ADR. Both `stereo_link_pct` and `ms_link_pct` remain frozen and are still
  read by the engine per mode — do not touch the processor.
- **Do NOT commit, do NOT push.** Leave `MCP` untouched.

────────────────────────────────────────
1. WHAT TO CHANGE
────────────────────────────────────────

Today (MainView): `sldStereoLink_`↔`stereo_link_pct` (`attStereoLink_`),
`sldMsLink_`↔`ms_link_pct` (`attMsLink_`), `cmbStereoMode_`↔`stereo_mode`, with
`updateStereoModeControls()` greying the inactive knob.

Collapse to one knob:

- **Keep a single "Link" knob** — reuse `sldStereoLink_` (label stays `"Link"`).
- **Remove the second knob from the UI:** stop `addAndMakeVisible`-ing
  `sldMsLink_` / `lblMsLink_` and drop them from the layout. (You may delete
  the now-unused `sldMsLink_`/`lblMsLink_` members, or leave them unused — but
  do NOT remove the `ms_link_pct` parameter.)
- **Mode-switched binding** for the single knob:
  - Replace the two static attachments with **one** dynamic
    `SliderAttachment` (`attLink_`) on `sldStereoLink_`, pointing at the
    **active mode's** param: `stereo_link_pct` when `stereo_mode == Stereo`,
    `ms_link_pct` when `== M/S`.
  - In `updateStereoModeControls()` (called from `cmbStereoMode_.onChange` and
    at init): **rebuild** the attachment for the active param —
    `attLink_.reset();` then
    `attLink_ = std::make_unique<SliderAttachment>(apvts_, pid(activeId), sldStereoLink_);`.
    The SliderAttachment constructor syncs the slider to the new param's
    current value, so each mode recalls its own Link value. (Destroy the old
    attachment before creating the new — never bound to both at once.)
  - Remove `attMsLink_` and the old static `attStereoLink_` (replaced by
    `attLink_`).
- **No more greying** — there is one knob, always active. Remove the
  enable/disable logic from `updateStereoModeControls()` (it now only swaps the
  attachment). The `cmbStereoMode_` toggle stays.
- **Layout:** the link cluster is now one Link knob + the Stereo Mode toggle
  (instead of two knobs + toggle). Re-lay it cleanly; close the gap left by the
  removed knob. avishali audits exact placement.
- Tooltip: make `sldStereoLink_`'s tooltip mode-agnostic (e.g. "Link amount for
  the active stereo mode: 100% fully linked, 0% independent."). Keep the mode
  combo's tooltip.

────────────────────────────────────────
2. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings.
- UI-only → Slice 3/4/5 unchanged (optional sanity).
- In a host:
  - One Link knob + a Stereo/M-S toggle (no second knob, no greyed control).
  - Stereo mode: Link knob drives L/R link; switch to M/S: the knob now drives
    the M/S link and **shows the M/S value**; switch back: the L/R value
    returns (each mode remembers its own setting).
  - Automation still works (host sees both params; the knob reflects the active
    one). No clicks on mode switch.

────────────────────────────────────────
3. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. MainView diff: single Link knob, dynamic `attLink_` swap on mode change,
   removed second knob + greying, layout update.
3. Build summary (Debug + Release, warnings).
4. Confirmation: mode switch recalls per-mode Link value; both params intact.
5. `git status --short` (slice-7b branch, no commit).
6. Open questions (link-cluster placement).

────────────────────────────────────────
4. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali auditions: one Link knob, mode toggle re-targets it, per-mode value
recall, clean layout. On approval → architect closes Slice 7b (product + the
ADR-0008 addendum + submodule bump + PROGRESS/PLAN + prompt archive). **Do not
self-close.**
