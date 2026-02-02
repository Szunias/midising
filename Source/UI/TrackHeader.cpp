#include "TrackHeader.h"

TrackHeader::TrackHeader()
{
    // Name label
    nameLabel.setEditable(true);
    nameLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    nameLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    nameLabel.onTextChange = [this]()
    {
        if (trackPtr != nullptr)
            trackPtr->setName(nameLabel.getText());
    };
    addAndMakeVisible(nameLabel);

    // Mute button
    muteButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    muteButton.onClick = [this]()
    {
        if (trackPtr != nullptr)
        {
            trackPtr->setMuted(!trackPtr->isMuted());
            updateFromTrack();
            if (onMuteChanged)
                onMuteChanged(trackPtr);
        }
    };
    addAndMakeVisible(muteButton);

    // Solo button
    soloButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    soloButton.onClick = [this]()
    {
        if (trackPtr != nullptr)
        {
            trackPtr->setSoloed(!trackPtr->isSoloed());
            updateFromTrack();
            if (onSoloChanged)
                onSoloChanged(trackPtr);
        }
    };
    addAndMakeVisible(soloButton);

    // Arm button
    armButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    armButton.onClick = [this]()
    {
        if (trackPtr != nullptr)
        {
            trackPtr->setArmed(!trackPtr->isArmed());
            updateFromTrack();
            if (onArmChanged)
                onArmChanged(trackPtr);
        }
    };
    addAndMakeVisible(armButton);
}

void TrackHeader::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background with track colour strip
    g.fillAll(MidiSingLookAndFeel::backgroundMid);

    if (trackPtr != nullptr)
    {
        // Colour strip on left
        g.setColour(trackPtr->getColour());
        g.fillRect(bounds.removeFromLeft(4));
    }

    // Right border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawLine(static_cast<float>(getWidth()) - 1.0f, 0.0f,
               static_cast<float>(getWidth()) - 1.0f, static_cast<float>(getHeight()));
}

void TrackHeader::resized()
{
    auto bounds = getLocalBounds().reduced(6, 4);
    bounds.removeFromLeft(4); // Space for colour strip

    // Track type indicator and name
    auto topRow = bounds.removeFromTop(24);
    nameLabel.setBounds(topRow);

    bounds.removeFromTop(4);

    // Buttons row
    auto buttonRow = bounds.removeFromTop(24);
    int buttonWidth = (buttonRow.getWidth() - 8) / 3;

    muteButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(4);
    soloButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(4);
    armButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
}

void TrackHeader::setTrack(Track* track)
{
    trackPtr = track;
    updateFromTrack();
}

void TrackHeader::updateFromTrack()
{
    if (trackPtr == nullptr)
        return;

    nameLabel.setText(trackPtr->getName(), juce::dontSendNotification);

    // Update button colours based on state
    muteButton.setColour(juce::TextButton::buttonColourId,
                         trackPtr->isMuted() ? juce::Colour(0xffd4a85a) // Orange for muted
                                             : MidiSingLookAndFeel::buttonBackground);

    soloButton.setColour(juce::TextButton::buttonColourId,
                         trackPtr->isSoloed() ? juce::Colour(0xff5ad4cf) // Cyan for soloed
                                              : MidiSingLookAndFeel::buttonBackground);

    armButton.setColour(juce::TextButton::buttonColourId,
                        trackPtr->isArmed() ? MidiSingLookAndFeel::recordColour
                                            : MidiSingLookAndFeel::buttonBackground);
}
