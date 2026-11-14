#pragma once
#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/ChainUI.h"
#include "ui/StripView.h"
#include "ui/FocusPanel.h"
#include "ui/MasterSection.h"

class FaderhouseEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FaderhouseEditor (FaderhouseProcessor&);
    ~FaderhouseEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseW = fh::FocusPanel::kWidth + fh::kNumChannels * fh::StripView::kWidth + fh::MasterSection::kWidth;
    static constexpr int kBaseH = 880;
    static constexpr int kHeaderH = 40;

    void selectChannel (int ch);
    void setTheme (int index);
    void setScale (double scale);
    void refreshMeters() { timerCallback(); } // for the screenshot tool

private:
    void timerCallback() override;

    class Header;
    FaderhouseProcessor& proc;
    fh::ConsoleLookAndFeel lnf;
    fh::ChainUI chainUI;
    juce::Component content;
    std::unique_ptr<Header> header;
    juce::OwnedArray<fh::StripView> strips;
    std::unique_ptr<fh::FocusPanel> focus;
    std::unique_ptr<fh::MasterSection> master;
    juce::TooltipWindow tooltips { this, 600 };
    int selected = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaderhouseEditor)
};
