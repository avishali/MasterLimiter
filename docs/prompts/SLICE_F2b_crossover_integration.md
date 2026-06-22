# Cursor Build Prompt — Slice F-2b: Wire LinearPhaseCrossover into the plugin (DEV-auditionable)

**Context:** `docs/CUSTOM_FILTERS.md`, F-2 prompt (`SLICE_F2_linearphase_crossover.md`). F-2a shipped the
`LinearPhaseCrossover` engine (`mdsp_dsp`, `735c6bc`); a symmetry fold-and-add was added to
`LinearPhaseFIR` (halves OS-rate MACs). This slice replaces the IIR crossover in the plugin and exposes
**DEV controls to audition transition width / attenuation by ear** before a shipping value is baked.

Plugin work is in `Source/PluginProcessor.{h,cpp}` (MasterLimiter repo). SDK already built against the
sibling `melechdsp-hq`.

> ⚠️ Reworks the shipping signal path + plugin latency. Gate is the **offline rig Color sweep**, not just
> unit tests. Decisions already made: **audition-first** (DEV-tunable spec), **direct-conv + symmetry**
> for CPU (profile, don't pre-optimize to FFT).

---

## 1. Replace the crossover instances

`detectCrossover_` / `applyCrossover_` (`PluginProcessor.h:165-166`, `juce::dsp::LinkwitzRileyFilter`) →
two `mdsp_dsp::LinearPhaseCrossover`. Prepared at `PluginProcessor.cpp:185-189` with `xoSpec` at
**`osSampleRate`** (4× — keep that rate). Reset sites `:321-322`. Usage: detect loop `:1047-1060`,
apply loop `:1078-1090`, `snapToZero()` at `:1060`/`:1090` (LinearPhaseCrossover has no snapToZero —
drop those calls).

`processSample` signature is now `(ch, x, low, high, xDelayed)`.

## 2. Phase-coherent reconstruction (THE correctness point — verify, don't assume)

Today `:1086` blends the **undelayed** `delayed` with the split bands. With the FIR split,
`low+high == delay(x, M)`. The linked/wideband reference MUST become the engine's `xDelayed`:

```cpp
float low = 0.0f, high = 0.0f, xDelayed = 0.0f;
applyCrossover_.processSample (ch, delayed, low, high, xDelayed);
bandLimitedBuf_.getWritePointer (ch)[i] =
      linkedGain * xDelayed                                   // was: linkedGain * delayed
    + oneMinusBandLink * (low * gainLow[i] + high * gainHigh[i]);
```

If the linked term stays `delayed`, intermediate Color re-creates the cancellation we are removing.
The offline Color sweep MUST show the dip gone — that is the whole slice.

## 3. Detection-path latency alignment

`detectCrossover_` now delays the band-peak streams by M, so the envelope gain curve arrives M samples
late vs the audio. The applied audio is delayed by `lookahead_` (`:1081`). Compensate so gain-to-audio
timing is preserved — reduce the `lookahead_` delay by M (clamp ≥ 0), or advance detection equivalently.
Confirm GR onset timing is unchanged vs the IIR build on a transient (measure, don't eyeball).

## 4. Fixed latency via Nmax zero-padding (keeps host PDC stable while auditioning)

Changing the DEV transition width changes M — which would thrash host PDC. Avoid this:

- Pick a **worst-case spec** = the steepest/most-attenuating the DEV ranges allow (e.g. transition floor
  60 Hz, atten ceiling 72 dB). Design that to get **Nmax / Mmax** once at prepare.
- For any gentler DEV setting, design the (shorter) kernel and **center it, zero-padded, into an
  Nmax-length kernel**. A symmetric kernel zero-padded symmetrically stays symmetric with group delay
  **Mmax** and identical frequency response. So the crossover latency is **always Mmax**, regardless of
  the DEV setting → PDC never moves while you audition.
- Add the OS-domain crossover latency (`Mmax / osFactor`, host samples) into `baseLatencySamples_`
  (`:312`); keep `dryDelay_` (`:355`) and `lookaheadPad_` consistent; `setLatencySamples` `:333`/`:868`.

(If you'd rather not pad inside the plugin, add an optional `setKernelLength(Nmax)` to
`LinearPhaseCrossover` that zero-pads internally — cleaner. Your call; keep latency fixed either way.)

## 5. DEV controls (audition) — RT-safe redesign

Add temporary DEV params (mirror the existing `dev_*` pattern in `Parameters.cpp` + the DEV window,
SIGNAL_FLOW.md §6; all removed before 0.4):

- `dev_xover_cutoff_hz` (e.g. 40–250 Hz, default 120)
- `dev_xover_transition_hz` (e.g. 60–240 Hz, default ~120 — gentler = shorter kernel)
- `dev_xover_atten_db` (e.g. 48–72 dB, default 60)

**Redesign is NOT RT-safe** (`kaiserLowpass` allocates). On a DEV param change:
- Recompute the centered/zero-padded Nmax kernel **off the audio thread** (parameter listener /
  `AsyncUpdater` / timer), into a staging `LinearPhaseCrossover` (or staging tap buffer).
- Publish to the audio thread via a **lock-free swap** (double-buffer + `std::atomic` active pointer, or
  an atomic "new taps ready" flag the audio thread consumes at block start). No locks, no allocation on
  the audio thread. A tiny gain ramp / the existing bypass-fade over the swap avoids clicks.
- Latency stays Mmax across all settings (§4), so no PDC churn on tweak.

## 6. Gate (paste raw output)

- `git status --short`; build MasterLimiter (Standalone/AU/VST3) clean; full `mdsp_dsp_tests` exit 0.
- **Offline rig** (`tools/analysis/hammerstein.py` + sweep_render): Color sweep 0→25→50→75→100 —
  low-end dip **gone** at all values (show the before/after); band sum flat ±0.01 dB; THD/phase no worse
  than baseline (−119 dB passthrough, −81 dB hard-limit).
- **CPU**: report % in host at Color 50, OS 4×, with the gentle default — so we decide if direct-conv+fold
  ships or we escalate to F-2c (FFT-partition) / multirate.
- Confirm DEV cutoff/transition/atten audibly change the split with **no PDC change** and no clicks on tweak.

## Out of scope
F-2c (FFT-partition), multirate low-band split, oversampler port (F-3), movable-freq as a *user* control,
3-band. Bake the auditioned shipping spec + remove DEV params in a later close-out slice.
