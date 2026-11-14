#include "PluginEditor.h"

using namespace juce;
using namespace fh;

class FaderhouseEditor::Header : public Component
{
public:
    explicit Header (FaderhouseEditor& e) : ed (e)
    {
        for (auto& t : getThemes()) theme.addItem (t.name, theme.getNumItems() + 1);
        theme.setTooltip ("Console theme");
        theme.onChange = [this] { ed.setTheme (theme.getSelectedItemIndex()); };
        addAndMakeVisible (theme);

        for (int pct : { 60, 70, 80, 90, 100, 115 }) size.addItem (String (pct) + "%", pct);
        size.setTooltip ("Window size");
        size.onChange = [this] { ed.setScale (size.getSelectedId() / 100.0); };
        addAndMakeVisible (size);

        plugins.setTooltip ("Scan for and manage installed VST3 plugins");
        plugins.onClick = [this] { ed.chainUI.openScanner(); };
        addAndMakeVisible (plugins);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 8);
        plugins.setBounds (r.removeFromRight (100)); r.removeFromRight (8);
        size.setBounds (r.removeFromRight (70)); r.removeFromRight (8);
        theme.setBounds (r.removeFromRight (140));
    }

    void paint (Graphics& g) override
    {
        const auto& t = ConsoleLookAndFeel::themeOf (*this);
        auto r = getLocalBounds().toFloat();
        ColourGradient cg (t.panel.darker (0.25f), 0, 0, t.panel.darker (0.4f), 0, r.getBottom(), false);
        if (! t.bevel) cg = ColourGradient (t.panel.darker (0.05f), 0, 0, t.panel.darker (0.05f), 0, r.getBottom(), false);
        g.setGradientFill (cg); g.fillRect (r);
        g.setColour (t.sectionEdge); g.drawHorizontalLine (getHeight() - 1, 0, r.getRight());
        drawScrew (g, { 12, r.getCentreY() }, t);
        drawScrew (g, { r.getRight() - 12, r.getCentreY() }, t);

        const Colour ink = t.bevel ? t.panel.darker (0.25f).contrasting (0.9f) : t.text;
        g.setColour (ink);
        g.setFont (Font (FontOptions (20.0f, Font::bold)).withExtraKerningFactor (0.18f));
        g.drawText (String (FH_PRODUCT_NAME).toUpperCase(), 28, 0, 260, getHeight(), Justification::centredLeft);
        g.setColour (t.accent);
        g.fillRect (28.0f, r.getBottom() - 9.0f, 34.0f, 3.0f);
        g.setColour (ink.withAlpha (0.6f));
        g.setFont (Font (FontOptions (11.0f, Font::bold)).withExtraKerningFactor (0.1f));
        g.drawText ("16 CHANNEL CONSOLE  " + String (ed.proc.standalone ? "STANDALONE" : "PLUGIN") + "  " + String (FH_BRAND_NAME).toUpperCase(),
                    220, 0, 420, getHeight(), Justification::centredLeft);
    }

    FaderhouseEditor& ed;
    ComboBox theme, size;
    TextButton plugins { "PLUGINS..." };
};

FaderhouseEditor::FaderhouseEditor (FaderhouseProcessor& p)
    : AudioProcessorEditor (p), proc (p), chainUI (p.library)
{
    const double scale = jlimit (0.5, 1.5, (double) proc.uiState.getProperty ("scale", 0.8));
    setLookAndFeel (&lnf);
    proc.library.onPluginRemoving = [this] (AudioProcessor* pl) { chainUI.closeEditor (pl); };

    header = std::make_unique<Header> (*this);
    content.addAndMakeVisible (*header);
    for (int c = 0; c < kNumChannels; ++c)
        content.addAndMakeVisible (strips.add (new StripView (proc, chainUI, c, [this] (int ch) { selectChannel (ch); })));
    master = std::make_unique<MasterSection> (proc, chainUI);
    content.addAndMakeVisible (*master);
    addAndMakeVisible (content);

    setTheme ((int) proc.uiState.getProperty ("theme", 0));
    selectChannel ((int) proc.uiState.getProperty ("selected", 0));

    setResizable (true, true);
    setResizeLimits (kBaseW / 2, kBaseH / 2, kBaseW * 3 / 2, kBaseH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kBaseW / kBaseH);
    setSize (roundToInt (kBaseW * scale), roundToInt (kBaseH * scale));
    startTimerHz (30);
}

FaderhouseEditor::~FaderhouseEditor()
{
    stopTimer();
    proc.library.onPluginRemoving = nullptr;
    chainUI.closeAll();
    setLookAndFeel (nullptr);
}

void FaderhouseEditor::setTheme (int index)
{
    index = jlimit (0, (int) getThemes().size() - 1, index);
    proc.uiState.setProperty ("theme", index, nullptr);
    lnf.setTheme (getTheme (index));
    header->theme.setSelectedItemIndex (index, dontSendNotification);
    sendLookAndFeelChange();
    repaint();
}

void FaderhouseEditor::setScale (double scale)
{
    setSize (roundToInt (kBaseW * scale), roundToInt (kBaseH * scale));
}

void FaderhouseEditor::selectChannel (int ch)
{
    selected = jlimit (0, kNumChannels - 1, ch);
    proc.uiState.setProperty ("selected", selected, nullptr);
    if (focus == nullptr || focus->getChannel() != selected)
    {
        focus = std::make_unique<FocusPanel> (proc, chainUI, selected);
        content.addAndMakeVisible (*focus);
        resized();
    }
}

void FaderhouseEditor::resized()
{
    if (getWidth() <= 0) return;
    const double scale = (double) getWidth() / kBaseW;
    if (isVisible() || getParentComponent() != nullptr || scale >= 0.5)
        proc.uiState.setProperty ("scale", scale, nullptr);
    content.setBounds (0, 0, kBaseW, kBaseH);
    content.setTransform (AffineTransform::scale ((float) scale));

    auto r = Rectangle<int> (0, 0, kBaseW, kBaseH);
    header->setBounds (r.removeFromTop (kHeaderH));
    if (focus) focus->setBounds (r.removeFromLeft (FocusPanel::kWidth));
    master->setBounds (r.removeFromRight (MasterSection::kWidth));
    for (auto* s : strips) s->setBounds (r.removeFromLeft (StripView::kWidth));

    const int pct = roundToInt (scale * 100.0);
    header->size.setSelectedId (pct, dontSendNotification);
    if (header->size.getSelectedId() == 0) header->size.setText (String (pct) + "%", dontSendNotification);
}

void FaderhouseEditor::paint (Graphics& g)
{
    g.fillAll (lnf.theme().panel);
}

void FaderhouseEditor::timerCallback()
{
    if (focus) focus->refresh(); // reads the GR meters before the strips clear them
    for (int i = 0; i < strips.size(); ++i) strips[i]->refresh (i == selected);
    master->refresh();
}
