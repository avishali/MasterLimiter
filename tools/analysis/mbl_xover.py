#!/usr/bin/env python3
"""Verify the §4a N=2 result is robust to crossover frequency (Cursor flagged the LR N=2
DEFAULT crossover may be ~16 kHz = near-wideband, which would make the earlier 'N=2 breathes'
result a confound). Drive-matched N=2 range across explicit crossovers, peak-controlled (ramp).
"""
import subprocess, os, numpy as np, soundfile as sf
from spectral_proto import st_range, true_peak_db, rms_db, GENRES

BENCH = "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/build/shared/mdsp_dsp/tools/mbl_bench"
OUT = "/tmp/mbl_xo.wav"


def render(src, drive, xover):
    args = [BENCH, "--mode", "multi", "--bands", "2", "--split", "lr", "--in", src, "--out", OUT,
            "--drive-db", f"{drive:.3f}", "--ceiling-db", "-1.0", "--attack-mode", "ramp",
            "--release-ms", "150", "--lookahead-ms", "5"]
    if xover is not None:
        args += ["--crossovers", str(xover)]
    if os.path.exists(OUT):
        os.remove(OUT)
    subprocess.run(args, check=True, capture_output=True)
    return sf.read(OUT)[0]


def meas(y):
    m = y.mean(1) if y.ndim > 1 else y
    return dict(rms=rms_db(m), rng=st_range(m), tp=true_peak_db(y))


def match(src, xover, target_rms):
    best = None
    def scan(drives):
        nonlocal best
        for d in drives:
            mm = meas(render(src, d, xover))
            if best is None or abs(mm["rms"]-target_rms) < abs(best[1]["rms"]-target_rms):
                best = (d, mm)
    scan(np.arange(0.0, 24.01, 1.5))
    scan(np.arange(best[0]-1.25, best[0]+1.26, 0.5))
    return best


if __name__ == "__main__":
    xovers = [None, 120, 300, 700, 1500, 3000]
    for g in GENRES:
        print(f"\n===== {g['name']} — N=2 LR, crossover sweep, matched RMS {g['target_rms']:.2f} (Ozone range {g['target_rng']:.2f}) =====")
        print(f"  {'crossover':>10} {'RMS':>7} {'range':>6} {'TP':>6} {'drive':>6}")
        for xo in xovers:
            d, mm = match(g["src"], xo, g["target_rms"])
            label = "default" if xo is None else f"{xo} Hz"
            print(f"  {label:>10} {mm['rms']:7.2f} {mm['rng']:6.2f} {mm['tp']:6.2f} {d:+6.1f}")
    print("\nIf range is high (~Ozone) across sensible crossovers → §4a robust. "
          "If only 'default' is high → the earlier result was a crossover artifact.")
