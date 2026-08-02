# SLICE SMART-1.2 — make `Smart` the default release engine for the Open engine

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude · **Auditioned: DONE (avishali)**
**Scope:** plugin only, **one default value**. No DSP, no SDK, no new parameter, no UI change.

## The change (`Source/parameters/Parameters.cpp`)
`dev_mb_release_engine` default **`Manual` -> `Smart`**.

That is the whole slice.

## Why — measurement AND listening now agree

**Listening (avishali, 2026-08-02, the deciding evidence):**
> *"overall the limiter is performing good. clean. open-smart wins manual"*

**Measurement** — `|MACRO|+|PUMP|+|ROUGH|`, Open path, 4-source corpus, matched ~3 dB RMS-GR,
sPk −1.00 on every row (lower = better):

| config | mean |
|---|---:|
| **OPEN + Smart (SMART-1.1 defaults)** | **3.956** |
| Pro-L 2 Allround | 4.507 |
| OPEN + Manual *(today's default)* | 6.968 — **last of everything measured, below Ozone** |

Two independent measurements agreed on the ordering (Cursor 4.894 / Claude 5.076 pre-tuning), the
inline path agreed separately (Smart 3.114 / Adaptive 4.282 / Lookahead 5.954), and the ear test
now agrees. **Manual is the worst option we have and it is what ships.**

## Preset compatibility
`dev_mb_release_engine` is a stored parameter: **existing presets and sessions keep `Manual`.** Only
fresh instances get `Smart`. Say so in the close note — tester `.mlpreset` voicings are unchanged.

## Gate
- [ ] Fresh instance reports `dev_mb_release_engine = Smart`.
- [ ] A preset saved with `Manual` still loads as `Manual`.
- [ ] `mbl_calibrate.py` — A–N and Z all PASS (54/55; the one FAIL is the known Open-vs-inline IMD).
      **sPk ≤ −1.00 and latency 3003 unchanged** — a default change cannot move either.
- [ ] `dev_mb_release_ms` correctly greys out on a fresh instance (Smart selected), per UI-4.
- [ ] Build clean, AU + VST3, **both installed**, mtimes for both.

## Non-goals
- Do not remove `Manual` or `Lookahead` — both stay as comparison arms.
- No change to Transparent, Ceiling/Drive, or the `dev_smart_*` values (already set by SMART-1.1).
