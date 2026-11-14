#include "FocusPanel.h"
#include "PluginProcessor.h"

namespace fh
{
using namespace juce;

//==============================================================================
EqCurve::EqCurve (FaderhouseProcessor& p, int c) : proc (p), ch (c) { setInterceptsMouseClicks (false, false); }

void EqCurve::paint (Graphics& g)
{
    const auto& t = ConsoleLookAndFeel::themeOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (t.screen); g.fillRoundedRectangle (r, 3.0f);
    r = r.reduced (4, 4);

    const double sr = 48000.0;
    auto& strip = proc.channels[(size_t) ch].strip;
    EqDesign eq;
    const bool eqOn = strip.eqEnabled();
    eq.design (strip.readEq(), sr);
    Biquad hp2, lp2; OnePoleHP hp1;
    const float hpfF = strip.p.hpf->load(), lpfF = strip.p.lpf->load();
    const bool hpOn = hpfF > 16.5f, lpOn = lpfF < 21900.0f;
    if (hpOn) { hp2.highPass (sr, hpfF, 0.707); hp1.set (sr, hpfF); }
    if (lpOn) lp2.lowPass (sr, lpfF, 0.707);

    auto xFor = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
    auto yFor = [&] (double db) { return r.getCentreY() - (float) (db / 18.0) * r.getHeight() * 0.5f; };

    g.setColour (t.curve.withAlpha (0.12f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        g.drawVerticalLine ((int) xFor (f), r.getY(), r.getBottom());
    for (double db : { -12.0, -6.0, 6.0, 12.0 })
        g.drawHorizontalLine ((int) yFor (db), r.getX(), r.getRight());
    g.setColour (t.curve.withAlpha (0.3f));
    g.drawHorizontalLine ((int) yFor (0), r.getX(), r.getRight());

    Path p;
    for (int i = 0; i <= 160; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 160.0);
        double m = eqOn ? eq.magnitude (f, sr) : 1.0;
        if (hpOn) m *= hp2.magnitude (f, sr) * hp1.magnitude (f, sr);
        if (lpOn) m *= lp2.magnitude (f, sr);
        const double db = jlimit (-30.0, 30.0, 20.0 * std::log10 (jmax (1.0e-6, m)));
        const float x = xFor (f), y = jlimit (r.getY(), r.getBottom(), yFor (db));
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    Path fill (p);
    fill.lineTo (r.getRight(), yFor (0)); fill.lineTo (r.getX(), yFor (0)); fill.closeSubPath();
    g.setColour (t.curve.withAlpha (0.12f)); g.fillPath (fill);
    g.setColour (eqOn ? t.curve : t.curve.withAlpha (0.4f));
    g.strokePath (p, PathStrokeType (1.8f));

    g.setColour (t.curve.withAlpha (0.5f)); g.setFont (Font (FontOptions (9.0f)));
    g.drawText ("100", (int) xFor (100) - 15, (int) r.getBottom() - 11, 30, 10, Justification::centred);
    g.drawText ("1k", (int) xFor (1000) - 15, (int) r.getBottom() - 11, 30, 10, Justification::centred);
    g.drawText ("10k", (int) xFor (10000) - 15, (int) r.getBottom() - 11, 30, 10, Justification::centred);
}

//==============================================================================
FocusPanel::FocusPanel (FaderhouseProcessor& p, ChainUI& c, int channel)
    : proc (p), chains (c), ch (channel), curve (p, channel)
{
    auto& s = proc.apvts;
    source.addItemList (sourceNames(), 1);
    srcAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, cid (ch, "src"), source);
    source.setTooltip ("Where this channel's signal comes from");
    addAndMakeVisible (source);
    midiCh.addItemList ({ "MIDI Omni", "MIDI 1", "MIDI 2", "MIDI 3", "MIDI 4", "MIDI 5", "MIDI 6", "MIDI 7", "MIDI 8", "MIDI 9",
                          "MIDI 10", "MIDI 11", "MIDI 12", "MIDI 13", "MIDI 14", "MIDI 15", "MIDI 16" }, 1);
    midiAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, cid (ch, "midi"), midiCh);
    midiCh.setTooltip ("MIDI channel the hosted instrument listens to");
    addAndMakeVisible (midiCh);

    instrumentBtn.onClick = [this]
    {
        auto& inst = *proc.channels[(size_t) ch].instrument;
        auto setSrc = [this]
        {
            auto* prm = proc.apvts.getParameter (cid (ch, "src"));
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) kSourceInstrument));
        };
        if (inst.size() == 0) chains.showAddMenu (inst, &instrumentBtn, setSrc);
        else chains.showMenu (inst, &instrumentBtn, setSrc);
    };
    addAndMakeVisible (instrumentBtn);

    addInsertBtn.onClick = [this] { chains.showAddMenu (*proc.channels[(size_t) ch].inserts, &addInsertBtn); };
    addInsertBtn.setTooltip ("Add a VST3 effect to this channel's inserts");
    addAndMakeVisible (addInsertBtn);

    trim = &knob (Cap::Input, "TRIM", "trim", true);
    drive = &knob (Cap::Input, "DRIVE", "drive");
    phase = &toggle (String::fromUTF8 ("\xc3\x98"), "phase");
    phase->setTooltip ("Phase (polarity) invert");
    hpf = &knob (Cap::Filter, "HPF", "hpf");
    lpf = &knob (Cap::Filter, "LPF", "lpf");
    insPre = &toggle ("INS PRE", "insPre");
    insPre->setTooltip ("On: inserts run right after the input stage. Off: after dynamics and EQ");

    cThr = &knob (Cap::Dyn, "THRESH", "cThr");
    cRatio = &knob (Cap::Dyn, "RATIO", "cRatio");
    cRel = &knob (Cap::Dyn, "RELEASE", "cRel");
    cFast = &toggle ("F.ATK", "cFast");
    cOn = &toggle ("COMP", "cOn");
    gThr = &knob (Cap::Gate, "THRESH", "gThr");
    gRange = &knob (Cap::Gate, "RANGE", "gRange");
    gRel = &knob (Cap::Gate, "RELEASE", "gRel");
    gExp = &toggle ("EXP", "gExp");
    gExp->setTooltip ("Expander instead of gate");
    gOn = &toggle ("GATE", "gOn");
    dynPre = &toggle ("DYN PRE EQ", "dynPre");
    dynPre->setTooltip ("Run dynamics before the EQ instead of after it");
    addAndMakeVisible (compGr);
    addAndMakeVisible (gateGr);

    eqOn = &toggle ("EQ IN", "eqOn");
    eqBlk = &toggle ("BLACK", "eqBlk");
    eqBlk->setTooltip ("Black mode: proportional-Q bells and tighter shelves. Off: brown mode, constant Q");
    hfG = &knob (Cap::HF, "GAIN", "hfG", true);  hfF = &knob (Cap::HF, "kHz", "hfF");  hfBell = &toggle ("BELL", "hfBell");
    hmG = &knob (Cap::HMF, "GAIN", "hmG", true); hmF = &knob (Cap::HMF, "kHz", "hmF"); hmQ = &knob (Cap::HMF, "Q", "hmQ");
    lmG = &knob (Cap::LMF, "GAIN", "lmG", true); lmF = &knob (Cap::LMF, "kHz", "lmF"); lmQ = &knob (Cap::LMF, "Q", "lmQ");
    lfG = &knob (Cap::LF, "GAIN", "lfG", true);  lfF = &knob (Cap::LF, "Hz", "lfF");   lfBell = &toggle ("BELL", "lfBell");
    addAndMakeVisible (curve);

    for (auto* c2 : proc.allChains()) c2->addChangeListener (this);
    rebuildSlots();
}

FocusPanel::~FocusPanel()
{
    for (auto* c2 : proc.allChains()) c2->removeChangeListener (this);
}

Knob& FocusPanel::knob (Cap cap, const String& caption, const char* id, bool bipolar)
{
    auto* k = knobs.add (new Knob (cap, caption, bipolar));
    if (auto* prm = proc.apvts.getParameter (cid (ch, id))) k->setTooltip (prm->getName (64));
    sAtt.push_back (attachKnob (proc.apvts, cid (ch, id), *k));
    addAndMakeVisible (k);
    return *k;
}

ToggleBtn& FocusPanel::toggle (const String& text, const char* id, ToggleBtn::Role role)
{
    auto* b = toggles.add (new ToggleBtn (text, role));
    bAtt.push_back (attachToggle (proc.apvts, cid (ch, id), *b));
    addAndMakeVisible (b);
    return *b;
}

void FocusPanel::changeListenerCallback (ChangeBroadcaster*) { rebuildSlots(); }

void FocusPanel::rebuildSlots()
{
    auto& chain = *proc.channels[(size_t) ch].inserts;
    slotButtons.clear();
    for (int i = 0; i < chain.size(); ++i)
    {
        auto* pl = chain.getPlugin (i);
        auto* b = slotButtons.add (new TextButton (String (i + 1) + "  " + (pl ? pl->getName() : String ("?"))));
        b->setAlpha (chain.isBypassed (i) ? 0.45f : 1.0f);
        b->setTooltip ("Click: open editor. Right-click: bypass, move, remove");
        b->onClick = [this, i] { if (auto* q = proc.channels[(size_t) ch].inserts->getPlugin (i)) chains.openEditor (*q); };
        b->addMouseListener (this, false);
        addAndMakeVisible (b);
    }
    addInsertBtn.setEnabled (! chain.isFull());
    auto& inst = *proc.channels[(size_t) ch].instrument;
    instrumentBtn.setButtonText (inst.size() > 0 ? "INSTRUMENT: " + inst.getPlugin (0)->getName() : String ("LOAD INSTRUMENT..."));
    instrumentBtn.setToggleState (inst.size() > 0, dontSendNotification);
    resized();
}

void FocusPanel::mouseDown (const MouseEvent& e)
{
    const int idx = slotButtons.indexOf (dynamic_cast<TextButton*> (e.eventComponent));
    if (idx >= 0 && e.mods.isPopupMenu())
        chains.showSlotMenu (*proc.channels[(size_t) ch].inserts, idx, slotButtons[idx]);
}

void FocusPanel::refresh()
{
    compGr.push (proc.channels[(size_t) ch].strip.compGrDb.load(), 0);
    gateGr.push (proc.channels[(size_t) ch].strip.gateGrDb.load(), 0);
    curve.repaint();
}

void FocusPanel::resized()
{
    auto r = getLocalBounds().reduced (6, 4);
    r.removeFromTop (26); // header

    auto place = [] (Component* c, Rectangle<int> a) { c->setBounds (a); };
    const int kw = 54, kh = 56;

    // Routing
    secRouting = r.removeFromTop (66); r.removeFromTop (6);
    {
        auto a = secRouting.reduced (6).withTrimmedTop (12);
        auto row = a.removeFromTop (22);
        source.setBounds (row.removeFromLeft (row.getWidth() * 3 / 5)); row.removeFromLeft (6);
        midiCh.setBounds (row);
        a.removeFromTop (4);
        instrumentBtn.setBounds (a.removeFromTop (22));
    }

    // Inserts
    secInserts = r.removeFromTop (100); r.removeFromTop (6);
    {
        auto a = secInserts.reduced (6).withTrimmedTop (12);
        auto bottom = a.removeFromBottom (20);
        addInsertBtn.setBounds (bottom.removeFromLeft (80));
        bottom.removeFromLeft (6);
        insPre->setBounds (bottom.removeFromLeft (80));
        a.removeFromBottom (4);
        const int cw = (a.getWidth() - 6) / 2, rh = (a.getHeight() - 4) / 3;
        for (int i = 0; i < slotButtons.size(); ++i)
            slotButtons[i]->setBounds (a.getX() + (i / 3) * (cw + 6), a.getY() + (i % 3) * (rh + 2), cw, rh);
    }

    // Input + filters
    secInput = r.removeFromTop (84); r.removeFromTop (6);
    {
        auto a = secInput.reduced (6).withTrimmedTop (12);
        place (trim, a.removeFromLeft (kw).withHeight (kh));
        place (drive, a.removeFromLeft (kw).withHeight (kh));
        place (phase, a.removeFromLeft (34).withSizeKeepingCentre (30, 22));
        a.removeFromLeft (20);
        place (hpf, a.removeFromLeft (kw).withHeight (kh));
        place (lpf, a.removeFromLeft (kw).withHeight (kh));
    }

    // Dynamics
    secDyn = r.removeFromTop (150); r.removeFromTop (6);
    {
        auto a = secDyn.reduced (6).withTrimmedTop (12);
        auto meters = a.removeFromRight (64).withTrimmedBottom (14);
        compGr.setBounds (meters.removeFromLeft (32).withSizeKeepingCentre (12, meters.getHeight()));
        gateGr.setBounds (meters.withSizeKeepingCentre (12, meters.getHeight()));
        auto row1 = a.removeFromTop (kh);
        place (cThr, row1.removeFromLeft (kw)); place (cRatio, row1.removeFromLeft (kw)); place (cRel, row1.removeFromLeft (kw));
        auto bc = row1.removeFromLeft (70).reduced (6, 4);
        cOn->setBounds (bc.removeFromTop (22)); bc.removeFromTop (4); cFast->setBounds (bc.removeFromTop (22));
        a.removeFromTop (2);
        auto row2 = a.removeFromTop (kh);
        place (gThr, row2.removeFromLeft (kw)); place (gRange, row2.removeFromLeft (kw)); place (gRel, row2.removeFromLeft (kw));
        auto bg = row2.removeFromLeft (70).reduced (6, 4);
        gOn->setBounds (bg.removeFromTop (22)); bg.removeFromTop (4); gExp->setBounds (bg.removeFromTop (22));
        a.removeFromTop (2);
        dynPre->setBounds (a.removeFromTop (20).removeFromLeft (162));
    }

    // EQ
    secEq = r;
    {
        auto a = secEq.reduced (6).withTrimmedTop (12);
        auto top = a.removeFromTop (20);
        eqOn->setBounds (top.removeFromLeft (64)); top.removeFromLeft (6);
        eqBlk->setBounds (top.removeFromLeft (64));
        a.removeFromTop (4);
        curve.setBounds (a.removeFromTop (jmin (120, a.getHeight() - 210)));
        a.removeFromTop (16);
        const int cw = a.getWidth() / 4;
        Component* cols[4][3] { { hfG, hfF, hfBell }, { hmG, hmF, hmQ }, { lmG, lmF, lmQ }, { lfG, lfF, lfBell } };
        for (int c = 0; c < 4; ++c)
        {
            auto col = a.withX (a.getX() + c * cw).withWidth (cw);
            for (int k = 0; k < 3; ++k)
            {
                auto cell = col.removeFromTop (k < 2 ? kh + 6 : kh);
                if (dynamic_cast<ToggleBtn*> (cols[c][k]) != nullptr) cell = cell.withSizeKeepingCentre (48, 22);
                else cell = cell.withSizeKeepingCentre (kw, kh);
                cols[c][k]->setBounds (cell);
            }
        }
    }
}

void FocusPanel::paint (Graphics& g)
{
    const auto& t = ConsoleLookAndFeel::themeOf (*this);
    g.setColour (t.panel.darker (0.08f)); g.fillRect (getLocalBounds());
    auto head = getLocalBounds().reduced (6, 4).removeFromTop (24).toFloat();
    g.setColour (t.screen); g.fillRoundedRectangle (head, 3.0f);
    g.setColour (t.curve); g.setFont (Font (FontOptions (14.0f, Font::bold)));
    g.drawText ("CHANNEL " + String (ch + 1) + "   " + (proc.getChannelName (ch) == String (ch + 1) ? String() : proc.getChannelName (ch)),
                head.reduced (8, 0), Justification::centredLeft);

    drawSection (g, secRouting.toFloat(), t, "ROUTING");
    drawSection (g, secInserts.toFloat(), t, "INSERTS (VST3)");
    drawSection (g, secInput.toFloat(), t, "INPUT                                    FILTERS");
    drawSection (g, secDyn.toFloat(), t, "DYNAMICS");
    drawSection (g, secEq.toFloat(), t, "EQUALISER");

    g.setColour (t.textDim); g.setFont (Font (FontOptions (9.0f, Font::bold)));
    g.drawText ("COMP", compGr.getBounds().withSizeKeepingCentre (40, 10).withY (compGr.getBottom() + 2), Justification::centred);
    g.drawText ("GATE", gateGr.getBounds().withSizeKeepingCentre (40, 10).withY (gateGr.getBottom() + 2), Justification::centred);
    g.drawText ("GR", compGr.getBounds().withSizeKeepingCentre (70, 10).withX (compGr.getX()).withY (compGr.getY() - 12), Justification::centredLeft);
    const char* names[] { "HF", "HMF", "LMF", "LF" };
    Colour caps[] { t.capHF, t.capHMF, t.capLMF, t.capLF };
    for (int c = 0; c < 4; ++c)
    {
        auto x = (hfG->getX() + c * (hmG->getX() - hfG->getX()));
        auto lab = Rectangle<int> (x - 6, hfG->getY() - 15, hfG->getWidth() + 12, 12);
        g.setColour (caps[c]); g.fillRoundedRectangle (lab.toFloat().reduced (8, 1), 2.0f);
        g.setColour (caps[c].contrasting (0.9f)); g.drawText (names[c], lab, Justification::centred);
    }
}
} // namespace fh
