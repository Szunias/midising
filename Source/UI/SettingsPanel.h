#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

/**
 * SettingsPanel provides application settings controls.
 * Includes count-in configuration for recording.
 */
class SettingsPanel : public juce::Component,
                      public juce::ComboBox::Listener
{
public:
    SettingsPanel();
    ~SettingsPanel() override;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // ComboBox::Listener override
    void comboBoxChanged(juce::ComboBox* comboBox) override;

    // Callback for count-in changes
    std::function<void(int)> onCountInChange;

    // Get current count-in bars setting
    int getCountInBars() const;

private:
    // Count-in controls
    juce::ComboBox countInComboBox;
    juce::Label countInLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
