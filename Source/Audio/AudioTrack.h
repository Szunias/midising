#pragma once

#include "../Timeline/Track.h"
#include "../Timeline/Region.h"
#include "../Timeline/AutomationTarget.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include "../Effects/EffectChain.h"
#include "TimeStretch.h"
#include "PitchShift.h"

/**
 * Input channel configuration for audio tracks.
 * Specifies which physical input channels to route to the track.
 */
struct InputChannelConfig
{
    int leftChannel = 0;   // Physical input channel index for left (0-based)
    int rightChannel = 1;  // Physical input channel index for right (0-based, -1 for mono)
    bool isMono = false;   // If true, only use leftChannel
};

/**
 * AudioTrack is a track that plays and records audio.
 * Contains audio regions and handles audio playback/recording.
 *
 * Signal Flow:
 *   Input (Regions/Recording) -> Insert FX Slots -> [Output to Mixer for Volume/Pan]
 *
 * The mixer then applies: Fader (Volume) -> Pan -> Bus Sends -> Output
 */
class AudioTrack : public Track
{
public:
    /** Maximum number of insert effect slots per channel strip */
    static constexpr int MAX_INSERT_SLOTS = 8;

    AudioTrack(const juce::String& name = "Audio Track");
    ~AudioTrack() override = default;

    // Track interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;

    // Region management
    void addRegion(std::unique_ptr<AudioRegion> region);
    void removeRegion(int index);
    AudioRegion* getRegion(int index);
    const AudioRegion* getRegion(int index) const { return regions[static_cast<size_t>(index)].get(); }
    int getNumRegions() const { return static_cast<int>(regions.size()); }
    void clearRegions();

    // Input routing
    void setInputChannels(int leftChannel, int rightChannel = -1);
    void setInputChannelConfig(const InputChannelConfig& config);
    InputChannelConfig getInputChannelConfig() const;
    int getInputChannelLeft() const { return inputConfig.leftChannel; }
    int getInputChannelRight() const { return inputConfig.rightChannel; }
    bool isMonoInput() const { return inputConfig.isMono; }

    // Input monitoring - process input signal when armed
    void processInputSignal(const juce::AudioBuffer<float>& inputBuffer, int numSamples);

    // Input metering - returns current input level (0.0 to 1.0)
    float getInputLevel() const { return inputLevel.load(); }
    float getInputLevelLeft() const { return inputLevelLeft.load(); }
    float getInputLevelRight() const { return inputLevelRight.load(); }

    // Input monitoring - hear input through speakers when armed
    void setInputMonitoringEnabled(bool enabled) { inputMonitoringEnabled.store(enabled); }
    bool isInputMonitoringEnabled() const { return inputMonitoringEnabled.load(); }

    // Get the last processed input buffer for monitoring output
    // Returns true if monitoring buffer is valid and contains data
    bool getMonitoringBuffer(juce::AudioBuffer<float>& outputBuffer) const;

    // Latency compensation for recording
    void setLatencyCompensationSamples(int samples) { latencyCompensationSamples = samples; }
    int getLatencyCompensationSamples() const { return latencyCompensationSamples; }

    // Recording
    void startRecording(int64_t startPosition);
    void stopRecording();
    bool isCurrentlyRecording() const { return recording; }

    // Record audio sample (call from audio thread during recording)
    void recordSample(const float* leftChannel, const float* rightChannel, int numSamples);

    // Load audio from file and create region
    bool loadAudioFile(const juce::File& file, int64_t startPosition);

    // Effects / Insert Slots
    // Insert slots provide up to MAX_INSERT_SLOTS (8) effect processors in the signal chain
    // Signal path: Audio Input -> Insert Slots (in order) -> Output
    EffectChain& getEffectChain() { return effectChain; }
    const EffectChain& getEffectChain() const { return effectChain; }

    // Insert slot management
    /** Add effect to the next available insert slot. Returns slot index, or -1 if all slots full. */
    int addInsert(std::unique_ptr<Effect> effect);

    /** Add effect at specific slot index. Returns true if successful. */
    bool addInsertAtSlot(int slotIndex, std::unique_ptr<Effect> effect);

    /** Remove effect from specified slot. */
    void removeInsert(int slotIndex);

    /** Move effect from one slot to another. */
    void moveInsert(int fromSlot, int toSlot);

    /** Get effect at slot (nullptr if empty). */
    Effect* getInsert(int slotIndex);
    const Effect* getInsert(int slotIndex) const;

    /** Get number of active inserts (non-empty slots). */
    int getNumActiveInserts() const { return effectChain.getNumEffects(); }

    /** Check if an insert slot is empty. */
    bool isInsertSlotEmpty(int slotIndex) const;

    /** Check if insert slots are full. */
    bool areInsertSlotsFull() const { return effectChain.getNumEffects() >= MAX_INSERT_SLOTS; }

    /** Clear all insert slots. */
    void clearInserts();

    /** Get total latency from effect chain (for PDC). */
    int getPluginLatencySamples() const { return effectChain.getTotalLatencySamples(); }

    /** Apply effect parameter automation for the current playback position. */
    void applyEffectAutomation(int64_t position);

    /** Bypass/unbypass insert at slot. */
    void setInsertBypassed(int slotIndex, bool bypassed);
    bool isInsertBypassed(int slotIndex) const;

private:
    std::vector<std::unique_ptr<AudioRegion>> regions;

    // Insert FX chain - processes up to MAX_INSERT_SLOTS effects in series
    // Effects are applied after reading from regions, before output to mixer
    EffectChain effectChain;

    // Time stretch and pitch shift processors for real-time processing
    TimeStretch timeStretchProcessor;
    PitchShift pitchShiftProcessor;

    // Processing state tracking
    int64_t lastProcessedRegionId = -1;     // Track which region was last processed
    int64_t lastProcessedPosition = -1;     // Track last playhead position for discontinuity detection

    // Temporary buffer for time stretch/pitch shift processing
    juce::AudioBuffer<float> processingBuffer;
    juce::AudioBuffer<float> stretchInputBuffer;
    juce::AudioBuffer<float> stretchOutputBuffer;

    // Input routing configuration
    InputChannelConfig inputConfig;

    // Input metering (thread-safe for real-time access)
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> inputLevelLeft { 0.0f };
    std::atomic<float> inputLevelRight { 0.0f };
    static constexpr float levelDecayRate = 0.9995f; // Smooth decay for meters

    // Input monitoring state
    std::atomic<bool> inputMonitoringEnabled { false };
    mutable juce::AudioBuffer<float> monitoringBuffer;
    mutable std::atomic<bool> monitoringBufferValid { false };
    mutable int monitoringBufferSamples { 0 };

    // Latency compensation (in samples)
    int latencyCompensationSamples { 0 };

    // Recording state
    bool recording = false;
    int64_t recordingStartPosition = 0;
    std::unique_ptr<AudioRegion> recordingRegion;
    static constexpr int MAX_RECORDING_SAMPLES = 44100 * 60 * 10; // 10 minutes at 44.1kHz
    int recordingWritePosition = 0;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioTrack)
};
