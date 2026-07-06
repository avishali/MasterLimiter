#!/usr/bin/env python3
"""P-A — leapfrog first validation. Does a lookahead-adaptive LOW-BAND threshold (duck the low
band just before bass transients) reduce the CLIPPER's LF distortion while keeping breathing?

Chain: input → 2-band@120 (bench, per-block threshold curve) → sum → hardclip -1 (clipper proxy).
Compare, at matched final loudness:
  baseline  = fixed low/high threshold -1/-1 (current MB-2 engine)
  static    = fixed low -8 (over-duck all bass) — reference
  adaptive  = low threshold DIPS (lookahead) only when a bass transient is coming; high fixed -1
Metrics: pre-clip peak (clipper workload), LF-THD post-clip, 300ms range, RMS.
Gate: adaptive lowers clipper workload + LF-THD vs baseline WHILE keeping range (and beats static
on range/low-end-body). Uses S-E --band-threshold-curve; BUG 0 fixed so overwrites are safe.
"""
import subprocess, os, numpy as np, soundfile as sf
from scipy.signal import butter, sosfilt
from scipy.ndimage import maximum_filter1d
from spectral_proto import st_range, true_peak_db, rms_db, lf_thd_db, GENRES

B = "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/build/shared/mdsp_dsp/tools/mbl_bench"
OUT, CSV = "/tmp/pa_out.wav", "/tmp/pa_curve.csv"
SR, BLK, CEIL = 48000, 512, 10 ** (-1.0/20)


def lowband_env_db(x, cutoff=120.0, la_ms=8.0):
    mono = x.mean(1) if x.ndim > 1 else x
    sos = butter(4, cutoff/(SR/2), "low", output="sos")
    lb = sosfilt(sos, mono)
    L = int(la_ms*1e-3*SR)
    env = maximum_filter1d(np.abs(lb), size=2*L+1, mode="nearest")
    nb = int(np.ceil(len(mono)/BLK))
    per_blk = np.array([20*np.log10(env[k*BLK:(k+1)*BLK].max()+1e-9) for k in range(nb)])
    return per_blk


def build_curve(x, mode):
    nb = int(np.ceil((x.shape[0])/BLK))
    lo = np.full(nb, -1.0); hi = np.full(nb, -1.0)
    if mode == "baseline":
        pass
    elif mode == "static":
        lo[:] = -8.0
    elif mode == "adaptive":
        env = lowband_env_db(x)
        ref = np.percentile(env, 60)                 # duck only louder-than-typical bass
        duck = np.clip(env - ref, 0, 14)             # up to 14 dB extra low-band limiting
        lo = -1.0 - duck
    np.savetxt(CSV, np.column_stack([lo, hi]), delimiter=",", fmt="%.3f")


def render_sum(x, drive):
    if os.path.exists(OUT):
        os.remove(OUT)
    subprocess.run([B, "--mode", "multi", "--bands", "2", "--split", "lr", "--crossovers", "120",
                    "--in", "/tmp/pa_in.wav", "--out", OUT, "--drive-db", f"{drive:.3f}",
                    "--ceiling-db", "-1.0", "--attack-mode", "ramp", "--release-ms", "150",
                    "--lookahead-ms", "5", "--band-threshold-curve", CSV],
                   check=True, capture_output=True)
    return sf.read(OUT)[0]


def measure(x, drive):
    s = render_sum(x, drive)                          # pre-clip sum
    m = s.mean(1)
    preclip_peak = 20*np.log10(np.max(np.abs(s))+1e-12)
    y = np.clip(s, -CEIL, CEIL)                       # clipper proxy
    my = y.mean(1)
    return dict(rms=rms_db(my), rng=st_range(my), preclip=preclip_peak,
                lfthd=lf_thd_db(my), clipgr=preclip_peak-20*np.log10(np.max(np.abs(y))+1e-12))


def match(x, mode, target_rms):
    build_curve(x, mode)                              # curve depends only on x+mode, drive is separate
    best = None
    for d in list(np.arange(0, 24.01, 2.0)):
        mm = measure(x, float(d))
        if best is None or abs(mm["rms"]-target_rms) < abs(best[1]["rms"]-target_rms):
            best = (float(d), mm)
    for d in np.arange(max(0, best[0]-1.5), min(24, best[0]+1.51), 0.5):
        mm = measure(x, float(d))
        if abs(mm["rms"]-target_rms) < abs(best[1]["rms"]-target_rms):
            best = (float(d), mm)
    return best[1]


if __name__ == "__main__":
    for g in GENRES:
        x, sr = sf.read(g["src"]); assert sr == SR
        sf.write("/tmp/pa_in.wav", x, SR)
        print(f"\n===== {g['name']} — low-band ducking vs clipper LF distortion (matched RMS {g['target_rms']:.2f}) =====")
        print(f"  {'config':10s} {'range':>6} {'preclip pk':>10} {'clip GR':>8} {'LF-THD':>7} {'RMS':>7}")
        for mode in ("baseline", "static", "adaptive"):
            m = match(x, mode, g["target_rms"])
            print(f"  {mode:10s} {m['rng']:6.2f} {m['preclip']:10.2f} {m['clipgr']:8.2f} {m['lfthd']:7.1f} {m['rms']:7.2f}")
    print("\nGate: adaptive < baseline on preclip-pk/clip-GR/LF-THD, range >= baseline, and > static on range.")
