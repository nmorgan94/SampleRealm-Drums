#pragma once

#include "../PluginProcessor.h"

//==============================================================================
/** The shared WebView editor, with Drums' own functions and events. Their page side is webui/src/plugin/juceBackend.ts. */
juce::AudioProcessorEditor* createDrumsWebViewEditor (AudioPluginAudioProcessor&);
