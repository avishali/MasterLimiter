# Cursor Task — Slice 7b.2: M/S ceiling fix (decoded L/R was escaping the ceiling)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Correctness fix for a SHIPPED bug (7b is on `main`).** In M/S mode with
> Link < 100%, Mid and Side are limited independently then decoded
> (`L = M'+S'`, `R = M'−S'`) — the decoded L/R is never bounded, so it overshoots
> the ceiling (up to +6 dB worst case; ~+1 dB observed at Link < 100% +
> ceiling 0.0). Add a per-sample decoded-L/R safety bound in the M/S branch.
> Product fix + an M/S-mode overs bench test. **Do NOT push** — architect closes.

> **Ordering / branch:** this fixes `main`, independent of the in-progress
> auto-release work. First **preserve the uncommitted auto-release WIP**: on the
> `slice-auto-release` branch, `git add -A && git commit -m "WIP: auto-release
> (pop under investigation)"` (local only, no push) so the tree is clean. Then
> `git checkout main` and branch `slice-7b-2-ms-ceiling` off it. If the working
> tree can't be cleaned safely, STOP and report.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- Product edit: `Source/PluginProcessor.cpp` (M/S apply branch only).
- HQ edit (bench only): `shared/dsp_bench/` for the new M/S-mode overs test
  (sibling working copy). No `mdsp_dsp`/`mdsp_ui` source, no params, no ADR.
- RT-safe: per-sample arithmetic only, no allocation/branmerge weirdness.
- **Do NOT push.** Leave `MCP` untouched.

────────────────────────────────────────
1. THE FIX (PluginProcessor.cpp, M/S apply branch ~830–843)
────────────────────────────────────────

In the `else` (M/S) branch of the wideband apply, after the decode:

```cpp
outL[i] = mid + side;
outR[i] = mid - side;
```

add a per-sample decoded-L/R ceiling bound (gain, not clip), using the
existing `thresholdLin` (already carries the TP headroom in TP mode):

```cpp
const float decodedPeak = std::max (std::abs (outL[i]), std::abs (outR[i]));
float msSafetyGain = 1.0f;
if (decodedPeak > thresholdLin)
{
    msSafetyGain = thresholdLin / decodedPeak;
    outL[i] *= msSafetyGain;
    outR[i] *= msSafetyGain;
}
```

- Scaling **both** channels by the same factor preserves the stereo image at
  that instant; it only attenuates the overshoot.
- Confirm `thresholdLin` is in scope at this point (computed earlier in
  `processBlock`); if not, hoist it so the M/S branch can read it.
- **Fold into GR metering:** multiply the safety gain into the per-channel
  total so the meter reflects the extra reduction, e.g.
  `minTotalL = std::min (minTotalL, gDeepBand * gainWideL[i] * msSafetyGain);`
  (and R). Keep the existing `gDeepBand * gainWideR[i]` term, just × the safety
  gain.
- **Do NOT change the Stereo (`! useMsMode`) branch** — it already bounds L/R
  directly (each channel ≤ threshold). The default path stays bit-identical so
  Slice 3/4/5 remain unchanged.

────────────────────────────────────────
2. BENCH — M/S-mode overs test (close the coverage gap)
────────────────────────────────────────

The default bench runs Stereo mode, which is why this escaped. Add a test that
drives the plugin in **M/S mode at `ms_link_pct = 0`, ceiling 0.0 dB**, with a
decorrelated/wide stimulus pushed hard, and asserts **zero sample-peak and
true-peak overs** above the ceiling on the output. Follow the existing
`dsp_bench` driver/criteria structure; the master_limiter driver must be able
to set `stereo_mode` and `ms_link_pct` (add that if the driver can't yet).
Report the over counts before the fix (should fail) vs after (should pass) if
you can capture both; otherwise just after.

────────────────────────────────────────
3. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout -b slice-7b-2-ms-ceiling     # off main, after WIP-committing auto-release
cmake --build build-debug -j
cmake --build build-release -j
```
- Zero new `Source/` warnings.
- **Default (Stereo) bit-identical:** Slice 3/4/5 quick PASS unchanged.
- **New M/S overs test passes** (no overs in M/S mode at Link 0 / ceiling 0).
- In a host: M/S mode, Link < 100%, ceiling 0.0, push hard → output no longer
  exceeds 0 dBFS (verify with an external meter). Stereo mode unchanged.

────────────────────────────────────────
4. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Diff: the M/S decoded-L/R safety bound + GR-metering fold-in.
3. Bench: the new M/S overs test + result (and the WIP-commit SHA on
   slice-auto-release that preserved the auto-release work).
4. Build summary (Debug + Release, warnings).
5. Slice 3/4/5 quick: PASS unchanged (Stereo default bit-identical).
6. `git status --short` product (slice-7b-2 branch) + HQ sibling.
7. Open questions.

────────────────────────────────────────
5. ARCHITECT REVIEW + AUDITION
────────────────────────────────────────

avishali confirms M/S mode no longer overshoots the ceiling (external meter)
and the edge-case artifact is acceptable. On approval → architect closes
Slice 7b.2 (product + bench + submodule bump + PROGRESS/PLAN + prompt archive),
noting the fully-transparent final-ceiling stage as future polish. **Do not
self-close.**
