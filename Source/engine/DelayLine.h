#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace fh
{
// Stereo integer-sample delay used for plugin latency compensation.
// When the delay changes the history is cleared, so a re-alignment gives a
// short gap of silence instead of replaying stale audio.
class DelayLine
{
public:
    void prepare (int maxDelaySamples, int maxBlock)
    {
        size = maxDelaySamples + maxBlock + 1;
        buf.setSize (2, size);
        buf.clear();
        write = 0;
        current = 0;
    }

    int getMaxDelay (int blockSize) const noexcept { return juce::jmax (0, size - blockSize - 1); }

    void process (juce::AudioBuffer<float>& b, int n, int delay) noexcept
    {
        delay = juce::jlimit (0, getMaxDelay (n), delay);
        if (delay != current)
        {
            buf.clear();
            current = delay;
        }
        if (delay == 0 || size <= 0)
            return;
        for (int c = 0; c < 2; ++c)
        {
            auto* d = b.getWritePointer (c);
            auto* h = buf.getWritePointer (c);
            int w = write;
            for (int i = 0; i < n; ++i)
            {
                h[w] = d[i];
                int r = w - delay;
                if (r < 0) r += size;
                d[i] = h[r];
                if (++w == size) w = 0;
            }
        }
        write = (write + n) % size;
    }

    int getDelay() const noexcept { return current; }

private:
    juce::AudioBuffer<float> buf;
    int size = 0, write = 0, current = 0;
};
} // namespace fh
