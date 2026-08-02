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

## 7. ANSWERED by avishali, 2026-08-02 — these are now design law

### 7.1 Smart sits BESIDE Open (not a replacement)
Three engines: Transparent · Open · Smart. Alpha selector becomes 3-way when Smart is auditionable.
Open stays exactly as measured today; Smart must be A/B-able against it at all times.

### 7.2 ⭐ Adaptation scales with how hard the user is pushing
> *"when user is pushing a little that means he is getting the results with little movements and
> reduction; when he pushes hard and reduction is bigger, that's more audible, and there we should
> adapt harder to smooth artifacts and keep the limiter transparent."*

**The adaptation amount is a function of the measured gain reduction depth**, not a fixed law:

```
adapt(t) = f( GR_depth(t) )      f(0 dB) = 0   ->  Smart degenerates EXACTLY to Open
                                 f(large) = 1  ->  full adaptation where artifacts live
```

Three things make this the right shape, and one of them is measured:

1. **It is measured.** At ~7 dB push every engine we tested — ours, Pro-L 2, Ozone — preserved
   macro-dynamics to within **0.04 dB** of each other. There is nothing to win at light push. At ~11 dB
   they spread over **2.3 dB**. Adaptation should therefore be ~0 where the engines are already
   indistinguishable and maximal where they diverge. The push-dependent law falls straight out of the data.
2. **It cannot hurt light use.** At low GR, Smart ≡ Open bit-for-bit. A user doing 2 dB of gentle
   limiting gets today's proven engine and none of the risk.
3. **It makes the A/B clean.** Any Smart-vs-Open difference a tester reports is attributable to the
   heavy passages, because the light ones are identical by construction.

`GR_depth(t)` must be a *smoothed* measure (the adaptation must not itself become a fast modulator —
that would be a new pumping source). Time constant is a voicing parameter; start ~1 s.

### 7.3 Axis 3 optimises for DYNAMICS, not loudness
> *"preserving dynamics, preventing pumping and flatness and other artifacts"*

So the objective is explicitly **multi-term** — not just macro preservation. Formalised for every
prototype in this programme:

```
score =  w1 * |MACRO 0.1-0.5 Hz|        flatness AND invented slow swings (both are errors)
       + w2 * max(0, PUMP_added)        2-8 Hz movement the source did not have
       + w3 * max(0, ROUGH_added)       8-20 Hz -- grit/artifacts
       subject to   sample peak <= ceiling            HARD GATE, never traded
                    measured at matched ACTUAL gain reduction, realistic push, full corpus
minimise score
```

Loudness is deliberately **not** in the objective. Holding perceived loudness is the user's job via the
gain knob; the engine's job is to make the reduction inaudible. `tools/analysis/mbl_pump.py` already
reports all three terms — the weights are the open voicing question, not the structure.

## 8. Measured facts the implementation must respect

- **`dev_mb_attack_ms` is INERT in `Ramp` attack mode** (bit-identical from 0.5 to 25 ms), and active only
  in `Hybrid` / `Real`. Ramp derives its attack from the lookahead pre-ramp, so there is no attack
  constant to set — **Ramp already is the fast attack** avishali asked for. Every frontier and voicing
  number we have was measured in Ramp. Do not "sweep attack" without first leaving Ramp, and understand
  that leaving Ramp changes the engine's character, not just a time constant.
- Release has no global optimum (§1); default moves to 30 ms on avishali's listening, and the adaptive
  release of axis 1 is the real answer.
