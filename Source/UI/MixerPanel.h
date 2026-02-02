#pragma once

#include "../Timeline/Timeline.h"
#include "../Audio/Mixer.h"
#include "ChannelStrip.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPanel)
};
