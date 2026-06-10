# MasterLimiter — Beta Tester Guide (v0.3.0)

Thanks for testing! This is an early beta of **MasterLimiter**, a mastering maximizer. You're getting a special **tuning build** with extra "DEV" controls exposed — your job is to help dial in how it *sounds*, especially the **attack/release voicing**, and send back the settings you like.

---

## 1. Install
- macOS, **Apple Silicon + Intel** (universal). Formats: **AU, VST3, AAX** (Pro Tools).
- Run the installer. Rescan plugins in your DAW if it doesn't appear.
- It will show as **MelechDSP → MasterLimiter**.
- ~14 ms of latency (lookahead) — your DAW compensates automatically. Don't worry about it.

## 2. The 60-second mental model
Audio in → optional **Clipper** → **Input Gain** (this is what drives the limiting) → 2-band limiter → **Ceiling** (output level) → true-peak safety. The **Color** knob morphs from transparent (0%) to punchy multiband (100%). That's it.

To make it work hard: **raise Input Gain** until you see gain reduction; set **Ceiling** to your target output (e.g. −1 dB).

## 3. Your mission: voice the attack & release
Open the **History Graph** (button in the top bar) — it shows the gain-reduction over time so you can *see* what you're hearing. Then open the **DEV** window (button in the top bar) — this is the tuning rig, grouped by section.

**Recommended starting workflow:**
1. In **DEV → RELEASE · Engine**, set **Engine = Lookahead** (this is the new release we're voicing — it should sound smoother than Adaptive).
2. Push **Input Gain** so the limiter is doing ~3–6 dB of work on real music.
3. Sweep these while listening *and* watching the GR trace:
   - **DEV → ATTACK** — how fast transients are caught. Short = punchier/more aggressive, longer = softer. (It's capped by the Lookahead setting below.)
   - **DEV → LOOKAHEAD → Band / Wide** (0–6 ms) — how far ahead it "sees." Affects how cleanly the initial transient is caught.
   - **DEV → RELEASE · Lookahead → Time + Poles** — the release smoothness/speed. Time = how fast it recovers; Poles = how smooth (2/3/4).
4. Try a few musical sources (drums, full mix, vocal, bass-heavy track).

*(The Adaptive-engine and band-scaling controls in the DEV window are secondary — feel free to ignore them unless you're curious. Character is currently inactive while we tune attack directly.)*

**What we're chasing:** maximum loudness that stays **clean and natural** — no pumping, no dulled transients, no audible "breathing" as it releases. If you can get it loud *and* transparent, that's the win.

## 4. Save the voicings you like — and send them back
This is the important part. When you find a setting you like:
1. Top-bar **Presets menu → "Save current as…"**, give it a name (e.g. *"Punchy drums"*, *"Clean master"*).
2. Use the **A / B** buttons to quickly compare two voicings (set one, hit **A→B** to copy, tweak, then flip A/B).

**To send me your settings:** the presets are files here —
```
~/Library/Audio/Presets/MelechDSP/MasterLimiter/
```
(Finder → Go → Go to Folder → paste that.) **Email me the `.mlpreset` files** for the voicings you liked. Each one captures the *complete* setting, so I get exactly what you heard.

## 5. Feedback that helps most
Along with the preset files, a few words on each:
- What source/genre was it for?
- Loud enough? Too aggressive or too soft?
- Any **pumping, breathing, or dulled transients**? Where?
- How does it compare to your usual limiter (Pro-L, Ozone, L2, etc.) at matched loudness?
- Anything that sounded *wrong* (distortion, clicks, weird stereo).

## 6. Beta caveats (known / expected)
- The **DEV window controls are temporary** — they'll be baked into simpler controls for the release. They're here only so you can help tune.
- **Character** is greyed out on purpose during this phase (attack is controlled by the DEV Attack knob instead).
- At intermediate **Color** (between 0 and 100%) the low end can shift slightly — known, being reworked. The endpoints (0% and 100%) are clean.
- It's a beta — save your DAW session often, and tell me about any crash with what you were doing.

Thank you — your ears are doing real work here. Send the `.mlpreset` files + notes and I'll fold the best voicings into the next build. 🙏
— avishali / MelechDSP
