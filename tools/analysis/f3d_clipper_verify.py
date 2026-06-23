#!/usr/bin/env python3
"""F-3d Part B offline verification: clipper 8x OS IMD, Color recon, latency."""
import os, sys, numpy as np, soundfile as sf
from scipy.signal import get_window, resample_poly
from pedalboard import load_plugin

PLUGIN = os.environ.get(
    "ML_PLUGIN",
    "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build/"
    "MasterLimiter_artefacts/Release/AU/MasterLimiter.component",
)
SR = 48000
DUR = 2.0
OUT = os.path.join(os.path.dirname(__file__), "out")
os.makedirs(OUT, exist_ok=True)


def render(params, sig):
    p = load_plugin(PLUGIN)
    for k, v in params.items():
        assert k in p.parameters, f"unknown param {k!r}; have {sorted(p.parameters)}"
        setattr(p, k, v)
    return p(sig, SR)


def level_db_at(mono, sr, f_hz, hw=25.0):
    n = min(len(mono), 1 << 18)
    seg = mono[len(mono) // 4 : len(mono) // 4 + n]
    seg = seg - np.mean(seg)
    w = get_window("hann", len(seg))
    mag = np.abs(np.fft.rfft(seg * w))
    freqs = np.fft.rfftfreq(len(seg), 1.0 / sr)
    sel = (freqs >= f_hz - hw) & (freqs <= f_hz + hw)
    if not np.any(sel):
        return -200.0
    return 20 * np.log10(np.max(mag[sel]) + 1e-20)


def two_tone(f1, f2, peak):
    t = np.arange(int(SR * DUR)) / SR
    x = 0.5 * peak * (np.sin(2 * np.pi * f1 * t) + np.sin(2 * np.pi * f2 * t))
    return np.stack([x, x], axis=1).astype(np.float32)


def clipper_imd_test():
    f1, f2 = 11000.0, 13000.0
    # ~1.9 dB into clip: clipper threshold −3 dB, two-tone peak 0.8
    peak = 0.8
    x = two_tone(f1, f2, peak)
    params = {
        "limiter_active": True,
        "clipper_active": True,
        "clipper": -3.0,
        "clipper_mode": "Hard",
        "input_gain": 0.0,
        "color": 50.0,
        "ceiling": 0.0,
    }
    y = render(params, x)
    sf.write(os.path.join(OUT, "f3d_twotone_clip.wav"), y, SR)
    mono = y.mean(axis=1)

    ref_car = level_db_at(0.5 * peak * (np.sin(2*np.pi*f1*np.arange(int(SR*0.5))/SR) +
                          np.sin(2*np.pi*f2*np.arange(int(SR*0.5))/SR)), SR, f2)
    # relative to output carrier at f2
    car_db = level_db_at(mono, SR, f2)
    imd_9k = level_db_at(mono, SR, 2 * f1 - f2) - car_db
    imd_15k = level_db_at(mono, SR, 2 * f2 - f1) - car_db
    # aliased fold products (mirror pairs near 24k band edge for 4x marginal clip)
    alias_5k = level_db_at(mono, SR, 5000.0) - car_db
    alias_19k = level_db_at(mono, SR, 19000.0) - car_db
    alias_22k = level_db_at(mono, SR, 22000.0) - car_db
    worst_alias = max(alias_5k, alias_19k, alias_22k)
    print("=== Clipper two-tone (11k+13k, hard) ===")
    print(f"  carrier f2 level: {car_db:.1f} dBFS (abs)")
    print(f"  real IMD  9 kHz (2f1-f2): {imd_9k:6.1f} dB rel carrier")
    print(f"  real IMD 15 kHz (2f2-f1): {imd_15k:6.1f} dB rel carrier")
    print(f"  alias  5 kHz: {alias_5k:6.1f} dB  |  19 kHz: {alias_19k:6.1f} dB  |  22 kHz: {alias_22k:6.1f} dB")
    print(f"  worst alias bin: {worst_alias:6.1f} dB (pre-fix baseline ~−34 dB; need ≥10 dB drop → ≤−44 dB)")
    ok_alias = worst_alias <= -44.0
    ok_imd = imd_9k < -20.0 and imd_15k < -20.0  # real cubic IMD present, not washed out
    print(f"  gate alias drop: {'PASS' if ok_alias else 'FAIL'}  |  gate real IMD present: {'PASS' if ok_imd else 'FAIL'}")
    return dict(imd_9k=imd_9k, imd_15k=imd_15k, worst_alias=worst_alias)


def color_recon_test():
    print("\n=== Color reconstruction (sine @ 1 kHz) ===")
    t = np.arange(int(SR * DUR)) / SR
    x = (0.25 * np.sin(2 * np.pi * 1000 * t)).astype(np.float32)
    x = np.stack([x, x], axis=1)
    for color in (0, 25, 50, 75, 100):
        y = render({
            "limiter_active": True,
            "clipper_active": False,
            "input_gain": 0.0,
            "color": float(color),
            "ceiling": 0.0,
        }, x)
        # compare RMS gain vs dry (latency-independent magnitude ratio in steady state)
        n0 = int(DUR * SR // 4)
        n1 = int(3 * DUR * SR // 4)
        mono = y[n0:n1, 0]
        ref = x[n0:n1, 0]
        g = np.sqrt(np.mean(mono**2) / (np.mean(ref**2) + 1e-30))
        err_db = 20 * np.log10(g + 1e-12)
        print(f"  Color {color:3d}: gain err {err_db:+.4f} dB")


def latency_test():
    print("\n=== Reported plugin latency ===")
    for clip_on in (True, False):
        p = load_plugin(PLUGIN)
        p.limiter_active = True
        p.clipper_active = clip_on
        lat = getattr(p, "reported_latency_samples", None)
        if lat is None:
            # pedalboard may not expose; try processing impulse
            lat = "n/a (check DAW/host)"
        print(f"  clipper={'on' if clip_on else 'off'}: latency={lat}")


if __name__ == "__main__":
    clipper_imd_test()
    color_recon_test()
    latency_test()
