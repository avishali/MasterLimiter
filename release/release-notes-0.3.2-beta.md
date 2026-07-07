MasterLimiter 0.3.2-beta (build 1)
==================================

A mastering maximizer — TUNING beta. This build adds an **Engine selector** so you
can A/B two limiting engines on your own material and tell us which one wins.
Your job this round: switch between **Transparent** and **Open**, on several sources,
and report which sounds better (and why). Save voicings you like and send the presets.

Install
-------
- macOS, Apple Silicon + Intel (universal). Formats: AU, VST3, AAX (Pro Tools), Standalone.
- Run the installer (signed + notarized). Rescan plugins in your DAW if needed.
- Shows as: MelechDSP -> MasterLimiter.
- Check the plugin header for `main@<git-hash>` — confirms you have this build, not a stale copy.
- ~14 ms latency (lookahead) — your DAW compensates automatically.

What's new in 0.3.2-beta
------------------------
- **DEV -> Engine selector: Transparent vs Open.** Two ways to catch peaks:
  - **Transparent** — clean, controlled baseline limiting. Safe, neutral, "gets loud without getting weird."
  - **Open** — our new 2-band engine. Aims for more loudness and a more "open" macro-dynamic feel
    (the mix breathes more between transients) while still holding the ceiling.
  Switch freely and compare. There is no "right" answer — we want YOUR verdict per source.
- **Consistency + stability fixes** — the selector always matches what you hear (no mislabeled engine
  after loading a preset/session), and a build-time guard prevents UI text glitches.

What carried over
-----------------
- DEV panel embedded in the main editor (works in Pro Tools AAX).
- Attack/release + lookahead DEV controls, History Graph, user presets + A/B compare.
- Clipper 8x oversampling; gesture-commit + duck-and-swap on crossover changes.
- All four formats signed; AAX is PACE-signed for Pro Tools.

60-second mental model
----------------------
Audio in -> Input Gain (drives the limiting) -> the Engine (Transparent or Open)
-> Ceiling (output level) -> peak safety.
To make it work: raise Input Gain until you see gain reduction; set Ceiling to
your target (e.g. -1 dB).

Your mission this round: pick the engine
----------------------------------------
1. Open the History Graph (top bar) so you can SEE the gain reduction.
2. Open the DEV window (top bar). Find **Engine** at the top.
3. Push Input Gain so the limiter does ~3-6 dB on real music.
4. For each source: set a target loudness, then flip **Engine: Transparent <-> Open**
   and listen. Match levels by ear if one is louder.
5. Try several sources (drums, full mix, vocal, bass-heavy, dense electronic).

What we're chasing: maximum loudness that stays CLEAN and natural — no pumping,
no dulled transients, no audible "breathing" on release. Loud AND open = the win.
Tell us where each engine wins or falls apart.

Save the voicings you like — and send them back (IMPORTANT)
----------------------------------------------------------
1. Top-bar Presets menu -> "Save current as..." -> name it (e.g. "Open - punchy drums").
2. Use the A / B buttons to compare two voicings.

Send me the preset files. They live here:
   ~/Library/Audio/Presets/MelechDSP/MasterLimiter/
(Finder -> Go -> Go to Folder -> paste that path.)
Email me the .mlpreset files for the voicings you liked. Each captures the
COMPLETE setting, including which engine.

Feedback that helps most
------------------------
- Which engine did you prefer, per source? Transparent or Open — and why?
- Loud enough? Too aggressive or too soft?
- Any pumping, breathing, or dulled transients? Where?
- vs your usual limiter (Pro-L, Ozone, L2) at matched loudness?
- Anything that sounded wrong (distortion, clicks, weird stereo).

Known beta notes
----------------
- DEV controls are temporary (they'll be baked into simpler controls for release).
- Open can add low-end grit when pushed hard — expected on loud limiting; note if it bothers you.
- It's a beta — save your DAW session often; report any crash with what you were doing.

Thanks — your ears are doing real work here. Send the .mlpreset files + notes.
— avishali / MelechDSP
