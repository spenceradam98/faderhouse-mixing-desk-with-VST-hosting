#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace fh
{
// Installed-plugin list and format manager, shared by every chain in one console.
class PluginLibrary
{
public:
    PluginLibrary();
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    juce::File getPluginListFile() const;
    juce::File getDeadMansPedalFile() const;
    void saveKnownPlugins() const;

    // Called on the message thread just before a hosted plugin is deleted,
    // so the UI can close its editor window first.
    std::function<void (juce::AudioProcessor*)> onPluginRemoving;
};

// A serial chain of hosted plugins on its own AudioProcessorGraph, always stereo.
// An instrument chain holds one generator plugin fed with MIDI; an insert chain
// holds effects. Audio thread: process() and isActive(). Everything else: message thread.
class HostedChain : public juce::ChangeBroadcaster
{
public:
    enum class Kind { Inserts, Instrument };
    static constexpr int kMaxInserts = 6;

    HostedChain (PluginLibrary&, Kind, juce::String name);
    ~HostedChain() override;

    void prepare (double sampleRate, int blockSize);
    void release();
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&);
    bool isActive() const noexcept { return activeCount.load (std::memory_order_acquire) > 0; }

    bool addPlugin (const juce::PluginDescription&, juce::String& error);
    void removePlugin (int index);
    void movePlugin (int from, int to);
    void setBypassed (int index, bool bypassed);
    bool isBypassed (int index) const;
    void clear();

    int size() const noexcept { return (int) slots.size(); }
    bool isFull() const noexcept { return size() >= (kind == Kind::Instrument ? 1 : kMaxInserts); }
    juce::AudioPluginInstance* getPlugin (int index) const;
    int getLatencySamples() const { return graph.getLatencySamples(); }
    Kind getKind() const noexcept { return kind; }
    const juce::String& getName() const noexcept { return name; }
    juce::String describe() const; // "Reverb, EQ" or "" when empty

    std::unique_ptr<juce::XmlElement> toXml (const juce::String& tag) const;
    void fromXml (const juce::XmlElement&);

    PluginLibrary& library;

private:
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorGraph::Node::Ptr inputNode, outputNode, midiNode;
    std::vector<juce::AudioProcessorGraph::Node::Ptr> slots;
    std::vector<juce::PluginDescription> descs; // as loaded (instances can report a different file)
    std::atomic<int> activeCount { 0 };
    const Kind kind;
    const juce::String name;
    double currentRate = 48000.0;
    int currentBlock = 512;

    void rewire();
    void updateActive();
    void configureBuses (juce::AudioPluginInstance&) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedChain)
};
} // namespace fh
