#include "StripProcessor.h"
#include "StripEditor.h"

using namespace juce;
using namespace fh;

FaderhouseStripProcessor::FaderhouseStripProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createStripLayout())
{
    strip.attach (apvts, 0);
    pan = apvts.getRawParameterValue (cid (0, "pan"));
    fader = apvts.getRawParameterValue (cid (0, "fader"));
    mute = apvts.getRawParameterValue (cid (0, "mute"));
    uiState.setProperty ("theme", 0, nullptr);
    uiState.setProperty ("scale", 1.0, nullptr);
}

bool FaderhouseStripProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    const bool inOk = in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
    const bool outOk = out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
    return inOk && outOk && in.size() <= out.size();
}

void FaderhouseStripProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    work.setSize (2, blockSize);
    strip.prepare (sampleRate);
    last[0] = last[1] = 0.0f;
}

void FaderhouseStripProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ins = getTotalNumInputChannels(), outs = getTotalNumOutputChannels();
    if (n > work.getNumSamples()) work.setSize (2, n);

    if (ins == 0) work.clear (0, n);
    else
    {
        work.copyFrom (0, 0, buffer, 0, 0, n);
        work.copyFrom (1, 0, buffer, ins > 1 ? 1 : 0, 0, n);
    }
    auto raise = [] (std::atomic<float>& m, float v) { if (v > m.load()) m.store (v); };
    raise (inPeakL, work.getMagnitude (0, 0, n));
    raise (inPeakR, work.getMagnitude (1, 0, n));

    strip.processInput (work, n);
    strip.processMain (work, n);

    const float f = mute->load() > 0.5f ? 0.0f
                  : (fader->load() <= kFaderMinDb + 0.01f ? 0.0f : Decibels::decibelsToGain (fader->load()));
    const float p = pan->load(), halfPi = MathConstants<float>::halfPi;
    float gl = 1, gr = 1;
    if (outs == 1)        { gl = gr = 1; }                     // mono track: no pan
    else if (ins > 1)     { gl = p > 0 ? std::cos (p * halfPi) : 1.0f; gr = p < 0 ? std::cos (-p * halfPi) : 1.0f; } // balance
    else                  { const float t = (p + 1) * 0.5f * halfPi; gl = std::cos (t) * MathConstants<float>::sqrt2; gr = std::sin (t) * MathConstants<float>::sqrt2; }
    // A mono track keeps unity gain at centre: the -3 dB law is compensated (+3 dB),
    // since a single strip should not change level just by being inserted.

    for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    buffer.copyFromWithRamp (0, 0, work.getReadPointer (0), n, last[0], gl * f);
    if (outs > 1) buffer.copyFromWithRamp (1, 0, work.getReadPointer (1), n, last[1], gr * f);
    last[0] = gl * f; last[1] = gr * f;
    raise (outPeakL, buffer.getMagnitude (0, 0, n));
    raise (outPeakR, buffer.getMagnitude (outs > 1 ? 1 : 0, 0, n));
}

void FaderhouseStripProcessor::getStateInformation (MemoryBlock& dest)
{
    XmlElement root ("FADERHOUSE_STRIP");
    root.setAttribute ("version", 1);
    if (auto p = apvts.copyState().createXml()) root.addChildElement (p.release());
    if (auto u = uiState.createXml()) root.addChildElement (u.release());
    copyXmlToBinary (root, dest);
}

void FaderhouseStripProcessor::setStateInformation (const void* data, int size)
{
    auto root = getXmlFromBinary (data, size);
    if (root == nullptr || ! root->hasTagName ("FADERHOUSE_STRIP")) return;
    if (auto* p = root->getChildByName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*p));
    if (auto* u = root->getChildByName ("UI")) uiState.copyPropertiesFrom (ValueTree::fromXml (*u), nullptr);
}

AudioProcessorEditor* FaderhouseStripProcessor::createEditor() { return new FaderhouseStripEditor (*this); }
