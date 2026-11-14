#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/ChannelStrip.h"

namespace fh
{
// Live frequency response of a channel's filters and EQ.
class EqCurve : public juce::Component
{
public:
    explicit EqCurve (ChannelStrip& s) : strip (s) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
private:
    ChannelStrip& strip;
};
} // namespace fh
