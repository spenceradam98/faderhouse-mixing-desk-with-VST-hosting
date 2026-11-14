#include "MasterSection.h"
#include "PluginProcessor.h"

namespace fh
{
using namespace juce;

MasterSection::MasterSection (FaderhouseProcessor& p, ChainUI& c) : proc (p), chains (c)
{
    auto& s = proc.apvts;
    for (int a = 0; a < kNumAux; ++a)
    {
        auto& r = returns[(size_t) a];
        r.fx.onClick = [this, a] { chains.showMenu (*proc.aux[(size_t) a].chain, &returns[(size_t) a].fx); };
        r.fx.setTooltip ("Effects on FX return " + auxName (a) + " (e.g. a reverb or delay)");
        addAndMakeVisible (r.fx);
        r.level = std::make_unique<Knob> (Cap::Send, "RETURN");
        r.pan = std::make_unique<Knob> (Cap::Pan, "BAL", true);
        addAndMakeVisible (*r.level); addAndMakeVisible (*r.pan); addAndMakeVisible (r.mute); addAndMakeVisible (r.meter);
        sAtt.push_back (attachKnob (s, aid (a, "level"), *r.level));
        sAtt.push_back (attachKnob (s, aid (a, "pan"), *r.pan));
        bAtt.push_back (attachToggle (s, aid (a, "mute"), r.mute));
    }
    for (auto* k : { &bcThr, &bcRatio, &bcAtk, &bcRel, &bcMakeup, &bcMix }) addAndMakeVisible (k);
    sAtt.push_back (attachKnob (s, "bcThr", bcThr));
    sAtt.push_back (attachKnob (s, "bcRatio", bcRatio));
    sAtt.push_back (attachKnob (s, "bcAtk", bcAtk));
    sAtt.push_back (attachKnob (s, "bcRel", bcRel));
    sAtt.push_back (attachKnob (s, "bcMakeup", bcMakeup));
    sAtt.push_back (attachKnob (s, "bcMix", bcMix));
    addAndMakeVisible (bcOn);
    bAtt.push_back (attachToggle (s, "bcOn", bcOn));
    addAndMakeVisible (bcGr);

    masterIns.onClick = [this] { chains.showMenu (*proc.masterInserts, &masterIns); };
    masterIns.setTooltip ("Effects on the mix bus, before the bus compressor");
    addAndMakeVisible (masterIns);
    fader.setPopupDisplayEnabled (true, true, nullptr, 1200);
    addAndMakeVisible (fader);
    sAtt.push_back (attachKnob (s, "master", fader));
    addAndMakeVisible (meter);
}

void MasterSection::refresh()
{
    for (int a = 0; a < kNumAux; ++a)
    {
        auto& r = returns[(size_t) a];
        auto& x = proc.aux[(size_t) a];
        r.meter.push (x.peakL.exchange (0.0f), x.peakR.exchange (0.0f));
        const auto d = x.chain->describe();
        const String txt = d.isEmpty() ? String ("+ FX") : d;
        if (r.fx.getButtonText() != txt) r.fx.setButtonText (txt);
        r.fx.setToggleState (x.chain->isActive(), dontSendNotification);
    }
    lastGr = proc.busComp.meterGrDb.exchange (0.0f);
    bcGr.push (lastGr, 0);
    const auto d = proc.masterInserts->describe();
    const String mt = d.isEmpty() ? String ("MASTER INSERTS") : "INS: " + d;
    if (masterIns.getButtonText() != mt) masterIns.setButtonText (mt);
    masterIns.setToggleState (proc.masterInserts->isActive(), dontSendNotification);
    meter.push (proc.masterPeakL.exchange (0.0f), proc.masterPeakR.exchange (0.0f));
    if (proc.pdcSamples.load() != lastPdc) { lastPdc = proc.pdcSamples.load(); repaint (0, 0, getWidth(), 34); }
}

void MasterSection::resized()
{
    auto r = getLocalBounds().reduced (6, 4);
    r.removeFromTop (26);
    secReturns = r.removeFromTop (4 * 54 + 16); r.removeFromTop (6);
    {
        auto a = secReturns.reduced (6).withTrimmedTop (12);
        for (auto& ret : returns)
        {
            auto row = a.removeFromTop (54);
            ret.meter.setBounds (row.removeFromRight (8).reduced (0, 4));
            row.removeFromRight (4);
            ret.pan->setBounds (row.removeFromRight (44));
            ret.level->setBounds (row.removeFromRight (44));
            row.removeFromRight (4);
            ret.labelArea = row.removeFromTop (16);
            ret.mute.setBounds (ret.labelArea.removeFromRight (22).reduced (0, 1));
            ret.fx.setBounds (row.removeFromTop (22).translated (0, 2));
        }
    }
    secComp = r.removeFromTop (172); r.removeFromTop (6);
    {
        auto a = secComp.reduced (6).withTrimmedTop (12);
        auto top = a.removeFromTop (20);
        bcOn.setBounds (top.removeFromLeft (90));
        grArea = top.withTrimmedLeft (8).reduced (0, 4);
        a.removeFromTop (6);
        auto row1 = a.removeFromTop (60), row2 = a.removeFromTop (60);
        const int w = row1.getWidth() / 3;
        for (auto* k : { &bcThr, &bcRatio, &bcAtk }) k->setBounds (row1.removeFromLeft (w).withSizeKeepingCentre (54, 58));
        for (auto* k : { &bcRel, &bcMakeup, &bcMix }) k->setBounds (row2.removeFromLeft (w).withSizeKeepingCentre (54, 58));
    }
    bcGr.setBounds (0, 0, 0, 0); // drawn as a horizontal bar in paint()
    secMaster = r;
    {
        auto a = secMaster.reduced (6).withTrimmedTop (12);
        masterIns.setBounds (a.removeFromTop (22));
        a.removeFromTop (8);
        a.removeFromBottom (18);
        meter.setBounds (a.removeFromRight (22).reduced (0, 18));
        a.removeFromRight (6);
        auto fa = a.withSizeKeepingCentre (80, a.getHeight());
        scaleArea = fa.removeFromLeft (22);
        fader.setBounds (fa);
    }
}

void MasterSection::paint (Graphics& g)
{
    const auto& t = ConsoleLookAndFeel::themeOf (*this);
    g.setColour (t.panel.darker (0.08f)); g.fillRect (getLocalBounds());
    auto head = getLocalBounds().reduced (6, 4).removeFromTop (24).toFloat();
    g.setColour (t.screen); g.fillRoundedRectangle (head, 3.0f);
    g.setColour (t.curve); g.setFont (Font (FontOptions (14.0f, Font::bold)));
    g.drawText ("MASTER", head.reduced (8, 0), Justification::centredLeft);
    {
        // Plugin delay compensation currently applied
        const int pdc = proc.pdcSamples.load();
        g.setFont (Font (FontOptions (10.0f, Font::bold)));
        g.setColour (pdc > 0 ? t.curve : t.curve.withAlpha (0.45f));
        g.drawText ("PDC " + String (1000.0 * pdc / jmax (1.0, proc.currentSampleRate.load()), 1) + " ms",
                    head.reduced (8, 0), Justification::centredRight);
    }

    drawSection (g, secReturns.toFloat(), t, "FX RETURNS");
    drawSection (g, secComp.toFloat(), t, "BUS COMPRESSOR");
    drawSection (g, secMaster.toFloat(), t, "MIX BUS");

    g.setFont (Font (FontOptions (10.0f, Font::bold)));
    for (int a = 0; a < kNumAux; ++a)
    {
        g.setColour (t.text);
        g.drawText ("FX " + auxName (a), returns[(size_t) a].labelArea, Justification::centredLeft);
    }

    // Bus compressor gain reduction: horizontal LED bar, 0..-20 dB
    {
        auto gr = grArea.toFloat();
        g.setColour (t.meterOff); g.fillRoundedRectangle (gr, 2.0f);
        const int segs = 10;
        const float sw = gr.getWidth() / (float) segs;
        const float amount = jlimit (0.0f, 1.0f, -lastGr / 20.0f);
        for (int i = 0; i < segs; ++i)
            if ((float) i / (float) segs < amount)
            {
                g.setColour (t.ledOn);
                g.fillRect (gr.getX() + (float) i * sw + 1, gr.getY() + 1, sw - 2, gr.getHeight() - 2);
            }
        g.setColour (t.textDim); g.setFont (Font (FontOptions (8.5f, Font::bold)));
        g.drawText ("GR", grArea.translated (0, -11).withHeight (10), Justification::centredLeft);
    }

    g.setColour (t.scale); g.setFont (Font (FontOptions (9.0f)));
    for (float db : { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -40.0f, -60.0f })
    {
        const float y = (float) fader.getY() + (float) fader.getPositionOfValue (db);
        g.drawText (db > 0 ? "+" + String ((int) db) : String ((int) db), scaleArea.getX() - 4, (int) y - 5, scaleArea.getWidth(), 10,
                    Justification::centredRight);
        g.fillRect ((float) fader.getX() + 10, y - 0.5f, 8.0f, 1.0f);
    }
    auto* prm = proc.apvts.getParameter ("master");
    g.setColour (t.text); g.setFont (Font (FontOptions (11.0f, Font::bold)));
    g.drawText (prm->getCurrentValueAsText(), secMaster.reduced (6).removeFromBottom (16), Justification::centred);
}
} // namespace fh
