#include "TrackHeader.h"

TrackHeader::TrackHeader()
{
    // Enable keyboard focus for keyboard shortcuts
    setWantsKeyboardFocus(true);

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

void TrackHeader::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        showContextMenu();
    }
    else
    {
        // Left-click selects the track and grabs keyboard focus for shortcuts
        grabKeyboardFocus();
        if (onTrackSelected && trackPtr != nullptr)
            onTrackSelected(trackPtr);
    }
}

void TrackHeader::showContextMenu()
{
    juce::PopupMenu menu;

    menu.addItem(DeleteTrackId, "Delete Track", trackPtr != nullptr);

    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this](int result)
        {
            if (result == DeleteTrackId && trackPtr != nullptr)
            {
                showDeleteConfirmation();
            }
        });
}

bool TrackHeader::keyPressed(const juce::KeyPress& key)
{
    // Delete key - show confirmation dialog to delete track
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (trackPtr != nullptr)
        {
            showDeleteConfirmation();
            return true;
        }
    }

    return false;
}

void TrackHeader::showDeleteConfirmation()
{
    if (trackPtr == nullptr)
        return;

    juce::String trackName = trackPtr->getName();

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Delete Track")
        .withMessage("Are you sure you want to delete \"" + trackName + "\"?\n\nThis action cannot be undone.")
        .withButton("Delete")
        .withButton("Cancel");

    juce::AlertWindow::showAsync(options, [this](int result)
    {
        // result == 1 means the first button ("Delete") was clicked
        if (result == 1 && trackPtr != nullptr)
        {
            if (onDeleteTrack)
                onDeleteTrack(trackPtr);
        }
    });
}
