#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BreakpointEnvelope.h"

namespace srd
{
    //==============================================================================
    /** Describes one breakpoint envelope: its range, scale and default shape. */
    struct EnvelopeSpec
    {
        enum class Scale { linear, logarithmic };

        juce::String id;                       // stored as ENV[id=...] in the state tree
        Scale scale     = Scale::linear;       // logarithmic interpolates in log2 (use for Hz)
        float minValue  = 0.0f;
        float maxValue  = 1.0f;
        float maxTime   = 2.0f;                // seconds
        std::vector<EnvelopeNode> defaultNodes; // display units, time-ordered, first at t = 0
    };

    //==============================================================================
    class EnvelopeModel : private juce::ValueTree::Listener
    {
    public:
        static constexpr std::size_t maxEnvelopes = 8;
        using Snapshot = std::array<EnvelopeData, maxEnvelopes>;

        EnvelopeModel (juce::AudioProcessorValueTreeState&, std::vector<EnvelopeSpec>);
        ~EnvelopeModel() override;

        std::size_t getNumEnvelopes() const noexcept           { return specs.size(); }
        const EnvelopeSpec& getSpec (std::size_t env) const    { return specs[env]; }

        /** Validates the envelopes in the current state tree, creating defaults for any
            that are missing, then publishes them. */
        void reload();

        //==============================================================================
        /** Audio thread. Copies the latest envelopes if they changed since lastVersion.
            Returns false, leaving the snapshot untouched, if nothing changed or the lock is busy. */
        bool tryGetLatest (Snapshot&, juce::uint32& lastVersion) noexcept;

        /** Bumped on every publish, so the UI can poll for changes. */
        juce::uint32 getVersion() const noexcept               { return version.load(); }

        /** Seconds until the envelope's last node. Safe from any thread. */
        float getDuration (std::size_t env) const noexcept     { return durations[env].load(); }

        //==============================================================================
        // Message thread editing API. Values are in display units.
        int getNumNodes (std::size_t env) const;
        EnvelopeNode getNode (std::size_t env, int index) const;

        /** Moves a node, keeping it between its neighbours and inside the value range.
            The first node always stays at t = 0. */
        void setNode (std::size_t env, int index, EnvelopeNode);

        /** Inserts a node in time order. Returns its index, or -1 if it can't be added. */
        int insertNode (std::size_t env, EnvelopeNode);

        /** Removes a node. The first and last nodes can't be removed. */
        bool removeNode (std::size_t env, int index);

        /** Converted data for evaluation, in the interpolation domain. */
        EnvelopeData getData (std::size_t env) const;

        juce::UndoManager& getUndoManager() noexcept           { return undoManager; }

        /** Called on the message thread after an edit, but not after reload(). */
        std::function<void()> onUserEdit;

        //==============================================================================
        float toDomain (std::size_t env, float displayValue) const noexcept;
        float toDisplay (std::size_t env, float domainValue) const noexcept;
        float clampValue (std::size_t env, float displayValue) const noexcept;

        static constexpr float minGap = 0.0005f; // seconds between neighbouring nodes

    private:
        juce::AudioProcessorValueTreeState& apvts;
        const std::vector<EnvelopeSpec> specs;
        juce::UndoManager undoManager;

        juce::SpinLock publishLock;
        Snapshot published;
        std::array<std::atomic<float>, maxEnvelopes> durations {};
        std::atomic<juce::uint32> version { 0 };

        // Set while this class changes the tree itself, so it can publish once afterwards
        bool ignoreTreeChanges = false;

        juce::ValueTree getEnvTree (std::size_t env) const;
        juce::ValueTree createDefaultEnv (std::size_t env) const;
        bool sanitiseEnv (juce::ValueTree, std::size_t env) const;
        EnvelopeNode clampNode (std::size_t env, EnvelopeNode, float minTime, float maxTime) const noexcept;
        void publish();
        void notifyEdited();
        void handleTreeChange (const juce::ValueTree& changed);
        bool isEnvelopeTree (const juce::ValueTree&) const;

        void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
        void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override;
        void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override;
        void valueTreeChildOrderChanged (juce::ValueTree& parent, int, int) override;
        void valueTreeRedirected (juce::ValueTree&) override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeModel)
    };
}
