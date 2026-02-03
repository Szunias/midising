#include "ChannelStrip.h"
#include "../Audio/AudioTrack.h"
#include "../Audio/SendReturn.h"
#include "EffectRackUI.h"

//==============================================================================
// InsertSlotButton Implementation
//==============================================================================

InsertSlotButton::InsertSlotButton(int index)
    : slotIndex(index)
{
}

void InsertSlotButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    // Background colour based on state
    juce::Colour bgColour;
    if (empty)
    {
        bgColour = MidiSingLookAndFeel::sliderBackground;
    }
    else if (bypassed)
    {
        bgColour = MidiSingLookAndFeel::buttonBackground.darker(0.3f);
    }
    else
    {
        bgColour = MidiSingLookAndFeel::accentColour.darker(0.4f);
    }

    // Highlight on hover
    if (mouseOver)
        bgColour = bgColour.brighter(0.15f);

    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds, 2.0f);

    // Border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRoundedRectangle(bounds, 2.0f, 1.0f);

    // Text - slot number if empty, effect name if filled
    g.setFont(juce::Font(9.0f));

    if (empty)
    {
        g.setColour(MidiSingLookAndFeel::textDimColour);
        g.drawText(juce::String(slotIndex + 1), bounds, juce::Justification::centred);
    }
    else
    {
        // Show effect name (truncated) with bypass indicator
        juce::String displayText = effectName;
        if (bypassed)
            displayText = "(" + displayText + ")";

        g.setColour(bypassed ? MidiSingLookAndFeel::textDimColour : MidiSingLookAndFeel::textColour);
        g.drawText(displayText, bounds.reduced(2, 0), juce::Justification::centred, true);
    }
}

void InsertSlotButton::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        if (onRightClicked)
            onRightClicked(slotIndex, event.getScreenPosition());
    }
    else
    {
        if (onClicked)
            onClicked(slotIndex);
    }
}

void InsertSlotButton::mouseEnter(const juce::MouseEvent&)
{
    mouseOver = true;
    repaint();
}

void InsertSlotButton::mouseExit(const juce::MouseEvent&)
{
    mouseOver = false;
    repaint();
}

void InsertSlotButton::setEffectName(const juce::String& name)
{
    effectName = name;
    repaint();
}

void InsertSlotButton::setEmpty(bool isEmpty)
{
    empty = isEmpty;
    repaint();
}

void InsertSlotButton::setBypassed(bool isBypassed)
{
    bypassed = isBypassed;
    repaint();
}

//==============================================================================
// SendKnob Implementation
//==============================================================================

SendKnob::SendKnob(int index)
    : sendIndex(index)
{
    // Configure knob
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setRange(0.0, 1.0, 0.01);
    knob.setValue(0.0);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setColour(juce::Slider::rotarySliderFillColourId, MidiSingLookAndFeel::accentColour);
    knob.setColour(juce::Slider::rotarySliderOutlineColourId, MidiSingLookAndFeel::sliderBackground);
    knob.onValueChange = [this]()
    {
        if (onLevelChanged)
            onLevelChanged(sendIndex, static_cast<float>(knob.getValue()));
    };
    addAndMakeVisible(knob);

    // Configure label
    nameLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setFont(juce::Font(8.0f));
    nameLabel.setText("S" + juce::String(index + 1), juce::dontSendNotification);
    addAndMakeVisible(nameLabel);
}

void SendKnob::resized()
{
    auto bounds = getLocalBounds();

    // Label at bottom
    nameLabel.setBounds(bounds.removeFromBottom(12));

    // Knob fills the rest
    knob.setBounds(bounds);
}

void SendKnob::setAuxName(const juce::String& name)
{
    if (name.isNotEmpty())
        nameLabel.setText(name.substring(0, 3), juce::dontSendNotification);
    else
        nameLabel.setText("S" + juce::String(sendIndex + 1), juce::dontSendNotification);
}

void SendKnob::setSendLevel(float level)
{
    knob.setValue(static_cast<double>(level), juce::dontSendNotification);
}

float SendKnob::getSendLevel() const
{
    return static_cast<float>(knob.getValue());
}

void SendKnob::setEnabled(bool enabled)
{
    knob.setEnabled(enabled);
    knob.setAlpha(enabled ? 1.0f : 0.5f);
}

//==============================================================================
// ChannelStrip Implementation
//==============================================================================

ChannelStrip::ChannelStrip()
{
    // Name label at top
    nameLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setFont(juce::Font(11.0f));
    addAndMakeVisible(nameLabel);

    // Volume fader (vertical)
    volumeSlider.setSliderStyle(juce::Slider::LinearVertical);
    volumeSlider.setRange(-60.0, 6.0, 0.1);
    volumeSlider.setValue(0.0);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    volumeSlider.setColour(juce::Slider::textBoxTextColourId, MidiSingLookAndFeel::textColour);
    volumeSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    volumeSlider.onValueChange = [this]()
    {
        if (trackPtr != nullptr && onVolumeChanged)
        {
            float db = static_cast<float>(volumeSlider.getValue());
            float linear = juce::Decibels::decibelsToGain(db);
            trackPtr->setVolume(linear);
            onVolumeChanged(trackPtr, linear);
        }
    };
    addAndMakeVisible(volumeSlider);

    // Pan knob
    panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0);
    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    panSlider.onValueChange = [this]()
    {
        if (trackPtr != nullptr && onPanChanged)
        {
            float pan = static_cast<float>(panSlider.getValue());
            trackPtr->setPan(pan);
            onPanChanged(trackPtr, pan);
        }
    };
    addAndMakeVisible(panSlider);

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

    // FX button (opens full effect rack)
    fxButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    fxButton.onClick = [this]()
    {
        if (trackPtr != nullptr)
        {
            auto* audioTrack = dynamic_cast<AudioTrack*>(trackPtr);
            if (audioTrack)
            {
                auto rack = std::make_unique<EffectRackUI>(audioTrack->getEffectChain());
                rack->setSize(300, 400);
                juce::CallOutBox::launchAsynchronously(std::move(rack), fxButton.getScreenBounds(), nullptr);
            }
        }
    };
    addAndMakeVisible(fxButton);

    // Inserts label
    insertsLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
    insertsLabel.setJustificationType(juce::Justification::centred);
    insertsLabel.setFont(juce::Font(9.0f));
    insertsLabel.setText("INS", juce::dontSendNotification);
    addAndMakeVisible(insertsLabel);

    // Create insert slot buttons
    for (int i = 0; i < NUM_INSERT_SLOTS; ++i)
    {
        insertSlots[static_cast<size_t>(i)] = std::make_unique<InsertSlotButton>(i);
        insertSlots[static_cast<size_t>(i)]->onClicked = [this](int slotIndex) { handleInsertSlotClick(slotIndex); };
        insertSlots[static_cast<size_t>(i)]->onRightClicked = [this](int slotIndex, const juce::Point<int>& pos) { handleInsertSlotRightClick(slotIndex, pos); };
        addAndMakeVisible(insertSlots[static_cast<size_t>(i)].get());
    }

    // Sends label
    sendsLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
    sendsLabel.setJustificationType(juce::Justification::centred);
    sendsLabel.setFont(juce::Font(9.0f));
    sendsLabel.setText("SND", juce::dontSendNotification);
    addAndMakeVisible(sendsLabel);

    // Create send knobs
    for (int i = 0; i < NUM_SEND_KNOBS; ++i)
    {
        sendKnobs[static_cast<size_t>(i)] = std::make_unique<SendKnob>(i);
        sendKnobs[static_cast<size_t>(i)]->onLevelChanged = [this](int sendIndex, float level) { handleSendLevelChange(sendIndex, level); };
        sendKnobs[static_cast<size_t>(i)]->setEnabled(false);  // Disabled until aux tracks exist
        addAndMakeVisible(sendKnobs[static_cast<size_t>(i)].get());
    }
}

void ChannelStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.fillAll(MidiSingLookAndFeel::backgroundMid);

    // Track colour strip at top
    if (trackPtr != nullptr)
    {
        g.setColour(trackPtr->getColour());
        g.fillRect(bounds.removeFromTop(4));
    }

    // Level meters (simple visualisation on sides)
    auto meterArea = bounds.reduced(4);
    int meterWidth = 4;
    int clipIndicatorHeight = 6;

    // Left meter
    auto leftMeterBounds = meterArea.removeFromLeft(meterWidth);

    // Draw clip indicator at top of left meter
    auto leftClipBounds = leftMeterBounds.removeFromTop(clipIndicatorHeight);
    if (clipHoldL)
    {
        g.setColour(MidiSingLookAndFeel::recordColour);  // Red for clipping
    }
    else
    {
        g.setColour(MidiSingLookAndFeel::sliderBackground.brighter(0.1f));
    }
    g.fillRect(leftClipBounds);

    // Draw left meter background and level
    g.setColour(MidiSingLookAndFeel::sliderBackground);
    g.fillRect(leftMeterBounds);
    float leftHeight = leftMeterBounds.getHeight() * levelL;

    // Color gradient: green for normal, yellow/orange approaching 0dB, red at clipping
    juce::Colour leftMeterColour = MidiSingLookAndFeel::playColour;
    if (levelL > 0.9f)
        leftMeterColour = juce::Colour(0xffd4a85a);  // Orange/yellow warning
    if (levelL >= 1.0f)
        leftMeterColour = MidiSingLookAndFeel::recordColour;  // Red for clipping

    g.setColour(leftMeterColour);
    g.fillRect(leftMeterBounds.removeFromBottom(static_cast<int>(leftHeight)));

    // Right meter
    auto rightMeterBounds = meterArea.removeFromRight(meterWidth);

    // Draw clip indicator at top of right meter
    auto rightClipBounds = rightMeterBounds.removeFromTop(clipIndicatorHeight);
    if (clipHoldR)
    {
        g.setColour(MidiSingLookAndFeel::recordColour);  // Red for clipping
    }
    else
    {
        g.setColour(MidiSingLookAndFeel::sliderBackground.brighter(0.1f));
    }
    g.fillRect(rightClipBounds);

    // Draw right meter background and level
    g.setColour(MidiSingLookAndFeel::sliderBackground);
    g.fillRect(rightMeterBounds);
    float rightHeight = rightMeterBounds.getHeight() * levelR;

    // Color gradient: green for normal, yellow/orange approaching 0dB, red at clipping
    juce::Colour rightMeterColour = MidiSingLookAndFeel::playColour;
    if (levelR > 0.9f)
        rightMeterColour = juce::Colour(0xffd4a85a);  // Orange/yellow warning
    if (levelR >= 1.0f)
        rightMeterColour = MidiSingLookAndFeel::recordColour;  // Red for clipping

    g.setColour(rightMeterColour);
    g.fillRect(rightMeterBounds.removeFromBottom(static_cast<int>(rightHeight)));

    // Border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRect(getLocalBounds());
}

void ChannelStrip::resized()
{
    auto bounds = getLocalBounds().reduced(6);
    bounds.removeFromTop(8); // Space for colour strip

    // Name at top
    nameLabel.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(2);

    // Insert slots section
    insertsLabel.setBounds(bounds.removeFromTop(12));

    // Insert slots in 2 columns of 4
    int insertSlotHeight = 14;
    int insertSlotSpacing = 2;
    for (int row = 0; row < 4; ++row)
    {
        auto rowBounds = bounds.removeFromTop(insertSlotHeight);
        int halfWidth = rowBounds.getWidth() / 2;
        insertSlots[static_cast<size_t>(row * 2)]->setBounds(rowBounds.removeFromLeft(halfWidth).reduced(insertSlotSpacing, 0));
        insertSlots[static_cast<size_t>(row * 2 + 1)]->setBounds(rowBounds.reduced(insertSlotSpacing, 0));
        bounds.removeFromTop(1);
    }

    bounds.removeFromTop(2);

    // Pan knob
    auto panBounds = bounds.removeFromTop(36);
    panSlider.setBounds(panBounds.reduced(5, 0));
    bounds.removeFromTop(2);

    // Mute/Solo/FX buttons grid
    auto buttonRow = bounds.removeFromTop(44); // 2 rows of buttons

    // Row 1: Mute/Solo
    auto row1 = buttonRow.removeFromTop(20);
    muteButton.setBounds(row1.removeFromLeft(row1.getWidth() / 2).reduced(2, 0));
    soloButton.setBounds(row1.reduced(2, 0));

    buttonRow.removeFromTop(2);

    // Row 2: FX
    fxButton.setBounds(buttonRow.removeFromTop(20).reduced(2, 0));

    bounds.removeFromTop(4);

    // Send knobs section
    sendsLabel.setBounds(bounds.removeFromTop(12));

    // Send knobs in 2x2 grid
    int sendKnobSize = 32;
    for (int row = 0; row < 2; ++row)
    {
        auto rowBounds = bounds.removeFromTop(sendKnobSize);
        int halfWidth = rowBounds.getWidth() / 2;
        sendKnobs[static_cast<size_t>(row * 2)]->setBounds(rowBounds.removeFromLeft(halfWidth).reduced(2, 0));
        sendKnobs[static_cast<size_t>(row * 2 + 1)]->setBounds(rowBounds.reduced(2, 0));
        bounds.removeFromTop(2);
    }

    bounds.removeFromTop(2);

    // Volume fader fills the rest
    bounds.removeFromLeft(10); // Space for meter
    bounds.removeFromRight(10); // Space for meter
    volumeSlider.setBounds(bounds);
}

void ChannelStrip::setTrack(Track* track)
{
    trackPtr = track;
    updateFromTrack();
    updateInsertSlots();
    updateSendKnobs();
}

void ChannelStrip::setLevel(float left, float right)
{
    levelL = juce::jlimit(0.0f, 1.0f, left);
    levelR = juce::jlimit(0.0f, 1.0f, right);
    repaint();
}

void ChannelStrip::setLevelWithClipping(float left, float right, bool clippingL, bool clippingR)
{
    levelL = juce::jlimit(0.0f, 1.0f, left);
    levelR = juce::jlimit(0.0f, 1.0f, right);

    juce::int64 currentTime = juce::Time::currentTimeMillis();

    // Set clip hold if clipping detected
    if (clippingL)
    {
        clipHoldL = true;
        clipHoldTimeL = currentTime;
    }
    else if (clipHoldL)
    {
        // Check if hold duration has elapsed
        if (currentTime - clipHoldTimeL > clipHoldDurationMs)
        {
            clipHoldL = false;
        }
    }

    if (clippingR)
    {
        clipHoldR = true;
        clipHoldTimeR = currentTime;
    }
    else if (clipHoldR)
    {
        // Check if hold duration has elapsed
        if (currentTime - clipHoldTimeR > clipHoldDurationMs)
        {
            clipHoldR = false;
        }
    }

    repaint();
}

void ChannelStrip::resetClipIndicators()
{
    clipHoldL = false;
    clipHoldR = false;
    clipHoldTimeL = 0;
    clipHoldTimeR = 0;
    repaint();
}

void ChannelStrip::setAvailableAuxTracks(const juce::StringArray& auxNames)
{
    auxTrackNames = auxNames;

    // Update send knob names and enabled states
    for (int i = 0; i < NUM_SEND_KNOBS; ++i)
    {
        if (i < auxNames.size())
        {
            sendKnobs[static_cast<size_t>(i)]->setAuxName(auxNames[i]);
            sendKnobs[static_cast<size_t>(i)]->setEnabled(true);
        }
        else
        {
            sendKnobs[static_cast<size_t>(i)]->setAuxName("");
            sendKnobs[static_cast<size_t>(i)]->setEnabled(false);
        }
    }
}

void ChannelStrip::refreshInsertAndSendState()
{
    updateInsertSlots();
    updateSendKnobs();
}

void ChannelStrip::updateFromTrack()
{
    if (trackPtr == nullptr)
        return;

    nameLabel.setText(trackPtr->getName(), juce::dontSendNotification);
    volumeSlider.setValue(juce::Decibels::gainToDecibels(trackPtr->getVolume()), juce::dontSendNotification);
    panSlider.setValue(trackPtr->getPan(), juce::dontSendNotification);

    muteButton.setColour(juce::TextButton::buttonColourId,
                         trackPtr->isMuted() ? juce::Colour(0xffd4a85a)
                                             : MidiSingLookAndFeel::buttonBackground);

    soloButton.setColour(juce::TextButton::buttonColourId,
                         trackPtr->isSoloed() ? juce::Colour(0xff5ad4cf)
                                              : MidiSingLookAndFeel::buttonBackground);

    // Enable FX only for AudioTracks
    fxButton.setEnabled(dynamic_cast<AudioTrack*>(trackPtr) != nullptr);
}

void ChannelStrip::handleInsertSlotClick(int slotIndex)
{
    if (trackPtr != nullptr && onInsertSlotClicked)
    {
        onInsertSlotClicked(trackPtr, slotIndex);
    }
}

void ChannelStrip::handleInsertSlotRightClick(int slotIndex, const juce::Point<int>& position)
{
    showInsertContextMenu(slotIndex, position);
}

void ChannelStrip::handleSendLevelChange(int sendIndex, float level)
{
    if (trackPtr == nullptr)
        return;

    auto& sendManager = trackPtr->getSendManager();

    // Find or create send to this aux index
    int existingSendIndex = sendManager.findSendToAux(sendIndex);

    if (existingSendIndex >= 0)
    {
        // Update existing send level
        sendManager.setSendLevel(existingSendIndex, level);
    }
    else if (level > 0.0f)
    {
        // Create new send if level is non-zero
        sendManager.addSend(sendIndex, level, false);
    }

    if (onSendLevelChanged)
        onSendLevelChanged(trackPtr, sendIndex, level);
}

void ChannelStrip::showInsertContextMenu(int slotIndex, const juce::Point<int>& position)
{
    auto* audioTrack = dynamic_cast<AudioTrack*>(trackPtr);
    if (audioTrack == nullptr)
        return;

    auto* effect = audioTrack->getInsert(slotIndex);
    if (effect == nullptr)
        return;

    juce::PopupMenu menu;

    // Bypass toggle
    bool isBypassed = audioTrack->isInsertBypassed(slotIndex);
    menu.addItem(1, isBypassed ? "Enable" : "Bypass", true, isBypassed);

    // Remove option
    menu.addItem(2, "Remove");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(position, position)),
                       [this, slotIndex, audioTrack](int result)
                       {
                           if (result == 1)
                           {
                               // Toggle bypass
                               bool currentBypass = audioTrack->isInsertBypassed(slotIndex);
                               audioTrack->setInsertBypassed(slotIndex, !currentBypass);
                               updateInsertSlots();
                           }
                           else if (result == 2)
                           {
                               // Remove effect
                               audioTrack->removeInsert(slotIndex);
                               updateInsertSlots();
                           }
                       });
}

void ChannelStrip::updateInsertSlots()
{
    auto* audioTrack = dynamic_cast<AudioTrack*>(trackPtr);

    for (int i = 0; i < NUM_INSERT_SLOTS; ++i)
    {
        auto& slot = insertSlots[static_cast<size_t>(i)];

        if (audioTrack != nullptr)
        {
            auto* effect = audioTrack->getInsert(i);
            if (effect != nullptr)
            {
                slot->setEmpty(false);
                slot->setEffectName(effect->getName());
                slot->setBypassed(audioTrack->isInsertBypassed(i));
            }
            else
            {
                slot->setEmpty(true);
                slot->setEffectName("");
                slot->setBypassed(false);
            }
        }
        else
        {
            slot->setEmpty(true);
            slot->setEffectName("");
            slot->setBypassed(false);
        }
    }
}

void ChannelStrip::updateSendKnobs()
{
    if (trackPtr == nullptr)
        return;

    auto& sendManager = trackPtr->getSendManager();

    for (int i = 0; i < NUM_SEND_KNOBS; ++i)
    {
        // Find if there's a send to this aux index
        int sendIndex = sendManager.findSendToAux(i);

        if (sendIndex >= 0)
        {
            sendKnobs[static_cast<size_t>(i)]->setSendLevel(sendManager.getSendLevel(sendIndex));
        }
        else
        {
            sendKnobs[static_cast<size_t>(i)]->setSendLevel(0.0f);
        }
    }
}
