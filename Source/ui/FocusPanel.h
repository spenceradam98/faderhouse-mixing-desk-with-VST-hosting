#pragma once
#include "Controls.h"
#include "ChainUI.h"

class FaderhouseProcessor;

namespace fh
{
class EqCurve : public juce::Component
{
public:
    EqCurve (FaderhouseProcessor&, int channel);
    void paint (juce::Graphics&) override;
private:
    FaderhouseProcessor& proc;
    int ch;
};

// Full controls for the selected channel: routing, instrument, inserts,
// input stage, filters, dynamics and the four-band EQ with its curve.
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
    Knob& knob (Cap, const juce::String& caption, const char* id, bool bipolar = false);
    ToggleBtn& toggle (const juce::String& text, const char* id, ToggleBtn::Role = ToggleBtn::Role::Led);

    FaderhouseProcessor& proc;
    ChainUI& chains;
    const int ch;

    juce::ComboBox source, midiCh;
    juce::TextButton instrumentBtn, addInsertBtn { "+ ADD" };
    juce::OwnedArray<juce::TextButton> slotButtons;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<ToggleBtn> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAtt, midiAtt;
    EqCurve curve;
    Meter compGr { Meter::Kind::Reduction }, gateGr { Meter::Kind::Reduction };

    // named controls for layout
    Knob *trim, *drive, *hpf, *lpf, *cThr, *cRatio, *cRel, *gThr, *gRange, *gRel,
         *hfG, *hfF, *hmG, *hmF, *hmQ, *lmG, *lmF, *lmQ, *lfG, *lfF;
    ToggleBtn *phase, *insPre, *cOn, *cFast, *gOn, *gExp, *dynPre, *eqOn, *eqBlk, *hfBell, *lfBell;
    juce::Rectangle<int> secRouting, secInserts, secInput, secDyn, secEq;
    int lastSlotCount = -1;
};
} // namespace fh
