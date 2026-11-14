#include "Controls.h"

namespace fh
{
using namespace juce;

ConsoleLookAndFeel::ConsoleLookAndFeel() { setTheme (getTheme (0)); }

ConsoleLookAndFeel* ConsoleLookAndFeel::of (const Component& c)
{
    return dynamic_cast<ConsoleLookAndFeel*> (&c.getLookAndFeel());
}

const Theme& ConsoleLookAndFeel::themeOf (const Component& c)
{
    if (auto* l = of (c)) return l->theme();
    return getTheme (0);
}

Font ConsoleLookAndFeel::font (float h, bool bold) const
{
    return Font (FontOptions (h, bold ? Font::bold : Font::plain));
}

void ConsoleLookAndFeel::setTheme (const Theme& t)
{
    th = &t;
    setColour (ResizableWindow::backgroundColourId, t.panel);
    setColour (Label::textColourId, t.text);
    setColour (TextButton::buttonColourId, t.buttonOff);
    setColour (TextButton::textColourOffId, t.buttonText);
    setColour (TextButton::textColourOnId, t.buttonText);
    setColour (ComboBox::backgroundColourId, t.screen);
    setColour (ComboBox::textColourId, t.curve);
    setColour (ComboBox::outlineColourId, t.sectionEdge);
    setColour (ComboBox::arrowColourId, t.curve);
    setColour (PopupMenu::backgroundColourId, t.section);
    setColour (PopupMenu::textColourId, t.text);
    setColour (PopupMenu::highlightedBackgroundColourId, t.select);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (PopupMenu::headerTextColourId, t.textDim);
    setColour (TooltipWindow::backgroundColourId, t.screen);
    setColour (TooltipWindow::textColourId, t.curve);
    setColour (BubbleComponent::backgroundColourId, t.screen);
    setColour (BubbleComponent::outlineColourId, t.sectionEdge);
    setColour (TextEditor::backgroundColourId, t.screen);
    setColour (TextEditor::textColourId, t.curve);
    setColour (TextEditor::highlightColourId, t.select.withAlpha (0.5f));
    setColour (Label::textWhenEditingColourId, t.curve);
    setColour (Slider::textBoxTextColourId, t.text);
    setColour (DocumentWindow::backgroundColourId, t.panel);
    setColour (ListBox::backgroundColourId, t.section);
    setColour (ListBox::textColourId, t.text);
}

Colour ConsoleLookAndFeel::capColour (Cap c) const
{
    switch (c)
    {
        case Cap::Input:  return th->capInput;
        case Cap::Filter: return th->capFilter;
        case Cap::Dyn:    return th->capDyn;
        case Cap::Gate:   return th->capGate;
        case Cap::HF:     return th->capHF;
        case Cap::HMF:    return th->capHMF;
        case Cap::LMF:    return th->capLMF;
        case Cap::LF:     return th->capLF;
        case Cap::Send:   return th->capSend;
        case Cap::Pan:    return th->capPan;
    }
    return th->capPan;
}

Slider::SliderLayout ConsoleLookAndFeel::getSliderLayout (Slider& s)
{
    // Captions are handled in drawRotarySlider, so the layout never depends on
    // which look-and-feel was active when the knob was first sized.
    return LookAndFeel_V4::getSliderLayout (s);
}

void ConsoleLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto& t = *th;
    auto* knob = dynamic_cast<Knob*> (&s);
    if (knob != nullptr && knob->caption.isNotEmpty())
        h = jmax (8, h - 11);
    const Colour cap = knob ? capColour (knob->cap) : t.capPan;
    const bool bipolar = knob && knob->bipolar;

    const float d = (float) jmin (w, h) - 4.0f;
    const auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const auto r = Rectangle<float> (d, d).withCentre (c);
    const float angle = start + pos * (end - start);

    if (t.flatKnobs)
    {
        // track + value arc
        Path track; track.addCentredArc (c.x, c.y, d * 0.5f - 1, d * 0.5f - 1, 0, start, end, true);
        g.setColour (t.knobBody.contrasting (0.15f)); g.strokePath (track, PathStrokeType (2.5f, PathStrokeType::curved, PathStrokeType::rounded));
        const float from = bipolar ? (start + end) * 0.5f : start;
        Path val; val.addCentredArc (c.x, c.y, d * 0.5f - 1, d * 0.5f - 1, 0, jmin (from, angle), jmax (from, angle), true);
        g.setColour (cap); g.strokePath (val, PathStrokeType (2.5f, PathStrokeType::curved, PathStrokeType::rounded));
        auto body = r.reduced (d * 0.2f);
        g.setColour (t.knobBody); g.fillEllipse (body);
        g.setColour (t.knobBody.contrasting (0.2f)); g.drawEllipse (body, 1.0f);
        const auto p1 = c.getPointOnCircumference (body.getWidth() * 0.12f, angle);
        const auto p2 = c.getPointOnCircumference (body.getWidth() * 0.45f, angle);
        g.setColour (t.knobPointer); g.drawLine ({ p1, p2 }, 2.0f);
        return;
    }

    // Classic: dark skirt, coloured cap, white pointer line
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (r.translated (0, 1.5f));
    ColourGradient skirt (t.knobBody.brighter (0.35f), r.getX(), r.getY(), t.knobBody.darker (0.4f), r.getRight(), r.getBottom(), false);
    g.setGradientFill (skirt); g.fillEllipse (r);
    // skirt ticks
    g.setColour (t.knobBody.brighter (0.6f).withAlpha (0.5f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.0f;
        g.drawLine ({ c.getPointOnCircumference (d * 0.43f, a), c.getPointOnCircumference (d * 0.5f, a) }, 0.8f);
    }
    auto capR = r.reduced (d * 0.2f);
    ColourGradient cg (cap.brighter (0.35f), capR.getX(), capR.getY(), cap.darker (0.35f), capR.getRight(), capR.getBottom(), false);
    g.setGradientFill (cg); g.fillEllipse (capR);
    g.setColour (Colours::white.withAlpha (0.25f)); g.drawEllipse (capR.reduced (0.5f), 0.8f);
    const auto p1 = c.getPointOnCircumference (d * 0.1f, angle);
    const auto p2 = c.getPointOnCircumference (d * 0.48f, angle);
    g.setColour (cap.getPerceivedBrightness() > 0.7f ? Colours::black : t.knobPointer);
    g.drawLine ({ p1, p2 }, 2.2f);
}

void ConsoleLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const auto& t = *th;
    const float cx = (float) x + (float) w * 0.5f;
    auto slot = Rectangle<float> (cx - 2.5f, (float) y, 5.0f, (float) h);
    g.setColour (t.faderSlot); g.fillRoundedRectangle (slot, 2.5f);
    g.setColour (Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (slot, 2.5f, 1.0f);

    const float capW = jmin ((float) w - 4.0f, 30.0f), capH = 44.0f;
    auto capR = Rectangle<float> (capW, capH).withCentre ({ cx, pos });
    g.setColour (Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (capR.translated (0, 2), 3.0f);
    ColourGradient cg (t.faderCap.brighter (0.45f), capR.getX(), capR.getY(), t.faderCap.darker (0.4f), capR.getX(), capR.getBottom(), false);
    if (! t.bevel) cg = ColourGradient (t.faderCap, capR.getX(), capR.getY(), t.faderCap.darker (0.08f), capR.getX(), capR.getBottom(), false);
    g.setGradientFill (cg); g.fillRoundedRectangle (capR, 3.0f);
    g.setColour (t.faderCap.contrasting (0.25f).withAlpha (0.6f));
    for (int i = -3; i <= 3; ++i)
        if (i != 0) g.drawHorizontalLine ((int) (pos + (float) i * 5.0f), capR.getX() + 4, capR.getRight() - 4);
    g.setColour (t.faderLine); g.fillRect (capR.getX() + 2, pos - 1.0f, capR.getWidth() - 4, 2.0f);
    g.setColour (Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (capR, 3.0f, 1.0f);
}

void ConsoleLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    const auto& t = *th;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    Colour on = t.ledOn;
    if (auto* tb = dynamic_cast<ToggleBtn*> (&b))
    {
        if (tb->role == ToggleBtn::Role::Mute) on = t.mute;
        if (tb->role == ToggleBtn::Role::Solo) on = t.solo;
        if (tb->role == ToggleBtn::Role::Select) on = t.select;
    }
    Colour base = b.getToggleState() ? on : t.buttonOff;
    if (over) base = base.brighter (0.08f);
    if (down) base = base.darker (0.15f);
    if (t.bevel)
    {
        ColourGradient cg (base.brighter (0.2f), r.getX(), r.getY(), base.darker (0.2f), r.getX(), r.getBottom(), false);
        g.setGradientFill (cg);
    }
    else g.setColour (base);
    g.fillRoundedRectangle (r, 2.5f);
    if (b.getToggleState())
    {
        g.setColour (on.withAlpha (0.35f));
        g.drawRoundedRectangle (r.expanded (0.5f), 3.0f, 1.5f);
    }
    g.setColour (Colours::black.withAlpha (t.bevel ? 0.45f : 0.12f));
    g.drawRoundedRectangle (r, 2.5f, 1.0f);
}

void ConsoleLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const auto& t = *th;
    Colour bg = b.getToggleState() ? t.ledOn : t.buttonOff;
    if (auto* tb = dynamic_cast<ToggleBtn*> (&b); tb && b.getToggleState())
    {
        if (tb->role == ToggleBtn::Role::Mute) bg = t.mute;
        if (tb->role == ToggleBtn::Role::Solo) bg = t.solo;
        if (tb->role == ToggleBtn::Role::Select) bg = t.select;
    }
    g.setColour (bg.contrasting (0.85f).withAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), Justification::centred, 1, 0.7f);
}

void ConsoleLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto& t = *th;
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (t.screen); g.fillRoundedRectangle (r, 2.5f);
    g.setColour (t.sectionEdge); g.drawRoundedRectangle (r, 2.5f, 1.0f);
    Path arrow;
    const float ax = (float) w - 9.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 2, ax + 3.5f, ay - 2, ax, ay + 2.5f);
    g.setColour (t.curve.withAlpha (box.isEnabled() ? 0.9f : 0.3f)); g.fillPath (arrow);
}

void ConsoleLookAndFeel::positionComboBoxText (ComboBox& box, Label& l)
{
    l.setBounds (2, 0, box.getWidth() - 16, box.getHeight());
    l.setFont (getComboBoxFont (box));
}

void drawSection (Graphics& g, Rectangle<float> r, const Theme& t, const String& title)
{
    if (t.bevel)
    {
        g.setColour (Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (r.translated (0, 1.5f), 4.0f);
        ColourGradient cg (t.section.brighter (0.06f), r.getX(), r.getY(), t.section.darker (0.06f), r.getX(), r.getBottom(), false);
        g.setGradientFill (cg);
    }
    else g.setColour (t.section);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (t.sectionEdge); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (t.textDim);
        g.setFont (Font (FontOptions (10.0f, Font::bold)));
        g.drawText (title, r.withHeight (14.0f).reduced (6, 0), Justification::centredLeft);
    }
}

void drawScrew (Graphics& g, Point<float> p, const Theme& t)
{
    if (! t.bevel) return;
    auto r = Rectangle<float> (7, 7).withCentre (p);
    g.setColour (t.panel.darker (0.4f)); g.fillEllipse (r);
    g.setColour (t.panel.brighter (0.3f)); g.drawEllipse (r, 0.8f);
    g.setColour (t.panel.darker (0.7f)); g.drawLine (r.getX() + 1.5f, r.getCentreY(), r.getRight() - 1.5f, r.getCentreY(), 1.0f);
}

//==============================================================================
Knob::Knob (Cap c, String cap_, bool bip) : Slider (RotaryHorizontalVerticalDrag, NoTextBox), cap (c), caption (std::move (cap_)), bipolar (bip)
{
    setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    setPopupDisplayEnabled (true, true, nullptr, 1200);
    setVelocityBasedMode (false);
    setMouseDragSensitivity (220);
}

void Knob::paintOverChildren (Graphics& g)
{
    if (caption.isEmpty()) return;
    const auto& t = ConsoleLookAndFeel::themeOf (*this);
    g.setColour (t.textDim);
    g.setFont (Font (FontOptions (9.5f, Font::bold)));
    g.drawText (caption, getLocalBounds().removeFromBottom (11), Justification::centred, false);
}

std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> attachKnob (AudioProcessorValueTreeState& s, const String& id, Slider& k)
{
    auto a = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, id, k);
    if (auto* p = s.getParameter (id))
        k.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    if (auto* p = s.getParameter (id))
        k.textFromValueFunction = [p] (double v) { return p->getText (p->convertTo0to1 ((float) v), 32); };
    k.updateText();
    return a;
}

std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> attachToggle (AudioProcessorValueTreeState& s, const String& id, Button& b)
{
    return std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, id, b);
}

//==============================================================================
void Meter::push (float l, float r)
{
    const float v[2] { l, r };
    for (int i = 0; i < 2; ++i)
    {
        if (kind == Kind::Reduction)
        {
            const float x = jlimit (0.0f, 1.0f, -v[i] / 20.0f); // 0..20 dB
            level[i] = x > level[i] ? x : level[i] * 0.85f;
            continue;
        }
        const float db = Decibels::gainToDecibels (v[i], -60.0f);
        const float x = jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f); // -60..+6 dBFS
        level[i] = x > level[i] ? x : jmax (x, level[i] - 0.025f);
        if (x >= hold[i]) { hold[i] = x; holdFrames[i] = 45; }
        else if (--holdFrames[i] <= 0) hold[i] = jmax (0.0f, hold[i] - 0.01f);
    }
    repaint();
}

void Meter::paint (Graphics& g)
{
    const auto& t = ConsoleLookAndFeel::themeOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (t.meterOff.darker (0.3f)); g.fillRoundedRectangle (r, 2.0f);
    r = r.reduced (1.5f);

    if (kind == Kind::Reduction)
    {
        auto bar = r.withHeight (r.getHeight() * level[0]);
        g.setColour (t.ledOn); g.fillRect (bar);
        return;
    }

    const int nBars = kind == Kind::Mono ? 1 : 2;
    const float bw = (r.getWidth() - (float) (nBars - 1)) / (float) nBars;
    const float zeroDb = 60.0f / 66.0f, warn = 54.0f / 66.0f;
    for (int i = 0; i < nBars; ++i)
    {
        auto col = Rectangle<float> (r.getX() + (float) i * (bw + 1.0f), r.getY(), bw, r.getHeight());
        // segments
        const int segs = jmax (8, (int) (col.getHeight() / 4.0f));
        const float sh = col.getHeight() / (float) segs;
        for (int s = 0; s < segs; ++s)
        {
            const float frac = (float) (s + 1) / (float) segs;
            Colour c = frac > zeroDb ? t.meterHigh : (frac > warn ? t.meterMid : t.meterLow);
            if (frac > level[i]) c = t.meterOff;
            g.setColour (c);
            g.fillRect (col.getX(), col.getBottom() - (float) (s + 1) * sh + 0.5f, col.getWidth(), sh - 1.0f);
        }
        if (hold[i] > 0.01f)
        {
            g.setColour (hold[i] > zeroDb ? t.meterHigh : t.meterMid);
            g.fillRect (col.getX(), col.getBottom() - hold[i] * col.getHeight(), col.getWidth(), 1.5f);
        }
    }
}
} // namespace fh
