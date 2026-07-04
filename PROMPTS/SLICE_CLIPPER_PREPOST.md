# SLICE — Clipper Pre/Post position select

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (rig) · **Audition/decide:** avishali → Asaf → testers
**Repos:** plugin `MasterLimiter` only. One new **frozen** param + relocating the existing clipper stage. No SDK change.
**Companion:** `docs/SIGNAL_FLOW.md` (clipper stage, §2.x).

> ⚠️ **Retrieval log first.** Re-confirm the clipper stage block, the wideband-output/ceiling write, the `lookaheadPad_` + downsample, and the FinalCeiling call in the current tree (lines drift a lot in `processCore`). Output the log with actual current line numbers before editing.

---

## Why

The clipper is a legitimate character + loudness tool, and **where** it sits matters:
- **Pre (current):** clips the input *before* the limiter — adds density/harmonics that the limiter then levels. Aggressive input shaping.
- **Post:** clips *after* the limiter — catches the transient peaks the limiter (esp. Real attack) passes, adds loudness/density on the already-leveled signal. The FinalCeiling (now **TruePeak** by default) cleans up the clipper's inter-sample residual. This is the classic "clipper after limiter" mastering move — and it's directly relevant to our transient findings (a post-clipper is a fast transient catcher).

Give the user the choice.

---

## Current clipper stage (map)

Self-contained block on the 4× `osBlock` ([PluginProcessor.cpp:1234-1310](Source/PluginProcessor.cpp:1234)):
1. `clipBlock = clipperOversampler_.processSamplesUp (osBlock)` — clipper's own OS on top of the 4× block.
2. If `clipperActive`: per-sample drive → hard/soft clip → un-drive, + clip-GR metering ([:1242-1298](Source/PluginProcessor.cpp:1242)).
3. `clipperOversampler_.processSamplesDown (osBlock)` ([:1304](Source/PluginProcessor.cpp:1304)) + `clipperOsAlignDelay_` alignment ([:1306-1310](Source/PluginProcessor.cpp:1306)).

**It runs unconditionally** (the OS round-trip happens even when the clipper is inactive) so toggling `clipper_active` doesn't change latency. Preserve that property.

---

## Param (1 new frozen ID)
`ParameterIDs.h` + `Parameters.cpp` — mirror `clipper_mode` (frozen Choice):
- **`clipper_position`** — `AudioParameterChoice { "Pre", "Post" }`, default index **0 (Pre)** = **current behavior exactly (null)**.

---

## DSP — make the clipper stage relocatable
1. **Refactor** the clipper block ([:1234-1310](Source/PluginProcessor.cpp:1234), the up → clip → down → align) into a single reusable lambda/helper `runClipperStage (osBlock)` that does the full OS round-trip + clip (if active) + align + metering. No behavior change when called at the current site.
2. **Read `clipper_position` once per block.** Call `runClipperStage(osBlock)`:
   - **Pre:** at the current location (before the crossover/limiter) — bit-identical to today.
   - **Post:** at the **post-limiter point** — after the wideband stage has written the limited+ceilinged output to `osBlock` (after the `× wideGain × ceilingLin` / M/S-decode writes, ~[:1821](Source/PluginProcessor.cpp:1821) stereo & the M/S branch) and **before** `lookaheadPad_` + `limiterOversampler_.processSamplesDown` ([~:1940-1954](Source/PluginProcessor.cpp:1940)). The downstream FinalCeiling (TruePeak) then catches the clipper's ISP.
3. **Run the stage exactly once per block** (at Pre *or* Post, never both, never zero) so total reported **latency stays constant** regardless of position — the clipper OS latency + `clipperOsAlignDelay_` are present either way. **Verify latency is identical for Pre and Post** (this is the critical guard).

> RT-safe: same work, relocated. The `clipperOversampler_`/align state is single-instance; a live Pre↔Post switch may click (state discontinuity) — acceptable for a character control; note it (smooth/duck later if needed).

---

## Metering / docs
- Clip-GR metering (`currentClipDb_`, `clipEnvBuf_`) moves with the stage — unchanged values, just computed at the active site.
- `docs/SIGNAL_FLOW.md`: document the Pre/Post clipper position and that Post relies on TruePeak FinalCeiling for ISP safety.

---

## Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
```
**Acceptance (Claude verifies 1–4 + measures 5 on the rig; avishali auditions 6):**
1. Build clean, AU validates.
2. **Pre (default) is bit-identical to HEAD** at every clipper setting (active + inactive) — the refactor must not change the pre path. State how confirmed (offline null).
3. **Latency identical for Pre vs Post** (report both `getLatencySamples()` — must match). Critical guard.
4. Clip metering works in both positions.
5. **(Claude runs the rig)** Post clipper on the real mix / bass+transient: confirm it catches transient peaks (crest ↓ vs no clip) and that **true-peak ≤ ceiling with FinalCeiling on (TruePeak)** — i.e. FinalCeiling catches the post-clip ISP. Report SP/TP/crest, Pre vs Post.
6. **Audition (avishali first):** Pre vs Post character on program material.

**Close gate:** update `docs/SIGNAL_FLOW.md` + `docs/PROGRESS.md` + `PROMPTS/PLAN.md`; commit **plugin-only**; **push after C is done** (avishali: push SDK + this once C closes). Archive `SLICE_CLIPPER_PREPOST_CLOSE.md`.

## Output requirements
1. Retrieval log. 2. Param diff (`clipper_position`). 3. DSP diff (the `runClipperStage` refactor + Pre/Post call sites). 4. Build + auval. 5. Pre-path null evidence. 6. Latency Pre vs Post (must match). 7. `git status --short` + commit hash. 8. Open questions (live-switch click).

## Notes for the architect (not for Cursor)
- Strategically this is more than a character toggle: a **Post clipper is a fast transient catcher** on the limited signal — a cheap, real tool for the transient problem Real attack leaves (the thing the two-stage rebuild targets). Worth measuring how much crest a modest Post clip removes on the mix; it may be a usable interim before the full Stage-1.
- Frozen ID (`clipper_position`) — permanent once shipped; name it right now.
- The refactor-into-a-lambda is the whole risk surface for the Pre null: keep the pre call site producing identical samples.
