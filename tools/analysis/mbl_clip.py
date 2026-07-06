#!/usr/bin/env python3
"""Push the fast catcher: how much breathing survives at a MUSICAL crossover (2-band@120)
when the sum tips are caught by a CLIPPER (zero release) vs a fast limiter?
Clip is a probe post-process (1-line nonlinearity; real engine clipper would be oversampled) —
tests the design idea, the 2-band limiting is the real bench module. Drive-matched to Ozone RMS."""
import subprocess, os, numpy as np, soundfile as sf
from spectral_proto import st_range, true_peak_db, rms_db, GENRES

B = "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/build/shared/mdsp_dsp/tools/mbl_bench"
P1, OUT = "/tmp/clip_p1.wav", "/tmp/clip_out.wav"
CEIL = 10 ** (-1.0 / 20)


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


def band2(src, drive):
    bench(src, P1, "multi", drive, "120", 2, 150)
    return sf.read(P1)[0]


def catch(y, mode):
    if mode == "none":
        return y
    if mode == "hardclip":
        return np.clip(y, -CEIL, CEIL)
    if mode == "softclip":
        return (CEIL * np.tanh(y / CEIL)).astype(np.float32)
    if mode.startswith("lim"):     # fast wideband limiter via 2nd bench pass
        rel = float(mode[3:])
        sf.write(P1, y, 48000)
        bench(P1, OUT, "single", 0.0, None, 1, rel)
        return sf.read(OUT)[0]
    return y


def meas(y):
    m = y.mean(1) if y.ndim > 1 else y
    return dict(rms=rms_db(m), rng=st_range(m), tp=true_peak_db(y),
                spk=20*np.log10(np.max(np.abs(y))+1e-12))


def match(src, mode, target_rms):
    best = None
    def scan(drives):
        nonlocal best
        for d in drives:
            mm = meas(catch(band2(src, float(d)), mode))
            if best is None or abs(mm["rms"]-target_rms) < abs(best[1]["rms"]-target_rms):
                best = (float(d), mm)
    scan(np.arange(0.0, 24.01, 2.0))
    scan(np.arange(max(0, best[0]-1.5), min(24.0, best[0]+1.51), 0.5))
    return best


if __name__ == "__main__":
    modes = [("none (breathing, unsafe)", "none"), ("hardclip -1", "hardclip"),
             ("softclip", "softclip"), ("lim 2ms", "lim2"), ("lim 5ms", "lim5")]
    for g in GENRES:
        print(f"\n===== {g['name']} — 2band@120 + tip-catcher, matched RMS {g['target_rms']:.2f} (Ozone {g['target_rng']:.2f}) =====")
        print(f"  {'catcher':26s} {'RMS':>7} {'range':>6} {'sPk':>6} {'TP':>6}")
        for label, mode in modes:
            d, mm = match(g["src"], mode, g["target_rms"])
            print(f"  {label:26s} {mm['rms']:7.2f} {mm['rng']:6.2f} {mm['spk']:6.2f} {mm['tp']:6.2f}")
    print("\nClipper (zero release) should preserve MORE breathing than a limiter while holding sample-peak -1.")
