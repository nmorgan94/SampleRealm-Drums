#include "PresetBar.h"

namespace srd
{
    PresetBar::PresetBar (PresetManager& p, PromptOverlay& o) : presets (p), prompt (o)
    {
        previousButton.onClick = [this] { presets.loadPrevious(); };
        nextButton.onClick     = [this] { presets.loadNext(); };
        nameButton.onClick     = [this] { showPresetMenu(); };
        saveButton.onClick     = [this] { save(); };
        moreButton.onClick     = [this] { showMoreMenu(); };

        for (auto* b : { &previousButton, &nextButton, &nameButton, &saveButton, &moreButton })
            addAndMakeVisible (b);

        updateName();
        startTimerHz (10);
    }

    void PresetBar::resized()
    {
        auto bounds = getLocalBounds();

        moreButton.setBounds (bounds.removeFromRight (32));
        bounds.removeFromRight (4);
        saveButton.setBounds (bounds.removeFromRight (60));
        bounds.removeFromRight (8);

        previousButton.setBounds (bounds.removeFromLeft (28));
        nextButton.setBounds (bounds.removeFromRight (28));
        nameButton.setBounds (bounds.reduced (4, 0));
    }

    void PresetBar::timerCallback()
    {
        if (presets.getChangeCounter() != lastChange)
            updateName();
    }

    void PresetBar::updateName()
    {
        lastChange = presets.getChangeCounter();
        nameButton.setButtonText (presets.getCurrent().name + (presets.isModified() ? " *" : ""));
    }

    //==============================================================================
    void PresetBar::showPresetMenu()
    {
        presets.refreshUserPresets();

        auto all = presets.getAllPresets();
        const auto current = presets.getCurrent();

        const auto makeItem = [&] (int index)
        {
            juce::PopupMenu::Item item (all.getReference (index).name);
            item.itemID = index + 1;
            item.isTicked = current.matches (all.getReference (index));
            return item;
        };

        // Factory presets by category, user presets flat
        std::map<juce::String, juce::PopupMenu> categories;
        juce::String currentCategory;
        juce::Array<int> userIndices;

        for (int i = 0; i < all.size(); ++i)
        {
            const auto& preset = all.getReference (i);

            if (! preset.isFactory)
            {
                userIndices.add (i);
                continue;
            }

            const auto category = preset.category.isNotEmpty() ? preset.category : juce::String ("Other");
            categories[category].addItem (makeItem (i));

            if (current.matches (preset))
                currentCategory = category;
        }

        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());
        menu.addSectionHeader ("Factory");

        for (auto& [category, subMenu] : categories)
            menu.addSubMenu (category, subMenu, true, nullptr, category == currentCategory);

        menu.addSectionHeader ("User");

        for (auto i : userIndices)
            menu.addItem (makeItem (i));

        if (userIndices.isEmpty())
            menu.addItem ("No user presets yet", false, false, nullptr);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (nameButton),
                            [safeThis = juce::Component::SafePointer (this), listed = std::move (all)] (int result)
                            {
                                if (safeThis != nullptr && result > 0)
                                    safeThis->presets.loadPreset (listed.getReference (result - 1));
                            });
    }

    void PresetBar::showMoreMenu()
    {
        const auto user = currentUserPreset();

        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());
        menu.addItem ("Save As...", [this] { saveAs(); });
        menu.addItem ("Rename...", user.has_value(), false, [this, user] { rename (*user); });
        menu.addItem ("Delete", user.has_value(), false, [this, user] { remove (*user); });
        menu.addSeparator();
        menu.addItem ("Show Preset Folder", [this]
        {
            const auto folder = presets.getUserDirectory();
            folder.createDirectory();
            folder.startAsProcess();
        });

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (moreButton));
    }

    //==============================================================================
    void PresetBar::save()
    {
        // User presets save in place; anything else needs a name first
        if (const auto user = currentUserPreset())
            report (presets.saveUserPreset (user->name, user->category));
        else
            saveAs();
    }

    std::optional<PresetManager::Preset> PresetBar::currentUserPreset() const
    {
        if (auto current = presets.getCurrentPreset(); current.has_value() && ! current->isFactory)
            return current;

        return std::nullopt;
    }

    void PresetBar::saveAs()
    {
        // Factory names are reserved, so don't suggest one
        const auto current = presets.getCurrent();
        const auto suggestion = current.isFactory ? juce::String() : current.name;

        prompt.askForText ("Save Preset", suggestion, "Save", [this] (const juce::String& name)
        {
            if (! presets.userPresetExists (name))
            {
                saveAs (name);
                return;
            }

            prompt.askToConfirm ("Replace Preset", "\"" + name.trim() + "\" already exists. Replace it?", "Replace",
                                 [this, name] { saveAs (name); });
        });
    }

    void PresetBar::saveAs (const juce::String& name)
    {
        report (presets.saveUserPreset (name, presets.getCurrent().category));
    }

    void PresetBar::rename (const PresetManager::Preset& preset)
    {
        prompt.askForText ("Rename Preset", preset.name, "Rename", [this, preset] (const juce::String& name)
        {
            report (presets.renameUserPreset (preset, name));
        });
    }

    void PresetBar::remove (const PresetManager::Preset& preset)
    {
        prompt.askToConfirm ("Delete Preset", "Delete \"" + preset.name + "\"?", "Delete",
                             [this, preset] { report (presets.deleteUserPreset (preset)); });
    }

    void PresetBar::report (const juce::Result& result)
    {
        if (result.failed())
            prompt.showMessage ("Preset", result.getErrorMessage());

        updateName();
    }
}
