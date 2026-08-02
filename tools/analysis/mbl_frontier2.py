#!/usr/bin/env python3
"""FRONTIER, on the metric that means something.

Every frontier comparison we hold used `st_range`, which cannot tell breathing from pumping and can be
gamed by letting peaks through. This asks the same question on the objective from
`docs/PROGRAM_DEPENDENT_ENGINE.md` section 7.3:

    score = |MACRO 0.1-0.5| + |PUMP 2-8| + |ROUGH 8-20|      (deviation from the source, lower = better)
    hard gate: sample peak <= -1.00

...across the profiled 4-source corpus, matched by ACTUAL gain reduction, now that the Open engine can
run ReleaseEngine::Smart (SMART-1).
"""
import numpy as np
import mbl_pump as P
from mbl_voicing import CORPUS, load, gr_of, GR_TARGET

def ours(x, sr, gain, engine, mb=True):
    p = P._plug(P.OURS)
    p.dev_mb_engine = mb
    if mb:
        p.dev_mb_crossover_hz = 120.0; p.dev_mb_attack_mode = "Ramp"
        p.dev_mb_release_ms = 30.0; p.dev_mb_safety = False
        p.dev_mb_release_engine = engine
    # Set the four Smart knobs EXPLICITLY rather than leaning on defaults. They are currently the
    # SMART-1.1 tuned values, but a run that depends on an unstated default is not reproducible --
    # and silent defaults have burned this project repeatedly today.
    p.dev_smart_fast_ms = 40.0; p.dev_smart_slow_ms = 300.0
    p.dev_smart_sustain_ms = 450.0; p.dev_smart_leak = 0.15
    p.limiter_active = True; p.drive_active = False
    p.ceiling_active = True; p.ceiling_release_ms = "Clip"
    p.ceiling_mode = "SamplePeak"; p.ceiling_db = -1.0; p.auto_track = False
    p.input_gain_db = float(gain)
    return p(x, sr)

CONFIGS = [
    ("OPEN + Manual",        lambda x, sr, g: ours(x, sr, g, "Manual")),
    ("OPEN + Smart",         lambda x, sr, g: ours(x, sr, g, "Smart")),
    ("TRANSPARENT",          lambda x, sr, g: ours(x, sr, g, "Manual", mb=False)),
    ("Pro-L 2 Transparent",  lambda x, sr, g: P.render_prol(x, sr, g, "Transparent")),
    ("Pro-L 2 Allround",     lambda x, sr, g: P.render_prol(x, sr, g, "Allround")),
    ("Pro-L 2 Aggressive",   lambda x, sr, g: P.render_prol(x, sr, g, "Aggressive")),
    ("Ozone11 IRC (m2)",     lambda x, sr, g: P.render_oz(x, sr, g, 2)),
]

def score(x, y, sr):
    am = P.added_modulation(x, y, sr)
    return sum(abs(am[k]) for k in ("MACRO 0.1-0.5", "PUMP 2-8", "ROUGH 8-20"))

if __name__ == "__main__":
    print("FRONTIER on |MACRO|+|PUMP|+|ROUGH| (lower = closer to the source), sPk gate -1.00")
    print(f"corpus of {len(CORPUS)}, matched to ~{GR_TARGET:.0f} dB RMS-GR\n", flush=True)
    tot = {c[0]: [] for c in CONFIGS}
    for name, path in CORPUS:
        x, sr = load(path)
        print(f"  {name}", flush=True)
        for label, fn in CONFIGS:
            b = None
            for g in np.arange(6.0, 24.01, 1.5):
                y = fn(x, sr, float(g)); e = abs(gr_of(x, y, g) - GR_TARGET)
                if b is None or e < b[2]: b = (g, y, e)
            g, y, _ = b
            s, spk = score(x, y, sr), P.sample_peak_db(y)
            ok = spk <= -0.985
            tot[label].append(s if ok else np.nan)
            print(f"    {label:22s} drive {g:+5.1f}  sPk {spk:6.2f}  score {s:6.2f}"
                  + ("" if ok else "  <-- PEAK MISS"), flush=True)
    print("\n  ===== MEAN across corpus (lower = better) =====", flush=True)
    for label, v in sorted(tot.items(), key=lambda kv: np.nanmean(kv[1])):
        print(f"    {label:22s} {np.nanmean(v):6.3f}", flush=True)
