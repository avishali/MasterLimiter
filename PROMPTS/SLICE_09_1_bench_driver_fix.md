# Cursor Task — Slice 9.1: bench driver — pin character to "Clean" for Slice 3/4/5 regression

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **HQ-side bench-driver fix only — NO product repo changes.** Slice 9
> expanded the `character` choice from 1 option to 3, and the bench
> driver's existing `"character": 0` no longer reliably selects Clean
> via pedalboard (pedalboard expects the option name as a string for
> choice params). Fix the driver, re-run Slice 3/4/5 regression, then
> a separate HQ commit. **Do NOT push HQ.** Product repo untouched —
> the Slice 9 product close prompt will run after this.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:
- HQ bench file only. No DSP edits. No product edits.
- **Do NOT push HQ. Do NOT amend the previous HQ commit (`bbd4293`).**
- Stay where you are; do not touch product branch state.

────────────────────────────────────────
1. WHY
────────────────────────────────────────

Slice 9's `character` expansion (1 → 3 options) means the bench
driver's existing pin `"character": 0` doesn't select Clean reliably
under pedalboard, which exposes AudioParameterChoice as a string.
Result: Slice 5 bench ran with whatever pedalboard's default mapping
gave (Tight), failing 24/25. Slice 3 / 4 happened to pass either by
coincidence of their drivers' settings or by mapping fluke.

Fix: pin character by the option **name** ("Clean") in the driver, so
the regression always runs against Clean regardless of choice-list
size or default-index shifts.

────────────────────────────────────────
2. SCOPE — files
────────────────────────────────────────

**MODIFY (HQ only):**
- `shared/dsp_bench/drivers/master_limiter.py` — line ~38: change
  `"character": 0` to `"character": "Clean"`.

Do NOT touch anything else. No product files. No other bench files.

────────────────────────────────────────
3. WHAT TO DO
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
# Edit shared/dsp_bench/drivers/master_limiter.py:
#   - "character": 0,
#   + "character": "Clean",
```

(If the driver's parameter-setter logic already supports passing
either int or string for choice params, the string form is the
correct one going forward. If the driver currently coerces to int,
adjust to accept string for the choice case — minimal touch only.)

Re-run the regression:

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench
source .venv/bin/activate

PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3

for SLICE in 3 4 5; do
  python bench.py --driver master_limiter --slice $SLICE --quick \
    --plugin-path "$PLUGIN" --output-dir "runs/SLICE09_1_REGRESSION_S$SLICE"
done
```

All three must end `PASS N/N` (Slice 3: 13/13, Slice 4: 14/14,
Slice 5: 25/25).

────────────────────────────────────────
4. HQ COMMIT (separate from `bbd4293`, no push)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git status --short
# expect:
#    M shared/dsp_bench/drivers/master_limiter.py
# (no other changes)

git add shared/dsp_bench/drivers/master_limiter.py

git commit -m "$(cat <<'EOF'
dsp_bench/master_limiter driver: pin character by name ("Clean")

Slice 9 expanded MasterLimiter's character choice from 1 option to 3
(Clean/Tight/Aggressive). Pedalboard exposes AudioParameterChoice as a
string attribute, so the previous integer pin "character": 0 no longer
reliably selected Clean under the expanded list. Pin by name instead so
Slice 3/4/5 regression keeps running against Clean regardless of future
choice-list or default-index changes.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

Do **NOT** push HQ. Do **NOT** amend the previous HQ commit
(`bbd4293`). This is a separate, self-contained bench-driver fix.

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. The exact line change (before/after).
2. Bench regression: invocation + final `PASS N/N` line for each of
   Slice 3 / 4 / 5.
3. HQ `git log --oneline -3` showing the new commit on top of
   `bbd4293` (which is on top of `276c397` from Slice 11a).
4. Confirmation: HQ NOT pushed; product repo NOT touched.
5. Open questions.

────────────────────────────────────────
6. AFTER THIS
────────────────────────────────────────

avishali re-auditions Slice 9 in Live (Release VST3) for the Character
modes + Clipper Drive + limiter on/off smoke checks. On approval, the
Slice 9 close prompt runs (single clean product commit), then I FF main
+ push.
