#include "StripEditor.h"

using namespace juce;
using namespace fh;

FaderhouseStripEditor::FaderhouseStripEditor (FaderhouseStripProcessor& p)
    : AudioProcessorEditor (p), proc (p), processing (p.apvts, p.strip, 0)
{
    const double scale = jlimit (0.6, 1.6, (double) proc.uiState.getProperty ("scale", 1.0));
    setLookAndFeel (&lnf);

    for (auto& t : getThemes()) theme.addItem (t.name, theme.getNumItems() + 1);
    theme.onChange = [this] { setTheme (theme.getSelectedItemIndex()); };
    theme.setTooltip ("Theme");
    content.addAndMakeVisible (theme);
    for (int pct : { 70, 85, 100, 125, 150 }) size.addItem (String (pct) + "%", pct);
    size.onChange = [this] { const double s = size.getSelectedId() / 100.0; setSize (roundToInt (kBaseW * s), roundToInt (kBaseH * s)); };
    size.setTooltip ("Window size");
    content.addAndMakeVisible (size);

    content.addAndMakeVisible (processing);
    for (auto* c : std::initializer_list<Component*> { &pan, &mute, &fader, &inMeter, &outMeter }) content.addAndMakeVisible (c);
    panAtt = attachKnob (proc.apvts, cid (0, "pan"), pan);
    faderAtt = attachKnob (proc.apvts, cid (0, "fader"), fader);
    muteAtt = attachToggle (proc.apvts, cid (0, "mute"), mute);
    fader.setPopupDisplayEnabled (true, true, nullptr, 1200);
    fader.setTooltip ("Output fader (double-click: 0 dB)");
    addAndMakeVisible (content);

    setTheme ((int) proc.uiState.getProperty ("theme", 0));
    setResizable (true, true);
    setResizeLimits (kBaseW * 6 / 10, kBaseH * 6 / 10, kBaseW * 16 / 10, kBaseH * 16 / 10);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kBaseW / kBaseH);
    setSize (roundToInt (kBaseW * scale), roundToInt (kBaseH * scale));
    startTimerHz (30);
}

FaderhouseStripEditor::~FaderhouseStripEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FaderhouseStripEditor::setTheme (int i)
{
    i = jlimit (0, (int) getThemes().size() - 1, i);
    proc.uiState.setProperty ("theme", i, nullptr);
    lnf.setTheme (getTheme (i));
    theme.setSelectedItemIndex (i, dontSendNotification);
    sendLookAndFeelChange();
    repaint();
}

void FaderhouseStripEditor::resized()
{
    if (getWidth() <= 0) return;
    const double scale = (double) getWidth() / kBaseW;
    proc.uiState.setProperty ("scale", scale, nullptr);
    content.setBounds (0, 0, kBaseW, kBaseH);
    content.setTransform (AffineTransform::scale ((float) scale));
    size.setSelectedId (roundToInt (scale * 100), dontSendNotification);
    if (size.getSelectedId() == 0) size.setText (String (roundToInt (scale * 100)) + "%", dontSendNotification);

    auto r = Rectangle<int> (0, 0, kBaseW, kBaseH);
    auto head = r.removeFromTop (kHeaderH).reduced (8, 8);
    size.setBounds (head.removeFromRight (64)); head.removeFromRight (6);
    theme.setBounds (head.removeFromRight (130));

    auto body = r.reduced (6, 4);
    processing.setBounds (body.removeFromLeft (kPanelW - 12));
    body.removeFromLeft (6);
    outSection = body;
    auto a = outSection.reduced (6).withTrimmedTop (14);
    pan.setBounds (a.removeFromTop (58).withSizeKeepingCentre (56, 58));
    a.removeFromTop (4);
    mute.setBounds (a.removeFromTop (22).reduced (14, 0));
    a.removeFromTop (12);
    a.removeFromBottom (20);
    auto meters = a.removeFromRight (34);
    inMeter.setBounds (meters.removeFromLeft (15).reduced (0, 18));
    meters.removeFromLeft (4);
    outMeter.setBounds (meters.removeFromLeft (15).reduced (0, 18));
    scaleArea = a.removeFromLeft (26);
    fader.setBounds (a);
}

void FaderhouseStripEditor::Content::paint (Graphics& g)
{
    const auto& t = ConsoleLookAndFeel::themeOf (*this);
    g.fillAll (t.panel.darker (0.08f));
    auto head = Rectangle<float> (0, 0, (float) kBaseW, (float) kHeaderH);
    ColourGradient cg (t.panel.darker (0.25f), 0, 0, t.panel.darker (0.4f), 0, head.getBottom(), false);
    if (! t.bevel) cg = ColourGradient (t.panel.darker (0.05f), 0, 0, t.panel.darker (0.05f), 0, head.getBottom(), false);
    g.setGradientFill (cg); g.fillRect (head);
    g.setColour (t.sectionEdge); g.drawHorizontalLine (kHeaderH - 1, 0, head.getRight());
    drawScrew (g, { 12, head.getCentreY() }, t);
    const Colour ink = t.bevel ? t.panel.darker (0.25f).contrasting (0.9f) : t.text;
    g.setColour (ink);
    g.setFont (Font (FontOptions (18.0f, Font::bold)).withExtraKerningFactor (0.15f));
    g.drawText (String (FH_PRODUCT_NAME).toUpperCase(), 26, 0, 200, kHeaderH, Justification::centredLeft);
    g.setColour (t.accent); g.fillRect (26.0f, head.getBottom() - 9.0f, 30.0f, 3.0f);
    g.setColour (ink.withAlpha (0.6f));
    g.setFont (Font (FontOptions (11.0f, Font::bold)).withExtraKerningFactor (0.1f));
    g.drawText ("STRIP", 168, 0, 80, kHeaderH, Justification::centredLeft);

    drawSection (g, ed.outSection.toFloat(), t, "OUTPUT");
    g.setColour (t.textDim); g.setFont (Font (FontOptions (9.0f, Font::bold)));
    g.drawText ("IN", ed.inMeter.getBounds().withHeight (12).translated (0, -15).expanded (6, 0), Justification::centred);
    g.drawText ("OUT", ed.outMeter.getBounds().withHeight (12).translated (0, -15).expanded (6, 0), Justification::centred);

    g.setColour (t.scale); g.setFont (Font (FontOptions (9.0f)));
    for (float db : { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -40.0f, -60.0f })
    {
        const float y = (float) ed.fader.getY() + (float) ed.fader.getPositionOfValue (db);
        g.drawText (db > 0 ? "+" + String ((int) db) : String ((int) db), ed.scaleArea.getX() - 2, (int) y - 5, ed.scaleArea.getWidth(), 10,
                    Justification::centredRight);
        g.fillRect ((float) ed.fader.getX() + 8, y - 0.5f, 6.0f, 1.0f);
    }
    auto* prm = ed.proc.apvts.getParameter (cid (0, "fader"));
    g.setColour (t.text); g.setFont (Font (FontOptions (11.0f, Font::bold)));
    g.drawText (prm->getCurrentValueAsText(), ed.outSection.reduced (6).removeFromBottom (16), Justification::centred);
}

void FaderhouseStripEditor::paint (Graphics& g) { g.fillAll (lnf.theme().panel); }

void FaderhouseStripEditor::timerCallback()
{
    processing.refresh();
    proc.strip.compGrDb.store (0.0f);
    proc.strip.gateGrDb.store (0.0f);
    inMeter.push (proc.inPeakL.exchange (0.0f), proc.inPeakR.exchange (0.0f));
    outMeter.push (proc.outPeakL.exchange (0.0f), proc.outPeakR.exchange (0.0f));
    content.repaint (outSection.withTrimmedTop (outSection.getHeight() - 24));
}
