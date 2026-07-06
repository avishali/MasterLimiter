# SLICE UI-3 — fix: non-ASCII UI glyphs + DEV group overlap (pre-alpha bugs)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (ASCII check + install) · **Audition:** avishali (visual)
**Repo/scope:** plugin `MasterLimiter` UI (`Source/ui/MainView.cpp/.h`, `Source/ui/DevControlsComponent.cpp`). UI-only. No DSP/param change.
**Why:** avishali on the fresh UI-1/UI-2 build: (1) a garbled character shows (non-ASCII glyph), (2) DEV engine controls overlap.

## Fix 1 — non-ASCII UI glyphs (garbled character)
`scripts/check_ui_ascii.sh` fails on a raw non-ASCII glyph. Make all VISIBLE UI strings ASCII:
- **`MainView.cpp:789`** — tooltip contains a raw **em-dash `—`**. Replace with ASCII (`-` or ` - `). (This is the check-flagged violation.)
- **A→B / B→A compare buttons** (`MainView.h:159`, `MainView.cpp:1088-1089`) — currently `fromUTF8(u8"A→B")`. The `→` may not render in the plugin font (garbles). Replace with ASCII, e.g. **`"A>B"` / `"B>A"`** (or `"A-B"`/label as you prefer) — keep it ASCII.
- Grep the whole `Source/ui/` for any other non-ASCII in visible strings (`grep -rnP "[^\x00-\x7F]"`) and ASCII-ify. Intentional glyphs that MUST stay (none expected here) use `fromUTF8` AND must actually render — prefer ASCII for buttons/labels/tooltips.
- **Gate:** `bash scripts/check_ui_ascii.sh` passes clean.

## Fix 2 — DEV group overlap (heights too tight after UI-2)
In `DevControlsComponent::resized()`, groups are placed by `placeGroupIfVisible(group, h)` with **hardcoded `h`**. Several `h` no longer match their row count, so rows spill past the box into the next group. The group needs, for N content rows: **`h >= 44 + N*28 + (N-1)*8`** (inner is `reduced(16,22)` → 44 vertical; rows are `rowH=28` with `8` gaps).
- **Robust fix (preferred):** compute `h` from the row count each group actually lays out (pass the row count, or a small helper `heightForRows(n)`), so it can never mismatch again.
- **Or** correct the hardcoded values. Known-tight ones to fix (audit ALL, these are examples):
  - `groupPeakControl_` (3 rows: MS clamp, Final Ceiling, FC Release): `136` → **≥ 144**.
  - `groupMbEngine_` (6 rows): `252` is exactly at the limit → give margin (**≥ 260**).
  - Verify every group: `groupAttack_`, `groupCrossover_` (248 for 6 rows = needs 260), `groupBandScaling_` (172 for 4 rows = needs 188), etc. — several look under.
- After: no group's rows touch/overlap the next group's box, in BOTH engine states (Transparent + Open). `content_.setSize(contentW, y+4)` already tracks total height for scrolling — keep it.

## Non-goals
- No DSP/param/engine changes; no layout redesign (just fit the content). Don't touch UI-2.1 (separate).

## Build / verify / audition
- Build clean; `check_ui_ascii.sh` passes; **install both formats + verify mtime** (recurring miss).
- (Claude) run `check_ui_ascii.sh` (clean) + confirm no non-ASCII in `Source/ui`.
- (avishali) no garbled characters; DEV panel groups don't overlap in either engine.

## Output requirements
1. Diff (ASCII fixes + group-height fix). 2. `check_ui_ascii.sh` output (pass). 3. Build + install mtimes. 4. Confirm no DSP/param change. 5. Open questions.
