#include "EqCurve.h"
#include "Controls.h"

namespace fh
{
using namespace juce;

void EqCurve::paint (Graphics& g)
{
    const auto& t = ConsoleLookAndFeel::themeOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (t.screen); g.fillRoundedRectangle (r, 3.0f);
    r = r.reduced (4, 4);

    const double sr = 48000.0;
    EqDesign eq;
    const bool eqOn = strip.eqEnabled();
    eq.design (strip.readEq(), sr);
    Biquad hp2, lp2; OnePoleHP hp1;
    const float hpfF = strip.p.hpf->load(), lpfF = strip.p.lpf->load();
    const bool hpOn = hpfF > 16.5f, lpOn = lpfF < 21900.0f;
    if (hpOn) { hp2.highPass (sr, hpfF, 0.707); hp1.set (sr, hpfF); }
    if (lpOn) lp2.lowPass (sr, lpfF, 0.707);

    auto xFor = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
    auto yFor = [&] (double db) { return r.getCentreY() - (float) (db / 18.0) * r.getHeight() * 0.5f; };

    g.setColour (t.curve.withAlpha (0.12f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        g.drawVerticalLine ((int) xFor (f), r.getY(), r.getBottom());
    for (double db : { -12.0, -6.0, 6.0, 12.0 })
        g.drawHorizontalLine ((int) yFor (db), r.getX(), r.getRight());
    g.setColour (t.curve.withAlpha (0.3f));
    g.drawHorizontalLine ((int) yFor (0), r.getX(), r.getRight());

    Path p;
    for (int i = 0; i <= 160; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 160.0);
        double m = eqOn ? eq.magnitude (f, sr) : 1.0;
        if (hpOn) m *= hp2.magnitude (f, sr) * hp1.magnitude (f, sr);
        if (lpOn) m *= lp2.magnitude (f, sr);
        const double db = jlimit (-30.0, 30.0, 20.0 * std::log10 (jmax (1.0e-6, m)));
        const float x = xFor (f), y = jlimit (r.getY(), r.getBottom(), yFor (db));
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    Path fill (p);
    fill.lineTo (r.getRight(), yFor (0)); fill.lineTo (r.getX(), yFor (0)); fill.closeSubPath();
    g.setColour (t.curve.withAlpha (0.12f)); g.fillPath (fill);
    g.setColour (eqOn ? t.curve : t.curve.withAlpha (0.4f));
    g.strokePath (p, PathStrokeType (1.8f));

    g.setColour (t.curve.withAlpha (0.5f)); g.setFont (Font (FontOptions (9.0f)));
    g.drawText ("100", (int) xFor (100) - 15, (int) r.getBottom() - 11, 30, 10, Justification::centred);
    g.drawText ("1k", (int) xFor (1000) - 15, (int) r.getBottom() - 11, 30, 10, Justification::centred);
    g.drawText ("10k", (int) xFor (10000) - 15, (int) r.getBottom() - 11, 30, 10, Justification::centred);
}
} // namespace fh
