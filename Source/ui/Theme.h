#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace fh
{
// A console colour scheme. Knob caps follow the classic console convention
// (red HF, green HMF, blue LMF, dark LF, etc.), so each theme sets them too.
struct Theme
{
    juce::String name;
    juce::Colour panel, section, sectionEdge, text, textDim, accent;
    juce::Colour knobBody, knobPointer;
    juce::Colour capInput, capFilter, capDyn, capGate, capHF, capHMF, capLMF, capLF, capSend, capPan;
    juce::Colour faderSlot, faderCap, faderLine, scale;
    juce::Colour buttonOff, buttonText, ledOn, mute, solo, select;
    juce::Colour screen, curve, meterLow, meterMid, meterHigh, meterOff;
    bool flatKnobs = false;   // modern flat knobs with value arcs instead of caps
    bool bevel = true;        // raised section panels and screws
};

inline juce::Colour hex (juce::uint32 rgb, float a = 1.0f) { return juce::Colour (0xff000000 | rgb).withAlpha (a); }

inline const std::vector<Theme>& getThemes()
{
    static const std::vector<Theme> themes = []
    {
        std::vector<Theme> v;
        { // Classic large-format console grey with coloured caps
            Theme t; t.name = "Studio Grey";
            t.panel = hex (0x8d969e); t.section = hex (0x9ea7af); t.sectionEdge = hex (0x5d656c);
            t.text = hex (0x15181b); t.textDim = hex (0x3c4248); t.accent = hex (0xd23b2a);
            t.knobBody = hex (0x2a2d30); t.knobPointer = hex (0xf4f4f0);
            t.capInput = hex (0xe8e2d0); t.capFilter = hex (0x6b6f73); t.capDyn = hex (0xf0f0ea); t.capGate = hex (0x3f8f4f);
            t.capHF = hex (0xc8372b); t.capHMF = hex (0x2f8a46); t.capLMF = hex (0x2d5fa8); t.capLF = hex (0x6b4a2e);
            t.capSend = hex (0xe0b42c); t.capPan = hex (0xd0d0cc);
            t.faderSlot = hex (0x1b1d1f); t.faderCap = hex (0x2b2b2b); t.faderLine = hex (0xf0f0f0); t.scale = hex (0x1d2023);
            t.buttonOff = hex (0xc8ccd0); t.buttonText = hex (0x111111); t.ledOn = hex (0xffd23a);
            t.mute = hex (0xe8452f); t.solo = hex (0xf4c430); t.select = hex (0x47a0ff);
            t.screen = hex (0x10171c); t.curve = hex (0xffb347);
            t.meterLow = hex (0x3ee05a); t.meterMid = hex (0xf4d03a); t.meterHigh = hex (0xff3b2a); t.meterOff = hex (0x1a1f22);
            v.push_back (t);
        }
        { // Black-finish console
            Theme t; t.name = "Black Session";
            t.panel = hex (0x1d1f22); t.section = hex (0x26292d); t.sectionEdge = hex (0x0c0d0e);
            t.text = hex (0xe6e6e2); t.textDim = hex (0x8e949a); t.accent = hex (0xff5a3c);
            t.knobBody = hex (0x0f1011); t.knobPointer = hex (0xffffff);
            t.capInput = hex (0xd9d4c4); t.capFilter = hex (0x8a8f94); t.capDyn = hex (0xe9e9e9); t.capGate = hex (0x48a85a);
            t.capHF = hex (0xd8402f); t.capHMF = hex (0x33a052); t.capLMF = hex (0x3772c8); t.capLF = hex (0x9a7148);
            t.capSend = hex (0xf0c330); t.capPan = hex (0xbfc3c7);
            t.faderSlot = hex (0x060606); t.faderCap = hex (0x3a3d40); t.faderLine = hex (0xff5a3c); t.scale = hex (0xa9aeb3);
            t.buttonOff = hex (0x3a3e43); t.buttonText = hex (0xe8e8e8); t.ledOn = hex (0xffc83d);
            t.mute = hex (0xff4b33); t.solo = hex (0xffc83d); t.select = hex (0x4aa8ff);
            t.screen = hex (0x050607); t.curve = hex (0x5fe0ff);
            t.meterLow = hex (0x37e05a); t.meterMid = hex (0xffd23a); t.meterHigh = hex (0xff3b2a); t.meterOff = hex (0x121416);
            v.push_back (t);
        }
        { // Vintage cream and brown
            Theme t; t.name = "Cream Classic";
            t.panel = hex (0xd9cfb8); t.section = hex (0xe6dcc5); t.sectionEdge = hex (0x8f8167);
            t.text = hex (0x2a2118); t.textDim = hex (0x5f5243); t.accent = hex (0xa8321f);
            t.knobBody = hex (0x3a2a1e); t.knobPointer = hex (0xf6eedb);
            t.capInput = hex (0xb8442c); t.capFilter = hex (0x6a5d4c); t.capDyn = hex (0x6b6b6b); t.capGate = hex (0x56704a);
            t.capHF = hex (0xb0402e); t.capHMF = hex (0x4f7d50); t.capLMF = hex (0x3e5f8a); t.capLF = hex (0x5a3f28);
            t.capSend = hex (0xc4902c); t.capPan = hex (0x8a7f6c);
            t.faderSlot = hex (0x2a2018); t.faderCap = hex (0x4a3a2c); t.faderLine = hex (0xf6eedb); t.scale = hex (0x2a2118);
            t.buttonOff = hex (0xc4b89c); t.buttonText = hex (0x241b12); t.ledOn = hex (0xffb03a);
            t.mute = hex (0xc8402c); t.solo = hex (0xe8b030); t.select = hex (0x3d7cc0);
            t.screen = hex (0x1e1812); t.curve = hex (0xffc26b);
            t.meterLow = hex (0xffb03a); t.meterMid = hex (0xffd27a); t.meterHigh = hex (0xff5533); t.meterOff = hex (0x2a2118);
            v.push_back (t);
        }
        { // Midnight blue broadcast desk
            Theme t; t.name = "Midnight";
            t.panel = hex (0x18223a); t.section = hex (0x1f2b48); t.sectionEdge = hex (0x0b1120);
            t.text = hex (0xdfe8ff); t.textDim = hex (0x8394bd); t.accent = hex (0x6fd3ff);
            t.knobBody = hex (0x0c1222); t.knobPointer = hex (0x9fe6ff);
            t.capInput = hex (0x8fb0e0); t.capFilter = hex (0x5f6f90); t.capDyn = hex (0xb9c6e0); t.capGate = hex (0x4ac08a);
            t.capHF = hex (0xff6a7a); t.capHMF = hex (0x4fd18b); t.capLMF = hex (0x5aa0ff); t.capLF = hex (0xc49a6c);
            t.capSend = hex (0xffd166); t.capPan = hex (0xaebde0);
            t.faderSlot = hex (0x060a14); t.faderCap = hex (0x2c3a5c); t.faderLine = hex (0x6fd3ff); t.scale = hex (0x8394bd);
            t.buttonOff = hex (0x2a3858); t.buttonText = hex (0xdfe8ff); t.ledOn = hex (0x6fd3ff);
            t.mute = hex (0xff5a6a); t.solo = hex (0xffd166); t.select = hex (0x6fd3ff);
            t.screen = hex (0x050914); t.curve = hex (0x6fd3ff);
            t.meterLow = hex (0x3fe0c5); t.meterMid = hex (0x9fe6ff); t.meterHigh = hex (0xff5a6a); t.meterOff = hex (0x0d1426);
            t.flatKnobs = true;
            v.push_back (t);
        }
        { // Modern flat, light
            Theme t; t.name = "Daylight";
            t.panel = hex (0xeceef0); t.section = hex (0xffffff); t.sectionEdge = hex (0xd0d4d8);
            t.text = hex (0x1d2329); t.textDim = hex (0x6b7580); t.accent = hex (0xff6b2c);
            t.knobBody = hex (0xdfe3e7); t.knobPointer = hex (0x1d2329);
            t.capInput = hex (0x8a94a0); t.capFilter = hex (0x8a94a0); t.capDyn = hex (0x5a6570); t.capGate = hex (0x28a86b);
            t.capHF = hex (0xe5484d); t.capHMF = hex (0x30a46c); t.capLMF = hex (0x3e7bfa); t.capLF = hex (0xad7f58);
            t.capSend = hex (0xf5a524); t.capPan = hex (0x5a6570);
            t.faderSlot = hex (0xc9ced3); t.faderCap = hex (0xffffff); t.faderLine = hex (0xff6b2c); t.scale = hex (0x6b7580);
            t.buttonOff = hex (0xe1e5e9); t.buttonText = hex (0x1d2329); t.ledOn = hex (0xff6b2c);
            t.mute = hex (0xe5484d); t.solo = hex (0xf5a524); t.select = hex (0x3e7bfa);
            t.screen = hex (0x1d2329); t.curve = hex (0xff8a4c);
            t.meterLow = hex (0x30a46c); t.meterMid = hex (0xf5a524); t.meterHigh = hex (0xe5484d); t.meterOff = hex (0xd9dde1);
            t.flatKnobs = true; t.bevel = false;
            v.push_back (t);
        }
        return v;
    }();
    return themes;
}

inline const Theme& getTheme (int i)
{
    const auto& t = getThemes();
    return t[(size_t) juce::jlimit (0, (int) t.size() - 1, i)];
}
} // namespace fh
