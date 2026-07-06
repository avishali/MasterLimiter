#!/usr/bin/env python3
"""Phase-linearity probe: LR IIR split vs LinearPhaseCrossover, 2-band, same crossover freq.
Breathing (300 ms range) expected ~equal; the real questions are transient integrity
(linphase pre-ring may soften) and latency. Peak-controlled (ramp), drive-matched to Ozone.
"""
import subprocess, os, numpy as np, soundfile as sf
from spectral_proto import st_range, true_peak_db, rms_db, GENRES

BENCH = "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/build/shared/mdsp_dsp/tools/mbl_bench"
OUT = "/tmp/mbl_ph.wav"


def crest10(mono, sr=48000, win_s=0.010):
    w = int(win_s*sr)
    cs = []
    for s in range(0, len(mono)-w, w):
        seg = mono[s:s+w]
        r = np.sqrt(np.mean(seg**2)); p = np.max(np.abs(seg))
        if r > 1e-6:
            cs.append(20*np.log10(p/r))
    return float(np.median(cs))


def render(src, split, drive, xover):
    args = [BENCH, "--mode", "multi", "--bands", "2", "--split", split, "--in", src, "--out", OUT,
            "--drive-db", f"{drive:.3f}", "--ceiling-db", "-1.0", "--attack-mode", "ramp",
            "--release-ms", "150", "--lookahead-ms", "5", "--crossovers", str(xover)]
    if os.path.exists(OUT):
        os.remove(OUT)
    subprocess.run(args, check=True, capture_output=True)
    return sf.read(OUT)[0]


def meas(y):
    m = y.mean(1) if y.ndim > 1 else y
    return dict(rms=rms_db(m), rng=st_range(m), tp=true_peak_db(y), crest=crest10(m))


def match(src, split, xover, target_rms):
    best = None
    def scan(drives):
        nonlocal best
        for d in drives:
            mm = meas(render(src, split, d, xover))
            if best is None or abs(mm["rms"]-target_rms) < abs(best[1]["rms"]-target_rms):
                best = (d, mm)
    scan(np.arange(0.0, 24.01, 1.5))
    scan(np.arange(best[0]-1.25, best[0]+1.26, 0.5))
    return best


if __name__ == "__main__":
    for g in GENRES:
        print(f"\n===== {g['name']} — LR vs LinearPhase, 2-band, matched RMS {g['target_rms']:.2f} =====")
        print(f"  {'split @ xover':>18} {'RMS':>7} {'range':>6} {'TP':>6} {'crest10':>8}")
        for xo in (120, 700):
            for split in ("lr", "linphase"):
                d, mm = match(g["src"], split, xo, g["target_rms"])
                print(f"  {split+' @ '+str(xo)+'Hz':>18} {mm['rms']:7.2f} {mm['rng']:6.2f} {mm['tp']:6.2f} {mm['crest']:8.2f}")
    print("\nrange ~equal => phase doesn't change breathing (expected). "
          "crest10: lower for linphase => pre-ring softening transients. Latency: LR 960 / linphase ~4046.")
