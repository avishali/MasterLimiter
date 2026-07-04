# SLICE — M/S per-band (opt-in): encode→limit→decode inside the 3-band stage

**Status:** closed 2026-07-04 · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali (Asaf on DEV build)
**Repos:** plugin `MasterLimiter` only. No SDK edit, no submodule bump (`LimiterEnvelope` already has everything).
**Companion:** `docs/SIGNAL_FLOW.md` §2.9–2.14. **ADR:** extends ADR-0009 §4 — Claude adds the ADR note at close.
**Arc position:** the M/S-per-band slice foreshadowed in `SLICE_PERBAND_STEREO_UNLINK.md` notes (lines 103, 127): *"encode→unlink→decode inside the band stage (wideband still untouched), flip meter labels to M/S."* This is that slice.

> ⚠️ **First step — retrieval log (do not skip).** The line numbers below are from a mapping pass and **will have shifted**. Read `PluginProcessor.cpp` `processCore` and re-confirm every cited location before editing. Output a retrieval log of the actual current lines.

---

## Why

Today "M/S mode" (`stereo_mode == M/S`) is a **half-measure**: only the **wideband** final stage runs Mid/Side (encode [PluginProcessor.cpp:1701](Source/PluginProcessor.cpp:1701), decode [:1798](Source/PluginProcessor.cpp:1798)). The **3 bands stay L/R**, mono-linked: in M/S mode `bandUnlink = false` ([:1400](Source/PluginProcessor.cpp:1400)) so each band detects `max(|L|,|R|)` → one gain ([:1426–1458](Source/PluginProcessor.cpp:1426), [:1510](Source/PluginProcessor.cpp:1510)). There is **no per-band M/S**.

This slice adds an **opt-in** per-band M/S path: when M/S mode is on **and** a new DEV toggle is enabled, each band encodes its L/R to Mid/Side, limits M and S with an independent per-band M/S link, and decodes back to L/R before recombination. The wideband stage is untouched. It **reuses the exact two-channel band machinery** already built for per-band stereo unlink (Slice A2) — fed Mid/Side instead of L/R.

**The crossover is linear-phase (linear):** `xover(0.5(L+R)) = 0.5(xover(L)+xover(R))`, so encoding L/R→M/S at the band-path input and running the existing per-channel detect+apply on (M,S) as channels {0,1} is mathematically identical to encoding after the split. Use that — it makes M/S a drop-in of the A2 path.

---

## Allowed files
```
Source/PluginProcessor.h / PluginProcessor.cpp
Source/parameters/ParameterIDs.h  Source/parameters/Parameters.cpp
Source/ui/DevControlsComponent.h / .cpp          # Band M/S toggle + Band M/S Link slider
Source/ui/meters/GainReductionMeter.h / .cpp     # relabel L/R → M/S sub-bars when active
docs/SIGNAL_FLOW.md  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_MS_PERBAND_CLOSE.md                (new, at close)
```

**Non-goals / STOP:**
- **Wideband stage untouched** — it keeps doing its own M/S encode/decode + `dev_ms_safety_clamp` on `bandLimitedBuf_` (which stays L/R). Do not double-encode.
- **No new frozen IDs.** Both new params are `dev_`-prefixed.
- **No latency change, no new envelopes/buffers.** The band M/S path **reuses** the A2 two-channel envelopes (`envelopeLow_`/`envelopeLowR_`, Mid, High) and their gain buffers — band-stereo-unlink and band-M/S are **mutually exclusive** (one needs Stereo mode, the other M/S mode), so the same R-slot resources serve both.
- **No change to the Stereo-mode band path** (A2) or the band-color/`band_color` inter-band blend.
- No meter window/layout change (same footprint as A2's L/R sub-bars — just relabel).

---

## Params (2 new DEV params)

`ParameterIDs.h` + `Parameters.cpp` (mirror existing `dev_` params; cache raw pointers + `jassert` in ctor and `prepareToPlay` exactly like `devBandStereoLinkPct_`):

1. **`dev_band_ms`** — `AudioParameterBool`, display **"DEV Band M/S"**, default **false**.
2. **`dev_band_ms_link_pct`** — `AudioParameterFloat` `0..100` step 1, display **"DEV Band M/S Link"**, default **100**.

**Default `dev_band_ms=false` ⇒ every existing path is bit-identical (Stereo, M/S, mono, all links).** This is the null.

---

## DSP — generalize the two-channel band path to a domain (A/B)

Engagement flags (near [:1399–1400](Source/PluginProcessor.cpp:1399)):
```cpp
const bool useMsMode  = stereoMode_->getIndex() == 1 && nch > 1;      // existing
const bool bandUnlink = (! useMsMode) && (nch > 1);                    // existing — Stereo two-chan path
const bool bandMs     = useMsMode && (nch > 1)
                        && devBandMs_ != nullptr && devBandMs_->load() > 0.5f;  // NEW
const bool bandTwoChan = bandUnlink || bandMs;                         // run the per-channel machinery
```
`bandUnlink` and `bandMs` are mutually exclusive (opposite `useMsMode`). The existing **mono-linked** path ([:1426–1458](Source/PluginProcessor.cpp:1426) + [:1510](Source/PluginProcessor.cpp:1510)) runs whenever `!bandTwoChan` — i.e. Stereo-linked-fast, mono, **and M/S with the toggle off (today's default)**.

For the two-channel path, parameterize by domain:
- **Stereo (`bandUnlink`):** channels A=L, B=R. Link = `dev_band_stereo_link_pct`. **No** encode/decode. (Exactly the A2 path — keep bit-identical.)
- **M/S (`bandMs`):** channels A=Mid, B=Side. Link = `dev_band_ms_link_pct`. **Encode** the band-path input L/R→M/S; run the *same* per-channel detect ([:1459–1489](Source/PluginProcessor.cpp:1459)), per-channel envelopes + link blend ([:1527–1551](Source/PluginProcessor.cpp:1527)), and per-channel band-color reconstruction ([:1636–1684](Source/PluginProcessor.cpp:1636)) with A/B = M/S; then **decode** each `bandLimitedBuf_` write M/S→L/R.

### Encode / decode (stateless per-sample, RT-safe)
```
encode:  M = 0.5f*(L + R);   S = 0.5f*(L - R);
decode:  L = M + S;          R = M - S;
```
Cleanest wiring (leverages crossover linearity): when `bandMs`, feed **M into channel 0 and S into channel 1** of `detectCrossover_`/`detectXoMidHi_` (detection) **and** of `applyCrossover_`/`applyXoMidHi_` (reconstruction), i.e. encode the per-sample L/R to M/S at the point each channel enters those filters. The entire A2 per-channel path then runs unchanged on (M,S). At the **final `bandLimitedBuf_` write**, the two channel results are limited-M and limited-S → **decode** to L/R and store. Because a single scalar gain on M,S decodes to the same scalar on L,R, this is exact.

### Fast path (mirror `bandFast`)
`bandMs` with link ≥ 99.95% → single envelope on `max(|M|,|S|)` (one gain applied to both M and S). Note: this is **M/S-linked**, deliberately *not* identical to today's L/R-linked M/S (`max(|L|,|R|)`) — that difference is the feature and only appears with the toggle **on**.

### Safety
Band M/S decode can push `bandLimitedBuf_` L/R peaks up, but the **downstream wideband M/S stage re-limits** and its `dev_ms_safety_clamp` + `ceilingLin` + final ceiling guarantee TP ≤ ceiling. **No new per-band clamp.** (Acceptance verifies TP at Band M/S Link 0.)

> No latency change (same filters/envelopes/lookahead). Extra work (two envelopes + encode/decode) only when `bandMs` and link < fast — same cost profile as A2's Stereo unlink.

---

## Meter — relabel L/R → M/S when band-M/S is active (`GainReductionMeter`)

Publish a new atomic `bandMsActive_` (bool) from the processor (true only when `bandMs`). Per-band GR taps ([:1856–1887](Source/PluginProcessor.cpp:1856)) already carry two channels — in the `bandMs` path, the A-slot (`...LDb_`) = **Mid** band GR, B-slot (`...RDb_`) = **Side** band GR (from the M/S gains). No new atomics for values; reuse the A2 L/R per-band atomics.
- Meter reads `getBandMsActive()`; when true, the per-band sub-bar micro-labels read **M / S** instead of **L / R** (group captions LO/MID/HI unchanged). When false, **L / R** as today.
- History traces (3, band max of the two channels) unchanged — `max(gM,gS)` in the M/S path.

---

## Build, verify, close

```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build 2>&1 | tail -8
auval -v aufx MaLm Melc 2>&1 | tail -5
```

**Acceptance (Claude verifies 1–6; avishali/Asaf audition 7):**
1. Build clean, no new warnings; AU validates; **no latency change** vs current HEAD.
2. **Global null — `dev_band_ms = false`: bit-identical to HEAD in ALL modes** (Stereo @ every Band Stereo link, M/S @ every M/S link, mono). This is the headline gate — the toggle-off path must not touch a sample.
3. **Stereo mode unaffected** at any `dev_band_ms` value (band M/S only engages in M/S mode).
4. **Engage (M/S mode + `dev_band_ms` on, Band M/S Link 100):** bands run M/S-linked; verify Mid/Side detection (a hard-panned transient now drives the **Side** band, not just a max) — differs from toggle-off, no TP breach.
5. **Independence (M/S mode + toggle on, Band M/S Link 0):** a loud centred low element reduces the **Mid** low band without ducking the **Side** low band; sub-bars relabel **M/S** and track independently.
6. **True-peak ≤ ceiling** across Band M/S Link 0/50/100 (M/S mode, toggle on), SP + TP — proves the wideband stage catches decode overshoot.
7. **Audition:** M/S-per-band feel on a wide mix; live-toggle click-safety (smooth the link value if a hard flip zippers; the `dev_band_ms` switch should be click-safe — if it clicks, note it for a follow-up crossfade).

**Close gate:** ADR-0009 note (per-band M/S rung); update `docs/SIGNAL_FLOW.md` §2.9–2.14 (annotate the band-stage M/S domain branch) + §7 param table; `docs/PROGRESS.md`; `PROMPTS/PLAN.md`; commit **plugin-only, do not push** (Quell hold); archive `SLICE_MS_PERBAND_CLOSE.md`.

---

## Output requirements (for Cursor)
1. Retrieval log (actual current lines for every cited location).
2. Param diff (`dev_band_ms`, `dev_band_ms_link_pct`).
3. DSP diff (A/B domain generalization; encode/decode; fast-path; flags).
4. Meter diff (M/S relabel + `bandMsActive_`).
5. Build + auval summary.
6. Null evidence: toggle-off bit-identical (state how tested — e.g. offline render null or bench Slice 3/4/5 unchanged).
7. `git status --short` + commit hash (no push).
8. Open questions (live-toggle click; meter label legibility; anything ambiguous in the reconstruction).

## Notes for the architect (not for Cursor)
- The whole slice is a **transposition of A2**: same two-channel envelopes/buffers/link-blend/reconstruction, chosen by domain. If Cursor finds itself adding new envelopes, it's over-building — the R-slot resources are free in M/S mode (Stereo unlink can't be active simultaneously).
- Null discipline mirrors A2/7b: the toggle-off path is the guarantee; toggle-on @ link 100 is legitimately M/S-detected (not an L/R null), by design.
- Meter M/S relabel is the last piece of the A2 note. After this, per-band viz is domain-correct in both modes.
