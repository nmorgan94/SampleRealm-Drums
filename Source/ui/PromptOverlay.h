#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace srd
{
    //==============================================================================
    /**
     * An in-editor dialog that dims its parent.
     */
    class PromptOverlay : public juce::Component
    {
    public:
        enum ColourIds
        {
            dimColourId = 0x5d10400,
            panelColourId,
            outlineColourId
        };

        PromptOverlay()
        {
            titleLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            messageLabel.setFont (juce::FontOptions (12.0f));
            messageLabel.setJustificationType (juce::Justification::topLeft);

            textEditor.onReturnKey = [this] { accept(); };
            textEditor.onEscapeKey = [this] { dismiss(); };

            okButton.onClick     = [this] { accept(); };
            cancelButton.onClick = [this] { dismiss(); };

            addAndMakeVisible (titleLabel);
            addAndMakeVisible (okButton);

            // Shown per prompt
            for (auto* c : std::initializer_list<juce::Component*> { &messageLabel, &textEditor, &cancelButton })
                addChildComponent (c);

            setWantsKeyboardFocus (true);
        }

        /** Font for the text field; labels and buttons use the LookAndFeel's. */
        void setFont (const juce::Font& font)   { textEditor.setFont (font); }

        void askForText (const juce::String& title, const juce::String& initialText, const juce::String& okText,
                         std::function<void (const juce::String&)> onOk)
        {
            show (title, {}, okText, true, true, std::move (onOk));
            textEditor.setText (initialText, false);
            textEditor.selectAll();
            textEditor.grabKeyboardFocus();
        }

        void askToConfirm (const juce::String& title, const juce::String& message, const juce::String& okText,
                           std::function<void()> onOk)
        {
            show (title, message, okText, false, true, [confirmed = std::move (onOk)] (const juce::String&) { confirmed(); });
        }

        void showMessage (const juce::String& title, const juce::String& message)
        {
            show (title, message, "OK", false, false, nullptr);
        }

        void dismiss()
        {
            callback = nullptr;
            setVisible (false);
        }

        //==============================================================================
        void paint (juce::Graphics& g) override
        {
            g.fillAll (findColour (dimColourId));

            const auto panel = getPanelBounds().toFloat();
            g.setColour (findColour (panelColourId));
            g.fillRoundedRectangle (panel, 8.0f);
            g.setColour (findColour (outlineColourId));
            g.drawRoundedRectangle (panel.reduced (0.5f), 8.0f, 1.0f);
        }

        void resized() override
        {
            auto area = getPanelBounds().reduced (20, 16);

            titleLabel.setBounds (area.removeFromTop (24));
            area.removeFromTop (8);

            auto buttons = area.removeFromBottom (28);
            okButton.setBounds (buttons.removeFromRight (90));
            buttons.removeFromRight (8);
            cancelButton.setBounds (buttons.removeFromRight (90));
            area.removeFromBottom (12);

            textEditor.setBounds (area.withHeight (28));
            messageLabel.setBounds (area);
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::escapeKey)   { dismiss(); return true; }
            if (key == juce::KeyPress::returnKey)   { accept();  return true; }
            return true; // modal: nothing behind the overlay gets keys
        }

        void mouseDown (const juce::MouseEvent&) override {} // swallows clicks outside the panel

        void parentSizeChanged() override
        {
            if (auto* parent = getParentComponent())
                setBounds (parent->getLocalBounds());
        }

    private:
        juce::Label titleLabel, messageLabel;
        juce::TextEditor textEditor;
        juce::TextButton okButton, cancelButton { "Cancel" };
        std::function<void (const juce::String&)> callback;

        juce::Rectangle<int> getPanelBounds() const
        {
            return getLocalBounds().withSizeKeepingCentre (380, 160);
        }

        void show (const juce::String& title, const juce::String& message, const juce::String& okText,
                   bool wantsText, bool canCancel, std::function<void (const juce::String&)> onOk)
        {
            callback = std::move (onOk);
            titleLabel.setText (title, juce::dontSendNotification);
            messageLabel.setText (message, juce::dontSendNotification);
            okButton.setButtonText (okText);

            messageLabel.setVisible (! wantsText);
            textEditor.setVisible (wantsText);
            cancelButton.setVisible (canCancel);

            parentSizeChanged();
            setVisible (true);
            toFront (true);
        }

        /** Closes first, so the callback can open another prompt. */
        void accept()
        {
            auto onOk = std::move (callback);
            const auto text = textEditor.getText();
            dismiss();

            if (onOk != nullptr)
                onOk (text);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PromptOverlay)
    };
}
