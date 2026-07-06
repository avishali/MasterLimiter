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

## Behaviour preservation (the gate)
- **Open engine must sound the same:** at Ceiling=clip (min release, −1, Hard, oversampled), the Open engine output must match today's Open (2-band + forced clipper). **Claude verifies:** render jazz/EDM through Open, matched loudness → **300 ms range ≈ 4.9 / 6.4 and sample-peak ≈ −1**, matching the pre-CLIP-1 build (null/close). If it diverges, the Ceiling clip path isn't identical to the old clipper — fix.
- **Transparent engine unaffected** except Ceiling replaces FinalCeiling (same limiter DSP at release > 0).

## Non-goals
- No SDK edits (compose existing FinalCeilingLimiter + clipper). No engine/breathing DSP change. Don't touch Smart/Adaptive.

## Build/verify/audition
- Build clean, AU+VST3, **install both formats** (verify mtime — this keeps getting missed).
- (Claude) Open-engine range/peak matches pre-CLIP-1; Ceiling On/Off + Release(limiter↔clip) behaves; no forced clipper; Drive is pre-only tone; LED (now Drive) follows the toggle.
- (avishali) one clear peak control (Ceiling), one tone control (Drive); can't stack two peak stages; the Ceiling release knob morphs limiter→clip.

## Output requirements
1. Retrieval log. 2. Diffs (Ceiling group, Drive relabel/pre-only, Open-branch de-force, processor composition). 3. Build+install mtimes. 4. Open-engine range/peak vs pre-CLIP-1. 5. Confirm no SDK edits, Smart untouched. 6. Open questions.

## Notes for the architect (decide with avishali before build)
- **Clipper → pre-Drive vs remove:** default = keep as pre-Drive (no lost capability). Confirm.
- **Ceiling location:** main window (recommended, it's a core control now) vs DEV.
- **Release→clip mapping:** a single Release knob where MIN = clip is simplest; alternatively a small "Limiter | Clipper" mode + a release knob. Recommend the single knob with a clear MIN="Clip" detent/label.
- May split: **CLIP-1a** = Ceiling composition + Open de-force (DSP/peak model) ; **CLIP-1b** = Drive relabel + UI move + docs. Split if the diff can't be reviewed in ~10 min.
