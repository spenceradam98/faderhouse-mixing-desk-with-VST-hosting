// Minimal FX used to verify Faderhouse's latency compensation:
// delays the signal by 480 samples and reports that latency to the host.
#include <juce_audio_utils/juce_audio_utils.h>

class TestDelay : public juce::AudioProcessor
{
public:
    static constexpr int kDelay = 480;
    TestDelay() : AudioProcessor (BusesProperties().withInput ("In", juce::AudioChannelSet::stereo())
                                                   .withOutput ("Out", juce::AudioChannelSet::stereo()))
    {
        setLatencySamples (kDelay);
    }
    void prepareToPlay (double, int) override { history.setSize (2, kDelay); history.clear(); pos = 0; setLatencySamples (kDelay); }
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) override
    {
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            for (int c = 0; c < juce::jmin (2, b.getNumChannels()); ++c)
            {
                const float in = b.getSample (c, i);
                b.setSample (c, i, history.getSample (c, pos));
                history.setSample (c, pos, in);
            }
            pos = (pos + 1) % kDelay;
        }
    }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "TestDelay"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    juce::AudioBuffer<float> history;
    int pos = 0;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TestDelay(); }
