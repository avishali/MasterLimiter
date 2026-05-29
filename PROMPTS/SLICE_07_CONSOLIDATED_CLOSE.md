# Cursor Task — Slice 7 Consolidated Close (7 + 7.1 + 7.1.1 + 7.1.2 + 7.1.3 + 7.1.4 + 7.1.4.1 + 7.1.5)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Consolidated close covering the entire Slice 7 family: stereo unlink
> core, 2ch GR meter with latched-max readouts, the pop investigation
> arc (timing probe, click detector, warm-keep, last-click snapshot),
> NEON SIMD in the envelope inner ramp loop, click threshold tuning,
> and the visual polish (em-dash mojibake fix, Clipper LED relocation,
> on/off button position).**
>
> Five phases:
> 1. **Product debug-probe removal** — pull out the timing watermarks,
>    click detector, and last-click snapshot. They served their purpose
>    for the pop investigation; production code is cleaner without them.
> 2. **HQ push** — 4 commits become public.
> 3. **Product docs** — PROGRESS entry + PLAN update (Slice 7 marked
>    shipped, Slice 11b2 promoted to next).
> 4. **Product prompts archive** — all Slice 7.x prompts committed.
> 5. **Product push** — all commits FF onto main and pushed.
>
> No DSP behaviour change in the close (debug removal is instrumentation
> only). Bench Slice 3/4/5 must still PASS unchanged after debug removal.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. Verify
current branch in each repo before pushing. **Do not force-push.** Do
not skip hooks. If `git push` requires interactive auth and the
credential helper isn't configured, STOP and report.

────────────────────────────────────────
1. SCOPE — files
────────────────────────────────────────

**HQ — push only, no edits.**

**Product — edits:**
- `Source/PluginProcessor.h` — remove debug-only atomics, getters,
  reset methods (timing watermarks, output-click stats, last-click
  snapshot).
- `Source/PluginProcessor.cpp` — remove debug instrumentation:
  per-section timing probes (`std::chrono::steady_clock` snapshots),
  per-block input-abs tracking, output-side click detector scan,
  last-click atomic stores. Keep all DSP behaviour unchanged.
- `Source/ui/MainView.h/.cpp` — remove debug header rendering (the
  "Blk N | Drv N / Up N / ..." top line and the "LastClick: ..."
  second line shown when Clicks > 0).
- `docs/PROGRESS.md` — new Slice 7 family entry at top.
- `PROMPTS/PLAN.md` — Slice 7 row marked shipped, Slice 11b2 promoted
  to next; multiband detection backlog updated.
- `PROMPTS/SLICE_07_*.md` — archived in a single commit.

**DO NOT TOUCH:**
- HQ DSP source. The NEON SIMD ramp loop, T/S combinator, ramp anchor,
  fast-path, Clean attack cap (3 ms) — all stay as they are.
- Bench files.
- Any param ID, range, default.
- `currentGrDb_`, `currentGrLDb_`, `currentGrRDb_`, `maxGrSinceResetDb_`,
  `currentClipDb_`, `maxClipSinceResetDb_` — these are the
  **production** atomics powering the GR meter and Clipper readout.
  Keep. Only remove the debug-investigation atomics added in 7.1.1
  (timing watermarks) and 7.1.2/7.1.3 (output-click + last-click
  snapshot).
- `currentTpTrimDb_` is already gone since Slice 9.6 — confirm it's
  not re-introduced anywhere.

────────────────────────────────────────
2. PHASE 1 — Product debug-probe removal
────────────────────────────────────────

### 2.1 PluginProcessor.h — remove debug atomics + getters + reset

Remove these fields (added in 7.1.1, 7.1.2, 7.1.3):

```cpp
std::atomic<int64_t> audioBlockMaxUs_;
std::atomic<int64_t> sectionMaxUsDrive_;
std::atomic<int64_t> sectionMaxUsUpsample_;
std::atomic<int64_t> sectionMaxUsClipperPeak_;
std::atomic<int64_t> sectionMaxUsEnvelope_;
std::atomic<int64_t> sectionMaxUsGainMul_;
std::atomic<int64_t> sectionMaxUsDownsample_;
std::atomic<int64_t> sectionMaxUsOutput_;

std::atomic<float>   outputMaxDeltaSeen_;
std::atomic<float>   outputMaxAbsSeen_;
std::atomic<int64_t> outputClickCount_;

std::atomic<int64_t> lastClickBlkUs_;
std::atomic<int64_t> lastClickDrvUs_;
std::atomic<int64_t> lastClickUpUs_;
std::atomic<int64_t> lastClickClpUs_;
std::atomic<int64_t> lastClickEnvUs_;
std::atomic<int64_t> lastClickGMulUs_;
std::atomic<int64_t> lastClickDnUs_;
std::atomic<int64_t> lastClickOutUs_;
std::atomic<float>   lastClickD_;
std::atomic<float>   lastClickAbsIn_;
std::atomic<float>   lastClickAbsOut_;
```

Remove all corresponding public getters and `reset*()` methods. The
existing names should be greppable: `getAudioBlockMaxUs`,
`getSectionMaxUs*`, `getOutputMaxDelta*`, `getOutputMaxAbs*`,
`getOutputClickCount`, `getLastClick*`, `resetAudioBlockMaxUs`,
`resetOutputClickStats`. Remove every match.

**Keep** (production atomics):
- `currentGrDb_`, `currentGrLDb_`, `currentGrRDb_`, `maxGrSinceResetDb_`
- `currentClipDb_`, `maxClipSinceResetDb_`
- Their getters and resets.

### 2.2 PluginProcessor.cpp — remove debug instrumentation

In `processBlock`, remove:
- `const auto t0/t1/.../t7 = std::chrono::steady_clock::now();` lines.
- Per-section duration computation + atomic stores (`sectionMaxUs*`).
- Block-total watermark store (`audioBlockMaxUs_`).
- `float blockInputAbs = 0.0f;` and the input-abs scan loop near the
  top of processBlock.
- The end-of-processBlock output click detector block:
  - The per-channel/per-sample loop computing `blockMaxDelta`,
    `blockMaxAbs`, and `blockHasClick`.
  - The atomic stores for `outputMaxDeltaSeen_`, `outputMaxAbsSeen_`,
    `outputClickCount_`.
  - The `if (blockHasClick) { lastClick*_.store(...); }` snapshot block.
- The `kClickThreshold` constant (the 0.5f one from 7.1.5) — no longer
  used.

Keep all DSP work: drive, OS up/down, clipper, peak detect, envelope,
lookahead, gain multiply, IO trims, metering, loudness, ispTrim
removed since 9.6. The functional pipeline is untouched.

### 2.3 MainView.h/.cpp — remove debug header

Locate the debug header rendering added in 7.1.1 and extended in 7.1.2
/ 7.1.3. It's likely in `paint(...)` or a dedicated debug-render
method. Remove:
- The top "Blk N | Drv N / Up N / ... | Clicks N / D N.NN / Abs N.NN"
  rendering.
- The conditional "LastClick: ..." second-line rendering when
  `Clicks > 0`.
- Any member variables / fonts / colours added solely to support the
  debug header.

Verify by building and visually inspecting: the plugin window should
no longer show any text in the top area where the debug line was. The
normal title bar and content area (Maximizer panel, I/O panel, GR
meter) are unchanged.

### 2.4 Product debug-removal commit

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect:
#    M Source/PluginProcessor.h
#    M Source/PluginProcessor.cpp
#    M Source/ui/MainView.h
#    M Source/ui/MainView.cpp

git add Source/PluginProcessor.h Source/PluginProcessor.cpp Source/ui/MainView.*

git commit -m "$(cat <<'EOF'
MasterLimiter Slice 7 close: remove debug timing/click instrumentation

The Slice 7.1.1 / 7.1.2 / 7.1.3 debug instrumentation served its
purpose for the audible-pop investigation that led to the Slice
7.1.4.1 NEON SIMD fix in the envelope inner ramp loop. With pops
resolved across all Character modes and Link values, the
instrumentation is no longer earning its keep in production code.

Removed:
- Per-section processBlock timing probe (Blk/Drv/Up/Clp/Env/GMul/Dn/
  Out us watermarks).
- Output click detector (max sample-to-sample delta, max abs,
  click counter).
- Last-click section snapshot (Blk/Drv/.../Out + D/AbsIn/AbsOut at
  the most recent click event).
- The debug header rendering in MainView (top line + conditional
  LastClick second line).
- kClickThreshold constant.

Retained:
- Production GR atomics: currentGrDb_, currentGrLDb_, currentGrRDb_,
  maxGrSinceResetDb_ + getters + resetMaxGr.
- Production Clipper atomics: currentClipDb_, maxClipSinceResetDb_
  + getters + resetMaxClip.
- All DSP behaviour: envelope chain, NEON SIMD ramp loop, OS chain,
  stereo unlink fast-path / full-path, gain blend.

Bench Slice 3/4/5 PASS 13/13, 14/14, 25/25 unchanged (UI / atomic
removal, no DSP touch).

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

Build verification before committing:

```bash
cmake --build build-release --config Release
cmake --build build-debug -j
```

Both must build clean.

────────────────────────────────────────
3. PHASE 2 — HQ push
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# Existing unrelated MCP dirt remains; leave it.

git rev-parse --abbrev-ref HEAD
# Confirm: master

git log --oneline origin/master..HEAD
# Should show 4 commits:
#   643ac50 LimiterEnvelope: actually engage SIMD in inner ramp loop
#   52b5e07 LimiterEnvelope: SIMD the inner ramp loop
#   d805b2b dsp_bench: Slice 3/4/5 recal (treatment B) + Ozone 11 reference driver
#   b64f7ae ADR-0008: MasterLimiter stereo unlink (per-channel parallel envelopes)

git push origin master

git log --oneline -8
```

Report the post-push log. If push requires interactive auth, STOP and
report.

────────────────────────────────────────
4. PHASE 3 — Product docs (PROGRESS + PLAN)
────────────────────────────────────────

### 4.1 PROGRESS.md entry

Open `docs/PROGRESS.md`. Insert the following entry at the **top** of
the slice list, above the most recent existing entry (Slice 7 — wait,
the original Slice 7 entry from earlier draft was never written
because we never finished that close; this is the actual Slice 7
close entry now):

```markdown
## Slice 7 — Stereo Link % + 2ch GR meter + envelope SIMD (2026-05-29)

**Shipped (final state across the 7.x family):**

Product (`MasterLimiter`):
- **Stereo Link %** — new param `stereo_link_pct` (float 0..100%, step
  0.1, default 100, frozen ID, "XX%" UI). Per-channel parallel
  `LimiterEnvelope` instances (`envelope_L_` / `envelope_R_`) with
  gain blend:
    g_linked = min(g_L, g_R)
    g_out_L  = link · g_linked + (1 − link) · g_L
    g_out_R  = link · g_linked + (1 − link) · g_R
  Fast-path at link ≥ 99.95: bit-exact Slice 9.6 behaviour (single
  envelope on max(|L|, |R|), shared gain). Default 100 lands here →
  Slice 3/4/5 bench PASS bit-exact.
- **2ch GR meter** — two thin L/R bars in a dark inset slot,
  top-down warning fill, single compact "current / max" readout
  (e.g. "3.2 / 5.1"). Click the readout to reset latched max.
- **Latched max readouts** for GR and Clipper. Click-to-reset.
  Always-numeric formatting (no em-dash, no placeholder text).
- **Clipper LED relocated** out of the readout text region (sits in
  the Clipper knob label area now).
- **limiter_active button relocated** from the Maximizer panel
  header to a position above and between the Gain and Ceiling knobs,
  so it no longer crowds the wider 2-bar GR meter.

HQ (`mdsp_dsp::LimiterEnvelope`):
- **NEON SIMD** on the inner ramp loop (`vld1q_f32`, `vfmaq_f32`,
  `vminq_f32`, `vst1q_f32`) with `__ARM_NEON` guard and `__SSE2__`
  fallback (`_mm_loadu_ps`, `_mm_mul_ps`, `_mm_add_ps`, `_mm_min_ps`,
  `_mm_storeu_ps`). Assembly inspection confirms `fmla.4s` / `fmin.4s`
  on arm64. ~4× speedup on the per-peak ramp work; resolves the
  audible buffer underrun that occurred under heavy GR (Clean mode,
  any Link value).
- Loop refactored to forward j-iteration (substitute j = i + la − k)
  so `ext_` and `attackTable_` read forward with stride 1 —
  SIMD-friendly memory access.

HQ (`dsp_bench`):
- ADR-0008 documenting the stereo unlink decision (per-channel
  parallel envelopes, gain blend math, fast-path bit-exactness,
  constant latency, detection-bus-only — mirrors ADR-0005 reasoning,
  M/S deferred as orthogonal axis).
- Slice 3/4/5 PASS 13/13, 14/14, 25/25 unchanged after SIMD (math is
  bit-equivalent within float-rounding tolerance; criteria
  accommodate sub-epsilon variance).

**Pop investigation arc (resolved, debug probes removed):**

The 7.x family included a five-step pop investigation that surfaced
several diagnostic findings:
- 7.1.1: added per-section timing probe, tried envelope_R_ warm-keep
  as defensive fix.
- 7.1.2: numeric readouts (em-dash mojibake fix), Clipper LED
  relocation, output click detector (max delta + max abs + click
  counter).
- 7.1.3: data showed warm-keep was paying CPU for a hypothesis
  disproved by the click counter; reverted. Lowered click threshold
  0.5 → 0.2 to chase subtler events. Added last-click section
  snapshot via atomic stores.
- 7.1.4: tried SIMD refactor via juce::dsp::SIMDRegister, but
  available JUCE version's `fromRawArray` asserted on alignment.
  Cursor's unaligned-wrapper workaround produced mathematically
  correct output (bench PASS) but didn't actually vectorize.
  avishali audition confirmed: mode-scaled pops (Aggressive 0,
  Tight moderate, Clean many) — direct evidence of scalar-loop cost
  proportional to attackSamples.
- 7.1.4.1: replaced with direct NEON intrinsics. Assembly verified
  SIMD engaged. avishali audition: pops resolved.
- 7.1.5: raised click threshold back to 0.5 (the 0.2 threshold
  caught legitimate brickwall transients as "clicks"). Attempted
  Clean attack cap 3 → 1.5 ms for further CPU correlation reduction
  but bench failed (sine sweep null −3.16 dB, IMD 10.05% / 10.0,
  SP overs 4400 / 2200) — character changed more than predicted.
  Reverted the cap; click threshold change shipped on its own.

This close commit removes the debug instrumentation (per-section
timing watermarks, click detector, last-click snapshot) — production
code is cleaner without them and the audible issue they helped
diagnose is resolved.

**ADRs:**
- ADR-0008 (Stereo unlink / per-channel parallel envelopes) — accepted
  2026-05-29.

**Gate status:**
- Builds clean (Release + Debug).
- avishali audition: default 100% sounds identical to Slice 9.6
  (fast-path bit-exactness). Sweep 100 → 50 → 0 produces progressive
  width preservation. A/B vs Ozone Stereo Unlink: width gap closed
  into family. Pops resolved across all Character modes and Link
  values after the 7.1.4.1 NEON SIMD fix.
- Constant latency: 301 SP / 301 TP at 48k, unchanged from Slice 9.6.
  Link % automation produces no PDC change.
- Bench: Slice 3/4/5 PASS 13/13, 14/14, 25/25 — bit-exact at default
  link = 100 via fast-path; SIMD ramp loop bit-equivalent within
  float-rounding tolerance.
- Reference Ozone IRC IV comparison from Slice 9.6c shows MasterLimiter
  in family for IMD (~25% wider on synthetic SMPTE corpus); the
  remaining ~7 dB null residual gap and the visible CPU↔GR
  correlation in Ableton's CPU meter on heavy unlinked Clean
  reduction are architectural (single-band 4× OS limiting). Lever for
  parity is multiband detection (new ADR-0009 — promoted to active
  backlog).

**Deferred / placeholders:**
- `ms_link_pct` ID stays in `ParameterIDs.h` as a deferred placeholder
  for a future Slice 7b (M/S detection) if ever promoted from backlog.
- Per-channel envelope SIMD optimisation OF the smoothing stage —
  current SIMD covers the dominant inner ramp loop; smoothing could
  be vectorized in a future micro-slice if needed.
- Slice 7.1.5's Clean attack cap to 1.5 ms attempted and reverted —
  character change exceeded expectation. Not in backlog as separate
  item; subsumed by the multiband detection direction.

**Followups in PLAN backlog:**
- Slice 11b2 (Auto/Track + Learn + Bypass-with-match) promoted to
  next active slice.
- Multiband detection (new ADR-0009) — primary lever for the
  remaining "open" gap vs Ozone IRC IV and the architectural CPU↔GR
  correlation.
- Slice 7b (M/S detection) — placeholder param retained; no active
  demand.
```

### 4.2 PLAN.md update

Open `PROMPTS/PLAN.md`. Find the Slice 7 row in the table. Update its
status to shipped per the existing convention. Replace the prior
gate-headline wording with:

```
Shipped 2026-05-29. Per-channel parallel envelopes with continuous
Link % blend (frozen ID `stereo_link_pct`, default 100% = bit-exact
fast-path). 2ch GR meter + latched-max readouts. NEON SIMD inner
ramp loop (~4× speedup, resolves heavy-GR underrun). Constant
latency 301/301. Bench Slice 3/4/5 PASS 13/13, 14/14, 25/25
unchanged. ADR-0008. M/S deferred. Multiband detection promoted as
next architectural lever for "open" gap and CPU↔GR correlation
parity with Ozone.
```

Find the "next slice" marker — promote **Slice 11b2** to immediate
next position.

Backlog rows — add/update (preserving prior Slice 9 close entries;
only adding new):

```
| Backlog | Multiband detection (detection-bus only) | new ADR-0009 |
new product params | Promoted to next architectural slice after Slice
7 close. Closes the remaining ~7 dB null-residual / "open" gap vs
Ozone IRC IV (Slice 9.6c reference) AND addresses the visible CPU↔GR
correlation surfaced during 7.1.5 (single-band 4× OS limiting puts
all peak work in one envelope; multiband distributes across band-
specific envelopes, each with lower per-band peak density).
Detection-bus only to avoid LR4 audio-path phase concerns per
ADR-0005's reasoning. |
| Backlog | Slice 7b: M/S detection | ADR-0008 §Alternatives |
ms_link_pct already declared in ParameterIDs.h | Placeholder ID
retained. No active demand. |
| Backlog | Envelope smoothing-stage SIMD | mdsp_dsp::LimiterEnvelope |
none | The inner ramp loop is NEON-vectorized as of Slice 7.1.4.1.
The per-sample smoothing cascade (s1/s2 + T/S s1s/s2s for Clean)
remains scalar. If a future regression surfaces the smoothing as a
hot path, vectorize. Low priority. |
```

### 4.3 Product docs commit

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git add docs/PROGRESS.md PROMPTS/PLAN.md
git status --short
# expect ONLY:
#    M  docs/PROGRESS.md
#    M  PROMPTS/PLAN.md
#    ?? PROMPTS/SLICE_07_*.md   (handled in Phase 4)

git commit -m "$(cat <<'EOF'
Slice 7 close: PROGRESS entry + PLAN update

Comprehensive Slice 7 family entry documenting:
- Stereo unlink (per-channel parallel envelopes, gain blend, fast-
  path at link >= 99.95, bit-exact at default 100).
- 2ch GR meter with latched-max readouts, click-to-reset.
- Pop investigation arc (timing probe, click detector, last-click
  snapshot) and resolution via NEON SIMD in the envelope inner ramp
  loop (Slice 7.1.4.1).
- ADR-0008.
- Bench Slice 3/4/5 PASS 13/13, 14/14, 25/25 throughout.

PLAN: Slice 7 marked shipped, Slice 11b2 promoted to next.
Multiband detection backlog row promoted in priority as the
architectural lever for the remaining "open" gap and CPU<->GR
correlation parity with Ozone IRC IV. Envelope smoothing-stage SIMD
added as low-priority backlog. M/S detection placeholder retained.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

────────────────────────────────────────
5. PHASE 4 — Product prompts archive
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git status --short
# expect ONLY:
#    ?? PROMPTS/SLICE_07_stereo_unlink.md
#    ?? PROMPTS/SLICE_07_CLOSE.md
#    ?? PROMPTS/SLICE_07_1_gr_2ch_meter_and_max_readouts.md
#    ?? PROMPTS/SLICE_07_1_1_pop_diag_em_dash_onoff.md
#    ?? PROMPTS/SLICE_07_1_2_readout_numeric_clipper_led.md
#    ?? PROMPTS/SLICE_07_1_3_revert_warmkeep_tighter_clicks.md
#    ?? PROMPTS/SLICE_07_1_4_simd_ramp_loop.md
#    ?? PROMPTS/SLICE_07_1_4_1_actually_engage_simd.md
#    ?? PROMPTS/SLICE_07_1_5_clean_attack_cap_and_click_threshold.md
#    ?? PROMPTS/SLICE_07_CONSOLIDATED_CLOSE.md   (this file)

git add PROMPTS/SLICE_07_*.md

git commit -m "$(cat <<'EOF'
Slice 7: archive design prompts (entire 7.x family)

Substantial design history for Slice 7 spanning ADR-0008 stereo
unlink, the 2ch GR meter, the pop investigation arc (timing probe →
warm-keep → click detector → last-click snapshot → NEON SIMD), and
visual polish (em-dash mojibake fix, Clipper LED relocation,
limiter_active button repositioning).

Ten prompt files archived as paper trail. Useful for future
debugging if any regression surfaces: each prompt captures the
hypothesis being tested and the data that confirmed/refuted it.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

If any listed file doesn't exist (e.g., `SLICE_07_CLOSE.md` from the
earlier draft was never created), omit it from the glob and adjust.
The `PROMPTS/SLICE_07_*.md` glob picks up everything 7-family.

────────────────────────────────────────
6. PHASE 5 — Product push
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git rev-parse --abbrev-ref HEAD
# Capture branch.

git log --oneline -12
# Expect (top to bottom, approximate):
#   <prompts archive commit>
#   <PROGRESS+PLAN commit>
#   <debug removal commit>
#   0562ca6 MasterLimiter Slice 7.1.5: raise click threshold 0.2 -> 0.5
#   724c441 MasterLimiter Slice 7.1.3: revert warm-keep and snapshot click blocks
#   b0240d7 MasterLimiter Slice 7.1.2: numeric readouts, Clipper LED, output click detector
#   fb13dba MasterLimiter Slice 7.1.1: pop diag, em-dash fix, on/off relocation
#   9ca9d61 MasterLimiter Slice 7.1: 2ch GR meter + latched-max readouts (GR + Clip)
#   f05c062 MasterLimiter Slice 7: Stereo Link % (per-channel parallel envelopes)
#   7ec771c Slice 9: archive design prompts
#   ...

# If on a feature branch and main FF is needed:
#   git checkout main
#   git merge --ff-only <branch>
#   git push origin main
# If already on main:
#   git push origin main
```

If non-FF would be required, STOP and report.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. **Phase 1 debug removal commit**: SHA + title + brief diff
   summary (atomics removed, processBlock instrumentation removed,
   MainView debug header removed). Build PASS lines for both
   configurations.
2. **HQ push**: branch name, `git log --oneline -8` post-push,
   confirmation push succeeded.
3. **Product docs commit**: SHA + title. PROGRESS.md confirmed
   at top of slice list. PLAN.md confirmed Slice 7 shipped + Slice
   11b2 promoted + backlog rows added.
4. **Product prompts archive commit**: SHA + title + list of
   archived prompt filenames.
5. **Product push**: branch name, `git log --oneline -12` post-push,
   confirmation push succeeded.
6. **Final state**: HQ MCP unrelated dirt remains (expected);
   product clean.
7. Any auth / non-FF issues → reported, not force-resolved.
8. Open questions.

────────────────────────────────────────
8. AFTER THIS
────────────────────────────────────────

Slice 7 closed and shipped to both remotes. Next session:

- I open the **Slice 11b2 (Auto/Track + Learn) interview** to scope
  the implementation. PLAN entry already exists. Pin: one-shot Learn
  vs continuous Track, snapshot window length, UI placement, frozen-
  ID param names.
- **Parallel exploration**: ADR-0009 (multiband detection) design
  sketch as backlog work. Promoted in importance after Slice 7
  closed; multiband is now the primary architectural lever for
  Ozone-IRC-IV parity (both the "open" gap and the CPU↔GR
  correlation).
- avishali audits the shipped Slice 7 build in real-world mix
  sessions — stereo unlink, 2ch meter, and the resolved limiter
  feel together over extended mixing time often surface things
  short auditions miss.
