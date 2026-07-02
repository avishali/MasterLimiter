# SLICE — 3-band multiband limiting (crossover tree + mid envelope + N-band Color)

**Status:** ready for Cursor **after `SLICE_BAND_GR_VIZ` closes** · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali
**Repos:** plugin `MasterLimiter` (+ SDK `melechdsp-hq` only if a helper is genuinely missing — prefer reuse; SDK commit first if touched).
**Companion ADR:** **ADR-0012 (3-band multiband)** — Claude provides; skeleton in §0. Follows ADR-0009 (2-band) style.
**Companion doc:** `docs/SIGNAL_FLOW.md` §1, §2.9–2.14, §4.1, §6, §8.
**Depends on:** `SLICE_BAND_GR_VIZ` (per-band GR atomics + `HistoryFrame` band fields + 3-band meter already exist; this slice **fills the MID band** they reserved).

> ⚠️ **Read before coding — hard-won lessons (memory):**
> - **Crossover swap race** — kernel rebuild uses an `atomic_flag` spinlock on bank ownership + duck-and-swap; the swap MUST be **block-aligned** (a mid-block flip crashed before). Reuse the EXISTING duck-swap machinery; generalize it to the tree, do NOT reinvent it.
> - **Stale-binary gotcha** — verify the installed AU mtime before auditioning; a "still 2-band" report is usually a stale bundle.
> - **Latency honesty** — report the EXACT integer host latency; JUCE `setUsingIntegerLatency` rounding shows as HF phase tilt. Detect + apply crossovers BOTH add group delay and they CANCEL against detection lookahead — do not double-subtract.

---

## 0. ADR-0012 skeleton (Claude finalizes in `melechdsp-hq/docs/DECISIONS/`)
- **Decision:** extend the 2-band linear-phase multiband to **3 bands** via a **crossover tree** of two `mdsp_dsp::LinearPhaseCrossover` splits (Low↔Mid at `f_lo`, Mid↔High at `f_hi`), reusing the class unchanged.
- **Reconstruction invariant:** `lowAligned + mid + high == delay(x, M1+M2)` exactly (linear phase), so Color=0 (linked) stays transparent.
- **Latency:** crossover group delay grows from `M1` → `M1 + M2`; both rounded to a multiple of the OS factor (4) so host PDC divides exactly. Plugin latency increases by `M2`; reported latency stays constant across DEV auditioning via the existing fixed-latency pad.
- **Color/link:** generalized N-band — `gLinked = min(gainLow,gainMid,gainHigh)`, `gXxxOut = bl·gLinked + (1−bl)·gainXxx`. `band_color` remains the single source but is **removed from the shipping surface** (relocated to the DEV dock for tuning) pending a purpose-built multiband control (future slice).
- **Params:** frozen `band_color` unchanged (relocated in UI, not re-IDed). New **DEV** params only (temporary): high split spec + mid release scale. No new frozen IDs this slice.
- **Alternatives rejected:** parallel bandpass bank (worse reconstruction/CPU); moving to the STFT path (bigger, deferred — still in backlog).

---

## Allowed files to touch
```
Source/PluginProcessor.h
Source/PluginProcessor.cpp
Source/parameters/ParameterIDs.h
Source/parameters/Parameters.cpp
Source/ui/DevControlsComponent.h / .cpp        # high-split DEV controls + mid release + Band Link relocation
Source/ui/MainView.cpp                          # grey/disable the shipping Color knob
Source/ui/meters/GainReductionMeter.h / .cpp   # per-band SOLO toggle buttons under each band bar
Source/PluginProcessor.cpp  (GR loop)           # fill MID into the Slice-A per-band GR taps
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
melechdsp-hq/docs/DECISIONS/ADR-0012-masterlimiter-3band.md   (Claude authors; Cursor references)
PROMPTS/SLICE_3BAND_DSP_CLOSE.md                (new, at close)
```
**Non-goals / STOP if scope expands:** no new **frozen** user params (DEV only this slice); no change to the wideband/final-ceiling stages beyond consuming the 3-band `bandLimitedBuf_`; no metering-format change (Slice A froze it at N=3); no removal of `band_color` (relocate only); no clipper/loudness/IO change.

---

## 1. DSP topology — crossover tree (the core)

### 1a. Members (`PluginProcessor.h`)
The existing `detectCrossover_[2]` / `applyCrossover_[2]` (banked) become **stage-1 (Lo↔Mid)**. Add **stage-2 (Mid↔Hi)** for both roles, plus a low-band alignment delay:
```cpp
// Stage 1 (Lo↔Mid) — the existing pair, semantically renamed in comments (IDs/members may keep names):
mdsp_dsp::LinearPhaseCrossover detectXoLoMid_[2];   // was detectCrossover_[2]
mdsp_dsp::LinearPhaseCrossover applyXoLoMid_ [2];   // was applyCrossover_[2]
// Stage 2 (Mid↔Hi) — NEW:
mdsp_dsp::LinearPhaseCrossover detectXoMidHi_[2];
mdsp_dsp::LinearPhaseCrossover applyXoMidHi_ [2];
// Align the LOW band (out of stage 1) by stage-2 group delay so all 3 bands co-time:
mdsp_dsp::LookaheadDelay<float> detectLowAlign_;    // delay = M2
mdsp_dsp::LookaheadDelay<float> applyLowAlign_;     // delay = M2
```
Keep the existing bank/duck-swap state (`activeCrossoverBank_`, `xoDuck*`, fade counters). The duck now covers the **whole tree** reconstruction.

### 1b. Prepare (`prepareToPlay` / `prepareCrossoverBanks`)
- Prepare BOTH stages on BOTH banks with `prepareFixedLatency(worstCaseSpec, nch, /*latencyAlignment=*/osFactor)`; install active kernels from params (§2).
- `M1 = detectXoLoMid_[bank].getLatencySamples()`, `M2 = detectXoMidHi_[bank].getLatencySamples()`. Assert each is a multiple of `osFactor`.
- `detectLowAlign_/applyLowAlign_.prepare(nch, M2); setDelaySamples(M2);`
- **Total crossover group delay = M1 + M2.** Wherever the 2-band code used the single crossover latency (detection-lookahead reduction, latency accounting, `baseLatencySamples_`, `dryDelay_`, `setLatencySamples`), use `M1 + M2`. Verify the reported host latency is exact-integer (memory: rounding = HF tilt).

### 1c. Per-sample split — detection (`processCore`, replaces the L1281–1304 block)
```cpp
// stage 1: x -> low1, high1  (both at latency M1)
detectXoLoMid_[xoBank].processSample (ch, x, low1, high1, xDel1);
// stage 2: high1 -> mid, high (both at latency M1+M2)
detectXoMidHi_[xoBank].processSample (ch, high1, mid, high, xDel2);
// align low by M2 so low,mid,high co-time at M1+M2:
const float low = detectLowAlign_.pushPop (ch, low1);
// per-band peaks (max across channels), as today for low/high, plus mid:
peakLow[i]  = std::max (peakLow[i],  std::abs (low));
peakMid[i]  = std::max (peakMid[i],  std::abs (mid));
peakHigh[i] = std::max (peakHigh[i], std::abs (high));
```
> `xDel2` from stage 2 == `delay(high1, M2)`; the fully-aligned full-band reference is `low + mid + high` (used as `xDelayed` on the apply side). You do not need a separate wideband delay for the linked term — reuse the reconstruction sum.

### 1d. Mid envelope
Add `mdsp_dsp::LimiterEnvelope envelopeMid_;` (declare, prepare with the same spec as low/high, configure via the existing `configureEnvelope()` lambda). Process:
```cpp
envelopeLow_ .process (peakLow,  gainLow,  osN);
envelopeMid_ .process (peakMid,  gainMid,  osN);   // NEW
envelopeHigh_.process (peakHigh, gainHigh, osN);
```
Mid release scale ← new DEV `dev_mid_band_release_scale` (default 1.0), applied like `dev_low/high_band_release_scale`.

### 1e. N-band Color/link blend (replaces L1346–1351)
```cpp
const float bl = bandLinkSmoothed_.getNextValue();
const float oneMinusBl = 1.0f - bl;
const float gLinked = std::min (gainLow[i], std::min (gainMid[i], gainHigh[i]));
const float linkedGain = bl * gLinked;
gLowOut [i] = linkedGain + oneMinusBl * gainLow [i];
gMidOut [i] = linkedGain + oneMinusBl * gainMid [i];   // NEW buffer
gHighOut[i] = linkedGain + oneMinusBl * gainHigh[i];
```

### 1f. Audio reconstruction — apply tree (replaces L1353–1364)
```cpp
const float delayed = lookahead_.pushPop (ch, d[i]);
applyXoLoMid_[xoBank].processSample (ch, delayed, aLow1, aHigh1, aDel1);
applyXoMidHi_[xoBank].processSample (ch, aHigh1, aMid, aHigh, aDel2);
const float aLow = applyLowAlign_.pushPop (ch, aLow1);
const float xDelayed = aLow + aMid + aHigh;              // == delay(delayed, M1+M2)
bandLimitedBuf_[ch][i] = duck * ( linkedGain * xDelayed
    + oneMinusBl * (aLow * gainLow[i] + aMid * gainMid[i] + aHigh * gainHigh[i]) );
```

### 1g. Scratch buffers (`prepareToPlay`, mirror the low/high allocations at L230–242)
Add: `peakMidBuf_`, `gainMidBuf_` (1×osMax), `gMidOutBuf_` (1×osMax). Reuse the existing `low1/high1/mid/high` as per-sample locals (no per-sample buffers needed for the split beyond peaks/gains).

---

## 2. Crossover params — DEV-first (two splits)

### 2a. Keep (stage 1 = Lo↔Mid) — existing frozen-name DEV IDs, unchanged:
`dev_xover_cutoff_hz` (default 120), `dev_xover_transition_hz` (120), `dev_xover_atten_db` (60) → now drive **stage 1**.

### 2b. Add (stage 2 = Mid↔Hi) — new DEV IDs (`ParameterIDs.h`, `Parameters.cpp`):
| ID | Type | Range | Default |
|---|---|---|---|
| `dev_xover_hi_cutoff_hz` | Float (Hz) | 800 – 8000 (skew log) | **2500** |
| `dev_xover_hi_transition_hz` | Float (Hz) | 200 – 2000 | **600** |
| `dev_xover_hi_atten_db` | Float (dB) | 48 – 72 | **60** |
| `dev_mid_band_release_scale` | Float (×) | 0.5 – 8.0 | **1.0** |

`readCrossoverSpecFromParams()` → split into `readLoMidSpec()` / `readMidHiSpec()`. Kernel rebuild (the `heavyCrossoverDirty_` timer path) rebuilds **both** stage kernels into the pending bank, then does ONE block-aligned duck-swap (reuse existing machinery — see the memory warning).

### 2c. UI (`DevControlsComponent`) — CROSSOVER section
- Relabel the existing three as **Lo/Mid** split.
- Add **Mid/Hi Cutoff / Transition / Atten** sliders + **Mid Release** slider.
- Add a **Band Link (Color)** slider **attached to `band_color`** here (single source; relocated for tuning). Tooltip: *"Multiband link — 0 = glued/linked, 100 = independent 3-band. Shipping control TBD."*

---

## 3. Shipping Color knob — disable/hide (`MainView.cpp`)
Grey out and disable the main **Color** knob (keep the component for layout, `setEnabled(false)`, muted look, tooltip *"Multiband control being redesigned for 3-band — tune via DEV Band Link for now."*). Do **not** detach or delete the `band_color` attachment (frozen param, preset compat). This matches the Slice-16 "hide but keep frozen default" pattern.

---

## 4. Fill MID into the Slice-A GR taps (`PluginProcessor.cpp` GR loop)
Slice A left `minMid = 1.0f`. Now:
```cpp
minMid = std::min (minMid, gMidOut[i]);
```
so `currentGrMidDb_` / `maxGrMidDb_` / the history `grMidDb` populate. The 3-band meter's reserved MID bar and the flat MID history trace come alive automatically — **the built-in visual proof the 3rd band works.** Also extend `gDeepBand = min(gLowOut, gMidOut, gHighOut)` in the total-GR computation.

---

## 5. Per-band SOLO (listen buttons under each band bar)
avishali wants a **solo listen button under each band bar** (LO / MID / HI) in the GR meter, to audition one band in isolation.

**State — transient, NOT a saved/automatable param.** Solo is a *listen* utility: it must **not** persist in presets or recall on load. Implement as **UI-driven atomic flags on the processor**, not APVTS params (keeps zero frozen IDs and no preset pollution):
```cpp
// PluginProcessor.h
std::atomic<bool> bandSolo_[3] { {false}, {false}, {false} };   // [0]=LOW [1]=MID [2]=HIGH
void setBandSolo (int band, bool on) noexcept { bandSolo_[(size_t) band].store (on, std::memory_order_relaxed); }
bool getBandSolo (int band) const noexcept    { return bandSolo_[(size_t) band].load (std::memory_order_relaxed); }
```

**DSP — band reconstruction (apply loop, §1f).** Read the three flags once per block into locals `sLow/sMid/sHigh`. Build a per-band mute mask; if **any** solo is on, only soloed bands pass, else all three:
```cpp
const bool anySolo = sLow || sMid || sHigh;
const float mLow  = (! anySolo || sLow)  ? 1.0f : 0.0f;
const float mMid  = (! anySolo || sMid)  ? 1.0f : 0.0f;
const float mHigh = (! anySolo || sHigh) ? 1.0f : 0.0f;
bandLimitedBuf_[ch][i] = duck * ( linkedGain * (mLow*aLow + mMid*aMid + mHigh*aHigh)
    + oneMinusBl * (mLow*aLow*gainLow[i] + mMid*aMid*gainMid[i] + mHigh*aHigh*gainHigh[i]) );
```
> Soloed audio is the **limited** band (post per-band gain) — you hear exactly what the band limiter does. Wideband + ceiling still run (fine for a listen). RT-safe: masks are branchless scalars, flags are relaxed atomics. Solo does **not** touch the GR taps — every band keeps metering so you can still watch all three while soloing one.

**UI — `GainReductionMeter` (it owns the band column geometry).** Add three small **toggle** buttons, one centred under each band group (LO/MID/HI), in a row below the bars (grow the component's bottom strip; the widened meter from the UI-widen slice gives the room). Reuse the existing toggle look (lit = accent). Click toggles `processor_.setBandSolo(band, on)`; **multiple solos allowed** (they sum). Keep the total `current/max` readout — relocate it if the solo row needs the space.

**Verify (Claude):** soloing LOW outputs only lows; MID only mids; HIGH only highs; two solos sum; releasing all restores the full mix; toggling is click-free; GR meters keep showing all three bands. **Audition (avishali):** each band solos cleanly for listening.

---

## 6. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
# verify installed bundle mtime is fresh before auditioning (stale-binary gotcha)
```
**Acceptance (Claude verifies 1–6; avishali auditions 7):**
1. SDK (if touched) + plugin build clean, no new warnings; AU validates.
2. Reported latency = old + `M2`, exact integer, multiple of osFactor; `dryDelay_`/bypass stay sample-aligned (bypass A/B is click-free and phase-correct).
3. **Transparency null:** Band Link = 0 (linked), input gain such that no band limits → output nulls against the pre-slice linked path to ≤ −100 dBFS (linear-phase reconstruction intact across the tree). Run the offline sweep rig.
4. **Independence:** push gain, Band Link = 100 — LO/MID/HI GR bars + history traces move **independently**; a low tone reduces LO only, a mid tone MID only, a HF sweep HI only.
5. True-peak at ceiling holds (≤ ceiling +0.1 dB) across Band Link 0/50/100 in SP and TP (watch the known Color-100 TP leak — should be no worse than 2-band).
6. Sweep both crossover DEV specs live — kernel rebuild is click-free (duck-swap), no crash on repeated fast edits (the swap-race regression), block-aligned.
7. **Audition:** 3-band de-pumping vs the 2-band build on a dense mix; mid-band control is musically useful; pick default `f_lo`/`f_hi` and the three release scales by ear before any bake.

**Close gate:** finalize ADR-0012; update `docs/SIGNAL_FLOW.md` (§1 latency `M1+M2`, §2.9–2.12 three bands, §4.1 tree, §6 new DEV controls, §8 Color-knob relocation); append `docs/PROGRESS.md`; update `PROMPTS/PLAN.md` (new slice row); commit SDK→plugin (submodule bump only if SDK touched); archive `PROMPTS/SLICE_3BAND_DSP_CLOSE.md`.

---

## Notes for the architect (context, not for Cursor)
- **Why a tree, not a parallel bandpass bank:** the subtraction-trick crossover guarantees exact `sum == delay(x)` reconstruction per stage, so the linked (transparent) mode stays null-clean — a parallel bank would ripple. Two stages compose that invariant cleanly for 3 bands.
- **After tuning:** promote the two crossover frequencies + the chosen link behaviour into a real multiband user control (candidate designs: two draggable crossover handles + a single "Separation"/link macro, Ozone-style band tiles). That is the *next* slice, gated on this one's audition — this is where the "different user control" you asked for gets designed.
- **World-class chase hooks:** with 3 independent bands visible, the measurable levers are per-band release character, the Color-100 TP leak (raise OS on the band stage / the F-3d 8× work), and optional per-band attack. Voice against Pro-L 2 / Ozone with the offline rig + the new per-band viz.
