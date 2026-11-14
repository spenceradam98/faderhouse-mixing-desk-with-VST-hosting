#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace fh
{
enum class Cap { Input, Filter, Dyn, Gate, HF, HMF, LMF, LF, Send, Pan };

class ConsoleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ConsoleLookAndFeel();
    void setTheme (const Theme&);
    const Theme& theme() const { return *th; }
    juce::Colour capColour (Cap) const;
    juce::Font font (float h, bool bold = true) const;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 22; }
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (11.0f, h * 0.6f)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (11.0f); }
    juce::Font getPopupMenuFont() override { return font (13.0f, false); }
    juce::Font getLabelFont (juce::Label& l) override { return font ((float) l.getFont().getHeight()); }
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    static const Theme& themeOf (const juce::Component&);
    static ConsoleLookAndFeel* of (const juce::Component&);

private:
    const Theme* th = &getTheme (0);
};

// Drawing helpers shared by the panels
void drawSection (juce::Graphics&, juce::Rectangle<float>, const Theme&, const juce::String& title = {});
void drawScrew (juce::Graphics&, juce::Point<float>, const Theme&);

// Rotary knob with a coloured cap and a small caption underneath.
class Knob : public juce::Slider
{
public:
    Knob (Cap cap, juce::String caption, bool bipolar = false);
    Cap cap;
    juce::String caption;
    bool bipolar;
    void paintOverChildren (juce::Graphics&) override;
};

// Knob, toggle and choice widgets bound to parameters.
struct Bound
{
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> combo;
};

std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
attachKnob (juce::AudioProcessorValueTreeState&, const juce::String& id, juce::Slider&);
std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
attachToggle (juce::AudioProcessorValueTreeState&, const juce::String& id, juce::Button&);

// A small latching console button. "role" picks the lit colour.
class ToggleBtn : public juce::TextButton
{
public:
    enum class Role { Led, Mute, Solo, Select };
    ToggleBtn (const juce::String& text, Role r = Role::Led) : juce::TextButton (text), role (r) { setClickingTogglesState (true); }
    Role role;
};

// Peak meter (stereo bars or segments) with peak hold, or a gain reduction meter.
class Meter : public juce::Component
{
public:
    enum class Kind { Stereo, Mono, Reduction };
    explicit Meter (Kind k = Kind::Stereo) : kind (k) {}
    void push (float l, float r); // linear peaks (or GR dB for Reduction, <= 0)
    void paint (juce::Graphics&) override;

private:
    Kind kind;
    float level[2] {}, hold[2] {};
    int holdFrames[2] {};
};
} // namespace fh
