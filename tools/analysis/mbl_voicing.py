#!/usr/bin/env python3
"""VOICING SWEEP — find the Open engine's best settings against a metric that means something.

Why: every frontier number we ever measured used `dev_mb_release_ms = 150`, which is a leftover from
the MB-2 slice, not a chosen value. On a real 2.6-minute mix at ~11 dB push, release turned out to be a
large lever on macro-dynamic preservation:

    release  60 ms -> MACRO -0.69       150 ms -> -1.54       300 ms -> -2.58

So our default costs ~0.85 dB against 60 ms, and Open at 60 ms sits close to Pro-L 2 / Ozone. Before
building any new engine, find out how much of the "frontier gap" is just an untuned default.

METRIC: |MACRO| -- the ABSOLUTE 0.1-0.5 Hz added envelope modulation (see mbl_pump.py). Zero means the
limiter passed the source's section-level dynamics through untouched. Negative = it flattened them;
POSITIVE = it invented slow swings that were never in the source (long releases do this as gain crawls
back over seconds). Both directions are errors, so the objective is distance from zero -- ranking on the
signed value once crowned 300 ms purely because one source scored +1.53. `st_range` is reported
alongside but is NOT the objective: it cannot tell breathing from pumping.

METHOD: coordinate descent, not a full grid (a full grid is ~2.4 h per source).
    stage 1  release      (attack/crossover at default)
    stage 2  attack       (at stage 1's winner)
    stage 3  crossover    (at stage 1+2's winner)
Every config is matched to the same ACTUAL gain reduction (not the same output RMS -- on a high-headroom
source an RMS "push" can be reached with ~0 dB of limiting, which made one source return identical rows
at every setting). Every config must also hold sPk <= -1.00: a setting that wins by letting peaks through
is not a win, which is exactly how the old 6.39 EDM figure happened.

Usage:
    ./.venv/bin/python mbl_voicing.py --stage 1
    ./.venv/bin/python mbl_voicing.py --stage 2 --release 60
"""
import argparse
import numpy as np
import soundfile as sf

from mbl_pump import (added_modulation, st_range_win, render_ours, rms_of,
                      sample_peak_db, BANDS)

TF = "/Users/avishaylidani/Music/Test Files "
CORPUS = [   # spread of macro-dynamics (dry rng3s in comment), all 44.1 kHz
    ("live-show", "/Users/avishaylidani/Dropbox (Avishay Lidani Sound)/Recordings/IDIOT REC/"
                  "idiot barby 7.7 Project/Samples/Processed/Bounce/"
                  "Bounce 4-LR_Main1_IDIOT BARBI SHEL MELECH_070726_2058 [2026-08-02 043857]-1.wav"),
    ("ishay-ribo", TF + "/Bounce 4-LR_Main1_ISHAY RIBO_02_220226_0544 [2026-08-02 050258]-1.wav"),
    ("easy-master", TF + "/Easy - Mix (44_24) FOR MASTERING.wav"),
    ("homework-dense", TF + "/Homework MIX nom.wav"),
]

GR_TARGET = 8.0         # dB of ACTUAL gain reduction -- see match() for why this replaced an RMS push
MAX_SECS = 120.0        # plenty of 3 s windows for the macro statistic, half the render cost


def load(path):
    x, sr = sf.read(path, always_2d=True)
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    if len(x) > MAX_SECS * sr:
        s = (len(x) - int(MAX_SECS * sr)) // 2      # centre excerpt: skip intro/outro
        x = x[s:s + int(MAX_SECS * sr)]
    return np.ascontiguousarray(x.astype(np.float32)), sr


def gr_of(x, y, drive_db):
    """Actual gain reduction: gain we asked for, minus level we actually got."""
    return drive_db - (rms_of(y) - rms_of(x))


def match(x, sr, target_gr, **kw):
    """Search input gain so the limiter does `target_gr` dB of ACTUAL gain reduction.

    Matching on output RMS instead was a methodology flaw: on a source with a lot of headroom
    (easy-master, peak -9.9 dBFS) an 11 dB "push" was achieved almost entirely by clean gain with
    ~0 dB of limiting, so every release setting produced byte-identical results and the source
    contributed nothing but a flat row. Matching on GR guarantees the limiter is actually working
    equally hard on every source and every setting -- which is the thing being compared.
    """
    best = None
    for g in np.arange(2.0, 24.01, 1.5):
        y = render_ours(x, sr, float(g), True, **kw)
        e = abs(gr_of(x, y, g) - target_gr)
        if best is None or e < best[2]:
            best = (float(g), y, e)
    for g in np.arange(max(0.0, best[0] - 1.25), min(24.0, best[0] + 1.26), 0.5):
        y = render_ours(x, sr, float(g), True, **kw)
        e = abs(gr_of(x, y, g) - target_gr)
        if e < best[2]:
            best = (float(g), y, e)
    return best[0], best[1]


def evaluate(label, variants, kwargs_for):
    """variants: list of values; kwargs_for(v) -> render_ours kwargs. Prints per-source and mean."""
    per_source = {}
    for name, path in CORPUS:
        x, sr = load(path)
        print(f"\n  {name}  ({len(x)/sr:.0f}s, dry RMS {rms_of(x):.1f}, target {GR_TARGET:.0f} dB GR)")
        print(f"    {label:>10} {'drive':>6} {'GR':>6} {'sPk':>6} {'MACRO':>7} {'PUMP':>7} {'rng3s':>7}")
        for v in variants:
            d, y = match(x, sr, GR_TARGET, **kwargs_for(v))
            am = added_modulation(x, y, sr)
            spk, gr = sample_peak_db(y), gr_of(x, y, d)
            bad = spk > -0.99 or gr < GR_TARGET - 2.0
            note = "" if not bad else ("  <-- PEAK MISS" if spk > -0.99 else "  <-- GR NOT REACHED")
            per_source.setdefault(v, []).append(np.nan if bad else am["MACRO 0.1-0.5"])
            print(f"    {str(v):>10} {d:+6.1f} {gr:6.2f} {spk:6.2f} {am['MACRO 0.1-0.5']:+7.2f} "
                  f"{am['PUMP 2-8']:+7.2f} {st_range_win(y,3.0,sr):7.2f}{note}")
    # Objective is |MACRO| -- distance from transparency in EITHER direction. A positive MACRO means
    # the limiter ADDED slow level movement the source never had (long releases do this: gain crawls
    # back over seconds and invents 0.1-0.5 Hz swings). That is not "better than" removing movement,
    # it is a different way of being wrong. Ranking on the signed mean made 300 ms look like the
    # winner purely because one source scored +1.53.
    print(f"\n  ===== MEAN |MACRO| across corpus (LOWER = closer to transparent) =====")
    ranked = sorted(per_source.items(), key=lambda kv: np.nanmean(np.abs(kv[1])))
    for v, vals in ranked:
        a = np.nanmean(np.abs(vals))
        spread = np.nanmax(np.abs(vals)) - np.nanmin(np.abs(vals))
        print(f"    {str(v):>10}  {a:6.3f}  {'#' * int(round(a * 20)):30s} "
              f"per-source spread {spread:5.2f}"
              + ("   <-- source-dependent, treat mean with caution" if spread > 1.0 else ""))
    best = ranked[0]
    print(f"\n  BEST MEAN: {best[0]}  (|MACRO| {np.nanmean(np.abs(best[1])):.3f})")
    if np.nanmax(np.abs(best[1])) - np.nanmin(np.abs(best[1])) > 1.0:
        print("  ** The per-source spread exceeds the between-setting differences: this corpus does NOT")
        print("     support a single global winner. Report it as source-dependent, not as a default. **")
    return best[0]


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--stage", type=int, default=1)
    ap.add_argument("--release", type=float, default=150.0)
    ap.add_argument("--attack", type=float, default=5.0)
    args = ap.parse_args()

    print("VOICING SWEEP — objective = |MACRO| (0.1-0.5 Hz distance from transparent), "
          "peak-gated at sPk <= -1.00")
    print(f"matched to {GR_TARGET:.0f} dB of ACTUAL gain reduction; corpus of {len(CORPUS)}; "
          f"centre {MAX_SECS:.0f}s excerpts")

    if args.stage == 1:
        print("\nSTAGE 1 — RELEASE (attack/crossover at default)")
        evaluate("release", [30.0, 60.0, 100.0, 150.0, 220.0, 300.0],
                 lambda v: dict(release_ms=v))
    elif args.stage == 2:
        print(f"\nSTAGE 2 — ATTACK (release fixed at {args.release:.0f} ms)")
        evaluate("attack", [0.5, 1.0, 2.0, 5.0, 12.0, 25.0],
                 lambda v: dict(release_ms=args.release, attack_ms=v))
    elif args.stage == 3:
        print(f"\nSTAGE 3 — CROSSOVER (release {args.release:.0f} ms, attack {args.attack:.1f} ms)")
        evaluate("xover Hz", [60.0, 90.0, 120.0, 180.0, 250.0, 400.0],
                 lambda v: dict(release_ms=args.release, attack_ms=args.attack, crossover_hz=v))
