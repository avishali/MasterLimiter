#!/usr/bin/env python3
"""
Limiter render analyzer for the MasterLimiter voicing work.

Measures, per WAV (silence auto-trimmed):
  - Integrated LUFS (ITU-R BS.1770 via pyloudnorm)
  - Sample peak (dBFS) and True peak (dBTP, 4x oversampled)
  - Crest factor
  - For two-tone IMD test signals (e.g. 60Hz + 7kHz): intermod + harmonic
    distortion expressed as dB below the carrier (lower = cleaner)

Usage:
  analyze.py <file_or_folder> [more files...]
"""
import sys, os, glob
import numpy as np
import soundfile as sf
import pyloudnorm as pyln
from scipy.signal import resample_poly, get_window

sr_min_len = 2048


def list_wavs(args):
    out = []
    for a in args:
        if os.path.isdir(a):
            out += sorted(glob.glob(os.path.join(a, "*.wav")) + glob.glob(os.path.join(a, "*.WAV")))
        else:
            out.append(a)
    return out


def true_peak_db(x, sr):
    up = resample_poly(x, 4, 1, axis=0)
    tp = np.max(np.abs(up)) + 1e-12
    return 20 * np.log10(tp)


def trim_active(x, peak_frac=0.01, inner=0.10):
    mono = x if x.ndim == 1 else x.mean(axis=1)
    pk = np.max(np.abs(mono))
    if pk <= 0:
        return x, 0, len(mono)
    idx = np.where(np.abs(mono) > pk * peak_frac)[0]
    if len(idx) == 0:
        return x, 0, len(mono)
    a, b = idx[0], idx[-1] + 1
    margin = int((b - a) * inner)
    a2, b2 = a + margin, b - margin
    if b2 - a2 < sr_min_len:
        a2, b2 = a, b
    return (x[a2:b2] if x.ndim == 1 else x[a2:b2, :]), a2, b2


def find_tones(mono, sr, min_db_below=60.0):
    n = len(mono)
    seg = mono[n // 4: n // 4 + min(n // 2, 1 << 19)]
    seg = seg - np.mean(seg)
    w = get_window("hann", len(seg))
    mag = np.abs(np.fft.rfft(seg * w))
    freqs = np.fft.rfftfreq(len(seg), 1.0 / sr)
    peak = mag.max() + 1e-20
    thr = peak * 10 ** (-min_db_below / 20)
    tones = []
    for i in range(2, len(mag) - 2):
        if mag[i] > thr and mag[i] >= mag[i-1] and mag[i] > mag[i+1] and mag[i] >= mag[i-2] and mag[i] > mag[i+2]:
            tones.append((freqs[i], mag[i]))
    tones.sort(key=lambda t: -t[1])
    return tones, freqs, mag


def level_at(freqs, mag, f, hw=15.0):
    sel = (freqs >= f - hw) & (freqs <= f + hw)
    return float(np.max(mag[sel])) if np.any(sel) else 0.0


def imd_thd_report(mono, sr):
    tones, freqs, mag = find_tones(mono, sr)
    if len(tones) < 2:
        return None
    f1 = tones[0][0]
    f_high = next((f for f, _ in tones if abs(f - f1) > 200), None)
    if f_high is None:
        return None
    f_low, f_high = sorted([f1, f_high])
    car = level_at(freqs, mag, f_high)
    low_lvl = level_at(freqs, mag, f_low)
    if car <= 0:
        return None
    imd_pow = 0.0
    sidebands = []
    for k in range(1, 8):
        for sign in (-1, +1):
            fb = f_high + sign * k * f_low
            if 20 < fb < sr / 2 - 20 and abs(fb - f_high) > f_low * 0.4:
                lv = level_at(freqs, mag, fb)
                imd_pow += lv ** 2
                if lv > car * 1e-4:
                    sidebands.append((fb, 20 * np.log10(lv / car)))
    imd_db = 10 * np.log10(imd_pow / car ** 2 + 1e-30)
    thd_pow = sum(level_at(freqs, mag, f_low * h) ** 2 for h in range(2, 12) if f_low * h < sr / 2 - 20)
    thd_low_db = 10 * np.log10(thd_pow / (low_lvl ** 2 + 1e-30) + 1e-30) if low_lvl > 0 else None
    sidebands.sort(key=lambda s: -s[1])
    return dict(f_low=f_low, f_high=f_high, imd_db=imd_db, thd_low_db=thd_low_db, top_sidebands=sidebands[:6])


def analyze(path):
    x_full, sr = sf.read(path, always_2d=True)
    x_full = x_full.astype(np.float64)
    x, a, b = trim_active(x_full)
    active_pct = 100.0 * len(x) / max(1, len(x_full))
    mono = x.mean(axis=1)
    samp_peak = 20 * np.log10(np.max(np.abs(x)) + 1e-12)
    tp = true_peak_db(x, sr)
    rms = 20 * np.log10(np.sqrt(np.mean(mono ** 2)) + 1e-12)
    crest = samp_peak - rms
    try:
        lufs = pyln.Meter(sr).integrated_loudness(x if x.shape[1] > 1 else mono)
    except Exception:
        lufs = float("nan")
    return dict(name=os.path.basename(path), sr=sr, lufs=lufs, samp_peak=samp_peak,
                tp=tp, rms=rms, crest=crest, imd=imd_thd_report(mono, sr), active_pct=active_pct)


def main():
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    files = list_wavs(sys.argv[1:])
    results = []
    for f in files:
        try:
            results.append(analyze(f))
        except Exception as e:
            print(f"!! {os.path.basename(f)}: {e}")
    print()
    print(f"{'file':<42}{'LUFS':>8}{'sPk':>8}{'TP':>8}{'crest':>7}{'IMD↓car':>9}{'THDlo↓':>8}{'act%':>6}")
    print("-" * 98)
    for r in results:
        imd = r["imd"]
        imd_s = f"{imd['imd_db']:>8.1f}" if imd else "      --"
        thd_s = f"{imd['thd_low_db']:>7.1f}" if imd and imd['thd_low_db'] is not None else "     --"
        print(f"{r['name'][:41]:<42}{r['lufs']:>8.1f}{r['samp_peak']:>8.1f}{r['tp']:>8.1f}"
              f"{r['crest']:>7.1f}{imd_s}{thd_s}{r['active_pct']:>6.0f}")
    print("-" * 98)
    print("IMD↓car = total intermod sidebands below the HF carrier (dB, lower=cleaner)")
    print("THDlo↓  = harmonic distortion of the low tone (dB below it, lower=cleaner)")
    for r in results:
        if r["imd"]:
            imd = r["imd"]
            print(f"\n{r['name']}  ({imd['f_low']:.0f}Hz + {imd['f_high']:.0f}Hz)")
            for fb, db in imd["top_sidebands"]:
                print(f"    sideband {fb:7.0f} Hz : {db:6.1f} dB vs carrier")


if __name__ == "__main__":
    main()
