# SLICE AB-1.2 — blind A/B labels are inverted

**Status:** ready for Cursor · **Architect:** Claude · **Reported by:** avishali (DAW) · **Scope:** UI only, `DevControlsComponent.{h,cpp}`. No DSP, no params, no SDK.

## Report (DAW, build 11:32)
> *"A/B are level matched."* ✅ — **AB-1.1's control-law fix is CONFIRMED WORKING in a host.**
> *"problem is it says A and B until i press the match button, then its replaced by the engine name."*

So the labels are **exactly inverted**:

| `ab_match` | shows | should show |
|---|---|---|
| OFF (default) | **A / B** | Transparent / Open |
| ON | **Transparent / Open** | A / B |

## Prime suspect — stale raw-parameter read in the listener

`attAbMatchListener_` (~L412) receives the **new** value as a callback argument, but then calls
`refreshEngineSelectorLabels()`, which calls `isBlindEngineLabels()` (~L786), which **re-reads**
`apvts_.getRawParameterValue (param::ab_match)` instead of using the value it was just handed:

```cpp
[this] (float value)                       // new value, available right here
{
    if (value < 0.5f) { abReveal_ = false; btnAbReveal_.setToggleState (false, ...); }
    refreshEngineSelectorLabels();          // re-reads the raw atomic instead of using `value`
    resized();
}
```

If the raw atomic is not yet committed when the `ParameterAttachment` callback fires, the refresh sees the
**previous** state and the labels lag one toggle behind — which presents exactly as inversion.

**Fix:** thread the authoritative value through instead of re-reading it.
`refreshEngineSelectorLabels (bool matchOn)` / `isBlindEngineLabels (bool matchOn)`, with the listener
passing `value >= 0.5f`. Keep a no-arg overload for the construction-time and `resized()` call sites
(~L149, ~L504) that reads the parameter, since no callback value exists there.

⚠️ Verify this is actually the cause before changing code — read the ordering and say what you found. The
same symptom could come from the construction-time call at ~L504 running before the attachment's initial
update. **If it is that instead, fix that and say so.**

## Gate
- [ ] **Fresh instance, `ab_match` OFF ⇒ selector reads Transparent / Open.** (This is the startup case
      avishali sees wrong today.)
- [ ] Toggle Match ON ⇒ reads **A / B**. Toggle OFF again ⇒ back to real names. Repeat several times —
      it must not lag a toggle behind at any point.
- [ ] `Reveal` ON while Match ON ⇒ real names. `Reveal` is force-cleared when Match goes OFF (existing
      behaviour, keep it).
- [ ] Selecting A or B with Match ON still selects the correct engine — confirm `dev_mb_engine` in the
      preset matches what was audibly selected. **The relabel must not corrupt the engine choice** (that
      race is what the `onChange` suspension in `refreshEngineSelectorLabels` already guards).
- [ ] `mbl_calibrate.py` 58/59, latency 3003 — a UI fix must move no measurement.
- [ ] Build clean (ASCII gate), AU + VST3, both installed, mtimes.

## Non-goals
- Do not touch the A/B match DSP — it is confirmed working in a DAW. UI only.
