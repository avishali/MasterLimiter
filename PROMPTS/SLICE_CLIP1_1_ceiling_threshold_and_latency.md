# SLICE CLIP-1.1 — Ceiling@Clip must clip at `ceiling_db`; Open must delay what it reports

**Status:** ready for Cursor · **Follows CLIP-1** (do not close CLIP-1 until this is green) · **Architect:** Claude
**Verify:** `tools/analysis/mbl_calibrate.py` (Claude runs it; it is the close gate) · **Audition:** avishali
**Scope:** plugin only. **No SDK change.** No engine/breathing DSP change.

## Credit where due — CLIP-1 worked for Open
Measured on the Aug 2 02:25 build: **Open holds −0.99 dB sample-peak AND −0.99 dBTP** at +6/+12/+18 dB
input, range **4.90 jazz / 6.39 edm**. The tip-catch via Ceiling@Clip is real, the forced clipper is gone,
Drive is an audible toggle again, and latency is constant at 2995 across all 8 engine/Drive/Ceiling combos.
That is the hard part done.

The two defects below are what the other half of the gate was for.

---

## BUG 1 (priority) — Ceiling@Clip clips at 0 dBFS, not at `ceiling_db`

`ceiling_release_ms = 0` ("Clip") is now the **default**, so this is the default behaviour.

Transparent engine, +18 dB input, hostile burst train:

| `ceiling_db` | Ceiling = **Clip** | Ceiling = 20 ms limiter |
|---:|---:|---:|
| −1 | **−0.57** | −1.00 ✓ |
| −3 | **−0.52** | −3.00 ✓ |
| −6 | **−0.47** | −6.00 ✓ |
| −12 | **−0.34** | −12.00 ✓ |

The limiter path is exact. The Clip path is not clipping at the ceiling at all — **proof:** at `ceiling_db`
= −12 the output measures −0.34 dB, which is *precisely* the unclipped level of that material after the
ceiling output gain. Nothing was clipped, because the material never reached the clip threshold — the
threshold is sitting at **0 dBFS (1.0)**, not at `ceilingLin`.

Open masks this because the MB path tip-catches separately; **Transparent is fully exposed** and fails
peak safety at 44.1 / 48 / 88.2 / 96 kHz (−0.65 / −0.57 / −0.27 / −0.66 against a −1.0 ceiling), and in
TruePeak mode reaches **−0.06 dBTP**.

**Fix:** the Ceiling clip threshold must be `ceilingLin` (i.e. `ceiling_db`), not 1.0 — matching what the
FinalCeiling limiter path already does correctly. Check the ordering too: clipping must happen at the
ceiling, whether it is applied before or after the ceiling output gain.

**Clears 11 of the 18 current failures** (D x3, F x4, J-ceiling x4).

## BUG 2 — Open under-delays by 720 samples (15.0 ms) against its own reported latency

All four Open configs report 2995 samples but actually delay 2275. Transparent is exact (±0) in all four.
Constant-latency reporting is correct now; Open's **padding** is 720 samples short of the value reported.
In a DAW, Open sits **15 ms early** against every other track — which would also silently corrupt any
A/B between the engines.

**Fix:** pad the Open wet path to the same `kFixedLatencySamples` the plugin reports. Gate: residual error
0 for every engine/Drive/Ceiling combination.

## BUG 3 — 44.1 kHz under-delays by 118 samples (2.7 ms)
Transparent at 44.1 kHz reports 2995, delays 2877. 48 and 96 kHz are exact. Likely a rounding path that
assumes a rate — the fixed-latency max must be computed per prepared sample rate.

## BUG 4 — output level drifts 0.82 dB with host buffer size
RMS/peak drift across buffer 64 / 128 / 512 / 2048 (this is a level comparison, not sample-aligned
subtraction, so it is not an alignment artefact). Something is block-size dependent — smoothing anchored
to block boundaries, or a per-block reset. Lower priority than 1–3 but it means two DAWs at different
buffer settings do not produce the same master.

## BUG 5 — THD −58 dBc at −40 dBFS 1 kHz. **Isolated: it is the Ceiling stage.**

Same 1 kHz tone at −40 dBFS, no limiting, no Drive, only the surrounding config changed:

| config | THD |
|---|---:|
| `limiter_active` = false (dry path) | −165.7 dBc |
| limiter ON, gain 0, Drive off | **−58.0 dBc** |
| limiter ON, gain 0, **Ceiling OFF** | **−147.7 dBc** |
| Open engine, gain 0 | −159.2 dBc |

Turning Ceiling off removes it completely, and the Open path is clean — so this is the **Ceiling stage in
the Transparent path**, not the always-on clipper OS round-trip I originally guessed. A −40 dBFS tone
should never reach a −1 dB ceiling, so the stage is behaving nonlinearly when it should be dormant —
almost certainly the **same wrong threshold/scaling as BUG 1**.

Re-measure after fixing BUG 1; expect it to disappear. Group M's "−56.2 dBc alias product" is likely the
same artefact rather than true aliasing — re-check that too before treating it as an oversampling problem.

## NOT part of this slice — Stereo Link is a no-op (pre-existing, not caused by CLIP-1)
Recorded here so it is not lost; give it **its own slice after CLIP-1.1**.
L = 220 Hz at −4 dBFS, R = 3 kHz probe at −37 dBFS, input +15 dB, ceiling −1:
the limiter does **14.06 dB of GR on L**, and R moves **−0.06 dB at Stereo Link 100%, 50% AND 0%** —
identical in `Stereo` mode (`stereo_link`) and `M/S` mode (`m_s_link`). At 100% the design says both
channels take `min(gainL,gainR)`, so R should duck ~14 dB. The channels are never linked.
Second anomaly in the same probe: the −1 dB ceiling output gain does not appear on the quiet channel
either (R lands −0.06 dB from its input, not −1.0). Likely the same area.

---

## Close gate
`./.venv/bin/python tools/analysis/mbl_calibrate.py` — **groups A–J and Z all PASS** (currently 24/42).
Then re-run `mbl_frontier.py` to confirm Open's 4.90 / 6.39 survives the threshold fix.

**Do not report range without sample-peak.** The two numbers only mean something together: the whole
reason this slice exists is that a correct range was reported next to a ceiling that was not being held.

## Output requirements
1. Retrieval log. 2. Diffs. 3. The clip threshold before/after, and where it sits relative to the ceiling
gain. 4. Build + install mtimes for BOTH formats. 5. Full `mbl_calibrate.py` output pasted. 6. Confirm no
SDK edits.
