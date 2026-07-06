# SLICE UI-2.1 — Transparent-engine consistency guard (display == DSP)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (state-load behaviour) · **Audition:** avishali
**Repo/scope:** plugin `MasterLimiter` — parameter/state handling + (if needed) the selector sync in `DevControlsComponent`. Tiny (~a few lines). No DSP algorithm change. **Read `docs/ENGINE_NAMING.md`.**
**Why:** UI-2 hides the Smart/Adaptive release options and the selector only shows **Transparent / Open**. But `dev_release_engine` can still be **Smart/Adaptive** via a preset, saved session, or automation while `dev_mb_engine = OFF` — so the UI shows **Transparent** while the DSP runs **Smart**. That's exactly the mislabeled-engine confusion we're removing before Asaf's alpha.

## The invariant to enforce
> **`dev_mb_engine == OFF`  ⟹  `dev_release_engine == Lookahead`.**
(While Smart/Adaptive are hidden in alpha, "Transparent" must ALWAYS mean the Lookahead release. Open is unaffected.)

## Where to enforce (message thread only — NO audio-thread param writes, RT §3)
Enforce on BOTH entry points so it holds regardless of the UI being open:
1. **State load** — in `setStateInformation` (after the APVTS state is applied): if `dev_mb_engine == OFF` and `dev_release_engine != Lookahead`, set `dev_release_engine = Lookahead`.
2. **Runtime change** — a parameter listener (message thread) on `dev_mb_engine` (and optionally `dev_release_engine`): when `dev_mb_engine` is OFF and `dev_release_engine` becomes/stays Smart/Adaptive, coerce it back to Lookahead.

Use the normal APVTS setter (`getParameter(...)->setValueNotifyingHost(...)` or the same path the selector already uses) on the message thread — do NOT write params from `processBlock`. If the DevControlsComponent selector's existing `dev_mb_engine` ParameterAttachment is the natural place for #2, that's fine.

## Keep it easy to revert (temporary alpha guard)
- Wrap it with a clear comment: `// ALPHA GUARD: Smart/Adaptive hidden from selector — remove when Smart becomes a selectable engine (see docs/ENGINE_NAMING.md).`
- When we build the Smart engine and add it to the selector as a 3rd option, this guard is deleted (or gated off). Do NOT delete the Smart/Adaptive params or code — only the guard is temporary.

## Non-goals
- No DSP change, no param removal, no new param, no UI layout change (UI-2 owns layout).

## Build/verify
- Build clean, AU+VST3.
- (Claude) Load a state with `dev_mb_engine=OFF` + `dev_release_engine=Smart` → after load, `dev_release_engine` reads **Lookahead** and the selector shows **Transparent**; the render uses Lookahead (Transparent), not Smart. Open engine unaffected (mb on leaves release engine alone). No audio-thread param writes.

## Output requirements
1. Retrieval log (setStateInformation + param-listener location). 2. Diff. 3. Confirm message-thread-only. 4. Confirm Smart/Adaptive params/code untouched (only the guard added). 5. Build. 6. Open questions.
