#!/usr/bin/env python3
"""S-C measurement — drive the real mbl_bench (production SingleBandLimiter / MultibandLimiter)
and measure the §4a question: does per-band limiting + per-band release recover more macro
300 ms range than wideband, at matched loudness?

Claude's role (orchestration + measurement); the DSP is the C++ bench.

⚠️ PRELIMINARY caveat: the bench's hardcoded Hybrid-default attack passes transients (TP runs
hot, NOT Ozone-matched at −0.48). So this is the RELATIVE test (K=1 vs N at matched RMS, same
attack) — valid for 'does per-band release breathe more', NOT an Ozone-absolute match. The
peak-controlled Ozone match awaits S-C.1 (attack control in the bench).
"""
import subprocess, os, numpy as np, soundfile as sf
from spectral_proto import st_range, true_peak_db, rms_db, GENRES

BENCH = "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/build/shared/mdsp_dsp/tools/mbl_bench"
OUT = "/tmp/mbl_sc.wav"
CEIL_DB = -1.0


ATTACK = "ramp"    # peak-controlled (holds ceiling ~Ozone TP); "hybrid" passes transients


def render(src, mode, drive_db, release, bands=1, lookahead=5):
    args = [BENCH, "--mode", mode, "--in", src, "--out", OUT,
            "--drive-db", f"{drive_db:.3f}", "--ceiling-db", str(CEIL_DB),
            "--attack-mode", ATTACK, "--lookahead-ms", str(lookahead)]
    if mode == "multi":
        args += ["--bands", str(bands)]
        if isinstance(release, str):
            rel = release
        elif isinstance(release, (list, tuple, np.ndarray)):
            rel = ",".join(f"{r:.1f}" for r in release)
        else:
            rel = f"{float(release):.1f}"     # scalar broadcasts to all bands
        args += ["--release-ms", rel]
    else:
        args += ["--release-ms", f"{float(release):.1f}"]
    if os.path.exists(OUT):          # bench doesn't cleanly truncate an existing file
        os.remove(OUT)
    subprocess.run(args, check=True, capture_output=True)
    y, _ = sf.read(OUT)
    return y


def meas(y):
    m = y.mean(1) if y.ndim > 1 else y
    return dict(rms=rms_db(m), rng=st_range(m), tp=true_peak_db(y),
                spk=20*np.log10(np.max(np.abs(y))+1e-12))


def drive_match(src, mode, release, bands, target_rms, lookahead=5):
    """Grid+refine on drive so output RMS ≈ target (multiband RMS-vs-drive is non-monotonic,
    so bisection is unsafe — grid then refine around the closest)."""
    def closest(drives, best):
        for d in drives:
            mm = meas(render(src, mode, d, release, bands, lookahead))
            if best is None or abs(mm["rms"]-target_rms) < abs(best[1]["rms"]-target_rms):
                best = (d, mm)
        return best
    best = closest(np.arange(0.0, 24.01, 1.5), None)
    best = closest(np.arange(best[0]-1.25, best[0]+1.26, 0.5), best)
    return best


def fasthf(n):
    """Per-band release low→high: slow lows (300 ms) → fast highs (30 ms)."""
    return list(np.geomspace(300.0, 30.0, n))


if __name__ == "__main__":
    for g in GENRES:
        src, tgt = g["src"], g["target_rms"]
        print(f"\n===== {g['name']} — matched to Ozone RMS {tgt:.2f} (range target {g['target_rng']:.2f}, TP -0.5) =====")
        print(f"  {'config':30s} {'RMS':>7} {'range':>6} {'TP':>6} {'sPk':>6} {'drive':>6}")
        # K=1 wideband baseline (sanity: expect ~2.48 jazz / 3.35 edm)
        d, mm = drive_match(src, "single", 150.0, 1, tgt)
        print(f"  {'K=1 single rel150':30s} {mm['rms']:7.2f} {mm['rng']:6.2f} {mm['tp']:6.2f} {mm['spk']:6.2f} {d:+6.1f}  <sanity")
        base = mm["rng"]
        # multiband, uniform release (isolate band-count effect)
        for N in (2, 4, 8):
            d, mm = drive_match(src, "multi", 150.0, N, tgt)
            print(f"  {'N='+str(N)+' uniform rel150':30s} {mm['rms']:7.2f} {mm['rng']:6.2f} {mm['tp']:6.2f} {mm['spk']:6.2f} {d:+6.1f}  d{mm['rng']-base:+.2f}")
        # multiband, per-band release (fast HF / slow LF) — the §4a mechanism
        for N in (4, 8):
            rel = fasthf(N)
            d, mm = drive_match(src, "multi", rel, N, tgt)
            print(f"  {'N='+str(N)+' fastHF/slowLF':30s} {mm['rms']:7.2f} {mm['rng']:6.2f} {mm['tp']:6.2f} {mm['spk']:6.2f} {d:+6.1f}  d{mm['rng']-base:+.2f}")
    print(f"\nattack={ATTACK}. With ramp, peaks HELD (~Ozone TP) — this is the PEAK-MATCHED §4a test. "
          "Does multi still beat K=1 when both peak-controlled?")
