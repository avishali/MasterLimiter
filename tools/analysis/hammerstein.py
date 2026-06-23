#!/usr/bin/env python3
"""Offline Hammerstein-style harmonic + linear phase/magnitude analysis of ML.
- Harmonic check: render a sweep while limiting, track H1..H5 vs frequency (THD).
- Linear check: render a quiet sweep, matched-filter to the impulse response,
  report magnitude (HF rolloff / high-cut) and excess phase (the ~90 deg HF tilt),
  latency-removed. All on rendered files -> latency-independent, reproducible.
"""
import os, numpy as np, soundfile as sf
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from pedalboard import load_plugin
from scipy.signal import fftconvolve

VST3 = "/Users/avishaylidani/Library/Audio/Plug-Ins/Components/MasterLimiter.component"
SR, DUR = 48000, 4.0
OUT = os.path.dirname(__file__) + "/out"; os.makedirs(OUT, exist_ok=True)
bh = lambda N: (0.35875 - 0.48829*np.cos(2*np.pi*np.arange(N)/N)
                + 0.14128*np.cos(4*np.pi*np.arange(N)/N) - 0.01168*np.cos(6*np.pi*np.arange(N)/N))

def exp_sweep(f0, f1, peak):
    t = np.arange(int(SR*DUR))/SR
    L = DUR/np.log(f1/f0)
    x = np.sin(2*np.pi*f0*L*(np.exp(t/L)-1.0)).astype(np.float32)
    # short fades
    nf = int(0.01*SR); x[:nf] *= np.linspace(0,1,nf); x[-nf:] *= np.linspace(1,0,nf)
    return x, L

def render(x, params, peak):
    p = load_plugin(VST3)
    for k,v in params.items():
        # pedalboard uses sanitized DISPLAY names (input_gain, color, ceiling) — a wrong
        # name silently sets a dead attribute, so guard loudly instead of measuring nothing.
        assert k in p.parameters, f"unknown param '{k}'; available: {sorted(p.parameters)}"
        setattr(p, k, v)
    y = p(np.stack([x*peak, x*peak],1), SR)
    return y.mean(1)

# ---- 1) Harmonic tracking (limiter engaged) ----
def harmonics(params, peak, tag):
    f0, f1 = 20.0, 20000.0
    x,_ = exp_sweep(f0, f1, 1.0)
    y = render(x, params, peak)
    nfft, hop = 8192, 1024
    w = bh(nfft); freqs = np.fft.rfftfreq(nfft, 1/SR)
    curves = {k: [] for k in range(1,6)}; fund_f = []
    def mag_at(spec, f):
        if f >= SR/2: return 1e-9
        b = f/(SR/nfft); lo=int(np.floor(b));
        lo=max(0,min(lo,len(spec)-2)); frac=b-lo
        return abs(spec[lo])*(1-frac)+abs(spec[lo+1])*frac
    for s in range(0, len(y)-nfft, hop):
        spec = np.fft.rfft(y[s:s+nfft]*w)
        fi = np.argmax(np.abs(spec)); f = freqs[fi]
        if f < 60 or f > f1*0.9: continue
        h1 = abs(spec[fi]);
        if h1 < 1e-7: continue
        fund_f.append(f)
        for k in range(1,6):
            # search +-2 bins around k*f for the local peak
            kf = k*f; best=1e-9
            for db in range(-3,4):
                best=max(best, mag_at(spec, kf + db*SR/nfft))
            curves[k].append(20*np.log10(best/h1+1e-12))
    fund_f = np.array(fund_f)
    # THD vs freq
    H = {k: np.array(v) for k,v in curves.items()}
    thd = 10*np.log10(np.sum([10**(H[k]/10) for k in range(2,6)], axis=0)+1e-12)
    plt.figure(figsize=(9,5))
    for k,c in [(2,'r'),(3,'g'),(4,'m'),(5,'orange')]:
        plt.semilogx(fund_f, H[k], c, lw=1, label=f"H{k}")
    plt.semilogx(fund_f, thd, 'k', lw=1.5, label="THD")
    plt.ylim(-130,-20); plt.grid(True, which='both', alpha=.3)
    plt.xlabel("fundamental Hz"); plt.ylabel("dB rel H1"); plt.legend(); plt.title(f"Harmonics — {tag}")
    plt.tight_layout(); plt.savefig(f"{OUT}/harm_{tag}.png", dpi=110); plt.close()
    print(f"[harm {tag}] peak THD {thd.max():.1f} dB | worst H2 {H[2].max():.1f} | H3 {H[3].max():.1f} | -> harm_{tag}.png")

# ---- 2) Linear magnitude + excess phase (quiet, ~linear) ----
def linear(params, peak, tag):
    f0, f1 = 10.0, 23500.0
    x, L = exp_sweep(f0, f1, 1.0)
    y = render(x, params, peak)
    ir = fftconvolve(y, x[::-1], mode="full")          # matched filter
    pk = np.argmax(np.abs(ir))
    W = 8192
    seg = ir[pk-W//2:pk+W//2] * bh(W)
    H = np.fft.rfft(seg); freqs = np.fft.rfftfreq(W, 1/SR)
    mag = 20*np.log10(np.abs(H)/np.max(np.abs(H))+1e-12)
    ph = np.unwrap(np.angle(H))
    band = (freqs>30)&(freqs<22000)
    # remove best-fit linear phase (residual group delay) over the band
    A = np.polyfit(freqs[band], ph[band], 1); ph_excess = ph - np.polyval(A, freqs)
    ph_deg = np.degrees(ph_excess)
    fig, ax = plt.subplots(2,1, figsize=(9,7), sharex=True)
    ax[0].semilogx(freqs[band], mag[band]); ax[0].set_ylabel("dB"); ax[0].set_ylim(-3,1)
    ax[0].grid(True, which='both', alpha=.3); ax[0].set_title(f"Linear response — {tag}")
    ax[0].axhline(-0.1, color='r', ls=':', alpha=.5)
    ax[1].semilogx(freqs[band], ph_deg[band]); ax[1].set_ylabel("excess phase (deg)")
    ax[1].set_xlabel("Hz"); ax[1].grid(True, which='both', alpha=.3)
    plt.tight_layout(); plt.savefig(f"{OUT}/lin_{tag}.png", dpi=110); plt.close()
    # report rolloff point (-0.1 dB) and phase at 20k
    roll = freqs[band][mag[band] < -0.1]
    roll_f = roll[0] if len(roll) else freqs[band][-1]
    i20 = np.argmin(np.abs(freqs-20000))
    print(f"[lin {tag}] -0.1 dB at {roll_f:.0f} Hz | excess phase @20k {ph_deg[i20]:+.1f} deg | -> lin_{tag}.png")

if __name__ == "__main__":
    print("=== Harmonic (limiter engaged, +12 dB drive) ===")
    harmonics({"input_gain": 12.0}, 0.5, "default")
    print("\n=== Linear filter response (quiet, ~linear) ===")
    linear({}, 0.10, "default")
    print("\nplots in tools/analysis/out/ (harm_default.png, lin_default.png)")
