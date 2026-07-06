#!/usr/bin/env python3
"""Re-run §4a on MUSICAL crossovers (not the 16 kHz artifact). Shows the real
breathing-vs-TP-safety picture at sensible splits: 2-band ~120-150 Hz and 3-band 100/2500.
Peak-controlled (ramp), drive-matched to Ozone RMS. Safety via two-pass (bench has no --safety).
Every render rm's its output first (avoid the bench truncation quirk)."""
import subprocess, os, numpy as np, soundfile as sf
from spectral_proto import st_range, true_peak_db, rms_db, GENRES

B = "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/build/shared/mdsp_dsp/tools/mbl_bench"
P1, OUT = "/tmp/mus_p1.wav", "/tmp/mus_out.wav"


def bench(src, out, mode, drive, xovers, bands, release="150"):
    args = [B, "--mode", mode, "--in", src, "--out", out, "--drive-db", f"{drive:.3f}",
            "--ceiling-db", "-1.0", "--attack-mode", "ramp", "--release-ms", release, "--lookahead-ms", "5"]
    if mode == "multi":
        args += ["--bands", str(bands), "--split", "lr"]
        if xovers:
            args += ["--crossovers", xovers]
    if os.path.exists(out):
        os.remove(out)
    subprocess.run(args, check=True, capture_output=True)


def render(src, drive, xovers, bands, safety):
    if bands == 1:
        bench(src, OUT, "single", drive, None, 1)
        return sf.read(OUT)[0]
    bench(src, P1, "multi", drive, xovers, bands)
    if not safety:
        return sf.read(P1)[0]
    bench(P1, OUT, "single", 0.0, None, 1)   # wideband safety on the sum (ramp, -1)
    return sf.read(OUT)[0]


def meas(y):
    m = y.mean(1) if y.ndim > 1 else y
    return dict(rms=rms_db(m), rng=st_range(m), tp=true_peak_db(y))


def match(src, xovers, bands, safety, target_rms):
    best = None
    def scan(drives):
        nonlocal best
        for d in drives:
            mm = meas(render(src, float(d), xovers, bands, safety))
            if best is None or abs(mm["rms"]-target_rms) < abs(best[1]["rms"]-target_rms):
                best = (float(d), mm)
    scan(np.arange(0.0, 24.01, 2.0))
    scan(np.arange(max(0, best[0]-1.5), min(24.0, best[0]+1.51), 0.5))
    return best


CONFIGS = [
    ("K=1 wideband",        None,        1, False),
    ("2band 120  safetyOFF", "120",      2, False),
    ("2band 120  safetyON",  "120",      2, True),
    ("2band 150  safetyOFF", "150",      2, False),
    ("3band 100/2500 sOFF",  "100,2500", 3, False),
    ("3band 100/2500 sON",   "100,2500", 3, True),
]

if __name__ == "__main__":
    for g in GENRES:
        print(f"\n===== {g['name']} — MUSICAL crossovers, matched RMS {g['target_rms']:.2f} (Ozone range {g['target_rng']:.2f}, TP ~-0.5) =====")
        print(f"  {'config':22s} {'RMS':>7} {'range':>6} {'TP':>6} {'drive':>6}")
        for label, xo, bands, safety in CONFIGS:
            d, mm = match(g["src"], xo, bands, safety, g["target_rms"])
            print(f"  {label:22s} {mm['rms']:7.2f} {mm['rng']:6.2f} {mm['tp']:6.2f} {d:+6.1f}")
    print("\nRead: does musical 2/3-band breathe > K=1 (safety OFF)? what TP overshoot? does safety flatten it?")
