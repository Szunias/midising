#pragma once

#include "../Timeline/Track.h"
#include "../Timeline/Timeline.h"
#include "../Timeline/AutomationLane.h"
#include "AudioTrack.h"
#include "SendReturn.h"
#include "GroupBus.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * Mixer processes and sums all tracks with gain, pan, mute, solo.
 * Designed for real-time audio processing (no allocations in process).
 *
 * Channel Strip Signal Flow:
 * ========================
 *
 * For each track:
 *   1. INPUT:     Audio from regions / recording input
 *   2. INSERT FX: Up to 8 effect slots processed in series (in AudioTrack)
 *   3. FADER:     Volume control (0.0 to 2.0, unity = 1.0)
 *   4. PAN:       Stereo position (-1.0 left to +1.0 right)
 *   5. SENDS:     Routes copy to aux tracks (pre or post fader)
 *   6. OUTPUT:    Mixed to master bus or group bus (based on track routing)
 *
 * Aux Tracks (Send/Return):
 *   - Receives audio from track sends
 *   - Processes through insert FX chain (e.g., reverb)
 *   - Output mixed to master bus
 *
 * Group Busses (Submixing):
 *   - Tracks can be routed to group busses instead of master
 *   - Group bus sums all routed track outputs
 *   - Processes through insert FX chain (e.g., bus compression)
 *   - Group fader/pan applied
 *   - Output mixed to master bus
 *
 * Master Bus:
 *   - Direct track outputs summed (tracks not routed to groups)
 *   - All group bus outputs summed
 *   - All aux track outputs summed
 *   - Master volume applied
 *   - Peak metering with clip detection
 *
 * Thread Safety:
 *   - All parameters use std::atomic for lock-free access
 *   - No memory allocation in processBlock
 */
class Mixer
{
public:
    Mixer() = default;
    ~Mixer() = default;

    /**
     * Prepare the mixer for playback.
     * Pre-allocates buffers to avoid allocation during processing.
     */
    void prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = samplesPerBlock;

        // Pre-allocate work buffer (stereo)
        workBuffer.setSize(2, samplesPerBlock);

        // Pre-allocate aux work buffer for send/return processing
        auxWorkBuffer.setSize(2, samplesPerBlock);

        // Pre-allocate group work buffer for group bus processing
        groupWorkBuffer.setSize(2, samplesPerBlock);
    }

    /**
     * Process and sum all tracks into the output buffer.
     * Implements the channel strip signal flow:
     *   Track.processBlock (regions + insert FX) -> Fader -> Pan -> Mix to Output
     *
     * @param timeline The timeline containing tracks
     * @param outputBuffer The output buffer to write to
     * @param startSample The start sample position in the timeline
     * @param numSamples Number of samples to process
     */
    void processBlock(Timeline& timeline, juce::AudioBuffer<float>& outputBuffer,
                      int64_t startSample, int numSamples)
    {
        outputBuffer.clear();

        // Clear all aux track input buffers before processing sends
        clearAuxInputBuffers(timeline);

        // Clear all group bus input buffers before routing tracks
        clearGroupInputBuffers(timeline);

        bool anySoloed = timeline.hasAnySoloedTrack();

        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            Track* track = timeline.getTrack(i);
            if (track == nullptr)
                continue;

            // Handle mute/solo logic
            bool shouldPlay = true;
            if (track->isMuted())
                shouldPlay = false;
            if (anySoloed && !track->isSoloed())
                shouldPlay = false;

            if (!shouldPlay)
                continue;

            // Clear work buffer and let track fill it
            // Track.processBlock handles: regions/input -> insert FX chain
            workBuffer.clear();
            track->processBlock(workBuffer, static_cast<int>(startSample), numSamples);

            // Get volume and pan for fader calculations
            // Check if automation should be read for this track
            float volumeStart, volumeEnd;
            float panStart, panEnd;

            if (track->isAutomationReading())
            {
                // Get automated values at start and end of block for smooth ramping
                volumeStart = track->getAutomatedVolume(startSample);
                volumeEnd = track->getAutomatedVolume(startSample + numSamples);
                panStart = track->getAutomatedPan(startSample);
                panEnd = track->getAutomatedPan(startSample + numSamples);
            }
            else
            {
                // Use static track values (no automation)
                volumeStart = volumeEnd = track->getVolume();
                panStart = panEnd = track->getPan();
            }

            // Route pre-fader sends (before volume is applied)
            // Use average volume for send routing
            float avgVolume = (volumeStart + volumeEnd) * 0.5f;
            routeSends(timeline, *track, workBuffer, numSamples, avgVolume, true);

            // Calculate stereo pan gains (constant power panning) for start of block
            float angleStart = (panStart + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            float leftGainStart = volumeStart * std::cos(angleStart);
            float rightGainStart = volumeStart * std::sin(angleStart);

            // Calculate stereo pan gains for end of block
            float angleEnd = (panEnd + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            float leftGainEnd = volumeEnd * std::cos(angleEnd);
            float rightGainEnd = volumeEnd * std::sin(angleEnd);

            // Route post-fader sends (with volume applied)
            routeSends(timeline, *track, workBuffer, numSamples, avgVolume, false);

            // Route to output: either group bus or master
            // Use ramped gains for smooth automation playback
            int groupBusIndex = track->getOutputGroupBus();
            if (groupBusIndex >= 0)
            {
                // Route to group bus with automation ramping
                GroupBus* groupBus = timeline.getGroupBus(groupBusIndex);
                if (groupBus != nullptr)
                {
                    groupBus->addToInputBufferWithRamp(workBuffer,
                                                        leftGainStart, rightGainStart,
                                                        leftGainEnd, rightGainEnd,
                                                        numSamples);
                }
                else
                {
                    // Invalid group index - fall back to master
                    routeToMasterWithRamp(outputBuffer, numSamples,
                                          leftGainStart, rightGainStart,
                                          leftGainEnd, rightGainEnd);
                }
            }
            else
            {
                // Route direct to master with automation ramping
                routeToMasterWithRamp(outputBuffer, numSamples,
                                      leftGainStart, rightGainStart,
                                      leftGainEnd, rightGainEnd);
            }
        }

        // Process group busses and mix their output to master
        processGroupBusses(timeline, outputBuffer, numSamples);

        // Process aux tracks and mix their output to master
        processAuxTracks(timeline, outputBuffer, numSamples);

        // Mix input monitoring for armed tracks with monitoring enabled
        mixInputMonitoring(timeline, outputBuffer, numSamples);

        // Apply master volume (master bus processing)
        outputBuffer.applyGain(masterVolume.load());
    }

    // Master volume control
    float getMasterVolume() const { return masterVolume.load(); }
    void setMasterVolume(float volume) { masterVolume.store(juce::jlimit(0.0f, 2.0f, volume)); }

    // Peak level for metering (call from audio thread)
    float getPeakLevel(int channel) const
    {
        return channel == 0 ? peakLevelLeft.load() : peakLevelRight.load();
    }

    /**
     * Get true peak level (maximum sample value, can exceed 1.0).
     * Used for clip detection.
     */
    float getTruePeakLevel(int channel) const
    {
        return channel == 0 ? truePeakLeft.load() : truePeakRight.load();
    }

    /**
     * Check if a channel is currently clipping (true peak > 0dB).
     */
    bool isClipping(int channel) const
    {
        return channel == 0 ? clippingLeft.load() : clippingRight.load();
    }

    /**
     * Reset clip indicators (called from UI when user clicks to clear).
     */
    void resetClipIndicators()
    {
        clippingLeft.store(false);
        clippingRight.store(false);
        truePeakLeft.store(0.0f);
        truePeakRight.store(0.0f);
    }

    void updatePeakLevels(const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() >= 1)
        {
            float peak = buffer.getMagnitude(0, 0, buffer.getNumSamples());
            peakLevelLeft.store(peak);

            // Update true peak (holds maximum)
            float currentTruePeak = truePeakLeft.load();
            if (peak > currentTruePeak)
                truePeakLeft.store(peak);

            // Set clipping flag if exceeding 0dB (1.0 linear)
            if (peak >= 1.0f)
                clippingLeft.store(true);
        }
        if (buffer.getNumChannels() >= 2)
        {
            float peak = buffer.getMagnitude(1, 0, buffer.getNumSamples());
            peakLevelRight.store(peak);

            // Update true peak (holds maximum)
            float currentTruePeak = truePeakRight.load();
            if (peak > currentTruePeak)
                truePeakRight.store(peak);

            // Set clipping flag if exceeding 0dB (1.0 linear)
            if (peak >= 1.0f)
                clippingRight.store(true);
        }
    }

    void releaseResources()
    {
        workBuffer.setSize(0, 0);
        auxWorkBuffer.setSize(0, 0);
        groupWorkBuffer.setSize(0, 0);
    }

private:
    /**
     * Clear all aux track input buffers.
     * Called at the start of each processBlock.
     */
    void clearAuxInputBuffers(Timeline& timeline)
    {
        for (int i = 0; i < timeline.getNumAuxTracks(); ++i)
        {
            AuxTrack* auxTrack = timeline.getAuxTrack(i);
            if (auxTrack != nullptr)
            {
                auxTrack->clearInputBuffer();
            }
        }
    }

    /**
     * Clear all group bus input buffers.
     * Called at the start of each processBlock.
     */
    void clearGroupInputBuffers(Timeline& timeline)
    {
        for (int i = 0; i < timeline.getNumGroupBusses(); ++i)
        {
            GroupBus* groupBus = timeline.getGroupBus(i);
            if (groupBus != nullptr)
            {
                groupBus->clearInputBuffer();
            }
        }
    }

    /**
     * Route track output to master bus.
     * Helper method to reduce code duplication.
     */
    void routeToMaster(juce::AudioBuffer<float>& outputBuffer, int numSamples,
                       float leftGain, float rightGain)
    {
        int numChannels = juce::jmin(outputBuffer.getNumChannels(), 2);

        if (numChannels >= 1)
        {
            outputBuffer.addFrom(0, 0, workBuffer, 0, 0, numSamples, leftGain);
        }
        if (numChannels >= 2)
        {
            outputBuffer.addFrom(1, 0, workBuffer,
                                 workBuffer.getNumChannels() > 1 ? 1 : 0,
                                 0, numSamples, rightGain);
        }
    }

    /**
     * Route track output to master bus with gain ramping.
     * Provides smooth automation playback by interpolating between start and end gains.
     */
    void routeToMasterWithRamp(juce::AudioBuffer<float>& outputBuffer, int numSamples,
                               float leftGainStart, float rightGainStart,
                               float leftGainEnd, float rightGainEnd)
    {
        int numChannels = juce::jmin(outputBuffer.getNumChannels(), 2);

        // Check if gains are the same (no need for ramping)
        bool needsRamp = (std::abs(leftGainStart - leftGainEnd) > 0.0001f) ||
                         (std::abs(rightGainStart - rightGainEnd) > 0.0001f);

        if (!needsRamp)
        {
            // Use the simpler constant gain method
            routeToMaster(outputBuffer, numSamples, leftGainStart, rightGainStart);
            return;
        }

        // Apply left channel with ramping
        if (numChannels >= 1)
        {
            outputBuffer.addFromWithRamp(0, 0, workBuffer.getReadPointer(0),
                                         numSamples, leftGainStart, leftGainEnd);
        }

        // Apply right channel with ramping
        if (numChannels >= 2)
        {
            int sourceChannel = workBuffer.getNumChannels() > 1 ? 1 : 0;
            outputBuffer.addFromWithRamp(1, 0, workBuffer.getReadPointer(sourceChannel),
                                         numSamples, rightGainStart, rightGainEnd);
        }
    }

    /**
     * Route sends from a track to aux tracks.
     *
     * @param timeline The timeline containing aux tracks
     * @param track The source track with sends
     * @param sourceBuffer The track's audio buffer
     * @param numSamples Number of samples to process
     * @param trackVolume The track's volume (used for post-fader sends)
     * @param preFaderOnly If true, only process pre-fader sends; if false, only post-fader
     */
    void routeSends(Timeline& timeline, Track& track,
                    const juce::AudioBuffer<float>& sourceBuffer,
                    int numSamples, float trackVolume, bool preFaderOnly)
    {
        const SendManager& sendMgr = track.getSendManager();

        for (int sendIdx = 0; sendIdx < sendMgr.getNumSends(); ++sendIdx)
        {
            const Send* send = sendMgr.getSend(sendIdx);
            if (send == nullptr)
                continue;

            // Skip if not enabled or no destination
            if (!send->enabled.load() || send->destAuxIndex < 0)
                continue;

            // Check pre/post fader mode
            bool isPreFader = send->preFader.load();
            if (isPreFader != preFaderOnly)
                continue;

            // Get destination aux track
            AuxTrack* auxTrack = timeline.getAuxTrack(send->destAuxIndex);
            if (auxTrack == nullptr)
                continue;

            // Calculate send gain
            float sendLevel = send->sendLevel.load();
            float gain = isPreFader ? sendLevel : (sendLevel * trackVolume);

            // Add audio to aux track's input buffer
            auxTrack->addToInputBuffer(sourceBuffer, gain, numSamples);
        }
    }

    /**
     * Process all group busses and mix their output to the master buffer.
     *
     * @param timeline The timeline containing group busses
     * @param outputBuffer The master output buffer
     * @param numSamples Number of samples to process
     */
    void processGroupBusses(Timeline& timeline, juce::AudioBuffer<float>& outputBuffer, int numSamples)
    {
        bool anyGroupSoloed = timeline.hasAnySoloedGroupBus();

        for (int i = 0; i < timeline.getNumGroupBusses(); ++i)
        {
            GroupBus* groupBus = timeline.getGroupBus(i);
            if (groupBus == nullptr)
                continue;

            // Handle mute/solo logic for group busses
            bool shouldPlay = true;
            if (groupBus->isMuted())
                shouldPlay = false;
            if (anyGroupSoloed && !groupBus->isSoloed())
                shouldPlay = false;

            if (!shouldPlay)
                continue;

            // Clear group work buffer and process accumulated input
            groupWorkBuffer.clear();
            groupBus->processAccumulatedInput(groupWorkBuffer, numSamples);

            // Apply group bus volume and pan
            float volume = groupBus->getVolume();
            float pan = groupBus->getPan();

            // Calculate stereo pan gains (constant power panning)
            float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            float leftGain = volume * std::cos(angle);
            float rightGain = volume * std::sin(angle);

            // Mix group output into master output
            int numChannels = juce::jmin(outputBuffer.getNumChannels(), 2);

            if (numChannels >= 1)
            {
                outputBuffer.addFrom(0, 0, groupWorkBuffer, 0, 0, numSamples, leftGain);
            }
            if (numChannels >= 2)
            {
                outputBuffer.addFrom(1, 0, groupWorkBuffer,
                                     groupWorkBuffer.getNumChannels() > 1 ? 1 : 0,
                                     0, numSamples, rightGain);
            }
        }
    }

    /**
     * Process all aux tracks and mix their output to the master buffer.
     *
     * @param timeline The timeline containing aux tracks
     * @param outputBuffer The master output buffer
     * @param numSamples Number of samples to process
     */
    void processAuxTracks(Timeline& timeline, juce::AudioBuffer<float>& outputBuffer, int numSamples)
    {
        bool anyAuxSoloed = timeline.hasAnySoloedAuxTrack();

        for (int i = 0; i < timeline.getNumAuxTracks(); ++i)
        {
            AuxTrack* auxTrack = timeline.getAuxTrack(i);
            if (auxTrack == nullptr)
                continue;

            // Handle mute/solo logic for aux tracks
            bool shouldPlay = true;
            if (auxTrack->isMuted())
                shouldPlay = false;
            if (anyAuxSoloed && !auxTrack->isSoloed())
                shouldPlay = false;

            if (!shouldPlay)
                continue;

            // Clear aux work buffer and process accumulated input
            auxWorkBuffer.clear();
            auxTrack->processAccumulatedInput(auxWorkBuffer, numSamples);

            // Apply aux track volume and pan
            float volume = auxTrack->getVolume();
            float pan = auxTrack->getPan();

            // Calculate stereo pan gains (constant power panning)
            float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            float leftGain = volume * std::cos(angle);
            float rightGain = volume * std::sin(angle);

            // Mix aux output into master output
            int numChannels = juce::jmin(outputBuffer.getNumChannels(), 2);

            if (numChannels >= 1)
            {
                outputBuffer.addFrom(0, 0, auxWorkBuffer, 0, 0, numSamples, leftGain);
            }
            if (numChannels >= 2)
            {
                outputBuffer.addFrom(1, 0, auxWorkBuffer,
                                     auxWorkBuffer.getNumChannels() > 1 ? 1 : 0,
                                     0, numSamples, rightGain);
            }
        }
    }

    /**
     * Mix input monitoring from armed audio tracks into the output buffer.
     * This allows users to hear their input signal when recording.
     *
     * @param timeline The timeline containing tracks
     * @param outputBuffer The master output buffer
     * @param numSamples Number of samples to process
     */
    void mixInputMonitoring(Timeline& timeline, juce::AudioBuffer<float>& outputBuffer, int numSamples)
    {
        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            Track* track = timeline.getTrack(i);
            if (track == nullptr || track->getType() != TrackType::Audio)
                continue;

            // Skip muted tracks
            if (track->isMuted())
                continue;

            auto* audioTrack = static_cast<AudioTrack*>(track);

            // Check if input monitoring is enabled and track is armed
            if (audioTrack->isInputMonitoringEnabled() && audioTrack->isArmed())
            {
                // Get the monitoring buffer and mix it into the output
                audioTrack->getMonitoringBuffer(outputBuffer);
            }
        }
    }

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    juce::AudioBuffer<float> workBuffer;
    juce::AudioBuffer<float> auxWorkBuffer;    // For aux track processing
    juce::AudioBuffer<float> groupWorkBuffer;  // For group bus processing

    std::atomic<float> masterVolume { 1.0f };
    std::atomic<float> peakLevelLeft { 0.0f };
    std::atomic<float> peakLevelRight { 0.0f };

    // True peak metering (for clip detection)
    std::atomic<float> truePeakLeft { 0.0f };
    std::atomic<float> truePeakRight { 0.0f };
    std::atomic<bool> clippingLeft { false };
    std::atomic<bool> clippingRight { false };
};
