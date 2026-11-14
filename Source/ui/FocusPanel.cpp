#include "FocusPanel.h"
#include "PluginProcessor.h"

namespace fh
{
using namespace juce;

FocusPanel::FocusPanel (FaderhouseProcessor& p, ChainUI& c, int channel)
    : proc (p), chains (c), ch (channel), processing (p.apvts, p.channels[(size_t) channel].strip, channel)
{
    auto& s = proc.apvts;
    source.addItemList (sourceNames(), 1);
    srcAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, cid (ch, "src"), source);
    source.setTooltip ("Where this channel's signal comes from");
    addAndMakeVisible (source);
    StringArray midiNames { "MIDI Omni" };
    for (int i = 1; i <= 16; ++i) midiNames.add ("MIDI " + String (i));
    midiCh.addItemList (midiNames, 1);
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
    insPreAtt = attachToggle (s, cid (ch, "insPre"), insPre);
    insPre.setTooltip ("On: inserts run right after the input stage. Off: after dynamics and EQ");
    addAndMakeVisible (insPre);

    addAndMakeVisible (processing);
    for (auto* c2 : proc.allChains()) c2->addChangeListener (this);
    rebuildSlots();
}

FocusPanel::~FocusPanel()
{
    for (auto* c2 : proc.allChains()) c2->removeChangeListener (this);
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

void FocusPanel::refresh() { processing.refresh(); }

void FocusPanel::resized()
{
    auto r = getLocalBounds().reduced (6, 4);
    r.removeFromTop (26); // header

    secRouting = r.removeFromTop (66); r.removeFromTop (6);
    {
        auto a = secRouting.reduced (6).withTrimmedTop (12);
        auto row = a.removeFromTop (22);
        source.setBounds (row.removeFromLeft (row.getWidth() * 3 / 5)); row.removeFromLeft (6);
        midiCh.setBounds (row);
        a.removeFromTop (4);
        instrumentBtn.setBounds (a.removeFromTop (22));
    }

    secInserts = r.removeFromTop (100); r.removeFromTop (6);
    {
        auto a = secInserts.reduced (6).withTrimmedTop (12);
        auto bottom = a.removeFromBottom (20);
        addInsertBtn.setBounds (bottom.removeFromLeft (80));
        bottom.removeFromLeft (6);
        insPre.setBounds (bottom.removeFromLeft (80));
        a.removeFromBottom (4);
        const int cw = (a.getWidth() - 6) / 2, rh = (a.getHeight() - 4) / 3;
        for (int i = 0; i < slotButtons.size(); ++i)
            slotButtons[i]->setBounds (a.getX() + (i / 3) * (cw + 6), a.getY() + (i % 3) * (rh + 2), cw, rh);
    }
    processing.setBounds (r);
}

void FocusPanel::paint (Graphics& g)
{
    const auto& t = ConsoleLookAndFeel::themeOf (*this);
    g.setColour (t.panel.darker (0.08f)); g.fillRect (getLocalBounds());
    auto head = getLocalBounds().reduced (6, 4).removeFromTop (24).toFloat();
    g.setColour (t.screen); g.fillRoundedRectangle (head, 3.0f);
    g.setColour (t.curve); g.setFont (Font (FontOptions (14.0f, Font::bold)));
    const auto name = proc.getChannelName (ch);
    g.drawText ("CHANNEL " + String (ch + 1) + "   " + (name == String (ch + 1) ? String() : name), head.reduced (8, 0), Justification::centredLeft);
    drawSection (g, secRouting.toFloat(), t, "ROUTING");
    drawSection (g, secInserts.toFloat(), t, "INSERTS (VST3)");
}
} // namespace fh
