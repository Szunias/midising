#include "TrackHeader.h"
#include "../Timeline/AutomationLane.h"

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
            bool newArmedState = !trackPtr->isArmed();
            trackPtr->setArmed(newArmedState);

            // For audio tracks, automatically enable input monitoring when arming
            if (trackPtr->getType() == TrackType::Audio)
            {
                auto* audioTrack = static_cast<AudioTrack*>(trackPtr);
                if (newArmedState)
                {
                    // Enable monitoring when arming
                    audioTrack->setInputMonitoringEnabled(true);
                }
            }

            updateFromTrack();
            if (onArmChanged)
                onArmChanged(trackPtr);
        }
    };
    addAndMakeVisible(armButton);

    // Input monitoring button (for audio tracks only)
    monitorButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    monitorButton.setTooltip("Input Monitoring - hear input while armed");
    monitorButton.onClick = [this]()
    {
        if (trackPtr != nullptr && trackPtr->getType() == TrackType::Audio)
        {
            auto* audioTrack = static_cast<AudioTrack*>(trackPtr);
            audioTrack->setInputMonitoringEnabled(!audioTrack->isInputMonitoringEnabled());
            updateFromTrack();
            if (onInputMonitoringChanged)
                onInputMonitoringChanged(trackPtr);
        }
    };
    addAndMakeVisible(monitorButton);

    // Automation button - shows automation lane menu
    autoButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    autoButton.onClick = [this]()
    {
        showAutomationMenu();
    };
    addAndMakeVisible(autoButton);
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
    auto topRow = bounds.removeFromTop(20);
    nameLabel.setBounds(topRow);

    bounds.removeFromTop(2);

    // First buttons row (M, S, R, I for audio tracks)
    auto buttonRow1 = bounds.removeFromTop(20);

    // Determine if this is an audio track (show monitoring button)
    bool isAudioTrack = (trackPtr != nullptr && trackPtr->getType() == TrackType::Audio);

    if (isAudioTrack)
    {
        // 4 buttons: M, S, R, I
        int buttonWidth = (buttonRow1.getWidth() - 9) / 4;

        muteButton.setBounds(buttonRow1.removeFromLeft(buttonWidth));
        buttonRow1.removeFromLeft(3);
        soloButton.setBounds(buttonRow1.removeFromLeft(buttonWidth));
        buttonRow1.removeFromLeft(3);
        armButton.setBounds(buttonRow1.removeFromLeft(buttonWidth));
        buttonRow1.removeFromLeft(3);
        monitorButton.setBounds(buttonRow1.removeFromLeft(buttonWidth));
        monitorButton.setVisible(true);
    }
    else
    {
        // 3 buttons: M, S, R
        int buttonWidth = (buttonRow1.getWidth() - 6) / 3;

        muteButton.setBounds(buttonRow1.removeFromLeft(buttonWidth));
        buttonRow1.removeFromLeft(3);
        soloButton.setBounds(buttonRow1.removeFromLeft(buttonWidth));
        buttonRow1.removeFromLeft(3);
        armButton.setBounds(buttonRow1.removeFromLeft(buttonWidth));
        monitorButton.setVisible(false);
    }

    bounds.removeFromTop(2);

    // Second buttons row (Automation)
    auto buttonRow2 = bounds.removeFromTop(20);
    autoButton.setBounds(buttonRow2);
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

    // Update input monitoring button for audio tracks
    if (trackPtr->getType() == TrackType::Audio)
    {
        auto* audioTrack = static_cast<AudioTrack*>(trackPtr);
        bool monitoring = audioTrack->isInputMonitoringEnabled();

        monitorButton.setColour(juce::TextButton::buttonColourId,
                                monitoring ? juce::Colour(0xff5ab4d4) // Light blue for monitoring
                                           : MidiSingLookAndFeel::buttonBackground);
    }

    // Make sure layout is updated (for monitoring button visibility)
    resized();
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

void TrackHeader::showAutomationMenu()
{
    if (trackPtr == nullptr)
        return;

    juce::PopupMenu menu;

    // Check which lanes are currently visible
    bool volumeVisible = false;
    bool panVisible = false;

    for (size_t i = 0; i < trackPtr->getNumAutomationLanes(); ++i)
    {
        auto* lane = trackPtr->getAutomationLane(i);
        if (lane != nullptr)
        {
            if (lane->getParameterName().equalsIgnoreCase("Volume") && lane->isVisible())
                volumeVisible = true;
            if (lane->getParameterName().equalsIgnoreCase("Pan") && lane->isVisible())
                panVisible = true;
        }
    }

    menu.addItem(ShowVolumeAutomationId,
                 volumeVisible ? "Hide Volume Automation" : "Show Volume Automation",
                 true, volumeVisible);
    menu.addItem(ShowPanAutomationId,
                 panVisible ? "Hide Pan Automation" : "Show Pan Automation",
                 true, panVisible);

    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this](int result)
        {
            if (result == ShowVolumeAutomationId && onToggleAutomationLane)
            {
                onToggleAutomationLane(trackIndex, "Volume");
            }
            else if (result == ShowPanAutomationId && onToggleAutomationLane)
            {
                onToggleAutomationLane(trackIndex, "Pan");
            }
        });
}
