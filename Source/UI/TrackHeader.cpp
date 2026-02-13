#include "TrackHeader.h"
#include "../Timeline/AutomationLane.h"
#include <juce_gui_extra/juce_gui_extra.h>

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

    // CPU usage label
    cpuLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
    cpuLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    cpuLabel.setFont(juce::Font(10.0f));
    cpuLabel.setJustificationType(juce::Justification::centredRight);
    cpuLabel.setText("0%", juce::dontSendNotification);
    cpuLabel.setTooltip("Track CPU usage");
    addAndMakeVisible(cpuLabel);

    // Start timer for CPU display updates (10 Hz)
    startTimer(100);

    // Mute button
    muteButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    muteButton.setTooltip("Mute Track");
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
    soloButton.setTooltip("Solo Track");
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
    armButton.setTooltip("Arm for Recording");
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
    autoButton.setTooltip("Automation Lanes");
    autoButton.onClick = [this]()
    {
        showAutomationMenu();
    };
    addAndMakeVisible(autoButton);
}

TrackHeader::~TrackHeader()
{
    stopTimer();
}

void TrackHeader::timerCallback()
{
    updateCpuDisplay();
}

void TrackHeader::updateCpuDisplay()
{
    if (trackPtr == nullptr)
    {
        cpuLabel.setText("--", juce::dontSendNotification);
        cpuLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
        return;
    }

    double cpuLoad = trackPtr->getCpuLoad();
    int cpuPercent = static_cast<int>(cpuLoad * 100.0);

    // Clamp display to reasonable range
    cpuPercent = juce::jlimit(0, 999, cpuPercent);

    juce::String cpuText = juce::String(cpuPercent) + "%";
    cpuLabel.setText(cpuText, juce::dontSendNotification);

    // Color code based on CPU load
    juce::Colour textColour;
    if (cpuLoad < 0.5)
    {
        // Green for low load
        textColour = MidiSingLookAndFeel::playColour;
    }
    else if (cpuLoad < 0.8)
    {
        // Orange/warning for medium load
        textColour = MidiSingLookAndFeel::warningColour;
    }
    else
    {
        // Red for high load
        textColour = MidiSingLookAndFeel::recordColour;
    }

    cpuLabel.setColour(juce::Label::textColourId, textColour);
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

    // Track type indicator and name with CPU label on the right
    auto topRow = bounds.removeFromTop(20);

    // CPU label takes 35 pixels on the right
    cpuLabel.setBounds(topRow.removeFromRight(35));
    topRow.removeFromRight(2); // Small gap
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

    // Update CPU display
    updateCpuDisplay();

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
        showContextMenu(event.getScreenPosition());
    }
    else
    {
        // Left-click selects the track and grabs keyboard focus for shortcuts
        grabKeyboardFocus();
        if (onTrackSelected && trackPtr != nullptr)
            onTrackSelected(trackPtr);
    }
}

void TrackHeader::mouseDoubleClick(const juce::MouseEvent& /*event*/)
{
    showPropertiesDialog();
}

void TrackHeader::showContextMenu(juce::Point<int> screenPosition)
{
    juce::PopupMenu menu;

    if (trackPtr != nullptr)
    {
        bool frozen = trackPtr->isFrozen();
        menu.addItem(FreezeTrackId, frozen ? "Unfreeze Track" : "Freeze Track");
        menu.addItem(SetColourId, "Set Colour...");
        menu.addSeparator();
    }

    menu.addItem(DeleteTrackId, "Delete Track", trackPtr != nullptr);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPosition, screenPosition)),
        [this](int result)
        {
            if (result == DeleteTrackId && trackPtr != nullptr)
            {
                showDeleteConfirmation();
            }
            else if (result == FreezeTrackId && trackPtr != nullptr)
            {
                if (onFreezeTrack)
                    onFreezeTrack(trackPtr);
            }
            else if (result == SetColourId && trackPtr != nullptr)
            {
                showPropertiesDialog();
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

//==============================================================================
// Track Properties Dialog
//==============================================================================

/**
 * Custom component that contains the track properties editor.
 */
class TrackPropertiesComponent : public juce::Component,
                                  public juce::ChangeListener
{
public:
    TrackPropertiesComponent(Track* track, std::function<void()> onChanged)
        : trackPtr(track), onPropertiesChanged(std::move(onChanged))
    {
        // Track name label
        nameLabel.setText("Track Name:", juce::dontSendNotification);
        nameLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
        addAndMakeVisible(nameLabel);

        // Track name editor
        nameEditor.setText(track->getName(), juce::dontSendNotification);
        nameEditor.setColour(juce::TextEditor::backgroundColourId, MidiSingLookAndFeel::buttonBackground);
        nameEditor.setColour(juce::TextEditor::textColourId, MidiSingLookAndFeel::textColour);
        nameEditor.setColour(juce::TextEditor::outlineColourId, MidiSingLookAndFeel::borderColour);
        nameEditor.onTextChange = [this]()
        {
            if (trackPtr != nullptr)
            {
                trackPtr->setName(nameEditor.getText());
                if (onPropertiesChanged)
                    onPropertiesChanged();
            }
        };
        addAndMakeVisible(nameEditor);

        // Track colour label
        colourLabel.setText("Track Colour:", juce::dontSendNotification);
        colourLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
        addAndMakeVisible(colourLabel);

        // Colour selector
        colourSelector.setCurrentColour(track->getColour());
        colourSelector.addChangeListener(this);
        addAndMakeVisible(colourSelector);

        setSize(300, 280);
    }

    ~TrackPropertiesComponent() override
    {
        colourSelector.removeChangeListener(this);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);

        // Name label and editor
        auto nameRow = bounds.removeFromTop(24);
        nameLabel.setBounds(nameRow.removeFromLeft(90));
        nameEditor.setBounds(nameRow);

        bounds.removeFromTop(10);

        // Colour label
        colourLabel.setBounds(bounds.removeFromTop(24));

        bounds.removeFromTop(5);

        // Colour selector takes the rest
        colourSelector.setBounds(bounds);
    }

    void changeListenerCallback(juce::ChangeBroadcaster* source) override
    {
        if (source == &colourSelector && trackPtr != nullptr)
        {
            trackPtr->setColour(colourSelector.getCurrentColour());
            if (onPropertiesChanged)
                onPropertiesChanged();
        }
    }

private:
    Track* trackPtr;
    std::function<void()> onPropertiesChanged;

    juce::Label nameLabel;
    juce::TextEditor nameEditor;
    juce::Label colourLabel;
    juce::ColourSelector colourSelector { juce::ColourSelector::showColourAtTop
                                        | juce::ColourSelector::showSliders
                                        | juce::ColourSelector::showColourspace };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackPropertiesComponent)
};

void TrackHeader::showPropertiesDialog()
{
    if (trackPtr == nullptr)
        return;

    // Create the properties component
    auto* propertiesComponent = new TrackPropertiesComponent(trackPtr, [this]()
    {
        // Update the UI when properties change
        updateFromTrack();
        repaint();
        if (onTrackPropertiesChanged)
            onTrackPropertiesChanged(trackPtr);
    });

    // Configure the dialog window
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(propertiesComponent);
    options.dialogTitle = "Track Properties - " + trackPtr->getName();
    options.dialogBackgroundColour = MidiSingLookAndFeel::backgroundDark;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    // Launch the dialog as a non-modal window
    options.launchAsync();
}
