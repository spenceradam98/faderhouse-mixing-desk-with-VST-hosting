#pragma once
#include "Controls.h"
#include "ChainUI.h"

class FaderhouseProcessor;

namespace fh
{
// FX returns, bus compressor, master inserts and master fader.
class MasterSection : public juce::Component
{
public:
    MasterSection (FaderhouseProcessor&, ChainUI&);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kWidth = 224;

private:
    FaderhouseProcessor& proc;
    ChainUI& chains;

    struct Return
    {
        juce::TextButton fx;
        std::unique_ptr<Knob> level, pan;
        ToggleBtn mute { "M", ToggleBtn::Role::Mute };
        Meter meter;
        juce::Rectangle<int> labelArea;
    };
    std::array<Return, 4> returns;
    Knob bcThr { Cap::Dyn, "THRESH" }, bcRatio { Cap::Dyn, "RATIO" }, bcAtk { Cap::Dyn, "ATTACK" },
         bcRel { Cap::Dyn, "RELEASE" }, bcMakeup { Cap::Dyn, "MAKEUP" }, bcMix { Cap::Dyn, "MIX" };
    ToggleBtn bcOn { "BUS COMP" };
    Meter bcGr { Meter::Kind::Reduction };
    juce::TextButton masterIns;
    juce::Slider fader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    Meter meter;
    juce::Rectangle<int> secReturns, secComp, secMaster, scaleArea, grArea;
    float lastGr = 0;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;
};
} // namespace fh
