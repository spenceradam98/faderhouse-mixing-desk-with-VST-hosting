#pragma once
#include "StripProcessor.h"
#include "ui/Controls.h"
#include "ui/ProcessingPanel.h"

class FaderhouseStripEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FaderhouseStripEditor (FaderhouseStripProcessor&);
    ~FaderhouseStripEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void setTheme (int);
    void refreshMeters() { timerCallback(); }

    static constexpr int kPanelW = 372, kOutW = 150, kHeaderH = 40;
    static constexpr int kBaseW = kPanelW + kOutW, kBaseH = 700;

private:
    void timerCallback() override;

    class Content : public juce::Component
    {
    public:
        explicit Content (FaderhouseStripEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        FaderhouseStripEditor& ed;
    };

    FaderhouseStripProcessor& proc;
    fh::ConsoleLookAndFeel lnf;
    Content content { *this };
    juce::ComboBox theme, size;
    fh::ProcessingPanel processing;
    fh::Knob pan { fh::Cap::Pan, "PAN", true };
    fh::ToggleBtn mute { "MUTE", fh::ToggleBtn::Role::Mute };
    juce::Slider fader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    fh::Meter inMeter, outMeter;
    juce::Rectangle<int> outSection, scaleArea;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAtt, faderAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAtt;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaderhouseStripEditor)
};
