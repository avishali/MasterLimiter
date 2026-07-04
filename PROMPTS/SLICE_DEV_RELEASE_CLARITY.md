# SLICE — DEV release controls: clearer labels/tooltips, High/Wide split, engine-aware greying

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali
**Repos:** plugin `MasterLimiter` only. No SDK. DEV controls + one new DEV param + a tiny processor decouple. No audio-path/latency change; no frozen-ID change.
**Why:** the DEV release controls are hard to read (what the `×` trims, which controls belong to which engine). Make them legible so avishali/Asaf tune the right knobs.

---

## 1. Relabels + tooltips (`DevControlsComponent.cpp` setup)
Update labels/tooltips (keep the APVTS attachments/IDs unchanged unless noted):

| Control | New label | New tooltip |
|---|---|---|
| `dev_low_release_scale` | **Low ×** | "Low-band release = this × the base release. >1 slower (bass, less pump), <1 faster. Affects Auto + Manual." |
| `dev_mid_release_scale` | **Mid ×** | "Mid-band release trim (× the base release)." |
| `dev_high_wide_release_scale` | **High ×** | "High-band release trim (× the base release)." *(now drives the HIGH band only — see §2)* |
| *(new)* `dev_wide_release_scale` | **Wide ×** | "Wideband final-stage release trim (× the base release)." |
| `dev_release_engine` (combo) | **Auto Engine** | "Auto-release algorithm. Lookahead = recovers only in real gaps seen in the lookahead window (smooth, program-dependent, current best). Adaptive = legacy sigma tracker (A/B only)." |
| `dev_la_release_ms` | **Release (ms)** | "Lookahead engine: recovery time (how fast gain lets go in a gap). Per-band × trims multiply this." |
| `dev_la_release_poles` | **Smoothness** | "Lookahead engine: recovery-curve order (2–4). More = rounder S-curve, same speed." |
| `dev_sigma_attack_ms` | **Adapt Onset (ms)** | "Adaptive (legacy): how fast it decides limiting is sustained → switches to slow release. Lower = reacts sooner." |
| `dev_sigma_decay_scale` | **Adapt Hold ×** | "Adaptive (legacy): how long it stays in slow-release after limiting stops." |
| `release_sustain_ratio` | **Manual Sustain** | "Manual release only (Auto OFF): fast+slow split. Higher = more sustain held." |

Group headers: rename the two auto sections to **"RELEASE · Auto (Lookahead)"** and **"RELEASE · Auto (Adaptive · legacy)"**, and the band-scaling section to **"RELEASE · per-band trim (× base)"**.

## 2. Split High/Wide → independent Wide scale
Today `dev_high_wide_release_scale` feeds **both** the high-band envelopes (`envelopeHigh_/HighR_`) **and** the wideband envelopes (`envelope_/envelope_R_`) via `configureEnvelope(..., highWideScale)`.
- **Add** DEV param `dev_wide_release_scale` (`ParameterIDs.h`, `Parameters.cpp`): Float `0.5 – 8.0`, step `0.01`, default **1.0**, display "DEV Wide Release Scale". Cache `devWideReleaseScale_` raw pointer + jassert.
- **Decouple in `processCore`**: `configureEnvelope(envelopeHigh_, …, highScale)` and `envelopeHighR_` use `dev_high_wide_release_scale` (now HIGH only); `configureEnvelope(envelope_, …, wideScale)` and `envelope_R_` use the new `dev_wide_release_scale`. (The ID `dev_high_wide_release_scale` keeps its name — it's a temporary DEV param; renaming would break DEV-preset recall. It now means "high only".)
- UI: add the **Wide ×** slider + label + `SliderAttachment` to `dev_wide_release_scale`, placed right after **High ×** in the per-band-trim section; extend `resized()`/`placeSliderRow` for the extra row.

## 3. Engine-aware greying (the requested behaviour)
When **Auto Engine = Lookahead**, the Adaptive-only controls are irrelevant, and vice-versa. Grey out the ones that don't apply so nobody tunes a dead knob.
- Add `void updateReleaseEngineEnablement();` — reads the current `dev_release_engine` index (0 = Adaptive, 1 = Lookahead per the combo item order; confirm against `Parameters.cpp`):
  - **Lookahead:** `setEnabled(true)` on `sldLaRelease_`/`lblLaRelease_`, `cmbLaPoles_`/`lblLaPoles_`; `setEnabled(false)` on `sldSigmaAttack_`/`lblSigmaAttack_`, `sldSigmaDecay_`/`lblSigmaDecay_`.
  - **Adaptive:** the reverse.
  - Band-trim (Low/Mid/High/Wide ×) and Manual Sustain are **left as-is** (they apply to both engines / are gated by Auto elsewhere).
- Call it: (a) at the end of the constructor (initial state), and (b) from `cmbReleaseEngine_.onChange` (append to any existing lambda so the APVTS attachment still fires first). For robustness against preset/automation changes, also call it from the existing 30 Hz DEV sync (`syncDevReadouts`) — cheap idempotent enable/disable.
- Greyed = `setEnabled(false)` (standard JUCE dim). Ensure disabled sliders don't accept mouse.

---

## 4. Build, verify, close
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8 && auval -v aufx MaLm Melc 2>&1 | tail -5
./scripts/install_user.sh build 2>&1 | tail -3
```
**Acceptance (Claude verifies 1–4; avishali auditions 5):**
1. Build clean; auval PASS; installed fresh. No latency/audio change at default (Low 1.52 / Mid 1 / High 1 / **Wide 1** reproduces the prior High/Wide=1 behavior — verify default is unchanged sound).
2. Labels/tooltips read as the table; section headers updated.
3. **High × and Wide × are independent** — moving Wide × changes only the wideband final-stage recovery; High × changes only the high band.
4. **Engine greying:** switch Auto Engine → Lookahead greys Adapt Onset/Hold and enables Release(ms)/Smoothness; → Adaptive reverses. Holds across preset/automation changes to `dev_release_engine`.
5. **Audition:** the controls are legible; you're never tuning a knob that does nothing for the selected engine.

**Close gate:** update `docs/SIGNAL_FLOW.md` §6 (renamed DEV controls + new Wide scale + engine greying), `docs/PROGRESS.md`, `PROMPTS/PLAN.md`; commit plugin-only; archive CLOSE.

## Note
This is DEV-panel clarity only — it changes nothing users see (Color still greyed; DEV panel is Asaf's tuning surface). The default-value equivalence check in §1 is the one to watch (splitting High/Wide must not change the default voicing).
