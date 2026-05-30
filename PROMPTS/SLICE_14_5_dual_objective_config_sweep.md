# Cursor Task — Slice 14.5: dual-objective config sweep (pumping vs steady-state)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Measurement/tuning slice, on the existing Slice 14 branches. No product
> source changes.** Slice 14.4's serial chain fixed the ceiling leak
> (28k → 2.8k SP overs), but the aggressive default config (kBandLink 0.0,
> headroom 2.0) over-processes broadband: on the steady SMPTE torture tone it
> raises IMD (11.3% vs single-band 8.45%) and leaves residual overs. 2-band
> IIR trades steady-state IMD for transient de-pumping; we must pick the
> `(kBandLink, headroom)` that keeps most of the pumping win while not
> wrecking steady-state. This slice measures a small grid against BOTH
> objectives (plus over magnitudes) so the architect can pick. **No threshold
> recalibration here. Do NOT commit, do NOT push.**

> **Continue on the existing Slice 14 branches**: product
> `slice-14-multiband-2band`, HQ `slice-14-multiband-bench`. No new branches.
> The macro overrides `MDSP_BAND_LINK` / `MDSP_BAND_HEADROOM_DB` from 14.3/14.4
> already enable build variants — no `Source/` edits needed.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- HQ `shared/dsp_bench/` edits permitted: one small measure addition (SP-over
  magnitude) + a combined report. No new bench framework.
- NO product `Source/` edits (build variants only, via `CMAKE_CXX_FLAGS`).
- No `mdsp_dsp` edits, no ADR edits.
- **Do NOT commit, do NOT push.** `MCP` stays dirty.

────────────────────────────────────────
1. TRINITY (retrieval)
────────────────────────────────────────

Read and log:
- `shared/dsp_bench/measure.py` — `sample_peak_overs`, `true_peak_overs`,
  `imd_smpte_pct`; the pumping measures `hf_ducking_modulation_db`,
  `per_band_gr_db` (from 14.2).
- `shared/dsp_bench/bench.py` — the Slice 3 runner and the pumping runner
  (`--pumping` / `--headroom-sweep-paths`).
- Confirm `MDSP_BAND_LINK` / `MDSP_BAND_HEADROOM_DB` macros exist and override
  cleanly (14.3/14.4).

Output the retrieval log.

────────────────────────────────────────
2. SP-OVER MAGNITUDE (small measure addition)
────────────────────────────────────────

Add to `measure.py` a helper that returns the **worst-case overshoot in dB
above the ceiling** (max sample magnitude over ceiling, dB), alongside the
existing over *count*. Purpose: distinguish harmless sub-0.1 dB downsample
ringing (treatment-B) from a real ceiling leak. Report it wherever
`sample_peak_overs` is reported in the Slice 3 runs below.

```python
def sample_peak_max_over_db(audio, ceiling_dbfs=-1.0) -> float:
    # 20*log10(max|sample|) - ceiling_dbfs ; <=0 means no over. Stereo: max channel.
```

────────────────────────────────────────
3. BUILD THE CONFIG GRID
────────────────────────────────────────

Build these Release variants (separate build dirs, `CMAKE_CXX_FLAGS`
overrides; confirm each override via CMakeCache + distinct SHA):

| build dir                | kBandLink | headroom |
|--------------------------|-----------|----------|
| build-rel-l00-h2         | 0.0       | 2.0      |
| build-rel-l00-h3         | 0.0       | 3.0      |
| build-rel-l05-h2         | 0.5       | 2.0      |
| build-rel-l05-h3         | 0.5       | 3.0      |
| build-rel-l025-h3        | 0.25      | 3.0      |

```bash
# example:
cmake -B build-rel-l05-h3 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-DMDSP_BAND_LINK=0.5f -DMDSP_BAND_HEADROOM_DB=3.0f"
cmake --build build-rel-l05-h3 -j
```

Also keep the **single-band baseline** (pre-Slice-14) and **Ozone** numbers
for reference (reuse prior runs if still valid, else re-measure).

────────────────────────────────────────
4. MEASURE BOTH OBJECTIVES PER CONFIG
────────────────────────────────────────

For each of the 5 grid builds, report:

**Transient (pumping):** `hf_ducking_modulation_db` on `pumping_kick_hf` at
matched ~3 dB GR (lower = less pumping; Ozone ≈ −22.80, single-band ≈ −17.48).

**Steady-state (Slice 3 quick):**
- `imd_smpte_pct` (single-band ≈ 8.45; Ozone ≈ 6.99).
- `sample_peak_overs` **count** AND `sample_peak_max_over_db` (magnitude).
- `true_peak_overs`.

Print ONE combined table:

```
config        link  hr   HF-duck   imd%    SP_overs  SP_max_over_dB  TP_overs
l00-h2        0.0   2    ...       ...     ...       ...             ...
l00-h3        0.0   3    -22.8     11.x    ...       ...             ...
l025-h3       0.25  3    ...       ...     ...       ...             ...
l05-h2        0.5   2    ...       ...     ...       ...             ...
l05-h3        0.5   3    ...       ...     ...       ...             ...
single-band   —     —    -17.48    8.45    <2200     ...             ...
Ozone         —     —    -22.80    6.99    0         ...             ...
```

────────────────────────────────────────
5. NO PICKING, NO RECAL
────────────────────────────────────────

Do NOT pick a config, do NOT change defaults, do NOT touch Slice 3/4/5
thresholds. This slice only produces the table. The architect reads it and
chooses the `(kBandLink, headroom)` trade, then a follow-up sets defaults +
does the minimal treatment-B recal + audition.

Flag in "open questions": which row you'd nominate and why (one line), but
leave the decision to the architect.

────────────────────────────────────────
6. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. The `sample_peak_max_over_db` measure diff.
3. Build list + override confirmation (5 variants).
4. **The combined dual-objective table** (§4).
5. Note whether the SP overs are sub-0.1 dB ringing (treatment-B) or larger.
6. `git status --short` product + HQ; list extra build dirs.
7. Open questions (incl. your one-line nomination).

────────────────────────────────────────
7. ARCHITECT REVIEW
────────────────────────────────────────

Architect reads the table and picks the shipped `(kBandLink, headroom)` —
the config that keeps the most transient de-pumping while holding steady-state
IMD/overs near single-band (treatment-B only for inaudible crossover
character). Then: a small follow-up sets the defaults, does the minimal Slice
3/4/5 recal, and goes to avishali's audition. **Do not self-pick, do not
recal, do not close.**
