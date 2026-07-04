# SLICE — Clipper Pre/Post UI control (Pre/Post segment button)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition:** avishali → Asaf → testers
**Repos:** plugin `MasterLimiter` only. **UI-only** — the `clipper_position` param already exists and works (`ae9a675`); this just exposes it on-screen. No DSP, no param, no SDK.

> ⚠️ **Retrieval log first.** Re-confirm the `btnClipperMode_` pattern (member, attachment, onClick, `updateClipperModeButton`, placement in `resized()`, tooltip) in the current `Source/ui/MainView.{cpp,h}` before editing — this slice mirrors it exactly.

---

## Why
The clipper Pre/Post slice added `clipper_position` (Choice {Pre, Post}) as an automatable param but **no on-screen control** — avishali can't see/toggle it. Add a **Pre/Post segment button** in the clipper cluster, mirroring the existing **Hard/Soft** button (`btnClipperMode_`) exactly.

---

## Pattern to mirror — `btnClipperMode_` (Hard/Soft)
It's a 2-state segment button ([MainView.cpp:528-529, 600-608, 730-733, 1260-1266](Source/ui/MainView.cpp:528)):
- Member `btnClipperMode_` + `attClipperMode_` (`juce::ParameterAttachment`) + `lastClipperModeIdx_`.
- `setClickingTogglesState(false)`, `addAndMakeVisible`, `setName("ClipperModeSegment")`, tooltip.
- Attachment: `ParameterAttachment(*param, [cb: updateClipperModeButton(idx)])` + `sendInitialUpdate()`.
- `onClick`: `attClipperMode_->setValueAsCompleteGesture(lastClipperModeIdx_==0 ? 1 : 0)` (toggles).
- `updateClipperModeButton(int)`: sets button text "Hard"/"Soft", toggle state, repaints.
- Placed in `resized()` in the clipper cluster.

## Add — `btnClipperPosition_` (Pre/Post), identical pattern
1. **`MainView.h`:** add `juce::TextButton btnClipperPosition_;`, `std::unique_ptr<juce::ParameterAttachment> attClipperPosition_;`, `int lastClipperPositionIdx_ = 0;`, `void updateClipperPositionButton (int idx);`.
2. **Constructor:** `setClickingTogglesState(false)`, `addAndMakeVisible(btnClipperPosition_)`, `setName("ClipperPositionSegment")`, tooltip: *"Clipper position: Pre = before the limiter (input shaping); Post = after the limiter (transient catcher; TruePeak ceiling catches its inter-sample peaks)."*
3. **Attachment:** to `param::clipper_position`, callback → `updateClipperPositionButton((int) std::lround(value))`, `sendInitialUpdate()`.
4. **`onClick`:** `attClipperPosition_->setValueAsCompleteGesture(lastClipperPositionIdx_==0 ? 1.0f : 0.0f)`.
5. **`updateClipperPositionButton(int idx)`:** `const bool post = idx >= 1; lastClipperPositionIdx_ = post ? 1 : 0; btnClipperPosition_.setButtonText(post ? "Post" : "Pre"); setToggleState(post,...); repaint`.
6. **`resized()`:** place `btnClipperPosition_` in the clipper cluster next to `btnClipperMode_` (Hard/Soft) — same size/style. avishali/Cursor pick exact spot; keep the cluster tidy (may need a small relayout / a "Pre/Post" mini-label). If space is tight, put it directly under or beside the Hard/Soft segment.

---

## Non-goals
No DSP/param change (`clipper_position` exists). No change to Hard/Soft, drive, active, or readout behavior. No window resize unless the cluster genuinely can't fit the button — if so, minimal local relayout only.

## Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build --config Release 2>&1 | tail -6
bash scripts/install_user.sh build      # ⚠️ pass 'build' — bare defaults to ancient build-release
auval -v aufx MaLm Melc 2>&1 | tail -3
```
**Acceptance (Claude verifies 1–3; avishali auditions 4):**
1. Build clean, AU validates. **Install from `build`** and confirm the installed VST3 shows `clipper_position` (path-reliable check; AU loads-by-code).
2. The Pre/Post button appears in the clipper cluster, toggles the param (Pre↔Post), text tracks the value, and reflects automation/preset changes (via the ParameterAttachment callback) — exactly like Hard/Soft.
3. No regression to the other clipper controls or layout.
4. **Audition (avishali):** flip Pre/Post live, hear the character change (Post = transient catcher).

**Close gate:** update `docs/SIGNAL_FLOW.md` (clipper UI) + `docs/PROGRESS.md` + `PROMPTS/PLAN.md`; commit **plugin-only**; push (hold resolved). Archive CLOSE prompt.

## Notes for the architect (not for Cursor)
- Pure mirror of `btnClipperMode_`; risk is near-zero. The only judgment is placement in the clipper cluster.
- ⚠️ Install gotcha (just cost us a loop): `install_user.sh` bare picks `build-release` (ancient) → always `install_user.sh build`; verify installed via **VST3** (AU resolves by 4-char code, so AU-path reads the installed component, not the artefact). See [[stale-binary-gotcha]].
