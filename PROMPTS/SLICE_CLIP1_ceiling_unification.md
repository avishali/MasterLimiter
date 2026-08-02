# SLICE CLIP-1 — unify peak-safety into one "Ceiling" stage (Limiter→Clipper); clipper→pre "Drive"

**Status:** ready for Cursor (post-alpha; spec now) · **Architect:** Claude · **Verify:** Claude (Open-engine range unchanged + null) · **Audition:** avishali
**Repo/scope:** plugin `MasterLimiter` (control model + processor composition). **No SDK change** (composes existing `FinalCeilingLimiter` + the oversampled clipper). **Read `docs/ENGINE_NAMING.md`** (add Ceiling/Drive there).
**Why:** today there are TWO peak-affecting stages (user Clipper pre/post + FinalCeiling) that can both run = "both by mistake" + the Open engine *force-enables* the clipper (the LED hack). avishali's model: ONE peak stage spanning **Limiter → Clipper** (physical hard-clip at the min end), and the clipper's *tone* role split off as a pre-engine Drive. This makes the peak model unambiguous and removes the forced-clipper entirely (LED becomes clean by construction).

> ⚠️ **Retrieval log first.** Read: `runClipperStage` (the OS hard/soft clip + `forceActive`), the `FinalCeilingLimiter` usage + its params (`dev_final_ceiling`, `dev_final_ceiling_release_ms`), the Open-engine (MB) processing branch (`PluginProcessor.cpp` ~1530–1540, where it force-clips), and where clipper params live. Output the peak-stage call sites.

## The model
```
[ optional pre "Drive" (tone) ] → Engine (Transparent/Open) → [ CEILING (peak safety) ] → output
```
- **CEILING** = the single peak-safety stage. One **On/Off** + one **Release** knob spanning:
    - **Limiter end** (release ~100 ms → gentle): behaves as `FinalCeilingLimiter` at that release.
    - **Clipper end** (release at MIN): **physically switches to a hard clip** — the existing **oversampled hard-clip** (NOT a fast-release limiter; the breathing win needs a true instantaneous clip). Ceiling level = `ceiling_db` (global).
    - Implementation: `if (ceilingOn) { releaseMs > 0 ? FinalCeilingLimiter(releaseMs) : oversampledClip(ceiling) }` post-engine. Keep the clip oversampled (reuse the current clipper's OS path) so the clip end == today's clipper exactly.
    - Optional: a Hard/Soft choice for the clip end (carry `clipper_mode`).
- **The Open engine uses CEILING as its tip-catcher** (default Release = MIN = clip), **instead of force-enabling the clipper.** So `runClipperStage(..., forceActive=true)` at the MB branch is REMOVED — the Open engine simply runs with Ceiling in clip mode. This preserves the proven Open DSP (2-band → oversampled hard-clip at −1) byte-for-similar, and there's no forced user-clipper → the Clipper/Drive LED follows the user by construction.
- **The old Clipper control → "Drive" (pre-engine tone/saturation only):** on/off, drive, Hard/Soft, **PRE only** (remove the pre/post choice — post peak-catching is now the Ceiling's job). Clearly a *character* tool, not peak safety. (Alternatively remove the clipper entirely for v1 and add Drive later — architect will decide with avishali; default: keep as pre-Drive so no coloring capability is lost.)

## Control changes
- **New CEILING group** (absorbs FinalCeiling): On/Off (`dev_final_ceiling`→`ceiling_active`), Release (extend `dev_final_ceiling_release_ms` range down to a MIN that means "clip"; e.g. 0 = clip), optional clip Hard/Soft. Decide with avishali: Ceiling lives on the **main window** (it's a core user control now) or stays DEV — recommend **main window** (it's the peak character).
- **Rename Clipper → Drive**, PRE-only, tone. Keep `clipper_*` param IDs (avoid preset breakage) but relabel in UI + docs.
- **Remove the forced clipper** on the Open engine branch.
- Update `docs/ENGINE_NAMING.md` (+ SIGNAL_FLOW / MANUAL): Ceiling = peak stage (Limiter→Clipper); Drive = pre tone.

## ⚠️ GATE REVISED 2026-08-02 — read this before building

**The original gate below was unsatisfiable.** It asked you to match "range ≈ 4.9 AND sample-peak ≈ −1,
matching the pre-CLIP-1 build". Measured on 2026-08-02, **the pre-CLIP-1 build does not do both at once**
(jazz `MIX 0003`, 2-band@120, Ramp, rel 150, ceiling −1 SamplePeak, drive +8.2 — identical results on the
installed VST3, `build/`, and `build-release/`, i.e. including the binary shipped as 0.3.2-beta):

| `dev_mb_safety` | sample peak | 300 ms range |
|---|---:|---:|
| OFF (the "clipper tip-catch" config) | **−0.15** (ceiling missed by 0.85 dB) | 4.88 |
| ON | −1.00 | **3.15** (below Ozone IRC1's 4.68) |

Also measured: **`clipper_active` is a bit-exact no-op in the Open path** (residual −240 dB between ON and
OFF) — consistent with the MB branch force-enabling the clipper, which is the LED hack this slice removes.

So the Open engine's headline claim ("Ozone-IRC1-parity breathing **with** peaks held to −1") is **not
reproducible on the plugin path**. The range is real; the peak control is not. Most likely the −1 came from
the C++ bench (`mbl_clip` hardclip) and the plugin path diverged — the same class of bench-vs-plugin
divergence already caught once in MB-1.1 (`42a4aa9`).

**This makes CLIP-1 load-bearing rather than cosmetic:** a Ceiling stage that actually clips at `ceiling_db`
is exactly the missing tip-catcher. Do NOT "preserve" the current behaviour — the current behaviour is the bug.

### The gate that replaces it
- [ ] **Open holds the ceiling: sample peak ≤ −1.00 dB** at drive +8.2 (jazz) and the matched EDM drive,
      Ceiling=clip, `dev_mb_safety` OFF. This is the fix.
- [ ] **AND range stays ≈ 4.9 (jazz) / ≈ 6.4 (edm)** at matched loudness — i.e. the breathing survives real
      peak control. **Report both numbers together; neither alone is a pass.**
- [ ] If the two cannot both be met, **stop and report** — do not tune the range down to buy the peak. That
      tradeoff is an architect/avishali decision, because it decides whether Open is actually shippable.
- [ ] `clipper_active` (now Drive) must become a real, audible user toggle — a non-null residual between
      ON and OFF, in both engines.
- **Transparent engine unaffected** except Ceiling replaces FinalCeiling (same limiter DSP at release > 0).

Verification is `tools/analysis/mbl_frontier.py` + the config in its `ours_configs()`; Claude runs it.

## Non-goals
- No SDK edits (compose existing FinalCeilingLimiter + clipper). No engine/breathing DSP change. Don't touch Smart/Adaptive.

## Build/verify/audition
- Build clean, AU+VST3, **install both formats** (verify mtime — this keeps getting missed).
- (Claude) Open-engine range/peak matches pre-CLIP-1; Ceiling On/Off + Release(limiter↔clip) behaves; no forced clipper; Drive is pre-only tone; LED (now Drive) follows the toggle.
- (avishali) one clear peak control (Ceiling), one tone control (Drive); can't stack two peak stages; the Ceiling release knob morphs limiter→clip.

## Output requirements
1. Retrieval log. 2. Diffs (Ceiling group, Drive relabel/pre-only, Open-branch de-force, processor composition). 3. Build+install mtimes. 4. Open-engine range/peak vs pre-CLIP-1. 5. Confirm no SDK edits, Smart untouched. 6. Open questions.

## DECIDED (avishali, 2026-08-02) — these are no longer open questions
1. **Keep the clipper as a pre-engine "Drive"** (do NOT remove it). No coloring capability is lost.
2. **Ceiling lives on the MAIN WINDOW**, not DEV. It is a core user control now.
3. **Single Release knob, MIN = clip**, labelled with a clear `Clip` detent at the minimum.
   No separate "Limiter | Clipper" mode selector.

May still split if the diff can't be reviewed in ~10 min:
**CLIP-1a** = Ceiling composition + Open de-force (DSP/peak model) ; **CLIP-1b** = Drive relabel + UI move + docs.

## ⚠️ ADDED MID-SLICE 2026-08-02 (avishali) — FIXED LATENCY, one value for everything

**Decision: the plugin reports ONE constant latency — the longest configuration — always.**
Rationale (avishali): a latency that moves is another source of confusion during testing; we would rather
pay the worst-case delay everywhere than debug PDC differences between engines and settings.

The current WIP goes the wrong way. `syncReportedLatency()` now branches on `mbEngineOn`,
`clipperActive_`, `ceilingClip` AND `ceilingLimiter` — so latency changes when the user toggles Drive or
moves Ceiling Release. That is four NEW moving latencies on top of the engine one.

**Required instead:**
- Compute `kFixedLatencySamples = max()` over **every** combination (Transparent/Open x Drive on/off x
  Ceiling clip/limiter/off) once in `prepareToPlay`.
- `setLatencySamples (kFixedLatencySamples)` **once per prepare**. Never call it from `processCore`.
- Every configuration shorter than the max pads its wet path to the fixed total (reuse the existing
  `lookaheadPad_` / align-delay pattern). `dryDelay_` tracks the same constant.
- **Measured gate:** reported latency identical, and impulse-measured latency identical, across all of
  those combinations, at 44.1/48/96 kHz. `tools/analysis/mbl_calibrate.py` check B covers this.

This supersedes AB-1a in `SLICE_AB1_trustworthy_engine_ab.md` (that slice can drop its latency section
once this lands).

## Sequencing note (2026-08-02)
This slice is deliberately **engine-agnostic** and lands BEFORE the Transparent-vs-Open verdict:
it is behaviour-preserving for both engines, so it does not pre-judge which one ships. Do not
fold any voicing or engine-selection change into it.
