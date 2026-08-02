#!/usr/bin/env python3
"""SMART-3P — adaptive depth ORACLE (Python only; no plugin/SDK/C++ changes).

Question this answers before any DSP is written:

  What is the maximum achievable improvement from redistributing gain reduction
  at equal loudness and a hard peak ceiling — and (Stage B) how much of that
  headroom survives a causal, lookahead-limited implementation?

Stage A is a non-causal upper bound. Stage B runs only if Stage A beats tuned
OPEN+Smart by > 1.5 on the corpus-mean |MACRO|+|PUMP|+|ROUGH|.

Baseline: OPEN + Smart at SMART-1.1 defaults (fast 40 / slow 300 / sustain 450 / leak 0.15).
Metric: mbl_pump.added_modulation — do NOT invent a second objective.
Corpus: mbl_voicing.CORPUS, matched to GR_TARGET dB of actual RMS-GR.

Usage:
    ./.venv/bin/python mbl_depth_oracle.py              # Stage A, then Stage B if gate passes
    ./.venv/bin/python mbl_depth_oracle.py --stage A
    ./.venv/bin/python mbl_depth_oracle.py --stage B    # force Stage B (ignore gate)
"""
from __future__ import annotations

import argparse
import time

import numpy as np
from scipy.optimize import minimize

import mbl_pump as P
from mbl_voicing import CORPUS, GR_TARGET, load, gr_of

CEIL_DB = -1.0
CEIL_LIN = 10.0 ** (CEIL_DB / 20.0)
CTRL_HOP_S = 0.020          # 20 ms control grid (metric is 10 ms; 20 ms is enough for an upper bound)
N_SPLINE = 64               # global spline knots for the fast oracle pass
SPLINE_MAXITER = 60
BLOCK_S = 4.0               # light block polish
BLOCK_PASSES = 0            # polish off by default; spline upper bound is the Stage A answer
BLOCK_MAXITER = 8
RMS_TOL_DB = 0.05           # equal-loudness acceptance
SMOOTH_PENALTY = 1.0e-3     # discourage zipper in the control grid (does not replace the metric)

# SMART-1.1 shipping Smart defaults — set explicitly every render (no cache carry-over).
SMART_FAST = 40.0
SMART_SLOW = 300.0
SMART_SUSTAIN = 450.0
SMART_LEAK = 0.15


# ---------------------------------------------------------------- rendering
def render_open_smart(x, sr, gain):
    p = P._plug(P.OURS)
    names = set(p.parameters.keys())

    def put(a, v):
        if a not in names:
            raise KeyError(f"{a!r} missing — silent no-op would misconfigure the oracle")
        setattr(p, a, v)

    put("dev_mb_engine", True)
    put("dev_mb_release_engine", "Smart")
    put("dev_mb_crossover_hz", 120.0)
    put("dev_mb_attack_mode", "Ramp")
    put("dev_mb_release_ms", 30.0)
    put("dev_mb_safety", False)
    put("dev_smart_fast_ms", SMART_FAST)
    put("dev_smart_slow_ms", SMART_SLOW)
    put("dev_smart_sustain_ms", SMART_SUSTAIN)
    put("dev_smart_leak", SMART_LEAK)
    put("limiter_active", True)
    put("drive_active", False)
    put("ceiling_active", True)
    put("ceiling_release_ms", "Clip")
    put("ceiling_mode", "SamplePeak")
    put("ceiling_db", CEIL_DB)
    put("auto_track", False)
    put("input_gain_db", float(gain))
    return p(x, sr)


def render_prol_allround(x, sr, gain):
    p = P._plug(P.PROL)
    p.style = "Allround"
    p.true_peak_limiting = False
    p.oversampling = "Off"
    p.output_level = CEIL_DB
    p.gain = float(gain)
    return p(x, sr)


def match_gr(x, sr, render_fn, target_gr=GR_TARGET):
    best = None
    for g in np.arange(8.0, 22.01, 1.5):
        y = render_fn(x, sr, g)
        e = abs(gr_of(x, y, g) - target_gr)
        if best is None or e < best[2]:
            best = (float(g), y, e)
    g0 = best[0]
    for g in np.arange(max(0.0, g0 - 1.5), g0 + 1.51, 0.5):
        y = render_fn(x, sr, g)
        e = abs(gr_of(x, y, g) - target_gr)
        if e < best[2]:
            best = (float(g), y, e)
    return best[0], best[1]


def envelope_db_fast(y, sr):
    """Vectorized replacement for mbl_pump.envelope_db (same hop/win/FPS)."""
    m = y.mean(1) if y.ndim > 1 else y
    m = np.asarray(m, dtype=np.float64)
    hop = int(sr / P.ENV_FPS)
    win = hop * 2
    n = (len(m) - win) // hop
    if n <= 0:
        return np.array([-120.0])
    # stride tricks: shape (n, win)
    from numpy.lib.stride_tricks import as_strided
    step = m.strides[0]
    frames = as_strided(m, shape=(n, win), strides=(hop * step, step))
    e = np.sqrt(np.mean(frames * frames, axis=1) + 1e-20)
    return 20.0 * np.log10(e + 1e-12)


def score_total(x, y, sr, dry_spec=None):
    """|MACRO|+|PUMP|+|ROUGH|. Pass dry_spec=(freqs, spec) to avoid recomputing dry FFT."""
    if dry_spec is None:
        fx, sx = P.modulation_spectrum(envelope_db_fast(x, sr))
    else:
        fx, sx = dry_spec
    fy, sy = P.modulation_spectrum(envelope_db_fast(y, sr))
    am = {}
    for name, lo, hi in P.BANDS:
        a = P.band_energy(fy, sy, lo, hi)
        b = P.band_energy(fx, sx, lo, hi)
        am[name] = 20.0 * np.log10((a + 1e-12) / (b + 1e-12))
    return (abs(am["MACRO 0.1-0.5"]) + abs(am["PUMP 2-8"]) + abs(am["ROUGH 8-20"]), am)


def dry_modulation_spec(x, sr):
    return P.modulation_spectrum(envelope_db_fast(x, sr))


def sample_peak_db(y):
    return P.sample_peak_db(y)


def rms_lin(y):
    m = y.mean(1) if y.ndim > 1 else y
    return float(np.sqrt(np.mean(m.astype(np.float64) ** 2) + 1e-30))


# ---------------------------------------------------------------- gain grid
def peak_abs(x):
    if x.ndim == 1:
        return np.abs(x.astype(np.float64))
    return np.max(np.abs(x.astype(np.float64)), axis=1)


def estimate_gain(driven, y):
    """Per-sample linked gain from fixed-law output. Clamped to (0, 1]."""
    pd = peak_abs(driven)
    py = peak_abs(y)
    g = np.ones(len(pd), dtype=np.float64)
    mask = pd > 1e-8
    g[mask] = py[mask] / pd[mask]
    return np.clip(g, 1e-6, 1.0)


def control_times(n, sr, hop_s=CTRL_HOP_S):
    hop = max(1, int(round(hop_s * sr)))
    t = np.arange(0, n, hop, dtype=np.float64)
    if t[-1] != n - 1:
        t = np.append(t, n - 1)
    return t, hop


def decimate_gain(g, t_idx):
    """Mean gain in each control segment ending at t_idx[i]."""
    out = np.empty(len(t_idx), dtype=np.float64)
    prev = 0
    for i, end in enumerate(t_idx.astype(int)):
        sl = g[prev:end + 1]
        out[i] = float(np.mean(sl)) if len(sl) else 1.0
        prev = end + 1
    return np.clip(out, 1e-6, 1.0)


def interpolate_gain(ctrl, t_idx, n):
    return np.clip(np.interp(np.arange(n, dtype=np.float64), t_idx, ctrl), 1e-6, 1.0)


def project_peak_rms(g, driven, target_rms, ceil_lin=CEIL_LIN, iters=6, pd=None):
    """Hard peak ceiling + equal-loudness projection. Returns (g, valid, peak_db, rms_err_db)."""
    if pd is None:
        pd = peak_abs(driven)
    safe = np.maximum(pd, 1e-12)
    g = np.clip(np.asarray(g, dtype=np.float64), 1e-6, 1.0)
    # Precompute driven energy for RMS without forming the stereo buffer each iter.
    # RMS of mean-channel ≈ using linked peak is wrong; use per-sample energy of mean.
    if driven.ndim == 1:
        w = driven.astype(np.float64) ** 2
    else:
        w = np.mean(driven.astype(np.float64) ** 2, axis=1)
    for _ in range(iters):
        g = np.minimum(g, ceil_lin / safe)
        # rms of y = driven * g  (mean channel) = sqrt(mean(w * g^2))
        cur = float(np.sqrt(np.mean(w * g * g) + 1e-30))
        if cur < 1e-12:
            break
        g *= (target_rms / cur)
        g = np.clip(g, 1e-6, 1.0)
    g = np.minimum(g, ceil_lin / safe)
    g = np.clip(g, 1e-6, 1.0)
    cur = float(np.sqrt(np.mean(w * g * g) + 1e-30))
    pk = 20.0 * np.log10(float(np.max(pd * g)) + 1e-12)
    rms_err = 20.0 * np.log10((cur + 1e-30) / (target_rms + 1e-30))
    valid = pk <= -0.99 and abs(rms_err) <= RMS_TOL_DB
    return g, valid, pk, rms_err


def apply_gain(driven, g):
    return (driven * g[:, None]).astype(np.float32)


def project_ctrl(ctrl, t_idx, n, driven, target_rms, pd):
    """Interpolate control → project at sample rate."""
    return project_peak_rms(interpolate_gain(ctrl, t_idx, n), driven, target_rms, pd=pd)


# ---------------------------------------------------------------- Stage A oracle
def oracle_refine(x, driven, y_fixed, sr, verbose=False):
    """Non-causal oracle: redistribute a broadband gain envelope.

    Optimisation runs at OPT_SR (2 kHz) for speed — peak/RMS projections and the
    modulation metric are evaluated on the downsampled signals during the search.
    The returned score is then re-checked at full sample rate (hard gate).

    Method:
      1. Initialise from the fixed-law peak-ratio gain.
      2. Global L-BFGS-B over N_SPLINE log-gain knots.
    """
    OPT_SR = 2000.0
    factor = max(1, int(round(sr / OPT_SR)))
    opt_sr = sr / factor

    n = len(driven)
    driven_ds = driven[::factor]
    x_ds = x[::factor]
    y_fixed_ds = y_fixed[::factor]
    n_ds = len(driven_ds)

    t_idx, _ = control_times(n_ds, opt_sr)
    dry_spec = dry_modulation_spec(x_ds, opt_sr)
    pd = peak_abs(driven_ds)
    g0 = estimate_gain(driven_ds, y_fixed_ds)
    ctrl = decimate_gain(g0, t_idx)
    target_rms = rms_lin(y_fixed_ds)

    def eval_ctrl(c):
        gg, ok, pkk, rerr = project_ctrl(c, t_idx, n_ds, driven_ds, target_rms, pd)
        if not ok:
            return 1.0e3 + 100.0 * max(0.0, pkk - (-1.0)) + 50.0 * abs(rerr), None, False, pkk, rerr
        yy = apply_gain(driven_ds, gg)
        sc, am = score_total(x_ds, yy, opt_sr, dry_spec=dry_spec)
        d2 = np.diff(c, n=2)
        sc_pen = sc + SMOOTH_PENALTY * float(np.dot(d2, d2)) * (100.0 / max(1, len(c)))
        return sc_pen, (sc, am, gg, yy), True, pkk, rerr

    g, valid, pk, rms_err = project_ctrl(ctrl, t_idx, n_ds, driven_ds, target_rms, pd)
    y = apply_gain(driven_ds, g)
    best_score, best_am = score_total(x_ds, y, opt_sr, dry_spec=dry_spec)
    best_ctrl = ctrl.copy()
    sc_plugin, _ = score_total(x_ds, y_fixed_ds.astype(np.float32), opt_sr, dry_spec=dry_spec)
    if verbose:
        print(f"      init (ds {opt_sr:.0f} Hz) score={best_score:.3f}  "
              f"plugin~={sc_plugin:.3f}  sPk={pk:.2f}  rms_err={rms_err:+.3f} dB  valid={valid}")

    n_ctrl = len(best_ctrl)
    knot_idx = np.unique(np.round(np.linspace(0, n_ctrl - 1, N_SPLINE)).astype(int))
    knot_t = t_idx[knot_idx]
    z0 = np.log(np.clip(best_ctrl[knot_idx], 1e-6, 1.0))

    def spline_to_ctrl(z):
        knots = np.exp(z)
        return np.clip(np.interp(t_idx, knot_t, knots), 1e-6, 1.0)

    res = minimize(lambda z: eval_ctrl(spline_to_ctrl(z))[0], z0, method="L-BFGS-B",
                   bounds=[(np.log(1e-6), 0.0)] * len(z0),
                   options={"maxiter": SPLINE_MAXITER, "ftol": 1e-5})
    cand = spline_to_ctrl(res.x)
    sc_pen, payload, ok, pkk, rerr = eval_ctrl(cand)
    if ok and payload is not None and payload[0] < best_score - 1e-4:
        best_score, best_am, g, y = payload[0], payload[1], payload[2], payload[3]
        best_ctrl = cand
    if verbose:
        print(f"      spline ({len(knot_idx)} knots) score={best_score:.3f} "
              f"sPk={pkk:.2f} rms_err={rerr:+.3f} nfev={res.nfev}")

    # Upsample best control to full rate and re-validate / re-score (authoritative).
    t_full = t_idx * factor
    t_full[-1] = n - 1
    g_full = interpolate_gain(best_ctrl, t_full, n)
    pd_full = peak_abs(driven)
    target_rms_full = rms_lin(y_fixed)
    g_full, valid, pk, rms_err = project_peak_rms(g_full, driven, target_rms_full, pd=pd_full)
    y_full = apply_gain(driven, g_full)
    dry_full = dry_modulation_spec(x, sr)
    sc, am = score_total(x, y_full, sr, dry_spec=dry_full)
    sc_plugin_full, _ = score_total(x, y_fixed.astype(np.float32), sr, dry_spec=dry_full)
    if verbose:
        print(f"      full-rate check score={sc:.3f} plugin Smart={sc_plugin_full:.3f} "
              f"sPk={pk:.2f} rms_err={rms_err:+.3f} valid={valid}")

    hop = max(1, int(round(CTRL_HOP_S * opt_sr)))
    return {
        "score": sc,
        "am": am,
        "y": y_full,
        "g": g_full,
        "valid": valid,
        "peak_db": pk,
        "rms_err_db": rms_err,
        "n_ctrl": n_ctrl,
        "hop_ms": 1000.0 * hop / opt_sr,
        "plugin_score": sc_plugin_full,
    }


# ---------------------------------------------------------------- Stage B causal
def causal_refine(x, driven, y_fixed, sr, lookahead_ms, verbose=False):
    """Causal approximation: at time t, gain may only use signal up to t + lookahead.

    Method: running estimate of local crest / density from a causal lookahead
    window, used to warp the fixed-law gain toward the Stage-A direction —
    but only using past+lookahead samples. Scaled by f(GR_depth) so light
    limiting stays on the fixed law (§7.2).

    This is intentionally a *simple* causal law (not the oracle re-run with a
    mask). It prices how much of the oracle headroom a lookahead-limited
    controller can reach.
    """
    n = len(driven)
    la = max(1, int(round(lookahead_ms * 1e-3 * sr)))
    g_fixed = estimate_gain(driven, y_fixed)
    target_rms = rms_lin(y_fixed)
    pd = peak_abs(driven)

    # Oracle direction as a non-causal teacher (for measuring reachable fraction only
    # we do NOT use future beyond lookahead). Build a causal "desired" gain:
    # within each lookahead window, set gain to the minimum required by the
    # upcoming peak (classic limiter), then release with a program-dependent
    # rate toward 1. That is LookaheadFollower-like; the *depth* adaptation is
    # how far below the fixed-law we allow when the upcoming window is sparse.
    g_causal = np.ones(n, dtype=np.float64)
    env = 1.0
    # Smoothed GR depth from fixed law (≈1 s), for f(GR_depth).
    gr_fixed = np.clip(-20.0 * np.log10(np.maximum(g_fixed, 1e-6)), 0.0, 24.0)
    alpha_depth = np.exp(-1.0 / max(1.0, 1.0 * sr))  # ~1 s
    depth = 0.0

    # Precompute windowed peak (causal + lookahead): max of pd[t : t+la]
    # via reverse then forward maximum filter of length la+1 is non-causal;
    # use a deque-free O(n) trailing max on a shifted signal.
    # win_max[t] = max(pd[t : t+la+1]) clipped to n.
    win_max = np.empty(n, dtype=np.float64)
    # Compute from the end with a monotonic queue alternative: simple for la small.
    # For speed at la up to 20 ms * 48k = 960, use stride loop in numpy chunks.
    for t in range(n):
        end = min(n, t + la + 1)
        win_max[t] = pd[t:end].max() if end > t else pd[t]

    release_alpha = np.exp(-1.0 / max(1.0, 0.030 * sr))  # 30 ms baseline recovery
    for t in range(n):
        depth = alpha_depth * depth + (1.0 - alpha_depth) * gr_fixed[t]
        # f(GR): 0 at 0 dB, ~1 by ~8 dB.
        f = float(np.clip(depth / 8.0, 0.0, 1.0))

        # Instantaneous demand to hold the upcoming peak under ceiling.
        demand = min(1.0, CEIL_LIN / max(win_max[t], 1e-12))
        g_law = min(g_fixed[t], demand)

        # Open toward the lookahead-safe ceiling when f(GR) is high and the
        # upcoming window has headroom; stay on the fixed law when f→0.
        open_target = demand
        desired = (1.0 - f) * g_law + f * open_target
        desired = float(np.clip(desired, 1e-6, demand))

        if desired < env:
            env = desired  # attack: follow demand immediately (lookahead already spent)
        else:
            # Release toward desired; faster when f is small (light GR → stick to law).
            a = release_alpha ** (0.5 + 0.5 * (1.0 - f))
            env = a * env + (1.0 - a) * desired
        g_causal[t] = min(env, demand)

    g, valid, pk, rms_err = project_peak_rms(g_causal, driven, target_rms)
    y = apply_gain(driven, g)
    sc, am = score_total(x, y, sr)
    if verbose:
        print(f"      causal la={lookahead_ms:.0f} ms score={sc:.3f} sPk={pk:.2f} "
              f"rms_err={rms_err:+.3f} valid={valid}")
    return {
        "score": sc,
        "am": am,
        "y": y,
        "valid": valid,
        "peak_db": pk,
        "rms_err_db": rms_err,
        "lookahead_ms": lookahead_ms,
    }


# ---------------------------------------------------------------- driver
def run_stage_a(verbose=True):
    print("=" * 88)
    print("STAGE A — NON-CAUSAL DEPTH ORACLE (upper bound)")
    print(f"opt @ 2 kHz (20 ms grid, {N_SPLINE} spline knots, L-BFGS maxiter={SPLINE_MAXITER}); "
          f"authoritative score at full rate")
    print(f"baseline = OPEN+Smart tuned (fast={SMART_FAST}, slow={SMART_SLOW}, "
          f"sustain={SMART_SUSTAIN}, leak={SMART_LEAK})")
    print(f"matched to {GR_TARGET:.0f} dB RMS-GR; hard sPk <= {CEIL_DB:.2f}; equal loudness |ΔRMS|<={RMS_TOL_DB} dB")
    print(f"objective = |MACRO|+|PUMP|+|ROUGH|  (mbl_pump.added_modulation)")
    print("=" * 88)

    rows = []
    t0 = time.time()
    for name, path in CORPUS:
        print(f"\n  {name}", flush=True)
        x, sr = load(path)
        drive, y_smart = match_gr(x, sr, render_open_smart)
        sc_smart, am_smart = score_total(x, y_smart, sr)
        pk_smart = sample_peak_db(y_smart)
        gr_smart = gr_of(x, y_smart, drive)
        print(f"    Smart tuned  drive={drive:+.1f} GR={gr_smart:.2f} sPk={pk_smart:.2f}  "
              f"TOTAL={sc_smart:.3f}  (M={abs(am_smart['MACRO 0.1-0.5']):.2f} "
              f"P={abs(am_smart['PUMP 2-8']):.2f} R={abs(am_smart['ROUGH 8-20']):.2f})", flush=True)
        if pk_smart > -0.99:
            print("    INVALID Smart baseline (peak miss) — abort source", flush=True)
            rows.append(dict(name=name, smart=np.nan, oracle=np.nan, prol=np.nan, valid=False))
            continue

        drive_p, y_prol = match_gr(x, sr, render_prol_allround)
        sc_prol, am_prol = score_total(x, y_prol, sr)
        pk_prol = sample_peak_db(y_prol)
        print(f"    Pro-L Allround drive={drive_p:+.1f} sPk={pk_prol:.2f} TOTAL={sc_prol:.3f}", flush=True)

        driven = (x.astype(np.float64) * (10.0 ** (drive / 20.0)))
        # Align lengths (pedalboard should already match).
        n = min(len(driven), len(y_smart))
        driven = driven[:n]
        y_smart = y_smart[:n]
        x_ref = x[:n]

        ora = oracle_refine(x_ref, driven, y_smart.astype(np.float64), sr, verbose=verbose)
        tag = "" if ora["valid"] else "  <-- INVALID"
        print(f"    ORACLE       sPk={ora['peak_db']:.2f} rms_err={ora['rms_err_db']:+.3f} "
              f"ctrl={ora['n_ctrl']}@{ora['hop_ms']:.1f}ms  TOTAL={ora['score']:.3f}{tag}", flush=True)

        rows.append(dict(
            name=name,
            smart=sc_smart,
            oracle=ora["score"] if ora["valid"] else np.nan,
            prol=sc_prol if pk_prol <= -0.99 else np.nan,
            valid=ora["valid"],
            delta_smart=(sc_smart - ora["score"]) if ora["valid"] else np.nan,
            ora=ora,
            drive=drive,
            driven=driven,
            y_smart=y_smart,
            x=x_ref,
            sr=sr,
        ))

    print("\n  ===== STAGE A TABLE (|MACRO|+|PUMP|+|ROUGH|, lower better) =====")
    print(f"  {'source':16s} {'Smart':>8} {'Oracle':>8} {'Δ vs Sm':>8} {'Pro-L A':>8}")
    for r in rows:
        print(f"  {r['name']:16s} {r['smart']:8.3f} {r['oracle']:8.3f} "
              f"{r['delta_smart']:+8.3f} {r['prol']:8.3f}")

    smart_mean = float(np.nanmean([r["smart"] for r in rows]))
    ora_mean = float(np.nanmean([r["oracle"] for r in rows]))
    prol_mean = float(np.nanmean([r["prol"] for r in rows]))
    delta_mean = smart_mean - ora_mean
    live = next((r for r in rows if r["name"] == "live-show"), None)

    print(f"\n  MEAN           {smart_mean:8.3f} {ora_mean:8.3f} {delta_mean:+8.3f} {prol_mean:8.3f}")
    if live is not None:
        print(f"\n  HEADLINE live-show: Smart {live['smart']:.3f} → Oracle {live['oracle']:.3f} "
              f"(Δ {live['delta_smart']:+.3f})")

    print(f"\n  elapsed {time.time() - t0:.0f}s")
    print("\n  DECISION GATE (corpus-mean Oracle improvement over tuned Smart):")
    if delta_mean < 0.5:
        verdict = "STOP — axis 3 is not worth building (< 0.5 headroom)."
    elif delta_mean > 1.5:
        verdict = "PROCEED to Stage B — strong case (> 1.5 headroom)."
    else:
        verdict = "BORDERLINE — architect's call with avishali (0.5..1.5). Stage B not auto-run."
    print(f"  Δmean = {delta_mean:+.3f}  →  {verdict}")

    return rows, delta_mean, verdict


def run_stage_b(rows, lookaheads=(5.0, 10.0, 20.0)):
    print("\n" + "=" * 88)
    print("STAGE B — CAUSAL APPROXIMATION (lookahead-limited)")
    print(f"lookaheads_ms={lookaheads}; adaptation scaled by f(GR_depth) per §7.2")
    print("=" * 88)

    summary = {la: [] for la in lookaheads}
    for r in rows:
        if not r.get("valid"):
            continue
        print(f"\n  {r['name']}  (Smart {r['smart']:.3f}, Oracle {r['oracle']:.3f})", flush=True)
        for la in lookaheads:
            out = causal_refine(r["x"], r["driven"], r["y_smart"].astype(np.float64),
                                r["sr"], la, verbose=True)
            summary[la].append(out["score"] if out["valid"] else np.nan)
            frac = (r["smart"] - out["score"]) / max(1e-9, r["smart"] - r["oracle"]) if out["valid"] else np.nan
            print(f"    la={la:4.0f} ms  TOTAL={out['score']:.3f}  "
                  f"valid={out['valid']}  frac_of_oracle={frac:.2f}", flush=True)

    print("\n  ===== STAGE B MEAN vs Smart / Oracle =====")
    smart_mean = float(np.nanmean([r["smart"] for r in rows]))
    ora_mean = float(np.nanmean([r["oracle"] for r in rows]))
    print(f"  {'la_ms':>8} {'causal':>8} {'vs Smart':>9} {'vs Oracle':>10}")
    for la in lookaheads:
        m = float(np.nanmean(summary[la]))
        print(f"  {la:8.0f} {m:8.3f} {smart_mean - m:+9.3f} {m - ora_mean:+10.3f}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stage", choices=["A", "B", "AB"], default="AB",
                    help="A=oracle only; B=force causal; AB=A then B if Δmean>1.5")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    rows, delta_mean, verdict = run_stage_a(verbose=not args.quiet)

    if args.stage == "A":
        print("\n(Stage B skipped by --stage A)")
        return
    if args.stage == "B" or (args.stage == "AB" and delta_mean > 1.5):
        run_stage_b(rows)
    else:
        print(f"\n(Stage B not run — gate verdict: {verdict})")

    print("\nConfirm: this script touches tools/analysis/ only. No plugin/SDK edits.")


if __name__ == "__main__":
    main()
