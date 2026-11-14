#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Params.h"
#include "engine/ChannelStrip.h"

// Faderhouse Strip: one console channel (input stage, filters, dynamics, EQ,
// pan and fader) as an insert plugin for a single DAW track.
class FaderhouseStripProcessor : public juce::AudioProcessor
{
public:
    FaderhouseStripProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
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

    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree uiState { "UI" }; // theme, scale
    fh::ChannelStrip strip;
    std::atomic<float> inPeakL { 0 }, inPeakR { 0 }, outPeakL { 0 }, outPeakR { 0 };

private:
    juce::AudioBuffer<float> work;
    std::atomic<float> *pan, *fader, *mute;
    float last[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaderhouseStripProcessor)
};
