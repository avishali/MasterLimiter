# Cursor Task — Slice 16b: visual restyle (palette, buttons, power button, link icon, LUFS box, knobs)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-only visual restyle.** Implements the approved direction in
> `design/ui_direction_v1.html` (open it — it is the visual target): refined
> clean/dark/teal palette, redesigned buttons with a clear on-state, a **power
> button** for Limiter On/Off, a **horizontal Ozone-style link icon** for the
> Link toggles, segmented toggles, a restyled LUFS box, and refined knob/fader
> rings. No DSP/param/audio change. All vector-drawn (no embedded image
> assets). Branch off the new `main` (after Slice 16 close). **Do NOT push.**

> **Ordering:** assumes Slice 16 has closed and is on `main`. If not, STOP.
> **Expect iteration:** this is visual — implement the base pass, expose key
> constants (radii, glow, gradient stops, sizes), and avishali tunes by eye.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Product UI only: `Source/ui/MasterLimiterLookAndFeel.{cpp,h}`,
  `Source/ui/MainView.{cpp,h}`, `Source/ui/loudness/LoudnessNumericPanel.{cpp,h}`,
  and a new power-button / link-icon component or LookAndFeel path as needed.
- **Do NOT edit the shared HQ `mdsp_ui/Theme.h`** — override the palette
  **product-local** (set this plugin's theme/UiContext values at construction,
  or a product palette the LookAndFeel reads). Find the existing theme-injection
  point and override there.
- No params, no DSP, no HQ source. UI-thread only.
- Branch `slice-16b-visual-restyle` off `main`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. PALETTE (product-local override → the mockup hex)
────────────────────────────────────────

Override the theme values used by the UI to the `design/ui_direction_v1.html`
palette:
- bg deep `#0D1015` · panel `#161B22` · control `#1E242E` (raised top `#232A35`)
- border `#2C333F` · border-strong `#3A4350` · grid `#38414E`
- text `#E8EDF3` · text-muted `#8C97A6`
- accent (teal) `#33D2BE` · accent-bright `#5BE7D6` · accent-dim `#1C7A70`
- warning (warm coral) `#E8704F`

Map these onto the existing theme fields (`background`, `panel`, `accent`,
`text`, `textMuted`, `grid`, `gridMajor`, `border*`, `warning`, …). Pull any
remaining hardcoded inline colours in the LookAndFeel/MainView into the theme.

────────────────────────────────────────
2. BUTTONS — redesign (clear on-state)
────────────────────────────────────────

In `MasterLimiterLookAndFeel` (drawButton / drawToggleButton), match the
mockup: rounded rect **radius ~7 px**, idle = soft gradient (`#232A35`→`#1A2029`)
+ 1 px top highlight + 1 px bottom shadow (not flat) + `#2C333F` border + muted
text; hover = brighter gradient + `#3A4350` border + brighter text; **on/active**
= teal border + inner teal glow + outer soft glow + bright text. Apply to the
plain text/toggle buttons: SP/TP, Bypass, Learn, Auto, RMS, the I/O `L/R`
toggles, the meter range `−/+`, etc.

────────────────────────────────────────
3. POWER BUTTON — Limiter On/Off
────────────────────────────────────────

Replace the Limiter On/Off button (`btnLimiterActive_`) with a **circular power
button** (vector, per the mockup): dark recessed circle + thin rim; a centered
**power glyph** (broken-ring arc + vertical stroke). **OFF** = grey glyph
(`#5A6675`), neutral rim. **ON** = teal glyph (`#5BE7D6`) with a soft glow + a
faint teal rim tint. Implement as a small dedicated drawn component (or a
LookAndFeel button-class path). Keep it bound to `limiter_active`.

────────────────────────────────────────
4. LINK ICON — horizontal Ozone-style
────────────────────────────────────────

Replace the Link toggle buttons with a **horizontal chain-link icon** (two
interlocking horizontal rounded-rect links, per the mockup, NOT rotated):
**OFF** = grey (`#5A6675`); **ON/linked** = teal (`#33D2BE`) + soft glow. Apply
to all link toggles: `gain_ceiling_link` (Gain⇄Ceiling), `io_input_link`,
`io_output_link`. Keep their bindings.

────────────────────────────────────────
5. SEGMENTED TOGGLES
────────────────────────────────────────

Style the Clipper Hard/Soft, Stereo (L/R | M/S), and Auto-release mode controls
as **segmented toggles** matching the mockup: rounded container, divider lines,
selected segment = teal-tinted fill + subtle inner glow + bright text;
unselected = muted text. Keep them compact (per 16.2 sizing).

────────────────────────────────────────
6. LUFS BOX
────────────────────────────────────────

Restyle `LoudnessNumericPanel` (the M / S / I LUFS box) to match: panel
background (`#161B22`/`#1E242E`), `#2C333F` border + ~8 px radius, `#E8EDF3`
values with `#8C97A6` labels, tabular-aligned numbers. Consistent with the I/O
panel's new look.

────────────────────────────────────────
7. KNOBS + FADERS — refined ring (not a full re-draw)
────────────────────────────────────────

Refine `drawRotarySlider` / `drawLinearSlider` to the mockup: recessed knob
face, a **two-tone value arc** (muted track `#283038` + teal `#33D2BE` value
with a soft accent glow), value text in `text`. **Indicator = a SHORT tick near
the outer rim** (aligned to the value-arc position): it must NOT be a
full-radius line through the centre — a long line covers the centre value text.
Keep the tick short (well clear of the centre text) and shorter than a typical
full indicator. Faders: dark
track, gradient fill (dim-teal → teal → warm-coral near top), refined handle.
This is a palette/ring **refinement** — the full bespoke knob re-draw stays a
post-beta backlog item.

────────────────────────────────────────
8. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout -b slice-16b-visual-restyle
cmake --build build-debug -j && cmake --build build-release -j
```
- Zero new `Source/` warnings. UI-only → Slice 3/4/5 unchanged (optional).
- In a host, compare against `design/ui_direction_v1.html`: palette matches,
  buttons have the new idle/hover/on states, the Limiter power button glows
  teal when on, Link toggles are horizontal chain icons (teal when linked),
  segmented toggles styled, LUFS box matches, knob/fader rings refined.

────────────────────────────────────────
9. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log (theme-injection point; control members).
2. Diffs per section; list the tunable constants you exposed (radii, glow
   strength, gradient stops, power-glyph/link sizes).
3. Build summary (Debug + Release, warnings).
4. Confirmation no HQ `Theme.h` edit (palette is product-local override).
5. `git status --short` (slice-16b branch, no commit).
6. Open questions / where it diverges from the mockup and why.

────────────────────────────────────────
10. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali compares in-host to the mockup and tunes by eye (colours, glow,
radius, power/link sizing). Iterate, then close Slice 16b. Next: Slice 17
(packaging: smoothing/default audit, presets, version stamp), manual, beta.
**Do not self-close.**
