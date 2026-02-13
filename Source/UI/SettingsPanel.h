#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "LookAndFeel.h"

/**
 * SettingsPanel allows audio/MIDI device selection and plugin path management.
 * Includes tabs for Audio/MIDI settings and Plugin settings.
 */
class SettingsPanel : public juce::Component,
                      public juce::ChangeListener
{
public:
    SettingsPanel(juce::AudioDeviceManager& deviceManager,
                  juce::MidiInputCallback* midiCallback = nullptr);
    ~SettingsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    void syncMidiInputCallbacks();
    void saveDeviceSettings();
    void loadDeviceSettings();
    juce::File getSettingsFile() const;
    void setupPluginPathsTab();
    void setupGeneralTab();
    void refreshPluginPathList();

    juce::AudioDeviceManager& audioDeviceManager;
    juce::MidiInputCallback* midiInputCallback = nullptr;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;

    // Plugin paths UI components (in plugin tab)
    std::unique_ptr<juce::Component> pluginPathsPanel;
    std::unique_ptr<juce::ListBox> pluginPathsListBox;
    std::unique_ptr<juce::StringArray> pluginPathsArray;
    std::unique_ptr<juce::TextButton> addPathButton;
    std::unique_ptr<juce::TextButton> removePathButton;
    std::unique_ptr<juce::TextButton> rescanButton;

    // General settings tab
    std::unique_ptr<juce::Component> generalPanel;

    // Tabs
    std::unique_ptr<juce::TabbedComponent> tabs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
