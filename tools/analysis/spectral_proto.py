#!/usr/bin/env python3
"""P-0 rev2 — Offline spectral-limiter prototype (SPECTRAL_ENGINE_DESIGN.md §8).

STFT-primary (sqrt-Hann, 75% overlap, Bark bin groups). IIR butter bandpass
dropped — does not reconstruct. Ozone-matched test: SP −1 tolerance, no FC/brickwall.

Per-band law: req_k = min(1, T/e_k), shared T, breathing release, sum (no brickwall).
Tune (drive, T) on ~18 s segment; one full-file render for reported metrics.
"""
from __future__ import annotations

import os
import time

import numpy as np
import soundfile as sf
from scipy.ndimage import maximum_filter1d
from scipy.signal import butter, istft, sosfilt, stft

SR = 48000
TARGET_TP_DB = -0.5
LOOKAHEAD_MS = 2.0
LEAK = 0.25
FAST_MS, SLOW_MS, SIG_MS = 50.0, 500.0, 150.0
DEPTH_SCALE = 3.0
STFT_N = 2048
STFT_HOP = STFT_N // 4
TUNE_SEC = 18.0
_WIN = np.sqrt(np.hanning(STFT_N))
_FRAME_RATE = SR / STFT_HOP


# ---------- metrics ----------
def st_range(mono, sr=SR, win_s=0.300, hop_s=0.100):
    w, h = int(win_s * sr), int(hop_s * sr)
    v = [20 * np.log10(np.sqrt(np.mean(mono[s:s + w] ** 2)) + 1e-12)
         for s in range(0, len(mono) - w, h)
         if np.sqrt(np.mean(mono[s:s + w] ** 2)) > 1e-7]
    v = np.array(v)
    return float(np.percentile(v, 95) - np.percentile(v, 10))


def true_peak_db(x, sr=SR, up=4):
    n = len(x)
    X = np.fft.rfft(x, axis=0)
    Y = np.zeros((n * up // 2 + 1,) + x.shape[1:], dtype=complex)
    Y[:X.shape[0]] = X
    return float(20 * np.log10(np.max(np.abs(np.fft.irfft(Y, n=n * up, axis=0) * up)) + 1e-12))


def sample_peak_db(x):
    return float(20 * np.log10(np.max(np.abs(x)) + 1e-12))


def rms_db(mono):
    return float(20 * np.log10(np.sqrt(np.mean(mono ** 2)) + 1e-12))


def alpha(ms, rate):
    return float(np.exp(-1.0 / (max(ms, 1e-3) * 1e-3 * rate)))


def bark_edges(nbands, sr=SR, fmin=30.0, fmax=20000.0):
    b = lambda f: 13 * np.arctan(0.00076 * f) + 3.5 * np.arctan((f / 7500) ** 2)
    fgrid = np.linspace(fmin, fmax, 40000)
    bgrid = b(fgrid)
    return np.interp(np.linspace(b(fmin), b(fmax), nbands + 1), bgrid, fgrid)


def lf_thd_db(mono, sr=SR, f_lo=45.0, f_hi=130.0):
    sos = butter(4, [f_lo / (sr / 2), f_hi / (sr / 2)], btype="band", output="sos")
    bp = sosfilt(sos, mono.astype(np.float64))
    n = len(bp)
    seg = bp[n // 4: n // 4 + min(n // 2, 1 << 18)]
    if len(seg) < 4096:
        return float("nan")
    w = np.hanning(len(seg))
    mag = np.abs(np.fft.rfft(seg * w))
    freqs = np.fft.rfftfreq(len(seg), 1.0 / sr)
    band = (freqs >= f_lo) & (freqs <= f_hi)
    if not np.any(band):
        return float("nan")
    idx = np.argmax(mag[band])
    f0 = freqs[band][idx]
    if f0 < 20:
        return float("nan")
    f0_lvl = float(np.max(mag[(freqs >= f0 - 2) & (freqs <= f0 + 2)]))
    if f0_lvl <= 0:
        return float("nan")
    harm = 0.0
    for h in range(2, 9):
        fh = f0 * h
        if fh >= sr / 2 - 10:
            break
        harm += float(np.max(mag[(freqs >= fh - 3) & (freqs <= fh + 3)])) ** 2
    return float(10 * np.log10(harm / (f0_lvl ** 2) + 1e-30))


def mean_gr_db(g_frame):
    g = np.clip(g_frame, 1e-6, 1.0)
    return float(-20 * np.log10(np.mean(g)))


def breathing_gain(req, fast_a, slow_a, depth_scale, sig_rel, leak):
    n = len(req)
    g = np.empty(n)
    gi = 1.0
    sig = 0.0
    for i in range(n):
        r = float(req[i])
        if r < gi:
            gi = r
            sig = min(1.0, (1.0 - gi) * depth_scale)
        else:
            sig = sig_rel * sig + (1.0 - sig_rel) * min(1.0, (1.0 - gi) * depth_scale)
            rel_a = fast_a + sig * (slow_a - fast_a)
            target = min(1.0, r + leak * (1.0 - r))
            gi = rel_a * gi + (1.0 - rel_a) * target
        g[i] = gi
    return g


_BP = dict(
    fast_a=alpha(FAST_MS, _FRAME_RATE),
    slow_a=alpha(SLOW_MS, _FRAME_RATE),
    depth_scale=DEPTH_SCALE,
    sig_rel=alpha(SIG_MS, _FRAME_RATE),
    leak=LEAK,
)


def _bark_bin_slices(K, n_bins, sr=SR):
    edges = bark_edges(K)
    freqs = np.fft.rfftfreq((n_bins - 1) * 2, 1.0 / sr)
    slices = []
    for i in range(K):
        lo, hi = edges[i], edges[i + 1]
        mask = (freqs >= lo) & (freqs < hi) if i < K - 1 else (freqs >= lo) & (freqs <= hi)
        slices.append(np.where(mask)[0])
    return slices


def _stft_analyze(sig):
    return stft(sig, fs=SR, window=_WIN, nperseg=STFT_N, noverlap=STFT_N - STFT_HOP,
                boundary="zeros", padded=True, return_onesided=True)


def _env_scale(sig):
    """Map STFT bin magnitudes → time-domain peak scale (per channel)."""
    _, _, Z = _stft_analyze(sig.astype(np.float64))
    td = float(np.max(np.abs(sig)))
    fd = float(np.max(np.abs(Z)))
    return td / (fd + 1e-12)


def limit_stft(x, K, T, lookahead_ms=LOOKAHEAD_MS, env_scale=1.0):
    """Per-Bark-band STFT limiter; K=1 = wideband (all bins one band). No brickwall."""
    if x.ndim == 1:
        x = x[:, None]
    n_samp = x.shape[0]
    out = np.zeros((n_samp, x.shape[1]), dtype=np.float64)
    g_frames = []
    L_frames = max(1, int(lookahead_ms * 1e-3 * SR / STFT_HOP))

    for ch in range(x.shape[1]):
        sig = x[:, ch].astype(np.float64)
        _, _, Z = _stft_analyze(sig)
        n_f, n_t = Z.shape
        bin_slices = _bark_bin_slices(K, n_f)
        Zout = Z.copy()
        ch_g = np.ones(n_t, dtype=np.float64)
        scale = env_scale if np.isscalar(env_scale) else env_scale[ch]

        for sl in bin_slices:
            if len(sl) == 0:
                continue
            mag = np.max(np.abs(Z[sl, :]), axis=0) * scale
            env = maximum_filter1d(mag, size=2 * L_frames + 1, mode="nearest")
            req = np.minimum(1.0, T / (env + 1e-12))
            g = breathing_gain(req, **_BP)
            ch_g *= g
            Zout[sl, :] *= g[None, :]

        _, y = istft(Zout, fs=SR, window=_WIN, nperseg=STFT_N, noverlap=STFT_N - STFT_HOP,
                     boundary="zeros")
        out[: min(len(y), n_samp), ch] = y[:n_samp]
        g_frames.append(ch_g)

    return out[:n_samp].astype(np.float32), np.mean(g_frames)


def measure_y(y, g_proxy=None):
    mono = y.mean(1) if y.ndim > 1 else y
    m = dict(
        rms=rms_db(mono),
        rng=st_range(mono),
        tp=true_peak_db(y),
        spk=sample_peak_db(y),
        lf_thd=lf_thd_db(mono),
        gr=mean_gr_db(g_proxy) if g_proxy is not None else float("nan"),
    )
    return m


def _score(m, target_rms, target_tp=TARGET_TP_DB):
    tp_pen = 0.0
    if m["tp"] < -1.5:
        tp_pen += (-1.5 - m["tp"]) ** 2 * 2.0
    elif m["tp"] > 1.5:
        tp_pen += (m["tp"] - 1.5) ** 2 * 2.0
    return (m["rms"] - target_rms) ** 2 + (m["tp"] - target_tp) ** 2 + tp_pen


def _render(x, K, drive_db, T_db, env_scale):
    scale = 10.0 ** (float(drive_db) / 20.0)
    T = 10.0 ** (float(T_db) / 20.0)
    return limit_stft(x * scale, K, T, env_scale=env_scale)


def tune_and_render(x, K, target_rms, tune_sec=TUNE_SEC):
    """Phase 1: grid (drive,T) on segment. Phase 2: bisect drive on full file for RMS."""
    if x.ndim == 1:
        x = x[:, None]
    xt = x[: min(x.shape[0], int(tune_sec * SR))]
    escale = [_env_scale(xt[:, ch]) for ch in range(x.shape[1])]
    escale_mean = float(np.mean(escale))

    best = None
    best_cost = float("inf")
    for drive_db in np.arange(6.0, 32.0, 1.5):
        for T_db in np.arange(-6.0, -0.5, 0.5):
            y, g = _render(xt, K, drive_db, T_db, escale)
            m = measure_y(y, g)
            if m["gr"] < 0.05:
                continue
            cost = _score(m, target_rms)
            if cost < best_cost:
                best_cost = cost
                best = dict(drive_db=float(drive_db), T_db=float(T_db))

    if best is None:
        for drive_db in np.arange(6.0, 32.0, 1.5):
            for T_db in np.arange(-12.0, -0.5, 0.5):
                y, g = _render(xt, K, drive_db, T_db, escale)
                cost = _score(measure_y(y, g), target_rms)
                if cost < best_cost:
                    best_cost = cost
                    best = dict(drive_db=float(drive_db), T_db=float(T_db))

    if best is None:
        raise RuntimeError(f"tune failed K={K}")

    d0, t0 = best["drive_db"], best["T_db"]
    for drive_db in np.arange(d0 - 1.5, d0 + 1.6, 0.25):
        for T_db in np.arange(t0 - 1.0, t0 + 1.1, 0.25):
            y, g = _render(xt, K, drive_db, T_db, escale)
            cost = _score(measure_y(y, g), target_rms)
            if cost < best_cost:
                best_cost = cost
                best = dict(drive_db=float(drive_db), T_db=float(T_db))

    # Bisect drive on full file (hold T) to nail RMS
    t_fix = best["T_db"]
    lo, hi = max(0.0, best["drive_db"] - 6.0), best["drive_db"] + 6.0
    for _ in range(12):
        mid = 0.5 * (lo + hi)
        y, g = _render(x, K, mid, t_fix, escale)
        m = measure_y(y, g)
        if m["rms"] < target_rms:
            lo = mid
        else:
            hi = mid
    best["drive_db"] = hi

    y, g = _render(x, K, best["drive_db"], best["T_db"], escale)
    best.update(measure_y(y, g))
    best["env_scale"] = escale_mean
    best["y"] = y
    return best


GENRES = [
    dict(
        name="JAZZ",
        src="/Users/avishaylidani/Music/test Project/Samples/Processed/Consolidate/MIX 0003 [2026-07-05 033539].wav",
        ozone="/Users/avishaylidani/Music/test Project/ test_ozone_11 mix 1.wav",
        target_rng=4.68,
        target_rms=-11.09,
    ),
    dict(
        name="EDM",
        src="/Users/avishaylidani/Music/test Project/Samples/Processed/Consolidate/MIX 0001 [2026-07-05 033723].wav",
        ozone="/Users/avishaylidani/Music/test Project/ test_ozone_11 mix 2.wav",
        target_rng=5.11,
        target_rms=-10.60,
    ),
]

K_VALUES = (1, 8, 16, 24)


def print_row(label, m, extra=""):
    print(f"  {label:36s} RMS {m['rms']:7.2f}  range {m['rng']:5.2f}  "
          f"TP {m['tp']:6.2f}  sPk {m['spk']:6.2f}  GR {m['gr']:5.2f}  "
          f"LF-THD {m['lf_thd']:6.1f}{extra}")


def verify_stft_recon():
    t = np.random.default_rng(0).standard_normal(int(SR * 0.5))
    _, _, Z = _stft_analyze(t)
    _, y = istft(Z, fs=SR, window=_WIN, nperseg=STFT_N, noverlap=STFT_N - STFT_HOP,
                 boundary="zeros")
    err = 20 * np.log10(np.sqrt(np.mean((y[:len(t)] - t) ** 2)) / (np.sqrt(np.mean(t ** 2)) + 1e-12) + 1e-12)
    print(f"STFT null recon rel-RMS: {err:.1f} dB (expect ≪ −60)")


def run():
    t0 = time.perf_counter()
    results = []

    print("P-0 rev2 — STFT-only spectral prototype (Ozone SP −1, no FC/brickwall)")
    print(f"  tune segment={TUNE_SEC}s  lookahead={LOOKAHEAD_MS}ms  leak={LEAK}\n")
    verify_stft_recon()

    for g in GENRES:
        x, sr = sf.read(g["src"])
        assert sr == SR
        oz, _ = sf.read(g["ozone"])
        ozm = oz.mean(1)
        oz_m = dict(rms=rms_db(ozm), rng=st_range(ozm), tp=true_peak_db(oz),
                    spk=sample_peak_db(oz), lf_thd=lf_thd_db(ozm), gr=0.0)

        print(f"\n{'=' * 100}")
        print(f"{g['name']}  —  Ozone target range {g['target_rng']:.2f}  RMS {g['target_rms']:.2f}")
        print_row(">>> OZONE IRC1 (TARGET)", oz_m)

        prev_rng = None
        for K in K_VALUES:
            print(f"  tuning {g['name']} STFT K={K}...", flush=True)
            try:
                best = tune_and_render(x, K, g["target_rms"])
            except RuntimeError as e:
                print(f"  K={K} FAIL: {e}", flush=True)
                continue
            tag = "  <wideband sanity>" if K == 1 else ""
            if K > 1 and prev_rng is not None:
                tag = f"  Δrange vs prev-K {best['rng'] - prev_rng:+.2f}"
            print_row(f"STFT K={K:2d}  d={best['drive_db']:+.1f}dB T={best['T_db']:+.1f}dB", best, tag)
            results.append(dict(
                genre=g["name"], K=K, drive_db=best["drive_db"], T_db=best["T_db"],
                rms=best["rms"], rng=best["rng"], tp=best["tp"], spk=best["spk"],
                gr=best["gr"], lf_thd=best["lf_thd"],
            ))
            prev_rng = best["rng"]

    elapsed = time.perf_counter() - t0
    print(f"\n{'=' * 100}")
    print(f"Runtime: {elapsed:.1f}s ({elapsed / 60:.1f} min)\n")
    print(f"{'genre':<6} {'K':>3} {'RMS':>7} {'range':>6} {'TP':>6} {'GR':>5} {'LF-THD':>7}  "
          f"{'drive':>6} {'T_dB':>6}")
    print("-" * 68)
    for r in results:
        print(f"{r['genre']:<6} {r['K']:3d} {r['rms']:7.2f} {r['rng']:6.2f} {r['tp']:6.2f} "
              f"{r['gr']:5.2f} {r['lf_thd']:7.1f}  {r['drive_db']:+6.1f} {r['T_db']:+6.1f}")
    print("-" * 68)
    print("Targets: JAZZ range 4.68 RMS −11.09 | EDM range 5.11 RMS −10.60 | K=1 floor ≈ 2.48/3.35")

    for gname, lo, hi in [("JAZZ", 2.0, 3.5), ("EDM", 2.5, 4.0)]:
        rows = [r for r in results if r["genre"] == gname and r["K"] == 1]
        if rows:
            r = rows[0]
            ok = lo <= r["rng"] <= hi
            print(f"SANITY K=1 STFT {gname}: range {r['rng']:.2f} RMS {r['rms']:.2f} TP {r['tp']:.2f} "
                  f"{'PASS' if ok else 'CHECK'}")

    return results, elapsed


if __name__ == "__main__":
    only = os.environ.get("P0_GENRE")
    if only:
        GENRES = [g for g in GENRES if g["name"] == only.upper()]
    run()
