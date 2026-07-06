#!/usr/bin/env python3
"""Verify the PLUGIN's MB-engine path reproduces the bench §4a result (2-band parity).
Renders jazz/EDM through the installed VST3 with dev_mb_engine ON (crossover 120, Ramp,
safety ON, ceiling -1 SP), input-gain-matched to Ozone RMS, measures 300 ms range.
Gate: range ≈ bench (jazz ~4.6 / edm ~5.0)."""
import numpy as np, soundfile as sf
from pedalboard import load_plugin
from spectral_proto import st_range, true_peak_db, rms_db, GENRES

VST3 = "/Users/avishaylidani/Library/Audio/Plug-Ins/VST3/MasterLimiter.vst3"
SR = 48000


def configure(p, gain_db, xover, safety):
    p.dev_mb_engine = True
    p.dev_mb_crossover_hz = float(xover)
    p.dev_mb_attack_mode = "Ramp"
    p.dev_mb_release_ms = 150.0
    p.dev_mb_safety = bool(safety)
    p.limiter_active = True
    p.clipper_active = False
    p.ceiling_mode = "SamplePeak"
    p.ceiling_db = -1.0
    p.input_gain_db = min(24.0, max(0.0, gain_db))
    p.gain_match_auto = False


def render(x, gain_db, xover, safety):
    p = load_plugin(VST3)
    configure(p, gain_db, xover, safety)
    return p(x, SR)


def meas(y):
    m = y.mean(1) if y.ndim > 1 else y
    return dict(rms=rms_db(m), rng=st_range(m), tp=true_peak_db(y))


def match(x, target_rms, xover, safety):
    best = None
    for g in np.arange(0.0, 24.01, 2.0):
        mm = meas(render(x, float(g), xover, safety))
        if best is None or abs(mm["rms"]-target_rms) < abs(best[1]["rms"]-target_rms):
            best = (float(g), mm)
    g0 = best[0]
    for g in np.arange(max(0, g0-1.5), min(24.0, g0+1.51), 0.5):
        mm = meas(render(x, float(g), xover, safety))
        if abs(mm["rms"]-target_rms) < abs(best[1]["rms"]-target_rms):
            best = (float(g), mm)
    return best


if __name__ == "__main__":
    print("Plugin MB-engine path — (crossover, safety) matrix, matched to Ozone RMS, Ramp, ceiling -1 SP")
    print("bench refs: 120Hz no-safety jazz~5.75/edm~6.40 ; default(~16k)+safety jazz~4.61/edm~5.03 ; wideband floor 2.1/3.0")
    for g in GENRES:
        x, sr = sf.read(g["src"]); assert sr == SR
        print(f"  --- {g['name']} (Ozone {g['target_rng']:.2f}) ---")
        for xover, safety in [(120, False), (120, True), (700, False), (3000, False), (3000, True), (16000, True)]:
            d, mm = match(x, g["target_rms"], xover, safety)
            print(f"    xover {xover:4d}Hz safety {str(safety):5s}  gain +{d:4.1f}  RMS {mm['rms']:7.2f}  range {mm['rng']:5.2f}  TP {mm['tp']:6.2f}")
