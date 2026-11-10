// The page side of these names is webui/src/plugin/juceBackend.ts
#include "WebViewEditor.h"
#include "../PluginEditor.h"
#include "../shared/ParameterSpecs.h"
#include "../shared/UiInfoJson.h"
#include "../ui/CustomLookAndFeel.h"
#include <WebUIData.h>

#if JUCE_MAC
 #import <Foundation/Foundation.h>
#endif

namespace
{
    using Args = juce::Array<juce::var>;
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

    // As LevelMeter's
    constexpr int updateHz = 30;

    const char* mimeTypeFor (const juce::String& fileName)
    {
        static const std::unordered_map<juce::String, const char*> types {
            { "html", "text/html" },
            { "js",   "text/javascript" },
            { "css",  "text/css" },
            { "ttf",  "font/ttf" },
            { "json", "application/json" }
        };

        const auto it = types.find (fileName.fromLastOccurrenceOf (".", false, false).toLowerCase());
        return it != types.end() ? it->second : "application/octet-stream";
    }

    std::vector<std::byte> readAll (juce::InputStream& stream)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (stream.getTotalLength()));
        stream.read (bytes.data(), static_cast<int> (bytes.size()));
        return bytes;
    }

    struct SinglePageBrowser final : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL == getResourceProviderRoot();
        }
    };

    juce::var toVar (const juce::String& name, const juce::String& category, bool isFactory)
    {
        auto entry = std::make_unique<juce::DynamicObject>();
        entry->setProperty ("name", name);
        entry->setProperty ("category", category);
        entry->setProperty ("isFactory", isFactory);
        return juce::var (entry.release());
    }

    juce::var presetStatus (const srd::PresetManager& presets)
    {
        const auto current = presets.getCurrent();
        auto status = std::make_unique<juce::DynamicObject>();
        status->setProperty ("current", toVar (current.name, current.category, current.isFactory));
        status->setProperty ("modified", presets.isModified());
        return juce::var (status.release());
    }

    // Among the presets listed for the current instrument
    std::optional<srd::PresetManager::Preset> findPreset (const srd::PresetManager& presets, const juce::var& entry)
    {
        const srd::PresetManager::CurrentPreset wanted { entry["name"].toString(), {}, entry["isFactory"] };

        for (const auto& preset : presets.getPresets())
            if (wanted.matches (preset))
                return preset;

        return std::nullopt;
    }

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

    // The Mac web view is white until the page draws, and JUCE has no option for it
    void hideMacWebViewBackground ([[maybe_unused]] juce::WebBrowserComponent& browser)
    {
       #if JUCE_MAC
        for (auto* child : browser.getChildren())
            if (auto* host = dynamic_cast<juce::NSViewComponent*> (child))
                [(id) host->getView() setValue: @NO forKey: @"drawsBackground"];
       #endif
    }
}

//==============================================================================
WebViewEditor::WebViewEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      uiZipStream (WebUIData::WebUI_zip, static_cast<size_t> (WebUIData::WebUI_zipSize), false)
{
    previewRenderer.prepare();

    for (const auto* spec : ParameterSpecs::all)
        relays.push_back (std::make_unique<juce::WebSliderRelay> (spec->id));

    lastPresetChange = processorRef.getPresetManager().getChangeCounter();
    lastEnvelopeVersion = processorRef.getEnvelopeModel().getOutsideVersion();
    lastHitCount = processorRef.getHitCount();
    lastSentNotes = lastNotes();

    browser = std::make_unique<SinglePageBrowser> (createOptions());
    hideMacWebViewBackground (*browser);

    auto& apvts = processorRef.getAPVTS();

    for (size_t i = 0; i < relays.size(); ++i)
        attachments.push_back (std::make_unique<juce::WebSliderParameterAttachment> (
            *apvts.getParameter (ParameterSpecs::all[i]->id), *relays[i], nullptr));

    // Drop anything played while no meter was showing
    processorRef.getOutputMeter().readAndReset();

    addAndMakeVisible (*browser);
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setSize (AudioPluginAudioProcessorEditor::width, AudioPluginAudioProcessorEditor::height);
    startTimerHz (updateHz);
}

juce::var WebViewEditor::lastNotes() const
{
    using Instrument = Parameters::Instrument;

    return juce::Array<juce::var> { processorRef.getLastNote (Instrument::kick),
                                    processorRef.getLastNote (Instrument::snare),
                                    processorRef.getLastNote (Instrument::cymbal) };
}

juce::WebBrowserComponent::Options WebViewEditor::createOptions()
{
    auto options = juce::WebBrowserComponent::Options{}
                       .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                       .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                                    .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory))
                                                    .withBackgroundColour (CustomLookAndFeel::Palette::background))
                       .withNativeIntegrationEnabled()
                       .withResourceProvider ([this] (const auto& url) { return getResource (url); });

    for (auto& relay : relays)
        options = options.withOptionsFrom (*relay);

    return withDrumFunctions (withPresetFunctions (options))
        .withInitialisationData ("uiInfo", juce::String (uiInfoJson (JucePlugin_VersionString, About::commit)))
        .withInitialisationData ("running", About::running (processorRef))
        .withInitialisationData ("presetStatus", juce::JSON::toString (presetStatus (processorRef.getPresetManager()), true))
        .withInitialisationData ("envelopes", juce::JSON::toString (envelopesVar (processorRef.getEnvelopeModel()), true))
        .withInitialisationData ("lastNotes", juce::JSON::toString (lastSentNotes, true));
}

juce::WebBrowserComponent::Options WebViewEditor::withPresetFunctions (const juce::WebBrowserComponent::Options& options)
{
    auto& presets = processorRef.getPresetManager();

    const auto userPresetCall = [&presets] (auto action)
    {
        return [&presets, action] (const Args& args, Completion complete)
        {
            const auto preset = findPreset (presets, args[0]);
            complete (preset.has_value() ? action (*preset, args).getErrorMessage() : juce::String ("That preset no longer exists."));
        };
    };

    return options
        .withNativeFunction ("presetStatus", [&presets] (const Args&, Completion complete)
        {
            complete (presetStatus (presets));
        })
        .withNativeFunction ("presetList", [&presets] (const Args&, Completion complete)
        {
            presets.refreshUserPresets();

            juce::Array<juce::var> list;

            for (const auto& preset : presets.getPresets())
                list.add (toVar (preset.name, preset.category, preset.isFactory));

            complete (list);
        })
        .withNativeFunction ("presetLoad", [&presets] (const Args& args, Completion complete)
        {
            if (const auto preset = findPreset (presets, args[0]))
                presets.loadPreset (*preset);

            complete ({});
        })
        .withNativeFunction ("presetInit", [&presets] (const Args&, Completion complete)
        {
            presets.loadInit();
            complete ({});
        })
        .withNativeFunction ("presetStep", [&presets] (const Args& args, Completion complete)
        {
            if (const int step = args[0]; step > 0)
                presets.loadNext();
            else
                presets.loadPrevious();

            complete ({});
        })
        .withNativeFunction ("presetExists", [&presets] (const Args& args, Completion complete)
        {
            complete (presets.userPresetExists (args[0].toString()));
        })
        .withNativeFunction ("presetSave", [&presets] (const Args& args, Completion complete)
        {
            complete (presets.saveUserPreset (args[0].toString(), args[1].toString()).getErrorMessage());
        })
        .withNativeFunction ("presetRename", userPresetCall ([&presets] (const srd::PresetManager::Preset& preset, const Args& args)
        {
            return presets.renameUserPreset (preset, args[1].toString());
        }))
        .withNativeFunction ("presetDelete", userPresetCall ([&presets] (const srd::PresetManager::Preset& preset, const Args&)
        {
            return presets.deleteUserPreset (preset);
        }))
        .withNativeFunction ("presetShowFolder", [&presets] (const Args&, Completion complete)
        {
            presets.showUserDirectory();
            complete ({});
        });
}

juce::WebBrowserComponent::Options WebViewEditor::withDrumFunctions (const juce::WebBrowserComponent::Options& options)
{
    return options
        .withNativeFunction ("setEnvelope", [this] (const Args& args, Completion complete)
        {
            auto& model = processorRef.getEnvelopeModel();
            const int env = args[0];

            if (env >= 0 && std::cmp_less (env, model.getNumEnvelopes()))
                model.setNodes (static_cast<std::size_t> (env), nodesFrom (args[1]));

            complete ({});
        })
        .withNativeFunction ("audition", [this] (const Args&, Completion complete)
        {
            processorRef.triggerAudition();
            complete ({});
        })
        // One hit for the waveform, its samples as base64 Float32
        .withNativeFunction ("preview", [this] (const Args& args, Completion complete)
        {
            const auto preview = previewRenderer.render (processorRef.getDrumSettings(), processorRef.getDrumEnvelopes(), args[0], args[1]);

            auto result = std::make_unique<juce::DynamicObject>();
            result->setProperty ("samples", juce::Base64::toBase64 (preview.samples, sizeof (float) * static_cast<size_t> (preview.numSamples)));
            result->setProperty ("sampleRate", PreviewRenderer::sampleRate);
            result->setProperty ("tailHz", preview.tailHz);
            complete (juce::var (result.release()));
        });
}

std::optional<juce::WebBrowserComponent::Resource> WebViewEditor::getResource (const juce::String& url)
{
    const auto path = url == "/" ? juce::String ("index.html") : url.fromFirstOccurrenceOf ("/", false, false);

    if (auto* entry = uiZip.getEntry (path))
    {
        const std::unique_ptr<juce::InputStream> stream (uiZip.createStreamForEntry (*entry));

        if (stream != nullptr)
            return juce::WebBrowserComponent::Resource { readAll (*stream), mimeTypeFor (path) };
    }

    return std::nullopt;
}

void WebViewEditor::timerCallback()
{
    // The page's meter falls on its own timer
    if (const auto peak = processorRef.getOutputMeter().readAndReset(); peak > 0.0f)
    {
        auto level = std::make_unique<juce::DynamicObject>();
        level->setProperty ("peaks", juce::Array<juce::var> { peak });
        browser->emitEventIfBrowserIsVisible ("level", juce::var (level.release()));
    }

    if (! browser->isShowing())
        return;

    auto& presets = processorRef.getPresetManager();

    if (const auto change = presets.getChangeCounter(); change != lastPresetChange)
    {
        lastPresetChange = change;
        browser->emitEventIfBrowserIsVisible ("presetStatus", presetStatus (presets));
    }

    // Presets and state restores; the page's own edits aren't sent back
    auto& model = processorRef.getEnvelopeModel();

    if (const auto version = model.getOutsideVersion(); version != lastEnvelopeVersion)
    {
        lastEnvelopeVersion = version;
        browser->emitEventIfBrowserIsVisible ("envelopes", envelopesVar (model));
    }

    // Before the hit, so the page has the note when it flashes
    if (const auto notes = lastNotes(); notes != lastSentNotes)
    {
        lastSentNotes = notes;
        browser->emitEventIfBrowserIsVisible ("lastNotes", notes);
    }

    if (const auto hits = processorRef.getHitCount(); hits != lastHitCount)
    {
        lastHitCount = hits;
        browser->emitEventIfBrowserIsVisible ("hit", processorRef.getLastNote());
    }
}

void WebViewEditor::paint (juce::Graphics& g)
{
    g.fillAll (CustomLookAndFeel::Palette::background);
}

void WebViewEditor::resized()
{
    browser->setBounds (getLocalBounds());
}
