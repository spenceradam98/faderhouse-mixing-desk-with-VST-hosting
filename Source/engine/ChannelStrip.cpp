#include "ChannelStrip.h"
#include "Params.h"

namespace fh
{
using namespace juce;

void ChannelStrip::attach (AudioProcessorValueTreeState& s, int c)
{
    auto g = [&] (const char* id) { auto* v = s.getRawParameterValue (cid (c, id)); jassert (v != nullptr); return v; };
    p = { g ("trim"), g ("drive"), g ("phase"), g ("hpf"), g ("lpf"),
          g ("cOn"), g ("cThr"), g ("cRatio"), g ("cRel"), g ("cFast"),
          g ("gOn"), g ("gThr"), g ("gRange"), g ("gRel"), g ("gExp"), g ("dynPre"), g ("eqOn"), g ("eqBlk"),
          g ("hfG"), g ("hfF"), g ("hfBell"), g ("hmG"), g ("hmF"), g ("hmQ"), g ("lmG"), g ("lmF"), g ("lmQ"),
          g ("lfG"), g ("lfF"), g ("lfBell") };
}

void ChannelStrip::prepare (double sampleRate)
{
    sr = sampleRate;
    trimGain.reset (sr, 0.02);
    trimGain.setCurrentAndTargetValue (Decibels::decibelsToGain (p.trim->load()));
    lastHpf = lastLpf = -1;
    eqDirty = true;
    reset();
}

void ChannelStrip::reset()
{
    hpf2.reset(); hpf1.reset(); lpf2.reset();
    eq.hf.reset(); eq.hm.reset(); eq.lm.reset(); eq.lf.reset();
    compGr = 0; gateDb = 0; gateHold = 0;
}

EqSettings ChannelStrip::readEq() const
{
    EqSettings s;
    s.black = p.eqBlk->load() > 0.5f;
    s.hfG = p.hfG->load(); s.hfF = p.hfF->load(); s.hfBell = p.hfBell->load() > 0.5f;
    s.hmG = p.hmG->load(); s.hmF = p.hmF->load(); s.hmQ = p.hmQ->load();
    s.lmG = p.lmG->load(); s.lmF = p.lmF->load(); s.lmQ = p.lmQ->load();
    s.lfG = p.lfG->load(); s.lfF = p.lfF->load(); s.lfBell = p.lfBell->load() > 0.5f;
    return s;
}

void ChannelStrip::processInput (AudioBuffer<float>& b, int n)
{
    float target = Decibels::decibelsToGain (p.trim->load());
    if (p.phase->load() > 0.5f) target = -target;
    if (std::signbit (target) != std::signbit (trimGain.getTargetValue()))
        trimGain.setCurrentAndTargetValue (target); // polarity flips instantly
    else
        trimGain.setTargetValue (target);

    const float drive = p.drive->load();
    const float a = 1.0f + 9.0f * drive;      // pre-gain into the saturator
    const float bias = 0.12f * drive;          // a touch of even harmonics
    const float biasOut = std::tanh (bias);

    auto* l = b.getWritePointer (0);
    auto* r = b.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float g = trimGain.getNextValue();
        float x0 = l[i] * g, x1 = r[i] * g;
        if (drive > 0.001f)
        {
            // tanh(a*x)/a has unity small-signal gain; the bias is removed again.
            x0 = (std::tanh (a * x0 + bias) - biasOut) / a;
            x1 = (std::tanh (a * x1 + bias) - biasOut) / a;
            // blend so low drive settings stay subtle
            x0 = x0 * drive + (l[i] * g) * (1.0f - drive);
            x1 = x1 * drive + (r[i] * g) * (1.0f - drive);
        }
        l[i] = x0; r[i] = x1;
    }
}

void ChannelStrip::processMain (AudioBuffer<float>& b, int n)
{
    processFilters (b, n);
    if (p.dynPre->load() > 0.5f) { processDynamics (b, n); processEq (b, n); }
    else                         { processEq (b, n); processDynamics (b, n); }
}

void ChannelStrip::processFilters (AudioBuffer<float>& b, int n)
{
    const float hf = p.hpf->load(), lf = p.lpf->load();
    if (hf != lastHpf)
    {
        lastHpf = hf;
        const bool was = hpfOn;
        hpfOn = hf > 16.5f;
        if (hpfOn) { hpf2.highPass (sr, hf, 0.707); hpf1.set (sr, hf); }
        if (hpfOn && ! was) { hpf2.reset(); hpf1.reset(); }
    }
    if (lf != lastLpf)
    {
        lastLpf = lf;
        const bool was = lpfOn;
        lpfOn = lf < 21900.0f;
        if (lpfOn) lpf2.lowPass (sr, lf, 0.707);
        if (lpfOn && ! was) lpf2.reset();
    }
    if (! hpfOn && ! lpfOn) return;
    for (int c = 0; c < 2; ++c)
    {
        auto* d = b.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            float x = d[i];
            if (hpfOn) x = hpf1.process (hpf2.process (x, c), c);
            if (lpfOn) x = lpf2.process (x, c);
            d[i] = x;
        }
    }
}

void ChannelStrip::processEq (AudioBuffer<float>& b, int n)
{
    if (! eqEnabled()) return;
    const auto s = readEq();
    if (eqDirty || ! (s == lastEq))
    {
        eq.design (s, sr);
        lastEq = s;
        eqDirty = false;
    }
    if (! (eq.hfOn || eq.hmOn || eq.lmOn || eq.lfOn)) return;
    for (int c = 0; c < 2; ++c)
    {
        auto* d = b.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            float x = d[i];
            if (eq.lfOn) x = eq.lf.process (x, c);
            if (eq.lmOn) x = eq.lm.process (x, c);
            if (eq.hmOn) x = eq.hm.process (x, c);
            if (eq.hfOn) x = eq.hf.process (x, c);
            d[i] = x;
        }
    }
}

void ChannelStrip::processDynamics (AudioBuffer<float>& b, int n)
{
    const bool compOn = p.cOn->load() > 0.5f, gateOn = p.gOn->load() > 0.5f;
    if (! compOn) compGr = 0;
    if (! gateOn) gateDb = 0;
    if (! compOn && ! gateOn) return;

    auto coeff = [this] (double sec) { return (float) std::exp (-1.0 / (std::max (1.0e-5, sec) * sr)); };

    // Compressor: feed-forward, stereo linked, soft knee, auto make-up gain.
    const float thr = p.cThr->load();
    const float ratio = p.cRatio->load() >= 19.9f ? 1000.0f : p.cRatio->load();
    const float slope = 1.0f / ratio - 1.0f;
    const float knee = 6.0f;
    const float makeup = Decibels::decibelsToGain (-thr * -slope * 0.4f);
    const float cAtk = coeff (p.cFast->load() > 0.5f ? 0.001 : 0.010);
    const float cRel = coeff (p.cRel->load());

    // Gate / expander
    const float gThr = p.gThr->load(), gRange = p.gRange->load();
    const bool expander = p.gExp->load() > 0.5f;
    const float gAtk = coeff (0.0002), gRel = coeff (p.gRel->load());
    const int holdSamples = (int) (0.02 * sr);

    float maxCompGr = 0, maxGateGr = 0;
    auto* l = b.getWritePointer (0);
    auto* r = b.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float peak = std::max (std::abs (l[i]), std::abs (r[i]));
        const float lvl = Decibels::gainToDecibels (peak, -120.0f);
        float g = 1.0f;

        if (gateOn)
        {
            // Smoothed in dB, so the release time is how long the gate takes to close.
            float targetDb = 0.0f;
            if (lvl < gThr)
            {
                if (expander) targetDb = std::max (-gRange, (lvl - gThr) * 1.0f); // 1:2 expansion
                else if (gateHold > 0) --gateHold;
                else targetDb = -gRange;
            }
            else gateHold = holdSamples;
            const float c = targetDb > gateDb ? gAtk : gRel;
            gateDb = targetDb + c * (gateDb - targetDb);
            g *= Decibels::decibelsToGain (gateDb, -200.0f);
            maxGateGr = std::min (maxGateGr, gateDb);
        }

        if (compOn)
        {
            const float over = lvl - thr;
            float target = 0;
            if (over > knee * 0.5f)        target = slope * over;
            else if (over > -knee * 0.5f)  target = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2 * knee);
            const float c = target < compGr ? cAtk : cRel;
            compGr = target + c * (compGr - target);
            g *= Decibels::decibelsToGain (compGr) * makeup;
            maxCompGr = std::min (maxCompGr, compGr);
        }

        l[i] *= g; r[i] *= g;
    }

    // Keep the deepest reduction until the UI reads it.
    if (maxCompGr < compGrDb.load (std::memory_order_relaxed)) compGrDb.store (maxCompGr, std::memory_order_relaxed);
    if (maxGateGr < gateGrDb.load (std::memory_order_relaxed)) gateGrDb.store (maxGateGr, std::memory_order_relaxed);
}
} // namespace fh
