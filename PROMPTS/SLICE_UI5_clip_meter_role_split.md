# SLICE UI-5 — the Ceiling's clipping is reported on the Drive meter; plus the Ceiling-OFF warning

**Status:** ready for Cursor · **Architect:** Claude · **Reported by:** avishali · **Verify:** Claude
**Scope:** metering + UI. **No audio-path change.** No SDK.

## BUG — Drive LED flashes when Drive is doing nothing

> avishali: *"when clipper and ceiling are both on, LED is flashing even if the clipper is at 0.0. if i
> move the ceil release 'clip ms' up then it stops flashing. input is well below 0 dBFS."*

**Verified in code.** `runClipperStage` (`PluginProcessor.cpp` ~L1608) serves **two roles** and takes a
`bool ceilingClipRole` to tell them apart. It uses that flag correctly for the *audio* — threshold, Hard/Soft
mode, drive gain, the every-other-sample path. But the **metering publish never checks it** (~L1680):

```cpp
const float clipReadDb = -maxAttenuationDb;
currentClipDb_.store (clipReadDb, std::memory_order_relaxed);   // no ceilingClipRole check
if (clipReadDb > maxClipSinceResetDb_.load (...)) maxClipSinceResetDb_.store (clipReadDb, ...);
```

So when **Ceiling runs in Clip mode**, its tip-catch attenuation is written into the meter that drives the
**Drive LED and the "DRIVE GR" readout**. `clipEnvBuf_` (the ballistics feed) is shared the same way.

Every detail of the report follows:
| observation | cause |
|---|---|
| LED flashes with Drive at 0.0 dB | Drive is inert; the **Ceiling** is clipping and reporting on Drive's meter |
| input well below 0 dBFS | irrelevant — Ceiling clips at `ceiling_db` (−1.0) *after* input gain |
| moving Ceiling Release up stops it | Ceiling becomes a limiter, so there is no clip depth to publish |

**This is metering only — the audio is correct.** Ceiling holds −1.00 in every measurement and Drive is
genuinely inert at 0.0 dB. It is a leftover from CLIP-1: that slice split one clipper into two roles and
split the audio path correctly, but not the meter. CLIP-2 then gave them separate oversamplers and still
did not separate the metering.

### Fix
Route the publish by role. `currentFinalCeilingDb_` / `maxFinalCeilingDb_` already exist and already feed
the Ceiling readout:
- `ceilingClipRole == true` → publish to the **Ceiling** meters.
- `ceilingClipRole == false` → publish to `currentClipDb_` / `maxClipSinceResetDb_` as today.
- **`clipEnvBuf_` needs the same split**, or the ballistics keep feeding the Drive LED.
- ⚠️ Do not double-count: the Ceiling already publishes `fcGrDb` from the FinalCeiling path (~L1945, ~L2703).
  In Clip mode that path does not run, so decide which one owns the readout in each mode and **state it**.

### Design call (architect's recommendation — avishali can overrule)
**The Ceiling's clip activity should show as a number in the Ceiling readout, not as a flashing LED.**
In Clip mode, clipping IS the intended peak-safety mechanism working normally — an indicator that is lit
during ordinary use carries no information. Keep the flashing LED for **Drive**, which is a deliberate
user-added colour and where a "you are driving it" light is genuinely useful.

## SECOND ITEM — Ceiling-OFF warning (carried over from UI-4)
`ceiling_active` OFF disables peak safety and the plugin will exceed its output level. The state is
currently invisible at a glance — one dark power button. Tint the **Output Level readout** (or the Ceiling
label) in a warning colour whenever Ceiling is OFF. Keep the existing tooltip.

## Gate
- [ ] Drive at 0.0 dB + Ceiling = Clip ⇒ **Drive LED dark, DRIVE GR reads 0.0**, on real program material.
- [ ] Drive ON with real drive ⇒ Drive LED and DRIVE GR respond to **Drive only**, and are unchanged by
      moving Ceiling Release between Clip and limiter values.
- [ ] Ceiling readout shows the clip depth in Clip mode, and the FinalCeiling depth in limiter mode, with
      no double-counting.
- [ ] Ceiling OFF is visually obvious without hovering.
- [ ] **`mbl_calibrate.py` 58/59, latency 3003, sPk −1.00, and the group-Z Drive discontinuity check still
      passes** — a metering change must not move a single audio measurement.
- [ ] Build clean (ASCII gate), AU + VST3, both installed, mtimes.

## Non-goals
- No audio-path change of any kind. No new parameters. No SDK edits.

## Output requirements
1. Retrieval log. 2. Diffs. 3. Which meter owns the Ceiling readout in each mode. 4. Full `mbl_calibrate.py`.
5. Build + install mtimes for BOTH formats.
