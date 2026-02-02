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
    void mouseDown(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void setTrack(Track* track);
    Track* getTrack() const { return trackPtr; }

    // Callbacks
    std::function<void(Track*)> onMuteChanged;
    std::function<void(Track*)> onSoloChanged;
    std::function<void(Track*)> onArmChanged;
    std::function<void(Track*)> onTrackSelected;
    std::function<void(Track*)> onDeleteTrack;

private:
    void updateFromTrack();
    void showContextMenu();
    void showDeleteConfirmation();

    Track* trackPtr = nullptr;

    juce::Label nameLabel;
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };
    juce::TextButton armButton { "R" };

    // Context menu item IDs
    enum MenuItemIds
    {
        DeleteTrackId = 1
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackHeader)
};
