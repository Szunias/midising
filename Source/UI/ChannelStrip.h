#pragma once

#include "../Timeline/Track.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

/**
 * ChannelStrip represents a single channel in the mixer.
 * Has volume fader, pan knob, mute/solo buttons, and level meter.
 */
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip();
    ~ChannelStrip() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setTrack(Track* track);
    Track* getTrack() const { return trackPtr; }

    void setLevel(float left, float right);

    // Callbacks
    std::function<void(Track*, float)> onVolumeChanged;
    std::function<void(Track*, float)> onPanChanged;
    std::function<void(Track*)> onMuteChanged;
    std::function<void(Track*)> onSoloChanged;

private:
    void updateFromTrack();

    Track* trackPtr = nullptr;

    juce::Label nameLabel;
    juce::Slider volumeSlider;
    juce::Slider panSlider;
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };
    juce::TextButton fxButton { "FX" };

    // Level meters
    float levelL = 0.0f;
    float levelR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelStrip)
};
