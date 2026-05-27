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
