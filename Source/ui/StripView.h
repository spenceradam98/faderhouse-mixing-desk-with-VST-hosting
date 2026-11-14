#pragma once
#include "Controls.h"
#include "ChainUI.h"

class FaderhouseProcessor;

namespace fh
{
// One compact channel strip in the console view: source, inserts, sends, pan,
// solo/mute/select, fader, meter and a scribble strip. The full processing
// controls of the selected strip live in the FocusPanel.
class StripView : public juce::Component
{
public:
    StripView (FaderhouseProcessor&, ChainUI&, int channel, std::function<void (int)> onSelect);
    ~StripView() override;

    void refresh (bool selected); // ~30 Hz from the editor
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int kWidth = 60;

private:
    void showSourceMenu();
    void editName();

    FaderhouseProcessor& proc;
    ChainUI& chains;
    const int ch;
    std::function<void (int)> onSelect;

    juce::TextButton sourceBtn, insertBtn;
    std::array<std::unique_ptr<Knob>, 4> sends;
    Knob pan { Cap::Pan, "PAN", true };
    ToggleBtn solo { "S", ToggleBtn::Role::Solo }, mute { "M", ToggleBtn::Role::Mute }, sel { "SEL", ToggleBtn::Role::Select };
    juce::Slider fader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    Meter meter;
    juce::Rectangle<int> scribble, ledArea, scaleArea;
    bool dynLit = false, eqLit = false, isSelected = false;
    float grDb = 0;
    std::unique_ptr<juce::Label> nameEditor;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;
};
} // namespace fh
