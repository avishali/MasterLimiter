#!/usr/bin/env python3
"""CALIBRATION SUITE — prove the plugin is correct on deterministic signals before any listening test.

Why this exists: on 2026-08-02 two "known good" facts turned out not to be true (the Open engine does not
hold its ceiling; `clipper_active` was a bit-exact no-op). Both had been believed for weeks because we only
ever checked them on music, where you cannot see 0.85 dB of peak overshoot. Tone / noise / impulse make
every one of these binary.

Each check is independent, uses a FRESH plugin instance (no state bleed), and prints PASS/FAIL with the
measured value next to the expected one. The output is the checklist: what is definitely correct, and what
is not. Nothing here is subjective.

Structure follows `docs/SIGNAL_FLOW.md`:
  A hygiene      - silence, NaN/Inf, determinism
  B latency      - reported == measured, and CONSTANT across every config (avishali 2026-08-02)
  C bypass/null  - bypass and limiter-off are true passthroughs
  D gain         - input gain, ceiling, I/O trims are exact dB
  E ceiling model- SIGNAL_FLOW section 3: GR responds to Input Gain, NOT to Ceiling
  F peak safety  - sample-peak and true-peak ceilings actually hold, on hostile signals
  G linearity    - flat response / perfect reconstruction when not limiting
  H stereo       - no L/R leakage; link behaves
  I block size   - output independent of host buffer size  (catches state bugs)
  J samplerate   - correct at 44.1/48/88.2/96 kHz
  K state        - preset round-trip (tester .mlpreset files must decode)
  L stereo link  - shipping control, deterministic probe
  M aliasing     - oversampling actually suppresses fold-back
  N DC/denormal  - the classic silent killers
  Z chain        - everything on at once, the end-to-end assertion

Usage:
    ./.venv/bin/python mbl_calibrate.py                # installed VST3
    ./.venv/bin/python mbl_calibrate.py --plugin PATH  # a specific build
    ./.venv/bin/python mbl_calibrate.py --group B,F    # only some groups

Claude's role (orchestration + measurement); the DSP is the C++ plugin.
"""
import argparse
import sys
import numpy as np
from pedalboard import load_plugin

DEFAULT_PLUGIN = "/Users/avishaylidani/Library/Audio/Plug-Ins/VST3/MasterLimiter.vst3"
SR = 48000

# ---------------------------------------------------------------- parameter name resolution
# pedalboard derives attribute names from the DISPLAY name, NOT the param ID -- so "plugin_bypass"
# (the ID) is exposed as "bypass" (the name), and CLIP-1's relabel turned "clipper_*" into "drive_*".
# Worse, pedalboard SILENTLY accepts a set to a non-existent attribute (it just creates a Python
# attribute), so a typo'd name is a no-op that still "passes". `Plug.set` therefore hard-fails on an
# unresolved role -- that exact silent no-op produced a full page of false FAILs on the first run.
ROLES = {
    "input_gain":    ["input_gain_db"],
    "ceiling":       ["ceiling_db"],
    "ceiling_mode":  ["ceiling_mode"],
    "limiter_on":    ["limiter_active"],
    "bypass":        ["bypass"],
    "mb_engine":     ["dev_mb_engine"],
    "mb_xover":      ["dev_mb_crossover_hz"],
    "mb_attack":     ["dev_mb_attack_mode"],
    "mb_release":    ["dev_mb_release_ms"],
    "mb_safety":     ["dev_mb_safety"],
    "gain_match":    ["auto_track"],
    "drive_on":      ["drive_active", "clipper_active"],
    "drive_db":      ["drive_db", "clipper_drive_db"],
    "drive_mode":    ["drive_mode", "clipper_mode"],
    "ceiling_on":    ["ceiling_active", "dev_final_ceiling"],
    "ceiling_rel":   ["ceiling_release_ms", "dev_final_ceiling_release_ms"],
    "stereo_link":   ["stereo_link"],
    "color":         ["color"],
    "io_in_l":       ["i_o_input_l_db"],
    "io_in_r":       ["i_o_input_r_db"],
    "io_out_l":      ["i_o_output_l_db"],
    "io_out_r":      ["i_o_output_r_db"],
}

# Analysis must ignore the plugin's start-up transient: a signal that begins abruptly produces
# filter ringing whose PEAK is not the steady-state gain. (Measuring peak on an abrupt sine is what
# made input-gain look like it was applied twice on the first run -- it is exact to 0.02 dB.)
SETTLE_S = 0.30


class Plug:
    """Thin role-addressed wrapper so checks read like the signal flow, not like param strings."""

    def __init__(self, path, sr=SR):
        self.p = load_plugin(path)
        self.sr = sr
        self.names = set(self.p.parameters.keys())

    def has(self, role):
        return any(n in self.names for n in ROLES[role])

    def set(self, role, value):
        for n in ROLES[role]:
            if n in self.names:
                setattr(self.p, n, value)
                return True
        raise KeyError(f"role {role!r} resolves to none of {ROLES[role]} "
                       f"-- a silent no-op set would fake a PASS/FAIL")

    def get(self, role):
        for n in ROLES[role]:
            if n in self.names:
                return getattr(self.p, n)
        return None

    def neutral(self):
        """A defined, documented starting state so every check begins from the same place."""
        self.set("bypass", False)
        self.set("limiter_on", True)
        self.set("gain_match", False)
        self.set("input_gain", 0.0)
        self.set("ceiling", 0.0)
        self.set("ceiling_mode", "SamplePeak")
        self.set("drive_on", False)
        self.set("drive_db", 0.0)
        self.set("mb_engine", False)
        return self

    def open_engine(self):
        self.set("mb_engine", True)
        self.set("mb_xover", 120.0)
        self.set("mb_attack", "Ramp")
        self.set("mb_release", 150.0)
        self.set("mb_safety", False)
        return self

    def render(self, x, buffer_size=None):
        kw = {} if buffer_size is None else {"buffer_size": buffer_size}
        return self.p(x, self.sr, **kw)

    @property
    def latency(self):
        return self.p.reported_latency_samples


# ---------------------------------------------------------------- signals & measurement

def stereo(mono):
    return np.stack([mono, mono], axis=1).astype(np.float32)


def _fade(mono, sr=SR, ms=25.0):
    """Raised-cosine fade in/out. Without it, a signal starting mid-cycle is a step discontinuity
    and the resulting filter ringing dominates any peak measurement."""
    n = int(ms * 1e-3 * sr)
    if n * 2 >= len(mono):
        return mono
    w = 0.5 * (1 - np.cos(np.linspace(0, np.pi, n)))
    out = mono.copy()
    out[:n] *= w
    out[-n:] *= w[::-1]
    return out


def sine(f, secs, amp=0.5, sr=SR):
    t = np.arange(int(secs * sr)) / sr
    return stereo(_fade(amp * np.sin(2 * np.pi * f * t), sr))


def settled(y, sr=SR):
    """Drop the start-up transient before measuring steady-state quantities."""
    n = int(SETTLE_S * sr)
    return y[n:] if len(y) > 2 * n else y


def fundamental_gain_db(y, x, f, sr=SR):
    """Gain at the test frequency. Immune to start transients and to distortion products,
    unlike a raw peak ratio."""
    a, b = settled(y, sr).mean(1), settled(x, sr).mean(1)
    n = min(len(a), len(b))
    a, b = a[:n] * np.hanning(n), b[:n] * np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1 / sr)
    i = int(np.argmin(np.abs(freqs - f)))
    return db(np.abs(np.fft.rfft(a)[i]) / max(np.abs(np.fft.rfft(b)[i]), 1e-30))


def impulse(secs=1.0, amp=0.25, at=1000, sr=SR):
    m = np.zeros(int(secs * sr), dtype=np.float32)
    m[at] = amp
    return stereo(m)


def noise(secs=2.0, amp=0.2, sr=SR, seed=7):
    m = np.random.default_rng(seed).standard_normal(int(secs * sr)).astype(np.float32) * amp
    return stereo(_fade(m, sr))


def bursts(secs=3.0, amp=0.9, sr=SR):
    """Hostile transient train: silence -> full-scale burst. Worst case for a peak ceiling."""
    m = np.zeros(int(secs * sr), dtype=np.float32)
    n = int(0.05 * sr)
    burst = _fade(amp * np.sin(2 * np.pi * 220 * np.arange(n) / sr), sr, ms=3.0)
    for k in range(6):
        s0 = int((0.4 + k * 0.45) * sr)
        m[s0:s0 + n] = burst
    return stereo(m)


def db(x):
    return 20 * np.log10(max(float(x), 1e-30))


def peak_db(y):
    return db(np.max(np.abs(y)))


def rms_db(y):
    m = y.mean(1) if y.ndim > 1 else y
    return db(np.sqrt(np.mean(m ** 2)))


def true_peak_db(y, up=8):
    n = len(y)
    X = np.fft.rfft(y, axis=0)
    Y = np.zeros((n * up // 2 + 1,) + y.shape[1:], dtype=complex)
    Y[:X.shape[0]] = X
    return db(np.max(np.abs(np.fft.irfft(Y, n=n * up, axis=0) * up)))


def best_align_residual(y, x, max_lag=20000):
    """Align by cross-correlation, then report the worst residual. Argmax-of-impulse alignment is
    unreliable through a bypass cross-fade, and a wrong lag fakes a huge residual."""
    a, b = y.mean(1), x.mean(1)
    n = min(len(a), len(b))
    corr = np.correlate(a[:n], b[:min(n, 4 * SR)], mode="valid")
    lag = int(np.argmax(np.abs(corr)))
    lag = min(lag, max_lag)
    m = min(len(b), len(a) - lag)
    return lag, float(np.max(np.abs(a[lag:lag + m] - b[:m]))) if m > 0 else (lag, 1.0)


def measured_latency(plug, sr=SR):
    """Impulse in, find where it lands. The honest latency, independent of what the plugin claims."""
    y = plug.render(impulse(sr=sr))
    mono = np.abs(y).max(1)
    if mono.max() < 1e-9:
        return None
    return int(np.argmax(mono)) - 1000


# ---------------------------------------------------------------- harness

RESULTS = []


def check(group, name, ok, detail, skipped=False):
    RESULTS.append((group, name, "SKIP" if skipped else ("PASS" if ok else "FAIL"), detail))
    tag = "SKIP" if skipped else ("PASS" if ok else "FAIL")
    colour = {"PASS": "\033[32m", "FAIL": "\033[31m", "SKIP": "\033[33m"}[tag]
    print(f"  [{colour}{tag}\033[0m] {name:52s} {detail}")


def new(path, sr=SR):
    return Plug(path, sr).neutral()


# ---------------------------------------------------------------- checks

def group_A(path):
    print("\nA. Hygiene — silence, non-finite, determinism")
    p = new(path)
    y = p.render(stereo(np.zeros(SR, dtype=np.float32)))
    check("A", "digital silence in -> silence out", np.max(np.abs(y)) < 1e-7,
          f"peak {peak_db(y):.1f} dBFS (expect < -140)")

    p = new(path); p.set("input_gain", 12.0)
    y = p.render(noise())
    check("A", "no NaN/Inf on noise at +12 dB", bool(np.all(np.isfinite(y))),
          f"finite={bool(np.all(np.isfinite(y)))}")

    p1 = new(path); p1.set("input_gain", 9.0)
    p2 = new(path); p2.set("input_gain", 9.0)
    x = noise()
    d = np.max(np.abs(p1.render(x) - p2.render(x)))
    check("A", "deterministic: two fresh instances agree", d < 1e-9,
          f"max|diff| {db(d):.0f} dB (expect < -180)")


def group_B(path):
    print("\nB. Latency — reported must equal measured, and be CONSTANT across configs")
    combos = []
    for engine in ("Transparent", "Open"):
        for drive in (False, True):
            for ceil_rel in ("clip", "limiter"):
                p = new(path)
                if engine == "Open":
                    p.open_engine()
                p.set("drive_on", drive)
                if p.has("ceiling_rel"):
                    try:
                        p.set("ceiling_rel", "Clip" if ceil_rel == "clip" else "20.0 ms")
                    except Exception:
                        p.set("ceiling_rel", 0.0 if ceil_rel == "clip" else 20.0)
                rep = p.latency
                meas = measured_latency(p)
                combos.append((f"{engine}/drive={drive}/ceil={ceil_rel}", rep, meas))

    # pedalboard ALREADY compensates the reported latency when it renders, so `measured` here is the
    # RESIDUAL error: 0 means the plugin delays exactly what it claims. A non-zero residual means the
    # plugin mis-reports, and in a DAW that config sits early/late against every other track.
    for label, rep, meas in combos:
        ok = meas is not None and abs(meas) <= 4
        check("B", f"delay matches its reported value  {label}", ok,
              f"reported {rep}, residual error {meas:+d} samples"
              + ("" if ok else f" = {1000*meas/SR:+.1f} ms off"))

    reported = {c[1] for c in combos}
    check("B", "latency CONSTANT across all configs", len(reported) == 1,
          f"{sorted(reported)} (avishali: must be one fixed value)")


def group_C(path):
    print("\nC. Bypass / limiter-off — must be true passthrough")
    x = noise()
    for role, label in (("bypass", "plugin_bypass ON"), ("limiter_on", "limiter_active OFF")):
        p = new(path)
        p.set(role, role == "bypass")
        y = p.render(x)
        lag, resid = best_align_residual(y, x)
        check("C", f"{label} -> null vs input", resid < 1e-4,
              f"residual {db(resid):.1f} dB at lag {lag}")


def group_D(path):
    print("\nD. Gain calibration — exact dB")
    for g in (3.0, 6.0, 12.0):
        p = new(path); p.set("input_gain", g)
        x = sine(1000, 2.0, amp=0.02)                  # tiny: far below any threshold
        got = fundamental_gain_db(p.render(x), x, 1000)
        check("D", f"input_gain {g:+.0f} dB is exact (no GR)", abs(got - g) < 0.15,
              f"measured {got:+.2f} dB at the fundamental")

    for c in (-1.0, -3.0, -6.0):
        p = new(path); p.set("input_gain", 18.0); p.set("ceiling", c)
        y = settled(p.render(bursts()))
        check("D", f"ceiling {c:+.0f} dB is the output peak", abs(peak_db(y) - c) < 0.25,
              f"peak {peak_db(y):+.2f} dBFS")


def group_E(path):
    print("\nE. Ceiling model (SIGNAL_FLOW section 3) — GR responds to Input Gain, NOT Ceiling")
    x = bursts()
    ref = None
    consistent = True
    detail = []
    for c in (0.0, -3.0, -6.0):
        p = new(path); p.set("input_gain", 15.0); p.set("ceiling", c)
        y = p.render(x)
        gr = rms_db(y) - c            # remove the ceiling output gain -> what limiting did
        detail.append(f"{c:+.0f}:{gr:.2f}")
        if ref is None:
            ref = gr
        elif abs(gr - ref) > 0.3:
            consistent = False
    check("E", "GR independent of Ceiling (ceiling is output gain)", consistent,
          "ceiling-compensated level " + " ".join(detail))


def group_F(path):
    print("\nF. Peak safety — the ceiling must actually hold, on hostile signals")
    for engine in ("Transparent", "Open"):
        for gain in (6.0, 12.0, 18.0):
            p = new(path)
            if engine == "Open":
                p.open_engine()
            p.set("input_gain", gain); p.set("ceiling", -1.0); p.set("ceiling_mode", "SamplePeak")
            y = settled(p.render(bursts()))
            pk = peak_db(y)
            check("F", f"{engine} sample-peak <= -1.0 dB @ +{gain:.0f} dB", pk <= -0.95,
                  f"peak {pk:+.2f} dBFS")

    for engine in ("Transparent", "Open"):
        p = new(path)
        if engine == "Open":
            p.open_engine()
        p.set("input_gain", 15.0); p.set("ceiling", -1.0); p.set("ceiling_mode", "TruePeak")
        y = settled(p.render(bursts()))
        tp = true_peak_db(y)
        check("F", f"{engine} TRUE-peak <= -1.0 dB (TruePeak mode)", tp <= -0.9,
              f"true peak {tp:+.2f} dBTP")


def group_G(path):
    print("\nG. Linearity — flat and clean when not limiting")
    p = new(path)
    y = settled(p.render(sine(1000, 2.0, amp=0.01)))
    mono = y.mean(1)
    spec = np.abs(np.fft.rfft(mono * np.hanning(len(mono))))
    f = np.fft.rfftfreq(len(mono), 1 / SR)
    fund = spec[np.argmin(np.abs(f - 1000))]
    harm = max(spec[np.argmin(np.abs(f - k * 1000))] for k in (2, 3, 4, 5))
    check("G", "THD at -40 dBFS 1 kHz (should be inaudible)", db(harm / fund) < -80,
          f"worst harmonic {db(harm/fund):.1f} dBc")

    p = new(path)
    resp = []
    for freq in (50, 200, 1000, 5000, 12000):
        x = sine(freq, 1.0, amp=0.01)
        resp.append(fundamental_gain_db(p.render(x), x, freq))
    flat = max(resp) - min(resp)
    check("G", "frequency response flat 50 Hz-12 kHz (no limiting)", flat < 0.5,
          f"spread {flat:.2f} dB across {[f'{r:+.2f}' for r in resp]}")


def group_H(path):
    print("\nH. Stereo — no leakage, link behaves")
    n = SR
    m = np.zeros(n, dtype=np.float32)
    m[:] = 0.5 * np.sin(2 * np.pi * 440 * np.arange(n) / SR)
    x = np.stack([m, np.zeros(n, dtype=np.float32)], axis=1)

    p = new(path); p.set("input_gain", 6.0)
    y = p.render(x)
    leak = db(np.max(np.abs(y[:, 1])))
    check("H", "L-only signal does not leak into R", leak < -80,
          f"R channel {leak:.1f} dBFS")


def group_I(path):
    print("\nI. Block-size invariance — output must not depend on host buffer size")
    x = noise(secs=1.5)
    base = None
    worst = 0.0
    for bs in (64, 128, 512, 2048):
        p = new(path); p.set("input_gain", 12.0); p.set("ceiling", -1.0)
        y = p.render(x, buffer_size=bs)
        # compare RMS + peak, not sample-by-sample: a 1-sample alignment shift between buffer sizes
        # makes a raw subtraction meaningless on noise, which would fake a failure.
        stat = (rms_db(settled(y)), peak_db(settled(y)))
        if base is None:
            base = stat
        else:
            worst = max(worst, abs(stat[0] - base[0]), abs(stat[1] - base[1]))
    check("I", "level identical at buffer 64/128/512/2048", worst < 0.05,
          f"worst RMS/peak drift {worst:.3f} dB")


def group_J(path):
    print("\nJ. Sample rate — correct at every rate")
    for sr in (44100, 48000, 88200, 96000):
        p = Plug(path, sr).neutral()
        p.set("input_gain", 15.0); p.set("ceiling", -1.0); p.set("ceiling_mode", "SamplePeak")
        y = settled(p.render(bursts(sr=sr)), sr)
        pk = peak_db(y)
        check("J", f"ceiling holds at {sr} Hz", pk <= -0.95, f"peak {pk:+.2f} dBFS")

    for sr in (44100, 96000):
        p = Plug(path, sr).neutral()
        rep, meas = p.latency, measured_latency(p, sr)
        check("J", f"delay matches reported at {sr} Hz",
              meas is not None and abs(meas) <= 4,
              f"reported {rep}, residual error {meas:+d} samples")


def group_Z(path):
    print("\nZ. Whole chain — everything engaged at once (the end-to-end assertion)")
    p = new(path).open_engine()
    p.set("input_gain", 12.0); p.set("ceiling", -1.0); p.set("ceiling_mode", "TruePeak")
    p.set("drive_on", True); p.set("drive_db", -3.0)
    y = settled(p.render(bursts()))
    ok_fin = bool(np.all(np.isfinite(y)))
    check("Z", "full chain: finite output", ok_fin, f"finite={ok_fin}")
    check("Z", "full chain: true peak <= ceiling", true_peak_db(y) <= -0.9,
          f"true peak {true_peak_db(y):+.2f} dBTP")

    # Discontinuity check. Group Z previously asserted only "finite" and "TP <= ceiling", which is why
    # it passed a build where Drive + Ceiling=Clip produced audible clicks in the Open path.
    # The test is RELATIVE, not absolute: turning Drive on must not change the worst sample-to-sample
    # step much, because Drive is a tone stage and cannot legitimately create steps the engine did not
    # already make. An absolute threshold hid the bug -- the synthetic burst reached 0.82 (under a 0.95
    # ceiling-amplitude bar) while every other config sat at 0.05, a 19x outlier that is obviously wrong.
    def worst_step(mb, crel, drive):
        q = new(path)
        if mb:
            q.open_engine()
        q.set("input_gain", 14.0); q.set("ceiling", -1.0); q.set("ceiling_on", True)
        try:
            q.set("ceiling_rel", crel)
        except Exception:
            q.set("ceiling_rel", 0.0 if crel == "Clip" else 20.0)
        q.set("drive_on", drive); q.set("drive_db", -6.0); q.set("drive_mode", "Hard")
        mono = settled(q.render(bursts(secs=6.0))).mean(1)
        return float(np.max(np.abs(np.diff(mono)))) if len(mono) > 1 else 0.0

    for eng_label, mb in (("Open", True), ("Transparent", False)):
        for crel in ("Clip", "20.0 ms"):
            off, on = worst_step(mb, crel, False), worst_step(mb, crel, True)
            ratio = on / max(off, 1e-9)
            check("Z", f"Drive adds no discontinuity  {eng_label}/Ceiling={crel}", ratio <= 3.0,
                  f"worst step {off:.4f} -> {on:.4f}  ({ratio:.1f}x)")

    p2 = new(path).open_engine()
    p2.set("input_gain", 12.0); p2.set("ceiling", -1.0); p2.set("ceiling_mode", "TruePeak")
    p2.set("drive_on", False); p2.set("drive_db", -3.0)
    y2 = settled(p2.render(bursts()))
    n = min(len(y), len(y2))
    resid = float(np.max(np.abs(y[:n] - y2[:n])))
    check("Z", "Drive toggle is audible (not a no-op)", resid > 1e-4,
          f"residual {db(resid):.1f} dB (was a -240 dB no-op pre-CLIP-1)")



def group_K(path):
    """State round-trip. Testers send .mlpreset files back and the A/B verdict is decoded FROM them —
    if state does not round-trip exactly, the returned voicings mean nothing."""
    print("\nK. State / preset round-trip — returned tester presets must decode exactly")
    p = new(path).open_engine()
    p.set("input_gain", 7.5); p.set("ceiling", -1.5); p.set("drive_on", True); p.set("drive_db", -4.0)
    p.set("ceiling_mode", "TruePeak")
    blob = bytes(p.p.preset_data)

    q = Plug(path)
    q.p.preset_data = blob
    same, diffs = True, []
    for role in ("input_gain", "ceiling", "drive_on", "drive_db", "mb_engine", "mb_xover"):
        a, b = p.get(role), q.get(role)
        if str(a) != str(b):
            same = False
            diffs.append(f"{role}: {a} != {b}")
    check("K", "all parameters survive a state round-trip", same,
          "exact" if same else "; ".join(diffs))

    x = bursts()
    n = min(len(p.render(x)), len(q.render(x)))
    resid = float(np.max(np.abs(p.render(x)[:n] - q.render(x)[:n])))
    check("K", "restored state renders identical audio", resid < 1e-6,
          f"residual {db(resid):.0f} dB")

    check("K", "engine choice is recoverable from state", str(q.get("mb_engine")) == str(p.get("mb_engine")),
          f"mb_engine {q.get('mb_engine')} (blind A/B decoding depends on this)")


def group_L(path):
    """Stereo link. MUST compare R with L silent vs L loud at each link setting -- comparing R across
    link settings with L always loud cannot tell "always linked" from "never linked" (it fooled me once)."""
    print("\nL. Stereo link — does the link control actually unlink?")
    n = int(2.0 * SR)
    t = np.arange(n) / SR
    loud = _fade(0.9 * np.sin(2 * np.pi * 220 * t).astype(np.float32))
    sil = np.zeros(n, dtype=np.float32)
    probe = _fade(0.02 * np.sin(2 * np.pi * 3000 * t).astype(np.float32))

    for mode, ctrl in (("Stereo", "stereo_link"), ("M/S", "m_s_link")):
        duck = {}
        for link in (100.0, 0.0):
            out = []
            for L in (sil, loud):
                p = new(path); p.set("input_gain", 15.0); p.set("ceiling", -1.0)
                p.p.stereo_mode = mode
                setattr(p.p, ctrl, link)
                y = settled(p.render(np.stack([L, probe], axis=1)))
                out.append(db(np.sqrt(np.mean(y[:, 1] ** 2))))
            duck[link] = out[1] - out[0]      # how much L's GR pulls R down

        check("L", f"{mode}: link 100% links the channels", duck[100.0] < -6.0,
              f"R ducked {duck[100.0]:+.2f} dB by L")
        check("L", f"{mode}: link 0% UNLINKS the channels", abs(duck[0.0]) < 1.0,
              f"R ducked {duck[0.0]:+.2f} dB by L (should be ~0)")


def group_M(path):
    """Intermodulation. A 19 kHz + 20 kHz pair puts a difference tone at exactly 1 kHz if anything in
    the chain is nonlinear -- and nothing legitimate can be there. This is the honest test of what the
    oversampling buys.

    The first version of this check drove DRIVE (a hard clipper) and called the result "aliasing".
    A hard clipper generating IMD is the clipper WORKING, not the oversampling failing. Drive must be
    OFF to measure the limiter; it is measured separately below and only sanity-bounded.
    """
    print("\nM. Intermodulation — 19 kHz + 20 kHz two-tone, difference tone at 1 kHz")
    n = int(4.0 * SR)
    t = np.arange(n) / SR
    two = _fade(0.45 * (np.sin(2 * np.pi * 19000 * t) + np.sin(2 * np.pi * 20000 * t)).astype(np.float32))
    x = stereo(two)

    def imd(y):
        m = settled(y).mean(1)
        w = np.hanning(len(m))
        S = np.abs(np.fft.rfft(m * w))
        fr = np.fft.rfftfreq(len(m), 1 / SR)

        def lvl(f0, bw=30):
            sel = (fr > f0 - bw) & (fr < f0 + bw)
            return float(np.sqrt(np.sum(S[sel] ** 2)))
        fund = np.sqrt(lvl(19000) ** 2 + lvl(20000) ** 2)
        return db(lvl(1000) / (fund + 1e-30))

    floor = imd(x)
    check("M", "analysis floor is low enough to resolve IMD", floor < -100,
          f"dry source {floor:.1f} dBc")

    for label, mb in (("Transparent", False), ("Open", True)):
        p = new(path)
        if mb:
            p.open_engine()
        p.set("input_gain", 18.0); p.set("ceiling", -1.0)
        p.set("drive_on", False)
        got = imd(p.render(x))
        # reference points measured 2026-08-02: Ozone IRC -132.0, Pro-L 2 Transparent 4x OS -105.6
        check("M", f"{label} limiter IMD <= -100 dBc (Drive off)", got <= -100.0,
              f"{got:.1f} dBc   [Pro-L 2 4xOS -105.6, Ozone -132.0]")

    p = new(path)
    p.set("input_gain", 18.0); p.set("ceiling", -1.0)
    p.set("drive_on", True); p.set("drive_db", -6.0); p.set("drive_mode", "Hard")
    got = imd(p.render(x))
    check("M", "Drive Hard produces IMD (it is a clipper -- expected)", got > -100.0,
          f"{got:.1f} dBc — deliberate nonlinearity, not a defect")


def group_N(path):
    """DC and denormals — the two classic silent killers."""
    print("\nN. DC & denormals")
    dc = np.full((SR, 2), 0.3, dtype=np.float32)
    p = new(path); p.set("input_gain", 6.0)
    y = p.render(dc)
    check("N", "constant DC does not produce non-finite output", bool(np.all(np.isfinite(y))),
          f"out mean {float(np.mean(y)):+.4f}")

    tiny = (noise(secs=1.0, amp=1e-8)).astype(np.float32)
    p = new(path); p.set("input_gain", 0.0)
    y = p.render(tiny)
    check("N", "denormal-level input stays finite and quiet", bool(np.all(np.isfinite(y))),
          f"out peak {peak_db(y):.0f} dBFS")

GROUPS = dict(A=group_A, B=group_B, C=group_C, D=group_D, E=group_E,
              F=group_F, G=group_G, H=group_H, I=group_I, J=group_J,
              K=group_K, L=group_L, M=group_M, N=group_N, Z=group_Z)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--plugin", default=DEFAULT_PLUGIN)
    ap.add_argument("--group", default="")
    args = ap.parse_args()

    wanted = [g.strip().upper() for g in args.group.split(",") if g.strip()] or list(GROUPS)

    print("MasterLimiter CALIBRATION SUITE — deterministic signals, PASS/FAIL, no listening")
    print(f"plugin: {args.plugin}")

    for g in wanted:
        if g not in GROUPS:
            print(f"  unknown group {g}")
            continue
        try:
            GROUPS[g](args.plugin)
        except Exception as e:
            check(g, f"group {g} crashed", False, f"{type(e).__name__}: {e}")

    npass = sum(1 for r in RESULTS if r[2] == "PASS")
    nfail = sum(1 for r in RESULTS if r[2] == "FAIL")
    print(f"\n{'='*100}\nSUMMARY: {npass} pass, {nfail} fail, {len(RESULTS)} total")
    if nfail:
        print("\nFAILING — these are the things NOT safe to trust in a listening test:")
        for grp, name, st, detail in RESULTS:
            if st == "FAIL":
                print(f"  [{grp}] {name}  --  {detail}")
    sys.exit(1 if nfail else 0)
