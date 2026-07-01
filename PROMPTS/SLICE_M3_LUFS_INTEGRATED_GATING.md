# SLICE M3 — Integrated LUFS: add BS.1770-4 gating (fix the systematically-low "I")

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition/decide:** avishali
**Repos:** SDK `melechdsp-hq` (`mdsp_dsp::LoudnessAnalyzer`) + plugin submodule bump. **Commit SDK first.**
**Reference:** `docs/METERING_ACCURACY_AUDIT.md` finding **6**. K-weighting + M/S windows are already correct — this is the **Integrated** path only.

---

## Why
The **Integrated** LUFS is a plain cumulative mean-square average since reset (`LoudnessAnalyzer.cpp:105-107,145-151`) — it has **neither** the BS.1770-4 **absolute gate (−70 LUFS)** nor the **relative gate (−10 LU below the ungated mean)**. Consequence: any silence/quiet content in the measured span drags "I" **systematically low, often > 1 dB** vs any compliant meter (WLM, Insight, youlean). For a mastering tool the integrated number must be standards-compliant.

## Allowed files
```
# SDK (sibling checkout melechdsp-hq/)
shared/mdsp_dsp/include/mdsp_dsp/loudness/LoudnessAnalyzer.h
shared/mdsp_dsp/src/loudness/LoudnessAnalyzer.cpp
shared/mdsp_dsp/tests/*Loudness*         (add a gating test)
# Plugin
third_party/melechdsp-hq                  (submodule bump at close)
docs/METERING_ACCURACY_AUDIT.md (tick M3)  docs/PROGRESS.md  PROMPTS/PLAN.md
PROMPTS/SLICE_M3_LUFS_INTEGRATED_GATING_CLOSE.md  (new)
```
**Non-goals / STOP:** do **not** touch K-weighting or the Momentary/Short-term paths; no product DSP change beyond consuming the corrected integrated value.

## Algorithm — BS.1770-4 §4 gating (Integrated only)
Replace the cumulative-mean integrated computation with **gated** loudness over **400 ms gating blocks with 75 % overlap** (i.e. a new block every 100 ms):
1. Maintain a rolling list of per-**gating-block** mean-square values `z_j` (K-weighted, already available; accumulate 400 ms of the running mean-square, hop 100 ms). Each block's loudness `l_j = −0.691 + 10·log10(z_j)`.
2. **Absolute gate Γ_a = −70 LUFS:** keep only blocks with `l_j ≥ −70`.
3. **Relative gate:** compute the mean-square average over the absolute-gated set → `l_relgate = −0.691 + 10·log10(mean(z_j over kept)) − 10` (i.e. 10 LU below the ungated-but-abs-gated loudness).
4. **Integrated = −0.691 + 10·log10( mean(z_j) over blocks with `l_j ≥ Γ_a` AND `l_j ≥ l_relgate` )**.
5. If no block passes the absolute gate → report `−inf`/floor (silence).

Implementation notes:
- Store `z_j` in a preallocated ring (bounded, e.g. cap the block count for the max measurement span; document the cap). RT-safety: the analyzer already runs off the audio thread / on the metering path — keep it allocation-free after `prepare()`.
- Keep the existing Momentary (0.4 s) and Short-term (3 s) exactly as-is; only the **Integrated** snapshot field changes.
- Reset clears the block list.

## Build, verify, close
```bash
# SDK
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq && cmake --build build 2>&1 | tail -6
./build/shared/mdsp_dsp/tests/mdsp_dsp_tests "*Loudness*" 2>&1 | tail
# Plugin (after submodule bump)
cd ../MasterLimiter && cmake --build build 2>&1 | tail -6 && auval -v aufx MaLm Melc 2>&1 | tail -3
./scripts/install_user.sh build 2>&1 | tail -3
```
**Acceptance (Claude verifies 1–3; avishali auditions 4):**
1. SDK tests pass incl. a new gating test: a signal with a loud section + silence → integrated matches the **gated** reference (not the silence-dragged mean); a −70 dB check; a known-value check against a compliant reference (e.g. a −23 LUFS EBU test tone reads −23.0 ± 0.1).
2. SDK committed/pushed first; submodule bumped; plugin builds clean; auval PASS; installed.
3. Momentary/Short-term unchanged (regression).
4. **Audition:** "I" now agrees with youlean/Insight on a real master to within ~0.1–0.3 LU (was reading low).

**Close gate:** ADR note if warranted; `docs/METERING_ACCURACY_AUDIT.md` (mark M3); `docs/PROGRESS.md`; `PROMPTS/PLAN.md`; commit SDK→submodule bump→plugin; archive CLOSE.
