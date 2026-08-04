# SLICE XOVER-1 — the Open engine's IIR crossover is the whole fidelity deficit

**Status:** spec — **SDK change, read §Scope first** · **Architect:** Claude · **Verify:** Claude · **Audition:** avishali

## The finding (measured 2026-08-05, on the real plugin, no simulation)

avishali: *"distortion more than normal coloring and different sound from the original track... Pro-L2 is
more punchy and more matching to the source."*

**Fidelity to source (residual after removing best-fit gain and lag; lower = closer to the original).
Quiet input, so NO limiting happens anywhere — the only thing acting is split + recombine:**

| config | fidelity | LF lag |
|---|---:|---:|
| **Transparent** — `LinearPhaseCrossover` | **−94.5** | 0 |
| Open, xover 400 Hz — `LinkwitzRileyBandSplitter` | −11.3 | 52 |
| **Open, xover 120 Hz (shipping)** | **−3.9** | **158** |
| Open, xover 60 Hz | −2.5 | 363 |

**With ~3 dB of limiting, Open measures −3.4 — essentially the same as −3.9 with none.**
⇒ **The limiting contributes almost nothing. The crossover is the entire deficit.**

Per-band lag confirms the mechanism — the LF is late against the HF, by an amount that scales with the
crossover frequency (363 samples at 60 Hz, 176 at 120, 50 at 400):

| | <80 Hz | ~200 Hz | ~1 kHz | >5 kHz |
|---|---:|---:|---:|---:|
| Transparent | 0 | 0 | 0 | 0 |
| Open @120 | **176** | 43 | −2 | 0 |

`LinkwitzRileyBandSplitter` is allpass-complementary: magnitude sums flat (which is why every calibration
check passed) but it is **not linear phase**, so group delay rises steeply toward the crossover. At the
shipping 120 Hz setting the low band arrives **~4 ms after** the high band. That smears every transient,
which is exactly "less punchy" and "different from the source".

⚠️ **This is NOT a missing compensation delay.** It scales with crossover frequency, so no fixed offset can
correct it. Only a different topology can.

⚠️ **This corrects a conclusion I published on 2026-08-03.** I wrote that "Open trades timbral fidelity for
macro-dynamic preservation — inherent to multiband". That was wrong: **Transparent is also multiband and
measures −94.5.** The variable is the crossover, not the band count.

## Scope — this touches the shared SDK
`MultibandLimiter` (`melechdsp-hq/shared/mdsp_dsp/.../dynamics/MultibandLimiter.h`) hard-codes
`LinkwitzRileyBandSplitter splitter_;`. The SDK **already contains `filters/LinearPhaseCrossover.h`**, which
the plugin's inline path has used since the custom-filter work.

Shared with **DeNoiser / Quell / CrowdSep**, so:
- **Additive only.** New optional splitter mode; the LR path stays **bit-identical** for every existing caller.
- Default must remain LR so other products are untouched.
- Null-test the LR path as part of the gate. This is not optional.

## The latency argument (why this may be nearly free)
LR was chosen for "0-latency" splitting. **That advantage is worth nothing here**: the plugin already reports
a fixed **3003 samples (~68 ms at 44.1 kHz)** in every configuration (CLIP-1.1), and the Transparent engine
already fits a linear-phase crossover tree inside that same budget.

**Report whether a linear-phase 2-band split at 120 Hz fits inside the existing 3003.** If it does, this
costs nothing the user can perceive. If it does not, the extra latency is avishali's decision, not Cursor's.

> ⚠️ **Retrieval log first.** Read `MultibandLimiter` (splitter member, `prepare`, `process`,
> `getLatencySamples`) and `LinearPhaseCrossover` (Spec, latency, API shape). Report whether the two can be
> composed without restructuring `MultibandLimiter`, and what latency a 120 Hz linear-phase split needs.

## Build
1. SDK, additive: let `MultibandLimiter::Spec` select the splitter — LR (default) or linear-phase.
   Do not reorder or rename anything existing.
2. Plugin: DEV param `dev_mb_xover_type` { `LinkwitzRiley`, `LinearPhase` }, **default `LinkwitzRiley`**
   so the slice lands as a null, on `updateMbEngineRuntimeConfig`'s watch list, and in the UI-4
   control→group association.

## Gate
- [ ] **Default is a null** — LR selected, output bit-identical to pre-slice HEAD (≤ −140 dB).
- [ ] **SDK regression null:** LR path bit-identical for a plain `MultibandLimiter` caller.
- [ ] **The payoff, measured:** with `LinearPhase` selected, fidelity on the quiet-input test must improve
      substantially from **−3.9**. Transparent's −94.5 is the ceiling; anything past ~−11 recovers the gap.
      Report per-band lag too — the <80 Hz figure should go to ~0.
- [ ] **Frontier unmoved or better:** `mbl_frontier2.py` Open+Smart mean was **3.956**. If linear phase
      costs macro-dynamics, report it — do not trade one axis for the other silently.
- [ ] `mbl_calibrate.py` 58/59, sPk −1.00, latency reported honestly (state the new value if it changes).
- [ ] Build clean, AU + VST3, both installed, mtimes.

## Non-goals
- No engine/voicing change beyond the splitter. Not axis 2. No new metering.

## Note
Pre-ringing is the known cost of linear phase and it is a real audible trade at low crossover frequencies.
**avishali auditions before any default changes.** The measurement says fidelity; only ears say "better".
