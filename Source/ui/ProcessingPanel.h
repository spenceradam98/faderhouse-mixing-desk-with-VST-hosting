#pragma once
#include "Controls.h"
#include "EqCurve.h"

namespace fh
{
// Input stage, filters, dynamics and EQ controls for one channel. Used by the
// console's channel panel and by the single-strip plugin.
class ProcessingPanel : public juce::Component
{
public:
    ProcessingPanel (juce::AudioProcessorValueTreeState&, ChannelStrip&, int channel);
    void refresh(); // ~30 Hz: GR meters, EQ curve
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kInputH = 84, kDynH = 160, kGap = 6;

private:
    Knob& knob (Cap, const juce::String& caption, const char* id, bool bipolar = false);
    ToggleBtn& toggle (const juce::String& text, const char* id);

    juce::AudioProcessorValueTreeState& state;
    ChannelStrip& strip;
    const int ch;

    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<ToggleBtn> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;
    EqCurve curve;
    Meter compGr { Meter::Kind::Reduction }, gateGr { Meter::Kind::Reduction };

    Knob *trim, *drive, *hpf, *lpf, *cThr, *cRatio, *cRel, *gThr, *gRange, *gRel,
         *hfG, *hfF, *hmG, *hmF, *hmQ, *lmG, *lmF, *lmQ, *lfG, *lfF;
    ToggleBtn *phase, *cOn, *cFast, *gOn, *gExp, *dynPre, *eqOn, *eqBlk, *hfBell, *lfBell;
    juce::Rectangle<int> secInput, secDyn, secEq;
};
} // namespace fh
