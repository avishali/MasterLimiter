# Cursor handoff — default preset + prune presets + compact/smaller default UI

**Repo:** `MasterLimiter`, `main` @ `ed320d4`. Build a **DEV-enabled** (`PLUGIN_DEV_MODE=ON`) test
build for avishali to audition; do NOT rebuild the official AAX beta yet (that's the next step,
`RELEASE_aax_devbeta_handoff.md`, once this is approved).

Four changes:

## 1. Make avishali's saved settings the fresh-instance DEFAULT
Source of truth (full state incl. DEV params):
`/Users/avishaylidani/Library/Audio/Presets/MelechDSP/MasterLimiter/Default.mlpreset`

Every fresh plugin instance must open on these exact values (clean float-noise like
`-5.36e-7` → `0.0`). Notable non-obvious ones: `clipper_active=0` (OFF), `release_auto=1`,
`auto_release_mode=2`, `release_sustain_ratio=4`, `dev_release_engine=1` (Lookahead),
`dev_attack_mode=1` (Real), `dev_xover_cutoff_hz=100`, `dev_xover_transition_hz=120`,
`dev_xover_atten_db=60`, `dev_la_release_ms=8`, `dev_low_band_release_scale=1.52`,
`dev_sigma_attack_ms=17.4`, `band_color=0`.

Preferred mechanism: update the **APVTS parameter defaults** in `Source/parameters/Parameters.cpp`
to match the preset (this is the native "fresh instance" state and covers the DEV params, which
factory presets do NOT). Verify: a brand-new instance (no saved session) opens exactly on these
values, DEV panel included; a DAW-restored session still restores its own saved state
(`setStateInformation` must still win).

## 2. Prune the old factory presets
`Source/ui/PresetManager.cpp` `kFactoryPresets` currently has 5 (Default, Transparent Master,
Loud & Aggressive, Gentle Glue, Clipper Punch). Per avishali the old ones are stale/irrelevant —
**remove all except "Default"**, and update that "Default" entry's main-control fields to match the
preset above (its struct only covers main controls, not DEV — that's fine; the param-defaults in
#1 carry DEV). Update `MainView` preset-menu code so it still works with a single factory preset
(`activeFactoryPresetId_` clamping, the "Factory" section heading, etc. — no out-of-range).

## 3. Default window size = smallest
`Source/PluginEditor.cpp:89` `setSize(1100, 620)` → open at the **smallest** comfortable size
instead of full. The UI scales with a locked aspect ratio (`constrainer_.setFixedAspectRatio`,
`mainView.setTransform(scale)`), so "smallest" = lowest scale that still renders cleanly. Set the
default to the min (or near-min) of the resize range. Coordinate the exact value with avishali by
eye — start compact and let him nudge.

## 4. Shave vertical height (move Gain Match up)
avishali sees wasted vertical space near the bottom. The Gain Match section is the lowest block
(`MainView.cpp:1619-1621`: button `y=568`, label `y=546`, note `y=568`); content extent ~y=580 in
a 620-tall design.
- Move the Gain Match section **up** to reclaim the gap above it (and any empty band below the
  controls), then **reduce `kDesignHeight`** so the window is genuinely shorter (not just empty
  space removed).
- Update the aspect-ratio constrainer to the new `kDesignWidth/kDesignHeight` and the default
  `setSize` (#3) to the new shorter design.
- This is visual/iterative — make a first pass (e.g. pull Gain Match up ~30–50 px and trim
  `kDesignHeight` to match), build, and iterate with avishali on the exact amount.

## Build + verify (post-stale-binary protocol — do NOT skip)
- Build **DEV-ON** Release, install USER-ONLY via `scripts/install_user.sh`, bust AU cache.
- Verify installed mtime current + header shows `main@<HEAD>` (anti-stale). Rescan note for avishali.

## Report back
- New instance opens on the Default values (list a few to confirm, incl. DEV xover 100/120/60).
- Only "Default" remains in the factory menu; no crash selecting it.
- New default window size + new design height; a before/after of the vertical layout.
- Commit: `feat: ship Default preset + prune factory presets + compact default UI`.
- Do NOT push (plugin main already on origin; push these after avishali approves). SDK stays held.

## avishali audition
Open a fresh instance: confirm it starts dialed-in (incl. DEV), opens small, and the vertical
layout feels right (Gain Match no longer floating with dead space). If good → rebuild the AAX
DEV beta (`RELEASE_aax_devbeta_handoff.md`) carrying these changes.
