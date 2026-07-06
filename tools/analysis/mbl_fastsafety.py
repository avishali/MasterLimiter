#!/usr/bin/env python3
"""Can a FAST-release safety hold TP at a MUSICAL crossover WITHOUT flattening the breathing?
2-band @120, ramp; safety pass (wideband on the sum) with release sweep. If fast release keeps
range high AND TP<=~0, the musical breathing+TP-safe engine is crackable."""
import subprocess, os, numpy as np, soundfile as sf
from spectral_proto import st_range, true_peak_db, rms_db, GENRES

B = "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/build/shared/mdsp_dsp/tools/mbl_bench"
P1, OUT = "/tmp/fs_p1.wav", "/tmp/fs_out.wav"


def bench(src, out, mode, drive, xovers, bands, release):
    args = [B, "--mode", mode, "--in", src, "--out", out, "--drive-db", f"{drive:.3f}",
            "--ceiling-db", "-1.0", "--attack-mode", "ramp", "--release-ms", str(release), "--lookahead-ms", "5"]
    if mode == "multi":
        args += ["--bands", str(bands), "--split", "lr"]
        if xovers:
            args += ["--crossovers", xovers]
    if os.path.exists(out):
        os.remove(out)
    subprocess.run(args, check=True, capture_output=True)


def render(src, drive, safety_rel):
    bench(src, P1, "multi", drive, "120", 2, 150)          # 2-band @120, breathing
    bench(P1, OUT, "single", 0.0, None, 1, safety_rel)     # wideband safety, variable release
    return sf.read(OUT)[0]


def meas(y):
    m = y.mean(1) if y.ndim > 1 else y
    return dict(rms=rms_db(m), rng=st_range(m), tp=true_peak_db(y))


def match(src, safety_rel, target_rms):
    best = None
    def scan(drives):
        nonlocal best
        for d in drives:
            mm = meas(render(src, float(d), safety_rel))
            if best is None or abs(mm["rms"]-target_rms) < abs(best[1]["rms"]-target_rms):
                best = (float(d), mm)
    scan(np.arange(0.0, 24.01, 2.0))
    scan(np.arange(max(0, best[0]-1.5), min(24.0, best[0]+1.51), 0.5))
    return best


if __name__ == "__main__":
    for g in GENRES:
        print(f"\n===== {g['name']} — 2band@120 + safety release sweep, matched RMS {g['target_rms']:.2f} (Ozone {g['target_rng']:.2f}) =====")
        print(f"  {'safety release':>16} {'RMS':>7} {'range':>6} {'TP':>6}")
        for rel in (150, 50, 20, 10, 5):
            d, mm = match(g["src"], rel, g["target_rms"])
            print(f"  {str(rel)+' ms':>16} {mm['rms']:7.2f} {mm['rng']:6.2f} {mm['tp']:6.2f}")
    print("\nIf fast release keeps range ~5-6 AND TP<=~+1: musical breathing+TP-safe is crackable via fast catcher.")
