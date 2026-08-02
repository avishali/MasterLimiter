# SLICE SMART-3P — adaptive depth: the ORACLE prototype (Python only, no DSP)

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **No audition** (offline study)
**Scope:** **`tools/analysis/` only. NO plugin code. NO SDK code. NO C++.**
**Frame:** axis 3 of `docs/PROGRAM_DEPENDENT_ENGINE.md`; validation-first per `ADAPTIVE_THRESHOLD_ENGINE.md` §8.

---

## Why a prototype and not a slice

Axis 3 is the highest-value axis — adapting *how hard to limit* is the thing no reference does — and the
highest-risk. **P-A (`e7ad6c2`, July) came back NEGATIVE after C++ work had already begun.** That is the
mistake this slice exists to not repeat.

Before anyone writes DSP, answer one question:

> **What is the maximum achievable improvement from redistributing gain reduction, and how much of it
> survives a causal, lookahead-limited implementation?**

If the offline oracle only beats the fixed law by a little, axis 3 is not worth building and we stop —
that is a successful outcome for this slice, not a failure.

## The idea

Today the reduction lands wherever the signal exceeds a fixed threshold. Axis 3 says: **at the same output
loudness and the same peak ceiling, put the reduction where it is least audible.**

Our objective already *is* an audibility proxy — added envelope modulation
(`|MACRO| + |PUMP| + |ROUGH|`, `mbl_pump.py`). So the engine should optimise the same thing we measure.

## Two stages — do them in order and report after each

### Stage A — the ORACLE (non-causal, offline, an upper bound)

Not an engine. A convex-ish offline search for the best possible gain envelope, allowed to see the whole
file. It answers "how much is on the table at all".

```
find g(t) >= 0                                   (a per-sample gain envelope, smooth)
minimise   |MACRO| + |PUMP| + |ROUGH|            (added modulation vs the dry source)
subject to peak(x(t) * g(t)) <= ceiling          HARD -- never traded
           RMS(x * g) == RMS(fixed-law output)   equal loudness, so this is not just "limit less"
```

- Start with the fixed-law gain envelope as the initialisation, then optimise (scipy is fine; a simple
  projected-gradient or coordinate refinement over a *decimated* control-point grid — e.g. one control
  point per 10 ms, interpolated — is enough; do NOT optimise per-sample directly).
- **The equal-loudness constraint is what makes this hard and what makes it honest.** Without it the
  optimiser trivially "wins" by limiting less, which is not a result.
- Report per source: oracle score vs `OPEN+Smart tuned` (the current best, mean **3.956**) and vs
  `Pro-L 2 Allround` (**4.507**).

**Decision gate after Stage A — report and STOP for the architect:**
- Oracle beats tuned Smart by **< 0.5** on the corpus mean → **axis 3 is not worth building.** Say so.
- Oracle beats it by **> 1.5** → strong case, proceed to Stage B.
- In between → architect's call with avishali.

### Stage B — the CAUSAL approximation (only if Stage A justifies it)

Same objective, but the gain at time `t` may only use the signal up to `t + lookahead`
(`dev_mb_lookahead_ms`, currently 5 ms; also try 10 and 20 ms to price the latency trade).

- This measures how much of the oracle's headroom a real implementation can actually reach.
- Report: causal score vs oracle vs tuned Smart, per source, at each lookahead.
- **Scale the adaptation by `f(GR_depth)`** per `PROGRAM_DEPENDENT_ENGINE.md` §7.2 — at low GR the
  output must converge to the fixed law, so the engine cannot hurt light use.

## Rules (these are what make the result trustworthy)

- **Peak ceiling is a hard gate, never traded.** Any candidate whose sample peak exceeds −1.00 is invalid
  and must be reported as invalid, not scored. Letting peaks through is how the old 6.39 edm figure
  happened and it is the single most repeated failure mode in this project.
- **Equal loudness, or the comparison is meaningless.**
- **Full 4-source corpus** (`mbl_voicing.CORPUS`), matched by actual GR, same as every other measurement.
- **Report `live-show` separately in the headline.** It is consistently our weakest source and the one
  where tuning made things worse — if the oracle cannot help there either, that is a finding about the
  material, not the method.
- Reuse `mbl_pump.added_modulation` / `mbl_voicing.load` / `gr_of` — **do not write a second metric.**

## Deliverable
`tools/analysis/mbl_depth_oracle.py` + the two reports pasted into the close note. No other file changes.

## Non-goals
- **No plugin or SDK code whatsoever.** If Stage A says go, the C++ slice is SMART-3 and is specced separately.
- Do not invent a new objective or a new corpus.
- Do not tune the fixed-law baseline to make the oracle look better — the baseline is `OPEN+Smart` at the
  SMART-1.1 defaults, exactly as shipping.

## Output requirements
1. Retrieval log. 2. Stage A method (what you optimise over, how many control points, which solver).
3. Stage A table + the decision-gate verdict, stated plainly. 4. Stage B only if the gate passes.
5. Confirm no plugin/SDK files touched. 6. Open questions.
