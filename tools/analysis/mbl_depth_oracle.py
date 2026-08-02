#!/usr/bin/env python3
"""SMART-3P.1 — causal depth oracle (Python only; no plugin/SDK/C++).

SMART-3P's +2.380 gate was confounded (broadband topology + non-causal opt vs
2-band Smart). This slice re-asks the gate with:

  Exp 1  plugin Smart | broadband unoptimised | non-causal oracle
         Δ_topology = Smart - broadband_unopt
         Δ_depth    = broadband_unopt - oracle   ← axis-3 evidence

  Exp 2  CAUSAL oracle at LA = 5, 10, 20, 50 ms
         (sliding-window optimiser; each window sees only up to t+LA)
         Gate is on causal-oracle Δ_depth vs plugin Smart (corpus mean).

Baseline: OPEN+Smart at SMART-1.1 defaults. Metric: |MACRO|+|PUMP|+|ROUGH|.
Corpus: mbl_voicing.CORPUS, matched GR_TARGET.

Usage:
    ./.venv/bin/python mbl_depth_oracle.py
    ./.venv/bin/python mbl_depth_oracle.py --quiet
"""
from __future__ import annotations

import argparse
import time

import numpy as np
from scipy.optimize import minimize_scalar, minimize

import mbl_pump as P
from mbl_voicing import CORPUS, GR_TARGET, load, gr_of

CEIL_DB = -1.0
CEIL_LIN = 10.0 ** (CEIL_DB / 20.0)
OPT_SR = 2000.0
CTRL_HOP_S = 0.020
N_SPLINE = 64
SPLINE_MAXITER = 60
RMS_TOL_DB = 0.05
SMOOTH_PENALTY = 1.0e-3
CAUSAL_LOOKAHEADS_MS = (5.0, 10.0, 20.0, 50.0)

# SMART-1.1 shipping Smart defaults — set explicitly every render.
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


# ---------------------------------------------------------------- metric
def envelope_db_fast(y, sr):
    m = y.mean(1) if y.ndim > 1 else y
    m = np.asarray(m, dtype=np.float64)
    hop = int(sr / P.ENV_FPS)
    win = hop * 2
    n = (len(m) - win) // hop
    if n <= 0:
        return np.array([-120.0])
    from numpy.lib.stride_tricks import as_strided
    step = m.strides[0]
    frames = as_strided(m, shape=(n, win), strides=(hop * step, step))
    e = np.sqrt(np.mean(frames * frames, axis=1) + 1e-20)
    return 20.0 * np.log10(e + 1e-12)


def score_total(x, y, sr, dry_spec=None):
    """|MACRO|+|PUMP|+|ROUGH| via the same bands as mbl_pump.added_modulation."""
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


# ---------------------------------------------------------------- gain helpers
def peak_abs(x):
    if x.ndim == 1:
        return np.abs(x.astype(np.float64))
    return np.max(np.abs(x.astype(np.float64)), axis=1)


def estimate_gain(driven, y):
    pd = peak_abs(driven)
    py = peak_abs(y)
    g = np.ones(len(pd), dtype=np.float64)
    mask = pd > 1e-8
    g[mask] = py[mask] / pd[mask]
    return np.clip(g, 1e-6, 1.0)


def control_times(n, sr, hop_s=CTRL_HOP_S):
    hop = max(1, int(round(hop_s * sr)))
    t = np.arange(0, n, hop, dtype=np.float64)
    if len(t) == 0 or t[-1] != n - 1:
        t = np.append(t, n - 1) if len(t) else np.array([n - 1], dtype=np.float64)
    return t, hop


def decimate_gain(g, t_idx):
    out = np.empty(len(t_idx), dtype=np.float64)
    prev = 0
    for i, end in enumerate(t_idx.astype(int)):
        sl = g[prev:end + 1]
        out[i] = float(np.mean(sl)) if len(sl) else 1.0
        prev = end + 1
    return np.clip(out, 1e-6, 1.0)


def interpolate_gain(ctrl, t_idx, n):
    return np.clip(np.interp(np.arange(n, dtype=np.float64), t_idx, ctrl), 1e-6, 1.0)


def project_peak_rms(g, driven, target_rms, ceil_lin=CEIL_LIN, iters=6, pd=None,
                     fill_headroom=False):
    if pd is None:
        pd = peak_abs(driven)
    safe = np.maximum(pd, 1e-12)
    g = np.clip(np.asarray(g, dtype=np.float64), 1e-6, 1.0)
    g_ceil = np.minimum(np.ones_like(g), ceil_lin / safe)
    if driven.ndim == 1:
        w = driven.astype(np.float64) ** 2
    else:
        w = np.mean(driven.astype(np.float64) ** 2, axis=1)
    for _ in range(iters):
        g = np.minimum(g, g_ceil)
        cur = float(np.sqrt(np.mean(w * g * g) + 1e-30))
        if cur < 1e-12:
            break
        g *= (target_rms / cur)
        g = np.clip(g, 1e-6, 1.0)
    g = np.minimum(g, g_ceil)
    g = np.clip(g, 1e-6, 1.0)
    cur = float(np.sqrt(np.mean(w * g * g) + 1e-30))
    # Optional: if uniform scale cannot reach target under the peak gate, fill
    # remaining headroom toward the per-sample ceiling (used by oracles only).
    if fill_headroom and cur + 1e-30 < target_rms:
        max_rms = float(np.sqrt(np.mean(w * g_ceil * g_ceil) + 1e-30))
        if max_rms >= target_rms:
            lo, hi = 0.0, 1.0
            base = g.copy()
            for _ in range(24):
                mid = 0.5 * (lo + hi)
                g_try = np.minimum(g_ceil, base + mid * (g_ceil - base))
                cur_try = float(np.sqrt(np.mean(w * g_try * g_try) + 1e-30))
                if cur_try < target_rms:
                    lo = mid
                else:
                    hi = mid
            g = np.minimum(g_ceil, base + hi * (g_ceil - base))
            cur = float(np.sqrt(np.mean(w * g * g) + 1e-30))
            if cur > target_rms * (10.0 ** (RMS_TOL_DB / 20.0)):
                g *= target_rms / cur
                g = np.minimum(g, g_ceil)
                cur = float(np.sqrt(np.mean(w * g * g) + 1e-30))
    pk = 20.0 * np.log10(float(np.max(pd * g)) + 1e-12)
    rms_err = 20.0 * np.log10((cur + 1e-30) / (target_rms + 1e-30))
    valid = pk <= -0.99 and abs(rms_err) <= RMS_TOL_DB
    return g, valid, pk, rms_err


def apply_gain(driven, g):
    return (driven * g[:, None]).astype(np.float32)


def project_ctrl(ctrl, t_idx, n, driven, target_rms, pd):
    return project_peak_rms(interpolate_gain(ctrl, t_idx, n), driven, target_rms, pd=pd,
                            fill_headroom=True)


def downsample_bundle(x, driven, y_fixed, sr):
    factor = max(1, int(round(sr / OPT_SR)))
    opt_sr = sr / factor
    return {
        "factor": factor,
        "opt_sr": opt_sr,
        "x": x[::factor],
        "driven": driven[::factor],
        "y_fixed": y_fixed[::factor],
        "n": len(driven[::factor]),
    }


def fullrate_from_ctrl(best_ctrl, t_idx, factor, n_full, driven_full, y_fixed_full, x_full, sr):
    t_full = t_idx * factor
    t_full[-1] = n_full - 1
    g_full = interpolate_gain(best_ctrl, t_full, n_full)
    pd_full = peak_abs(driven_full)
    target_rms_full = rms_lin(y_fixed_full)
    g_full, valid, pk, rms_err = project_peak_rms(
        g_full, driven_full, target_rms_full, pd=pd_full, fill_headroom=True)
    y_full = apply_gain(driven_full, g_full)
    dry_full = dry_modulation_spec(x_full, sr)
    sc, am = score_total(x_full, y_full, sr, dry_spec=dry_full)
    return dict(score=sc, am=am, y=y_full, g=g_full, valid=valid, peak_db=pk, rms_err_db=rms_err)


# ---------------------------------------------------------------- broadband unoptimised (Exp 1 control)
def broadband_unoptimised(x, driven, y_fixed, sr):
    """Fixed-law peak-ratio gain on x·g — no optimisation. Topology-matched baseline.

    If the fixed shape cannot reach Smart loudness under the peak gate (common with
    2-band → broadband reconstruction), fall back to the max peak-safe loudness for
    that shape and keep the row scorable (tagged rms_compromised).
    """
    g0 = estimate_gain(driven, y_fixed)
    target_rms = rms_lin(y_fixed)
    g, valid, pk, rms_err = project_peak_rms(g0, driven, target_rms)
    rms_compromised = False
    if not valid and pk <= -0.99 and rms_err < 0.0:
        pd = peak_abs(driven)
        g_shape = np.minimum(np.asarray(g0, dtype=np.float64), CEIL_LIN / np.maximum(pd, 1e-12))
        if driven.ndim == 1:
            w = driven.astype(np.float64) ** 2
        else:
            w = np.mean(driven.astype(np.float64) ** 2, axis=1)
        max_rms = float(np.sqrt(np.mean(w * g_shape * g_shape) + 1e-30))
        # Leave a tiny margin so the hard peak gate still holds after float noise.
        achiev = max_rms * (10.0 ** (-0.005 / 20.0))
        g, valid, pk, rms_err = project_peak_rms(g_shape, driven, achiev, pd=pd)
        rms_compromised = True
        valid = pk <= -0.99  # loudness matched to achievable, not Smart
    y = apply_gain(driven, g)
    sc, am = score_total(x, y, sr)
    return dict(score=sc, am=am, y=y, g=g, valid=valid, peak_db=pk,
                rms_err_db=rms_err, rms_compromised=rms_compromised,
                target_rms_smart=target_rms)


# ---------------------------------------------------------------- non-causal oracle
def oracle_noncausal(x, driven, y_fixed, sr, verbose=False):
    """Non-causal upper bound (same as SMART-3P Stage A)."""
    ds = downsample_bundle(x, driven, y_fixed, sr)
    t_idx, _ = control_times(ds["n"], ds["opt_sr"])
    dry_spec = dry_modulation_spec(ds["x"], ds["opt_sr"])
    pd = peak_abs(ds["driven"])
    g0 = estimate_gain(ds["driven"], ds["y_fixed"])
    ctrl = decimate_gain(g0, t_idx)
    target_rms = rms_lin(ds["y_fixed"])

    def eval_ctrl(c):
        gg, ok, pkk, rerr = project_ctrl(c, t_idx, ds["n"], ds["driven"], target_rms, pd)
        if not ok:
            return 1.0e3 + 100.0 * max(0.0, pkk + 1.0) + 50.0 * abs(rerr), None, False, pkk, rerr
        yy = apply_gain(ds["driven"], gg)
        sc, am = score_total(ds["x"], yy, ds["opt_sr"], dry_spec=dry_spec)
        d2 = np.diff(c, n=2)
        sc_pen = sc + SMOOTH_PENALTY * float(np.dot(d2, d2)) * (100.0 / max(1, len(c)))
        return sc_pen, (sc, am, gg), True, pkk, rerr

    best_ctrl = ctrl.copy()
    best_score = eval_ctrl(best_ctrl)[0]

    knot_idx = np.unique(np.round(np.linspace(0, len(best_ctrl) - 1, N_SPLINE)).astype(int))
    knot_t = t_idx[knot_idx]
    z0 = np.log(np.clip(best_ctrl[knot_idx], 1e-6, 1.0))

    def spline_to_ctrl(z):
        return np.clip(np.interp(t_idx, knot_t, np.exp(z)), 1e-6, 1.0)

    res = minimize(lambda z: eval_ctrl(spline_to_ctrl(z))[0], z0, method="L-BFGS-B",
                   bounds=[(np.log(1e-6), 0.0)] * len(z0),
                   options={"maxiter": SPLINE_MAXITER, "ftol": 1e-5})
    cand = spline_to_ctrl(res.x)
    sc_pen, payload, ok, pkk, rerr = eval_ctrl(cand)
    if ok and payload is not None and payload[0] < best_score - 1e-4:
        best_score, best_ctrl = payload[0], cand
    if verbose:
        print(f"      noncausal spline score~={best_score:.3f} nfev={res.nfev} "
              f"sPk~={pkk:.2f}", flush=True)

    out = fullrate_from_ctrl(best_ctrl, t_idx, ds["factor"], len(driven), driven, y_fixed, x, sr)
    out["n_ctrl"] = len(best_ctrl)
    if verbose:
        print(f"      noncausal full-rate TOTAL={out['score']:.3f} sPk={out['peak_db']:.2f} "
              f"rms_err={out['rms_err_db']:+.3f} valid={out['valid']}", flush=True)
    return out


# ---------------------------------------------------------------- causal oracle (Exp 2)
CAUSAL_OPT_SR = 500.0          # coarser grid for sequential window search
CAUSAL_CONTEXT_S = 3.0         # trailing context for the modulation metric
CAUSAL_MAXITER = 8
CAUSAL_OPT_PERIOD_S = 0.020    # re-run 1-D metric search ~every 20 ms


def oracle_causal(x, driven, y_fixed, sr, lookahead_ms, verbose=False):
    """Causal oracle: same objective as non-causal, information limited to t+LA.

    Sliding windows of length LA (prompt). Each window is peak-clamped using only
    pd[t:t+LA]. About every 20 ms a 1-D search minimises |MACRO|+|PUMP|+|ROUGH|
    on a trailing causal context ending at t+LA; intervening LA windows inherit
    the last decision (still re-clamped to their own LA peak). Previous windows
    are frozen; warm start blends fixed-law with the previous endpoint.

    Equal loudness via final full-rate peak+RMS projection.
    """
    factor = max(1, int(round(sr / CAUSAL_OPT_SR)))
    opt_sr = sr / factor
    x_ds = x[::factor]
    driven_ds = driven[::factor]
    y_fixed_ds = y_fixed[::factor]
    n = len(driven_ds)
    la = max(1, int(round(lookahead_ms * 1e-3 * opt_sr)))
    opt_every = max(1, int(round(CAUSAL_OPT_PERIOD_S * opt_sr / la)))
    pd = peak_abs(driven_ds)
    g_fixed = estimate_gain(driven_ds, y_fixed_ds)
    g = g_fixed.copy()
    ctx = max(la, int(round(CAUSAL_CONTEXT_S * opt_sr)))
    t = 0
    n_windows = 0
    n_searches = 0
    prev = float(g_fixed[0])
    g_star = prev
    while t < n:
        end = min(n, t + la)
        g_max = min(1.0, CEIL_LIN / max(float(np.max(pd[t:end])), 1e-12))
        do_search = (n_windows % opt_every == 0) or (end >= n)
        if do_search:
            c0 = max(0, end - ctx)
            x_ctx = x_ds[c0:end]
            d_ctx = driven_ds[c0:end]
            g_ctx_base = g[c0:end].copy()
            live0 = t - c0
            live1 = end - c0
            dry_spec = dry_modulation_spec(x_ctx, opt_sr)

            w_live = np.mean(driven_ds[t:end].astype(np.float64) ** 2, axis=1) \
                if driven_ds.ndim > 1 else driven_ds[t:end].astype(np.float64) ** 2
            rms_fixed_w = float(np.sqrt(np.mean(w_live * g_fixed[t:end] ** 2) + 1e-30))
            e_w = float(np.sqrt(np.mean(w_live) + 1e-30))

            def cost(log_g, g_max=g_max, g_ctx_base=g_ctx_base, live0=live0,
                     live1=live1, d_ctx=d_ctx, x_ctx=x_ctx, dry_spec=dry_spec,
                     opt_sr=opt_sr, e_w=e_w, rms_fixed_w=rms_fixed_w):
                gv = float(np.clip(np.exp(log_g), 1e-6, g_max))
                g_try = g_ctx_base.copy()
                g_try[live0:live1] = gv
                y_try = apply_gain(d_ctx, g_try)
                sc, _ = score_total(x_ctx, y_try, opt_sr, dry_spec=dry_spec)
                sc += 1.0e-3 * (gv - prev) ** 2
                # Anchor local loudness to the fixed-law window (equal-loudness).
                if e_w > 1e-12:
                    rms_try = gv * e_w
                    sc += 25.0 * (20.0 * np.log10((rms_try + 1e-30) / (rms_fixed_w + 1e-30))) ** 2
                return sc

            lo, hi = np.log(1e-6), np.log(max(1.01e-6, g_max))
            if hi <= lo + 1e-9:
                g_star = g_max
            else:
                res = minimize_scalar(cost, bounds=(lo, hi), method="bounded",
                                      options={"xatol": 2e-3, "maxiter": CAUSAL_MAXITER})
                g_star = float(np.clip(np.exp(res.x), 1e-6, g_max))
            n_searches += 1
        else:
            g_star = float(np.clip(g_star, 1e-6, g_max))
        g[t:end] = g_star
        prev = g_star
        n_windows += 1
        t = end

    target_rms_ds = rms_lin(y_fixed_ds)
    g, _, _, _ = project_peak_rms(g, driven_ds, target_rms_ds, pd=pd, fill_headroom=True)

    g_full = np.clip(np.repeat(g, factor)[:len(driven)], 1e-6, 1.0)
    if len(g_full) < len(driven):
        g_full = np.pad(g_full, (0, len(driven) - len(g_full)), mode="edge")
    pd_full = peak_abs(driven)
    target_rms_full = rms_lin(y_fixed)
    g_full, valid, pk, rms_err = project_peak_rms(
        g_full, driven, target_rms_full, pd=pd_full, fill_headroom=True)
    y_full = apply_gain(driven, g_full)
    sc, am = score_total(x, y_full, sr)
    if verbose:
        print(f"      causal-oracle LA={lookahead_ms:.0f} ms  windows={n_windows}  "
              f"searches={n_searches}  TOTAL={sc:.3f} sPk={pk:.2f} "
              f"rms_err={rms_err:+.3f} valid={valid}", flush=True)
    return dict(score=sc, am=am, y=y_full, g=g_full, valid=valid,
                peak_db=pk, rms_err_db=rms_err, lookahead_ms=lookahead_ms,
                n_windows=n_windows, n_searches=n_searches)


# ---------------------------------------------------------------- driver
def run(verbose=True):
    print("=" * 92)
    print("SMART-3P.1 — topology control + CAUSAL depth oracle")
    print(f"baseline = OPEN+Smart tuned (fast={SMART_FAST}, slow={SMART_SLOW}, "
          f"sustain={SMART_SUSTAIN}, leak={SMART_LEAK})")
    print(f"matched to {GR_TARGET:.0f} dB RMS-GR; hard sPk <= {CEIL_DB:.2f}; "
          f"|ΔRMS| <= {RMS_TOL_DB} dB")
    print(f"objective = |MACRO|+|PUMP|+|ROUGH|")
    print(f"causal LAs (ms) = {CAUSAL_LOOKAHEADS_MS}")
    print("=" * 92)

    rows = []
    t0 = time.time()

    for name, path in CORPUS:
        print(f"\n  {name}", flush=True)
        x, sr = load(path)
        drive, y_smart = match_gr(x, sr, render_open_smart)
        sc_smart, am_smart = score_total(x, y_smart, sr)
        pk_smart = sample_peak_db(y_smart)
        gr_smart = gr_of(x, y_smart, drive)
        print(f"    plugin Smart   drive={drive:+.1f} GR={gr_smart:.2f} sPk={pk_smart:.2f}  "
              f"TOTAL={sc_smart:.3f}", flush=True)
        if pk_smart > -0.99:
            print("    INVALID Smart baseline — skip source", flush=True)
            continue

        driven = (x.astype(np.float64) * (10.0 ** (drive / 20.0)))
        n = min(len(driven), len(y_smart))
        driven = driven[:n]
        y_smart = y_smart[:n]
        x_ref = x[:n]

        bb = broadband_unoptimised(x_ref, driven, y_smart.astype(np.float64), sr)
        tag = ""
        if not bb["valid"]:
            tag = "  <-- INVALID"
        elif bb.get("rms_compromised"):
            tag = "  (loudness=peak-safe max; < Smart RMS)"
        print(f"    broadband unopt sPk={bb['peak_db']:.2f} rms_err={bb['rms_err_db']:+.3f}  "
              f"TOTAL={bb['score']:.3f}{tag}", flush=True)

        ora = oracle_noncausal(x_ref, driven, y_smart.astype(np.float64), sr, verbose=verbose)
        tag = "" if ora["valid"] else "  <-- INVALID"
        print(f"    noncausal ora  sPk={ora['peak_db']:.2f} rms_err={ora['rms_err_db']:+.3f}  "
              f"TOTAL={ora['score']:.3f}{tag}", flush=True)

        d_topo = sc_smart - bb["score"] if bb["valid"] else np.nan
        d_depth = bb["score"] - ora["score"] if (bb["valid"] and ora["valid"]) else np.nan
        print(f"    Δ_topology={d_topo:+.3f}   Δ_depth(noncausal)={d_depth:+.3f}", flush=True)

        causal = {}
        for la in CAUSAL_LOOKAHEADS_MS:
            c = oracle_causal(x_ref, driven, y_smart.astype(np.float64), sr, la, verbose=verbose)
            causal[la] = c
            d_c = sc_smart - c["score"] if c["valid"] else np.nan
            d_c_depth = bb["score"] - c["score"] if (bb["valid"] and c["valid"]) else np.nan
            print(f"    causal LA={la:4.0f} ms  TOTAL={c['score']:.3f}  "
                  f"Δ vs Smart={d_c:+.3f}  Δ_depth vs bb={d_c_depth:+.3f}  "
                  f"valid={c['valid']}", flush=True)

        rows.append(dict(
            name=name,
            smart=sc_smart,
            bb=bb["score"] if bb["valid"] else np.nan,
            ora=ora["score"] if ora["valid"] else np.nan,
            d_topo=d_topo,
            d_depth=d_depth,
            causal=causal,
            bb_valid=bb["valid"],
            ora_valid=ora["valid"],
        ))

    # ---- Experiment 1 table ----
    print("\n" + "=" * 92)
    print("EXPERIMENT 1 — isolate topology from depth")
    print(f"  {'source':16s} {'Smart':>8} {'bb unopt':>9} {'oracle':>8} "
          f"{'Δ_topo':>8} {'Δ_depth':>8}")
    for r in rows:
        print(f"  {r['name']:16s} {r['smart']:8.3f} {r['bb']:9.3f} {r['ora']:8.3f} "
              f"{r['d_topo']:+8.3f} {r['d_depth']:+8.3f}")
    smart_m = float(np.nanmean([r["smart"] for r in rows]))
    bb_m = float(np.nanmean([r["bb"] for r in rows]))
    ora_m = float(np.nanmean([r["ora"] for r in rows]))
    topo_m = smart_m - bb_m
    depth_m = bb_m - ora_m
    print(f"  {'MEAN':16s} {smart_m:8.3f} {bb_m:9.3f} {ora_m:8.3f} "
          f"{topo_m:+8.3f} {depth_m:+8.3f}")
    live = next((r for r in rows if r["name"] == "live-show"), None)
    if live:
        print(f"\n  HEADLINE live-show: Smart {live['smart']:.3f} | bb {live['bb']:.3f} | "
              f"ora {live['ora']:.3f}  →  Δ_topo {live['d_topo']:+.3f}  "
              f"Δ_depth {live['d_depth']:+.3f}")

    # ---- Experiment 2 table ----
    print("\n" + "=" * 92)
    print("EXPERIMENT 2 — CAUSAL oracle (Δ vs plugin Smart; also Δ_depth vs bb unopt)")
    print(f"  {'source':16s} {'Smart':>7} {'bb':>7} {'n-caus':>7}", end="")
    for la in CAUSAL_LOOKAHEADS_MS:
        print(f"  {'c'+str(int(la)):>7}", end="")
    print()
    for r in rows:
        print(f"  {r['name']:16s} {r['smart']:7.3f} {r['bb']:7.3f} {r['ora']:7.3f}", end="")
        for la in CAUSAL_LOOKAHEADS_MS:
            c = r["causal"][la]
            v = c["score"] if c["valid"] else float("nan")
            print(f"  {v:7.3f}", end="")
        print()

    print(f"\n  {'MEAN Δ vs Smart':26s}", end="")
    # placeholder cols for Smart/bb/nc
    print(f"  {'':>7} {'':>7} {'':>7}", end="")
    causal_delta_means = {}
    causal_depth_means = {}
    for la in CAUSAL_LOOKAHEADS_MS:
        deltas = []
        depths = []
        for r in rows:
            c = r["causal"][la]
            if c["valid"]:
                deltas.append(r["smart"] - c["score"])
                if np.isfinite(r["bb"]):
                    depths.append(r["bb"] - c["score"])
        causal_delta_means[la] = float(np.nanmean(deltas)) if deltas else float("nan")
        causal_depth_means[la] = float(np.nanmean(depths)) if depths else float("nan")
        print(f"  {causal_delta_means[la]:+7.3f}", end="")
    print("   ← Δ = Smart - causal (positive = causal better than Smart)")

    print(f"  {'MEAN Δ_depth vs bb':26s}", end="")
    print(f"  {'':>7} {'':>7} {'':>7}", end="")
    for la in CAUSAL_LOOKAHEADS_MS:
        print(f"  {causal_depth_means[la]:+7.3f}", end="")
    print("   ← Δ_depth = bb - causal (axis-3 causal evidence)")

    # Best LA by Δ vs Smart (gate metric)
    best_la = max(CAUSAL_LOOKAHEADS_MS, key=lambda la: causal_delta_means.get(la, -1e9))
    best_delta = causal_delta_means[best_la]
    best_depth = causal_depth_means[best_la]

    print(f"\n  Non-causal reference: Δ vs Smart = {smart_m - ora_m:+.3f}  "
          f"(Δ_depth vs bb = {depth_m:+.3f})")
    print(f"  Causal/non-causal gap (best LA {best_la:.0f} ms): "
          f"noncausal beats causal by {(smart_m - ora_m) - best_delta:+.3f} on Δ-vs-Smart")

    if live:
        print(f"\n  HEADLINE live-show causal:")
        for la in CAUSAL_LOOKAHEADS_MS:
            c = live["causal"][la]
            d = live["smart"] - c["score"] if c["valid"] else float("nan")
            print(f"    LA={la:4.0f} ms  TOTAL={c['score']:.3f}  Δ vs Smart={d:+.3f}")

    print(f"\n  elapsed {time.time() - t0:.0f}s")

    # ---- Gate: causal-oracle Δ vs plugin Smart (corpus mean), best LA ----
    # Prompt: "causal-oracle Δ_depth vs plugin Smart" — reading the table header
    # "Δ_depth of the CAUSAL oracle" and the columns, the gate number is
    # Smart - causal_oracle (improvement over the shipping baseline). Also report
    # bb-relative Δ_depth. Primary gate = vs plugin Smart as in the verdict table.
    print("\n" + "=" * 92)
    print("DECISION GATE — causal-oracle improvement over plugin Smart (corpus mean)")
    print(f"  best LA = {best_la:.0f} ms   Δ vs Smart = {best_delta:+.3f}   "
          f"Δ_depth vs bb = {best_depth:+.3f}")
    if best_delta < 0.5:
        verdict = ("STOP — axis 3 is NOT buildable at these lookaheads. "
                   "Programme should move to axis 2.")
    elif best_delta > 1.5:
        verdict = (f"BUILD — causal headroom > 1.5 at LA={best_la:.0f} ms. "
                   "Latency cost of that LA is an avishali decision (fixed reported latency).")
    else:
        verdict = (f"MARGINAL — Δ={best_delta:+.3f} at LA={best_la:.0f} ms. "
                   "Architect + avishali decide, weighing latency cost.")
    print(f"  VERDICT: {verdict}")

    print("\nConfirm: tools/analysis/mbl_depth_oracle.py only. No plugin/SDK edits.")
    return rows, best_la, best_delta, verdict


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()
    run(verbose=not args.quiet)


if __name__ == "__main__":
    main()
