# Cursor Task — Slice 9.6c: Ozone 11 Maximizer reference bench run (discovery only)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Bench-tooling addition only. No DSP source changes. No bench-criteria
> changes. No commits.** This slice runs Ozone 11 Maximizer through the
> existing dsp_bench harness at matched ceiling and matched target GR
> depth, so we can compare its SMPTE IMD / null residual / SP+TP overs
> against MasterLimiter's post-9.6 numbers on the same synthetic corpus.
>
> The decision this run informs: whether to invest in Slice 9.6b
> (envelope snap-event smoother to reduce IMD ~8.8% → reference range)
> or treatment-(B) the current numbers and close Slice 9 as-is.
>
> If reference shows IMD in the 1–3% range, our 8.8% is real artifact
> we should address with 9.6b. If reference shows 5–8%, the corpus is
> inherently hard and our number is in family — treatment (B) and close.
>
> **No commits. No criteria edits. New driver file is created and left
> uncommitted alongside the prompt files.**

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`. New bench
driver file only. No HQ DSP source. No product files. No commits in
this slice. No push.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

Post-9.6 bench shows three categories of failures vs the pre-9.2
baseline:

- **TP overs** in Slice 4/5: collapsed (cascaded-OS root cause
  resolved). Residual 604/914 is small treatment-B territory.
- **SP overs** in Slice 3 (SP mode), and **IMD / null residual** across
  all slices: unchanged from 9.5 baseline. These are properties of the
  limiter envelope's snap-on-attack + fast release, not of the OS
  chain.

The SMPTE IMD shift from 1.0% to 8.8% is the most suspicious. Two
plausible explanations:
1. **Fast-release inherent.** A 30 ms release envelope tracks the 60 Hz
   tone in SMPTE's 60+7k test, creating 7000±60 Hz sidebands. Slow-
   release envelopes don't track 60 Hz and score low. We chose fast
   release intentionally.
2. **Snap-event harmonic content.** Our smoothing stage's `s1 = inp;
   s2 = inp;` falling-edge snap has hard-step harmonics, mostly
   suppressed by 4× OS but residual content within the IMD measurement
   bandwidth (≤22 kHz) inflates the SMPTE number.

The way to tell them apart is to run a fast-release reference brickwall
(Ozone 11 Maximizer in IRC IV / "Modern" mode is the closest match) on
the same bench corpus. If Ozone scores ~1–3% IMD on this exact test
suite, fast-release is NOT the bottleneck and the snap event is the
source — 9.6b is necessary. If Ozone also scores 5–8%, the synthetic
corpus is the hard part and our number is in family — close Slice 9 as
treatment (B).

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**CREATE (HQ, do NOT commit):**
- `shared/dsp_bench/drivers/ozone_maximizer.py` — new driver mirroring
  the structure of `master_limiter.py` (same fixture functions: load
  plugin, pin params, target GR drive). See §3 for pin specifics.

**DO NOT TOUCH:**
- `shared/dsp_bench/drivers/master_limiter.py` — reference driver
  unchanged.
- `shared/dsp_bench/criteria.py` — no threshold changes this slice.
- `shared/dsp_bench/bench.py` — no harness changes.
- Any HQ DSP source.
- Any product file.

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

### 3.1 Locate Ozone 11 Maximizer VST3

Standard macOS paths to check (in order):

```
/Library/Audio/Plug-Ins/VST3/Ozone 11 Maximizer.vst3
/Library/Audio/Plug-Ins/VST3/iZotope/Ozone 11 Maximizer.vst3
/Library/Audio/Plug-Ins/VST3/Ozone 11/Ozone 11 Maximizer.vst3
~/Library/Audio/Plug-Ins/VST3/Ozone 11 Maximizer.vst3
```

If none of those exist, search:

```bash
find /Library/Audio/Plug-Ins -iname "*Maximizer*.vst3" 2>/dev/null
find ~/Library/Audio/Plug-Ins -iname "*Maximizer*.vst3" 2>/dev/null
```

If still nothing found, STOP and report — avishali confirms or supplies
the path.

### 3.2 Introspect Ozone's parameter surface

In the activated bench venv:

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate
python -c "
import pedalboard
p = pedalboard.load_plugin('<resolved Ozone path>')
print('Parameters:')
for name in dir(p):
    if not name.startswith('_') and name not in ('reset', 'process'):
        try:
            val = getattr(p, name)
            print(f'  {name} = {val!r}')
        except Exception as e:
            print(f'  {name} = <error: {e}>')
"
```

Capture the full output. Identify the parameters analogous to our:

- `ceiling_db` → Ozone's output ceiling (likely `threshold` or
  `ceiling`, exposed in dB).
- `input_gain_db` → Ozone's input drive (likely `threshold` if the
  product uses threshold-based control, or a separate input gain).
- IRC mode → set to "IRC IV" / "Modern" if exposed as a string param;
  this is the closest match to our fast-release brickwall.
- Channel link → set to 100% / fully linked for fair comparison with
  our linked detection. Disable if Ozone defaults to anything else.
- Learn / Stereo Unlink / Transient Emphasis → leave at default off.

If Ozone's param names don't map cleanly (e.g., it uses an internal
preset system instead of exposed params), STOP and report the
introspection output — we'll decide pins in a follow-up rather than
guessing.

### 3.3 Write the driver

Create `shared/dsp_bench/drivers/ozone_maximizer.py` following
`master_limiter.py`'s structure. Key responsibilities:

1. `load_plugin(plugin_path)` — returns the pedalboard plugin instance.
2. Pin the params per §3.2 decisions. Document each pin with a comment
   referencing the analogous MasterLimiter param.
3. `apply_target_gr(plugin, signal, target_gr_db)` — drive the input
   level until measured GR matches the target. Reuse the same drive
   loop as master_limiter.py's analogous function (probably called
   something like `drive_to_gr_depth` or similar — Cursor reads
   master_limiter.py and mirrors).

The driver must respect the bench's existing slice-fixture interface
so that `bench.py --driver ozone_maximizer --slice N` works identically
to the master_limiter driver.

### 3.4 Run Slice 3/4/5 against Ozone

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

OZONE=<resolved Ozone path>

for SLICE in 3 4 5; do
  python bench.py --driver ozone_maximizer --slice $SLICE --quick \
    --plugin-path "$OZONE" --output-dir "runs/SLICE09_6c_OZONE_S$SLICE"
done
```

For each slice, report per-row:
- metric name
- Ozone measured value
- MasterLimiter post-9.6 measured value (from the 9.6 rerun summary)
- delta (Ozone − MasterLimiter; negative means Ozone is better)

Focus rows for the decision:
- `imd_smpte_pct` in Slice 3 imd row and Slice 5 gr3/gr5 imd rows
- `null_residual_kweighted_db` pink rows in Slice 3 / 4 / 5
- `noise_residual_pct_pink` pink rows
- `sample_peak_overs_max` Slice 3 (SP path)
- `true_peak_overs_max` Slice 4 / 5 (TP path)

────────────────────────────────────────
4. NO COMMITS
────────────────────────────────────────

Do NOT `git add` anything. Do NOT `git commit`. The new driver file
stays as an uncommitted addition next to the existing uncommitted
prompt files. We decide its disposition (commit / keep local / refine
pins) after we see the numbers.

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Resolved Ozone VST3 path.
2. Introspection output (full param list, value types).
3. Param-mapping decisions: which Ozone param maps to which
   MasterLimiter equivalent, with the value pinned.
4. Driver file (paste contents or summarize structure).
5. Bench run output per slice — PASS/FAIL count and the focus-row
   table from §3.4.
6. Side-by-side comparison conclusions:
   - Is Ozone IMD in the 1–3% range or 5–8% range?
   - Is Ozone TP overs in the few-hundred range or low double digits?
   - Is Ozone null residual much tighter than ours, similar, or worse?
7. Confirmation: no commits made, no criteria edited, no DSP touched.
8. Open questions or anomalies (Ozone refusing to load, GR loop not
   converging, etc.).

────────────────────────────────────────
6. AFTER THIS
────────────────────────────────────────

Two-way decision based on Ozone numbers:

**If Ozone IMD is 1–3%:**
- Snap event is a real source of our artifact (we're 3–8× Ozone).
- Slice 9.6b is justified: short IIR smoother on the gain envelope
  output in LimiterEnvelope to round the snap. Expected drop to
  Ozone-family numbers.
- After 9.6b lands → re-run Slice 9.5 → close Slice 9.

**If Ozone IMD is 5–8%:**
- Synthetic SMPTE on dense pink + dense IMD signal is just a hard
  corpus; our numbers are in family.
- Treatment (B) the bench thresholds with rationale ("fast-release
  brickwall character; reference Ozone scores X% on identical
  corpus").
- Close Slice 9 without 9.6b. Snap smoother stays in the backlog for
  if avishali ever wants tighter numbers later.

**Either way:**
- Document the Ozone reference numbers in the Slice 9 close commit
  message for future-us.
- Decide whether to commit `ozone_maximizer.py` driver to HQ (helps
  future comparison runs) or keep it local (avoids dependency on a
  proprietary plugin path in shared tooling).
