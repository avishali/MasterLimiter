# Cursor Task — Slice 15b.2: I/O meter release + peak bar colour

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Small product-only refinement on the uncommitted Slice 15b branch.** From
> avishali's audition: (1) the I/O level meter **bars fall too fast** when audio
> stops — add a slower display release (instant attack, slow fall). GR meter is
> fine, leave it. (2) The **sample-peak bar should be a lighter blue**, not
> yellow. **Do NOT commit, do NOT push.**

> Continue product branch `slice-15b-io-meter-features`. No HQ change (the
> sibling `slice-15b-meter-scale` Top48Db work stays as-is).

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product edits only: `Source/ui/meters/MeterGroupComponent.{cpp,h}` (release),
  `Source/ui/meters/MeterComponent.cpp` (peak colour). No HQ/param/ADR changes.
- Do NOT touch the GR meter (`GainReductionMeter.*`) — its 50 ms feel is
  approved.
- Keep it product-local: do NOT include the new-system `MeterTypes.h` /
  `MeterBallistics.h` in `MeterGroupComponent.cpp` (it includes the old-system
  `MeterRenderState.h` — they collide). Use a tiny inline smoother instead.
- **Do NOT commit, do NOT push.** Leave `MCP` untouched.

────────────────────────────────────────
1. I/O METER BAR RELEASE (slower fall on stop)
────────────────────────────────────────

The I/O level bars currently follow the per-block peak with no display
release, so when audio stops they snap down. Add an **asymmetric display
smoother** to the bar levels feeding the provider in
`MeterGroupComponent::pushLevelRenderStates` (or wherever the peak/RMS dB are
passed to `provider*.updateFromValues`):

- **Instant attack** (rising: jump straight to the new dB — peaks show
  immediately).
- **Slow release** (falling: ease down). Use a linear falloff of about
  **20 dB/s** (≈ full-range fall in ~1–1.5 s) — tunable; pick a graceful
  "settling" feel rather than a snap. (A per-channel one-pole with τ ≈ 0.4–0.6 s
  is an acceptable alternative — choose whichever reads cleaner; report which
  and the value.)
- Apply to the **peak** bar value, and to the **RMS** bar value when RMS
  display is enabled (the RMS measurement window in the engine is separate;
  this is display ballistics).
- Keep the existing numeric peak-hold + the click-to-reset behaviour unchanged.
- Implement as a tiny inline smoother (a couple of floats per channel) — no new
  HQ includes (avoid the `MeterRenderState`/`MeterTypes` collision).

Result: when audio stops, the I/O bars glide down rather than dropping
instantly.

────────────────────────────────────────
2. SAMPLE-PEAK BAR COLOUR → lighter blue
────────────────────────────────────────

In `MeterComponent::paintLevel`, the **sample-peak** bar/cap is currently
yellow. Change it to a **lighter shade of blue** (a light blue/teal tint from
the theme, consistent with the clean/dark/teal aesthetic). Ensure it stays
clearly distinct from the **RMS fill** colour (so peak vs RMS are
distinguishable when both are shown). Do not change the over/clip colouring or
the GR meter. avishali will fine-tune the exact shade in-host — pick a
sensible light blue and report the colour used.

────────────────────────────────────────
3. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings.
- UI-only → Slice 3/4/5 quick unchanged (optional sanity).
- In a host: stop audio → I/O bars glide down (not instant); peaks still jump
  up instantly. Sample-peak bar is light blue, RMS fill clearly distinct. GR
  meter unchanged.

────────────────────────────────────────
4. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Diffs: I/O bar release smoother (state + falloff value/method);
   peak-bar colour change (old → new colour).
3. Build summary (Debug + Release, warnings).
4. `git status --short` (product slice-15b branch; HQ unchanged).
5. Open questions (release rate feel, exact peak blue shade).

────────────────────────────────────────
5. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali re-auditions the I/O bar release + peak colour. On approval → the
architect writes the Slice 15b consolidated close (product + HQ Top48Db +
submodule bump + docs + prompt archive). **Do not self-close.**
