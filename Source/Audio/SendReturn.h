#pragma once

#include "../Timeline/Track.h"
#include "../Effects/EffectChain.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>

/**
 * Send configuration for routing audio to an aux track.
 *
 * Sends allow a copy of a track's signal to be routed to an aux (return) track
 * for parallel processing. Common use cases include:
 *   - Shared reverb bus (multiple tracks sending to one reverb)
 *   - Parallel compression
 *   - Delay throws
 *
 * Send Level: 0.0 (silent) to 1.0 (unity gain)
 * Pre/Post Fader: Pre = send level independent of track fader
 *                 Post = send level affected by track fader (most common)
 *
 * Thread Safety: All parameters use std::atomic for lock-free access.
 */
struct Send
{
    /** Index of the destination aux track (-1 = no destination) */
    int destAuxIndex { -1 };

    /** Send level: 0.0 (silent) to 1.0 (unity gain) */
    std::atomic<float> sendLevel { 0.0f };

    /** If true, send is taken before fader (pre-fader send) */
    std::atomic<bool> preFader { false };

    /** Enable/disable this send */
    std::atomic<bool> enabled { true };

    Send() = default;

    Send(int destIndex, float level = 0.5f, bool pre = false)
        : destAuxIndex(destIndex)
    {
        sendLevel.store(level);
        preFader.store(pre);
        enabled.store(true);
    }

    // Copy constructor for atomic members
    Send(const Send& other)
        : destAuxIndex(other.destAuxIndex)
    {
        sendLevel.store(other.sendLevel.load());
        preFader.store(other.preFader.load());
        enabled.store(other.enabled.load());
    }

    Send& operator=(const Send& other)
    {
        if (this != &other)
        {
            destAuxIndex = other.destAuxIndex;
            sendLevel.store(other.sendLevel.load());
            preFader.store(other.preFader.load());
            enabled.store(other.enabled.load());
        }
        return *this;
    }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("SEND");
        xml->setAttribute("destAuxIndex", destAuxIndex);
        xml->setAttribute("sendLevel", static_cast<double>(sendLevel.load()));
        xml->setAttribute("preFader", preFader.load());
        xml->setAttribute("enabled", enabled.load());
        return xml;
    }

    void loadFromXml(const juce::XmlElement& xml)
    {
        destAuxIndex = xml.getIntAttribute("destAuxIndex", -1);
        sendLevel.store(static_cast<float>(xml.getDoubleAttribute("sendLevel", 0.0)));
        preFader.store(xml.getBoolAttribute("preFader", false));
        enabled.store(xml.getBoolAttribute("enabled", true));
    }
};

/**
 * AuxTrack (Return Track) for parallel effect processing.
 *
 * An aux track receives audio from sends on other tracks, processes it
 * through its effect chain, and outputs to the master bus.
 *
 * Signal Flow:
 *   Accumulated Sends -> Insert FX Chain -> Fader -> Pan -> Master Output
 *
 * Usage:
 *   1. Create AuxTrack and add effects (e.g., reverb)
 *   2. Configure sends on audio/MIDI tracks to route to this aux
 *   3. Mixer accumulates sends during track processing
 *   4. Mixer processes aux tracks and mixes to master
 *
 * Thread Safety:
 *   - Uses std::atomic for parameters accessed from audio thread
 *   - No memory allocation in processBlock
 *   - Input buffer is pre-allocated in prepareToPlay
 */
class AuxTrack : public Track
{
public:
    /** Maximum number of insert effect slots per aux track */
    static constexpr int MAX_INSERT_SLOTS = 8;

    AuxTrack(const juce::String& name = "Aux");
    ~AuxTrack() override = default;

    // Track interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;

    /**
     * Clear the input accumulation buffer.
     * Call at the start of each audio block before accumulating sends.
     */
    void clearInputBuffer();

    /**
     * Add audio to the input accumulation buffer.
     * Call from mixer when processing sends from source tracks.
     *
     * @param sourceBuffer The source track's audio
     * @param gain The send level to apply
     * @param numSamples Number of samples to add
     */
    void addToInputBuffer(const juce::AudioBuffer<float>& sourceBuffer, float gain, int numSamples);

    /**
     * Process the accumulated input through the effect chain.
     * Call after all sends have been accumulated.
     * The result is written to the output buffer.
     *
     * @param outputBuffer Buffer to write processed audio to
     * @param numSamples Number of samples to process
     */
    void processAccumulatedInput(juce::AudioBuffer<float>& outputBuffer, int numSamples);

    // Effect chain access
    EffectChain& getEffectChain() { return effectChain; }
    const EffectChain& getEffectChain() const { return effectChain; }

    // Insert slot management (mirrors AudioTrack API)
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

    // Input accumulation buffer - receives audio from sends
    juce::AudioBuffer<float> inputBuffer;

    // Peak metering
    std::atomic<float> peakLevelLeft { 0.0f };
    std::atomic<float> peakLevelRight { 0.0f };

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuxTrack)
};

/**
 * SendManager handles send routing for a single track.
 *
 * Each track can have multiple sends routed to different aux tracks.
 * The manager provides a convenient interface for configuring sends.
 *
 * Thread Safety:
 *   - Send configuration uses atomic values
 *   - Vector modifications should be done from UI thread only
 */
class SendManager
{
public:
    /** Maximum number of sends per track */
    static constexpr int MAX_SENDS = 8;

    SendManager();
    ~SendManager() = default;

    /**
     * Add a new send to an aux track.
     * @param destAuxIndex Index of the destination aux track
     * @param level Initial send level (0.0 to 1.0)
     * @param preFader True for pre-fader send
     * @return Index of the new send, or -1 if max sends reached
     */
    int addSend(int destAuxIndex, float level = 0.5f, bool preFader = false);

    /**
     * Remove a send by index.
     */
    void removeSend(int sendIndex);

    /**
     * Get send by index (nullptr if invalid index).
     */
    Send* getSend(int index);
    const Send* getSend(int index) const;

    /**
     * Get number of active sends.
     */
    int getNumSends() const { return static_cast<int>(sends.size()); }

    /**
     * Set send level for a specific send.
     */
    void setSendLevel(int sendIndex, float level);
    float getSendLevel(int sendIndex) const;

    /**
     * Enable/disable a send.
     */
    void setSendEnabled(int sendIndex, bool enabled);
    bool isSendEnabled(int sendIndex) const;

    /**
     * Set pre/post fader mode.
     */
    void setSendPreFader(int sendIndex, bool preFader);
    bool isSendPreFader(int sendIndex) const;

    /**
     * Clear all sends.
     */
    void clearSends();

    /**
     * Find a send by destination aux index.
     * @return Send index, or -1 if not found
     */
    int findSendToAux(int auxIndex) const;

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const;
    void loadFromXml(const juce::XmlElement& xml);

private:
    std::vector<Send> sends;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SendManager)
};
