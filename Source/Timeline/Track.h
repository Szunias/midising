#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <atomic>
#include <vector>
#include <memory>

// Forward declarations
class AutomationLane;
class SendManager;

/**
 * Type of track (Audio or MIDI)
 */
enum class TrackType
{
    Audio,
    MIDI
};

/**
 * Base class for all track types in the DAW.
 * Contains common properties like name, color, volume, pan, mute, solo, and arm.
 */
class Track
{
public:
    Track(const juce::String& name, TrackType type);
    virtual ~Track();

    // Getters
    juce::String getName() const { return name; }
    TrackType getType() const { return type; }
    juce::Colour getColour() const { return colour; }
    float getVolume() const { return volume.load(); }
    float getPan() const { return pan.load(); }
    bool isMuted() const { return muted.load(); }
    bool isSoloed() const { return soloed.load(); }
    bool isArmed() const { return armed.load(); }
    int getHeight() const { return height; }

    // Setters
    void setName(const juce::String& newName) { name = newName; }
    void setColour(juce::Colour newColour) { colour = newColour; }
    void setVolume(float newVolume) { volume.store(juce::jlimit(0.0f, 2.0f, newVolume)); }
    void setPan(float newPan) { pan.store(juce::jlimit(-1.0f, 1.0f, newPan)); }
    void setMuted(bool shouldMute) { muted.store(shouldMute); }
    void setSoloed(bool shouldSolo) { soloed.store(shouldSolo); }
    void setArmed(bool shouldArm) { armed.store(shouldArm); }
    void setHeight(int newHeight) { height = juce::jmax(50, newHeight); }

    // Processing (to be implemented by subclasses)
    virtual void prepareToPlay(double sampleRate, int samplesPerBlock) = 0;
    virtual void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) = 0;
    virtual void releaseResources() = 0;

    // Automation lane management
    AutomationLane* addAutomationLane(const juce::String& parameterName);
    void removeAutomationLane(size_t index);
    size_t getNumAutomationLanes() const { return automationLanes.size(); }
    AutomationLane* getAutomationLane(size_t index);
    const AutomationLane* getAutomationLane(size_t index) const;
    AutomationLane* findAutomationLane(const juce::String& parameterName);
    const AutomationLane* findAutomationLane(const juce::String& parameterName) const;

    // Get automated parameter values at a position
    float getAutomatedVolume(int64_t position) const;
    float getAutomatedPan(int64_t position) const;

    //==========================================================================
    // Send routing to aux tracks
    //==========================================================================

    /**
     * Get the send manager for this track.
     * The send manager handles routing to aux/return tracks.
     */
    SendManager& getSendManager();
    const SendManager& getSendManager() const;

    /**
     * Check if this track has any active sends.
     */
    bool hasActiveSends() const;

    //==========================================================================
    // Output routing to group busses
    //==========================================================================

    /** Special value indicating output goes to master bus (no group routing) */
    static constexpr int OUTPUT_TO_MASTER = -1;

    /**
     * Get the output group bus index.
     * @return Index of the group bus, or OUTPUT_TO_MASTER (-1) for direct to master
     */
    int getOutputGroupBus() const { return outputGroupBusIndex.load(); }

    /**
     * Set the output group bus index.
     * @param groupIndex Index of the group bus, or OUTPUT_TO_MASTER (-1) for direct to master
     */
    void setOutputGroupBus(int groupIndex) { outputGroupBusIndex.store(groupIndex); }

    /**
     * Check if this track outputs to a group bus.
     * @return True if routed to a group, false if direct to master
     */
    bool isRoutedToGroup() const { return outputGroupBusIndex.load() >= 0; }

private:
    juce::String name;
    TrackType type;
    juce::Colour colour;
    int height = 100;

    // Thread-safe audio parameters
    std::atomic<float> volume { 1.0f };    // 0.0 to 2.0 (1.0 = unity gain)
    std::atomic<float> pan { 0.0f };       // -1.0 (left) to 1.0 (right)
    std::atomic<bool> muted { false };
    std::atomic<bool> soloed { false };
    std::atomic<bool> armed { false };

    // Automation lanes for this track
    std::vector<std::unique_ptr<AutomationLane>> automationLanes;

    // Send routing to aux tracks
    std::unique_ptr<SendManager> sendManager;

    // Output routing to group bus (-1 = master bus)
    std::atomic<int> outputGroupBusIndex { OUTPUT_TO_MASTER };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
};
