#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using namespace fh;

namespace
{
bool isStandaloneWrapper()
{
    return PluginHostType::getPluginLoadedAs() == AudioProcessor::wrapperType_Standalone;
}

} // namespace

AudioProcessor::BusesProperties FaderhouseProcessor::makeBuses (bool standalone)
{
    BusesProperties b;
    if (standalone)
    {
        // One wide input bus: the strips pick from the audio interface's inputs.
        b = b.withInput ("Interface", AudioChannelSet::discreteChannels (kNumInputs), true);
    }
    else
    {
        b = b.withInput ("In 1/2", AudioChannelSet::stereo(), true);
        for (int i = 2; i < kNumInputs; i += 2)
            b = b.withInput ("In " + String (i + 1) + "/" + String (i + 2), AudioChannelSet::stereo(), false);
    }
    return b.withOutput ("Output", AudioChannelSet::stereo(), true);
}

namespace
{
inline float faderGain (float db) { return db <= kFaderMinDb + 0.01f ? 0.0f : Decibels::decibelsToGain (db); }
inline float sendGain (float db)  { return db <= kSendMinDb + 0.01f ? 0.0f : Decibels::decibelsToGain (db); }

// Mono sources use a constant-power pan law (-3 dB in the centre);
// stereo sources use a balance control (unity in the centre).
inline void panGains (float pan, bool stereo, float& gl, float& gr)
{
    const float halfPi = MathConstants<float>::halfPi;
    if (stereo)
    {
        gl = pan > 0 ? std::cos (pan * halfPi) : 1.0f;
        gr = pan < 0 ? std::cos (-pan * halfPi) : 1.0f;
    }
    else
    {
        const float t = (pan + 1.0f) * 0.5f * halfPi;
        gl = std::cos (t);
        gr = std::sin (t);
    }
}

inline void raiseMeter (std::atomic<float>& m, float v)
{
    if (v > m.load (std::memory_order_relaxed)) m.store (v, std::memory_order_relaxed);
}
} // namespace

FaderhouseProcessor::FaderhouseProcessor()
    : AudioProcessor (makeBuses (isStandaloneWrapper())),
      standalone (isStandaloneWrapper()),
      apvts (*this, nullptr, "PARAMS", createLayout (standalone))
{
    for (int c = 0; c < kNumChannels; ++c)
    {
        auto& ch = channels[(size_t) c];
        ch.strip.attach (apvts, c);
        ch.instrument = std::make_unique<HostedChain> (library, HostedChain::Kind::Instrument, "Ch " + String (c + 1) + " instrument");
        ch.inserts = std::make_unique<HostedChain> (library, HostedChain::Kind::Inserts, "Ch " + String (c + 1) + " inserts");
        auto g = [&] (const char* id) { return apvts.getRawParameterValue (cid (c, id)); };
        ch.src = g ("src"); ch.midi = g ("midi"); ch.insPre = g ("insPre"); ch.pan = g ("pan");
        ch.fader = g ("fader"); ch.mute = g ("mute"); ch.solo = g ("solo");
        for (int a = 0; a < kNumAux; ++a)
        {
            const String s = "s" + String (a + 1);
            ch.send[(size_t) a] = g (s.toRawUTF8());
            ch.sendPre[(size_t) a] = g ((s + "Pre").toRawUTF8());
        }
    }
    for (int a = 0; a < kNumAux; ++a)
    {
        auto& x = aux[(size_t) a];
        x.chain = std::make_unique<HostedChain> (library, HostedChain::Kind::Inserts, "FX " + auxName (a));
        x.level = apvts.getRawParameterValue (aid (a, "level"));
        x.pan = apvts.getRawParameterValue (aid (a, "pan"));
        x.mute = apvts.getRawParameterValue (aid (a, "mute"));
    }
    masterInserts = std::make_unique<HostedChain> (library, HostedChain::Kind::Inserts, "Master inserts");

    bcOn = apvts.getRawParameterValue ("bcOn");
    bcThr = apvts.getRawParameterValue ("bcThr");
    bcRatio = apvts.getRawParameterValue ("bcRatio");
    bcAtk = apvts.getRawParameterValue ("bcAtk");
    bcRel = apvts.getRawParameterValue ("bcRel");
    bcMakeup = apvts.getRawParameterValue ("bcMakeup");
    bcMix = apvts.getRawParameterValue ("bcMix");
    masterFader = apvts.getRawParameterValue ("master");

    uiState.setProperty ("theme", 0, nullptr);
    uiState.setProperty ("scale", 0.8, nullptr);
    uiState.setProperty ("selected", 0, nullptr);
    startTimerHz (5);
}

FaderhouseProcessor::~FaderhouseProcessor()
{
    stopTimer();
    library.onPluginRemoving = nullptr;
}

std::vector<HostedChain*> FaderhouseProcessor::allChains()
{
    std::vector<HostedChain*> v;
    for (auto& c : channels) { v.push_back (c.instrument.get()); v.push_back (c.inserts.get()); }
    for (auto& a : aux) v.push_back (a.chain.get());
    v.push_back (masterInserts.get());
    return v;
}

String FaderhouseProcessor::getChannelName (int ch) const
{
    auto n = uiState.getProperty ("name" + String (ch + 1)).toString();
    return n.isNotEmpty() ? n : String (ch + 1);
}

void FaderhouseProcessor::setChannelName (int ch, const String& n)
{
    uiState.setProperty ("name" + String (ch + 1), n.trim().substring (0, 12), nullptr);
}

int FaderhouseProcessor::inputBufferIndex (int flat) const
{
    if (standalone)
        return flat < getTotalNumInputChannels() ? flat : -1;

    const int bus = flat / 2, chan = flat % 2;
    if (bus >= getBusCount (true)) return -1;
    auto* b = getBus (true, bus);
    if (b == nullptr || ! b->isEnabled() || chan >= b->getNumberOfChannels()) return -1;
    return getChannelIndexInProcessBlockBuffer (true, bus, chan);
}

int FaderhouseProcessor::numAvailableInputs() const
{
    int n = 0;
    for (int i = 0; i < kNumInputs; ++i)
        if (inputBufferIndex (i) >= 0) ++n;
    return n;
}

String FaderhouseProcessor::describeInput (int flat) const
{
    return inputBufferIndex (flat) >= 0 ? "In " + String (flat + 1) : "In " + String (flat + 1) + " (not connected)";
}

bool FaderhouseProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;
    if (standalone)
        return layouts.inputBuses.size() <= 1 && layouts.getMainInputChannelSet().size() <= kNumInputs;

    for (int i = 0; i < layouts.inputBuses.size(); ++i)
    {
        const auto& set = layouts.inputBuses.getReference (i);
        if (set.isDisabled() || set == AudioChannelSet::stereo()) continue;
        if (i == 0 && set == AudioChannelSet::mono()) continue;
        return false;
    }
    return true;
}

void FaderhouseProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    const int maxIn = jmax (getTotalNumInputChannels(), kNumInputs);
    inputCopy.setSize (maxIn, blockSize);
    chBuf.setSize (2, blockSize);
    mixBuf.setSize (2, blockSize);
    for (auto& b : auxBuf) b.setSize (2, blockSize);
    chMidi.ensureSize (2048);

    for (auto& c : channels)
    {
        c.strip.prepare (sampleRate);
        c.instrument->prepare (sampleRate, blockSize);
        c.inserts->prepare (sampleRate, blockSize);
        std::fill (std::begin (c.lastMain), std::end (c.lastMain), 0.0f);
        for (auto& s : c.lastSend) s[0] = s[1] = 0.0f;
    }
    for (auto& a : aux) { a.chain->prepare (sampleRate, blockSize); a.last[0] = a.last[1] = 0; }
    masterInserts->prepare (sampleRate, blockSize);
    busComp.prepare (sampleRate);
    lastMaster = faderGain (masterFader->load());
}

void FaderhouseProcessor::releaseResources()
{
    for (auto* c : allChains()) c->release();
}

void FaderhouseProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int totalIn = getTotalNumInputChannels();

    if (n > chBuf.getNumSamples()) // host broke its promise: grow (allocates, but keeps working)
    {
        chBuf.setSize (2, n); mixBuf.setSize (2, n);
        for (auto& b : auxBuf) b.setSize (2, n);
        inputCopy.setSize (jmax (totalIn, kNumInputs), n);
    }

    for (int i = 0; i < kNumInputs; ++i)
    {
        const int idx = inputBufferIndex (i);
        inputMap[(size_t) i] = (idx >= 0 && idx < totalIn) ? idx : -1;
    }
    for (int i = 0; i < totalIn && i < inputCopy.getNumChannels(); ++i)
        inputCopy.copyFrom (i, 0, buffer, i, 0, n);

    mixBuf.clear (0, n);
    for (auto& b : auxBuf) b.clear (0, n);

    bool anySolo = false;
    for (auto& c : channels) anySolo |= c.solo->load() > 0.5f;

    auto copyInput = [&] (int flat, int dest)
    {
        const int idx = inputMap[(size_t) flat];
        if (idx >= 0) chBuf.copyFrom (dest, 0, inputCopy, idx, 0, n);
        else          chBuf.clear (dest, 0, n);
    };

    for (auto& c : channels)
    {
        const int src = (int) c.src->load();
        float gl = 0, gr = 0, f = 0, m = 0;
        const bool live = src != kSourceOff && ! (src == kSourceInstrument && ! c.instrument->isActive());

        if (live)
        {
            if (src == kSourceInstrument)
            {
                chBuf.clear (0, n);
                chMidi.clear();
                const int want = (int) c.midi->load(); // 0 = omni
                for (const auto meta : midi)
                {
                    const auto msg = meta.getMessage();
                    if (want == 0 || msg.getChannel() == 0 || msg.getChannel() == want)
                        chMidi.addEvent (msg, meta.samplePosition);
                }
                c.instrument->process (chBuf, chMidi);
            }
            else if (isStereoSource (src))
            {
                const int first = (src - kSourceStereoFirst) * 2;
                copyInput (first, 0);
                if (inputMap[(size_t) first + 1] >= 0) copyInput (first + 1, 1);
                else chBuf.copyFrom (1, 0, chBuf, 0, 0, n); // mono main input on a stereo source
            }
            else
            {
                copyInput (src - kSourceMonoFirst, 0);
                chBuf.copyFrom (1, 0, chBuf, 0, 0, n);
            }

            c.strip.processInput (chBuf, n);
            const bool insPre = c.insPre->load() > 0.5f;
            if (insPre && c.inserts->isActive()) { chMidi.clear(); c.inserts->process (chBuf, chMidi); }
            c.strip.processMain (chBuf, n);
            if (! insPre && c.inserts->isActive()) { chMidi.clear(); c.inserts->process (chBuf, chMidi); }

            const bool muted = c.mute->load() > 0.5f || (anySolo && c.solo->load() < 0.5f);
            m = muted ? 0.0f : 1.0f;
            f = faderGain (c.fader->load());
            panGains (c.pan->load(), src != kSourceOff && (isStereoSource (src)), gl, gr);
        }
        else
        {
            chBuf.clear (0, n);
        }

        const float tl = gl * f * m, tr = gr * f * m;
        if (tl != 0 || c.lastMain[0] != 0) mixBuf.addFromWithRamp (0, 0, chBuf.getReadPointer (0), n, c.lastMain[0], tl);
        if (tr != 0 || c.lastMain[1] != 0) mixBuf.addFromWithRamp (1, 0, chBuf.getReadPointer (1), n, c.lastMain[1], tr);
        c.lastMain[0] = tl; c.lastMain[1] = tr;

        for (int a = 0; a < kNumAux; ++a)
        {
            const float s = sendGain (c.send[(size_t) a]->load()) * m * (c.sendPre[(size_t) a]->load() > 0.5f ? 1.0f : f);
            auto& last = c.lastSend[a];
            auto& dst = auxBuf[(size_t) a];
            if (s * gl != 0 || last[0] != 0) dst.addFromWithRamp (0, 0, chBuf.getReadPointer (0), n, last[0], s * gl);
            if (s * gr != 0 || last[1] != 0) dst.addFromWithRamp (1, 0, chBuf.getReadPointer (1), n, last[1], s * gr);
            last[0] = s * gl; last[1] = s * gr;
        }

        if (live)
        {
            raiseMeter (c.peakL, chBuf.getMagnitude (0, 0, n) * tl);
            raiseMeter (c.peakR, chBuf.getMagnitude (1, 0, n) * tr);
        }
    }

    // FX returns (solo-safe)
    for (int a = 0; a < kNumAux; ++a)
    {
        auto& x = aux[(size_t) a];
        auto& b = auxBuf[(size_t) a];
        if (x.chain->isActive()) { chMidi.clear(); x.chain->process (b, chMidi); }
        float gl, gr;
        panGains (x.pan->load(), true, gl, gr);
        const float g = x.mute->load() > 0.5f ? 0.0f : faderGain (x.level->load());
        mixBuf.addFromWithRamp (0, 0, b.getReadPointer (0), n, x.last[0], gl * g);
        mixBuf.addFromWithRamp (1, 0, b.getReadPointer (1), n, x.last[1], gr * g);
        x.last[0] = gl * g; x.last[1] = gr * g;
        raiseMeter (x.peakL, b.getMagnitude (0, 0, n) * gl * g);
        raiseMeter (x.peakR, b.getMagnitude (1, 0, n) * gr * g);
    }

    // Mix bus: master inserts -> bus compressor -> master fader
    if (masterInserts->isActive()) { chMidi.clear(); masterInserts->process (mixBuf, chMidi); }
    BusCompressor::Settings bs;
    bs.on = bcOn->load() > 0.5f;
    bs.thresholdDb = bcThr->load();
    bs.ratio = (float) busRatioNames()[(int) bcRatio->load()].getIntValue();
    bs.attackMs = busAttackMs ((int) bcAtk->load());
    bs.releaseSec = busReleaseSec ((int) bcRel->load());
    bs.makeupDb = bcMakeup->load();
    bs.mix = bcMix->load();
    busComp.process (mixBuf, n, bs);

    const float mg = faderGain (masterFader->load());
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
    buffer.copyFromWithRamp (0, 0, mixBuf.getReadPointer (0), n, lastMaster, mg);
    buffer.copyFromWithRamp (1, 0, mixBuf.getReadPointer (1), n, lastMaster, mg);
    lastMaster = mg;
    raiseMeter (masterPeakL, buffer.getMagnitude (0, 0, n));
    raiseMeter (masterPeakR, buffer.getMagnitude (1, 0, n));
}

//==============================================================================
std::unique_ptr<XmlElement> FaderhouseProcessor::hostingStateXml() const
{
    auto x = std::make_unique<XmlElement> ("HOSTING");
    for (int c = 0; c < kNumChannels; ++c)
    {
        auto& ch = channels[(size_t) c];
        if (ch.instrument->size() == 0 && ch.inserts->size() == 0) continue;
        auto* e = x->createNewChildElement ("CH");
        e->setAttribute ("index", c);
        e->addChildElement (ch.instrument->toXml ("INSTRUMENT").release());
        e->addChildElement (ch.inserts->toXml ("INSERTS").release());
    }
    for (int a = 0; a < kNumAux; ++a)
    {
        auto* e = aux[(size_t) a].chain->toXml ("AUX").release();
        e->setAttribute ("index", a);
        x->addChildElement (e);
    }
    x->addChildElement (masterInserts->toXml ("MASTER").release());
    return x;
}

void FaderhouseProcessor::applyHostingState (const XmlElement& x)
{
    for (auto& c : channels) { c.instrument->clear(); c.inserts->clear(); }
    for (auto* e : x.getChildWithTagNameIterator ("CH"))
    {
        const int c = e->getIntAttribute ("index", -1);
        if (! isPositiveAndBelow (c, kNumChannels)) continue;
        if (auto* i = e->getChildByName ("INSTRUMENT")) channels[(size_t) c].instrument->fromXml (*i);
        if (auto* i = e->getChildByName ("INSERTS")) channels[(size_t) c].inserts->fromXml (*i);
    }
    for (auto* e : x.getChildWithTagNameIterator ("AUX"))
    {
        const int a = e->getIntAttribute ("index", -1);
        if (isPositiveAndBelow (a, kNumAux)) aux[(size_t) a].chain->fromXml (*e);
    }
    if (auto* m = x.getChildByName ("MASTER")) masterInserts->fromXml (*m);
}

void FaderhouseProcessor::getStateInformation (MemoryBlock& dest)
{
    XmlElement root ("FADERHOUSE");
    root.setAttribute ("version", 1);
    if (auto p = apvts.copyState().createXml()) root.addChildElement (p.release());
    if (auto u = uiState.createXml()) root.addChildElement (u.release());
    {
        const ScopedLock sl (pendingLock);
        // If a restore hasn't been applied yet, keep saving what we were given.
        root.addChildElement (pendingHosting != nullptr ? new XmlElement (*pendingHosting) : hostingStateXml().release());
    }
    copyXmlToBinary (root, dest);
}

void FaderhouseProcessor::setStateInformation (const void* data, int size)
{
    auto root = getXmlFromBinary (data, size);
    if (root == nullptr || ! root->hasTagName ("FADERHOUSE"))
        return;
    if (auto* p = root->getChildByName (apvts.state.getType()))
        apvts.replaceState (ValueTree::fromXml (*p));
    if (auto* u = root->getChildByName ("UI"))
        uiState.copyPropertiesFrom (ValueTree::fromXml (*u), nullptr);
    if (auto* h = root->getChildByName ("HOSTING"))
    {
        // Hosted plugins must be created on the message thread.
        if (MessageManager::existsAndIsCurrentThread())
            applyHostingState (*h);
        else
        {
            const ScopedLock sl (pendingLock);
            pendingHosting = std::make_unique<XmlElement> (*h);
        }
    }
}

void FaderhouseProcessor::timerCallback()
{
    std::unique_ptr<XmlElement> h;
    {
        const ScopedLock sl (pendingLock);
        h = std::move (pendingHosting);
    }
    if (h != nullptr)
        applyHostingState (*h);
}

AudioProcessorEditor* FaderhouseProcessor::createEditor() { return new FaderhouseEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FaderhouseProcessor(); }
