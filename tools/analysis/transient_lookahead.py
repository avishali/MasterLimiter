#!/usr/bin/env python3
"""Probe whether the per-band limiter has working lookahead after the F-2b crossover.

Signal: a quiet steady 1 kHz tone with a single loud broadband spike partway through.
With lookahead, the gain ramps DOWN before the spike, so the quiet tone is attenuated
in the few ms preceding the spike. With broken lookahead (audio delay collapsed), the
tone stays at full level right up to the spike and only ducks after.

We render through the plugin (latency-independent: we locate the spike in the output and
measure the tone envelope just before it), at Color 100 so the per-band path does the work.
"""
import os, numpy as np, soundfile as sf
from pedalboard import load_plugin

COMP = "/Users/avishaylidani/Library/Audio/Plug-Ins/Components/MasterLimiter.component"
SR = 48000
OUT = os.path.dirname(__file__) + "/out"; os.makedirs(OUT, exist_ok=True)

def make_signal():
    n = int(SR * 2.0)
    t = np.arange(n) / SR
    tone = 0.06 * np.sin(2 * np.pi * 1000.0 * t)      # quiet sustained tone
    spike_at = int(SR * 1.0)
    x = tone.copy()
    # a short, very loud burst (a few ms) to force heavy GR
    burst = np.arange(spike_at, spike_at + int(0.002 * SR))
    x[burst] += 0.95 * np.sign(np.sin(2 * np.pi * 1000.0 * t[burst]))
    return x.astype(np.float32), spike_at

def render(x, params):
    p = load_plugin(COMP)
    for k, v in params.items():
        setattr(p, k, v)
    y = p(np.stack([x, x], 1), SR)
    return y.mean(1)

def envelope(sig, win=64):
    return np.sqrt(np.convolve(sig.astype(np.float64) ** 2, np.ones(win) / win, mode="same"))

def main():
    x, _ = make_signal()
    # Isolate the BAND lookahead: kill the wideband stage's lookahead so any pre-duck
    # can only come from the per-band path (the path F-2b's fix touches).
    # NOTE: pedalboard exposes the plugin's *display* param names, sanitized — e.g.
    # input_gain (not input_gain_db), color (not band_color), dev_la_band / dev_la_wide.
    y = render(x, {"input_gain": 18.0, "color": 100.0,
                   "limiter_active": True, "clipper_active": False,
                   "dev_la_wide": 0.0, "dev_la_band": 5.0})
    env = envelope(y)
    # Locate the spike in the OUTPUT (max envelope point) — handles plugin latency.
    sp = int(np.argmax(env))
    # Tone reference level: average envelope well before the spike (steady region).
    ref_lo = max(0, sp - int(0.25 * SR)); ref_hi = sp - int(0.05 * SR)
    ref = np.median(env[ref_lo:ref_hi]) + 1e-12
    # Pre-spike window: 6 ms .. 0.5 ms BEFORE the spike.
    pre_lo = sp - int(0.006 * SR); pre_hi = sp - int(0.0005 * SR)
    pre = np.median(env[pre_lo:pre_hi])
    duck_db = 20 * np.log10(pre / ref)
    print(f"[transient] spike@out={sp}  ref_tone_env={ref:.5f}  pre_spike_env={pre:.5f}")
    print(f"[transient] pre-spike duck = {duck_db:+.2f} dB "
          f"({'LOOKAHEAD PRESENT' if duck_db < -1.0 else 'NO PRE-DUCK -> lookahead broken'})")
    # Envelope shape (dB rel ref) at offsets around the spike — shows WHERE ducking begins.
    print("[shape] ms_rel_spike : dB_rel_ref")
    for ms in [-14, -12, -10, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 4, 8]:
        idx = sp + int(ms * 1e-3 * SR)
        if 0 <= idx < len(env):
            print(f"   {ms:+3d} ms : {20*np.log10(env[idx]/ref):+6.2f}")
    sf.write(f"{OUT}/transient_probe.wav", y, SR)

if __name__ == "__main__":
    main()
