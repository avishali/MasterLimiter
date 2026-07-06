#!/usr/bin/env python3
"""Probe: N≥4 multiband + SUM-peak control (the "TP mode"). Does it beat N=2?

Two-pass = MultibandLimiter(safety off) → wideband SingleBandLimiter on the sum (ramp, -1).
Functionally the MultibandLimiter-with-safety architecture; lets us test now without a bench
change. Peak-matched (ramp everywhere), drive-matched on the FINAL output to Ozone RMS.
If N≥4+safety > N=2, make it a permanent --safety bench flag (S-C.2) + validate.
"""
import subprocess, os, numpy as np, soundfile as sf
from spectral_proto import st_range, true_peak_db, rms_db, GENRES

BENCH = "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/build/shared/mdsp_dsp/tools/mbl_bench"
P1, OUT = "/tmp/mbl_p1.wav", "/tmp/mbl_p2.wav"


def bench(src, out, mode, drive, release, bands=1, lookahead=5):
    args = [BENCH, "--mode", mode, "--in", src, "--out", out, "--drive-db", f"{drive:.3f}",
            "--ceiling-db", "-1.0", "--attack-mode", "ramp", "--lookahead-ms", str(lookahead)]
    if mode == "multi":
        rel = release if isinstance(release, str) else f"{float(release):.1f}"
        args += ["--bands", str(bands), "--release-ms", rel]
    else:
        args += ["--release-ms", f"{float(release):.1f}"]
    if os.path.exists(out):
        os.remove(out)
    subprocess.run(args, check=True, capture_output=True)


def render_safety(src, bands, drive, release):
    bench(src, P1, "multi", drive, release, bands)   # per-band, safety off
    bench(P1, OUT, "single", 0.0, 100.0)             # wideband safety on the sum (ramp, -1)
    return sf.read(OUT)[0]


def meas(y):
    m = y.mean(1) if y.ndim > 1 else y
    return dict(rms=rms_db(m), rng=st_range(m), tp=true_peak_db(y),
                spk=20*np.log10(np.max(np.abs(y))+1e-12))


def match(src, bands, release, target_rms):
    best = None
    def scan(drives):
        nonlocal best
        for d in drives:
            mm = meas(render_safety(src, bands, d, release))
            if best is None or abs(mm["rms"]-target_rms) < abs(best[1]["rms"]-target_rms):
                best = (d, mm)
    scan(np.arange(0.0, 24.01, 1.5))
    scan(np.arange(best[0]-1.25, best[0]+1.26, 0.5))
    return best


if __name__ == "__main__":
    for g in GENRES:
        print(f"\n===== {g['name']} — N-band + SUM-safety (TP mode), matched RMS {g['target_rms']:.2f}, Ozone range {g['target_rng']:.2f} =====")
        print(f"  {'config':26s} {'RMS':>7} {'range':>6} {'TP':>6} {'sPk':>6} {'drive':>6}")
        for N in (2, 4, 8):
            d, mm = match(g["src"], N, 150.0, g["target_rms"])
            print(f"  {'N='+str(N)+'+safety uniform150':26s} {mm['rms']:7.2f} {mm['rng']:6.2f} {mm['tp']:6.2f} {mm['spk']:6.2f} {d:+6.1f}")
    print("\nCompare range to N=2-no-safety (jazz 4.76 / edm 5.35) and Ozone (4.68/5.11).")
    print("If N>=4+safety > N=2, the TP mode earns its place; else 2-band is the sweet spot.")
