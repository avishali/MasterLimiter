# SLICE SMART-3P.1 — the gate verdict is confounded; re-ask it with a CAUSAL oracle

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **No audition** (offline study)
**Scope:** **`tools/analysis/mbl_depth_oracle.py` only. NO plugin, NO SDK, NO C++.**
**Follows SMART-3P.** Its Stage A said "PROCEED"; **that verdict does not stand yet.**

---

## Why the +2.380 does not authorise C++

Cursor's own open question is the reason, and it is correct:

> *"Broadband vs 2-band confound. Oracle optimizes a single linked g(t); Smart is 2-band. Part of the
> Stage A win may be topology, not pure depth redistribution. live-show's broadband reconstruction
> already beats plugin Smart before optimizing (≈3.1 vs 4.4)."*

So on live-show the measured Δ of **+3.771** decomposes into roughly **+1.3 from topology** (broadband
reconstruction vs the 2-band plugin path) and **+2.5 from the optimisation**. The gate compared the oracle
against the *wrong baseline* — it scored "different topology + non-causal optimisation" and attributed all
of it to depth redistribution. We only have that decomposition for one source.

**And Stage B is the more important signal:** the causal law was *worse than Smart on every source*
(7.860 vs 3.713). Zero of the oracle's headroom was harvested. Two readings are possible:

1. the causal sketch was simply a poor controller, or
2. **the oracle's advantage is intrinsically non-causal** — it wins by seeing the whole file, which no
   limiter can ever do.

Reading 2 would mean axis 3 is unbuildable as specified, and no amount of C++ would recover it. That is
exactly the P-A failure mode (`e7ad6c2`) — a promising offline result that dies on contact with causality —
and it is what this slice must rule in or out **before** anyone writes DSP.

## The two experiments

### Experiment 1 — isolate topology from depth (fixes the baseline)

Add the missing control: the **unoptimised broadband reconstruction** (the same `x · g(t)` path with the
fixed-law gain, no optimisation) as a baseline row, on **all four sources**.

Report three columns: `plugin Smart` · `broadband unoptimised` · `oracle`.

```
Δ_topology  = plugin Smart        - broadband unoptimised
Δ_depth     = broadband unoptimised - oracle        <- THIS is what axis 3 is worth
```

Only **Δ_depth** is evidence for axis 3. Δ_topology is a different finding (and if it is consistently
large it is interesting on its own — it would say our 2-band reconstruction is costing us).

### Experiment 2 — the CAUSAL ORACLE (the decisive one)

Re-run the *same optimiser*, but the gain at time `t` may depend only on the signal up to `t + LA`:

- optimise the same objective, same hard peak gate, same equal-loudness constraint;
- but constrain each control point so it uses no information beyond its lookahead horizon
  (simplest defensible implementation: optimise on a sliding window of length `LA`, advancing causally,
  each window solved independently with the previous window's endpoint as the initial condition);
- run at **LA = 5, 10, 20, 50 ms**.

This is the **upper bound on what ANY causal controller can achieve** — better than any real
implementation, because it is still an optimiser rather than a heuristic. It is the number that decides
whether axis 3 is buildable.

## Decision gate — on Δ_depth of the CAUSAL oracle, not the non-causal one

| causal-oracle Δ_depth vs plugin Smart, corpus mean | verdict |
|---|---|
| **< 0.5** | **axis 3 is NOT buildable at these lookaheads. STOP.** Report it and the programme moves to axis 2. This is a legitimate, valuable outcome. |
| **0.5 – 1.5** | marginal — architect + avishali decide, weighing it against the latency cost of a longer lookahead |
| **> 1.5** | build it. Report which LA is needed, because that is a latency decision for avishali (reported latency is fixed at the max across all configs — CLIP-1.1 — so a longer lookahead costs *every* user). |

Also report the **non-causal** oracle alongside, so the causal/non-causal gap is explicit. A large gap is
itself the finding: it quantifies how much of "adaptive depth" is fundamentally unreachable.

## Rules (unchanged, and non-negotiable)
- Peak ceiling is a **hard gate**: any candidate over −1.00 is invalid and reported as invalid, not scored.
- **Equal loudness**, or the comparison is meaningless.
- Full 4-source corpus, matched by actual GR. **Report `live-show` separately** — it is our weakest source
  and the one where the oracle helped most (+3.77).
- Reuse `mbl_pump.added_modulation` / `mbl_voicing.load` / `gr_of`. Do not invent a second metric.
- **Do not tune the plugin-Smart baseline.** It is SMART-1.1 defaults, exactly as shipping.

## Non-goals
- No plugin/SDK code. If the gate passes, the C++ slice is SMART-3 and gets specced separately.
- Do not try to improve the Stage B heuristic from SMART-3P — replacing it with a proper causal *oracle*
  is the point. A better heuristic tells us nothing about the ceiling.

## Output requirements
1. Retrieval log. 2. Experiment 1 table (3 columns x 4 sources) with Δ_topology and Δ_depth separated.
3. Experiment 2 table (causal oracle at 4 lookaheads) + the non-causal oracle for reference.
4. **The gate verdict, stated plainly**, including "STOP" if that is what the number says.
5. Confirm no plugin/SDK files touched. 6. Open questions.
