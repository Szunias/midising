#include "TransportBar.h"

TransportBar::TransportBar()
{
    // Rewind button
    rewindButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    rewindButton.setTooltip("Rewind to start");
    rewindButton.onClick = [this]()
    {
        if (onRewind)
            onRewind();
    };
    addAndMakeVisible(rewindButton);

    // Play button
    playButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    playButton.setTooltip("Play / Pause (Space)");
    playButton.onClick = [this]()
    {
        if (onPlay)
            onPlay();
        updateButtonStates();
    };
    addAndMakeVisible(playButton);

    // Stop button
    stopButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    stopButton.setTooltip("Stop");
    stopButton.onClick = [this]()
    {
        if (onStop)
            onStop();
        updateButtonStates();
    };
    addAndMakeVisible(stopButton);

    // Record button
    recordButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    recordButton.setTooltip("Record (R)");
    recordButton.onClick = [this]()
    {
        if (onRecord)
            onRecord();
        updateButtonStates();
    };
    addAndMakeVisible(recordButton);

    // Loop button
    loopButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    loopButton.setTooltip("Toggle Loop");
    loopButton.setClickingTogglesState(true);
    loopButton.onClick = [this]()
    {
        if (onLoopToggle)
            onLoopToggle(loopButton.getToggleState());
    };
    addAndMakeVisible(loopButton);

    // Save button
    saveButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    saveButton.setTooltip("Save Project (Ctrl+S)");
    saveButton.onClick = [this]()
    {
        if (onSave)
            onSave();
    };
    addAndMakeVisible(saveButton);

    // Open button
    openButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    openButton.setTooltip("Open Project (Ctrl+O)");
    openButton.onClick = [this]()
    {
        if (onOpen)
            onOpen();
    };
    addAndMakeVisible(openButton);

    // BPM slider
    bpmSlider.setTooltip("Tempo (BPM)");
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

    // Position label - use monospace font to prevent width jumping when digits change
    positionLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    juce::Font monoFont(juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::bold);
    positionLabel.setFont(monoFont);
    positionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(positionLabel);

    // Metronome button
    metronomeButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    metronomeButton.setTooltip("Toggle Metronome");
    metronomeButton.setClickingTogglesState(true);
    metronomeButton.onClick = [this]()
    {
        if (onMetronomeToggle)
            onMetronomeToggle(metronomeButton.getToggleState());
    };
    addAndMakeVisible(metronomeButton);

    // Metronome volume slider
    metronomeVolumeSlider.setTooltip("Metronome Volume");
    metronomeVolumeSlider.setRange(0.0, 1.0, 0.01);
    metronomeVolumeSlider.setValue(0.7);
    metronomeVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    metronomeVolumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    metronomeVolumeSlider.onValueChange = [this]()
    {
        if (onMetronomeVolumeChange)
            onMetronomeVolumeChange(metronomeVolumeSlider.getValue());
    };
    addAndMakeVisible(metronomeVolumeSlider);

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
    // Fixed button dimensions - 40x30px as per spec
    constexpr int buttonWidth = 40;
    constexpr int buttonHeight = 30;
    constexpr int buttonSpacing = 5;
    constexpr int sectionSpacing = 15;
    constexpr int margin = 10;

    // Calculate vertical center for buttons
    const int y = (getHeight() - buttonHeight) / 2;
    int x = margin;

    // Transport buttons - fixed 40x30px with explicit pixel coordinates
    rewindButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + buttonSpacing;

    playButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + buttonSpacing;

    stopButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + buttonSpacing;

    recordButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + buttonSpacing;

    loopButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + sectionSpacing;

    // Project buttons - fixed 40x30px
    saveButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + buttonSpacing;

    openButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + sectionSpacing;

    // Position display - fixed width
    constexpr int positionLabelWidth = 80;
    positionLabel.setBounds(x, y, positionLabelWidth, buttonHeight);
    x += positionLabelWidth + sectionSpacing;

    // BPM controls - positioned from right side with fixed coordinates
    int rightX = getWidth() - margin;

    // Metronome volume slider - fixed 100px width
    constexpr int metronomeVolumeWidth = 100;
    rightX -= metronomeVolumeWidth;
    metronomeVolumeSlider.setBounds(rightX, y, metronomeVolumeWidth, buttonHeight);
    rightX -= buttonSpacing;

    // Metronome toggle button - fixed 40x30px
    rightX -= buttonWidth;
    metronomeButton.setBounds(rightX, y, buttonWidth, buttonHeight);
    rightX -= sectionSpacing;

    // BPM slider - fixed 120px width
    constexpr int bpmSliderWidth = 120;
    rightX -= bpmSliderWidth;
    bpmSlider.setBounds(rightX, y, bpmSliderWidth, buttonHeight);
    rightX -= buttonSpacing;

    // BPM label - fixed 35px width
    constexpr int bpmLabelWidth = 35;
    rightX -= bpmLabelWidth;
    bpmLabel.setBounds(rightX, y, bpmLabelWidth, buttonHeight);
}

void TransportBar::setMetronomeEnabled(bool enabled)
{
    metronomeButton.setToggleState(enabled, juce::dontSendNotification);
}

void TransportBar::setLoopEnabled(bool enabled)
{
    loopButton.setToggleState(enabled, juce::dontSendNotification);
    loopButton.setColour(juce::TextButton::buttonColourId,
                         enabled ? MidiSingLookAndFeel::accentColour
                                 : MidiSingLookAndFeel::buttonBackground);
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

    // Update loop button color based on toggle state
    loopButton.setColour(juce::TextButton::buttonColourId,
                         loopButton.getToggleState() ? MidiSingLookAndFeel::accentColour
                                                     : MidiSingLookAndFeel::buttonBackground);
}
