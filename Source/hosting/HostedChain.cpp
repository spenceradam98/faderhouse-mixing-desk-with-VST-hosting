#include "HostedChain.h"

namespace fh
{
using namespace juce;
using Graph = AudioProcessorGraph;
using IOProc = Graph::AudioGraphIOProcessor;

//==============================================================================
PluginLibrary::PluginLibrary()
{
    formatManager.addDefaultFormats();
    if (auto xml = XmlDocument::parse (getPluginListFile()))
        knownPlugins.recreateFromXml (*xml);
}

File PluginLibrary::getPluginListFile() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile (FH_BRAND_NAME).getChildFile (FH_PRODUCT_NAME).getChildFile ("KnownPlugins.xml");
}

File PluginLibrary::getDeadMansPedalFile() const { return getPluginListFile().getSiblingFile ("ScanInProgress.txt"); }

void PluginLibrary::saveKnownPlugins() const
{
    if (auto xml = knownPlugins.createXml())
    {
        getPluginListFile().getParentDirectory().createDirectory();
        xml->writeTo (getPluginListFile());
    }
}

//==============================================================================
HostedChain::HostedChain (PluginLibrary& lib, Kind k, String n)
    : library (lib), kind (k), name (std::move (n))
{
    graph.setPlayConfigDetails (2, 2, currentRate, currentBlock);
    inputNode = graph.addNode (std::make_unique<IOProc> (IOProc::audioInputNode));
    outputNode = graph.addNode (std::make_unique<IOProc> (IOProc::audioOutputNode));
    midiNode = graph.addNode (std::make_unique<IOProc> (IOProc::midiInputNode));
    rewire();
}

HostedChain::~HostedChain()
{
    activeCount = 0;
    graph.clear();
}

void HostedChain::prepare (double sampleRate, int blockSize)
{
    currentRate = sampleRate;
    currentBlock = blockSize;
    graph.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    graph.prepareToPlay (sampleRate, blockSize);
}

void HostedChain::release() { graph.releaseResources(); }

void HostedChain::process (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    graph.processBlock (buffer, midi);
}

void HostedChain::configureBuses (AudioPluginInstance& p) const
{
    auto layout = p.getBusesLayout();
    if (! layout.inputBuses.isEmpty())  layout.inputBuses.getReference (0) = AudioChannelSet::stereo();
    if (! layout.outputBuses.isEmpty()) layout.outputBuses.getReference (0) = AudioChannelSet::stereo();
    if (p.checkBusesLayoutSupported (layout))
        p.setBusesLayout (layout);
}

bool HostedChain::addPlugin (const PluginDescription& desc, String& error)
{
    if (kind == Kind::Instrument)
        clear();
    else if (isFull())
    {
        error = "This chain is full (" + String (kMaxInserts) + " plugins)";
        return false;
    }

    auto instance = library.formatManager.createPluginInstance (desc, currentRate, currentBlock, error);
    if (instance == nullptr)
    {
        if (error.isEmpty()) error = "Could not load " + desc.name;
        return false;
    }

    configureBuses (*instance);
    auto node = graph.addNode (std::move (instance));
    if (node == nullptr)
    {
        error = "Could not add " + desc.name;
        return false;
    }
    slots.push_back (node);
    descs.push_back (desc);
    rewire();
    sendChangeMessage();
    return true;
}

void HostedChain::removePlugin (int index)
{
    if (! isPositiveAndBelow (index, size()))
        return;
    if (library.onPluginRemoving)
        library.onPluginRemoving (slots[(size_t) index]->getProcessor());
    auto id = slots[(size_t) index]->nodeID;
    slots.erase (slots.begin() + index);
    descs.erase (descs.begin() + index);
    rewire();
    graph.removeNode (id);
    sendChangeMessage();
}

void HostedChain::movePlugin (int from, int to)
{
    if (! isPositiveAndBelow (from, size()) || ! isPositiveAndBelow (to, size()) || from == to)
        return;
    auto s = slots[(size_t) from];
    slots.erase (slots.begin() + from);
    slots.insert (slots.begin() + to, s);
    auto d = descs[(size_t) from];
    descs.erase (descs.begin() + from);
    descs.insert (descs.begin() + to, d);
    rewire();
    sendChangeMessage();
}

void HostedChain::setBypassed (int index, bool bypassed)
{
    if (! isPositiveAndBelow (index, size()))
        return;
    slots[(size_t) index]->setBypassed (bypassed);
    updateActive();
    sendChangeMessage();
}

bool HostedChain::isBypassed (int index) const
{
    return isPositiveAndBelow (index, size()) && slots[(size_t) index]->isBypassed();
}

void HostedChain::clear()
{
    if (slots.empty())
        return;
    for (auto& s : slots)
        if (library.onPluginRemoving)
            library.onPluginRemoving (s->getProcessor());
    auto old = slots;
    slots.clear();
    descs.clear();
    rewire();
    for (auto& s : old)
        graph.removeNode (s->nodeID);
    sendChangeMessage();
}

AudioPluginInstance* HostedChain::getPlugin (int index) const
{
    if (! isPositiveAndBelow (index, size()))
        return nullptr;
    return dynamic_cast<AudioPluginInstance*> (slots[(size_t) index]->getProcessor());
}

String HostedChain::describe() const
{
    StringArray names;
    for (int i = 0; i < size(); ++i)
        if (auto* p = getPlugin (i))
            names.add (p->getName());
    return names.joinIntoString (", ");
}

void HostedChain::updateActive()
{
    int n = 0;
    for (auto& s : slots)
        if (! s->isBypassed())
            ++n;
    activeCount.store (n, std::memory_order_release);
    refreshLatency();
}

void HostedChain::refreshLatency()
{
    latency.store (slots.empty() ? 0 : juce::jmax (0, graph.getLatencySamples()), std::memory_order_relaxed);
}

void HostedChain::rewire()
{
    for (auto& c : graph.getConnections())
        graph.removeConnection (c);

    auto audio = [this] (Graph::NodeID src, int srcChans, Graph::NodeID dst, int dstChans)
    {
        if (srcChans <= 0 || dstChans <= 0) return;
        for (int i = 0; i < jmax (srcChans, dstChans); ++i) // mono fans out, stereo sums into mono
            graph.addConnection ({ { src, i % srcChans }, { dst, i % dstChans } });
    };

    auto prevId = inputNode->nodeID;
    int prevChans = 2;
    for (auto& s : slots)
    {
        auto* p = s->getProcessor();
        if (p->acceptsMidi())
            graph.addConnection ({ { midiNode->nodeID, Graph::midiChannelIndex }, { s->nodeID, Graph::midiChannelIndex } });

        const int ins = p->getMainBusNumInputChannels();
        const int outs = p->getMainBusNumOutputChannels();
        if (outs == 0)
            continue; // MIDI-only plugin: gets MIDI, passes no audio
        if (ins > 0)
            audio (prevId, prevChans, s->nodeID, ins);
        // A generator (no inputs) replaces whatever came before it.
        prevId = s->nodeID;
        prevChans = outs;
    }
    audio (prevId, prevChans, outputNode->nodeID, 2);
    updateActive();
}

std::unique_ptr<XmlElement> HostedChain::toXml (const String& tag) const
{
    auto xml = std::make_unique<XmlElement> (tag);
    for (int i = 0; i < size(); ++i)
    {
        auto* p = getPlugin (i);
        if (p == nullptr) continue;
        auto* e = xml->createNewChildElement ("SLOT");
        e->setAttribute ("bypassed", isBypassed (i));
        e->addChildElement (descs[(size_t) i].createXml().release());
        MemoryBlock state;
        p->getStateInformation (state);
        e->createNewChildElement ("STATE")->addTextElement (state.toBase64Encoding());
    }
    return xml;
}

void HostedChain::fromXml (const XmlElement& xml)
{
    clear();
    for (auto* e : xml.getChildWithTagNameIterator ("SLOT"))
    {
        PluginDescription desc;
        auto* d = e->getChildByName ("PLUGIN");
        if (d == nullptr || ! desc.loadFromXml (*d))
            continue;
        String error;
        if (! addPlugin (desc, error))
        {
            DBG ("Restore " << name << ": " << error);
            continue;
        }
        if (auto* st = e->getChildByName ("STATE"))
        {
            MemoryBlock state;
            if (state.fromBase64Encoding (st->getAllSubText()))
                getPlugin (size() - 1)->setStateInformation (state.getData(), (int) state.getSize());
        }
        setBypassed (size() - 1, e->getBoolAttribute ("bypassed"));
    }
}
} // namespace fh
