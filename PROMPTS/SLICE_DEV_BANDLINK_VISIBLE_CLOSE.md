# SLICE CLOSE — DEV Band Split visibility + disambiguate Band Stereo

**Closed:** 2026-07-04 · **Repo:** MasterLimiter (DEV panel UI only)

## What shipped
- `band_color` slider relabeled **Band Split** with clarified multiband link tooltip.
- `dev_band_stereo_link_pct` relabeled **Band Stereo** (was duplicate "Band Link").
- **Band Split** moved from clipped 7th Crossover row into **BAND · Multiband link** group (between per-band trim and **BAND · Stereo link**).

## Files touched
- `Source/ui/DevControlsComponent.h`, `Source/ui/DevControlsComponent.cpp`
- `docs/SIGNAL_FLOW.md` §6, `docs/PROGRESS.md`, `PROMPTS/PLAN.md`

## Acceptance (pending avishali audition)
1. Build clean; auval PASS; installed fresh.
2. DEV panel shows **Band Split** and **Band Stereo** as distinct visible controls.
3. Band Split 0 → 100 separates/collapses per-band GR bars as expected.
4. Release/Peak/Manual groups fully visible; scroll height correct.
