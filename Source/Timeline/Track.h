#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>

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
 * Automation recording/playback modes for tracks.
 * These modes control how automation data is read and written during playback.
 */
enum class AutomationMode
{
    Off,    ///< Automation is disabled - parameters use manual values only
    Read,   ///< Plays back automation data but doesn't record new automation
    Write,  ///< Records automation continuously, overwriting existing data
    Touch,  ///< Records only while parameter is being adjusted, then returns to playback
    Latch   ///< Like Touch, but continues recording last value after release until stop
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
    bool isPhaseInverted() const { return phaseInverted.load(); }
    float getStereoWidth() const { return stereoWidth.load(); }
    bool isFrozen() const { return frozen.load(); }
    juce::File getFrozenFile() const { return frozenAudioFile; }

    // Setters
    void setName(const juce::String& newName) { name = newName; }
    void setColour(juce::Colour newColour) { colour = newColour; }
    void setVolume(float newVolume) { volume.store(juce::jlimit(0.0f, 2.0f, newVolume)); }
    void setPan(float newPan) { pan.store(juce::jlimit(-1.0f, 1.0f, newPan)); }
    void setMuted(bool shouldMute) { muted.store(shouldMute); }
    void setSoloed(bool shouldSolo) { soloed.store(shouldSolo); }
    void setArmed(bool shouldArm) { armed.store(shouldArm); }
    void setHeight(int newHeight) { height = juce::jmax(50, newHeight); }
    void setPhaseInverted(bool shouldInvert) { phaseInverted.store(shouldInvert); }
    void setStereoWidth(float newWidth) { stereoWidth.store(juce::jlimit(0.0f, 2.0f, newWidth)); }
    void setFrozen(bool shouldFreeze, const juce::File& frozenFile = {})
    {
        frozen.store(shouldFreeze);
        frozenAudioFile = frozenFile;
    }

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
    // Automation mode control
    //==========================================================================

    /**
     * Get the current automation mode for this track.
     * @return The current automation mode (Off/Read/Write/Touch/Latch)
     */
    AutomationMode getAutomationMode() const { return automationMode.load(); }

    /**
     * Set the automation mode for this track.
     * @param mode The automation mode to set (Off/Read/Write/Touch/Latch)
     */
    void setAutomationMode(AutomationMode mode) { automationMode.store(mode); }

    /**
     * Check if automation is being read (played back) on this track.
     * @return True if mode is Read, Touch, or Latch (modes that read automation)
     */
    bool isAutomationReading() const;

    /**
     * Check if automation is being written (recorded) on this track.
     * @return True if mode is Write, Touch, or Latch (modes that can write automation)
     */
    bool isAutomationWriting() const;

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

    //==========================================================================
    // Per-track CPU usage monitoring
    //==========================================================================

    /**
     * Get the current CPU load for this track's processing.
     * @return CPU load as a value from 0.0 to 1.0+ (>1.0 indicates overload)
     */
    double getCpuLoad() const { return cpuLoad.load(); }

    /**
     * Get the average processing time for this track in milliseconds.
     */
    double getAverageProcessingTimeMs() const { return averageProcessingTimeMs.load(); }

    /**
     * Update the CPU load metrics after processing a block.
     * Should be called from processBlock implementations.
     * @param processingTimeMs The time taken to process the block in milliseconds
     * @param bufferDurationMs The available time for processing (buffer size / sample rate * 1000)
     */
    void updateCpuLoad(double processingTimeMs, double bufferDurationMs);

    /**
     * Reset the CPU load metrics to zero.
     */
    void resetCpuLoad();

    /**
     * Start timing for processBlock. Call at the beginning of processBlock.
     */
    void startProcessingTimer();

    /**
     * Stop timing and update CPU metrics. Call at the end of processBlock.
     * @param bufferDurationMs The available time for processing
     */
    void stopProcessingTimer(double bufferDurationMs);

protected:
    // High-resolution clock for timing measurements
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point processingStartTime;

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
    std::atomic<bool> phaseInverted { false };   // Invert phase (multiply by -1)
    std::atomic<float> stereoWidth { 1.0f };     // 0.0 (mono) to 2.0 (widened), 1.0 = normal
    std::atomic<bool> frozen { false };          // Track is frozen (uses rendered audio file)
    juce::File frozenAudioFile;                  // Path to frozen audio file

    // Automation lanes for this track
    std::vector<std::unique_ptr<AutomationLane>> automationLanes;

    // Automation mode (Off/Read/Write/Touch/Latch)
    std::atomic<AutomationMode> automationMode { AutomationMode::Read };

    // Send routing to aux tracks
    std::unique_ptr<SendManager> sendManager;

    // Output routing to group bus (-1 = master bus)
    std::atomic<int> outputGroupBusIndex { OUTPUT_TO_MASTER };

    // Per-track CPU usage monitoring
    std::atomic<double> cpuLoad { 0.0 };
    std::atomic<double> averageProcessingTimeMs { 0.0 };

    // Exponential moving average coefficient for CPU load smoothing
    static constexpr double kCpuLoadSmoothing = 0.95;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
};
