#pragma once
#include "Controls.h"
#include "ChainUI.h"
#include "ProcessingPanel.h"

class FaderhouseProcessor;

namespace fh
{
// Full controls for the selected console channel: routing, instrument,
// inserts, then the shared input/filter/dynamics/EQ panel.
class FocusPanel : public juce::Component, private juce::ChangeListener
{
public:
    FocusPanel (FaderhouseProcessor&, ChainUI&, int channel);
    ~FocusPanel() override;
    void refresh(); // ~30 Hz
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    int getChannel() const { return ch; }

    static constexpr int kWidth = 372;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuildSlots();

    FaderhouseProcessor& proc;
    ChainUI& chains;
    const int ch;

    juce::ComboBox source, midiCh;
    juce::TextButton instrumentBtn, addInsertBtn { "+ ADD" };
    ToggleBtn insPre { "INS PRE" };
    juce::OwnedArray<juce::TextButton> slotButtons;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAtt, midiAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> insPreAtt;
    ProcessingPanel processing;
    juce::Rectangle<int> secRouting, secInserts;
};
} // namespace fh
