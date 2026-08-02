# SLICE AB-1 — make the tester engine A/B trustworthy (loudness match + blind labels)

**Status:** REVISED 2026-08-03 for the current build · **Architect:** Claude · **Verify:** Claude (measured) · **Audition:** avishali
**Prerequisites now met:** CLIP-1/1.1, LINK-1, SMART-0/1/1.1/1.2, CLIP-2, UI-4 have all landed.
**Repo/scope:** plugin `MasterLimiter` only. **No SDK change.** No engine/breathing DSP change.

## Why (measured 2026-08-02, installed 0.3.2-beta VST3, jazz `MIX 0003`, ceiling -1 SP)

The 0.3.2-beta asks testers to flip **Transparent <-> Open** and report which sounds better. As shipped,
that comparison is confounded by two things that have nothing to do with engine character:

**1. Latency is not equal across engines — it jumps 44 ms on switch.**

| engine | reported latency |
|---|---|
| Transparent | 3229 samples = **67.27 ms** |
| Open | 1104 samples = **23.00 ms** |

`syncReportedLatency()` (`Source/PluginProcessor.cpp:1425`) reports
`mbEngine_.getLatencySamples() + mbClipOsLatency` for Open vs `baseLatencySamples_` for Transparent.
Flipping mid-session either forces the host to renegotiate PDC (click / playback re-sync at exactly the
moment of comparison) or silently shifts the plugin 44 ms against the rest of the session. Testers will
hear that as "the engine".

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

Louder is reliably heard as fuller/punchier/better. The release notes currently say *"Match levels by ear
if one is louder"* — that is not a control, it is a request for the tester to do the hardest part of the
experiment unaided. `gain_match_auto` does NOT solve this: it tracks a *learned dry reference*
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

> ⚠️ **Retrieval log first.** Read and report: `syncReportedLatency` + `baseLatencySamples_` +
> `mbEngine_.getLatencySamples()`; the loudness estimator + `learnedRefLufs_` / `updateCompensationGainDb`
> / `applyCompensationGain`; `dryDelay_` and how it tracks reported latency; the DEV Engine selector
> (UI-2) and its param listener; `setStateInformation` (the UI-2.1a async guard pattern).

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
- [ ] **Latency identical across engines**, both formats, fresh instance per engine, and unchanged when
      toggling `dev_mb_engine` live. This is the headline number: `Transparent == Open`.
- [ ] **Loudness-matched switch:** at input +3/+6/+9/+12 dB, |Open RMS - Transparent RMS| <= **0.15 dB**
      with `ab_match` ON (vs the 0.81-1.21 dB above).
- [ ] **Ceiling still held:** sample peak <= -1.0 dB on both engines at every gain above, `ab_match` ON.
- [ ] **`ab_match` OFF is a null** vs pre-AB-1 HEAD (bit-identical or -140 dBFS residual), so the
      measured Open voicing in `SPECTRAL_ENGINE_DESIGN.md` is untouched.
- [ ] **Open's macro-breathing is preserved:** 300 ms range still ~4.9 jazz / ~6.4 edm at matched loudness.
      The padding and trim must not change what Open *does*, only how fairly it can be compared.
- [ ] Blind mapping stable across UI reopen + preset save/load; `.mlpreset` still records the true engine.
- [ ] Build clean, AU + VST3, **both installed** (verify mtimes — this keeps getting missed).

## Non-goals
- No SDK edits. No change to either engine's DSP. No new engine.
- Not the release control surface — `ab_match` and Reveal are beta-only instrumentation.

## Output requirements
1. Retrieval log. 2. Diffs. 3. Where the A/B trim is applied relative to the peak stage, and why the
ceiling still holds. 4. Build + install mtimes. 5. Confirm no SDK edits.
6. Open questions.

## Note for the architect
Do NOT ship another tester round on the old protocol. The blind + matched build is what produces a
verdict we can act on; the freeform "match by ear" round produces a number we would have to throw away.
