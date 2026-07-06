# SLICE UI-2.1a — Route the Transparent-engine guard through the async (message-thread) path

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (thread-safety + behaviour) · **Audition:** avishali
**Repo/scope:** plugin `MasterLimiter` — `Source/PluginProcessor.cpp` / `.h` only. Tiny (~8 lines). No DSP algorithm change, no param change, no UI change. **Read `docs/ENGINE_NAMING.md`.**
**Depends on:** UI-2.1 (the guard `enforceTransparentEngineGuard()` — already implemented). This slice does NOT change what the guard does, only *which thread* it runs on when triggered by a parameter change.

## Why (the defect this closes)
UI-2.1 enforces the invariant `dev_mb_engine==OFF ⟹ dev_release_engine==Lookahead` at two entry points:
1. **State load** — `enforceTransparentEngineGuard()` called from `setStateInformation()` (`PluginProcessor.cpp:2599`). ✅ This is genuinely on the message thread — **leave it as-is.**
2. **Runtime listener** — `parameterChanged()` (`PluginProcessor.cpp:802`) calls `enforceTransparentEngineGuard()` **synchronously and inline** (`:804-809`).

The problem is (2). `AudioProcessorValueTreeState::Listener::parameterChanged` can be invoked on the **audio thread** when a host automates a parameter during `processBlock`. The guard calls `AudioParameterChoice::setValueNotifyingHost()` (`:797-799`) — a host-notifying param write that must not run on the audio thread (RT §3). **Evidence it's audio-thread-reachable in this very file:** the sibling branches of `parameterChanged` (crossover `:812-822`, lookahead `:826-843`, mb-lookahead `:836-843`) deliberately do NOT work inline — they set an atomic dirty flag + `triggerAsyncUpdate()` and defer the real work to `handleAsyncUpdate()`. The guard branch must follow that same established pattern.

## The change (match the existing idiom exactly)

**1. New atomic dirty flag** — `PluginProcessor.h`, next to the other `*Dirty_` members (near `:411 mbEngineLookaheadDirty_`):
```cpp
std::atomic<bool> transparentGuardDirty_ { false };
```

**2. `parameterChanged()` guard branch** (`PluginProcessor.cpp:804-810`) — replace the inline call with the deferred pattern used by the crossover/lookahead branches:
```cpp
if (parameterID == param::dev_mb_engine.data()
    || parameterID == param::dev_release_engine.data())
{
    juce::ignoreUnused (newValue);
    // ALPHA GUARD: defer to the message thread (this callback may run on the audio thread under host automation).
    transparentGuardDirty_.store (true, std::memory_order_release);
    triggerAsyncUpdate();
    return;
}
```

**3. `handleAsyncUpdate()`** (`PluginProcessor.cpp:1022`, runs on the message thread) — drain the flag and run the guard. Add near the top (before or after the heavy-controls block, doesn't matter — it's independent):
```cpp
if (transparentGuardDirty_.exchange (false, std::memory_order_acq_rel))
    enforceTransparentEngineGuard();
```

**4. Leave `enforceTransparentEngineGuard()` itself untouched**, and leave its **direct call in `setStateInformation()` (`:2599`) untouched** — state load is already message-thread and must stay synchronous (the state must be consistent by the time load returns).

## Correctness notes (confirm these hold)
- **Thread:** `handleAsyncUpdate()` is a `juce::AsyncUpdater` callback → always message thread. The only host-notifying write (`setValueNotifyingHost`) now runs there or in `setStateInformation` — never on the audio thread. RT §3 satisfied.
- **Re-entrancy still terminates:** guard coerces `dev_release_engine` → `parameterChanged` fires again → sets `transparentGuardDirty_` + `triggerAsyncUpdate()` (does NOT re-enter `handleAsyncUpdate` synchronously) → next async pass runs the guard, sees `index == Lookahead (1)` → returns. One extra no-op async cycle. Fine.
- **No coalescing loss:** multiple rapid `dev_mb_engine`/`dev_release_engine` changes collapse to one `handleAsyncUpdate` pass — correct, the guard is idempotent (it reads current param state, not `newValue`).
- **Latency of enforcement:** the coercion now lands on the next message-thread tick instead of inline. For a hidden DEV param that is imperceptible and correct; the UI selector's own coercion (`DevControlsComponent::applyEngineSelectorChoice`) still fires immediately for the user-click path, so nothing user-visible regresses.

## Non-goals
- No change to the guard's logic/conditions, no change to `setStateInformation`'s direct call, no new param, no DSP change, no UI change. Do NOT touch Smart/Adaptive params or code — the guard stays temporary (delete-trigger unchanged: removed when Smart becomes a selectable engine).

## Build / verify
- Build clean, AU + VST3 + Standalone; ASCII gate passes.
- **(Claude verify)** (1) `parameterChanged` guard branch no longer calls `enforceTransparentEngineGuard()` inline — it sets `transparentGuardDirty_` + `triggerAsyncUpdate()`. (2) `handleAsyncUpdate()` drains `transparentGuardDirty_` and calls the guard. (3) `setStateInformation` still calls the guard directly. (4) Behaviour unchanged: load `dev_mb_engine=OFF` + `dev_release_engine=Smart` → after load reads back Lookahead; flipping `dev_release_engine` to Smart/Adaptive while MB off → coerced back to Lookahead; MB=ON leaves release alone.

## Output requirements
1. Retrieval log (the three edit sites + the header member). 2. Diff. 3. Confirm the only `setValueNotifyingHost` paths are now message-thread (`handleAsyncUpdate` + `setStateInformation`). 4. Confirm guard logic + Smart/Adaptive code otherwise untouched. 5. Build. 6. Open questions.
