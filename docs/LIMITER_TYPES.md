# MasterLimiter — Limiter Types Roadmap

**Status:** 🧭 ROADMAP / design notes — **not building yet.** Finish the current release/attack/lookahead **voicing** first (DEV knobs), then start here.
**Author:** Claude (architect) from avishali's ideas · **Companion:** `docs/SIGNAL_FLOW.md` (current single-type engine).
**North star:** *maximum loudness without distortion.* The thing that actually caps loudness is **intermodulation distortion (IMD)** — every idea below is a different way to lower it.

---

## The unifying model: a limiter is a *decomposition*
All the planned "limiter types" are the same core move —
> **split the signal into parts → limit each part with time-constants suited to it → recombine.**

They differ only in *how* they split:

| Type | Splits by… | IMD mechanism | Reuse from today |
|---|---|---|---|
| **Dual** (#1) | **time** — transient vs sustain | keeps transients off the leveler (no pumping) | Clean-mode fast/slow split; lookahead-follower release |
| **Multiband** (current) | **frequency** — 2 bands @120 Hz | bands don't intermodulate each other | the shipping engine |
| **Spectral** (#3) | **frequency** — many critical bands | narrow bands → minimal cross-frequency IMD | the 2-band split, generalized |
| **Content** (#2) | **source** — kick/bass/vocal… | sources don't pump each other | nothing yet (hardest) |

**Architectural consequence (the switch seam):** a **Style selector** chooses the *decomposition front-end*; everything downstream is **shared back-end** — per-part `LimiterEnvelope` → recombine → output Ceiling → FinalCeiling brickwall → metering/history. So these are **not separate plugins**; they're interchangeable front-ends on one engine. (Detailed seam design deferred until after voicing — see "Deferred".)

Today's plugin is already a **hybrid** of Dual + Multiband (2-band split + a transient/sustain split + wideband safety), which is why it already beats wideband on IMD in the shootout.

---

## Idea #1 — Dual limiter (fast catcher + slow leveler)  ·  *build first*
Two limiter stages in **series**:
- **Stage A — fast transient catcher:** very quick attack **and** release, short lookahead. Its job is only to tame peaks. **A dedicated fast limiter — NOT the clipper** (the clipper stays *outside* the limiter for now). **Lookahead sets how fast the initial transient is caught** (shorter = snappier/earlier catch).
- **Stage B — leveler:** rounder, handles the sustained signal — longer attack, the smooth **lookahead-follower** release. Does the loudness/density.
- **Macro knob — "how fast/slow Stage B kicks in":** controls the leveler's engagement (its attack/threshold and how much of the total limiting it owns vs Stage A).

**Why first:** highest reuse, lowest risk, and largely prototypable with the DEV knobs we already added (attack, LA Band/Wide, LA Release ms/Poles, per-stage envelopes). It's mostly **topology + one macro knob**, not new DSP — and it's the ideal simple case to validate the Style-switch architecture.

**This is essentially the current voicing, made explicit:** what we're tuning now (fast attack catching transients + a smooth program-dependent release leveling the rest) *is* a dual limiter. Idea #1 just formalizes it into two configurable stages with a balance knob.

---

## Idea #3 — Spectral limiter  ·  *the flagship, build second*
Our 2-band idea taken to the limit. An **STFT (FFT overlap-add)** front-end with many **log-spaced / critical bands** (Bark/ERB), each limited independently with:
- **Frequency-dependent time constants** — **fast highs, slow lows** (perceptually correct, and the natural generalization of our current low=3× slower rule).
- **Q that scales with frequency** (constant-Q / log bands) — the attenuator bandwidth follows the spectrum.

**Why strategic:** directly extends our proven edge — more bands = less intermodulation = more loudness clean. This is the most aligned with what the shootout *discovered*.
**Cost / craft:** known DSP (the Soothe / Gullfoss / Ozone-spectral family — *not* research-grade), but real work: FFT latency, and the difficulty is **avoiding musical-noise/spectral smearing** and keeping **phase coherent**. A multi-slice arc.

---

## Idea #2 — Content-dependent limiting  ·  *split: near-term kernel + long-term dream*
**Honest scoping:** *live ML stem separation in a plugin is a research project, not a slice* — it needs a bundled neural model (CoreML/ONNX), heavy real-time inference, seconds of latency, and it's imperfect. That's arguably a **separate product / long-term R&D track.**

**The achievable kernel inside it (build on top of Spectral later):** **content-*aware* adaptive limiting without true stems —**
- **Heuristic classification:** band energy + transient detection to tag "kick-like / bass / vocal-presence / cymbal" regions (no ML).
- **Distortion-aware controller ("the brain"):** monitors per-band **IMD/THD + loudness online** and reallocates gain to hit max loudness while holding distortion under a threshold. We already compute these metrics offline (`tools/analysis/analyze.py`) — an online version is real and novel.

This delivers most of the "sentient brain" value in real time, as an evolution of the Spectral engine. **Full ML stems = future / separate effort.**

---

## Sequencing
1. **Finish voicing** (current) — bake attack + LA Band/Wide + LA Release ms/Poles into the Auto modes; remove DEV knobs.
2. **#1 Dual limiter** — first real "Style"; proves the switch architecture.
3. **#3 Spectral** — flagship; its own multi-slice arc.
4. **#2 content-aware adaptive** — on top of Spectral; full ML stems remain long-term R&D.

Style selector becomes: **Dual · Multiband · Spectral · [Adaptive]** — decomposition front-ends, one shared back-end.

---

## Deferred (do NOT design yet — after voicing)
- The concrete **Style-selector seam** (interface between decomposition front-ends and the shared back-end; how state/params are scoped per Style; latency handling across types).
- Whether Dual/Multiband/Spectral are exclusive Styles or stackable layers.
- Per-band vs unified controls per Style.

These get designed when voicing is locked and we start #1.
