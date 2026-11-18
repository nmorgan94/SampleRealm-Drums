#pragma once

#include "../PluginProcessor.h"

//==============================================================================
/** The shared WebView editor, with Drums' own functions and events. Their page side is webui/src/plugin/pluginBackend.ts. */
juce::AudioProcessorEditor* createDrumsWebViewEditor (AudioPluginAudioProcessor&);
