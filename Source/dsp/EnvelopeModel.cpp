#include "EnvelopeModel.h"
#include <utility>

namespace srd
{
    namespace
    {
        const juce::Identifier envelopesType { "ENVELOPES" };
        const juce::Identifier envType       { "ENV" };
        const juce::Identifier nodeType      { "NODE" };
        const juce::Identifier idProp        { "id" };
        const juce::Identifier timeProp      { "t" };
        const juce::Identifier valueProp     { "v" };
        const juce::Identifier curveProp     { "c" };

        juce::ValueTree makeNode (const EnvelopeNode& n)
        {
            return juce::ValueTree (nodeType, { { timeProp, n.time }, { valueProp, n.value }, { curveProp, n.curve } });
        }

        EnvelopeNode readNode (const juce::ValueTree& node)
        {
            return { node.getProperty (timeProp, 0.0f), node.getProperty (valueProp, 0.0f), node.getProperty (curveProp, 0.0f) };
        }
    }

    //==============================================================================
    EnvelopeModel::EnvelopeModel (juce::AudioProcessorValueTreeState& s, std::vector<EnvelopeSpec> envelopeSpecs)
        : apvts (s), specs (std::move (envelopeSpecs))
    {
        jassert (! specs.empty() && specs.size() <= maxEnvelopes);

        apvts.state.addListener (this);
        reload();
    }

    EnvelopeModel::~EnvelopeModel()
    {
        apvts.state.removeListener (this);
    }

    //==============================================================================
    float EnvelopeModel::toDomain (std::size_t env, float displayValue) const noexcept
    {
        return getSpec (env).scale == EnvelopeSpec::Scale::logarithmic ? std::log2 (displayValue) : displayValue;
    }

    float EnvelopeModel::toDisplay (std::size_t env, float domainValue) const noexcept
    {
        return getSpec (env).scale == EnvelopeSpec::Scale::logarithmic ? std::exp2 (domainValue) : domainValue;
    }

    float EnvelopeModel::clampValue (std::size_t env, float displayValue) const noexcept
    {
        const auto& spec = getSpec (env);
        return juce::jlimit (spec.minValue, spec.maxValue, displayValue);
    }

    EnvelopeNode EnvelopeModel::clampNode (std::size_t env, EnvelopeNode n, float minTime, float maxTime) const noexcept
    {
        n.time  = juce::jlimit (minTime, std::max (minTime, maxTime), n.time);
        n.value = clampValue (env, n.value);
        n.curve = juce::jlimit (-1.0f, 1.0f, n.curve);
        return n;
    }

    //==============================================================================
    juce::ValueTree EnvelopeModel::createDefaultEnv (std::size_t env) const
    {
        juce::ValueTree tree (envType, { { idProp, getSpec (env).id } });

        for (const auto& n : getSpec (env).defaultNodes)
            tree.appendChild (makeNode (n), nullptr);

        return tree;
    }

    bool EnvelopeModel::sanitiseEnv (juce::ValueTree tree, std::size_t env) const
    {
        std::vector<EnvelopeNode> nodes;

        for (const auto& child : tree)
            if (child.hasType (nodeType))
                nodes.push_back (readNode (child));

        if (nodes.size() < 2)
            return false;

        std::stable_sort (nodes.begin(), nodes.end(), [] (auto& a, auto& b) { return a.time < b.time; });
        nodes.resize (std::min (nodes.size(), EnvelopeData::maxNodes));

        nodes[0] = clampNode (env, nodes[0], 0.0f, 0.0f);

        for (std::size_t i = 1; i < nodes.size(); ++i)
            nodes[i] = clampNode (env, nodes[i], nodes[i - 1].time + minGap, getSpec (env).maxTime);

        tree.removeAllChildren (nullptr);

        for (const auto& n : nodes)
            tree.appendChild (makeNode (n), nullptr);

        return true;
    }

    void EnvelopeModel::reload()
    {
        {
            const juce::ScopedValueSetter<bool> svs (ignoreTreeChanges, true);

            auto state = apvts.state;
            auto envelopes = state.getChildWithName (envelopesType);

            if (! envelopes.isValid())
            {
                envelopes = juce::ValueTree (envelopesType);
                state.appendChild (envelopes, nullptr);
            }

            for (std::size_t env = 0; env < getNumEnvelopes(); ++env)
            {
                auto tree = envelopes.getChildWithProperty (idProp, getSpec (env).id);

                if (tree.isValid() && ! sanitiseEnv (tree, env))
                {
                    envelopes.removeChild (tree, nullptr);
                    tree = {};
                }

                if (! tree.isValid())
                    envelopes.appendChild (createDefaultEnv (env), nullptr);
            }
        }

        undoManager.clearUndoHistory();
        publish();
    }

    //==============================================================================
    juce::ValueTree EnvelopeModel::getEnvTree (std::size_t env) const
    {
        return apvts.state.getChildWithName (envelopesType).getChildWithProperty (idProp, getSpec (env).id);
    }

    EnvelopeData EnvelopeModel::getData (std::size_t env) const
    {
        EnvelopeData data;

        for (const auto& child : getEnvTree (env))
        {
            if (data.numNodes >= EnvelopeData::maxNodes)
                break;

            auto node = readNode (child);
            node.value = toDomain (env, clampValue (env, node.value));
            data.nodes[data.numNodes++] = node;
        }

        return data;
    }

    void EnvelopeModel::publish()
    {
        Snapshot snapshot;

        for (std::size_t env = 0; env < getNumEnvelopes(); ++env)
            snapshot[env] = getData (env);

        {
            const juce::SpinLock::ScopedLockType lock (publishLock);
            std::copy_n (snapshot.begin(), getNumEnvelopes(), published.begin());
        }

        for (std::size_t env = 0; env < getNumEnvelopes(); ++env)
            durations[env].store (snapshot[env].getDuration());

        ++version;
    }

    bool EnvelopeModel::tryGetLatest (Snapshot& snapshot, juce::uint32& lastVersion) noexcept
    {
        const auto current = version.load();

        if (current == lastVersion)
            return false;

        const juce::SpinLock::ScopedTryLockType lock (publishLock);

        if (! lock.isLocked())
            return false;

        std::copy_n (published.begin(), getNumEnvelopes(), snapshot.begin());
        lastVersion = current;
        return true;
    }

    //==============================================================================
    int EnvelopeModel::getNumNodes (std::size_t env) const
    {
        return getEnvTree (env).getNumChildren();
    }

    EnvelopeNode EnvelopeModel::getNode (std::size_t env, int index) const
    {
        return readNode (getEnvTree (env).getChild (index));
    }

    void EnvelopeModel::setNode (std::size_t env, int index, EnvelopeNode node)
    {
        auto tree = getEnvTree (env);
        auto nodeTree = tree.getChild (index);

        if (! nodeTree.isValid())
            return;

        // The first node stays at t = 0; the others stay between their neighbours
        const auto isFirst = index == 0;
        const auto isLast  = index == tree.getNumChildren() - 1;
        const auto minTime = isFirst ? 0.0f : readNode (tree.getChild (index - 1)).time + minGap;
        const auto maxTime = isFirst ? 0.0f : isLast ? getSpec (env).maxTime : readNode (tree.getChild (index + 1)).time - minGap;
        node = clampNode (env, node, minTime, maxTime);

        // Three property changes, published once
        {
            const juce::ScopedValueSetter<bool> svs (ignoreTreeChanges, true);
            nodeTree.setProperty (timeProp,  node.time,  &undoManager);
            nodeTree.setProperty (valueProp, node.value, &undoManager);
            nodeTree.setProperty (curveProp, node.curve, &undoManager);
        }

        notifyEdited();
    }

    int EnvelopeModel::insertNode (std::size_t env, EnvelopeNode node)
    {
        auto tree = getEnvTree (env);
        const auto numNodes = tree.getNumChildren();

        if (std::cmp_greater_equal (numNodes, EnvelopeData::maxNodes))
            return -1;

        // New nodes always go between two existing ones: never before the first or after the last
        int index = 1;

        while (index < numNodes && readNode (tree.getChild (index)).time < node.time)
            ++index;

        if (index >= numNodes)
            return -1;

        const auto prevTime = readNode (tree.getChild (index - 1)).time;
        const auto nextTime = readNode (tree.getChild (index)).time;

        if (nextTime - prevTime < 2.0f * minGap)
            return -1;

        node = clampNode (env, node, prevTime + minGap, nextTime - minGap);
        tree.addChild (makeNode (node), index, &undoManager);
        return index;
    }

    bool EnvelopeModel::removeNode (std::size_t env, int index)
    {
        auto tree = getEnvTree (env);

        if (index <= 0 || index >= tree.getNumChildren() - 1)
            return false;

        tree.removeChild (index, &undoManager);
        return true;
    }

    //==============================================================================
    bool EnvelopeModel::isEnvelopeTree (const juce::ValueTree& tree) const
    {
        const auto envelopes = apvts.state.getChildWithName (envelopesType);
        return envelopes.isValid() && (tree == envelopes || tree.isAChildOf (envelopes));
    }

    void EnvelopeModel::notifyEdited()
    {
        publish();

        if (onUserEdit != nullptr)
            onUserEdit();
    }

    // Catches single-step edits (insert, remove) and undo/redo
    void EnvelopeModel::handleTreeChange (const juce::ValueTree& changed)
    {
        if (! ignoreTreeChanges && isEnvelopeTree (changed))
            notifyEdited();
    }

    void EnvelopeModel::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
    {
        if (tree.hasType (nodeType))
            handleTreeChange (tree);
    }

    void EnvelopeModel::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)        { handleTreeChange (parent); }
    void EnvelopeModel::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) { handleTreeChange (parent); }
    void EnvelopeModel::valueTreeChildOrderChanged (juce::ValueTree& parent, int, int)         { handleTreeChange (parent); }

    void EnvelopeModel::valueTreeRedirected (juce::ValueTree&)
    {
        reload();
    }
}
