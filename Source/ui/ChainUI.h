#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "hosting/HostedChain.h"

namespace fh
{
// Plugin editor windows, the plugin scanner window and the chain menus.
// One per console editor. Message thread only.
class ChainUI
{
public:
    explicit ChainUI (PluginLibrary&);
    ~ChainUI();

    // Pop-up menu for a chain: open/bypass/move/remove each slot, add a plugin.
    void showMenu (HostedChain&, juce::Component* target, std::function<void()> onChange = {});
    // Menu of plugins that can be added (effects or instruments), then add the choice.
    void showAddMenu (HostedChain&, juce::Component* target, std::function<void()> onChange = {});
    void showSlotMenu (HostedChain&, int slot, juce::Component* target);

    void openEditor (juce::AudioProcessor&);
    void closeEditor (juce::AudioProcessor*);
    void openScanner();
    void closeAll();

private:
    class PluginWindow;
    class ScannerWindow;
    PluginLibrary& library;
    std::vector<std::unique_ptr<PluginWindow>> windows;
    std::unique_ptr<ScannerWindow> scanner;
};
} // namespace fh
