# SLICE SMART-1.1 — retune the four `dev_smart_*` defaults

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Audition:** avishali
**Scope:** plugin only, **four default values**. No DSP change, no SDK change, no new parameter.

## The change (`Source/parameters/Parameters.cpp`)

| param | old | **new** |
|---|---:|---:|
| `dev_smart_fast_ms` | 20.0 | **40.0** |
| `dev_smart_slow_ms` | 300.0 | 300.0 *(unchanged — measured as insensitive)* |
| `dev_smart_sustain_ms` | 120.0 | **450.0** |
| `dev_smart_leak` | 0.30 | **0.15** |

## Why — measured, Open path, 4-source corpus, matched ~3 dB RMS-GR, sPk −1.00 on every row

`|MACRO| + |PUMP| + |ROUGH|`, lower is better:

| source | July defaults | **tuned** | Pro-L 2 Allround |
|---|---:|---:|---:|
| live-show | **4.58** | 4.91 | **3.18** |
| ishay-ribo | 4.00 | **2.91** | 6.46 |
| easy-master | 6.94 | **5.78** | 5.19 |
| homework-dense | 4.37 | **2.22** | 3.19 |
| **mean** | 4.973 | **3.956** | 4.507 |

Tuned improves on 3 of 4 sources (~20% on the mean) and takes the engine **past Pro-L 2 Allround on the
mean for the first time**. ⚠️ It is *worse* on `live-show` (4.58 → 4.91), which is also the source where
Smart-vs-Manual was ambiguous. Live-recorded material is consistently our weakest case — worth an ear
check specifically there before this ships.

⚠️ The sweep that found these values had a state-carry-over bug (a cached plugin instance retained
previously-set params), so it was a sequential search, not four independent sweeps. **The table above is
from a clean re-run** that sets all four explicitly on every render. Do not read per-parameter
attributions from the original sweep — only the combination is verified.

## Preset compatibility
All four are stored params: **existing presets and sessions keep their saved values.** Only fresh
instances get the new defaults. State this in the close note.

## Gate
- [ ] Fresh instance reports fast 40.0 / slow 300.0 / sustain 450.0 / leak 0.15.
- [ ] A preset saved with the old values still loads with the old values.
- [ ] `mbl_calibrate.py` — A–N and Z all PASS; **sPk ≤ −1.00 and latency 3003 unchanged** (this is a
      default change and cannot move either).
- [ ] Build clean, AU + VST3, **both installed**, mtimes for both.

## Non-goals
- Do not change `dev_mb_release_engine`'s default (still `Manual`) — that awaits avishali's ear test.
- No DSP, no SDK, no UI changes.
