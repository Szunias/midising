#pragma once

#include "../Timeline/Track.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <array>

// Forward declarations
class AudioTrack;
class EffectChain;

/**
 * VUMeter component with peak hold for professional metering.
 * Features:
 *   - Smooth VU ballistics (300ms rise, 300ms fall)
 *   - Peak hold with configurable decay time
 *   - Color gradient (green -> yellow -> red)
 *   - Clip indicator at top
 *   - Vertical orientation optimized for channel strips
 */
class VUMeter : public juce::Component, public juce::Timer
{
public:
    VUMeter();
    ~VUMeter() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    /**
     * Set the current input level (0.0 to 1.0+).
     * The meter applies VU ballistics internally.
     */
    void setLevel(float level);

    /**
     * Set level with explicit clipping flag for true peak detection.
     */
    void setLevelWithClipping(float level, bool clipping);

    /**
     * Reset the peak hold indicator.
     */
    void resetPeakHold();

    /**
     * Reset the clip indicator.
     */
    void resetClipIndicator();

    /**
     * Check if clip indicator is showing.
     */
    bool hasClipIndicator() const { return clipHold; }

    /**
     * Enable/disable peak hold display.
     */
    void setPeakHoldEnabled(bool enabled) { peakHoldEnabled = enabled; }

    /**
     * Set peak hold time in milliseconds (default 2000ms).
     */
    void setPeakHoldTimeMs(int ms) { peakHoldTimeMs = ms; }

    /**
     * Set clip hold time in milliseconds (default 2000ms).
     */
    void setClipHoldTimeMs(int ms) { clipHoldTimeMs = ms; }

    // Mouse interaction to reset indicators
    void mouseDown(const juce::MouseEvent& event) override;

private:
    // Current displayed level (after ballistics)
    float displayLevel = 0.0f;

    // Target level (input)
    float targetLevel = 0.0f;

    // Peak hold values
    float peakLevel = 0.0f;
    juce::int64 peakHoldTime = 0;
    bool peakHoldEnabled = true;
    int peakHoldTimeMs = 2000;

    // Clip indicator
    bool clipHold = false;
    juce::int64 clipHoldTime = 0;
    int clipHoldTimeMs = 2000;

    // VU ballistics constants (in normalized values per timer tick)
    static constexpr float vuRiseRate = 0.15f;   // Fast rise
    static constexpr float vuFallRate = 0.08f;   // Slower fall
    static constexpr int timerIntervalMs = 30;   // ~33fps update rate

    // Threshold levels for color changes
    static constexpr float yellowThreshold = 0.7f;   // -3dB
    static constexpr float redThreshold = 0.9f;      // -0.9dB

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUMeter)
};

/**
 * InsertSlotButton represents a single insert effect slot in the channel strip.
 * Click to add/edit effect, right-click for bypass/remove menu.
 */
class InsertSlotButton : public juce::Component
{
public:
    InsertSlotButton(int slotIndex);
    ~InsertSlotButton() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setEffectName(const juce::String& name);
    void setEmpty(bool isEmpty);
    void setBypassed(bool bypassed);

    int getSlotIndex() const { return slotIndex; }

    // Callbacks
    std::function<void(int slotIndex)> onClicked;
    std::function<void(int slotIndex, const juce::Point<int>& position)> onRightClicked;

private:
    int slotIndex;
    juce::String effectName;
    bool empty = true;
    bool bypassed = false;
    bool mouseOver = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InsertSlotButton)
};

/**
 * SendKnob represents a send level control to an aux track.
 */
class SendKnob : public juce::Component
{
public:
    SendKnob(int sendIndex);
    ~SendKnob() override = default;

    void resized() override;

    void setAuxName(const juce::String& name);
    void setSendLevel(float level);
    float getSendLevel() const;
    void setEnabled(bool enabled);

    int getSendIndex() const { return sendIndex; }

    // Callback when send level changes
    std::function<void(int sendIndex, float level)> onLevelChanged;

private:
    int sendIndex;
    juce::Slider knob;
    juce::Label nameLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SendKnob)
};

/**
 * ChannelStrip represents a single channel in the mixer.
 * Has volume fader, pan knob, mute/solo buttons, insert slots, send knobs, and level meter.
 *
 * Signal Flow Display:
 *   [Insert Slots] -> [Fader] -> [Pan] -> [Sends] -> [Output]
 *
 * Features:
 *   - 8 insert effect slots (click to add, right-click for menu)
 *   - 4 visible send knobs (expandable if more aux tracks exist)
 *   - Volume fader with level metering
 *   - Pan control
 *   - Mute/Solo buttons
 *   - True peak metering with clip indicators
 */
class ChannelStrip : public juce::Component
{
public:
    /** Maximum number of insert slots displayed */
    static constexpr int NUM_INSERT_SLOTS = 8;
    /** Number of send knobs displayed */
    static constexpr int NUM_SEND_KNOBS = 4;

    ChannelStrip();
    ~ChannelStrip() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setTrack(Track* track);
    Track* getTrack() const { return trackPtr; }

    void setLevel(float left, float right);

    /**
     * Set level with clipping information for true peak metering.
     * Clip indicators stay lit briefly after clipping occurs.
     */
    void setLevelWithClipping(float left, float right, bool clippingL, bool clippingR);

    /**
     * Reset clip indicators (call when user clicks on clip indicator).
     */
    void resetClipIndicators();

    /**
     * Reset peak hold indicators.
     */
    void resetPeakHold();

    /**
     * Check if either channel is showing a clip indicator.
     */
    bool hasClipIndicator() const { return leftMeter.hasClipIndicator() || rightMeter.hasClipIndicator(); }

    /**
     * Set the available aux track names for send destinations.
     * @param auxNames List of aux track names
     */
    void setAvailableAuxTracks(const juce::StringArray& auxNames);

    /**
     * Refresh the insert slots and send knobs from the track data.
     * Call this after effects are added/removed or send levels change.
     */
    void refreshInsertAndSendState();

    // Callbacks
    std::function<void(Track*, float)> onVolumeChanged;
    std::function<void(Track*, float)> onPanChanged;
    std::function<void(Track*)> onMuteChanged;
    std::function<void(Track*)> onSoloChanged;
    /** Callback when user wants to add/edit an insert effect */
    std::function<void(Track*, int slotIndex)> onInsertSlotClicked;
    /** Callback when user changes a send level */
    std::function<void(Track*, int sendIndex, float level)> onSendLevelChanged;

private:
    void updateFromTrack();
    void handleInsertSlotClick(int slotIndex);
    void handleInsertSlotRightClick(int slotIndex, const juce::Point<int>& position);
    void handleSendLevelChange(int sendIndex, float level);
    void showInsertContextMenu(int slotIndex, const juce::Point<int>& position);
    void updateInsertSlots();
    void updateSendKnobs();

    Track* trackPtr = nullptr;

    juce::Label nameLabel;
    juce::Slider volumeSlider;
    juce::Slider panSlider;
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };
    juce::TextButton fxButton { "FX" };

    // Insert effect slots
    std::array<std::unique_ptr<InsertSlotButton>, NUM_INSERT_SLOTS> insertSlots;
    juce::Label insertsLabel;
    bool showInsertSlots = true;

    // Send level knobs
    std::array<std::unique_ptr<SendKnob>, NUM_SEND_KNOBS> sendKnobs;
    juce::Label sendsLabel;
    juce::StringArray auxTrackNames;

    // VU Meters (stereo pair)
    VUMeter leftMeter;
    VUMeter rightMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelStrip)
};
