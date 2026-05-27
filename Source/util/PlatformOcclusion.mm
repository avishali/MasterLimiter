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
