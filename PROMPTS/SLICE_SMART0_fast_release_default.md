# SLICE SMART-0 — Open engine default release 150 -> 30 ms

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (`mbl_calibrate.py` + `mbl_voicing.py`) · **Audition:** avishali
**Scope:** plugin only, **one default value**. No DSP change, no SDK change, no new parameter.
**Frame:** first slice of `docs/PROGRAM_DEPENDENT_ENGINE.md` (SMART-0).

## The change
`dev_mb_release_ms` default **150.0 -> 30.0** in `Source/parameters/Parameters.cpp`.

That is the whole slice. Do not change the range, the ID, or anything else.

## Why
- **avishali's listening**, across his own tests: the Open engine's release should default fast, 30 ms or
  less. He is the audition authority and this is a voicing call.
- **The measurement agrees at realistic operating points.** At ~11 dB push, 30 ms was best or near-best on
  2 of 3 valid sources (live-show |MACRO| 0.41 vs 1.38 at 150 ms; homework 0.20 vs 0.35). The one sweep
  that appeared to favour slower release was matched at 8 dB of *RMS* gain reduction, which needed
  +20-24 dB of input drive — not a real master. That operating point has since been corrected to 3 dB.
- **150 was never chosen.** It is a leftover constant from the MB-2 slice.

⚠️ **This does not mean 30 ms is globally correct.** Release has no global optimum — the best value flips
direction across sources (`tools/analysis/mbl_voicing.py`, 4-source sweep). 30 ms is the best available
*fixed* default; the real answer is the adaptive release in SMART-1.

## Preset compatibility
`dev_mb_release_ms` is a stored parameter, so **existing presets and sessions keep their saved value** and
are unaffected. Only fresh instances get 30 ms. Say so explicitly in the close note — testers have been
asked to send `.mlpreset` files and we must not imply their voicings changed under them.

## Gate
- [ ] Fresh instance reports `dev_mb_release_ms` = 30.0 (pedalboard load, no preset).
- [ ] A preset saved with 150 ms still loads as 150 ms.
- [ ] `mbl_calibrate.py` — **A-N and Z all PASS** (currently 51/52; the one FAIL is the known Open-vs-inline
      IMD difference, not a defect). Peak safety and latency must be untouched: this is a default change,
      it cannot move sPk or reported latency.
- [ ] Build clean, AU + VST3, **both installed**, mtimes reported for both.

## Non-goals
- Do NOT touch `dev_mb_attack_ms`. **It is inert in `Ramp` mode** (measured: bit-identical 0.5 -> 25 ms)
  because Ramp derives its attack from the lookahead pre-ramp. Ramp already is the fast attack. Changing
  attack means leaving Ramp, which changes the engine's character and is not this slice.
- No adaptive behaviour. That is SMART-1.
- No change to Transparent, to Ceiling/Drive, or to the SDK.

## Output requirements
1. Retrieval log. 2. One-line diff. 3. Fresh-instance default + preset-compat check.
4. Full `mbl_calibrate.py` output. 5. Build + install mtimes for BOTH formats. 6. Confirm no SDK edits.
