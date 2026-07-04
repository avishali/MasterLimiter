#!/usr/bin/env python3
"""SLICE_CLIPPER_PREPOST offline verification: Pre null, latency, Post rig."""
import os
import numpy as np
from pedalboard import load_plugin

PLUGIN = os.environ.get(
    "ML_PLUGIN",
    "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build/"
    "MasterLimiter_artefacts/Release/AU/MasterLimiter.component",
)
SR = 48000
DUR = 2.0


def render(params, sig):
    p = load_plugin(PLUGIN)
    for k, v in params.items():
        try:
            setattr(p, k, v)
        except AttributeError as e:
            raise AssertionError(f"unknown param {k!r}; have {sorted(p.parameters)}") from e
    return p(sig, SR)


def peak_db(mono):
    return 20 * np.log10(np.max(np.abs(mono)) + 1e-20)


def crest_db(mono):
    rms = np.sqrt(np.mean(mono**2) + 1e-30)
    return 20 * np.log10(np.max(np.abs(mono)) / rms + 1e-20)


def bass_transient_mix(peak=0.95):
    t = np.arange(int(SR * DUR)) / SR
    bass = 0.7 * peak * np.sin(2 * np.pi * 100 * t)
    burst = np.zeros_like(t)
    for start in range(int(0.25 * SR), int(DUR * SR), int(0.5 * SR)):
        n = min(int(0.003 * SR), len(t) - start)
        burst[start : start + n] = peak * np.hanning(n)
    x = (bass + burst).astype(np.float32)
    return np.stack([x, x], axis=1)


def pre_null_test():
    print("=== Pre-path deterministic null (two identical Pre renders) ===")
    x = bass_transient_mix()
    base = {
        "limiter_active": True,
        "clipper_active": True,
        "clipper": -4.0,
        "clipper_mode": "Hard",
        "clipper_position": "Pre",
        "input_gain": 12.0,
        "color": 0.0,
        "ceiling": -1.0,
        "ceiling_mode": "TruePeak",
    }
    y1 = render(base, x)
    y2 = render(base, x)
    diff = np.max(np.abs(y1 - y2))
    print(f"  max |y1-y2| = {diff:.3e}  ({'PASS' if diff == 0.0 else 'FAIL'})")

    for active in (True, False):
        p = dict(base)
        p["clipper_active"] = active
        y = render(p, x)
        print(f"  clipper_active={active}: peak={peak_db(y.mean(1)):.2f} dBFS")


def latency_test():
    print("\n=== Latency Pre vs Post ===")
    lats = {}
    for pos in ("Pre", "Post"):
        for active in (True, False):
            p = load_plugin(PLUGIN)
            p.limiter_active = True
            p.clipper_active = active
            p.clipper_position = pos
            key = f"{pos}/active={active}"
            lats[key] = getattr(p, "reported_latency_samples", None)
            print(f"  {key}: {lats[key]}")
    pre_lat = lats["Pre/active=True"]
    post_lat = lats["Post/active=True"]
    ok = pre_lat == post_lat
    print(f"  Pre vs Post (active): {'PASS' if ok else 'FAIL'} ({pre_lat} vs {post_lat})")


def post_rig():
    print("\n=== Post clipper rig (bass + transient, ceiling -1, FC TruePeak) ===")
    x = bass_transient_mix()
    common = {
        "limiter_active": True,
        "input_gain": 12.0,
        "color": 0.0,
        "ceiling": -1.0,
        "ceiling_mode": "TruePeak",
        "clipper": -3.0,
        "clipper_mode": "Hard",
    }
    y_none = render({**common, "clipper_active": False, "clipper_position": "Pre"}, x)
    y_pre = render({**common, "clipper_active": True, "clipper_position": "Pre"}, x)
    y_post = render({**common, "clipper_active": True, "clipper_position": "Post"}, x)

    for tag, y in [("no clip", y_none), ("Pre clip", y_pre), ("Post clip", y_post)]:
        mono = y[int(DUR * SR // 4) :, 0]
        print(
            f"  {tag:10s}  SP={peak_db(mono):6.2f} dBFS  crest={crest_db(mono):5.1f} dB"
        )

    mono_post = y_post[int(DUR * SR // 4) :, 0]
    sp = peak_db(mono_post)
    tp_ok = sp <= -1.0 + 0.15  # TruePeak FC should hold near ceiling
    crest_drop = crest_db(mono_post) < crest_db(y_none[int(DUR * SR // 4) :, 0])
    print(f"  Post SP <= ceiling: {'PASS' if tp_ok else 'FAIL'} ({sp:.2f} vs -1.0)")
    print(f"  Post crest < no-clip: {'PASS' if crest_drop else 'FAIL'}")


if __name__ == "__main__":
    pre_null_test()
    latency_test()
    post_rig()
