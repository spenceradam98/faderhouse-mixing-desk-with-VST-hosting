// Renders PNG screenshots of the console in every theme with a test signal.
// Usage: Snapshots <outDir>   (use xvfb-run on a headless Linux box)
#include "PluginEditor.h"

using namespace juce;
using namespace fh;

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir (argc > 1 ? argv[1] : "shots");
    outDir.createDirectory();
    const double sr = 48000.0; const int bs = 512;

    FaderhouseProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, bs);
    proc.prepareToPlay (sr, bs);

    auto set = [&] (const String& id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    // A plausible session: a few named channels with some EQ and dynamics.
    const char* names[] { "Kick", "Snare", "OH", "Bass", "Gtr L", "Gtr R", "Keys", "Vox" };
    for (int c = 0; c < 8; ++c)
    {
        proc.setChannelName (c, names[c]);
        set (cid (c, "fader"), -2.0f - (float) (c % 4) * 3.0f);
        set (cid (c, "pan"), c == 4 ? -0.7f : (c == 5 ? 0.7f : 0.0f));
    }
    set (cid (0, "hpf"), 30.0f); set (cid (0, "lfG"), 4.0f); set (cid (0, "lfF"), 60.0f); set (cid (0, "hmG"), -5.0f); set (cid (0, "hmF"), 900.0f);
    set (cid (0, "hfG"), 3.0f); set (cid (0, "cOn"), 1.0f); set (cid (0, "cThr"), -20.0f); set (cid (0, "gOn"), 1.0f);
    set (cid (1, "s1"), -8.0f); set (cid (7, "s1"), -4.0f); set (cid (7, "s2"), -10.0f); set (cid (2, "s1"), -12.0f);
    set (cid (7, "cOn"), 1.0f); set (cid (7, "hpf"), 90.0f); set (cid (7, "hmG"), 3.0f);
    set ("bcOn", 1.0f); set ("bcThr", -14.0f);

    AudioBuffer<float> buf (2, bs); MidiBuffer midi; int64 pos = 0;
    auto feed = [&] (FaderhouseEditor& ed)
    {
        for (int k = 0; k < 40; ++k)
        {
            for (int i = 0; i < bs; ++i, ++pos)
            {
                const double t = (double) pos / sr;
                const float v = 0.45f * (float) std::sin (MathConstants<double>::twoPi * 110.0 * t) + 0.2f * (float) std::sin (MathConstants<double>::twoPi * 1320.0 * t);
                buf.setSample (0, i, v); buf.setSample (1, i, v * 0.8f);
            }
            proc.processBlock (buf, midi);
            if (k % 4 == 0) ed.refreshMeters();
        }
    };

    for (int t = 0; t < (int) getThemes().size(); ++t)
    {
        proc.uiState.setProperty ("theme", t, nullptr);
        proc.uiState.setProperty ("scale", 1.0, nullptr);
        proc.uiState.setProperty ("selected", 0, nullptr);
        set (cid (0, "src"), (float) kSourceStereoFirst);
        auto ed = std::make_unique<FaderhouseEditor> (proc);
        // fill every strip's meter: route the input to the first 8 strips
        for (int c = 1; c < 8; ++c) set (cid (c, "src"), (float) kSourceStereoFirst);
        feed (*ed);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile (String (t) + "_" + getTheme (t).name.replace (" ", "") + ".png");
        f.deleteFile();
        FileOutputStream os (f);
        PNGImageFormat().writeImageToStream (img, os);
        std::cout << "wrote " << f.getFullPathName() << "\n";
    }
    proc.releaseResources();
    return 0;
}
