#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Biquad.h"

namespace fh
{
// The DSP of one console channel, apart from routing, hosting and the fader.
// Signal order: input stage -> filters -> dynamics/EQ (order switchable).
// All processing is stereo; a mono source is copied to both sides first.
class ChannelStrip
{
public:
    void attach (juce::AudioProcessorValueTreeState&, int channelIndex);
    void prepare (double sampleRate);
    void reset();

    void processInput (juce::AudioBuffer<float>&, int numSamples);      // trim, drive, phase
    void processMain (juce::AudioBuffer<float>&, int numSamples);       // filters, dynamics, EQ

    EqSettings readEq() const;
    bool eqEnabled() const { return p.eqOn->load() > 0.5f; }

    // Meter taps, written on the audio thread, read and cleared by the UI.
    std::atomic<float> compGrDb { 0.0f }, gateGrDb { 0.0f };

    struct Params
    {
        std::atomic<float> *trim, *drive, *phase, *hpf, *lpf,
            *cOn, *cThr, *cRatio, *cRel, *cFast,
            *gOn, *gThr, *gRange, *gRel, *gExp, *dynPre, *eqOn, *eqBlk,
            *hfG, *hfF, *hfBell, *hmG, *hmF, *hmQ, *lmG, *lmF, *lmQ, *lfG, *lfF, *lfBell;
    } p {};

private:
    void processFilters (juce::AudioBuffer<float>&, int n);
    void processEq (juce::AudioBuffer<float>&, int n);
    void processDynamics (juce::AudioBuffer<float>&, int n);

    double sr = 48000.0;
    juce::LinearSmoothedValue<float> trimGain;

    Biquad hpf2, lpf2; OnePoleHP hpf1;
    float lastHpf = -1, lastLpf = -1;
    bool hpfOn = false, lpfOn = false;

    EqDesign eq; EqSettings lastEq; bool eqDirty = true;

    float compGr = 0;     // dB, <= 0
    float gateDb = 0;     // gate/expander gain in dB
    int gateHold = 0;
};
} // namespace fh
