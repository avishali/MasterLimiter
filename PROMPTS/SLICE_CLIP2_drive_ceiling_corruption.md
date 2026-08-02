# SLICE CLIP-2 — Drive + Ceiling=Clip corrupts audio in the Open engine (SHIP BLOCKER)

**Status:** ready for Cursor · **PRIORITY: highest — ahead of SMART-1.2 and SMART-2**
**Architect:** Claude · **Verify:** Claude (`mbl_calibrate.py --group Z`) · **Reported by:** avishali (heard it)
**Scope:** plugin. SDK only if the retrieval log proves the fault is inside `mdsp_dsp` — additive, and say so.

## Report
> avishali: *"when ceiling and clipper both 'on', the audio gets corrupt"*

Reproduced and characterised. **Real, audible, in a completely ordinary user configuration** — and
`Ceiling` now defaults to `Clip`, so it is one Drive click away from every user.

## The trigger is narrow and exact

Open engine + **Drive active** + **Ceiling release = `Clip`**. Nothing else reproduces it:

| config | worst sample-step | verdict |
|---|---:|---|
| **Open / Ceiling=Clip / Drive ON** | **0.8185** | **19.4x the Drive-off case** |
| Open / Ceiling=Clip / Drive off | 0.0421 | clean |
| Open / Ceiling=20 ms / Drive ON | 0.0547 | clean (1.3x) |
| Transparent / Ceiling=Clip / Drive ON | 0.0820 | clean (1.1x) |
| Transparent / Ceiling=20 ms / Drive ON | 0.0488 | clean (1.0x) |

On real program material (live-show, +14 dB, ceiling −1) the worst step reaches **1.069** — larger than
full scale for a signal whose ceiling amplitude is 0.891. That is a click, not audio. **67 events in 20 s.**

**It is block-size dependent**, which is the decisive clue — this is buffer/state management, not DSP math:

| host buffer | 64 | 128 | 512 | 2048 |
|---|---:|---:|---:|---:|
| events > 0.6 | 870 | **13783** | 4181 | 103 |
| same, Drive OFF | 0 | 0 | 0 | 0 |

Also measured: **independent of `drive_db` (0 / −6 / −12) and of Drive Hard vs Soft** — only whether Drive
is *active* matters. Independent of `ceiling_mode` (SP and TP both). Present with release engine `Manual`
(worse: 116 events, step 1.156) and `Smart`. **Reported latency stays a correct, constant 3003** in every
case, so this is not PDC — it is internal misalignment or a shared/stale buffer.

## Prime suspect

CLIP-1.1's own close note says the Open tip-catch previously broke because *"shared Drive OS corrupted
Ceiling"*, and the fix was a **separate `ceilingClipOversampler_`**. The bug now appears exactly when
**both** oversampler round-trips run in the Open path. Look first at:

- a scratch/working buffer shared between `limiterOversampler_` (Drive) and `ceilingClipOversampler_`;
- `processSamplesUp`/`processSamplesDown` block sizes or offsets assumed from the *other* stage;
- whether either oversampler is prepared for the true maximum block size, and reset per block;
- the wet-path padding when BOTH `clipperActive_` and `ceilingClip` add `osStageLatency`
  (`syncReportedLatency` adds both — confirm the pad matches what is actually consumed).

> ⚠️ **Retrieval log first.** Report: where each oversampler is prepared/reset, every buffer either one
> writes to, and whether any storage is shared. State the actual cause before changing code.

## Gate
- [ ] `mbl_calibrate.py --group Z` — **"Drive adds no discontinuity" PASSES for all four combinations.**
      (The check is relative: Drive ON must not raise the worst sample-step more than 3x vs Drive OFF.
      It currently reports 19.4x for Open/Clip and is the regression test for this bug.)
- [ ] Full `mbl_calibrate.py` — A–N and Z all PASS. **sPk ≤ −1.00 and latency 3003 unchanged.**
- [ ] Verified at host buffer sizes **64 / 128 / 512 / 2048** — the fault is block-size dependent, so one
      buffer size proving clean is NOT sufficient evidence.
- [ ] Drive remains audible (not "fixed" by making it a no-op again — that regression already happened once).
- [ ] Open engine measurement unchanged: `mbl_frontier2.py`-style score must not move materially.
- [ ] Build clean, AU + VST3, **both installed**, mtimes for both.

## Non-goals
- Do not "fix" it by disabling Drive when Ceiling=Clip, or by forcing Ceiling out of Clip. Both are normal
  settings and both must work together.
- No engine/voicing change. No new parameters.

## Output requirements
1. Retrieval log with the ACTUAL root cause named. 2. Diffs. 3. Group Z output. 4. Full calibrate.
5. Block-size table (64/128/512/2048) before and after. 6. Build + install mtimes for BOTH formats.
