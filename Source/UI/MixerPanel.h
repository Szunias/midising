#pragma once

#include "../Timeline/Timeline.h"
#include "../Audio/Mixer.h"
#include "ChannelStrip.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include <cmath>

/**
 * StereoCorrelationMeter displays the stereo correlation coefficient.
 * Visual range: -1 (out of phase) to +1 (mono/identical channels)
 *
 * Display interpretation:
 *   +1: Mono (identical L/R)
 *    0: Uncorrelated stereo
 *   -1: Out of phase (inverted)
 */
class StereoCorrelationMeter : public juce::Component
{
public:
    StereoCorrelationMeter() = default;
    ~StereoCorrelationMeter() override = default;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Background
        g.setColour(MidiSingLookAndFeel::sliderBackground);
        g.fillRoundedRectangle(bounds, 2.0f);

        // Draw scale marks and labels
        float centerX = bounds.getCentreX();
        float meterWidth = bounds.getWidth() - 4.0f;
        float meterLeft = bounds.getX() + 2.0f;

        // Scale positions (normalized 0-1 range mapped to -1 to +1)
        // -1 = left edge, 0 = center, +1 = right edge
        float pos_neg1 = meterLeft;
        float pos_0 = centerX;
        float pos_pos1 = meterLeft + meterWidth;

        // Draw tick marks
        g.setColour(MidiSingLookAndFeel::borderColour);
        float tickTop = bounds.getY() + 2.0f;
        float tickBottom = bounds.getY() + 6.0f;

        // -1, 0, +1 tick marks
        g.drawVerticalLine(static_cast<int>(pos_neg1), tickTop, tickBottom);
        g.drawVerticalLine(static_cast<int>(pos_0), tickTop, tickBottom);
        g.drawVerticalLine(static_cast<int>(pos_pos1), tickTop, tickBottom);

        // Draw the meter bar area
        auto meterArea = bounds.reduced(2.0f, 8.0f);
        meterArea.removeFromTop(2.0f);

        // Determine meter color based on correlation value
        // Green for positive correlation, yellow for low correlation, red for negative
        juce::Colour meterColour;
        if (correlation >= 0.5f)
        {
            // Good correlation (0.5 to 1.0) - Green
            meterColour = MidiSingLookAndFeel::playColour;
        }
        else if (correlation >= 0.0f)
        {
            // Low positive correlation (0 to 0.5) - Yellow
            float t = correlation / 0.5f;
            meterColour = MidiSingLookAndFeel::warningColour.interpolatedWith(
                MidiSingLookAndFeel::playColour, t);
        }
        else if (correlation >= -0.5f)
        {
            // Low negative correlation (-0.5 to 0) - Orange to Yellow
            float t = (correlation + 0.5f) / 0.5f;
            meterColour = MidiSingLookAndFeel::recordColour.interpolatedWith(
                MidiSingLookAndFeel::warningColour, t);
        }
        else
        {
            // Out of phase (-1 to -0.5) - Red
            meterColour = MidiSingLookAndFeel::recordColour;
        }

        // Draw correlation indicator as a filled bar from center
        // The indicator shows where the correlation sits on the -1 to +1 scale
        float indicatorX = centerX + (correlation * meterWidth * 0.5f);
        float indicatorWidth = 4.0f;

        // Draw the indicator
        g.setColour(meterColour);
        g.fillRoundedRectangle(indicatorX - indicatorWidth * 0.5f,
                               meterArea.getY(),
                               indicatorWidth,
                               meterArea.getHeight(), 2.0f);

        // Draw center line (0 correlation reference)
        g.setColour(MidiSingLookAndFeel::textDimColour.withAlpha(0.5f));
        g.drawVerticalLine(static_cast<int>(centerX),
                           meterArea.getY(), meterArea.getBottom());

        // Draw labels at bottom
        g.setColour(MidiSingLookAndFeel::textDimColour);
        g.setFont(8.0f);
        float labelY = bounds.getBottom() - 10.0f;
        g.drawText("-1", static_cast<int>(pos_neg1 - 6), static_cast<int>(labelY), 12, 10,
                   juce::Justification::centred);
        g.drawText("0", static_cast<int>(pos_0 - 6), static_cast<int>(labelY), 12, 10,
                   juce::Justification::centred);
        g.drawText("+1", static_cast<int>(pos_pos1 - 6), static_cast<int>(labelY), 12, 10,
                   juce::Justification::centred);
    }

    /**
     * Set the correlation value (-1.0 to +1.0).
     */
    void setCorrelation(float value)
    {
        float newCorr = juce::jlimit(-1.0f, 1.0f, value);
        if (std::abs(newCorr - correlation) > 0.001f)
        {
            correlation = newCorr;
            repaint();
        }
    }

    float getCorrelation() const { return correlation; }

private:
    float correlation = 1.0f;  // Default to mono (perfect correlation)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoCorrelationMeter)
};

/**
 * MixerPanel displays all channel strips for mixing.
 * Shows volume faders, pan knobs, and level meters for each track.
 */
class MixerPanel : public juce::Component,
                   public juce::Timer
{
public:
    MixerPanel();
    ~MixerPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setTimeline(Timeline* timeline) { timelinePtr = timeline; updateChannelStrips(); }
    void setMixer(Mixer* mixer) { mixerPtr = mixer; }

    static constexpr int CHANNEL_WIDTH = 80;
    static constexpr int MASTER_WIDTH = 100;

private:
    void updateChannelStrips();

    Timeline* timelinePtr = nullptr;
    Mixer* mixerPtr = nullptr;

    std::vector<std::unique_ptr<ChannelStrip>> channelStrips;
    std::unique_ptr<ChannelStrip> masterStrip;

    // Stereo correlation meter for master channel
    StereoCorrelationMeter correlationMeter;
    juce::Label correlationLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPanel)
};
