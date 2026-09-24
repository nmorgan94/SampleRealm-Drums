#include "PresetManager.h"

namespace srd
{
    namespace
    {
        const juce::Identifier presetTag      { "PRESET" };
        const juce::Identifier nameAttr       { "name" };
        const juce::Identifier categoryAttr   { "category" };
        const juce::Identifier authorAttr     { "author" };
        const juce::Identifier versionAttr    { "formatVersion" };

        // Stored on the saved session state only, never inside preset files
        const juce::Identifier presetNameProp     { "presetName" };
        const juce::Identifier presetCategoryProp { "presetCategory" };
        const juce::Identifier presetFactoryProp  { "presetIsFactory" };
        const juce::Identifier presetModifiedProp { "presetModified" };

        void sortPresets (juce::Array<PresetManager::Preset>& presets)
        {
            std::stable_sort (presets.begin(), presets.end(), [] (const auto& a, const auto& b)
            {
                if (a.category != b.category)
                    return a.category.compareNatural (b.category) < 0;

                return a.name.compareNatural (b.name) < 0;
            });
        }
    }

    //==============================================================================
    PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, Config c)
        : apvts (s), config (std::move (c))
    {
        for (std::size_t i = 0; i < config.factoryPresets.size(); ++i)
        {
            // Only the outer element is parsed, which holds the metadata but not the whole state
            if (auto xml = juce::XmlDocument (config.factoryPresets[i].toString()).getDocumentElement (true); xml != nullptr && xml->hasTagName (presetTag.toString()))
                factoryPresets.add ({ xml->getStringAttribute (nameAttr), xml->getStringAttribute (categoryAttr), true, i, {} });
            else
                jassertfalse; // a factory preset failed to parse
        }

        sortPresets (factoryPresets);

        apvts.processor.addListener (this);
    }

    PresetManager::~PresetManager()
    {
        apvts.processor.removeListener (this);
    }

    juce::File PresetManager::getDefaultUserDirectory (const juce::String& company, const juce::String& product)
    {
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

       #if JUCE_MAC
        base = base.getChildFile ("Application Support");
       #endif

        return base.getChildFile (company).getChildFile (product).getChildFile ("Presets");
    }

    //==============================================================================
    const juce::Array<PresetManager::Preset>& PresetManager::getUserPresets() const
    {
        if (! userPresetsScanned)
            scanUserPresets();

        return userPresets;
    }

    void PresetManager::refreshUserPresets()
    {
        scanUserPresets();
        ++changeCounter;
    }

    void PresetManager::scanUserPresets() const
    {
        userPresetsScanned = true;
        userPresets.clearQuick();

        for (const auto& file : config.userDirectory.findChildFiles (juce::File::findFiles, false, "*" + config.fileExtension))
        {
            Preset preset { file.getFileNameWithoutExtension(), {}, false, 0, file };

            // Only the outer element is parsed, which holds the metadata but not the whole state
            if (auto xml = juce::XmlDocument (file).getDocumentElement (true); xml != nullptr && xml->hasTagName (presetTag.toString()))
                preset.category = xml->getStringAttribute (categoryAttr);

            userPresets.add (preset);
        }

        sortPresets (userPresets);
    }

    juce::Array<PresetManager::Preset> PresetManager::getAllPresets() const
    {
        auto all = factoryPresets;
        all.addArray (getUserPresets());
        return all;
    }

    juce::File PresetManager::getUserFile (const juce::String& name) const
    {
        return config.userDirectory.getChildFile (juce::File::createLegalFileName (name.trim()) + config.fileExtension);
    }

    bool PresetManager::userPresetExists (const juce::String& name) const
    {
        return getUserFile (name).existsAsFile();
    }

    /** User presets can't share a factory preset's name, so the two never look alike in a list. */
    juce::Result PresetManager::checkUserName (const juce::String& trimmedName) const
    {
        const auto listedName = juce::File::createLegalFileName (trimmedName);

        if (listedName.isEmpty())
            return juce::Result::fail ("Enter a preset name.");

        for (const auto& preset : factoryPresets)
            if (preset.name.equalsIgnoreCase (listedName))
                return juce::Result::fail ("\"" + preset.name + "\" is a factory preset. Choose another name.");

        return juce::Result::ok();
    }

    //==============================================================================
    bool PresetManager::loadPreset (const Preset& preset)
    {
        std::unique_ptr<juce::XmlElement> xml;

        if (! preset.isFactory)
            xml = juce::XmlDocument::parse (preset.file);
        else if (preset.factoryIndex < config.factoryPresets.size())
            xml = juce::parseXML (config.factoryPresets[preset.factoryIndex].toString());

        if (xml == nullptr || ! xml->hasTagName (presetTag.toString()))
            return false;

        auto* stateXml = xml->getChildByName (apvts.state.getType());

        if (stateXml == nullptr)
            return false;

        restoreState (juce::ValueTree::fromXml (*stateXml));

        // A user preset's name comes from its filename, so renames made outside the plugin still show
        setCurrent ({ preset.name, preset.category, preset.isFactory }, false);
        return true;
    }

    bool PresetManager::step (int delta)
    {
        const auto all = getAllPresets();

        if (all.isEmpty())
            return false;

        const auto now = getCurrent();

        for (int i = 0; i < all.size(); ++i)
            if (now.matches (all[i]))
                return loadPreset (all[juce::negativeAwareModulo (i + delta, all.size())]);

        return loadPreset (delta > 0 ? all.getFirst() : all.getLast());
    }

    bool PresetManager::loadNext()      { return step (1); }
    bool PresetManager::loadPrevious()  { return step (-1); }

    //==============================================================================
    std::unique_ptr<juce::XmlElement> PresetManager::createPresetXml (const juce::String& name, const juce::String& category) const
    {
        auto state = apvts.copyState();
        auto xml = std::make_unique<juce::XmlElement> (presetTag);
        xml->setAttribute (nameAttr, name);
        xml->setAttribute (categoryAttr, category);
        xml->setAttribute (authorAttr, config.author);
        xml->setAttribute (versionAttr, config.formatVersion);

        if (auto stateXml = state.createXml())
            xml->addChildElement (stateXml.release());

        return xml;
    }

    juce::Result PresetManager::saveUserPreset (const juce::String& name, const juce::String& category)
    {
        const auto trimmed = name.trim();

        if (const auto check = checkUserName (trimmed); check.failed())
            return check;

        if (const auto created = config.userDirectory.createDirectory(); created.failed())
            return created;

        const auto file = getUserFile (trimmed);

        if (! createPresetXml (trimmed, category)->writeTo (file))
            return juce::Result::fail ("Couldn't write " + file.getFullPathName());

        refreshUserPresets();
        setCurrent ({ file.getFileNameWithoutExtension(), category, false }, false);
        return juce::Result::ok();
    }

    juce::Result PresetManager::renameUserPreset (const Preset& preset, const juce::String& newName)
    {
        const auto trimmed = newName.trim();

        if (preset.isFactory || ! preset.file.existsAsFile())
            return juce::Result::fail ("Only user presets can be renamed.");

        if (const auto check = checkUserName (trimmed); check.failed())
            return check;

        const auto target = getUserFile (trimmed);

        if (target != preset.file && target.exists())
            return juce::Result::fail ("A preset called \"" + trimmed + "\" already exists.");

        auto xml = juce::XmlDocument::parse (preset.file);

        if (xml == nullptr)
            return juce::Result::fail ("Couldn't read " + preset.file.getFullPathName());

        xml->setAttribute (nameAttr, trimmed);

        if (! xml->writeTo (preset.file))
            return juce::Result::fail ("Couldn't write " + preset.file.getFullPathName());

        // Compared as strings: File comparison ignores case on macOS, which would skip case-only renames
        if (target.getFullPathName() != preset.file.getFullPathName() && ! preset.file.moveFileTo (target))
            return juce::Result::fail ("Couldn't rename " + preset.file.getFullPathName());

        refreshUserPresets();

        if (const auto now = getCurrent(); now.matches (preset))
            setCurrent ({ target.getFileNameWithoutExtension(), now.category, false }, isModified());

        return juce::Result::ok();
    }

    juce::Result PresetManager::deleteUserPreset (const Preset& preset)
    {
        if (preset.isFactory || ! preset.file.existsAsFile())
            return juce::Result::fail ("Only user presets can be deleted.");

        if (! preset.file.isAChildOf (config.userDirectory))
            return juce::Result::fail ("That file isn't in the user preset folder.");

        if (! preset.file.moveToTrash() && ! preset.file.deleteFile())
            return juce::Result::fail ("Couldn't delete " + preset.file.getFullPathName());

        refreshUserPresets();

        if (getCurrent().matches (preset))
            markModified();

        return juce::Result::ok();
    }

    //==============================================================================
    PresetManager::CurrentPreset PresetManager::getCurrent() const
    {
        const juce::ScopedLock sl (currentLock);
        return current;
    }

    std::optional<PresetManager::Preset> PresetManager::getCurrentPreset() const
    {
        const auto now = getCurrent();

        for (const auto& preset : getAllPresets())
            if (now.matches (preset))
                return preset;

        return std::nullopt;
    }

    void PresetManager::setCurrent (const CurrentPreset& newCurrent, bool isModified)
    {
        {
            const juce::ScopedLock sl (currentLock);
            current = newCurrent;
        }

        modified = isModified;
        ++changeCounter;
    }

    void PresetManager::markModified() noexcept
    {
        if (isLoading.load() || modified.exchange (true))
            return;

        ++changeCounter;
    }

    void PresetManager::audioProcessorParameterChanged (juce::AudioProcessor*, int, float)
    {
        markModified();
    }

    //==============================================================================
    void PresetManager::addPresetInfoTo (juce::ValueTree& stateCopy) const
    {
        const auto now = getCurrent();
        stateCopy.setProperty (presetNameProp, now.name, nullptr);
        stateCopy.setProperty (presetCategoryProp, now.category, nullptr);
        stateCopy.setProperty (presetFactoryProp, now.isFactory, nullptr);
        stateCopy.setProperty (presetModifiedProp, isModified(), nullptr);
    }

    void PresetManager::restoreState (juce::ValueTree newState)
    {
        if (! newState.hasType (apvts.state.getType()))
            return;

        const auto hasInfo = newState.hasProperty (presetNameProp);
        const CurrentPreset info { newState[presetNameProp], newState[presetCategoryProp], newState[presetFactoryProp] };
        const bool wasModified = newState[presetModifiedProp];

        for (const auto& prop : { presetNameProp, presetCategoryProp, presetFactoryProp, presetModifiedProp })
            newState.removeProperty (prop, nullptr);

        isLoading = true;
        apvts.replaceState (newState);
        isLoading = false;

        if (hasInfo)
            setCurrent (info, wasModified);
    }
}
