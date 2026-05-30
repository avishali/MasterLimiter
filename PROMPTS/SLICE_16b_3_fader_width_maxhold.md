# Cursor Task — Slice 16b.3: fader width + max-peak hold

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **UI refinement on the uncommitted Slice 16b branch** (`slice-16b-visual-restyle`).
> Two fixes: (1) restore the I/O fader width — 16b.2 narrowed the faders, but
> the only intended change was moving the numeric value outside the meter;
> (2) the max-peak readout must latch and hold its maximum when the signal
> stops. No DSP/param change. **Do NOT commit, do NOT push.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Product UI only: `Source/ui/MainView.{cpp,h}`, `Source/ui/meters/
  MeterComponent.{cpp,h}`, `Source/ui/meters/MeterGroupComponent.{cpp,h}`.
- No params, no DSP, no HQ. Continue branch `slice-16b-visual-restyle`.
  **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. RESTORE I/O FADER WIDTH
────────────────────────────────────────

16b.2 narrowed the I/O fader (hit area / handle) to expose more meter — but the
intended change was only to move the **numeric value** outside the meter, which
is already done. **Widen the faders back** to a comfortable grab size (restore
the pre-16b.2 width / handle). The numeric value stays outside the meter box
(keep that). Meter click-to-reset (16b.2) must still work alongside the wider
fader (the fader can own its handle area; clicks elsewhere on the meter reset
the peak).

────────────────────────────────────────
2. MAX-PEAK READOUT — latch + hold on signal stop
────────────────────────────────────────

The max-peak readout drops when the signal stops; it must **hold the maximum
indefinitely** until reset. Drive the max readout from the **persistent latched
maximum** (`maxPeakLDb_` / `maxPeakRDb_`), which only ever rises to new maxima
and is cleared **only** by the meter click-reset (Slice 15b / 16b.2). It must
NOT decay or follow the signal down when playback stops. (This is distinct from
the live/current readout, which may release — only the **max** readout latches.)
Confirm: play → stop → the max readout stays at the peak it reached; click the
meter → it resets.

────────────────────────────────────────
3. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j && cmake --build build-release -j
```
- Zero new `Source/` warnings. UI-only → Slice 3/4/5 unchanged.
- In host: faders are comfortably wide again, numeric value still outside the
  meter; play then stop → max readout holds; click meter → resets.

────────────────────────────────────────
4. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log. 2. Diffs (fader width restore; max-readout latch source).
3. Build summary. 4. `git status --short`. 5. Open questions.
