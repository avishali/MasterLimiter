# MasterLimiter — User Guide (v0.3.0 beta)

A mastering limiter / maximizer: clipper + drive → 2-band (frequency-selective)
true-peak limiter → ceiling, with loudness metering and gain-match.

> **Beta build.** Please report anything that sounds wrong, looks off, or
> behaves unexpectedly. Known beta caveats are listed at the end.

---

## Quick start

1. Insert on your master/bus. Fresh insert = the **Default** preset
   (Ceiling −1.0 dB, limiter on, everything neutral).
2. Pick a **preset** from the header dropdown, or dial it yourself.
3. Raise **Gain** to push loudness into the limiter; watch the **GR meter**.
4. Set the **Ceiling** (Output Level) to your target (−1.0 dB is a safe,
   streaming-friendly default).
5. A/B with **Bypass** (top-right). Use **Gain Match** to compare at equal
   loudness.

**Tip:** double-click any knob's value to type a number (Enter commits, Esc
cancels, click away closes).

---

## Signal flow

```
In → Input trim → Clipper → Gain(drive) → 2-band limiter (Color) → Ceiling(SP/TP)
   → Gain-Match comp → Output trim → Out
```

---

## MAXIMIZER panel

- **Gain (drive)** — 0 to +24 dB. Drives the signal into the limiter for
  loudness. More Gain = more limiting (deeper GR).
- **Limiter (power button)** — turns the limiter stage on/off. Teal = on.
- **SP / TP** — ceiling mode. **SP** = sample-peak; **TP** = true-peak
  (inter-sample, oversampled) — safer for lossy codecs / streaming.
- **Gain⇄Ceiling Link** — when on, Gain and Ceiling move inversely, so you can
  push loudness while the output level stays put.
- **Output Level (Ceiling)** — −24 to 0 dB. The level the output will not
  exceed.
- **Clipper** — a clip stage *before* the limiter that catches fast transients
  for extra loudness. **Hard** = clamp; **Soft** = soft (tanh) knee. More
  Clipper = harder clipping.
- **Character** — **Clean / Tight / Aggressive**. The limiter's attack/release
  character, gentlest → most aggressive.
- **Release** — manual release time (how fast gain recovers). Disabled when
  **Auto** is on.
- **Auto** + mode — program-dependent release. **Transparent** (smoothest /
  least pumping), **Balanced**, **Reactive** (punchiest recovery).
- **Color** — frequency decoupling of the 2-band limiter. **Open** (high) keeps
  the highs clear when the bass is limited (less pumping, brighter); **Glued**
  (low) links the bands (warmer, more classic). **Balanced** in the middle.
- **Stereo / M·S** + **Link** — stereo handling domain and link amount.
  **Stereo** links/unlinks Left/Right; **M/S** links/unlinks Mid/Side.
  **Link 100%** = fully linked (mono-coupled limiting); lower = more
  independent (wider).
- **GR meter** — the dual bars show gain reduction per channel (L|R, or M|S in
  M/S mode). Click to reset the held maximum.

## I/O panel

- **Input / Output faders** — trim levels before/after the processor; the L/R
  link button ties the pair.
- **Level meters** — peak (cap) and, with **RMS** enabled, an RMS fill;
  numeric peak + RMS below. **Click a meter to reset** its peak/max.
- **Range +/-** — zoom the meter scale (Full / 48 / 24 / 12 / 6 dB).
- **LUFS box** — **M** momentary, **S** short-term, **I** integrated loudness.
- **Reset peaks** — clears the latched maxima.

## GAIN MATCH

- **Auto / Track** — loudness-compensate the output so on/off (and tweaks) are
  compared at matched loudness, not just matched peak.
- **Learn** — capture the current loudness as the reference.

## Header

- **Preset menu** — 5 factory starting points: Default, Transparent Master,
  Loud & Aggressive, Gentle Glue, Clipper Punch.
- **Bypass** — true bypass (latency-compensated).

---

## Known beta caveats

- **True-peak meter readout** is a max-sample-peak *approximation* — the TP
  *limiting* is real (oversampled), but the displayed TP number is approximate.
- **Latency** ≈ 541 samples (~11 ms) reported to the host (lookahead +
  oversampling). Bypass is latency-compensated.
- **Auto-release** mode voicings are first-pass; fine-tuning is planned.
- **AAX** (if provided) requires PACE signing; depending on the build you may
  receive VST3/AU only.
- Visual look is near-final; deeper knob/slider art is a later pass.

---

*MasterLimiter v0.3.0 (beta) · MelechDSP*
