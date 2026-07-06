# SLICE UI-2 — DEV panel restructure: Engine selector + per-engine frames (pre-alpha)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (build + selector behaviour) · **Audition:** avishali
**Repo/scope:** plugin `MasterLimiter`, DEV panel (`Source/ui/DevControlsComponent.cpp/.h`) + minimal processor wiring for the selector. No DSP change. **Read `docs/ENGINE_NAMING.md` — use product names Transparent / Open.**
**Why:** the DEV panel shows every engine's controls at once (contradictory states, cluttered). Restructure so the tester picks ONE engine and sees only its controls.

> ⚠️ **Retrieval log first.** Read `DevControlsComponent.cpp/.h` (the current groups + `updateReleaseEngineEnablement`), and how `dev_mb_engine` / `dev_release_engine` are set. Output the group layout.

## Target layout (top → bottom)
```
[ GLOBAL (DEV) — always shown, grouped by topic ]
   Peak:  Final Ceiling (dev_final_ceiling) · FC Release (dev_final_ceiling_release_ms) · M/S Safety Clamp (dev_ms_safety_clamp)
   (other DEV globals if any; most globals live on the main window — leave those)

[ ENGINE selector ]   Transparent | Open      (segmented buttons or ComboBox)

[ ENGINE FRAME — swaps entirely; only the active engine's controls visible ]
```

## The Engine selector (2 engines for alpha)
- Values: **Transparent**, **Open** (per glossary). **Do NOT show Smart or Adaptive in the alpha selector** (Smart is the next dev target — keep its params + code, just hide the option/frame for now; Adaptive is legacy).
- Selecting drives the underlying switches:
  - **Transparent** → `dev_mb_engine = OFF`, `dev_release_engine = Lookahead`.
  - **Open** → `dev_mb_engine = ON`.
- The selector must reflect current state (Open if `dev_mb_engine` on, else Transparent).
- **Recommended:** make the selector the single source of truth via a new `dev_engine` choice param (Transparent/Open) that the processor reads to derive the internal switches — cleaner for presets/future engines (Smart/Spectral). If that's too invasive for one slice, a UI-derived selector that reads/writes the existing params is acceptable; flag which you chose.

## Per-engine frames (show only the active engine's controls; hide the rest)
**Transparent frame** (inline 3-band, Lookahead release):
- Attack: `dev_attack_mode`, `dev_attack_ms`, `dev_real_attack_ms` (keep the mode-based greying)
- Per-band attack: `dev_low_band_attack_scale`, `dev_mid_band_attack_scale`, `dev_high_band_attack_scale`
- Lookahead: `dev_lookahead_band_ms`, `dev_lookahead_wide_ms`
- Crossovers: `dev_xover_cutoff_hz/transition_hz/atten_db`, `dev_xover_hi_cutoff_hz/transition_hz/atten_db`
- Band: `band_color` (Band Split %), `dev_band_stereo_link_pct`, `dev_band_ms`, `dev_band_ms_link_pct`
- Release (Lookahead): `dev_la_release_ms`, `dev_la_release_poles`
- Per-band release: `dev_low/mid/high_band_release_scale`, `dev_wide_release_scale`

**Open frame** (2-band MultibandLimiter):
- `dev_mb_crossover_hz`, `dev_mb_attack_mode`, `dev_mb_attack_ms`, `dev_mb_release_ms`, `dev_mb_safety`, `dev_mb_lookahead_ms`

**Hidden for alpha (parked, keep params + code):** all `dev_smart_*` and `dev_sigma_*` groups, and the Adaptive/Smart options of `dev_release_engine`. We re-expose Smart when we build it (after alpha voicing).

## Notes
- **Stale tooltip cleanup (from UI-1):** the DEV "Band Split %" (`band_color`) tooltip still references the now-removed main-window Color knob ("Main Color knob is greyed"). Update it — Band Split is now the sole control for band link.
- Global user controls on the MAIN window (input/output gain, ceiling, stereo, release/auto, gain match) stay where they are — don't move them.
- Keep the panel scrollable; group headers by topic; sensible order within each engine frame.
- If this is too big for one reviewable diff, split: **UI-2a** = engine selector + show/hide the two frames; **UI-2b** = regroup the globals + reorder within frames. Note the split if you do it.

## Build/verify/audition
- Build clean, AU+VST3. Selector switches engines; only the active engine's controls show; global + Peak controls always visible; Smart/Adaptive hidden.
- Toggling Transparent↔Open sets the right underlying params and the right controls appear; no dead/duplicated controls visible.
- (avishali) DEV panel is clear: pick engine → see only its controls.

## Output requirements
1. Retrieval log. 2. Diffs (selector + frame show/hide + any param). 3. Which selector approach (new `dev_engine` param vs UI-derived). 4. Build. 5. Confirm Smart/Adaptive params still exist (just hidden). 6. Open questions.

## Notes for the architect
- After UI-1+UI-2 land + avishali OKs → this is the alpha UI. Then: CLIP-1 (Ceiling limiter→clipper unification, parked), and **build the Smart engine** (re-expose it in the selector as a 3rd option per the glossary).
- Docs consistency pass (Transparent/Open across SIGNAL_FLOW/SPECTRAL_ENGINE_DESIGN/MANUAL) can ride the close gate — architect will handle.
