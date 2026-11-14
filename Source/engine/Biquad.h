#pragma once
#include <cmath>
#include <complex>

namespace fh
{
// RBJ cookbook biquad, transposed direct form II, two channels.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] {}, z2[2] {};

    inline float process (float x, int c) noexcept
    {
        const double y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return (float) y;
    }
    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    void setIdentity() noexcept { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }

    static double w0 (double f, double sr) { return 2.0 * 3.14159265358979323846 * std::fmin (f, sr * 0.49) / sr; }

    void highPass (double sr, double f, double q)
    {
        const double w = w0 (f, sr), cs = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
    void lowPass (double sr, double f, double q)
    {
        const double w = w0 (f, sr), cs = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
    void peak (double sr, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = w0 (f, sr), cs = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * cs, 1 - al * A, 1 + al / A, -2 * cs, 1 - al / A);
    }
    void lowShelf (double sr, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = w0 (f, sr), cs = std::cos (w), al = std::sin (w) / (2 * q);
        const double sA = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * cs + sA), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - sA),
             (A + 1) + (A - 1) * cs + sA, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - sA);
    }
    void highShelf (double sr, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = w0 (f, sr), cs = std::cos (w), al = std::sin (w) / (2 * q);
        const double sA = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * cs + sA), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - sA),
             (A + 1) - (A - 1) * cs + sA, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - sA);
    }

    double magnitude (double f, double sr) const
    {
        const auto z = std::polar (1.0, -w0 (f, sr));
        const auto z2 = z * z;
        return std::abs ((b0 + b1 * z + b2 * z2) / (1.0 + a1 * z + a2 * z2));
    }
};

// One-pole high pass (adds 6 dB/oct to the 12 dB/oct biquad for an 18 dB/oct HPF).
struct OnePoleHP
{
    float a = 0, x1[2] {}, y1[2] {};
    void set (double sr, double f) { a = (float) std::exp (-2.0 * 3.14159265358979323846 * f / sr); }
    void reset() { x1[0] = x1[1] = y1[0] = y1[1] = 0; }
    inline float process (float x, int c) noexcept
    {
        const float y = a * (y1[c] + x - x1[c]);
        x1[c] = x; y1[c] = y;
        return y;
    }
    double magnitude (double f, double sr) const
    {
        const auto z = std::polar (1.0, -2.0 * 3.14159265358979323846 * f / sr);
        return std::abs ((double) a * (1.0 - z) / (1.0 - (double) a * z));
    }
};

// Four-band channel EQ design shared by the audio path and the EQ curve display.
struct EqSettings
{
    bool black = false;
    float hfG = 0, hfF = 8000, lfG = 0, lfF = 100, hmG = 0, hmF = 2500, hmQ = 1, lmG = 0, lmF = 600, lmQ = 1;
    bool hfBell = false, lfBell = false;
    bool operator== (const EqSettings&) const = default;
};

struct EqDesign
{
    Biquad hf, hm, lm, lf;
    bool hfOn = false, hmOn = false, lmOn = false, lfOn = false;

    // "Brown" mode keeps Q constant. "Black" mode narrows the bells as gain rises
    // (proportional Q) and gives the shelves a little overshoot.
    void design (const EqSettings& s, double sr)
    {
        auto pq = [&] (float q, float g) { return s.black ? q * (0.6f + std::fabs (g) / 15.0f * 1.6f) : q; };
        const double shelfQ = s.black ? 1.0 : 0.707;
        hfOn = std::fabs (s.hfG) > 0.01f; hmOn = std::fabs (s.hmG) > 0.01f;
        lmOn = std::fabs (s.lmG) > 0.01f; lfOn = std::fabs (s.lfG) > 0.01f;
        if (s.hfBell) hf.peak (sr, s.hfF, pq (1.2f, s.hfG), s.hfG); else hf.highShelf (sr, s.hfF, shelfQ, s.hfG);
        if (s.lfBell) lf.peak (sr, s.lfF, pq (1.2f, s.lfG), s.lfG); else lf.lowShelf (sr, s.lfF, shelfQ, s.lfG);
        hm.peak (sr, s.hmF, pq (s.hmQ, s.hmG), s.hmG);
        lm.peak (sr, s.lmF, pq (s.lmQ, s.lmG), s.lmG);
    }

    double magnitude (double f, double sr) const
    {
        double m = 1.0;
        if (hfOn) m *= hf.magnitude (f, sr);
        if (hmOn) m *= hm.magnitude (f, sr);
        if (lmOn) m *= lm.magnitude (f, sr);
        if (lfOn) m *= lf.magnitude (f, sr);
        return m;
    }
};
} // namespace fh
