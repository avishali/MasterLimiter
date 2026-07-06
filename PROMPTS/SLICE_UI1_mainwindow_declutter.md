# SLICE UI-1 — Main-window de-clutter + clipper LED follows toggle (pre-alpha)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (build + LED behaviour) · **Audition:** avishali
**Repo/scope:** plugin `MasterLimiter`, main window (`Source/ui/MainView.cpp/.h`) + the clipper LED metering. Additive/removal only; no engine/DSP change. **Read `docs/ENGINE_NAMING.md` for naming.**
**Why:** as engines accreted the main window got cluttered/misleading. Remove the two permanently-dead controls and make the clipper LED honest, before Asaf's alpha.

## Change 1 — remove the two PERMANENTLY-dead main-window controls
Per the control inventory, these are greyed placeholders on the main window (their live control moved elsewhere):
- **Color** (`band_color`) — permanently greyed (`MainView.cpp:862-865`, "multiband redesigned; tune via DEV Band Link"). The live control is the DEV panel "Band Split %". **Remove the Color knob + its label from the main window.** (Keep the `band_color` *param* — it's still used by the DEV control; only remove the dead main-window widget.)
- **Character** (`character`) — permanently greyed (`MainView.cpp:868-870`, "DEV Attack overrides it"). **Remove the Character control + label from the main window.** (Keep the `character` param for now; just remove the dead widget.)
- Keep ALL conditional greying (Release when Auto on; Auto-Release-Mode when Auto off; Clipper Drive/Mode/Position when clipper off) — that's correct UX, leave it.

## Change 2 — clipper LED follows the user toggle (black when clipper Off)
Today the LED lights whenever clip GR > 0 (`MainView.cpp:1779`, `processClipLed(..., clipDb > 0.0f, ...)`), so the **Open** engine's forced tip-catch clipper (`PluginProcessor.cpp:1538`, `forceActive=true`) lights it even when the user's clipper toggle is Off.
- **Fix:** gate the LED (and the clipper readout, if it also shows the forced GR) on the **user's `clipper_active`** state: LED lights only when `clipper_active == ON` **and** clipping is occurring; **black whenever `clipper_active == OFF`**, regardless of the engine's internal clip.
- Simplest: in the LED update, use `clipperActiveUser && clipDb > 0.0f` (read the `clipper_active` param state the UI already has). Do NOT change the DSP — the Open engine still clips internally for peak safety; it just won't light the *user* LED. (The full clipper/Ceiling cleanup is CLIP-1, parked after the UI work.)

## Non-goals
- No DSP/engine changes. No param removals (only dead *widgets* removed). No DEV-panel changes (that's UI-2). No CLIP-1.

## Build/verify/audition
- Build clean, AU+VST3, no new warnings.
- (Claude) confirm: main window no longer shows Color/Character; with the Open (MB) engine ON and clipper toggle OFF, the clipper LED is **black**; with clipper toggle ON and clipping, LED lights.
- (avishali) main window is cleaner; LED behaves.

## Output requirements
1. Diff (MainView widget removals + LED gate). 2. Build. 3. Confirm no param/DSP changes. 4. Open questions.
