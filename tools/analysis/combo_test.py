#!/usr/bin/env python3
"""COMBO TEST (2026-07-06) — quantify the *tunable ceiling*: how far do the
shipped tools (post-clip + TP-safe FinalCeiling + release) close the ~2.6 dB
macro-dynamic-breathing gap to Ozone IRC 1?

Base per new decisions = FinalCeiling OFF + clipper OFF. We render a real raw
mix through the plugin at a decomposed matrix and measure:
  - macro breathing  = 300 ms short-term loudness RANGE (p95 - p10)  <-- the gap
  - loudness         = integrated RMS (dB) and LUFS-I if pyloudnorm present
  - true peak safety = inter-sample peak (4x oversampled) dB
  - micro crest      = peak/RMS on 10 ms windows

Banked Ozone reference (from LIMITER_TYPES.md, real mixes, matched loudness):
  300 ms range ~9.7 (live mix) / 4.7 (jazz) / 5.1 (EDM); RMS ~-10.7; TP ~-0.58.
Ours (base) landed ~7.9 live / 2.1 jazz / 2.5 EDM -> ~2.6 dB flatter.
"""
import numpy as np, soundfile as sf
from pedalboard import load_plugin

VST3 = "/Users/avishaylidani/Library/Audio/Plug-Ins/VST3/MasterLimiter.vst3"
SR = 48000
RAW = "/Users/avishaylidani/Music/ML_audition/mix_real_raw.wav"
OZONE = "/Users/avishaylidani/Music/test Project/ test_ozone_11 mix 1.wav"  # diff song, metric anchor only

try:
    import pyloudnorm as pyln
    _meter = pyln.Meter(SR)
except Exception:
    _meter = None


def st_range(mono, sr, win_s=0.300, hop_s=0.100):
    """300 ms windowed RMS loudness range (p95 - p10), in dB. Macro-breathing proxy."""
    w, h = int(win_s * sr), int(hop_s * sr)
    vals = []
    for s in range(0, len(mono) - w, h):
        seg = mono[s:s + w]
        r = np.sqrt(np.mean(seg ** 2))
        if r > 1e-7:
            vals.append(20 * np.log10(r))
    vals = np.array(vals)
    return np.percentile(vals, 95) - np.percentile(vals, 10)


def crest10(mono, sr, win_s=0.010):
    w = int(win_s * sr)
    cs = []
    for s in range(0, len(mono) - w, w):
        seg = mono[s:s + w]
        r = np.sqrt(np.mean(seg ** 2))
        p = np.max(np.abs(seg))
        if r > 1e-6:
            cs.append(20 * np.log10(p / r))
    return float(np.median(cs))


def true_peak_db(x, sr):
    # crude 4x oversample via FFT zero-pad per channel, take max
    n = len(x)
    up = 4
    X = np.fft.rfft(x, axis=0)
    Y = np.zeros((n * up // 2 + 1, x.shape[1]), dtype=complex)
    Y[:X.shape[0]] = X
    y = np.fft.irfft(Y, n=n * up, axis=0) * up
    return 20 * np.log10(np.max(np.abs(y)) + 1e-12)


def measure(y, sr, label):
    mono = y.mean(1) if y.ndim > 1 else y
    rms = 20 * np.log10(np.sqrt(np.mean(mono ** 2)) + 1e-12)
    rng = st_range(mono, sr)
    tp = true_peak_db(y, sr)
    cr = crest10(mono, sr)
    lufs = _meter.integrated_loudness(y) if _meter is not None else float('nan')
    print(f"  {label:26s} RMS {rms:7.2f}  LUFS {lufs:7.2f}  300ms-range {rng:5.2f}  "
          f"TP {tp:6.2f}  crest10 {cr:4.1f}")
    return dict(rms=rms, rng=rng, tp=tp, crest=cr, lufs=lufs)


def setp(p, name, value):
    setattr(p, name, value)


def render(raw, cfg):
    p = load_plugin(VST3)
    # common baseline
    setp(p, "limiter_active", True)
    setp(p, "ceiling_mode", "TruePeak")
    setp(p, "ceiling_db", -1.0)
    setp(p, "character", "Clean")
    setp(p, "release_auto", "Off")
    setp(p, "dev_release_engine", "Lookahead")
    setp(p, "dev_la_poles", 2.0)
    setp(p, "input_gain_db", 9.6)
    # per-config
    for k, v in cfg.items():
        setp(p, k, v)
    return p(raw, SR)


# Reframed by the 2026-07-06 CORRECTION (INTELLIGENT_RELEASE_DESIGN.md):
#   breathing comes from FC-OFF + SLOW main release; FC's hardcoded ~100ms
#   release + the clipper are what FLATTEN. Installed build exposes
#   dev_fc_release_ms -> test whether FC-fast-release + slow main release keeps
#   breathing AND true-peak safety at once (the real "tunable ceiling").
# 300ms-range is level-invariant, so range is comparable across configs w/o loudness match.
OFF = False
CONFIGS = {
    "breather (FCoff, rel300)":      dict(clipper_active=OFF, dev_final_ceiling=False, dev_la_release_ms=300.0),  # max breathing, NOT TP-safe
    "fast-all (FC5, rel8)":          dict(clipper_active=OFF, dev_final_ceiling=True, dev_fc_release_ms=5.0,   dev_la_release_ms=8.0),
    "safe combo (FC5, rel300)":      dict(clipper_active=OFF, dev_final_ceiling=True, dev_fc_release_ms=5.0,   dev_la_release_ms=300.0),  # <-- candidate
    "safe combo (FC25, rel300)":     dict(clipper_active=OFF, dev_final_ceiling=True, dev_fc_release_ms=25.0,  dev_la_release_ms=300.0),
    "old-FC (FC100, rel300)":        dict(clipper_active=OFF, dev_final_ceiling=True, dev_fc_release_ms=100.0, dev_la_release_ms=300.0),  # emulate hardcoded flattener
    "safe+clip (FC5,rel300,clip-3)": dict(clipper_active=True, clipper_position="Post", clipper_mode="Hard", clipper_db=-3.0, dev_final_ceiling=True, dev_fc_release_ms=5.0, dev_la_release_ms=300.0),
}

if __name__ == "__main__":
    raw, sr = sf.read(RAW)
    assert sr == SR
    print(f"RAW source: {RAW}  ({len(raw)/sr:.0f}s)")
    m = raw.mean(1)
    print(f"  raw RMS {20*np.log10(np.sqrt(np.mean(m**2))+1e-12):.2f}  "
          f"300ms-range {st_range(m,sr):.2f}\n")

    print("Ozone ref (DIFFERENT song — metric anchor only):")
    oz, _ = sf.read(OZONE)
    measure(oz, sr, "Ozone IRC1 out")
    print()

    print("Our plugin @ input_gain +9.6, ceiling -1 TP, Lookahead/2-pole:")
    results = {}
    for name, cfg in CONFIGS.items():
        y = render(raw, cfg)
        results[name] = measure(y, sr, name)

    base = results["breather (FCoff, rel300)"]
    print("\nDelta vs breather (the max-breathing, TP-UNSAFE reference):")
    for name, r in results.items():
        if name.startswith("breather"):
            continue
        print(f"  {name:30s} dRange {r['rng']-base['rng']:+5.2f}  "
              f"TP {r['tp']:+6.2f}  (breather range {base['rng']:.2f}, TP {base['tp']:+.2f})")
