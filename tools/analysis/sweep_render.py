#!/usr/bin/env python3
"""Render a sine sweep through MasterLimiter VST3 offline (pedalboard) and
produce spectrograms + an aliasing metric. Latency-independent: we analyze the
output spectrogram, so it answers 'real audio aliasing vs Plugin Doctor measurement ghost'.
"""
import sys, numpy as np, soundfile as sf
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from pedalboard import load_plugin

VST3 = "/Users/avishaylidani/Library/Audio/Plug-Ins/Components/MasterLimiter.component"
SR = 48000
DUR = 6.0
F0, F1 = 20.0, 24000.0
OUTDIR = "/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/tools/analysis/out"

import os
os.makedirs(OUTDIR, exist_ok=True)

def linear_sweep(sr, dur, f0, f1, peak):
    t = np.arange(int(sr*dur))/sr
    # linear-in-frequency sweep (matches Plugin Doctor 'Lin')
    inst = f0 + (f1-f0)*t/dur
    phase = 2*np.pi*np.cumsum(inst)/sr
    x = np.sin(phase).astype(np.float32) * peak
    return np.stack([x, x], axis=1)  # stereo

def spectrogram_png(sig, sr, path, title):
    mono = sig.mean(axis=1) if sig.ndim > 1 else sig
    plt.figure(figsize=(9, 5))
    plt.specgram(mono, NFFT=4096, Fs=sr, noverlap=3072, cmap="turbo",
                 vmin=-200, vmax=0, scale="dB", mode="magnitude")
    plt.ylim(0, sr/2); plt.xlabel("time (s)"); plt.ylabel("freq (Hz)")
    plt.title(title); plt.colorbar(label="dB"); plt.tight_layout()
    plt.savefig(path, dpi=110); plt.close()

def alias_floor_db(sig, sr):
    """Track the swept fundamental; measure energy OUTSIDE a band around it = distortion/alias floor."""
    mono = sig.mean(axis=1) if sig.ndim > 1 else sig
    nfft, hop = 4096, 1024
    win = np.hanning(nfft)
    worst = -200.0
    frames = []
    for start in range(0, len(mono)-nfft, hop):
        seg = mono[start:start+nfft]*win
        mag = np.abs(np.fft.rfft(seg))
        freqs = np.fft.rfftfreq(nfft, 1/sr)
        if mag.max() < 1e-9:
            continue
        f_fund = freqs[np.argmax(mag)]
        fund_db = 20*np.log10(mag.max()+1e-12)
        # mask out fundamental +/- 200 Hz, and DC/very-low
        mask = (np.abs(freqs - f_fund) > 200) & (freqs > 40)
        if not mask.any():
            continue
        other_db = 20*np.log10(mag[mask].max()+1e-12)
        rel = other_db - fund_db
        frames.append((f_fund, rel))
        worst = max(worst, rel)
    return worst, frames

def render(peak, state_note, tag):
    x = linear_sweep(SR, DUR, F0, F1, peak)
    plug = load_plugin(VST3)
    # default plugin state as loaded
    y = plug(x, SR)
    in_path = f"{OUTDIR}/sweep_in_{tag}.wav"
    out_path = f"{OUTDIR}/sweep_out_{tag}.wav"
    sf.write(in_path, x, SR); sf.write(y, SR, out_path) if False else sf.write(out_path, y, SR)
    spectrogram_png(x, SR, f"{OUTDIR}/spec_in_{tag}.png", f"INPUT sweep (peak {peak:.2f})")
    spectrogram_png(y, SR, f"{OUTDIR}/spec_out_{tag}.png", f"ML OUTPUT — {state_note} (peak in {peak:.2f})")
    in_floor, _ = alias_floor_db(x, SR)
    out_floor, _ = alias_floor_db(y, SR)
    print(f"[{tag}] {state_note}")
    print(f"    input  worst off-fundamental: {in_floor:6.1f} dB (clean source reference)")
    print(f"    OUTPUT worst off-fundamental: {out_floor:6.1f} dB")
    print(f"    -> spec_out_{tag}.png")
    return out_floor

if __name__ == "__main__":
    print(f"pedalboard render through {VST3}\n")
    # 1) low level: limiter barely engages -> tests pure passthrough cleanliness
    render(0.12, "default state, LOW level (passthrough)", "low")
    # 2) hot level: drives clipper/limiter -> tests real limiter aliasing
    render(0.95, "default state, HOT level (limiter+clipper engaged)", "hot")
    print("\nDone. Compare spec_out_low (should be clean if passthrough is clean)")
    print("vs spec_out_hot (shows any real limiter/clipper aliasing).")
