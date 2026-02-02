#include "SettingsPanel.h"

SettingsPanel::SettingsPanel(juce::AudioDeviceManager& deviceManager)
    : audioDeviceManager(deviceManager)
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

    setSize(500, 400);
}

SettingsPanel::~SettingsPanel()
{
    audioDeviceManager.removeChangeListener(this);
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
    // Audio device settings changed - could notify parent component
}
