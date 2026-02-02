#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "LookAndFeel.h"

/**
 * SettingsPanel allows audio and MIDI device selection.
 * When MIDI input devices are enabled in the UI, they are automatically
 * connected to the provided MidiInputCallback (typically the MidiEngine).
 */
class SettingsPanel : public juce::Component,
                      public juce::ChangeListener
{
public:
    /**
     * Create a SettingsPanel for audio and MIDI device configuration.
     * @param deviceManager The audio device manager to configure
     * @param midiCallback Optional callback to receive MIDI input from enabled devices.
     *                     Pass the MidiEngine pointer here to enable MIDI input recording.
     */
    SettingsPanel(juce::AudioDeviceManager& deviceManager,
                  juce::MidiInputCallback* midiCallback = nullptr);
    ~SettingsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    /**
     * Synchronize MIDI input callbacks with currently enabled devices.
     * Registers the midiInputCallback for all enabled MIDI input devices
     * and removes it from disabled devices. Safe to call multiple times.
     */
    void syncMidiInputCallbacks();

    juce::AudioDeviceManager& audioDeviceManager;
    juce::MidiInputCallback* midiInputCallback = nullptr;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
