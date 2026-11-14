#include "BusCompressor.h"
#include <cmath>

namespace fh
{
using namespace juce;

void BusCompressor::process (AudioBuffer<float>& b, int n, const Settings& s)
{
    if (! s.on) { grDb = 0; slowGr = 0; return; }

    auto coeff = [this] (double sec) { return (float) std::exp (-1.0 / (std::max (1.0e-5, sec) * sr)); };
    const float atk = coeff (s.attackMs * 0.001);
    const float slope = 1.0f / s.ratio - 1.0f;
    const float knee = 4.0f;
    const float makeup = Decibels::decibelsToGain (s.makeupDb);
    const float slowC = coeff (2.0);
    const float fixedRel = s.releaseSec > 0 ? coeff (s.releaseSec) : 0.0f;
    const float autoFast = coeff (0.1), autoSlow = coeff (1.2);

    float deepest = 0;
    auto* l = b.getWritePointer (0);
    auto* r = b.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float peak = std::max (std::abs (l[i]), std::abs (r[i]));
        const float lvl = Decibels::gainToDecibels (peak, -120.0f);
        const float over = lvl - s.thresholdDb;
        float target = 0;
        if (over > knee * 0.5f)       target = slope * over;
        else if (over > -knee * 0.5f) target = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2 * knee);

        float rel = fixedRel;
        if (s.releaseSec <= 0)
        {
            // Auto: short peaks recover fast, sustained reduction recovers slowly.
            const float sustained = jlimit (0.0f, 1.0f, -slowGr / 6.0f);
            rel = autoFast + (autoSlow - autoFast) * sustained;
        }
        const float c = target < grDb ? atk : rel;
        grDb = target + c * (grDb - target);
        slowGr = grDb + slowC * (slowGr - grDb);
        deepest = std::min (deepest, grDb);

        const float g = Decibels::decibelsToGain (grDb) * makeup;
        const float wet = g * s.mix + (1.0f - s.mix);
        l[i] *= wet; r[i] *= wet;
    }
    if (deepest < meterGrDb.load (std::memory_order_relaxed))
        meterGrDb.store (deepest, std::memory_order_relaxed);
}
} // namespace fh
