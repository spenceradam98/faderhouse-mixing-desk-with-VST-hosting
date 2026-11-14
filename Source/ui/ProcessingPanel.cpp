#include "ProcessingPanel.h"
#include "engine/Params.h"

namespace fh
{
using namespace juce;

ProcessingPanel::ProcessingPanel (AudioProcessorValueTreeState& s, ChannelStrip& st, int channel)
    : state (s), strip (st), ch (channel), curve (st)
{
    trim = &knob (Cap::Input, "TRIM", "trim", true);
    drive = &knob (Cap::Input, "DRIVE", "drive");
    phase = &toggle (String::fromUTF8 ("\xc3\x98"), "phase");
    phase->setTooltip ("Phase (polarity) invert");
    hpf = &knob (Cap::Filter, "HPF", "hpf");
    lpf = &knob (Cap::Filter, "LPF", "lpf");

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
}

Knob& ProcessingPanel::knob (Cap cap, const String& caption, const char* id, bool bipolar)
{
    auto* k = knobs.add (new Knob (cap, caption, bipolar));
    if (auto* prm = state.getParameter (cid (ch, id))) k->setTooltip (prm->getName (64));
    sAtt.push_back (attachKnob (state, cid (ch, id), *k));
    addAndMakeVisible (k);
    return *k;
}

ToggleBtn& ProcessingPanel::toggle (const String& text, const char* id)
{
    auto* b = toggles.add (new ToggleBtn (text));
    bAtt.push_back (attachToggle (state, cid (ch, id), *b));
    addAndMakeVisible (b);
    return *b;
}

void ProcessingPanel::refresh()
{
    compGr.push (strip.compGrDb.load(), 0);
    gateGr.push (strip.gateGrDb.load(), 0);
    curve.repaint();
}

void ProcessingPanel::resized()
{
    auto r = getLocalBounds();
    auto place = [] (Component* c, Rectangle<int> a) { c->setBounds (a); };
    const int kw = 54, kh = 56;

    secInput = r.removeFromTop (kInputH); r.removeFromTop (kGap);
    {
        auto a = secInput.reduced (6).withTrimmedTop (12);
        place (trim, a.removeFromLeft (kw).withHeight (kh));
        place (drive, a.removeFromLeft (kw).withHeight (kh));
        place (phase, a.removeFromLeft (34).withSizeKeepingCentre (30, 22));
        a.removeFromLeft (20);
        place (hpf, a.removeFromLeft (kw).withHeight (kh));
        place (lpf, a.removeFromLeft (kw).withHeight (kh));
    }

    secDyn = r.removeFromTop (kDynH); r.removeFromTop (kGap);
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

    secEq = r;
    {
        auto a = secEq.reduced (6).withTrimmedTop (12);
        auto top = a.removeFromTop (20);
        eqOn->setBounds (top.removeFromLeft (64)); top.removeFromLeft (6);
        eqBlk->setBounds (top.removeFromLeft (64));
        a.removeFromTop (4);
        curve.setBounds (a.removeFromTop (jlimit (40, 120, a.getHeight() - 210)));
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

void ProcessingPanel::paint (Graphics& g)
{
    const auto& t = ConsoleLookAndFeel::themeOf (*this);
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
