# SLICE_3BAND_DSP — Close record

**Closed:** 2026-07-02 · **Slice:** `PROMPTS/SLICE_3BAND_DSP.md` · **ADR:** `melechdsp-hq/docs/DECISIONS/ADR-0012-masterlimiter-3band.md`

## Shipped
- 3-band linear-phase crossover tree (Lo/Mid + Mid/Hi) with low-band M2 alignment delay
- `envelopeMid_` + N-band Band Link blend (`min(gLow,gMid,gHigh)`)
- MID GR taps + history trace populated
- DEV: hi-split params, mid release scale, Band Link relocated from shipping Color
- GR meter: per-band SOLO listen buttons (transient atomics)
- Shipping Color knob disabled; `band_color` frozen ID unchanged

## Verify (automated)
- [x] Release build clean
- [x] `auval -v aufx MaLm Melc` PASS

## Verify (pending)
- [ ] Offline transparency null (Band Link 0)
- [ ] Per-band independence (Band Link 100)
- [ ] TP at ceiling across Band Link sweep
- [ ] Live crossover DEV sweep (duck-swap, no swap-race crash)
- [ ] avishali audition vs 2-band build

## Notes
- SDK untouched (reuse existing `LinearPhaseCrossover`, `LookaheadDelay`, `LimiterEnvelope`)
- Next slice candidate: purpose-built multiband user control (crossover handles + link macro)
