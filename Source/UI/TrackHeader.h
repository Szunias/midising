#pragma once

#include "../Timeline/Track.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

/**
 * TrackHeader displays track name, mute/solo/arm buttons for the track list.
 */
class TrackHeader : public juce::Component
{
public:
    TrackHeader();
    ~TrackHeader() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setTrack(Track* track);
    Track* getTrack() const { return trackPtr; }

    // Callbacks
    std::function<void(Track*)> onMuteChanged;
    std::function<void(Track*)> onSoloChanged;
    std::function<void(Track*)> onArmChanged;
    std::function<void(Track*)> onTrackSelected;

private:
    void updateFromTrack();

    Track* trackPtr = nullptr;

    juce::Label nameLabel;
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };
    juce::TextButton armButton { "R" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackHeader)
};
