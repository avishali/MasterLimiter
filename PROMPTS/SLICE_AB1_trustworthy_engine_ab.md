# SLICE AB-1 — make the tester engine A/B trustworthy (loudness match + blind labels)

**Status:** REVISED 2026-08-03 for the current build · **Architect:** Claude · **Verify:** Claude (measured) · **Audition:** avishali
**Prerequisites now met:** CLIP-1/1.1, LINK-1, SMART-0/1/1.1/1.2, CLIP-2, UI-4 have all landed.
**Repo/scope:** plugin `MasterLimiter` only. **No SDK change.** No engine/breathing DSP change.

## Why

The 0.3.2-beta asked testers to flip **Transparent <-> Open** and report which sounded better. That
comparison was confounded by two things unrelated to engine character. **One is now fixed:**

**1. ~~Latency jumped 44 ms on switch~~ — FIXED.** On 0.3.2-beta, Transparent reported 3229 samples
(67.3 ms) and Open 1104 (23.0 ms), so flipping mid-session re-synced or misaligned playback at the exact
moment of comparison. CLIP-1.1 fixed latency at the maximum across all configurations; both engines now
report **3003** on every corpus source. Nothing to build — but keep it as a regression check (gate below).

**2. The loudness offset is SMALLER now, but it VARIES BY SOURCE — which is worse.**

Re-measured 2026-08-03, Transparent vs **Open+Smart** (the current defaults), +14 dB input:

| source | Transparent | Open+Smart | offset |
|---|---:|---:|---:|
| live-show | -12.14 | -12.03 | +0.11 |
| ishay-ribo | -9.58 | -9.69 | -0.11 |
| **easy-master** | -14.54 | -13.57 | **+0.97** |
| homework-dense | -11.06 | -10.99 | +0.07 |

Mean +0.26 dB, **spread 1.08 dB, and the sign flips**. So a tester cannot compensate with one fixed trim,
and neither can we: the match has to be **live and program-dependent**. That is what AB-1b does. (The old
0.8-1.2 dB figure was Transparent vs Open+**Manual**; Smart is much closer in level, which is why the
*variance* rather than the offset is now the problem.)

Louder is reliably heard as fuller/punchier/better. The 0.3.2 release notes said *"Match levels by ear
if one is louder"* — that is not a control, it is asking the tester to do the hardest part of the
experiment unaided, and with a sign-flipping per-source offset it cannot be done by hand at all.
`gain_match_auto` (exposed as **"Auto / Track"**) does NOT solve it: it tracks a *learned dry reference*
(`learnedRefLufs_`), not engine-to-engine, and `PresetManager.cpp:118` sets it OFF on preset load.

**Consequence:** the engine verdict gates what ships in 1.0. A biased verdict is worse than no verdict,
because we would act on it. Fix the instrument before collecting more data.

### ⭐ AND THE QUESTION HAS CHANGED — this is the important revision
The frontier measurement (2026-08-02, `mbl_frontier2.py`) shows the two engines are **complementary, not
competing**:

| source | OPEN+Smart | TRANSPARENT |
|---|---:|---:|
| live-show (live recording) | 4.91 | **3.05** — best of our engines, beats Pro-L 2 Allround |
| ishay-ribo | **2.91** | 10.94 — worst of everything measured |
| easy-master | **5.78** | 6.41 |
| homework-dense | **2.22** | 5.39 |

Transparent wins on live-recorded material; Open+Smart wins on dense studio production. So the A/B must
**not** ask "which engine is better" — it asks **"which engine for which material"**. The blind A/B and the
loudness match are still exactly what is needed; only the framing and the analysis change.

> ⚠️ **Retrieval log first.** Read and report: the loudness estimator + `learnedRefLufs_` /
> `updateCompensationGainDb` / `applyCompensationGain`; the DEV Engine selector (UI-2) and its param
> listener; `setStateInformation` (the UI-2.1a async guard pattern); and the UI-4 control->group
> association, since the new controls must join it or they will leak across engine frames again.

## What to build

### ~~AB-1a — latency parity~~ ✅ ALREADY DONE — do not rebuild
CLIP-1.1 fixed reported latency at the maximum across every configuration. **Re-measured 2026-08-03 on the
current build: Transparent and Open+Smart both report 3003 samples on all four corpus sources.** Switching
engines mid-playback is already glitch-free. Nothing to do here.

### AB-1b — loudness-matched switching (the validity fix)
- New param `ab_match` (bool, **default ON**), UI label **"A/B Match"**, next to the engine selector.
- When ON: maintain a rolling short-term LUFS of the output. **On engine switch**, latch the pre-switch
  short-term LUFS as the target and drive the new engine's output trim toward it.
  - Reuse the existing estimator + smoother; clamp trim to **+/-6 dB**; same smoothing coefficient as
    `compGainDbSmoothed_` so it cannot zipper.
  - This is a *comparison aid*, applied post-Ceiling, and must NOT affect the peak ceiling: apply the
    trim BEFORE the final peak stage, or re-clamp after, so the ceiling still holds. **State which you chose.**
- When OFF: bit-identical to today's path (null test required).

### AB-1c — blind labels (the bias fix)
- With `ab_match` ON, the engine selector shows **"A" / "B"**, not the product names.
- The A<->B to Transparent<->Open mapping is **randomized per plugin instance**, stable for the session,
  and **stored in the preset** so a returned `.mlpreset` can be decoded on our side.
  (`dev_mb_engine` already records the true engine — do not obscure it in the state, only in the UI.)
- A small **"Reveal"** control un-blinds the labels; it must be a deliberate click, not the default view.
- ⚠️ Follow the **UI-2.1a async pattern**: any `setValueNotifyingHost` from the guard/mapping logic goes
  through `triggerAsyncUpdate`/`handleAsyncUpdate`, never from the audio thread.
- ⚠️ **ASCII-GATE**: labels are ASCII only (the build now fails on non-ASCII UI literals).

## Gate (Claude verifies by measurement — same rig as the Why table)
- [ ] **`ab_match` OFF is a null** vs pre-slice HEAD (residual <= -140 dB). This slice must not move either
      engine's measured voicing.
- [ ] **Loudness-matched switch:** on all four corpus sources at +14 dB input,
      |Open+Smart RMS - Transparent RMS| <= **0.15 dB** with `ab_match` ON
      (today: +0.11 / -0.11 / +0.97 / +0.07 — the +0.97 on `easy-master` is the one to kill).
- [ ] **Ceiling still held:** sample peak <= -1.00 dB on both engines with `ab_match` ON. The trim must not
      buy a level match by letting peaks through.
- [ ] **Latency regression check:** both engines still report **3003**, unchanged by `ab_match`.
- [ ] **`mbl_calibrate.py` 58/59** — A-N and Z all PASS except the known Open-vs-inline IMD.
- [ ] **Frontier score unmoved** with `ab_match` OFF: `mbl_frontier2.py` Open+Smart mean still **3.956**.
- [ ] Blind mapping stable across UI reopen + preset save/load; `.mlpreset` still records the true engine.
- [ ] New controls join the UI-4 control->group association (no leaking across engine frames).
- [ ] Build clean, AU + VST3, **both installed**, mtimes for both.

## Non-goals
- No SDK edits. No change to either engine's DSP. No new engine.
- Not the release control surface — `ab_match` and Reveal are beta-only instrumentation.

## Output requirements
1. Retrieval log. 2. Diffs. 3. Where the A/B trim is applied relative to the peak stage, and why the
ceiling still holds. 4. Build + install mtimes. 5. Confirm no SDK edits.
6. Open questions.

## Note for the architect
Do NOT ship another tester round on the old protocol. The blind + matched build is what produces a verdict
we can act on; the freeform "match by ear" round produces a number we would have to throw away.

`docs/AB_PROTOCOL.md` also needs its analysis section updated: the "split by source type" branch, written
as a fallback outcome, is now the **expected** one — the measurement predicts Transparent wins on
live-recorded material and Open+Smart on dense studio production. The protocol must ask testers to bring
**both kinds of source** or it cannot detect the split it is looking for.
