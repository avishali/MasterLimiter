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
| 2 | **Attack** | ramp shape decided from what the lookahead window actually contains | new — see §4 |
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

## 4. Axis 2 — attack, decided from the lookahead (avishali, 2026-08-02)

> *"attack should also be program dependent, using the lookahead to determine how fast to react."*

This is the right formulation and it supersedes the weaker "fast globally, LF slows down" proposal.

### The insight
The lookahead window **already contains the future**. Today's `Ramp` mode uses it, but with a *fixed*
shape — which is exactly why `dev_mb_attack_ms` is inert in Ramp (§8): there is no decision being made,
just a constant pre-ramp. Program-dependent attack makes that shape a **decision** taken from what is
actually sitting in the buffer.

### The lookahead window is a budget
The ramp must finish before the peak arrives — that is what makes a lookahead limiter distortion-free
instead of a fast RC follower. So the lookahead time is a **budget**, and the question per event is how
much of it to spend:

| what the lookahead sees | ramp | why |
|---|---|---|
| isolated sharp transient, HF-dominated | **short** — spend little of the budget | ear is insensitive to fast gain change on HF; a long ramp needlessly ducks the material before the hit |
| large overshoot on LF content | **long** — spend most of the budget | fast gain change on a low-frequency waveform IS waveform distortion (the measured "wideband attack distortion law", `SPECTRAL_ENGINE_DESIGN.md`) |
| sustained loud passage, small overshoot | **long / gentle** | nothing transient to catch; a fast attack here only adds movement |
| dense transient train | **short, and hold** | re-ramping per hit is a modulation source; better to stay down |

Signals to extract from the lookahead buffer: **time-to-peak**, **overshoot height above threshold**,
**LF content of the upcoming block**, **transient density**. These are the "eyes" in the
eyes -> brain -> hands framing of `ADAPTIVE_THRESHOLD_ENGINE.md` §3.

### Why this unifies the whole engine
All three axes want to look at the same thing. **One analysis of the lookahead window can drive all three
decisions** — release rate (axis 1), ramp shape (axis 2), and reduction depth (axis 3) — instead of three
independent detectors. That is cheaper, easier to reason about, and means the axes cannot fight each other
by acting on different views of the signal.

```
        lookahead buffer  ->  ANALYSE  ->  { time-to-peak, overshoot, LF share, density, GR depth }
                                             |            |             |
                                          attack       release       depth
                                          (ramp)       (recovery)    (how hard)
```

This also composes with §7.2: the whole block scales by `f(GR_depth)`, so at light push the analysis
output is ignored and the engine is Open exactly.

### Consequence for lookahead time
A bigger budget buys more room to be gentle. `dev_mb_lookahead_ms` (currently 5 ms) becomes a real design
parameter rather than a fixed cost, and the latency trade is now a *voicing* decision. Worth sweeping once
axis 2 exists — but note the plugin's reported latency is fixed at the maximum by design (CLIP-1.1), so
raising the lookahead raises latency for every configuration.

## 5. Sequencing

| Slice | Content | Risk |
|---|---|---|
| **SMART-0** | Defaults: release 150 → 30 ms, attack → fast (value from the stage-2 sweep). No new DSP. | trivial |
| **SMART-1** | Adaptive **release** in the Open engine's `MultibandLimiter` path (axis 1). DEV-toggled, A/B against fixed. | medium |
| **SMART-2** | Adaptive **attack** (axis 2): ramp shape from the lookahead analysis. | medium |
| ~~**SMART-3**~~ | ⛔ **CLOSED — NOT BUILDABLE.** The oracle prototype returned STOP: a *causal* oracle (the upper bound on any causal controller) cannot match the Smart engine we already ship, at LA 5/10/20/50 ms. The offline win is intrinsically non-causal. See §10. | — |

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
score =  w1 * |MACRO 0.1-0.5 Hz|   +   w2 * |PUMP 2-8 Hz|   +   w3 * |ROUGH 8-20 Hz|
       subject to   sample peak <= ceiling            HARD GATE, never traded
                    measured at matched ACTUAL gain reduction, realistic push, full corpus
minimise score
```

⚠️ **Corrected 2026-08-02 — use `|deviation|` in EVERY band, not `max(0, added)`.**
The first version penalised only *added* movement. But every limiter we have measured *removes* movement
in the PUMP and ROUGH bands (all values negative), so `max(0, .)` was always zero and the whole score
silently collapsed to `|MACRO|` alone. Removal is exactly the **flatness** avishali asked us to prevent —
it has to count. This was not academic: on the very first measurement it inverted the verdict, scoring the
existing Smart engine as the *worst* of three when on the correct objective it is the *best* (§9).

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

## 9. Axis 1 is already substantially built — `ReleaseEngine::Smart` measured 2026-08-02

`ReleaseEngine::Smart` (commit `775a1c3`, July, `dev_smart_fast_ms` / `_slow_ms` / `_sustain_ms` / `_leak`)
had never been measured against a real objective — it predates the MACRO/PUMP metric by three weeks.
Measured now, inline path, 60 s excerpts, matched to ~3 dB RMS-GR, ceiling held at −1.00 by all three:

| source | engine | \|MACRO\| | \|PUMP\| | \|ROUGH\| | **total** |
|---|---|---:|---:|---:|---:|
| live-show | Adaptive | 0.93 | 1.42 | 0.66 | 3.01 |
| live-show | Lookahead *(shipping)* | **0.53** | 1.32 | 1.99 | 3.84 |
| live-show | **Smart** | 1.19 | **1.24** | **0.47** | **2.90** ✅ |
| homework | Adaptive | **0.51** | 1.43 | 1.00 | **2.94** ✅ |
| homework | Lookahead *(shipping)* | 0.96 | 1.60 | 2.64 | 5.20 |
| homework | **Smart** | 1.22 | **1.02** | **0.73** | 2.97 |

**Smart is not inert and it is not a failure.** It trades macro preservation for markedly better
preservation of beat-rate (PUMP) and fast (ROUGH) movement — best or tied-best overall on both sources,
and it beats the shipping `Lookahead` engine on total deviation by a wide margin on both.

`Lookahead` — what Transparent ships with — is the **worst** of the three on total deviation, because it
flattens the ROUGH band hardest (1.99 / 2.64).

⇒ **SMART-1 shrinks.** It is not "build adaptive release"; it is **port the existing Smart release into the
Open engine's `MultibandLimiter` path and drive it from the unified lookahead analysis (§4), scaled by
`f(GR_depth)` (§7.2)**.

⚠️ Caveats before acting: 2 sources, 60 s, one operating point, and the four `dev_smart_*` params are at
untuned defaults. Confirm across the full corpus and sweep those params before treating this as settled.


## 10. ⛔ Axis 3 is closed — measured, not assumed (2026-08-02)

`tools/analysis/mbl_depth_oracle.py` (SMART-3P / 3P.1) answered the go/no-go question **before any C++**.

**Causal oracle vs plugin Smart, corpus mean Δ** (positive = better than Smart):

| LA 5 ms | LA 10 ms | LA 20 ms | LA 50 ms |
|---:|---:|---:|---:|
| −0.352 | −0.376 | −0.470 | **−0.252** |

The causal oracle is an **optimiser, not a heuristic** — it is the ceiling on what *any* causal controller
could reach. It does not merely fail to clear the +0.5 gate; **it cannot match the engine we already
ship.** Non-causally the same optimiser wins by Δ_depth ≈ +1.87, so adaptive depth's advantage is
**intrinsically non-causal**: it comes from seeing the whole file, which a limiter never can.

**Do not revive axis 3 without new evidence** — a fundamentally different controller class, or a lookahead
far beyond what the latency budget allows.

### What this cost, and what it saved
One day of Python. `ADAPTIVE_THRESHOLD_ENGINE.md` §8 (validation-first) exists because P-A came back
negative in July *after* C++ work had begun. This time the negative arrived before a line of DSP.

### Two caveats recorded so they are not mis-cited later
- **SMART-3P's first verdict (+2.380 → PROCEED) was confounded** — a broadband oracle scored against the
  2-band plugin measures topology *and* optimisation together. Cursor raised this itself. An oracle must be
  compared against the same topology.
- **Δ_topology (+0.36 mean, +1.16 live-show) is NOT clean**: the broadband baseline could often not reach
  Smart's RMS and was "scored at peak-safe max loudness" — quieter, therefore less limited, therefore
  flattered. Before anyone chases "broadband beats our 2-band reconstruction", re-run it at equal loudness.
- live-show alone showed causal headroom (+2.56 at LA 5) that does not hold on the corpus. Genre-specific,
  not a general engine.
