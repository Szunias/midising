#pragma once

#include "../Timeline/Track.h"
#include "../Effects/EffectChain.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

/**
 * GroupBus for submixing multiple tracks together.
 *
 * A group bus (also called a bus or subgroup) allows multiple tracks to be
 * routed together and processed as a single unit. This is useful for:
 *   - Applying effects to a group of tracks (e.g., drum compression)
 *   - Collective volume control (raise/lower all drums at once)
 *   - Organizing mix structure
 *
 * Signal Flow:
 *   Accumulated Track Outputs -> Insert FX Chain -> Fader -> Pan -> Master Output
 *
 * Usage:
 *   1. Create GroupBus and optionally add effects
 *   2. Set track output routing to this group (via track's groupBusIndex)
 *   3. Mixer routes track outputs to group instead of master
 *   4. Mixer processes group busses and mixes to master
 *
 * Thread Safety:
 *   - Uses std::atomic for parameters accessed from audio thread
 *   - No memory allocation in processBlock
 *   - Input buffer is pre-allocated in prepareToPlay
 */
class GroupBus : public Track
{
public:
    /** Maximum number of insert effect slots per group bus */
    static constexpr int MAX_INSERT_SLOTS = 8;

    /** Special value indicating output goes to master bus (no group routing) */
    static constexpr int OUTPUT_TO_MASTER = -1;

    GroupBus(const juce::String& name = "Group");
    ~GroupBus() override = default;

    // Track interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;

    /**
     * Clear the input accumulation buffer.
     * Call at the start of each audio block before accumulating track outputs.
     */
    void clearInputBuffer();

    /**
     * Add audio to the input accumulation buffer.
     * Call from mixer when routing track outputs to this group.
     *
     * @param sourceBuffer The track's audio after fader/pan
     * @param leftGain Left channel gain (includes volume and pan)
     * @param rightGain Right channel gain (includes volume and pan)
     * @param numSamples Number of samples to add
     */
    void addToInputBuffer(const juce::AudioBuffer<float>& sourceBuffer,
                          float leftGain, float rightGain, int numSamples);

    /**
     * Add audio to the input accumulation buffer with gain ramping.
     * Provides smooth automation playback by interpolating between start and end gains.
     *
     * @param sourceBuffer The track's audio after fader/pan
     * @param leftGainStart Left channel gain at start of buffer
     * @param rightGainStart Right channel gain at start of buffer
     * @param leftGainEnd Left channel gain at end of buffer
     * @param rightGainEnd Right channel gain at end of buffer
     * @param numSamples Number of samples to add
     */
    void addToInputBufferWithRamp(const juce::AudioBuffer<float>& sourceBuffer,
                                   float leftGainStart, float rightGainStart,
                                   float leftGainEnd, float rightGainEnd,
                                   int numSamples);

    /**
     * Process the accumulated input through the effect chain.
     * Call after all track outputs have been accumulated.
     * The result is written to the output buffer.
     *
     * @param outputBuffer Buffer to write processed audio to
     * @param numSamples Number of samples to process
     */
    void processAccumulatedInput(juce::AudioBuffer<float>& outputBuffer, int numSamples);

    // Effect chain access
    EffectChain& getEffectChain() { return effectChain; }
    const EffectChain& getEffectChain() const { return effectChain; }

    // Insert slot management (mirrors AudioTrack/AuxTrack API)
    int addInsert(std::unique_ptr<Effect> effect);
    bool addInsertAtSlot(int slotIndex, std::unique_ptr<Effect> effect);
    void removeInsert(int slotIndex);
    void moveInsert(int fromSlot, int toSlot);
    Effect* getInsert(int slotIndex);
    const Effect* getInsert(int slotIndex) const;
    int getNumActiveInserts() const { return effectChain.getNumEffects(); }
    bool isInsertSlotEmpty(int slotIndex) const;
    bool areInsertSlotsFull() const { return effectChain.getNumEffects() >= MAX_INSERT_SLOTS; }
    void clearInserts();
    void setInsertBypassed(int slotIndex, bool bypassed);
    bool isInsertBypassed(int slotIndex) const;

    // Peak level for metering
    float getPeakLevel(int channel) const
    {
        return channel == 0 ? peakLevelLeft.load() : peakLevelRight.load();
    }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const;
    void loadFromXml(const juce::XmlElement& xml);

private:
    // Effect chain for processing
    EffectChain effectChain;

    // Input accumulation buffer - receives audio from routed tracks
    juce::AudioBuffer<float> inputBuffer;

    // Peak metering
    std::atomic<float> peakLevelLeft { 0.0f };
    std::atomic<float> peakLevelRight { 0.0f };

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GroupBus)
};
