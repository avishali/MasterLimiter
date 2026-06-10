#!/usr/bin/env python3
"""Isolate the source of the swept-sine artifact by rendering through ML in
several parameter states (offline, latency-independent). Reports the worst
non-fundamental component (Blackman-Harris window, ~-92 dB sidelobes)."""
import os, numpy as np, soundfile as sf
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from pedalboard import load_plugin

VST3 = "/Library/Audio/Plug-Ins/VST3/MasterLimiter.vst3"
SR, DUR, F0, F1 = 48000, 6.0, 20.0, 24000.0
OUT = os.path.dirname(__file__) + "/out"; os.makedirs(OUT, exist_ok=True)

def sweep(peak):
    t = np.arange(int(SR*DUR))/SR
    inst = F0 + (F1-F0)*t/DUR
    x = (np.sin(2*np.pi*np.cumsum(inst)/SR)*peak).astype(np.float32)
    return np.stack([x, x], 1)

def alias_metric(sig):
    """Worst non-fundamental peak (dB rel fundamental) and the freq where it's worst."""
    mono = sig.mean(1) if sig.ndim>1 else sig
    nfft, hop = 8192, 2048
    win = np.blackman(nfft)  # ~-58 dB; use blackmanharris-like via product if needed
    # 4-term Blackman-Harris for low sidelobes:
    n = np.arange(nfft)
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    win = a[0]-a[1]*np.cos(2*np.pi*n/nfft)+a[2]*np.cos(4*np.pi*n/nfft)-a[3]*np.cos(6*np.pi*n/nfft)
    freqs = np.fft.rfftfreq(nfft, 1/SR)
    worst, worst_f, worst_ff = -200.0, 0.0, 0.0
    for s in range(0, len(mono)-nfft, hop):
        mag = np.abs(np.fft.rfft(mono[s:s+nfft]*win))
        if mag.max() < 1e-9: continue
        fi = np.argmax(mag); ff = freqs[fi]; fund_db = 20*np.log10(mag[fi]+1e-12)
        if ff < 200 or ff > 20000: continue          # only judge mid sweep
        mask = (np.abs(freqs-ff) > 1500) & (freqs > 60) & (freqs < SR/2-1000)
        if not mask.any(): continue
        oi = np.argmax(mag[mask]); o_db = 20*np.log10(mag[mask][oi]+1e-12)
        rel = o_db - fund_db
        if rel > worst:
            worst, worst_f, worst_ff = rel, freqs[mask][oi], ff
    return worst, worst_f, worst_ff

def specpng(sig, path, title):
    mono = sig.mean(1) if sig.ndim>1 else sig
    plt.figure(figsize=(9,5))
    plt.specgram(mono, NFFT=4096, Fs=SR, noverlap=3072, cmap="turbo", vmin=-200, vmax=0,
                 scale="dB", mode="magnitude")
    plt.ylim(0, SR/2); plt.title(title); plt.xlabel("s"); plt.ylabel("Hz")
    plt.colorbar(label="dB"); plt.tight_layout(); plt.savefig(path, dpi=110); plt.close()

def run(tag, peak, params):
    p = load_plugin(VST3)
    for k,v in params.items(): setattr(p, k, v)
    y = p(sweep(peak), SR)
    sf.write(f"{OUT}/iso_{tag}.wav", y, SR)
    specpng(y, f"{OUT}/iso_{tag}.png", f"{tag}  (peak {peak})")
    w, wf, ff = alias_metric(y)
    print(f"  {tag:<26} worst non-fund: {w:6.1f} dB  (at {wf:6.0f} Hz when fund={ff:5.0f} Hz)  ratio f_alias/f_fund={wf/max(ff,1):.2f}")
    return w

print("ref: clean source sweep")
ws,_,_ = alias_metric(sweep(0.5)); print(f"  {'SOURCE (no plugin)':<26} worst non-fund: {ws:6.1f} dB\n")
print("LOW level 0.12 (nothing should engage):")
run("low_default",      0.12, {})
run("low_bypass",       0.12, {"bypass": True})
run("low_limiter_off",  0.12, {"limiter_active": False})
run("low_clipper_off",  0.12, {"clipper_active": False})
print("\nHOT (input_gain +12 dB -> limiter actively limiting):")
run("hot_default",      0.5,  {"input_gain_db": 12.0})
run("hot_clipper_off",  0.5,  {"input_gain_db": 12.0, "clipper_active": False})
run("hot_la_engine",    0.5,  {"input_gain_db": 12.0, "dev_release_engine": "Lookahead"})
print("\nPNGs + WAVs in tools/analysis/out/  (iso_*.png)")
