#include "ChainUI.h"

namespace fh
{
using namespace juce;

class ChainUI::PluginWindow : public DocumentWindow
{
public:
    PluginWindow (AudioProcessor& p, std::function<void (AudioProcessor*)> onClose)
        : DocumentWindow (p.getName(), Colours::darkgrey, DocumentWindow::closeButton), processor (p), closed (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        if (auto* ed = p.createEditorIfNeeded())
            setContentNonOwned (ed, true);
        else
            setContentOwned (new GenericAudioProcessorEditor (p), true);
        auto* ed = dynamic_cast<AudioProcessorEditor*> (getContentComponent());
        setResizable (ed != nullptr && ed->isResizable(), false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
        setAlwaysOnTop (true);
    }

    ~PluginWindow() override
    {
        if (auto* ed = dynamic_cast<AudioProcessorEditor*> (getContentComponent()))
        {
            const bool owned = dynamic_cast<GenericAudioProcessorEditor*> (ed) != nullptr;
            if (! owned) processor.editorBeingDeleted (ed);
            clearContentComponent();
            delete ed;
        }
    }

    void closeButtonPressed() override { MessageManager::callAsync ([c = closed, p = &processor] { c (p); }); }

    AudioProcessor& processor;

private:
    std::function<void (AudioProcessor*)> closed;
};

class ChainUI::ScannerWindow : public DocumentWindow
{
public:
    ScannerWindow (PluginLibrary& l, std::function<void()> onClose)
        : DocumentWindow ("Plugins", Colours::darkgrey, DocumentWindow::closeButton), lib (l), closed (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        auto* list = new PluginListComponent (lib.formatManager, lib.knownPlugins, lib.getDeadMansPedalFile(), nullptr, false);
        list->setSize (700, 480);
        setContentOwned (list, true);
        setResizable (true, false);
        centreWithSize (700, 480);
        setVisible (true);
        setAlwaysOnTop (true);
    }
    void closeButtonPressed() override
    {
        lib.saveKnownPlugins();
        MessageManager::callAsync (closed);
    }

private:
    PluginLibrary& lib;
    std::function<void()> closed;
};

ChainUI::ChainUI (PluginLibrary& l) : library (l) {}
ChainUI::~ChainUI() { closeAll(); }

void ChainUI::closeAll()
{
    windows.clear();
    if (scanner != nullptr) { library.saveKnownPlugins(); scanner.reset(); }
}

void ChainUI::openEditor (AudioProcessor& p)
{
    for (auto& w : windows)
        if (&w->processor == &p) { w->toFront (true); return; }
    windows.push_back (std::make_unique<PluginWindow> (p, [this] (AudioProcessor* proc) { closeEditor (proc); }));
}

void ChainUI::closeEditor (AudioProcessor* p)
{
    windows.erase (std::remove_if (windows.begin(), windows.end(), [p] (auto& w) { return &w->processor == p; }), windows.end());
}

void ChainUI::openScanner()
{
    if (scanner != nullptr) { scanner->toFront (true); return; }
    scanner = std::make_unique<ScannerWindow> (library, [this] { scanner.reset(); });
}

void ChainUI::showAddMenu (HostedChain& chain, Component* target, std::function<void()> onChange)
{
    const bool wantInstruments = chain.getKind() == HostedChain::Kind::Instrument;
    Array<PluginDescription> types;
    for (auto& d : library.knownPlugins.getTypes())
        if (d.isInstrument == wantInstruments)
            types.add (d);

    PopupMenu m;
    if (types.isEmpty())
        m.addItem (wantInstruments ? "No instruments found yet. Scan for plugins..." : "No effects found yet. Scan for plugins...",
                   [this] { openScanner(); });
    else
    {
        KnownPluginList::addToMenu (m, types, KnownPluginList::sortByManufacturer);
        m.addSeparator();
        m.addItem ("Scan for plugins...", [this] { openScanner(); });
    }

    auto opts = PopupMenu::Options();
    if (target != nullptr) opts = opts.withTargetComponent (target);
    m.showMenuAsync (opts, [this, &chain, types, onChange] (int result)
    {
        const int idx = KnownPluginList::getIndexChosenByMenu (types, result);
        if (! isPositiveAndBelow (idx, types.size()))
            return;
        String error;
        if (! chain.addPlugin (types.getReference (idx), error))
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Could not load plugin", error);
        else if (auto* p = chain.getPlugin (chain.size() - 1))
            openEditor (*p);
        if (onChange) onChange();
    });
}

void ChainUI::showSlotMenu (HostedChain& chain, int i, Component* target)
{
    auto* p = chain.getPlugin (i);
    if (p == nullptr) return;
    PopupMenu m;
    m.addSectionHeader (p->getName());
    m.addItem ("Open editor", [this, p] { openEditor (*p); });
    m.addItem ("Bypass", true, chain.isBypassed (i), [&chain, i] { chain.setBypassed (i, ! chain.isBypassed (i)); });
    if (chain.getKind() == HostedChain::Kind::Inserts)
    {
        m.addItem ("Move up", i > 0, false, [&chain, i] { chain.movePlugin (i, i - 1); });
        m.addItem ("Move down", i < chain.size() - 1, false, [&chain, i] { chain.movePlugin (i, i + 1); });
    }
    m.addSeparator();
    m.addItem ("Remove", [&chain, i] { chain.removePlugin (i); });
    auto opts = PopupMenu::Options();
    if (target != nullptr) opts = opts.withTargetComponent (target);
    m.showMenuAsync (opts);
}

void ChainUI::showMenu (HostedChain& chain, Component* target, std::function<void()> onChange)
{
    PopupMenu m;
    m.addSectionHeader (chain.getName());
    for (int i = 0; i < chain.size(); ++i)
    {
        auto* p = chain.getPlugin (i);
        if (p == nullptr) continue;
        PopupMenu sub;
        sub.addItem ("Open editor", [this, p] { openEditor (*p); });
        sub.addItem ("Bypass", true, chain.isBypassed (i), [&chain, i] { chain.setBypassed (i, ! chain.isBypassed (i)); });
        if (chain.getKind() == HostedChain::Kind::Inserts)
        {
            sub.addItem ("Move up", i > 0, false, [&chain, i] { chain.movePlugin (i, i - 1); });
            sub.addItem ("Move down", i < chain.size() - 1, false, [&chain, i] { chain.movePlugin (i, i + 1); });
        }
        sub.addSeparator();
        sub.addItem ("Remove", [&chain, i] { chain.removePlugin (i); });
        m.addSubMenu (String (i + 1) + ". " + p->getName() + (chain.isBypassed (i) ? "  (bypassed)" : ""), sub);
    }
    if (chain.size() > 0) m.addSeparator();
    const bool instrument = chain.getKind() == HostedChain::Kind::Instrument;
    m.addItem (instrument ? (chain.size() > 0 ? "Replace instrument..." : "Load instrument...") : "Add effect...",
               ! chain.isFull() || instrument,
               false, [this, &chain, target, onChange] { showAddMenu (chain, target, onChange); });
    m.addItem ("Scan for plugins...", [this] { openScanner(); });
    auto opts = PopupMenu::Options();
    if (target != nullptr) opts = opts.withTargetComponent (target);
    m.showMenuAsync (opts);
}
} // namespace fh
