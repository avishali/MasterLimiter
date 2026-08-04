# SLICE STATE-1 — deterministic state restore + a Reset-to-Default button

**Status:** ready for Cursor · **PRIORITY: high** · **Architect:** Claude · **Reported by:** avishali
**Scope:** plugin. No SDK. No engine/voicing DSP change.

## Report
> *"that happens once then after moving controls they are resolved."*
> *"clipper set to on, move the knob a bit and it immediately starts to clip — LED starts flashing."*
> *"meter readout overshoot above 0 dBFS, then after a while and some moving of knobs it settles to the ceiling."*
> *"i am not sure ... it behaves differently now. **so maybe it happens when i open a DAW project and the
> plugin is already loaded from previous session.**"*
> *"i think we should have a reset button to recall the default state (which needs to be defined by us now)"*

## The signature, and why the rig missed it
Every symptom is **wrong at first, correct after any control moves**. That is DSP state configured only on
a *parameter change*, never on *restore*. `setStateInformation` is a different path from a live edit, and
avishali's own observation (session reload) is the strongest lead.

**My offline rig cannot reproduce it**: it always sets parameters *before* the first render, so it always
takes the configure path. Group K's preset round-trip passes for the same reason. Do not treat the green
suite as evidence here.

Suspect areas (report what you actually find — do not assume):
- `setStateInformation` → does it mark `updateMbEngineRuntimeConfig`'s watch state dirty, or only change
  parameter values? The MB engine keeps its previous config if nothing marks it stale.
- The **UI-2.1a async guard** (`transparentGuardDirty_` + `triggerAsyncUpdate`): on session load the
  message thread may not have run `handleAsyncUpdate` before audio starts — a window of mis-configuration
  that resolves the moment anything else touches the graph.
- `prepareToPlay` ordering vs restore: which runs first in each host, and does the engine get configured
  from the *restored* values or from whatever it held?
- Meter/ballistics state (`METERING_ACCURACY_AUDIT.md`): a readout above 0 dBFS that later settles to the
  ceiling smells like a max-hold seeded from an uninitialised or pre-restore value.

## Part A — make restore deterministic
**After `setStateInformation`, the DSP must be fully configured from the restored values before the first
`processBlock`** — same end state as if every parameter had just been moved by hand. Force the runtime
reconfigure rather than relying on change detection.
⚠️ RT-safety: keep the existing async pattern — `setValueNotifyingHost` only from `handleAsyncUpdate` /
`setStateInformation`, never the audio thread (UI-2.1a).

## Part B — Reset to Default (avishali's ask)
A header-bar **Reset** that returns every parameter to the defined default state, and forces the same full
reconfigure as Part A. This is both a usability feature and the escape hatch for exactly this class of bug.

### Proposed default state — **avishali to confirm; two entries are deliberate changes**
| param | default | note |
|---|---|---|
| `limiter_active` | On | |
| `dev_mb_engine` | **Transparent** (off) | measured 2026-08-03: Transparent is far more faithful to source (−11.7 vs Open −3.4); Open is the loud/open choice, not the safe one |
| `dev_mb_release_engine` | Smart | avishali's ear test |
| `ceiling_db` | **−1.0** | ⚠️ **CHANGE — it is currently 0.0.** A mastering limiter defaulting to a 0 dBFS ceiling is not a safe default; −1.0 is the convention and every measurement we have used it |
| `ceiling_mode` | TruePeak | |
| `ceiling_release_ms` | Clip | Clip vs 20 ms limiter measured only 0.1–0.4 dB apart on fidelity, so this is not the distortion source |
| `ceiling_active` | On | never ship with peak safety off |
| `drive_active` / `drive_db` | Off / 0 | |
| `a_b_match` | Off | beta instrumentation |
| `input_gain_db` | 0 | |
| `auto_track` (Gain Match) | Off | |
| `stereo_mode` / `stereo_link` | Stereo / 100% | now that LINK-1 made it functional |

## Gate
- [ ] **Restore determinism:** configure a distinctive state → `getStateInformation` → load into a FRESH
      instance → **first render is bit-identical** to the original instance's render (≤ −140 dB). Then
      repeat *without* touching any control afterwards — that is the case that fails today.
- [ ] **Meters:** no readout above 0 dBFS on a restored session at any point, including the first second.
- [ ] **Reset** returns every parameter to the table above and the audio matches a freshly-instantiated
      plugin bit-for-bit.
- [ ] `mbl_calibrate.py` 58/59; latency 3003; ceiling −1.00.
- [ ] Build clean, AU + VST3, both installed, mtimes.

## Non-goals
- No engine/voicing change. No SDK edits. Not the A/B match.

## Note
**The close gate is a DAW check by avishali**: save a project with a pushed setting, reopen it, and press
play *without touching anything*. Offline verification is structurally blind to this — same limitation that
let the A/B match integrator ship.
