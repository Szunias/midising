#include "TransportBar.h"

TransportBar::TransportBar()
{
    // Play button
    playButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    playButton.onClick = [this]()
    {
        if (onPlay)
            onPlay();
        updateButtonStates();
    };
    addAndMakeVisible(playButton);

    // Stop button
    stopButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    stopButton.onClick = [this]()
    {
        if (onStop)
            onStop();
        updateButtonStates();
    };
    addAndMakeVisible(stopButton);

    // Record button
    recordButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    recordButton.onClick = [this]()
    {
        if (onRecord)
            onRecord();
        updateButtonStates();
    };
    addAndMakeVisible(recordButton);

    // Save button
    saveButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    saveButton.onClick = [this]()
    {
        if (onSave)
            onSave();
    };
    addAndMakeVisible(saveButton);

    // Open button
    openButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    openButton.onClick = [this]()
    {
        if (onOpen)
            onOpen();
    };
    addAndMakeVisible(openButton);

    // BPM slider
    bpmSlider.setRange(20.0, 300.0, 1.0);
    bpmSlider.setValue(120.0);
    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    bpmSlider.onValueChange = [this]()
    {
        if (onBpmChange)
            onBpmChange(bpmSlider.getValue());
    };
    addAndMakeVisible(bpmSlider);

    // BPM label
    bpmLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    addAndMakeVisible(bpmLabel);

    // Position label
    positionLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    positionLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    positionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(positionLabel);

    // Start timer for position updates
    startTimer(50); // 20 FPS
}

TransportBar::~TransportBar()
{
    stopTimer();
}

void TransportBar::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundMid);

    // Bottom border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawLine(0.0f, static_cast<float>(getHeight()) - 1.0f,
               static_cast<float>(getWidth()), static_cast<float>(getHeight()) - 1.0f);
}

void TransportBar::resized()
{
    auto bounds = getLocalBounds().reduced(10, 5);

    // Transport buttons on left
    playButton.setBounds(bounds.removeFromLeft(60));
    bounds.removeFromLeft(5);
    stopButton.setBounds(bounds.removeFromLeft(60));
    bounds.removeFromLeft(5);
    recordButton.setBounds(bounds.removeFromLeft(50));
    bounds.removeFromLeft(20);

    // Project buttons
    saveButton.setBounds(bounds.removeFromLeft(50));
    bounds.removeFromLeft(5);
    openButton.setBounds(bounds.removeFromLeft(50));
    bounds.removeFromLeft(20);

    // Position display
    positionLabel.setBounds(bounds.removeFromLeft(100));
    bounds.removeFromLeft(20);

    // BPM controls on right
    auto rightBounds = bounds.removeFromRight(350);
    
    // Metronome volume
    metronomeVolumeSlider.setBounds(rightBounds.removeFromRight(100));
    rightBounds.removeFromRight(10);
    
    // Metronome toggle
    metronomeButton.setBounds(rightBounds.removeFromRight(60));
    rightBounds.removeFromRight(20);

    // BPM
    bpmLabel.setBounds(rightBounds.removeFromLeft(40));
    bpmSlider.setBounds(rightBounds);
}

void TransportBar::setMetronomeEnabled(bool enabled)
{
    metronomeButton.setToggleState(enabled, juce::dontSendNotification);
}

void TransportBar::timerCallback()
{
    if (transportPtr != nullptr)
    {
        // Update position display
        int64_t pos = transportPtr->getPlayheadPosition();
        double seconds = pos / 44100.0; // Assume 44.1kHz
        int mins = static_cast<int>(seconds) / 60;
        int secs = static_cast<int>(seconds) % 60;
        int ms = static_cast<int>((seconds - std::floor(seconds)) * 100);

        positionLabel.setText(juce::String::formatted("%d:%02d:%02d", mins, secs, ms),
                              juce::dontSendNotification);

        updateButtonStates();
    }
}

void TransportBar::updateButtonStates()
{
    if (transportPtr == nullptr)
        return;

    bool isPlaying = transportPtr->isPlaying();
    bool isRecording = transportPtr->isRecording();

    playButton.setButtonText(isPlaying ? "Pause" : "Play");
    playButton.setColour(juce::TextButton::buttonColourId,
                         isPlaying ? MidiSingLookAndFeel::playColour
                                   : MidiSingLookAndFeel::buttonBackground);

    recordButton.setColour(juce::TextButton::buttonColourId,
                           isRecording ? MidiSingLookAndFeel::recordColour
                                       : MidiSingLookAndFeel::buttonBackground);
}
