#include "MixerPanel.h"

MixerPanel::MixerPanel()
{
    // Master channel strip
    masterStrip = std::make_unique<ChannelStrip>();
    addAndMakeVisible(masterStrip.get());

    // Stereo correlation meter for master channel
    addAndMakeVisible(correlationMeter);

    // Correlation label
    correlationLabel.setText("CORR", juce::dontSendNotification);
    correlationLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
    correlationLabel.setJustificationType(juce::Justification::centred);
    correlationLabel.setFont(juce::Font(9.0f));
    addAndMakeVisible(correlationLabel);

    startTimer(50); // 20 FPS for meter updates
}

MixerPanel::~MixerPanel()
{
    stopTimer();
}

void MixerPanel::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark);

    // Label for master section
    g.setColour(MidiSingLookAndFeel::textDimColour);
    g.setFont(10.0f);
    g.drawText("MASTER", getWidth() - MASTER_WIDTH, 4, MASTER_WIDTH, 16,
               juce::Justification::centred);
}

void MixerPanel::resized()
{
    updateChannelStrips();

    auto bounds = getLocalBounds();

    // Master section on right
    auto masterBounds = bounds.removeFromRight(MASTER_WIDTH);
    masterBounds.removeFromTop(20); // Space for "MASTER" label

    // Stereo correlation meter at bottom of master section
    auto correlationBounds = masterBounds.removeFromBottom(40);
    correlationLabel.setBounds(correlationBounds.removeFromTop(12));
    correlationMeter.setBounds(correlationBounds.reduced(4, 2));

    // Master strip fills the rest
    masterStrip->setBounds(masterBounds);

    // Channel strips fill the rest
    bounds.removeFromTop(20);
    int x = 0;
    for (auto& strip : channelStrips)
    {
        strip->setBounds(x, bounds.getY(), CHANNEL_WIDTH, bounds.getHeight());
        x += CHANNEL_WIDTH;
    }
}

void MixerPanel::timerCallback()
{
    if (mixerPtr != nullptr)
    {
        // Update master meters
        masterStrip->setLevel(mixerPtr->getPeakLevel(0), mixerPtr->getPeakLevel(1));

        // Update stereo correlation meter
        correlationMeter.setCorrelation(mixerPtr->getStereoCorrelation());
    }

    // Note: Individual track meters would need per-track peak tracking
    // For now, tracks just show 0 levels
}

void MixerPanel::updateChannelStrips()
{
    if (timelinePtr == nullptr)
        return;

    // Ensure we have the right number of channel strips
    while (static_cast<int>(channelStrips.size()) < timelinePtr->getNumTracks())
    {
        auto strip = std::make_unique<ChannelStrip>();
        addAndMakeVisible(strip.get());
        channelStrips.push_back(std::move(strip));
    }

    while (static_cast<int>(channelStrips.size()) > timelinePtr->getNumTracks())
    {
        channelStrips.pop_back();
    }

    // Configure strips
    for (size_t i = 0; i < channelStrips.size(); ++i)
    {
        channelStrips[i]->setTrack(timelinePtr->getTrack(static_cast<int>(i)));
    }
}
