#pragma once

#include "../Timeline/Track.h"
#include "../Audio/AudioTrack.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

/**
 * TrackHeader displays track name, mute/solo/arm/automation buttons for the track list.
 * Also shows per-track CPU usage for performance monitoring.
 */
class TrackHeader : public juce::Component,
                    private juce::Timer
{
public:
    TrackHeader();
    ~TrackHeader() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void setTrack(Track* track);
    Track* getTrack() const { return trackPtr; }
    void setTrackIndex(int index) { trackIndex = index; }
    int getTrackIndex() const { return trackIndex; }

    // Callbacks
    std::function<void(Track*)> onMuteChanged;
    std::function<void(Track*)> onSoloChanged;
    std::function<void(Track*)> onArmChanged;
    std::function<void(Track*)> onInputMonitoringChanged;
    std::function<void(Track*)> onTrackSelected;
    std::function<void(Track*)> onDeleteTrack;
    std::function<void(int, const juce::String&)> onToggleAutomationLane;
    std::function<void(Track*)> onTrackPropertiesChanged;

private:
    void updateFromTrack();
    void showContextMenu(juce::Point<int> screenPosition);
    void showDeleteConfirmation();
    void showAutomationMenu();
    void showPropertiesDialog();

    Track* trackPtr = nullptr;
    int trackIndex = -1;

    juce::Label nameLabel;
    juce::Label cpuLabel;  // CPU usage display
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };
    juce::TextButton armButton { "R" };
    juce::TextButton monitorButton { "I" };  // Input monitoring toggle button
    juce::TextButton autoButton { "A" };  // Automation toggle button

    void updateCpuDisplay();

    // Context menu item IDs
    enum MenuItemIds
    {
        DeleteTrackId = 1,
        ShowVolumeAutomationId = 10,
        ShowPanAutomationId = 11
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackHeader)
};
