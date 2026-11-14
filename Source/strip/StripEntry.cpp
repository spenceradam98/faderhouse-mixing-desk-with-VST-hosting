#include "StripProcessor.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FaderhouseStripProcessor(); }
