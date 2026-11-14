#include "StripView.h"
#include "PluginProcessor.h"

namespace fh
{
using namespace juce;

StripView::StripView (FaderhouseProcessor& p, ChainUI& c, int channel, std::function<void (int)> sel_)
    : proc (p), chains (c), ch (channel), onSelect (std::move (sel_))
{
    auto& s = proc.apvts;
    sourceBtn.setTooltip ("Input source: an interface input, a stereo pair, or a hosted instrument");
    sourceBtn.onClick = [this] { showSourceMenu(); };
    addAndMakeVisible (sourceBtn);

    insertBtn.setTooltip ("Insert effects on this channel (VST3)");
    insertBtn.onClick = [this] { chains.showMenu (*proc.channels[(size_t) ch].inserts, &insertBtn); };
    addAndMakeVisible (insertBtn);

    for (int a = 0; a < kNumAux; ++a)
    {
        sends[(size_t) a] = std::make_unique<Knob> (Cap::Send, auxName (a));
        sends[(size_t) a]->setTooltip ("Send to FX " + auxName (a) + " (right-click: pre/post fader)");
        sends[(size_t) a]->addMouseListener (this, false);
        addAndMakeVisible (*sends[(size_t) a]);
        sliderAtt.push_back (attachKnob (s, cid (ch, ("s" + String (a + 1)).toRawUTF8()), *sends[(size_t) a]));
    }
    addAndMakeVisible (pan);
    sliderAtt.push_back (attachKnob (s, cid (ch, "pan"), pan));

    for (auto* b : { &solo, &mute }) addAndMakeVisible (b);
    buttonAtt.push_back (attachToggle (s, cid (ch, "solo"), solo));
    buttonAtt.push_back (attachToggle (s, cid (ch, "mute"), mute));
    sel.setClickingTogglesState (false);
    sel.onClick = [this] { onSelect (ch); };
    sel.setTooltip ("Show this channel's EQ, dynamics and routing in the channel panel");
    addAndMakeVisible (sel);

    fader.setPopupDisplayEnabled (true, true, nullptr, 1200);
    fader.setTooltip ("Channel fader (double-click: 0 dB)");
    addAndMakeVisible (fader);
    sliderAtt.push_back (attachKnob (s, cid (ch, "fader"), fader));
    addAndMakeVisible (meter);
}

StripView::~StripView() = default;

void StripView::refresh (bool selected)
{
    auto& c = proc.channels[(size_t) ch];
    meter.push (c.peakL.exchange (0.0f), c.peakR.exchange (0.0f));
    grDb = jmin (c.strip.compGrDb.exchange (0.0f), c.strip.gateGrDb.exchange (0.0f));

    const int src = (int) c.src->load();
    String srcText = shortSourceName (src);
    if (src == kSourceInstrument)
        srcText = c.instrument->size() > 0 ? c.instrument->getPlugin (0)->getName() : String ("INST?");
    if (sourceBtn.getButtonText() != srcText) sourceBtn.setButtonText (srcText);

    const int n = c.inserts->size();
    const String ins = n == 0 ? String ("INS") : "INS " + String (n);
    if (insertBtn.getButtonText() != ins) insertBtn.setButtonText (ins);
    insertBtn.setToggleState (c.inserts->isActive(), dontSendNotification);
    insertBtn.setTooltip (n == 0 ? String ("Insert effects (none)") : c.inserts->describe());

    for (int a = 0; a < kNumAux; ++a)
    {
        const bool pre = c.sendPre[(size_t) a]->load() > 0.5f;
        const String cap = auxName (a) + (pre ? " PRE" : "");
        if (sends[(size_t) a]->caption != cap) { sends[(size_t) a]->caption = cap; sends[(size_t) a]->repaint(); }
    }

    const bool dyn = c.strip.p.cOn->load() > 0.5f || c.strip.p.gOn->load() > 0.5f;
    const auto eqs = c.strip.readEq();
    const bool eq = c.strip.eqEnabled() && (eqs.hfG != 0 || eqs.hmG != 0 || eqs.lmG != 0 || eqs.lfG != 0);
    if (dyn != dynLit || eq != eqLit || selected != isSelected)
    {
        dynLit = dyn; eqLit = eq; isSelected = selected;
        sel.setToggleState (selected, dontSendNotification);
        repaint();
    }
    else
        repaint (ledArea);
    repaint (scribble);
}

void StripView::resized()
{
    auto r = getLocalBounds().reduced (3, 4);
    sel.setBounds (r.removeFromTop (18)); r.removeFromTop (3);
    sourceBtn.setBounds (r.removeFromTop (20)); r.removeFromTop (3);
    insertBtn.setBounds (r.removeFromTop (20)); r.removeFromTop (3);
    ledArea = r.removeFromTop (12); r.removeFromTop (4);
    for (auto& k : sends) { k->setBounds (r.removeFromTop (44).withSizeKeepingCentre (44, 44)); }
    r.removeFromTop (2);
    pan.setBounds (r.removeFromTop (46).withSizeKeepingCentre (44, 46));
    r.removeFromTop (4);
    auto sm = r.removeFromTop (20);
    solo.setBounds (sm.removeFromLeft (sm.getWidth() / 2 - 1)); sm.removeFromLeft (2); mute.setBounds (sm);
    r.removeFromTop (6);
    scribble = r.removeFromBottom (22); r.removeFromBottom (4);
    auto fa = r;
    meter.setBounds (fa.removeFromRight (9).reduced (0, 18));
    scaleArea = fa.removeFromLeft (14);
    fader.setBounds (fa);
}

void StripView::paint (Graphics& g)
{
    const auto& t = ConsoleLookAndFeel::themeOf (*this);
    auto r = getLocalBounds().toFloat().reduced (1.0f, 0);
    if (t.bevel)
    {
        ColourGradient cg (t.panel.brighter (0.05f), r.getX(), 0, t.panel.darker (0.05f), r.getRight(), 0, false);
        g.setGradientFill (cg);
    }
    else g.setColour (t.panel);
    g.fillRect (r);
    g.setColour (t.sectionEdge.withAlpha (0.6f));
    g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());
    if (isSelected)
    {
        g.setColour (t.select.withAlpha (0.12f)); g.fillRect (r);
    }

    // DYN / EQ status LEDs
    auto led = [&] (Rectangle<int> a, const String& txt, bool on)
    {
        auto dot = a.removeFromLeft (8).toFloat().withSizeKeepingCentre (6, 6);
        g.setColour (on ? t.ledOn : t.meterOff); g.fillEllipse (dot);
        if (on) { g.setColour (t.ledOn.withAlpha (0.3f)); g.fillEllipse (dot.expanded (2)); }
        g.setColour (t.textDim); g.setFont (Font (FontOptions (9.0f, Font::bold)));
        g.drawText (txt, a.withTrimmedLeft (1), Justification::centredLeft);
    };
    auto la = ledArea;
    led (la.removeFromLeft (la.getWidth() / 2), "DYN", dynLit || grDb < -0.5f);
    led (la, "EQ", eqLit);

    // Fader scale
    g.setColour (t.scale); g.setFont (Font (FontOptions (8.5f, Font::plain)));
    auto range = fader.getRange();
    for (float db : { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -40.0f, -60.0f })
    {
        if (db < range.getStart()) continue;
        const float y = (float) fader.getY() + (float) fader.getPositionOfValue (db);
        g.drawText (db > 0 ? "+" + String ((int) db) : String ((int) db), scaleArea.getX() - 2, (int) y - 5, scaleArea.getWidth() + 2, 10,
                    Justification::centredRight);
        g.fillRect ((float) fader.getX() + 4, y - 0.5f, 5.0f, 1.0f);
    }

    // Scribble strip
    auto sc = scribble.toFloat();
    g.setColour (t.screen); g.fillRoundedRectangle (sc, 2.5f);
    g.setColour (isSelected ? t.select : t.sectionEdge); g.drawRoundedRectangle (sc, 2.5f, 1.0f);
    g.setColour (t.curve); g.setFont (Font (FontOptions (11.0f, Font::bold)));
    g.drawFittedText (proc.getChannelName (ch), scribble.reduced (2, 0), Justification::centred, 1, 0.6f);
}

void StripView::mouseDown (const MouseEvent& e)
{
    for (int a = 0; a < kNumAux; ++a)
        if (e.eventComponent == sends[(size_t) a].get() && e.mods.isPopupMenu())
        {
            auto* prm = proc.apvts.getParameter (cid (ch, ("s" + String (a + 1) + "Pre").toRawUTF8()));
            const bool pre = prm->getValue() > 0.5f;
            PopupMenu m;
            m.addItem ("Post fader", true, ! pre, [prm] { prm->setValueNotifyingHost (0.0f); });
            m.addItem ("Pre fader", true, pre, [prm] { prm->setValueNotifyingHost (1.0f); });
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (sends[(size_t) a].get()));
            return;
        }

    if (e.eventComponent == this && scribble.contains (e.getPosition()))
    {
        if (e.getNumberOfClicks() > 1) editName();
        else onSelect (ch);
    }
}

void StripView::editName()
{
    nameEditor = std::make_unique<Label> (String(), proc.getChannelName (ch));
    nameEditor->setEditable (true, true, false);
    nameEditor->setJustificationType (Justification::centred);
    nameEditor->setBounds (scribble);
    addAndMakeVisible (*nameEditor);
    nameEditor->onEditorHide = [this]
    {
        proc.setChannelName (ch, nameEditor->getText());
        MessageManager::callAsync ([sp = SafePointer<StripView> (this)] { if (sp) { sp->nameEditor.reset(); sp->repaint(); } });
    };
    nameEditor->showEditor();
}

void StripView::showSourceMenu()
{
    auto* prm = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (cid (ch, "src")));
    const int cur = prm->getIndex();
    auto set = [prm] (int idx) { prm->setValueNotifyingHost (prm->convertTo0to1 ((float) idx)); };

    PopupMenu m, mono, stereo;
    for (int i = 0; i < kNumInputs; ++i)
        mono.addItem (proc.describeInput (i), true, cur == kSourceMonoFirst + i, [set, i] { set (kSourceMonoFirst + i); });
    for (int i = 0; i < kNumInputs / 2; ++i)
        stereo.addItem (shortSourceName (kSourceStereoFirst + i).replace ("/", " / ") + (proc.describeInput (i * 2).contains ("not") ? "  (not connected)" : ""),
                        true, cur == kSourceStereoFirst + i, [set, i] { set (kSourceStereoFirst + i); });
    m.addItem ("Off", true, cur == kSourceOff, [set] { set (kSourceOff); });
    m.addSubMenu ("Mono input", mono, true, nullptr, cur >= kSourceMonoFirst && cur < kSourceStereoFirst);
    m.addSubMenu ("Stereo input", stereo, true, nullptr, cur >= kSourceStereoFirst && cur < kSourceInstrument);
    m.addSeparator();
    auto& inst = *proc.channels[(size_t) ch].instrument;
    const String instName = inst.size() > 0 ? inst.getPlugin (0)->getName() : String();
    if (instName.isNotEmpty())
        m.addItem ("Instrument: " + instName, true, cur == kSourceInstrument, [set] { set (kSourceInstrument); });
    m.addItem (instName.isEmpty() ? "Load instrument..." : "Instrument options...", [this, set]
    {
        auto& c = *proc.channels[(size_t) ch].instrument;
        if (c.size() == 0) chains.showAddMenu (c, &sourceBtn, [set] { set (kSourceInstrument); });
        else chains.showMenu (c, &sourceBtn, [set] { set (kSourceInstrument); });
    });
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&sourceBtn));
}
} // namespace fh
