# MasterLimiter — Filter & Crossover Quality Roadmap

**Status:** 🧭 ROADMAP — after voicing. The keystone is a **custom linear-phase FIR**; most items below fall out of it.
**Measure everything** with the offline rig (`tools/analysis/sweep_render.py`, `sweep_isolate.py`, and the planned Hammerstein analyzer) — before/after THD + phase numbers, not eyeballed spectrograms.

**Baseline (measured 2026-06-10, offline pedalboard render):** passthrough −119 dB, hard-limiting worst component −81 dB. The plugin is already clean; these items are *refinements*, not bug-chasing. (The Plugin Doctor "harmonic tent" was a latency-induced measurement artifact, not real audio.)

---

## B. Measurement tooling — Hammerstein / H1-H2-H3  *(build first, supports all below)*
Add a **Farina exponential-sweep** analyzer to the offline rig: an exp sweep deconvolved with its inverse separates harmonic orders into distinct impulses → clean **H1/H2/H3+ levels and per-order phase** vs frequency. Gives us the same data as Plugin Doctor's Hammerstein tab, but on a rendered file (latency-independent, scriptable, reproducible). No plugin code. Claude can build this.

## C. Custom linear-phase FIR  *(keystone)*
Build our own FIR (claim: better than JUCE's stock `Oversampling`/`LinkwitzRiley`) for:
- **Complementary linear-phase crossover** — bands sum to flat with **zero phase shift** (fixes D + the crossover-phase concern in E).
- **Cleaner oversampling anti-alias** — sharper, with the passband flat further up (supports F).
Trade-off: linear-phase FIR adds latency (symmetric). We already run lookahead latency, so this is affordable. Needs careful design (length vs ripple vs latency) — measure with B.

## D. Color knob phase shift (0–100%)  *(fixed by C)*
Today, intermediate Color blends the **allpass-split band** (Linkwitz-Riley = phase-shifted) against the **full-band original phase** → partial cancellation, low-end dip; endpoints (0/100) are clean. A **linear-phase complementary crossover (C)** makes the split phase-coherent with the full band → no cancellation at any Color value. This is the direct fix.

## E. Crossover overhaul  *(rides on C)*
- **Movable crossover frequency** — replace the fixed 120 Hz with a user **selector / freq control**.
- **Optional 3rd band** — 2-band → 3-band split (low/mid/high), each with its own limiter envelope. Generalizes the current per-band engine; also a stepping stone toward the spectral limiter (#3 in `LIMITER_TYPES.md`).
- **Crossover phase** — verify/clean with the linear-phase FIR.

## F. Oversampler / HF polish  *(custom-FIR / higher-OS)*
- **Move the anti-alias high-cut up**, far from Nyquist, so the top octave stays flat (no audible-band rolloff).
- **HF ~90° phase @20 kHz** — the integer-latency FIR phase tilt (inaudible, low-pri polish); a native-integer / linear-phase FIR addresses it.
- Possibly raise OS 4× → 8× for the nonlinear stages (also lowers the −81 dB limiting distortion if we ever want it lower; low priority — −81 dB is already excellent).

---

## Dependency order
1. **B** — Hammerstein/3-harmonic analyzer (tooling).
2. **C** — custom linear-phase FIR (keystone).
3. **D / E / F** — fall out of C, each measured with B.

Pairs with the **spectral limiter (#3)**, which needs exactly this multi-band/FIR foundation.
