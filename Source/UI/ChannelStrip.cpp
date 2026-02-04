#include "ChannelStrip.h"
#include "../Audio/AudioTrack.h"
#include "../Audio/SendReturn.h"
#include "EffectRackUI.h"

//==============================================================================
// VUMeter Implementation
//==============================================================================

VUMeter::VUMeter()
{
    startTimerHz(static_cast<int>(1000 / timerIntervalMs));
}

void VUMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Calculate clip indicator height
    float clipIndicatorHeight = juce::jmin(8.0f, bounds.getHeight() * 0.04f);

    // Draw clip indicator at top
    auto clipBounds = bounds.removeFromTop(clipIndicatorHeight);
    if (clipHold)
    {
        g.setColour(MidiSingLookAndFeel::recordColour);
    }
    else
    {
        g.setColour(MidiSingLookAndFeel::sliderBackground.brighter(0.1f));
    }
    g.fillRect(clipBounds);

    // Small gap between clip and meter
    bounds.removeFromTop(1.0f);

    // Draw meter background
    g.setColour(MidiSingLookAndFeel::sliderBackground);
    g.fillRect(bounds);

    // Calculate meter height
    float meterHeight = bounds.getHeight();
    float levelHeight = meterHeight * juce::jlimit(0.0f, 1.0f, displayLevel);

    // Draw level meter with gradient colors
    if (levelHeight > 0.0f)
    {
        auto levelBounds = bounds;
        levelBounds = levelBounds.removeFromBottom(levelHeight);

        // Determine color based on level
        juce::Colour meterColour;
        if (displayLevel >= redThreshold)
        {
            // Red zone - approaching clipping
            meterColour = MidiSingLookAndFeel::recordColour;
        }
        else if (displayLevel >= yellowThreshold)
        {
            // Yellow/orange zone - warning
            float t = (displayLevel - yellowThreshold) / (redThreshold - yellowThreshold);
            meterColour = MidiSingLookAndFeel::warningColour.interpolatedWith(
                MidiSingLookAndFeel::recordColour, t);
        }
        else
        {
            // Green zone - safe
            float t = displayLevel / yellowThreshold;
            meterColour = MidiSingLookAndFeel::playColour.interpolatedWith(
                MidiSingLookAndFeel::warningColour, t * 0.3f);
        }

        g.setColour(meterColour);
        g.fillRect(levelBounds);
    }

    // Draw peak hold indicator
    if (peakHoldEnabled && peakLevel > 0.01f)
    {
        float peakY = bounds.getBottom() - (meterHeight * juce::jlimit(0.0f, 1.0f, peakLevel));

        // Peak indicator color (matches level color at that position)
        juce::Colour peakColour;
        if (peakLevel >= redThreshold)
            peakColour = MidiSingLookAndFeel::recordColour;
        else if (peakLevel >= yellowThreshold)
            peakColour = MidiSingLookAndFeel::warningColour;
        else
            peakColour = MidiSingLookAndFeel::playColour;

        g.setColour(peakColour.brighter(0.3f));
        g.fillRect(bounds.getX(), peakY - 2.0f, bounds.getWidth(), 2.0f);
    }

    // Draw subtle scale marks
    g.setColour(MidiSingLookAndFeel::borderColour.withAlpha(0.5f));

    // -6dB mark (0.5 normalized)
    float y6db = bounds.getBottom() - (meterHeight * 0.5f);
    g.drawHorizontalLine(static_cast<int>(y6db), bounds.getX(), bounds.getRight());

    // -12dB mark (0.25 normalized)
    float y12db = bounds.getBottom() - (meterHeight * 0.25f);
    g.drawHorizontalLine(static_cast<int>(y12db), bounds.getX(), bounds.getRight());
}

void VUMeter::resized()
{
    // Nothing special needed
}

void VUMeter::timerCallback()
{
    bool needsRepaint = false;

    // Apply VU ballistics
    if (displayLevel < targetLevel)
    {
        // Rising - use rise rate
        displayLevel = juce::jmin(displayLevel + vuRiseRate, targetLevel);
        needsRepaint = true;
    }
    else if (displayLevel > targetLevel)
    {
        // Falling - use fall rate
        displayLevel = juce::jmax(displayLevel - vuFallRate, targetLevel);
        needsRepaint = true;
    }

    // Update peak hold
    if (peakHoldEnabled)
    {
        juce::int64 currentTime = juce::Time::currentTimeMillis();

        // Update peak if current display level exceeds it
        if (displayLevel > peakLevel)
        {
            peakLevel = displayLevel;
            peakHoldTime = currentTime;
            needsRepaint = true;
        }
        else if (peakLevel > 0.0f && currentTime - peakHoldTime > peakHoldTimeMs)
        {
            // Slowly decay peak after hold time
            peakLevel = juce::jmax(0.0f, peakLevel - 0.02f);
            needsRepaint = true;
        }
    }

    // Update clip hold
    if (clipHold)
    {
        juce::int64 currentTime = juce::Time::currentTimeMillis();
        if (currentTime - clipHoldTime > clipHoldTimeMs)
        {
            clipHold = false;
            needsRepaint = true;
        }
    }

    if (needsRepaint)
        repaint();
}

void VUMeter::setLevel(float level)
{
    targetLevel = juce::jlimit(0.0f, 1.5f, level);  // Allow some headroom for clipping indication

    // Auto-detect clipping
    if (level >= 1.0f)
    {
        clipHold = true;
        clipHoldTime = juce::Time::currentTimeMillis();
    }
}

void VUMeter::setLevelWithClipping(float level, bool clipping)
{
    targetLevel = juce::jlimit(0.0f, 1.5f, level);

    if (clipping)
    {
        clipHold = true;
        clipHoldTime = juce::Time::currentTimeMillis();
    }
}

void VUMeter::resetPeakHold()
{
    peakLevel = 0.0f;
    peakHoldTime = 0;
    repaint();
}

void VUMeter::resetClipIndicator()
{
    clipHold = false;
    clipHoldTime = 0;
    repaint();
}

void VUMeter::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    // Click anywhere on meter resets both peak and clip indicators
    resetPeakHold();
    resetClipIndicator();
}

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

    // VU meters for level display
    addAndMakeVisible(leftMeter);
    addAndMakeVisible(rightMeter);
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

    // Volume fader and VU meters - meters on each side
    int meterWidth = 6;

    // Left VU meter
    auto leftMeterBounds = bounds.removeFromLeft(meterWidth);
    leftMeter.setBounds(leftMeterBounds);

    // Right VU meter
    auto rightMeterBounds = bounds.removeFromRight(meterWidth);
    rightMeter.setBounds(rightMeterBounds);

    // Small gap between meters and fader
    bounds.removeFromLeft(2);
    bounds.removeFromRight(2);

    // Volume fader fills the rest
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
    leftMeter.setLevel(left);
    rightMeter.setLevel(right);
}

void ChannelStrip::setLevelWithClipping(float left, float right, bool clippingL, bool clippingR)
{
    leftMeter.setLevelWithClipping(left, clippingL);
    rightMeter.setLevelWithClipping(right, clippingR);
}

void ChannelStrip::resetClipIndicators()
{
    leftMeter.resetClipIndicator();
    rightMeter.resetClipIndicator();
}

void ChannelStrip::resetPeakHold()
{
    leftMeter.resetPeakHold();
    rightMeter.resetPeakHold();
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
