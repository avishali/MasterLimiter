# Engine A/B — tester protocol

**Purpose:** produce a verdict on **Transparent vs Open** that we can actually act on, because that verdict
gates which engine ships in 1.0. Established 2026-08-02.

**Requires the AB-1 build** (`PROMPTS/SLICE_AB1_trustworthy_engine_ab.md`): latency parity, loudness-matched
switching, blind A/B labels. Do not run this protocol on 0.3.2-beta — see "Why the old round doesn't count".

---

## Why the old round doesn't count

Measured on the shipped 0.3.2-beta (jazz `MIX 0003`, ceiling -1 dB sample-peak):

- **Latency differs by 44 ms across engines** (Transparent 3229 samples / 67.3 ms, Open 1104 / 23.0 ms).
  Flipping mid-session re-syncs or misaligns playback at the exact moment of comparison.
- **Open is 0.8-1.2 dB louder at the same input gain.** Louder is heard as better. The old instruction was
  "match levels by ear if one is louder" — that asks the tester to do the hardest part of the experiment
  unaided, and it is the single largest bias in limiter listening tests.

Any preference data collected under those conditions measures loudness and timing, not engine character.
It is not a small effect to correct for afterwards — it is the size of the effect we are trying to detect.

---

## The decision rule (pre-registered — agreed BEFORE looking at results)

Fixing the rule in advance is the point. It stops us reading whatever comes back as confirmation.

- Each tester runs **5 sources x 2 passes = 10 forced-choice trials**.
- A tester's vote for a source counts only if **both passes agree** (self-consistency check). Disagreeing
  passes are recorded as "no preference" — they are data about how close the engines are, not noise to discard.
- **Ship the winner** if it takes **>= 70% of consistent votes** across all testers.
- **50-70%** => the engines are close enough that the choice is not being made on quality. Decide on
  simplicity/CPU/latency instead, and say so explicitly rather than pretending the ears decided.
- ⭐ **Split by source type => this is now the EXPECTED outcome, not a fallback.** Measured 2026-08-02
  (`mbl_frontier2.py`, |MACRO|+|PUMP|+|ROUGH|, peak-gated): Transparent is the best engine we have on the
  **live-recorded** source (3.05, beating Pro-L 2 Allround 3.18) and the worst on **dense studio
  production** (10.94); Open+Smart is the exact inverse (2.91 and 2.22 on the dense sources, 4.91 on the
  live one). If the listening splits the same way, that **confirms both engines earn their place** and the
  selector is a product feature, not a leftover. If it does NOT split that way, the measurement and the
  ears disagree and we stop and find out why before shipping either.

Minimum for any conclusion: **3 testers**. Below that, report it as anecdote and say so.

---

## Setup (once)

1. Insert MasterLimiter last on the master bus. Nothing after it.
2. Confirm **A/B Match is ON** (it is by default). The selector shows **A** and **B**, not engine names.
   You are not supposed to know which is which — that is deliberate, and it is what makes your answer worth
   collecting.
3. Open the **History Graph** so you can see gain reduction.
4. Set **Ceiling = -1.0 dB**.

## Per source (5 sources)

Use five contrasting sources. Suggested spread — swap in your own, but keep the spread:

| # | Source type |
|---|---|
| 1 | **Live recording** (live show, room mics) — REQUIRED, this is where the engines are predicted to differ most |
| 2 | **Dense studio production** (loud, tightly mixed) — REQUIRED, the predicted opposite case |
| 3 | Full mix, acoustic or dynamic |
| 4 | Drums / percussion-led, or vocal-forward |
| 5 | Bass-heavy material |

⚠️ **Sources 1 and 2 are not optional.** The measurement predicts the engines swap places between exactly
those two kinds of material. A tester who brings five dense productions cannot detect the split, and their
data would read as "Open always wins" — which would be an artefact of their library, not a verdict.

For each source:

1. Raise **Input Gain** until the History Graph shows **6-10 dB** of gain reduction on the loudest section.
   **Leave it there for the whole comparison** — do not re-tune it per engine. Note the value.

   > ⚠️ **Why not 3-6 dB (the original instruction)?** Measured 2026-08-02 on a 2.6-minute live mix:
   > at ~7 dB of push every engine — ours and both references — preserves the source's macro-dynamics
   > to within **0.04 dB** of each other. They are genuinely indistinguishable on the metrics there.
   > At ~11 dB of push they spread out over **2.3 dB**. Asking testers to judge at 3-6 dB GR would be
   > asking them to hear a difference that is not present. Push until the engines actually differ.
2. Play a **10-20 s section** that includes both a loud and a quiet part.
3. Flip **A <-> B** at least four times while it loops. Loudness is already matched, so what changes is
   character only.
4. Pick a winner. If you genuinely cannot tell, **say "no preference"** — that is a real and useful answer,
   not a failure to do the task.
5. **Do the whole source again later** (pass 2), ideally after the other sources, without checking what you
   answered the first time.

## What to listen for

- Does the mix **breathe** between transients, or does it pump / flatten?
- Are transients (snare, pick, consonants) **intact** or dulled?
- Low end: **solid** or grainy / loose?
- Any pumping, distortion, clicks, or stereo weirdness — and **where in the audio**.

---

## Response form (copy this, fill it in, send it back)

```
TESTER:
DAW / OS:
BUILD (from plugin header, main@<hash>):

SOURCE 1  type: ................  input gain: ..... dB   GR seen: ..... dB
  pass 1 winner:  A / B / no preference
  pass 2 winner:  A / B / no preference
  what decided it:
  anything wrong (where):

SOURCE 2  type: ................  input gain: ..... dB   GR seen: ..... dB
  pass 1 winner:  A / B / no preference
  pass 2 winner:  A / B / no preference
  what decided it:
  anything wrong (where):

SOURCE 3  type: ................  input gain: ..... dB   GR seen: ..... dB
  pass 1 winner:  A / B / no preference
  pass 2 winner:  A / B / no preference
  what decided it:
  anything wrong (where):

SOURCE 4  type: ................  input gain: ..... dB   GR seen: ..... dB
  pass 1 winner:  A / B / no preference
  pass 2 winner:  A / B / no preference
  what decided it:
  anything wrong (where):

SOURCE 5  type: ................  input gain: ..... dB   GR seen: ..... dB
  pass 1 winner:  A / B / no preference
  pass 2 winner:  A / B / no preference
  what decided it:
  anything wrong (where):

OVERALL
  If you had ONE engine for every master, which:  A / B
  vs your usual limiter (which one?) at matched loudness:
  Did you hit Reveal at any point before finishing?  yes / no
```

Also send any `.mlpreset` voicings you liked — they record the true engine, so we can decode them.
`~/Library/Audio/Presets/MelechDSP/MasterLimiter/`

---

## Notes for us (not for testers)

- The A<->B mapping is **randomized per plugin instance**, so testers are automatically counterbalanced —
  "A" is not the same engine for everyone. Decode from the returned preset / the mapping stored in state.
- "Did you hit Reveal" is the blinding-integrity check. Trials after a reveal are excluded, not argued about.
- Aggregate per source *type*, not just overall — the split-by-source case above is a real possible outcome
  and it is the one that would change the product shape.
- This protocol answers **which of our two engines**. It does NOT answer **are we competitive** — that is
  `tools/analysis/mbl_frontier.py` (Pro-L 2 / Ozone IRC modes, measured, not listened). Keep the two
  questions separate; a win here is not a win against the market.
