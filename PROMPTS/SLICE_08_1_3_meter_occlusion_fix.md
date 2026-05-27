# Cursor Task — Slice 8.1.3: fix occlusion-triggered audio clicks (editor timer) + clear the int→float warning

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product repo only. UI/editor-thread only — NO DSP, NO APVTS changes.**
> Run BEFORE the close prompt (`SLICE_08_1_2_close.md`). Do NOT commit —
> architect re-auditions the exact repro first.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

Obey `third_party/melechdsp-hq/PROMPTS/00_SYSTEM_RULES.txt`:

- ONE focused diff, files in §3 only. No HQ edits, no AnalyzerPro edits.
- No DSP, no APVTS, no `PluginProcessor` audio-path changes. The audio
  thread is not touched by this slice.
- Do NOT commit / branch / push. Leave the tree dirty for audition.

────────────────────────────────────────
1. WHY (confirmed diagnosis)
────────────────────────────────────────

Audition (VST3 in Ableton) reproduced audible pops/clicks **only** when:
playback running + plugin editor window open + DAW switched to a different
macOS Space (window occluded). **Closing the editor window eliminates the
clicks entirely.**

Root cause: the 30 Hz `juce::Timer` added in Slice 8
(`MasterLimiterAudioProcessorEditor::timerCallback`) calls
`mainView.syncMetersFromProcessor()` + `mainView.repaintMeterStrip()` every
frame. When the window is occluded on another Space, macOS changes how it
services the window's timer/redraw, and this per-frame GUI work disturbs
the host's audio. Before Slice 8 the editor had no timer — hence "it was
fine before." The limiter DSP (SP path) is unchanged from Slice 5 and is
NOT the cause; the offline bench cannot see this (no GUI, no occlusion).

Fix: **do no meter work when the editor is not actually visible on
screen.** `juce::Component::isShowing()` does NOT detect Space-occlusion on
macOS (the NSWindow is still "visible"), so we must consult
`NSWindow.occlusionState`. The timer keeps ticking as a cheap no-op while
occluded; meters resume immediately when the window becomes visible again
(snapshot atomics keep updating on the audio thread, so no stale state).

────────────────────────────────────────
2. TRINITY (lightweight)
────────────────────────────────────────

Read before editing:
- `Source/PluginEditor.{h,cpp}` (the Timer + `timerCallback`).
- `Source/ui/MainView.{h,cpp}` (`syncMetersFromProcessor`, `repaintMeterStrip`).
- `Source/ui/meters/MeterComponent.cpp` (the int→float warning, §5).
- `CMakeLists.txt` (`target_sources` list).

Output the retrieval log.

────────────────────────────────────────
3. SCOPE — files you may CREATE / MODIFY
────────────────────────────────────────

**CREATE:**
- `Source/util/PlatformOcclusion.h`
- `Source/util/PlatformOcclusion.mm`  (Objective-C++ — macOS impl + portable fallback)

**MODIFY:**
- `Source/PluginEditor.cpp` — gate `timerCallback` on visibility.
- `CMakeLists.txt` — add the two new files to the plugin `target_sources`.
- `Source/ui/meters/MeterComponent.cpp` — the §5 warning fix.

Do NOT touch any other file. No `PluginProcessor`, no `Parameters`, no
`MainView` logic changes (you may read them but the fix lives in the editor).

────────────────────────────────────────
4. IMPLEMENTATION
────────────────────────────────────────

### 4.1 `Source/util/PlatformOcclusion.h`

```cpp
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mdsp
{
/** True if the component's window is currently visible on screen.
    On macOS this consults NSWindow.occlusionState so a window moved to
    another Space (occluded) reports false. Other platforms fall back to
    juce::Component::isShowing(). */
bool isComponentVisibleOnScreen (juce::Component& component);
}
```

### 4.2 `Source/util/PlatformOcclusion.mm`

```objc
#include "PlatformOcclusion.h"

#if JUCE_MAC
 #import <AppKit/AppKit.h>

namespace mdsp
{
bool isComponentVisibleOnScreen (juce::Component& component)
{
    if (! component.isShowing())
        return false;

    if (auto* peer = component.getPeer())
    {
        if (auto* view = static_cast<NSView*> (peer->getNativeHandle()))
        {
            if (NSWindow* window = [view window])
                return ([window occlusionState] & NSWindowOcclusionStateVisible) != 0;
        }
    }

    return true; // best-effort: if we can't query, assume visible
}
}
#else
namespace mdsp
{
bool isComponentVisibleOnScreen (juce::Component& component)
{
    return component.isShowing();
}
}
#endif
```

(If `peer->getNativeHandle()` typing needs a cast adjustment for your JUCE
version, adapt minimally — the goal is the `occlusionState` query.)

### 4.3 `Source/PluginEditor.cpp` — gate the timer

Add `#include "util/PlatformOcclusion.h"`. Change `timerCallback`:

```cpp
void MasterLimiterAudioProcessorEditor::timerCallback()
{
    if (! mdsp::isComponentVisibleOnScreen (*this))
        return; // occluded / off-screen: skip meter sync + repaint entirely

    mainView.syncMetersFromProcessor();
    mainView.repaintMeterStrip();
}
```

Leave the timer running at 30 Hz (the early-return is the whole fix). Do
not change the timer rate or the constructor/destructor timer lifecycle.

### 4.4 CMakeLists.txt

Add to the plugin `target_sources` list (next to the other `Source/ui`
entries):

```
        Source/util/PlatformOcclusion.h
        Source/util/PlatformOcclusion.mm
```

────────────────────────────────────────
5. INT→FLOAT WARNING FIX
────────────────────────────────────────

`Source/ui/meters/MeterComponent.cpp` has one implicit `int → float`
conversion warning (reported in the Slice 8.1.1 build). Locate the exact
line from the compiler output and make it explicit with
`static_cast<float>(...)` (or a float literal). Pure type-clarity, no
numeric change. Report file:line + before/after.

────────────────────────────────────────
6. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
cmake --build build-debug -j
cmake --build build-release -j
```

- Zero warnings from MasterLimiter `Source/` files (the int→float one must
  be gone; confirm `.mm` compiles clean).
- **No bench run required** — this is editor-only, the DSP/audio path is
  untouched, so Slice 3/4/5 behavior is unchanged by construction. (If you
  want a sanity check you may run them, but it is not a gate here.)

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log.
2. Files created/modified, one-line purpose each.
3. The `timerCallback` before/after and confirmation the timer lifecycle
   is otherwise unchanged.
4. Warning fix: file:line + before/after; rebuild confirmation that no
   `Source/` warnings remain (both configs).
5. `git status --short` showing the new util files + the modified editor/
   meter/CMake files, all uncommitted.
6. Explicit confirmation: no commit, no push.
7. Open questions.

────────────────────────────────────────
8. ARCHITECT RE-AUDITION (after Cursor reports)
────────────────────────────────────────

avishali re-runs the exact repro in Ableton (VST3): playback running,
editor window open, switch to another Space and back. Expected: **no
clicks while occluded, and meters resume live on return.** Also spot-check
that meters/LUFS still update normally when the window is visible.

On audition pass → the close prompt (`SLICE_08_1_2_close.md`, updated to
stage the new `Source/util/` files in the Slice 8.1 commit) runs: two
clean commits + the already-written PROGRESS update.
