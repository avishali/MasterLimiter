#!/usr/bin/env python3
"""PUMPING vs BREATHING — separate the two things `st_range` conflates.

avishali's objection (2026-08-02), and it is correct: `st_range` = p95-p10 of 300 ms RMS windows sits
right on top of a 150 ms release. A limiter that pumps modulates level at ~2-7 Hz, which lands INSIDE
that measurement band, so pumping RAISES st_range. The metric we have been ranking engines with cannot
tell musical breathing from audible pumping, and plausibly rewards the latter.

This measures the distinction directly.

METHOD — added envelope modulation.
  1. Take the loudness envelope of the INPUT and of the OUTPUT (100 Hz frame rate, RMS in dB).
  2. Gain-normalise both (we care about movement, not level).
  3. FFT each envelope -> a MODULATION spectrum: how much the level moves, per rate.
  4. Report OUTPUT/INPUT per modulation band, in dB:
        > 0 dB  the limiter ADDED movement that was not in the source  == pumping
        < 0 dB  the limiter REMOVED movement that was in the source    == flattening
        ~ 0 dB  the limiter passed the source's macro-dynamics through == transparent

  Bands:
    0.1-0.5 Hz  MACRO   - song sections, phrases. Preserving this is what "open" should mean.
    0.5-2 Hz    SLOW    - bar-level movement; musical breathing lives here.
    2-8 Hz      PUMP    - syllable/beat rate. Added energy here is what people HEAR as pumping.
    8-20 Hz     ROUGH   - fast modulation, heard as grit/distortion rather than level movement.

A limiter that scores well on st_range purely by pumping shows up as a large positive PUMP number.
A genuinely open limiter shows ~0 in MACRO/SLOW and <= 0 in PUMP.

Also reports st_range at 300 ms / 1 s / 3 s so the window-length sensitivity is visible.

Drives are taken from the loudness-matched frontier run so no search is needed here.

Claude's role (orchestration + measurement); DSP is the C++ plugin / the reference plugins.
"""
import numpy as np
import soundfile as sf
from pedalboard import load_plugin

import ozone_state as oz
from spectral_proto import GENRES, SR, rms_db, sample_peak_db

CEIL = -1.0
ENV_FPS = 100.0          # envelope frame rate (10 ms hop) -> modulation rates up to 50 Hz
BANDS = [("MACRO 0.1-0.5", 0.1, 0.5), ("SLOW 0.5-2", 0.5, 2.0),
         ("PUMP 2-8", 2.0, 8.0), ("ROUGH 8-20", 8.0, 20.0)]


def envelope_db(y, sr):
    """Level envelope in dB at ENV_FPS, mean of channels."""
    m = y.mean(1) if y.ndim > 1 else y
    hop = int(sr / ENV_FPS)
    win = hop * 2
    n = (len(m) - win) // hop
    e = np.array([np.sqrt(np.mean(m[i * hop:i * hop + win] ** 2) + 1e-20) for i in range(n)])
    return 20 * np.log10(e + 1e-12)


def modulation_spectrum(env_db):
    """Spectrum of the level envelope. Mean-removed so it is movement only, not level."""
    e = env_db - np.mean(env_db)
    w = np.hanning(len(e))
    spec = np.abs(np.fft.rfft(e * w))
    freqs = np.fft.rfftfreq(len(e), 1.0 / ENV_FPS)
    return freqs, spec


def band_energy(freqs, spec, lo, hi):
    sel = (freqs >= lo) & (freqs < hi)
    return float(np.sqrt(np.mean(spec[sel] ** 2))) if np.any(sel) else 0.0


def added_modulation(x, y, sr):
    """OUTPUT/INPUT envelope modulation per band, in dB. Positive = limiter added movement."""
    fx, sx = modulation_spectrum(envelope_db(x, sr))
    fy, sy = modulation_spectrum(envelope_db(y, sr))
    out = {}
    for name, lo, hi in BANDS:
        a, b = band_energy(fy, sy, lo, hi), band_energy(fx, sx, lo, hi)
        out[name] = 20 * np.log10((a + 1e-12) / (b + 1e-12))
    return out


def st_range_win(y, win_s, sr):
    m = y.mean(1) if y.ndim > 1 else y
    w, h = int(win_s * sr), int(0.1 * sr)
    v = [20 * np.log10(np.sqrt(np.mean(m[s:s + w] ** 2)) + 1e-12)
         for s in range(0, len(m) - w, h)
         if np.sqrt(np.mean(m[s:s + w] ** 2)) > 1e-7]
    return float(np.percentile(v, 95) - np.percentile(v, 10)) if len(v) > 4 else float("nan")


# ---------------------------------------------------------------- configs (drives from the matched run)
OURS = "/Users/avishaylidani/Library/Audio/Plug-Ins/VST3/MasterLimiter.vst3"
PROL = "/Library/Audio/Plug-Ins/VST3/FabFilter Pro-L 2.vst3"
OZ11 = "/Library/Audio/Plug-Ins/VST3/Ozone 11 Maximizer.vst3"

DRIVES = {   # (jazz, edm) loudness-matched drives from mbl_frontier.py
    "OPEN rel150":       (8.2, 4.2),
    "OPEN rel300":       (8.2, 4.2),
    "OPEN rel60":        (8.2, 4.2),
    "TRANSPARENT":       (9.2, 6.8),
    "Pro-L 2 Transparent": (9.2, 5.2),
    "Pro-L 2 Aggressive":  (9.0, 5.2),
    "Ozone11 IRC (m2)":    (9.2, 5.2),
}


_CACHE = {}


def _plug(path):
    """One instance per plugin, reused across renders. pedalboard resets state between process()
    calls and every parameter is set explicitly below, so reuse is safe -- and it removes ~280
    plugin loads from a full sweep."""
    if path not in _CACHE:
        _CACHE[path] = load_plugin(path)
    return _CACHE[path]


def render_ours(x, sr, gain, mb, release_ms=150.0, attack_ms=None, crossover_hz=120.0):
    p = _plug(OURS)
    names = set(p.parameters.keys())

    def put(a, v):
        if a not in names:
            raise KeyError(f"{a!r} missing on this build -- silent no-op would misconfigure the test")
        setattr(p, a, v)

    put("dev_mb_engine", mb)
    if mb:
        put("dev_mb_crossover_hz", float(crossover_hz))
        put("dev_mb_attack_mode", "Ramp")
        put("dev_mb_release_ms", release_ms)
        if attack_ms is not None:
            put("dev_mb_attack_ms", float(attack_ms))
        put("dev_mb_safety", False)
    put("limiter_active", True)
    put("drive_active", False)
    put("ceiling_active", True)
    put("ceiling_release_ms", "Clip")
    put("ceiling_mode", "SamplePeak")
    put("ceiling_db", CEIL)
    put("auto_track", False)
    put("input_gain_db", float(gain))
    return p(x, sr)


def render_prol(x, sr, gain, style):
    p = _plug(PROL)
    p.style = style
    p.true_peak_limiting = False
    p.oversampling = "Off"
    p.output_level = CEIL
    p.gain = float(gain)
    return p(x, sr)


def render_oz(x, sr, gain, mode=2):
    p = _plug(OZ11)
    p.preset_data = oz.set_params(bytes(p.preset_data), Mode=mode, Gain=gain, Margin=CEIL)
    return p(x, sr)


def rms_of(y):
    m = y.mean(1) if y.ndim > 1 else y
    return 20 * np.log10(np.sqrt(np.mean(m ** 2)) + 1e-12)


def match(fn, target, gmax=24.0):
    """Search drive so output RMS ~= target (grid then refine), like mbl_frontier."""
    best = None
    for g in np.arange(0.0, gmax + 0.01, 2.0):
        y = fn(float(g))
        if best is None or abs(rms_of(y) - target) < abs(rms_of(best[1]) - target):
            best = (float(g), y)
    for g in np.arange(max(0.0, best[0] - 1.5), min(gmax, best[0] + 1.51), 0.5):
        y = fn(float(g))
        if abs(rms_of(y) - target) < abs(rms_of(best[1]) - target):
            best = (float(g), y)
    return best


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--targets", default="-16,-12", help="output RMS targets = light,heavy push")
    args = ap.parse_args()

    x, sr = sf.read(args.src)
    if x.ndim == 1:
        x = np.stack([x, x], axis=1)
    print("PUMPING vs BREATHING — added envelope modulation (output/input) + st_range vs window length")
    print(f"source: {args.src.split('/')[-1]}")
    print(f"        {sr} Hz, {len(x)/sr:.0f} s, peak {20*np.log10(np.max(np.abs(x))+1e-12):.2f} dBFS, "
          f"RMS {rms_of(x):.2f} dB")
    print("  > 0 dB in a band = the limiter ADDED level movement the source did not have.")
    print("  PUMP 2-8 Hz is heard as pumping. MACRO 0.1-0.5 Hz is section-level 'open'.\n")

    CONFIGS = [
        ("OPEN rel150",         lambda d, sr=sr: render_ours(x, sr, d, True, 150.0)),
        ("OPEN rel300",         lambda d, sr=sr: render_ours(x, sr, d, True, 300.0)),
        ("OPEN rel60",          lambda d, sr=sr: render_ours(x, sr, d, True, 60.0)),
        ("TRANSPARENT",         lambda d, sr=sr: render_ours(x, sr, d, False)),
        ("Pro-L 2 Transparent", lambda d, sr=sr: render_prol(x, sr, d, "Transparent")),
        ("Pro-L 2 Aggressive",  lambda d, sr=sr: render_prol(x, sr, d, "Aggressive")),
        ("Ozone11 IRC (m2)",    lambda d, sr=sr: render_oz(x, sr, d, 2)),
    ]

    for target in [float(t) for t in args.targets.split(",")]:
        push = target - rms_of(x)
        print("=" * 120)
        print(f"OUTPUT RMS matched to {target:.1f} dB  (~{push:.1f} dB of push above the source)")
        print(f"  {'config':22s} {'drive':>6} {'RMS':>7} {'sPk':>6} | {'MACRO':>7} {'SLOW':>7} "
              f"{'PUMP':>7} {'ROUGH':>7} | {'rng.3s':>7} {'rng1s':>7} {'rng3s':>7}")
        for label, fn in CONFIGS:
            try:
                d, y = match(fn, target)
            except Exception as e:
                print(f"  {label:22s}  FAILED {type(e).__name__}: {e}")
                continue
            am = added_modulation(x, y, sr)
            print(f"  {label:22s} {d:+6.1f} {rms_of(y):7.2f} {sample_peak_db(y):6.2f} | "
                  + " ".join(f"{am[n]:+7.2f}" for n, _, _ in BANDS)
                  + f" | {st_range_win(y,0.3,sr):7.2f} {st_range_win(y,1.0,sr):7.2f} {st_range_win(y,3.0,sr):7.2f}")
        print(f"  {'(dry source = 0 ref)':22s} {'':6s} {rms_of(x):7.2f} "
              f"{sample_peak_db(x):6.2f} | " + " ".join(f"{0.0:+7.2f}" for _ in BANDS)
              + f" | {st_range_win(x,0.3,sr):7.2f} {st_range_win(x,1.0,sr):7.2f} {st_range_win(x,3.0,sr):7.2f}")
        print()
