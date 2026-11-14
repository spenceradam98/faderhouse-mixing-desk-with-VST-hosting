#include "Params.h"

namespace fh
{
using namespace juce;

StringArray sourceNames()
{
    StringArray s { "Off" };
    for (int i = 0; i < kNumInputs; ++i) s.add ("In " + String (i + 1));
    for (int i = 0; i < kNumInputs; i += 2) s.add ("In " + String (i + 1) + "/" + String (i + 2));
    s.add ("Instrument");
    return s;
}

String shortSourceName (int src)
{
    if (src == kSourceOff) return "OFF";
    if (src == kSourceInstrument) return "INST";
    if (src < kSourceStereoFirst) return "IN " + String (src);
    const int first = (src - kSourceStereoFirst) * 2 + 1;
    return String (first) + "/" + String (first + 1);
}

bool isStereoSource (int src) { return src >= kSourceStereoFirst; }

const StringArray& busRatioNames()   { static StringArray s { "2:1", "4:1", "10:1" }; return s; }
const StringArray& busAttackNames()  { static StringArray s { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }; return s; }
const StringArray& busReleaseNames() { static StringArray s { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" }; return s; }
float busAttackMs (int i)  { static const float v[] { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f }; return v[jlimit (0, 5, i)]; }
float busReleaseSec (int i) { static const float v[] { 0.1f, 0.3f, 0.6f, 1.2f, -1.0f }; return v[jlimit (0, 4, i)]; }

namespace
{
NormalisableRange<float> freqRange (float lo, float hi)
{
    NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (std::sqrt (lo * hi));
    return r;
}

String hzText (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; }
String dbText (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; }

struct Builder
{
    AudioProcessorValueTreeState::ParameterLayout& layout;

    void flt (const String& id, const String& name, NormalisableRange<float> r, float def,
              std::function<String (float, int)> txt = {}, const String& label = {})
    {
        auto attrs = AudioParameterFloatAttributes().withLabel (label);
        if (txt) attrs = attrs.withStringFromValueFunction (std::move (txt));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, attrs));
    }
    void boolean (const String& id, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    }
    void choice (const String& id, const String& name, const StringArray& c, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def));
    }
};

String faderText (float v, int)
{
    return v <= kFaderMinDb + 0.01f ? String ("-inf") : (v > 0 ? "+" : "") + String (v, 1) + " dB";
}
String sendText (float v, int)
{
    return v <= kSendMinDb + 0.01f ? String ("off") : (v > 0 ? "+" : "") + String (v, 1) + " dB";
}
String panText (float v, int)
{
    const int p = roundToInt (v * 100.0f);
    return p == 0 ? String ("C") : (p < 0 ? "L" + String (-p) : "R" + String (p));
}

void addProcessingParams (Builder& b, int c, const String& n)
{
    // Input
    b.flt (cid (c, "trim"), n + "Trim", { -20.0f, 20.0f }, 0.0f, dbText);
    b.flt (cid (c, "drive"), n + "Drive", { 0.0f, 1.0f }, 0.0f,
           [] (float v, int) { return String (roundToInt (v * 100)) + "%"; });
    b.boolean (cid (c, "phase"), n + "Phase Invert", false);
    b.flt (cid (c, "hpf"), n + "High Pass", freqRange (16.0f, 350.0f), 16.0f,
           [] (float v, int i) { return v <= 16.5f ? String ("off") : hzText (v, i); });
    b.flt (cid (c, "lpf"), n + "Low Pass", freqRange (3000.0f, 22000.0f), 22000.0f,
           [] (float v, int i) { return v >= 21900.0f ? String ("off") : hzText (v, i); });

    // Dynamics
    b.boolean (cid (c, "cOn"), n + "Comp On", false);
    b.flt (cid (c, "cThr"), n + "Comp Threshold", { -40.0f, 0.0f }, -10.0f, dbText);
    b.flt (cid (c, "cRatio"), n + "Comp Ratio", NormalisableRange<float> (1.0f, 20.0f, 0.0f, 0.4f), 4.0f,
           [] (float v, int) { return v >= 19.9f ? String ("inf:1") : String (v, 1) + ":1"; });
    b.flt (cid (c, "cRel"), n + "Comp Release", NormalisableRange<float> (0.05f, 4.0f, 0.0f, 0.4f), 0.3f,
           [] (float v, int) { return String (v, 2) + " s"; });
    b.boolean (cid (c, "cFast"), n + "Comp Fast Attack", false);
    b.boolean (cid (c, "gOn"), n + "Gate On", false);
    b.flt (cid (c, "gThr"), n + "Gate Threshold", { -80.0f, 0.0f }, -50.0f, dbText);
    b.flt (cid (c, "gRange"), n + "Gate Range", { 0.0f, 80.0f }, 40.0f, dbText);
    b.flt (cid (c, "gRel"), n + "Gate Release", NormalisableRange<float> (0.01f, 4.0f, 0.0f, 0.4f), 0.2f,
           [] (float v, int) { return String (v, 2) + " s"; });
    b.boolean (cid (c, "gExp"), n + "Gate Expander Mode", false);
    b.boolean (cid (c, "dynPre"), n + "Dynamics Before EQ", false);

    // EQ
    b.boolean (cid (c, "eqOn"), n + "EQ On", true);
    b.boolean (cid (c, "eqBlk"), n + "EQ Black Mode", false);
    b.flt (cid (c, "hfG"), n + "HF Gain", { -15.0f, 15.0f }, 0.0f, dbText);
    b.flt (cid (c, "hfF"), n + "HF Freq", freqRange (1500.0f, 16000.0f), 8000.0f, hzText);
    b.boolean (cid (c, "hfBell"), n + "HF Bell", false);
    b.flt (cid (c, "hmG"), n + "HMF Gain", { -15.0f, 15.0f }, 0.0f, dbText);
    b.flt (cid (c, "hmF"), n + "HMF Freq", freqRange (600.0f, 7000.0f), 2500.0f, hzText);
    b.flt (cid (c, "hmQ"), n + "HMF Q", NormalisableRange<float> (0.4f, 3.0f, 0.0f, 0.6f), 1.0f);
    b.flt (cid (c, "lmG"), n + "LMF Gain", { -15.0f, 15.0f }, 0.0f, dbText);
    b.flt (cid (c, "lmF"), n + "LMF Freq", freqRange (200.0f, 2500.0f), 600.0f, hzText);
    b.flt (cid (c, "lmQ"), n + "LMF Q", NormalisableRange<float> (0.4f, 3.0f, 0.0f, 0.6f), 1.0f);
    b.flt (cid (c, "lfG"), n + "LF Gain", { -15.0f, 15.0f }, 0.0f, dbText);
    b.flt (cid (c, "lfF"), n + "LF Freq", freqRange (30.0f, 450.0f), 100.0f, hzText);
    b.boolean (cid (c, "lfBell"), n + "LF Bell", false);
}
} // namespace

AudioProcessorValueTreeState::ParameterLayout createLayout (bool standalone)
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    Builder b { layout };
    const auto sources = sourceNames();

    NormalisableRange<float> fader (kFaderMinDb, 12.0f);
    fader.setSkewForCentre (-12.0f);
    NormalisableRange<float> send (kSendMinDb, 10.0f);
    send.setSkewForCentre (-10.0f);

    for (int c = 0; c < kNumChannels; ++c)
    {
        const String n = "Ch" + String (c + 1) + " ";
        // Standalone: each strip starts on its own interface input.
        // Plugin: strip 1 takes the main input, strips 2-8 the optional extra inputs.
        int defSrc = kSourceOff;
        if (standalone)          defSrc = kSourceMonoFirst + c;
        else if (c < kNumInputs / 2) defSrc = kSourceStereoFirst + c;

        b.choice (cid (c, "src"), n + "Source", sources, defSrc);
        b.choice (cid (c, "midi"), n + "MIDI Channel",
                  StringArray { "Omni", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" }, 0);

        addProcessingParams (b, c, n);

        b.boolean (cid (c, "insPre"), n + "Inserts Before Processing", false);

        // Sends, pan, fader
        for (int a = 0; a < kNumAux; ++a)
        {
            const String s = "s" + String (a + 1);
            b.flt (cid (c, s.toRawUTF8()), n + "Send " + auxName (a), send, kSendMinDb, sendText);
            b.boolean (cid (c, (s + "Pre").toRawUTF8()), n + "Send " + auxName (a) + " Pre Fader", false);
        }
        b.flt (cid (c, "pan"), n + "Pan", { -1.0f, 1.0f }, 0.0f, panText);
        b.flt (cid (c, "fader"), n + "Fader", fader, 0.0f, faderText);
        b.boolean (cid (c, "mute"), n + "Mute", false);
        b.boolean (cid (c, "solo"), n + "Solo", false);
    }

    for (int a = 0; a < kNumAux; ++a)
    {
        const String n = "FX " + auxName (a) + " ";
        b.flt (aid (a, "level"), n + "Return", fader, 0.0f, faderText);
        b.flt (aid (a, "pan"), n + "Return Pan", { -1.0f, 1.0f }, 0.0f, panText);
        b.boolean (aid (a, "mute"), n + "Return Mute", false);
    }

    b.boolean ("bcOn", "Bus Comp On", false);
    b.flt ("bcThr", "Bus Comp Threshold", { -30.0f, 0.0f }, -10.0f, dbText);
    b.choice ("bcRatio", "Bus Comp Ratio", busRatioNames(), 1);
    b.choice ("bcAtk", "Bus Comp Attack", busAttackNames(), 3);
    b.choice ("bcRel", "Bus Comp Release", busReleaseNames(), 4);
    b.flt ("bcMakeup", "Bus Comp Makeup", { 0.0f, 15.0f }, 0.0f, dbText);
    b.flt ("bcMix", "Bus Comp Mix", { 0.0f, 1.0f }, 1.0f,
           [] (float v, int) { return String (roundToInt (v * 100)) + "%"; });
    b.flt ("master", "Master Fader", fader, 0.0f, faderText);
    return layout;
}
AudioProcessorValueTreeState::ParameterLayout createStripLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    Builder b { layout };
    addProcessingParams (b, 0, {});
    NormalisableRange<float> fader (kFaderMinDb, 12.0f);
    fader.setSkewForCentre (-12.0f);
    b.flt (cid (0, "pan"), "Pan", { -1.0f, 1.0f }, 0.0f, panText);
    b.flt (cid (0, "fader"), "Fader", fader, 0.0f, faderText);
    b.boolean (cid (0, "mute"), "Mute", false);
    return layout;
}
} // namespace fh
