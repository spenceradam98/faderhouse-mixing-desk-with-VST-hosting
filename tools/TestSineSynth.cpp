// Minimal instrument used to verify Faderhouse's instrument hosting:
// plays a sine at the pitch of the last held note, at 0.25 amplitude.
#include <juce_audio_utils/juce_audio_utils.h>

class TestSineSynth : public juce::AudioProcessor
{
public:
    TestSineSynth() : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo())) {}
    void prepareToPlay (double s, int) override { sr = s; }
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer& midi) override
    {
        b.clear();
        int pos = 0;
        auto render = [&] (int end)
        {
            for (; pos < end; ++pos)
            {
                const float v = note >= 0 ? 0.25f * (float) std::sin (phase) : 0.0f;
                if (note >= 0) phase += juce::MathConstants<double>::twoPi * juce::MidiMessage::getMidiNoteInHertz (note) / sr;
                for (int c = 0; c < b.getNumChannels(); ++c) b.setSample (c, pos, v);
            }
        };
        for (const auto m : midi)
        {
            render (m.samplePosition);
            const auto msg = m.getMessage();
            if (msg.isNoteOn()) note = msg.getNoteNumber();
            else if (msg.isNoteOff() && msg.getNoteNumber() == note) note = -1;
        }
        render (b.getNumSamples());
    }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "TestSineSynth"; }
    bool acceptsMidi() const override { return true; }
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
    double sr = 48000, phase = 0;
    int note = -1;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TestSineSynth(); }
