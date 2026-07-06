# SLICE MB-1.1 — fix the plugin↔bench discrepancy + crossover range/default

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (re-run the plugin-vs-bench matrix) · **Audition:** avishali (after it matches)
**Repo/scope:** plugin `MasterLimiter` (the MB-1 path) + possibly SDK `MultibandLimiter` if a real module bug is found (flag first). Additive/behaviour-preserving to the toggle-OFF path.
**Why:** MB-1 verify (Claude, `tools/analysis/mbl_plugin_verify.py`) shows the plugin's MB-engine path does **not** reproduce the bench `MultibandLimiter` (`tools/analysis/mbl_xover.py`), and is broken at higher crossover. Must match the proven DSP before avishali auditions.

## The repro (Claude measured, installed VST3, MB engine ON, Ramp, ceiling −1 SP, matched to Ozone RMS)
| config | PLUGIN range | BENCH range (mbl_xover / lr) | note |
|---|---|---|---|
| xover 120, safety OFF | 5.07 (jazz) / 6.76 (edm) | ~5.75 / 6.40 | close-ish (still off ~0.7) |
| xover 3000, safety OFF | **2.19 / 3.40** | **4.93 / 5.65** | ❌ way off — plugin flat where bench breathes |
| xover 120, safety ON | 2.41 / 3.16 | — | safety re-flattens low-xover overshoot (bad default) |
Plugin also needs ~5 dB MORE drive at 3000 than the bench to hit the same RMS → it's over-limiting at higher crossover.

## Investigate (bench = the reference; find why the plugin's same-module use differs)
Prime suspects — verify each against `tools/mbl_bench.cpp`'s `--split lr` path (which IS `MultibandLimiter` and matches the bench numbers):
1. **numBands / crossover setup.** Confirm the plugin actually prepares/uses **2 bands**. `setCrossoverFrequencies(&hz, 1)` sets 1 crossover — is `numBands` in the prepare `Spec` = 2, and does the splitter honour it? Compare to how `mbl_bench` prepares `MultibandLimiter` (it works). Suspect the plugin's Spec or a stale-since-prepare crossover.
2. **Divergence grows with crossover freq** (120 close, 3000 broken) → something crossover-frequency-dependent. Check the crossover value is passed in Hz (not normalized), and that the LR splitter isn't clamping/misplacing the single crossover differently than the bench.
3. **Gain staging / extra stage.** Is anything besides `input_gain_db → MultibandLimiter → [safety] → output` in the path (a residual limiter/FC/OS/dry-wet/level stage not fully bypassed when `dev_mb_engine` ON)? The +5 dB extra drive needed at 3000 hints at extra level loss.
4. **Block size.** `mbl_bench` vs the host block size — if `MultibandLimiter` behaves differently across block sizes, that's an SDK bug (S-B tests block-processed but maybe not all sizes). Test the bench at the host's block size to isolate.

**Method:** render the SAME file through `mbl_bench --split lr --bands 2 --crossovers 3000 --attack-mode ramp --release-ms 150` AND the plugin MB path at xover 3000 safety-off, matched drive; if they differ, bisect the config difference (bands, crossover, gain, block size) until they match. Add a temporary debug print of the plugin's actual configured crossover/numBands if needed (remove before close).

## Also fix (once matching)
- **Crossover range:** raise `dev_mb_crossover_hz` max from 3000 → **18000** (the proven peak-controlled ≈Ozone config uses a ~16 kHz split — "limit the body as one band, the air separately"; the plugin currently can't reach it).
- **Defaults for a good first audition:** set `dev_mb_crossover_hz` default to the peak-controlled config once confirmed (likely ~14–16 kHz with safety ON = breathing + TP-safe, matching bench ~4.6/5.0), OR — if avishali prefers the low-xover character — document that safety must be OFF there. Architect will confirm the default after the plugin matches the bench and Claude re-sweeps crossover on the plugin.

## Verify (Claude) / Acceptance
1. Plugin MB path **matches the bench** at (xover 120 / 700 / 3000, safety off) to within ~0.3 range and ~1 dB drive. (Claude re-runs `mbl_plugin_verify.py` vs `mbl_xover.py`.)
2. Crossover reaches 18 kHz; at ~16 kHz + safety ON the plugin reproduces the bench ~4.6/5.0 peak-controlled result.
3. Toggle OFF still byte-identical; latency correct; no new warnings.

## Output requirements
1. Retrieval log + the root-cause found (what differed). 2. Diff. 3. The plugin-vs-bench match numbers at 120/700/3000. 4. Confirm toggle-OFF unchanged. 5. If the bug was in the SDK `MultibandLimiter`, flag it explicitly (it would affect the bench too — but the bench matches, so more likely plugin-side).

## Notes for the architect (not for Cursor)
- Since the BENCH (`--split lr` = MultibandLimiter) matches the reference and the PLUGIN (same module) doesn't, the bug is almost certainly in the **plugin's configuration/gain-staging of the module**, not the module itself. Start there.
- This also resolves the honesty gap: confirm on the plugin which (crossover, safety) is actually peak-controlled-AND-breathing, so the "2-band parity" claim rests on a plugin-reachable, audition-verified config — not the bench's default-16k artifact.
