#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs, ranges and layout for the whole console.
namespace fh
{
constexpr int kNumChannels = 16;   // channel strips
constexpr int kNumAux = 4;         // aux sends / FX returns
constexpr int kNumInputs = 16;     // flattened input channels a strip can pick
constexpr int kSourceOff = 0;
constexpr int kSourceMonoFirst = 1;                            // In 1 .. In 16
constexpr int kSourceStereoFirst = kSourceMonoFirst + kNumInputs; // In 1/2 .. In 15/16
constexpr int kSourceInstrument = kSourceStereoFirst + kNumInputs / 2;
constexpr float kFaderMinDb = -90.0f;  // bottom of fader = off
constexpr float kSendMinDb = -60.0f;   // bottom of send = off

inline juce::String cid (int ch, const char* name) { return "c" + juce::String (ch + 1) + "_" + name; }
inline juce::String aid (int aux, const char* name) { return "aux" + juce::String (aux + 1) + "_" + name; }
inline juce::String auxName (int aux) { return juce::String::charToString ((juce::juce_wchar) ('A' + aux)); }

juce::StringArray sourceNames();
juce::String shortSourceName (int src);
bool isStereoSource (int src);

const juce::StringArray& busRatioNames();   // 2, 4, 10
const juce::StringArray& busAttackNames();  // 0.1 .. 30 ms
const juce::StringArray& busReleaseNames(); // 0.1 .. 1.2 s, Auto
float busAttackMs (int index);
float busReleaseSec (int index); // < 0 means Auto

juce::AudioProcessorValueTreeState::ParameterLayout createLayout (bool standalone);
// The single channel-strip plugin: the processing parameters of channel 1 plus pan, fader, mute.
juce::AudioProcessorValueTreeState::ParameterLayout createStripLayout();
} // namespace fh
