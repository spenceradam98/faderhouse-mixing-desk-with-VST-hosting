#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace fh
{
// Stereo-linked VCA-style mix bus compressor in the spirit of classic
// console centre-section compressors: stepped ratio/attack/release, auto
// (program dependent) release, make-up gain and a parallel mix control.
class BusCompressor
{
public:
    struct Settings
    {
        bool on = false;
        float thresholdDb = -10, ratio = 4, attackMs = 3, releaseSec = -1 /* auto */, makeupDb = 0, mix = 1;
    };

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { grDb = 0; slowGr = 0; }
    void process (juce::AudioBuffer<float>&, int numSamples, const Settings&);

    std::atomic<float> meterGrDb { 0.0f };

private:
    double sr = 48000.0;
    float grDb = 0, slowGr = 0;
};
} // namespace fh
