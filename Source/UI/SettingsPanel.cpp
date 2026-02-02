#include "SettingsPanel.h"

SettingsPanel::SettingsPanel(juce::AudioDeviceManager& deviceManager,
                             juce::MidiInputCallback* midiCallback)
    : audioDeviceManager(deviceManager),
      midiInputCallback(midiCallback)
{
    // Create device selector with input and output enabled
    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
        audioDeviceManager,
        0, 2,       // min/max input channels
        0, 2,       // min/max output channels
        true,       // show MIDI inputs
        true,       // show MIDI outputs
        true,       // treat channels as stereo pairs
        false       // hide advanced options
    );

    addAndMakeVisible(deviceSelector.get());
    audioDeviceManager.addChangeListener(this);

    // Initialize MIDI callbacks for any already-enabled devices
    syncMidiInputCallbacks();

    setSize(500, 400);
}

SettingsPanel::~SettingsPanel()
{
    audioDeviceManager.removeChangeListener(this);

    // NOTE: We intentionally do NOT remove MIDI callbacks here.
    // The MIDI devices should remain connected after the settings dialog closes.
    // The callbacks are registered with the AudioDeviceManager and will persist
    // until the application exits or the device is disabled.
}

void SettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark);
}

void SettingsPanel::resized()
{
    deviceSelector->setBounds(getLocalBounds().reduced(10));
}

void SettingsPanel::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused(source);
    // Audio/MIDI device settings changed - sync MIDI callbacks
    syncMidiInputCallbacks();
}

void SettingsPanel::syncMidiInputCallbacks()
{
    if (midiInputCallback == nullptr)
        return;

    // Get list of available MIDI input devices
    auto midiInputs = juce::MidiInput::getAvailableDevices();

    // For each device, ensure the callback state matches the enabled state.
    // We remove and re-add to be idempotent (safe to call multiple times).
    for (const auto& device : midiInputs)
    {
        bool isEnabled = audioDeviceManager.isMidiInputDeviceEnabled(device.identifier);

        // Always remove first to avoid duplicate registrations
        audioDeviceManager.removeMidiInputDeviceCallback(device.identifier, midiInputCallback);

        if (isEnabled)
        {
            // Device is enabled - add the callback
            audioDeviceManager.addMidiInputDeviceCallback(device.identifier, midiInputCallback);
        }
    }
}
