// The page side of these names is webui/src/plugin/juceBackend.ts
#include "DrumsWebView.h"
#include "../PluginEditor.h"
#include "../dsp/PreviewRenderer.h"
#include "../shared/DrumInfoJson.h"
#include <webview_editor/webview_editor.h>

namespace
{
    using Args = juce::Array<juce::var>;
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;
    using Options = juce::WebBrowserComponent::Options;

    // Display units, by DrumEnvelopes::Index
    juce::var envelopesVar (const srd::EnvelopeModel& model)
    {
        juce::Array<juce::var> envelopes;

        for (std::size_t env = 0; env < model.getNumEnvelopes(); ++env)
        {
            juce::Array<juce::var> nodes;

            for (int i = 0; i < model.getNumNodes (env); ++i)
            {
                const auto node = model.getNode (env, i);
                auto object = std::make_unique<juce::DynamicObject>();
                object->setProperty ("time", node.time);
                object->setProperty ("value", node.value);
                object->setProperty ("curve", node.curve);
                nodes.add (juce::var (object.release()));
            }

            envelopes.add (nodes);
        }

        return envelopes;
    }

    std::vector<srd::EnvelopeNode> nodesFrom (const juce::var& list)
    {
        std::vector<srd::EnvelopeNode> nodes;

        if (const auto* array = list.getArray())
            for (const auto& node : *array)
                nodes.push_back ({ node["time"], node["value"], node["curve"] });

        return nodes;
    }

    juce::var lastNotes (const AudioPluginAudioProcessor& processor)
    {
        using Instrument = Parameters::Instrument;

        return juce::Array<juce::var> { processor.getLastNote (Instrument::kick),
                                        processor.getLastNote (Instrument::snare),
                                        processor.getLastNote (Instrument::cymbal) };
    }

    // What one editor's page has seen, and its preview. Kept by the functions the editor holds
    struct Page
    {
        explicit Page (AudioPluginAudioProcessor& p)
            : processor (p),
              lastEnvelopeVersion (p.getEnvelopeModel().getOutsideVersion()),
              lastHitCount (p.getHitCount()),
              lastSentNotes (lastNotes (p))
        {
            previewRenderer.prepare();
        }

        Options withFunctions (Options options)
        {
            return options
                .withNativeFunction ("setEnvelope", [this] (const Args& args, Completion complete)
                {
                    auto& model = processor.getEnvelopeModel();
                    const int env = args[0];

                    if (env >= 0 && std::cmp_less (env, model.getNumEnvelopes()))
                        model.setNodes (static_cast<std::size_t> (env), nodesFrom (args[1]));

                    complete ({});
                })
                .withNativeFunction ("audition", [this] (const Args&, Completion complete)
                {
                    processor.triggerAudition();
                    complete ({});
                })
                // One hit for the waveform, its samples as base64 Float32
                .withNativeFunction ("preview", [this] (const Args& args, Completion complete)
                {
                    const auto preview = previewRenderer.render (processor.getDrumSettings(), processor.getDrumEnvelopes(), args[0], args[1]);

                    auto result = std::make_unique<juce::DynamicObject>();
                    result->setProperty ("samples", juce::Base64::toBase64 (preview.samples, sizeof (float) * static_cast<size_t> (preview.numSamples)));
                    result->setProperty ("sampleRate", PreviewRenderer::sampleRate);
                    result->setProperty ("tailHz", preview.tailHz);
                    complete (juce::var (result.release()));
                })
                .withInitialisationData ("drumInfo", juce::String (drumInfoJson()))
                .withInitialisationData ("envelopes", juce::JSON::toString (envelopesVar (processor.getEnvelopeModel()), true))
                .withInitialisationData ("lastNotes", juce::JSON::toString (lastSentNotes, true));
        }

        void tick (const WebViewEditor::EmitEvent& emit)
        {
            // Presets and state restores; the page's own edits aren't sent back
            auto& model = processor.getEnvelopeModel();

            if (const auto version = model.getOutsideVersion(); version != lastEnvelopeVersion)
            {
                lastEnvelopeVersion = version;
                emit ("envelopes", envelopesVar (model));
            }

            // Before the hit, so the page has the note when it flashes
            if (const auto notes = lastNotes (processor); notes != lastSentNotes)
            {
                lastSentNotes = notes;
                emit ("lastNotes", notes);
            }

            if (const auto hits = processor.getHitCount(); hits != lastHitCount)
            {
                lastHitCount = hits;
                emit ("hit", processor.getLastNote());
            }
        }

        AudioPluginAudioProcessor& processor;
        PreviewRenderer previewRenderer;
        juce::uint32 lastEnvelopeVersion, lastHitCount;
        juce::var lastSentNotes;
    };
}

//==============================================================================
juce::AudioProcessorEditor* createDrumsWebViewEditor (AudioPluginAudioProcessor& processor)
{
    const auto page = std::make_shared<Page> (processor);

    return new WebViewEditor (processor, { .specs = ParameterSpecs::all,
                                           .parameters = processor.getAPVTS(),
                                           .presets = &processor.getPresetManager(),
                                           .meter = &processor.getOutputMeter(),
                                           .width = AudioPluginAudioProcessorEditor::width,
                                           .height = AudioPluginAudioProcessorEditor::height,
                                           .background = CustomLookAndFeel::Palette::background,
                                           .onTick = [page] (const WebViewEditor::EmitEvent& emit) { page->tick (emit); },
                                           .withOptions = [page] (Options options) { return page->withFunctions (std::move (options)); } });
}
