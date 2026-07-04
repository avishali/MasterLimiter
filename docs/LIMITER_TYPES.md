# MasterLimiter — Limiter Types Roadmap

**Status:** 🧭 ROADMAP — **near-term plan is now concrete & measured** (see the 2026-07-04 section below). Alpha shipped; next: the Hybrid-attack experiment as a cheap test of the Dual (#1) idea. The longer-term Spectral/Content sections remain design notes.
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

## 2026-07-04 — Measured findings → concrete near-term plan

An investigation into "the limiter doesn't hold the ceiling / FinalCeiling works hard" became a full characterization of the attack/transient behavior. All below is **measured** (offline rig + avishali DAW-confirmed on a real live-show mix).

**Core finding — one attack curve can't be both transparent and transient-catching:**
- **Real attack** forces `attackSamples_ = 1` (SDK `LimiterEnvelope.cpp:279`) → *no* lookahead pre-ramp → pure RC follower → transparent on sustained (THD −58 dB) but **cannot catch transients at any lookahead or oversampling** (real mix: **+4 to +8.6 dB** peak overs; crest untouched). This is structural, not resolution — the attack simply never pre-empts.
- **Ramp attack** → lookahead pre-ramp + hard snap → catches transients (holds ceiling; crest 15.4→11.3 on the mix; ~+1 dB louder) but distorts sustained (THD −41 dB).
- **Lookahead shipped at 0 ms** (placeholder) → fixed to Band 2 / Wide 5 ms so Real holds the ceiling on *sustained* material, latency-free; TruePeak FinalCeiling made default (`SLICE_LOOKAHEAD_CEILING_FIX`).

**Stage-1's concrete spec (from the real mix, crest 15.4 dB):** reduce crest **~3–4 dB** on peaks (→ ~11–12), holding true-peak, at Real-grade distortion.

**Hybrid attack — TESTED, NOT the shortcut (2026-07-04):** `AttackMode::Hybrid` (pre-ramp + RC follower) shipped and was swept on the rig. Result: it **does** catch transients (fast RC ≤0.2 ms + pre-ramp ≥4 ms → crest ~13, holds ceiling, like Ramp) — Cursor's "behaves like Real" was just the default 3 ms RC. **But at the catching setting its 50 Hz THD is −44 dB ≈ Ramp's −42 (dirty), nowhere near Real's −51.** There is no setting that catches *and* stays clean. Hybrid is a useful continuous Ramp↔Real **morph knob**, kept for DEV/A-B, but does **not** defer the rebuild.

**⚠️ The measured law this exposed:** *any wideband attack fast enough to catch a transient (~1–6 ms) distorts the low end,* because that's within a 50 Hz cycle (20 ms). Ramp / Real-fast / Hybrid all hit the same wall. **The escape is decoupling, not a better attack curve:**
- **Frequency decoupling → per-band attack** (fast highs / slow lows). Extends the existing multiband; fast HF gain is far less audible than fast LF gain. **Promoted from footnote to the next cheap experiment.**
- **Time decoupling → Stage-1 transient-gated catcher** (avishali's design) — acts only during transients, leaves sustained bass alone.
- **Hard physical limit:** a *low-frequency* transient (kick) can't be caught fast without distorting — "fast" vs 50 Hz is within-cycle. That residual is where a touch of clipper/FinalCeiling legitimately earns its place (short LF transient = well-masked). Neither per-band attack nor two-stage fully solves LF transients.

**avishali's concrete multi-stage design (the real #1, if Hybrid isn't enough):**
- **Stage 1 — fast catcher:** wideband, lookahead pre-ramp, *move the whole transient down, don't clip the tip.* Optional: transient-accurate detection (gate/isolate the low-level bed so it triggers on transients, not sustain).
- **Stage 2 — slow multiband leveler:** the **existing** 3-band + Real (slow/transparent) + per-band low-end release. Only levels the body — transients already tamed → no distortion. *Mostly already built.*
- **Clipper — pre/post** selectable (character).
- **Final ceiling — TruePeak safety only** (catches ~nothing once 1+2 work).

**"Learn" / auto-target** (folds into #2's brain, offline-first): scan a file — or a DAW-timeline playback pass — build the full loudness/crest/spectral/transient picture, then solve input gain + thresholds for smooth limiting at a **target LUFS**. Offline-file version is near-term-achievable; timeline capture + the online adaptive brain are v1.0+.

**Phasing:**
- **0.4 (quick):** lookahead+TP ceiling fix · **Hybrid attack experiment** · clipper pre/post · per-band GR readouts.
- **0.4 → v1.0 (medium):** full Stage-1 fast catcher feeding the existing multiband as Stage 2 — *only if Hybrid is insufficient.*
- **v1.0 (big, plan separately):** Learn/auto-target + content-aware brain (#2).

**Cautions:** two serial lookahead stages add latency (budget it — not the free single-stage pad trick); the fast catcher must grab only the *overshoot*, not squash punch (voicing-critical); gain-staging between stages is a deliberate decision.

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
