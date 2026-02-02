#include "ChannelStrip.h"
#include "../Audio/AudioTrack.h"
#include "EffectRackUI.h"

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

    // FX button
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
    bounds.removeFromTop(4);

    // Pan knob
    auto panBounds = bounds.removeFromTop(40);
    panSlider.setBounds(panBounds.reduced(5, 0));
    bounds.removeFromTop(4);

    // Mute/Solo/FX buttons grid
    auto buttonRow = bounds.removeFromTop(48); // 2 rows of buttons
    
    // Row 1: Mute/Solo
    auto row1 = buttonRow.removeFromTop(22);
    muteButton.setBounds(row1.removeFromLeft(row1.getWidth() / 2).reduced(2, 0));
    soloButton.setBounds(row1.reduced(2, 0));
    
    buttonRow.removeFromTop(4);

    // Row 2: FX
    fxButton.setBounds(buttonRow.removeFromTop(22).reduced(2, 0));
    
    bounds.removeFromTop(4);

    // Volume fader fills the rest
    bounds.removeFromLeft(10); // Space for meter
    bounds.removeFromRight(10); // Space for meter
    volumeSlider.setBounds(bounds);
}

void ChannelStrip::setTrack(Track* track)
{
    trackPtr = track;
    updateFromTrack();
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
