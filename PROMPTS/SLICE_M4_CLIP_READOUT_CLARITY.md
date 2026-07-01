# SLICE M4 — Clip readout clarity + hold; meter hygiene

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali
**Repos:** plugin `MasterLimiter` only. UI/metering only; no DSP/param/audio/latency change.
**Reference:** `docs/METERING_ACCURACY_AUDIT.md` findings **7** and **10**. Lowest priority of M1–M4.

---

## Why
- The **"Clip x.x / y.y"** readout is the **clipper's gain-reduction depth** (how hard the soft/hard clipper is working), **not** an output-over-0-dBFS indicator (`PluginProcessor.cpp:1176-1191`, `MainView.cpp:127-129`). Accurate for what it is, but a mastering engineer reads "Clip" as "output clipped by x dB" — misleading.
- The current-depth number decays at **50 ms with no hold** (`ClipBallistics.cpp:11-26`; `MeterBallistics` ignores `clipHoldMs`), so transient clip depth under-reads / is too fast to read.
- Hygiene: a dead peak-hold-decay path and a divergent FullRange anchor table (finding 10).

## Allowed files
```
Source/ui/meters/ClipBallistics.cpp / .h
Source/ui/MainView.cpp                         (clip label/caption + tooltip)
Source/ui/meters/MeterRenderStateProvider... (shared) OR MeterGroupComponent — hygiene items, only if low-risk
docs/METERING_ACCURACY_AUDIT.md (tick M4)  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_M4_CLIP_READOUT_CLARITY_CLOSE.md  (new)
```
**Non-goals / STOP:** no DSP/clipper change; no param change; don't touch the M1/M2/M3 areas. The FullRange-anchor unification (finding 10b) is **optional** — do it only if trivially safe; otherwise just annotate and defer.

## Changes
1. **Clarify the clip readout label.** Rename/caption it so it reads as **clipper drive/depth**, e.g. label "CLIP GR" (or keep "Clip" with a tooltip: *"Clipper gain-reduction depth in dB — not an output-over indicator; output overs are guaranteed ≤ ceiling by the FinalCeiling."*). Keep the value/format (`formatPositiveBare`, current/max).
2. **Hold the current-depth number.** The left "x.x" must hold a transient long enough to read: use a peak-hold with **~600 ms–1 s hold then decay** (mirror the readout ballistics used elsewhere), instead of the 50 ms release that `MeterBallistics` currently applies (it ignores `clipHoldMs`). The right "y.y" (max since reset) is already a true latch — keep it.
3. **Hygiene (low-risk only):**
   - The bar peak-hold "1500 ms + 12 dB/s decay" path is **dead** because `holdEnabled_ == true` bypasses it (`MeterRenderStateProvider.cpp:129-137`) → the max marker latches. Latching is the intended mastering behavior, so **annotate** it (comment) or delete the unreachable decay branch to remove confusion. Do **not** change the latch behavior.
   - (Optional) unify `MeterComponent` FullRange mapping onto the shared `MeterRenderStateProvider::normaliseDb(...FullRange)` instead of the file-local anchor table, so all meters share one auditable mapping. Skip if it risks visual regression — annotate and defer to a follow-up.

## Build, verify, close
```bash
cmake --build build 2>&1 | tail -8 && auval -v aufx MaLm Melc 2>&1 | tail -5
./scripts/install_user.sh build 2>&1 | tail -3
```
**Acceptance (Claude verifies 1–2; avishali auditions 3):**
1. Build clean; auval PASS; installed fresh; no DSP/param/latency change.
2. Clip readout reads as clipper depth (label/tooltip clear); a transient clip holds ~0.6–1 s so it's readable; max side still latches.
3. **Audition:** the clip figure is legible and unambiguous; nothing else changed.

**Close gate:** `docs/METERING_ACCURACY_AUDIT.md` (mark M4); `docs/PROGRESS.md`; `PROMPTS/PLAN.md`; commit plugin-only; archive CLOSE.
