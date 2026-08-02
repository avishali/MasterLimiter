# Program-Dependent Engine ("Smart") — unified design

**Status:** approved as the next development direction (avishali, 2026-08-02). **Architect:** Claude.
**Supersedes as the umbrella for:** `INTELLIGENT_RELEASE_DESIGN.md` (adaptive release) and
`ADAPTIVE_THRESHOLD_ENGINE.md` (adaptive per-band threshold). Those remain the detailed designs for
axes 1 and 3; this document is the frame that ties them together and adds axis 2.

---

## 1. Why — the measured case

We spent 2026-08-02 trying to find better *constants* for the Open engine. The answer came back that
there aren't any:

- **Release has no global optimum.** Across 4 sources the best value flips direction: the live show wants
  ~30 ms, a dense pop mix wants ~300 ms, and one source inverts between operating points. Mean
  differences (0.1–0.5 dB) are swamped by per-source spread (2–4 dB). `tools/analysis/mbl_voicing.py`.
- **avishali's listening agrees at real operating points**: release should default **fast (≤30 ms)** and
  attack **fast**. The measurement that appeared to contradict this was matched at 8 dB of *RMS* gain
  reduction, which needed +20–24 dB of drive — not a real master. At a realistic push, fast release wins
  on most sources.
- **The frontier gap is not a tuning problem.** Open sits −0.24 dB (jazz) / −0.43 dB (edm) behind the best
  Pro-L 2 style with genuine peak control. Voicing does not close that; a different mechanism might.

> The conclusion is not "our release is mistuned". It is **a fixed release cannot be right**, because the
> optimum inverts between programs. Same argument now extends to reduction depth (§2, axis 3).

## 2. The three adaptive axes

| # | Axis | What adapts | Detailed design |
|---|---|---|---|
| 1 | **Release** | recovery rate follows program density / transient rate | `INTELLIGENT_RELEASE_DESIGN.md` (`ReleaseEngine::AdaptiveLookahead`) |
| 2 | **Attack** | fast by default; slows only where a fast attack would audibly distort (LF) | new — see §4 |
| 3 | **Reduction depth** | *how much* GR to apply, per program and per band | `ADAPTIVE_THRESHOLD_ENGINE.md` (dynamic threshold) |

**Axis 3 is avishali's 2026-08-02 addition and it is the most novel.** Every limiter on the market adapts
release; adapting *depth* — deciding that this passage should be limited less hard than that one, at equal
output loudness — is the part with no obvious incumbent. It is also what `ADAPTIVE_THRESHOLD_ENGINE.md`
§4 already describes as the "dynamic-threshold law", so the two ideas converge.

## 3. Design constraint that falls out of today's measurements

**Whatever adapts must be judged on |MACRO| at a fixed peak ceiling, not on `st_range`.**
`st_range` cannot separate breathing from pumping and can be gamed two ways — by letting peaks through
(the old 6.39 edm figure) or by inventing slow swings (a long release scores *positive* added modulation).
The objective for every prototype in this programme is:

```
minimise |added envelope modulation| in 0.1-0.5 Hz  (MACRO)
  subject to   sample peak <= ceiling            (hard gate, no exceptions)
               PUMP band (2-8 Hz) not made worse than the fixed-release baseline
  measured at  a realistic push, matched by ACTUAL gain reduction, across the profiled corpus
```

`tools/analysis/mbl_pump.py` and `mbl_voicing.py` already implement this. **A prototype that improves
MACRO while missing the ceiling has not improved anything** — that mistake cost us three weeks of
believing Open was at parity.

## 4. Axis 2 — attack (the small one, do it first)

avishali: attack should be fast by default. The known counter-pressure is that a fast attack on
low-frequency content produces waveform distortion (measured, `SPECTRAL_ENGINE_DESIGN.md`: "the wideband
attack distortion law"). The 3-band work already showed per-band attack scaling is a weak lever on its own.

Proposal: **fast global attack, with the LF band alone permitted to slow down when its own crest demands
it.** This is a much smaller adaptive rule than axes 1 and 3 and is a good first slice — it exercises the
adaptation plumbing on a low-risk axis before the harder ones.

## 5. Sequencing

| Slice | Content | Risk |
|---|---|---|
| **SMART-0** | Defaults: release 150 → 30 ms, attack → fast (value from the stage-2 sweep). No new DSP. | trivial |
| **SMART-1** | Adaptive **release** in the Open engine's `MultibandLimiter` path (axis 1). DEV-toggled, A/B against fixed. | medium |
| **SMART-2** | Adaptive **attack** (axis 2), LF-aware. | low |
| **SMART-3** | Adaptive **reduction depth** (axis 3) — the novel one. Prototype in Python against the rig FIRST (`ADAPTIVE_THRESHOLD_ENGINE.md` §8 validation-first), C++ only once it measures. | high |

**Do SMART-0 now** (it is a default change avishali's ears already back). **Do not start SMART-3 in C++**
— §8 of the adaptive-threshold doc exists because the previous spectral prototype (P-A) came back NEGATIVE
after C++ work had begun. Prototype in Python, measure, then commit to DSP.

## 6. What this does NOT change

- Peak safety is settled and must stay settled: Ceiling holds −1.00 at all rates, latency is constant and
  exact, 51/52 calibration checks pass. **`mbl_calibrate.py` runs as the gate on every SMART slice.**
- Transparent and Open remain as they are. Smart is a third engine, DEV-selectable, not a replacement.
- SDK primitives keep their technical names (`ENGINE_NAMING.md`); "Smart" is the product name for the
  composition.

## 7. Open questions for avishali

1. **Does Smart replace Open, or sit beside it?** (Affects whether the alpha selector becomes 3-way.)
2. **How much adaptation is too much?** A limiter whose behaviour changes under the user is harder to
   trust on a master. Should adaptation depth itself be a user control (an "Adapt" amount)?
3. **Axis 3 target:** adapt depth to preserve macro-dynamics, or to hold a *perceived* loudness? Those
   pull in different directions and the choice defines the engine.
