#!/usr/bin/env python3
"""FRONTIER measurement — where does the Open engine actually sit against the MODERN references?

Every parity claim we own is against **Ozone IRC 1** (their oldest, weakest single-band mode),
via a pre-rendered file. This renders the SAME sources through the references LIVE and on ONE
axis, so "are we at the frontier?" becomes a measured question instead of an assumption:

    - Ozone 11 Maximizer  - every IRC mode        (the historical benchmark, all modes)
    - Ozone 12 Maximizer  - every IRC mode        (adds the newest spectral mode)
    - FabFilter Pro-L 2   - all 8 styles          (the reference every reviewer will A/B against)
    - MasterLimiter       - Open and Transparent  (measured live, not quoted from an old run)

Method (identical to the existing rig so numbers stay comparable with SPECTRAL_ENGINE_DESIGN.md):
    - loudness-match each config to the genre's `target_rms` by searching input drive
    - ceiling -1 dB, sample-peak convention (Ozone "Prevent Intersample Clipping" off,
      Pro-L true-peak off) -- true peak is REPORTED, not enforced, so the TP cost is visible
    - metric = `st_range` (300 ms short-term p95-p10) = the macro-dynamic "breathing" figure

RIG VALIDATION (the reason to trust the rest): the pre-rendered `test_ozone_11 mix N.wav`
reference is measured too, and the Ozone-11 mode whose live render lands closest to it is
reported as the empirical IRC-1 identification. If no mode matches the anchor, the live-driving
approach is wrong and every other number here is suspect -- the script says so explicitly.

Claude's role (orchestration + measurement); the DSP is the C++ plugin / the reference plugins.

Usage:  ./.venv/bin/python mbl_frontier.py [--quick]
"""
import sys
import numpy as np
import soundfile as sf
from pedalboard import load_plugin

import ozone_state as oz
from spectral_proto import st_range, true_peak_db, rms_db, sample_peak_db, GENRES, SR

OZONE_11 = "/Library/Audio/Plug-Ins/VST3/Ozone 11 Maximizer.vst3"
OZONE_12 = "/Library/Audio/Plug-Ins/VST3/Ozone 12 Maximizer.vst3"
PRO_L2 = "/Library/Audio/Plug-Ins/VST3/FabFilter Pro-L 2.vst3"
OURS = "/Users/avishaylidani/Library/Audio/Plug-Ins/VST3/MasterLimiter.vst3"

CEIL_DB = -1.0
QUICK = "--quick" in sys.argv


def meas(y):
    mono = y.mean(1) if y.ndim > 1 else y
    return dict(rms=rms_db(mono), rng=st_range(mono),
                tp=true_peak_db(y), spk=sample_peak_db(y))


def match(render, target_rms, gmax):
    """Search input drive so output RMS ~= target_rms. Coarse grid then refine -- the same
    shape as the existing rig (never bisection: multiband RMS-vs-drive is not monotonic)."""
    step = 3.0 if QUICK else 2.0
    best = None
    for g in np.arange(0.0, gmax + 0.01, step):
        m = meas(render(float(g)))
        if best is None or abs(m["rms"] - target_rms) < abs(best[1]["rms"] - target_rms):
            best = (float(g), m)
    lo, hi = max(0.0, best[0] - step * 0.75), min(gmax, best[0] + step * 0.75)
    for g in np.arange(lo, hi + 0.01, 0.5):
        m = meas(render(float(g)))
        if abs(m["rms"] - target_rms) < abs(best[1]["rms"] - target_rms):
            best = (float(g), m)
    return best


# ---------------------------------------------------------------- reference renderers

def ozone_configs(path, label):
    p = load_plugin(path)
    base = bytes(p.preset_data)
    n = oz.num_modes(p, base)
    out = []
    for mode in range(n):
        def render(gain, _p=p, _b=base, _m=mode):
            _p.preset_data = oz.set_params(_b, Mode=_m, Gain=gain, Margin=CEIL_DB)
            return _p(x_cur, SR)
        out.append((f"{label} mode {mode}", render, 20.0))
    return out


def prol2_configs():
    p = load_plugin(PRO_L2)
    styles = p.parameters["style"].valid_values
    out = []
    for style in styles:
        def render(gain, _p=p, _s=style):
            _p.style = _s
            _p.true_peak_limiting = False
            _p.oversampling = "Off"
            _p.output_level = CEIL_DB
            _p.gain = float(gain)
            return _p(x_cur, SR)
        out.append((f"Pro-L 2 {_short(style)}", render, 24.0))
    return out


def ours_configs():
    p = load_plugin(OURS)

    names = set(p.parameters.keys())

    def put(attr, value):
        """pedalboard SILENTLY accepts a set to a non-existent attribute (it just creates a Python
        attribute), and it names attributes from the DISPLAY name -- so CLIP-1's "Clipper*" ->
        "Drive*" relabel turned three setters here into no-ops without any error. Fail loudly."""
        if attr not in names:
            raise KeyError(f"{attr!r} does not exist on this build "
                           f"-- a silent no-op would misconfigure the measurement")
        setattr(p, attr, value)

    def setup(gain, mb):
        put("dev_mb_engine", mb)
        if mb:
            put("dev_mb_crossover_hz", 120.0)
            put("dev_mb_attack_mode", "Ramp")
            put("dev_mb_release_ms", 150.0)
            put("dev_mb_safety", False)
        put("limiter_active", True)
        # Post-CLIP-1 the peak tip-catch is the CEILING stage (release = Clip), not the user clipper.
        # Drive is now a separate PRE tone control and stays OFF for a clean engine measurement.
        put("drive_active", False)
        put("drive_db", 0.0)
        put("ceiling_active", True)
        put("ceiling_release_ms", "Clip")
        put("ceiling_mode", "SamplePeak")
        put("ceiling_db", CEIL_DB)
        put("auto_track", False)
        put("input_gain_db", float(gain))
        return p(x_cur, SR)

    return [("MasterLimiter OPEN", lambda g: setup(g, True), 24.0),
            ("MasterLimiter TRANSPARENT", lambda g: setup(g, False), 24.0)]


def _short(s):
    return s if len(s) <= 12 else s[:12]


# ---------------------------------------------------------------- main

if __name__ == "__main__":
    print("FRONTIER measurement - Open engine vs the modern references, one axis, live renders")
    print(f"ceiling {CEIL_DB} dB sample-peak; metric = 300 ms short-term range (p95-p10); "
          f"loudness-matched per genre{'  [QUICK]' if QUICK else ''}\n")

    for g in GENRES:
        x_cur, sr = sf.read(g["src"])
        assert sr == SR, f"{g['name']} is {sr} Hz, rig is {SR}"
        target = g["target_rms"]

        anchor_y, _ = sf.read(g["ozone"])
        anchor = meas(anchor_y)

        print("=" * 104)
        print(f"{g['name']}   source {x_cur.shape[0]/SR:.0f}s   matched to RMS {target:.2f}")
        print(f"  ANCHOR  pre-rendered Ozone-11 IRC1 file : "
              f"RMS {anchor['rms']:7.2f}  range {anchor['rng']:5.2f}  "
              f"TP {anchor['tp']:6.2f}  sPk {anchor['spk']:6.2f}")
        print("-" * 104)
        print(f"  {'config':30s} {'drive':>6} {'RMS':>7} {'range':>6} {'TP':>6} {'sPk':>6}   vs anchor range")

        configs = (ozone_configs(OZONE_11, "Ozone11") + ozone_configs(OZONE_12, "Ozone12")
                   + prol2_configs() + ours_configs())

        rows = []
        for label, render, gmax in configs:
            try:
                drive, m = match(render, target, gmax)
            except Exception as e:                       # a reference may refuse to load
                print(f"  {label:30s}  FAILED: {type(e).__name__}: {e}")
                continue
            rows.append((label, drive, m))
            print(f"  {label:30s} {drive:+6.1f} {m['rms']:7.2f} {m['rng']:6.2f} "
                  f"{m['tp']:6.2f} {m['spk']:6.2f}   {m['rng']-anchor['rng']:+6.2f}")

        # --- rig validation: which Ozone-11 mode reproduces the pre-rendered IRC1 anchor?
        oz11 = [r for r in rows if r[0].startswith("Ozone11")]
        if oz11:
            best = min(oz11, key=lambda r: abs(r[2]["rng"] - anchor["rng"]))
            err = abs(best[2]["rng"] - anchor["rng"])
            verdict = "VALIDATED" if err < 0.35 else "*** NO MODE MATCHES THE ANCHOR ***"
            print(f"\n  rig check: closest Ozone-11 mode to the IRC1 anchor = {best[0]} "
                  f"(range {best[2]['rng']:.2f} vs {anchor['rng']:.2f}, err {err:.2f})  -> {verdict}")

        ours = [r for r in rows if r[0].startswith("MasterLimiter OPEN")]
        others = [r for r in rows if not r[0].startswith("MasterLimiter")]
        if ours and others:
            top = max(others, key=lambda r: r[2]["rng"])
            gap = ours[0][2]["rng"] - top[2]["rng"]
            print(f"  frontier gap: OPEN {ours[0][2]['rng']:.2f} vs best reference "
                  f"{top[0]} {top[2]['rng']:.2f}  ->  {gap:+.2f} dB")
        print()
