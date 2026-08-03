#!/usr/bin/env python3
"""FIDELITY TO SOURCE — the axis `mbl_pump.py` does NOT measure.

avishali, 2026-08-03, listening: *"overall i notice distortion that is more than normal coloring and
different sound from the original track. compared to Pro-L2 - this is more punchy and more matching to
the source sound."*

Our objective (|MACRO|+|PUMP|+|ROUGH|) scores **envelope movement** — how the level moves over time. It
says nothing about timbre or waveform fidelity. So "Open+Smart leads the frontier at 3.956" has always
meant "preserves macro-dynamics best", never "sounds closest to the source". Those are different axes and
this module measures the second one.

METRIC: residual after removing the best-fit gain AND the best time alignment.
    lower = the output is closer to a clean scaled copy of the input.

⚠️ BOTH corrections are mandatory:
  - **gain**: otherwise it measures loudness difference, not distortion.
  - **time**: our plugin delays ~3003 samples, Pro-L 2 almost none. Without alignment this metric
    returned -0.1 dB for us and -14 for Pro-L 2 -- a pure lag artefact that reads as "catastrophically
    unfaithful". Cross-correlate first.

NULL TEST (run it before trusting any headline from this metric):
    bypass                 -142.4 dB
    limiter off            -142.4 dB
    limiter on, no GR       -92.5 dB
Validated 2026-08-03.

MEASURED (live-show, matched ~3 dB RMS-GR, ceiling -1):
    OPEN (2-band)          -3.4      <- the Open engine's own split is the cost
    TRANSPARENT (inline)  -11.7
    Pro-L 2 Allround      -13.9
Ceiling Clip vs a 20 ms limiter differ by only 0.1-0.4 dB, so the ceiling mode is NOT the cause.

Claude's role (orchestration + measurement); the DSP is the C++ plugin.
"""
import numpy as np


def fidelity_db(x, y, sr, align_secs=10.0):
    """Residual vs source after removing best-fit gain and integer-sample lag. Lower = more faithful."""
    a = x.mean(1) if x.ndim > 1 else x
    b = y.mean(1) if y.ndim > 1 else y
    n = min(len(a), len(b))
    a, b = a[:n], b[:n]

    seg = int(min(n, align_secs * sr))
    corr = np.correlate(b[:seg], a[:seg], mode="full")
    lag = int(np.argmax(np.abs(corr))) - (seg - 1)

    if lag > 0:
        bb, aa = b[lag:], a[:n - lag]
    elif lag < 0:
        bb, aa = b[:n + lag], a[-lag:]
    else:
        bb, aa = b, a
    m = min(len(aa), len(bb))
    aa, bb = aa[:m], bb[:m]

    g = float(np.dot(aa, bb) / max(np.dot(aa, aa), 1e-20))
    r = bb - g * aa
    return 20 * np.log10(np.sqrt(np.mean(r ** 2)) / (np.sqrt(np.mean(bb ** 2)) + 1e-20) + 1e-20), lag
