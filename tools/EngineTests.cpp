// Console engine tests. Usage: EngineTests [TestHalfGain.vst3] [TestSineSynth.vst3]
// Runs the processor the way a DAW would (plugin layout: stereo main input)
// and checks routing, gain staging, EQ, dynamics, sends and plugin hosting.
#include "PluginProcessor.h"
#include "strip/StripProcessor.h"

using namespace juce;
using namespace fh;

static int failures = 0, passes = 0;
static void check (bool ok, const String& what, const String& detail = {})
{
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << (detail.isNotEmpty() ? "  (" + detail + ")" : String()) << "\n";
    (ok ? passes : failures)++;
}

static constexpr double kSr = 48000.0;
static constexpr int kBs = 256;

struct Rig
{
    FaderhouseProcessor p;
    Rig()
    {
        p.setPlayConfigDetails (2, 2, kSr, kBs);
        p.prepareToPlay (kSr, kBs);
    }
    void set (const String& id, float value)
    {
        auto* prm = p.apvts.getParameter (id);
        jassert (prm != nullptr);
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    }
    // Feed a sine (amp, freq) to the main input for `seconds`; return output peak in the last 0.1 s.
    std::pair<float, float> run (float freq, float amp, double seconds = 0.5, MidiBuffer* midiAtStart = nullptr)
    {
        AudioBuffer<float> b (2, kBs);
        const int blocks = (int) (seconds * kSr / kBs);
        const int measureFrom = blocks - (int) (0.1 * kSr / kBs);
        float pl = 0, pr = 0;
        for (int k = 0; k < blocks; ++k)
        {
            for (int i = 0; i < kBs; ++i)
            {
                const float v = amp * (float) std::sin (MathConstants<double>::twoPi * freq * (double) pos / kSr);
                b.setSample (0, i, v); b.setSample (1, i, v);
                ++pos;
            }
            MidiBuffer midi;
            if (k == 0 && midiAtStart != nullptr) midi = *midiAtStart;
            p.processBlock (b, midi);
            if (k >= measureFrom) { pl = jmax (pl, b.getMagnitude (0, 0, kBs)); pr = jmax (pr, b.getMagnitude (1, 0, kBs)); }
        }
        return { pl, pr };
    }
    int64 pos = 0;

    // Feed a unit impulse; return where the output's biggest sample lands and its value.
    // Two silent blocks first, so gain ramps have settled; positions are relative to the impulse.
    std::pair<int, float> impulse (int blocks = 14)
    {
        AudioBuffer<float> b (2, kBs);
        int at = -1; float best = 0;
        for (int k = 0; k < blocks; ++k)
        {
            b.clear();
            if (k == 2) { b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f); }
            MidiBuffer m;
            p.processBlock (b, m);
            for (int i = 0; i < kBs; ++i)
                if (std::abs (b.getSample (0, i)) > best) { best = std::abs (b.getSample (0, i)); at = (k - 2) * kBs + i; }
        }
        return { at, best };
    }
    // Sum of |output| after the main peak, outside +-2 samples: should be ~0 when aligned.
    float leakage (int blocks = 14)
    {
        AudioBuffer<float> b (2, kBs);
        std::vector<float> out;
        for (int k = 0; k < blocks; ++k)
        {
            b.clear();
            if (k == 2) { b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f); }
            MidiBuffer m;
            p.processBlock (b, m);
            if (k >= 2) for (int i = 0; i < kBs; ++i) out.push_back (b.getSample (0, i));
        }
        int at = 0;
        for (int i = 0; i < (int) out.size(); ++i) if (std::abs (out[(size_t) i]) > std::abs (out[(size_t) at])) at = i;
        float sum = 0;
        for (int i = 0; i < (int) out.size(); ++i) if (std::abs (i - at) > 2) sum += std::abs (out[(size_t) i]);
        return sum;
    }
};

static String db (float g) { return String (Decibels::gainToDecibels (g), 2) + " dB"; }
static bool near (float a, float b, float tolDb = 0.2f)
{
    return std::abs (Decibels::gainToDecibels (a, -200.0f) - Decibels::gainToDecibels (b, -200.0f)) <= tolDb;
}

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI gui;

    std::cout << "Routing and gain\n";
    {
        Rig r;
        auto [l, rr] = r.run (1000, 0.5f);
        check (near (l, 0.5f) && near (rr, 0.5f), "default console passes main input at unity", db (l) + " / " + db (rr));
        r.set (cid (0, "fader"), -6.0f);
        auto [l2, r2] = r.run (1000, 0.5f);
        check (near (l2, 0.5f * Decibels::decibelsToGain (-6.0f)), "channel fader -6 dB", db (l2));
        r.set (cid (0, "fader"), 0.0f);
        r.set ("master", -12.0f);
        auto [l3, r3] = r.run (1000, 0.5f);
        check (near (l3, 0.5f * Decibels::decibelsToGain (-12.0f)), "master fader -12 dB", db (l3));
        r.set ("master", 0.0f);
        r.set (cid (0, "pan"), 1.0f);
        auto [l4, r4] = r.run (1000, 0.5f);
        check (l4 < 0.001f && near (r4, 0.5f), "stereo source panned hard right (balance)", db (l4) + " / " + db (r4));
        r.set (cid (0, "pan"), 0.0f);
        r.set (cid (0, "src"), (float) kSourceMonoFirst); // In 1 mono
        auto [l5, r5] = r.run (1000, 0.5f);
        check (near (l5, 0.5f * 0.7071f) && near (r5, 0.5f * 0.7071f), "mono source centred is -3 dB each side", db (l5));
        r.set (cid (0, "src"), (float) kSourceStereoFirst);
        r.set (cid (0, "mute"), 1.0f);
        auto [l6, r6] = r.run (1000, 0.5f);
        check (l6 < 1.0e-4f, "mute cuts the channel", db (l6));
        r.set (cid (0, "mute"), 0.0f);
        r.set (cid (1, "solo"), 1.0f);
        auto [l7, r7] = r.run (1000, 0.5f);
        check (l7 < 1.0e-4f, "solo on another channel silences this one", db (l7));
        r.set (cid (1, "solo"), 0.0f);
        r.set (cid (2, "src"), (float) kSourceStereoFirst); // second strip on the same input
        auto [l8, r8] = r.run (1000, 0.5f);
        check (near (l8, 1.0f), "two strips on one input sum (+6 dB)", db (l8));
        r.set (cid (2, "src"), (float) kSourceOff);
        r.set (cid (0, "trim"), 6.0f);
        auto [l9, r9] = r.run (1000, 0.25f);
        check (near (l9, 0.25f * Decibels::decibelsToGain (6.0f)), "input trim +6 dB", db (l9));
        r.set (cid (0, "trim"), 0.0f);
        r.set (cid (0, "phase"), 1.0f);
        AudioBuffer<float> b (2, kBs);
        for (int k = 0; k < 20; ++k) { for (int i = 0; i < kBs; ++i) { b.setSample (0, i, 0.5f); b.setSample (1, i, 0.5f); } MidiBuffer m; r.p.processBlock (b, m); }
        check (b.getSample (0, kBs - 1) < -0.45f, "phase invert flips polarity", String (b.getSample (0, kBs - 1)));
    }

    std::cout << "Filters and EQ\n";
    {
        Rig r;
        r.set (cid (0, "hmG"), 12.0f); r.set (cid (0, "hmF"), 1000.0f);
        auto [l, _] = r.run (1000, 0.1f);
        check (near (l, 0.1f * Decibels::decibelsToGain (12.0f), 0.5f), "HMF +12 dB at 1 kHz", db (l / 0.1f));
        r.set (cid (0, "eqOn"), 0.0f);
        auto [l2, _2] = r.run (1000, 0.1f);
        check (near (l2, 0.1f), "EQ IN off bypasses the EQ", db (l2 / 0.1f));
        r.set (cid (0, "eqOn"), 1.0f); r.set (cid (0, "hmG"), 0.0f);
        r.set (cid (0, "lfG"), -10.0f); r.set (cid (0, "lfF"), 200.0f);
        auto [l3, _3] = r.run (40, 0.1f);
        check (near (l3, 0.1f * Decibels::decibelsToGain (-10.0f), 0.7f), "LF shelf -10 dB well below the corner", db (l3 / 0.1f));
        r.set (cid (0, "lfG"), 0.0f);
        r.set (cid (0, "hpf"), 200.0f);
        auto [l4, _4] = r.run (50, 0.1f);
        check (Decibels::gainToDecibels (l4 / 0.1f) < -25.0f, "HPF 200 Hz cuts 50 Hz by > 25 dB (18 dB/oct)", db (l4 / 0.1f));
        auto [l5, _5] = r.run (2000, 0.1f);
        check (near (l5, 0.1f, 0.3f), "HPF leaves 2 kHz alone", db (l5 / 0.1f));
        r.set (cid (0, "hpf"), 16.0f); r.set (cid (0, "lpf"), 3000.0f);
        auto [l6, _6] = r.run (15000, 0.1f);
        check (Decibels::gainToDecibels (l6 / 0.1f) < -20.0f, "LPF 3 kHz cuts 15 kHz", db (l6 / 0.1f));

        EqSettings s; s.hmG = 6; s.hmF = 1000; s.hmQ = 1;
        EqDesign brown, black; brown.design (s, kSr); s.black = true; black.design (s, kSr);
        const double bwBrown = brown.magnitude (1500, kSr), bwBlack = black.magnitude (1500, kSr);
        check (bwBlack < bwBrown, "black mode narrows a small boost (proportional Q)", String (bwBrown, 3) + " vs " + String (bwBlack, 3));
    }

    std::cout << "Dynamics\n";
    {
        Rig r;
        r.set (cid (0, "cOn"), 1.0f); r.set (cid (0, "cThr"), -30.0f); r.set (cid (0, "cRatio"), 20.0f);
        r.run (1000, 0.9f, 0.6);
        const float gr = r.p.channels[0].strip.compGrDb.exchange (0.0f);
        check (gr < -20.0f, "compressor at -30 dB, inf:1 reduces a -1 dBFS tone by > 20 dB", String (gr, 1) + " dB GR");
        auto [quiet, _] = r.run (1000, 0.01f, 1.5);
        check (near (quiet, 0.01f * Decibels::decibelsToGain (30.0f * 0.4f * (1 - 1 / 1000.0f)), 0.5f),
               "below threshold only auto make-up is applied", db (quiet / 0.01f));
        r.set (cid (0, "cOn"), 0.0f);
        r.set (cid (0, "gOn"), 1.0f); r.set (cid (0, "gThr"), -30.0f); r.set (cid (0, "gRange"), 60.0f);
        auto [g1, _g1] = r.run (1000, 0.003f, 1.0);
        check (Decibels::gainToDecibels (g1 / 0.003f) < -50.0f, "gate closes on a -50 dBFS signal", db (g1 / 0.003f));
        auto [g2, _g2] = r.run (1000, 0.3f, 0.3);
        check (near (g2, 0.3f, 0.3f), "gate opens on a loud signal", db (g2 / 0.3f));
    }

    std::cout << "Sends, returns, bus compressor\n";
    {
        Rig r;
        r.set (cid (0, "s1"), 0.0f);
        auto [l, _] = r.run (1000, 0.25f);
        check (near (l, 0.5f), "post-fader send A at 0 dB returns at 0 dB (doubles)", db (l));
        r.set ("aux1_mute", 1.0f);
        auto [l2, _2] = r.run (1000, 0.25f);
        check (near (l2, 0.25f), "return mute", db (l2));
        r.set ("aux1_mute", 0.0f);
        r.set (cid (0, "fader"), kFaderMinDb);
        auto [l3, _3] = r.run (1000, 0.25f);
        check (l3 < 1.0e-4f, "post-fader send follows the fader", db (l3));
        r.set (cid (0, "s1Pre"), 1.0f);
        auto [l4, _4] = r.run (1000, 0.25f);
        check (near (l4, 0.25f), "pre-fader send ignores the fader", db (l4));
        r.set (cid (0, "s1Pre"), 0.0f); r.set (cid (0, "fader"), 0.0f); r.set (cid (0, "s1"), kSendMinDb);

        r.set ("bcOn", 1.0f); r.set ("bcThr", -20.0f); r.set ("bcRatio", 2.0f /* 10:1 */);
        auto [b1, _b1] = r.run (1000, 0.5f, 1.0);
        check (Decibels::gainToDecibels (b1 / 0.5f) < -8.0f, "bus compressor 10:1 at -20 dB", db (b1 / 0.5f));
        r.set ("bcMix", 0.0f);
        auto [b2, _b2] = r.run (1000, 0.5f, 0.5);
        check (near (b2, 0.5f), "bus compressor mix 0% is dry", db (b2));
    }

    std::cout << "State\n";
    {
        MemoryBlock state;
        {
            Rig r;
            r.set (cid (4, "hfG"), 7.5f);
            r.set (cid (9, "fader"), -3.0f);
            r.p.setChannelName (2, "Kick");
            r.p.getStateInformation (state);
        }
        Rig r2;
        r2.p.setStateInformation (state.getData(), (int) state.getSize());
        check (std::abs (r2.p.apvts.getRawParameterValue (cid (4, "hfG"))->load() - 7.5f) < 0.01f
               && std::abs (r2.p.apvts.getRawParameterValue (cid (9, "fader"))->load() + 3.0f) < 0.01f
               && r2.p.getChannelName (2) == "Kick",
               "parameters and channel names survive save/restore");
    }

    // Plugin hosting
    const File halfGain (argc > 1 ? argv[1] : ""), sineSynth (argc > 2 ? argv[2] : "");
    auto describe = [] (FaderhouseProcessor& p, const File& f) -> std::unique_ptr<PluginDescription>
    {
        OwnedArray<PluginDescription> found;
        for (auto* fmt : p.library.formatManager.getFormats())
            if (fmt->getName() == "VST3")
                fmt->findAllTypesForFile (found, f.getFullPathName());
        if (found.isEmpty()) return nullptr;
        return std::make_unique<PluginDescription> (*found[0]);
    };

    if (halfGain.exists())
    {
        std::cout << "Hosting (" << halfGain.getFileName() << ")\n";
        MemoryBlock state;
        {
            Rig r;
            auto d = describe (r.p, halfGain);
            check (d != nullptr, "VST3 test effect found");
            if (d)
            {
                String err;
                check (r.p.channels[0].inserts->addPlugin (*d, err), "loads into channel 1 inserts", err);
                auto [l, _] = r.run (1000, 0.5f);
                check (near (l, 0.25f), "channel insert halves the signal", db (l));
                r.p.channels[0].inserts->setBypassed (0, true);
                auto [l2, _2] = r.run (1000, 0.5f);
                check (near (l2, 0.5f), "bypassed insert passes the signal", db (l2));
                r.p.channels[0].inserts->setBypassed (0, false);
                check (r.p.masterInserts->addPlugin (*d, err), "loads into the master inserts", err);
                auto [l3, _3] = r.run (1000, 0.5f);
                check (near (l3, 0.125f), "channel + master inserts in series", db (l3));
                r.p.getStateInformation (state);
            }
        }
        if (state.getSize() > 0)
        {
            Rig r2;
            r2.p.setStateInformation (state.getData(), (int) state.getSize());
            auto [l, _] = r2.run (1000, 0.5f);
            check (r2.p.channels[0].inserts->size() == 1 && r2.p.masterInserts->size() == 1 && near (l, 0.125f),
                   "hosted plugins are restored from saved state",
                   db (l) + ", sizes " + String (r2.p.channels[0].inserts->size()) + "/" + String (r2.p.masterInserts->size()));
        }
    }
    else std::cout << "Hosting: skipped (no TestHalfGain.vst3 given)\n";

    if (sineSynth.exists())
    {
        std::cout << "Instrument hosting (" << sineSynth.getFileName() << ")\n";
        Rig r;
        auto d = describe (r.p, sineSynth);
        check (d != nullptr && d->isInstrument, "VST3 test instrument found and flagged as an instrument");
        if (d)
        {
            String err;
            check (r.p.channels[5].instrument->addPlugin (*d, err), "loads as channel 6's instrument", err);
            r.set (cid (5, "src"), (float) kSourceInstrument);
            r.set (cid (0, "src"), (float) kSourceOff);
            r.set (cid (5, "midi"), 2.0f); // MIDI channel 2
            MidiBuffer on1; on1.addEvent (MidiMessage::noteOn (1, 69, 0.8f), 0);
            auto [a, _a] = r.run (0, 0.0f, 0.3, &on1);
            check (a < 1.0e-4f, "instrument ignores notes on another MIDI channel", db (a));
            MidiBuffer on2; on2.addEvent (MidiMessage::noteOn (2, 69, 0.8f), 0);
            auto [b, _b] = r.run (0, 0.0f, 0.3, &on2);
            check (near (b, 0.25f * 0.7071f * 2.0f / std::sqrt (2.0f), 0.3f) || near (b, 0.25f, 0.3f),
                   "instrument plays a note on its MIDI channel, through the strip", db (b));
        }
    }
    else std::cout << "Instrument hosting: skipped (no TestSineSynth.vst3 given)\n";

    // Latency compensation
    const File delayPlug (argc > 3 ? argv[3] : "");
    if (delayPlug.exists())
    {
        std::cout << "Latency compensation (" << delayPlug.getFileName() << ")\n";
        auto d = [&]
        {
            Rig tmp;
            return describe (tmp.p, delayPlug);
        }();
        check (d != nullptr, "VST3 test delay found");
        if (d)
        {
            String err;
            {
                Rig r;
                r.set (cid (1, "src"), (float) kSourceStereoFirst); // channel 2 on the same input, no plugins
                check (r.p.channels[0].inserts->addPlugin (*d, err), "delay plugin loads on channel 1", err);
                const auto [at, v] = r.impulse();
                check (r.p.pdcSamples.load() == 480, "reports 480 samples of compensation", String (r.p.pdcSamples.load()));
                check (at == 480 && std::abs (v - 2.0f) < 0.01f, "channel 2 is delayed to line up with channel 1 (one impulse, +6 dB)",
                       "peak " + String (v, 3) + " at " + String (at));
                check (r.leakage() < 1.0e-4f, "no early copy of the impulse leaks through");
                r.p.channels[0].inserts->setBypassed (0, true);
                const auto [at2, v2] = r.impulse();
                check (at2 == 0 && std::abs (v2 - 2.0f) < 0.01f && r.p.pdcSamples.load() == 0,
                       "bypassing the plugin removes the compensation", "peak " + String (v2, 3) + " at " + String (at2));
            }
            {
                Rig r;
                r.set (cid (0, "s1"), 0.0f);   // channel 1 sends to FX A, whose return has the delay
                check (r.p.aux[0].chain->addPlugin (*d, err), "delay plugin loads on FX return A", err);
                const auto [at, v] = r.impulse();
                check (at == 480 && std::abs (v - 2.0f) < 0.01f, "dry mix waits for the delayed FX return",
                       "peak " + String (v, 3) + " at " + String (at));
                check (r.leakage() < 1.0e-4f, "dry and return arrive together");
            }
            {
                Rig r;
                check (r.p.masterInserts->addPlugin (*d, err) && r.p.channels[0].inserts->addPlugin (*d, err), "delay on channel 1 and master");
                r.impulse();
                check (r.p.pdcSamples.load() == 960, "channel + master latencies add up", String (r.p.pdcSamples.load()));
            }
        }
    }
    else std::cout << "Latency compensation: skipped (no TestDelay.vst3 given)\n";

    std::cout << "Faderhouse Strip\n";
    {
        FaderhouseStripProcessor sp;
        sp.setPlayConfigDetails (2, 2, kSr, kBs);
        sp.prepareToPlay (kSr, kBs);
        auto setS = [&] (const char* id, float v) { auto* q = sp.apvts.getParameter (cid (0, id)); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
        auto runS = [&] (float freq, float amp)
        {
            AudioBuffer<float> b (2, kBs); float pk = 0;
            for (int k = 0; k < 100; ++k)
            {
                for (int i = 0; i < kBs; ++i)
                {
                    const float v = amp * (float) std::sin (MathConstants<double>::twoPi * freq * (double) (k * kBs + i) / kSr);
                    b.setSample (0, i, v); b.setSample (1, i, v);
                }
                MidiBuffer m; sp.processBlock (b, m);
                if (k > 80) pk = jmax (pk, b.getMagnitude (0, 0, kBs));
            }
            return pk;
        };
        check (near (runS (1000, 0.5f), 0.5f), "strip is unity by default");
        setS ("hmG", 12.0f); setS ("hmF", 1000.0f);
        check (near (runS (1000, 0.1f), 0.1f * Decibels::decibelsToGain (12.0f), 0.5f), "strip EQ +12 dB at 1 kHz");
        setS ("hmG", 0.0f); setS ("fader", -6.0f);
        check (near (runS (1000, 0.5f), 0.25f), "strip fader -6 dB");
        setS ("fader", 0.0f); setS ("mute", 1.0f);
        check (runS (1000, 0.5f) < 1.0e-4f, "strip mute");
        setS ("mute", 0.0f);
        MemoryBlock st; setS ("lfG", -4.0f); sp.getStateInformation (st);
        FaderhouseStripProcessor sp2; sp2.setStateInformation (st.getData(), (int) st.getSize());
        check (std::abs (sp2.apvts.getRawParameterValue (cid (0, "lfG"))->load() + 4.0f) < 0.01f, "strip state round-trips");
        check (sp.getLatencySamples() == 0, "strip adds no latency");
    }

    std::cout << "\n" << passes << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
