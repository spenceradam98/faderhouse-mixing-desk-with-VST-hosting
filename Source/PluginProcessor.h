#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Params.h"
#include "engine/ChannelStrip.h"
#include "engine/BusCompressor.h"
#include "hosting/HostedChain.h"

// The console: 16 channel strips, 4 aux sends/FX returns, a mix bus with
// hosted master inserts and a bus compressor, and the master fader.
// Standalone: strips take interface inputs or host an instrument each.
// Plugin: strips take the main input plus up to 7 optional stereo inputs.
class FaderhouseProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    FaderhouseProcessor();
    ~FaderhouseProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    const bool standalone;
    fh::PluginLibrary library;
    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree uiState { "UI" }; // theme, size, selected channel, channel names

    struct Channel
    {
        fh::ChannelStrip strip;
        std::unique_ptr<fh::HostedChain> instrument, inserts;
        std::atomic<float> peakL { 0 }, peakR { 0 };
        // raw parameters used by the router
        std::atomic<float> *src = nullptr, *midi = nullptr, *insPre = nullptr, *pan = nullptr, *fader = nullptr,
                           *mute = nullptr, *solo = nullptr;
        std::array<std::atomic<float>*, fh::kNumAux> send {}, sendPre {};
        // gain ramp state (main L/R, then per send L/R)
        float lastMain[2] {}, lastSend[fh::kNumAux][2] {};
    };
    struct Aux
    {
        std::unique_ptr<fh::HostedChain> chain;
        std::atomic<float> *level = nullptr, *pan = nullptr, *mute = nullptr;
        std::atomic<float> peakL { 0 }, peakR { 0 };
        float last[2] {};
    };

    std::array<Channel, fh::kNumChannels> channels;
    std::array<Aux, fh::kNumAux> aux;
    std::unique_ptr<fh::HostedChain> masterInserts;
    fh::BusCompressor busComp;
    std::atomic<float> masterPeakL { 0 }, masterPeakR { 0 };
    std::atomic<double> currentSampleRate { 48000.0 };

    juce::String getChannelName (int ch) const;
    void setChannelName (int ch, const juce::String&);
    int numAvailableInputs() const;          // inputs the strips can pick right now
    juce::String describeInput (int flatIndex) const;

    // Every hosted chain, for listeners and teardown.
    std::vector<fh::HostedChain*> allChains();

private:
    static BusesProperties makeBuses (bool standalone);
    void timerCallback() override;
    void applyHostingState (const juce::XmlElement&);
    std::unique_ptr<juce::XmlElement> hostingStateXml() const;
    int inputBufferIndex (int flatIndex) const;

    juce::AudioBuffer<float> inputCopy, chBuf, mixBuf;
    std::array<juce::AudioBuffer<float>, fh::kNumAux> auxBuf;
    juce::MidiBuffer chMidi, emptyMidi;
    std::array<int, fh::kNumInputs> inputMap {};
    std::atomic<float> *bcOn, *bcThr, *bcRatio, *bcAtk, *bcRel, *bcMakeup, *bcMix, *masterFader;
    float lastMaster = 1.0f;
    std::unique_ptr<juce::XmlElement> pendingHosting;
    juce::CriticalSection pendingLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaderhouseProcessor)
};
