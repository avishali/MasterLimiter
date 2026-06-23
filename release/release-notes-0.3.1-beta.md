MasterLimiter 0.3.1-beta (build 3)
==================================

A mastering maximizer — early TUNING beta. This build exposes extra "DEV"
controls so you can help voice the attack/release and the linear-phase crossover.
Your job: find voicings you like, SAVE them as presets, and email the preset files back.

Install
-------
- macOS, Apple Silicon + Intel (universal). Formats: AU, VST3, AAX (Pro Tools), Standalone.
- Run the installer (signed + notarized). Rescan plugins in your DAW if needed.
- Shows as: MelechDSP -> MasterLimiter.
- Check the plugin header for `main@<git-hash>` — confirms you have this build, not a stale copy.
- ~14 ms latency (lookahead) — your DAW compensates automatically.

What's new in build 3
---------------------
- **Linear-phase crossover DEV controls** (Cutoff / Transition / Atten) — drag cleanly, no crash.
- **Clipper 8× oversampling** — cleaner hard-clip on HF material.
- **Gesture-commit + duck-and-swap** on crossover changes — brief dip on release, no click per step.
- DEV panel embedded in the main editor (works in Pro Tools AAX).
- All four formats signed; AAX is PACE-signed for Pro Tools.

What's still in this beta (from build 2)
----------------------------------------
- Lookahead-follower release — selectable in the DEV window.
- History Graph — gain-reduction over time while you tune.
- DEV tuning — Attack, Lookahead (Band/Wide), Release (Time/Poles), Crossover.
- User presets + A/B compare.

60-second mental model
----------------------
Audio in -> optional Clipper -> Input Gain (drives the limiting) -> 2-band
limiter -> Ceiling (output level) -> true-peak safety. The Color knob morphs
from transparent (0%) to punchy multiband (100%).
To make it work: raise Input Gain until you see gain reduction; set Ceiling to
your target (e.g. -1 dB).

Your mission: voice the attack & release
----------------------------------------
1. Open the History Graph (top bar) so you can SEE the gain reduction.
2. Open the DEV window (top bar). In RELEASE - Engine, set Engine = Lookahead.
3. Push Input Gain so the limiter does ~3-6 dB on real music.
4. Sweep these while listening AND watching the GR trace:
   - DEV -> ATTACK -> Mode: "Ramp" (transparent, capped by lookahead) or "Real" (decoupled time-constant).
     In "Real" mode use the Real Attack knob: slow = transients PUNCH through (punchy), fast = tight/controlled.
   - DEV -> ATTACK (Ramp mode knob): how gently the gain eases into the peak (subtle smoothness).
   - DEV -> LOOKAHEAD -> Band / Wide (0-6 ms): how far ahead it sees.
   - DEV -> RELEASE - Lookahead -> Time + Poles: release speed (Time) and smoothness (Poles 2/3/4).
5. Try several sources (drums, full mix, vocal, bass-heavy).

What we're chasing: maximum loudness that stays CLEAN and natural — no pumping,
no dulled transients, no audible "breathing" on release. Loud AND transparent = the win.
(The Adaptive-engine, band-scaling, and sustain controls are secondary — ignore unless curious.
Character is intentionally inactive while attack is tuned via the DEV knob.)

Save the voicings you like — and send them back (IMPORTANT)
----------------------------------------------------------
When you find a setting you like:
1. Top-bar Presets menu -> "Save current as..." -> name it (e.g. "Punchy drums").
2. Use the A / B buttons to compare two voicings (set one, hit A->B to copy, tweak, flip A/B).

Send me the preset files. They live here:
   ~/Library/Audio/Presets/MelechDSP/MasterLimiter/
(Finder -> Go -> Go to Folder -> paste that path.)
Email me the .mlpreset files for the voicings you liked. Each captures the
COMPLETE setting, so I get exactly what you heard.

Feedback that helps most
------------------------
With each preset, a few words:
- Source/genre it was for.
- Loud enough? Too aggressive or too soft?
- Any pumping, breathing, or dulled transients? Where?
- vs your usual limiter (Pro-L, Ozone, L2) at matched loudness?
- Anything that sounded wrong (distortion, clicks, weird stereo).

Known beta notes
----------------
- DEV controls are temporary (they'll be baked into simpler controls for release).
- At intermediate Color (between 0 and 100%) the low end can shift slightly — known, being reworked. Endpoints (0/100) are clean.
- It's a beta — save your DAW session often; report any crash with what you were doing.

Thanks — your ears are doing real work here. Send the .mlpreset files + notes.
— avishali / MelechDSP
