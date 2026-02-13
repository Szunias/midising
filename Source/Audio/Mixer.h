#pragma once

#include "../Timeline/Track.h"
#include "../Timeline/Timeline.h"
#include "../Timeline/AutomationLane.h"
#include "AudioTrack.h"
#include "SendReturn.h"
#include "GroupBus.h"
#include "LoudnessMeter.h"
#include "DelayCompensation.h"
#include "SidechainRouter.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
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
 *
 * SIMD Optimization:
 *   - Uses juce::FloatVectorOperations for SIMD-accelerated DSP operations
 *   - JUCE auto-selects optimal SIMD implementation (SSE/AVX on x86, NEON on ARM)
 *   - Block processing with unrolled loops for compiler auto-vectorization
 *   - Single-pass algorithms where possible to maximize cache efficiency
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

        // Prepare loudness meter for LUFS metering
        loudnessMeter.prepareToPlay(sampleRate, samplesPerBlock);
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

        // Clear sidechain buffers for this block
        sidechainRouter.clearBuffers();

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

            // Wire sidechain buffers for this track's effects before processing
            if (track->getType() == TrackType::Audio)
            {
                auto* audioTrack = static_cast<AudioTrack*>(track);
                for (int fxIdx = 0; fxIdx < audioTrack->getEffectChain().getNumEffects(); ++fxIdx)
                {
                    Effect* fx = audioTrack->getEffectChain().getEffect(fxIdx);
                    if (fx != nullptr && fx->supportsSidechain())
                    {
                        int sourceIdx = sidechainRouter.getSidechainSource(i, fxIdx);
                        if (sourceIdx >= 0)
                        {
                            const auto* scBuf = sidechainRouter.getSidechainBuffer(sourceIdx);
                            if (scBuf != nullptr)
                                fx->setSidechainBuffer(scBuf);
                        }
                    }
                }
            }

            track->processBlock(workBuffer, static_cast<int>(startSample), numSamples);

            // Store this track's audio as a potential sidechain source
            if (sidechainRouter.isTrackUsedAsSidechainSource(i))
                sidechainRouter.storeSidechainBuffer(i, workBuffer, numSamples);

            // Apply PDC compensation delay for this track
            pdcManager.processTrack(i, workBuffer);

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

        // Process loudness meter (LUFS metering on final output)
        loudnessMeter.processSamples(outputBuffer);
    }

    // Master volume control
    float getMasterVolume() const { return masterVolume.load(); }
    void setMasterVolume(float volume) { masterVolume.store(juce::jlimit(0.0f, 2.0f, volume)); }

    /**
     * Recalculate Plugin Delay Compensation for all tracks.
     * Call when effects are added/removed/changed on any track.
     */
    void recalculatePDC(Timeline& timeline)
    {
        int numTracks = timeline.getNumTracks();
        pdcManager.prepare(8192, numTracks);

        for (int i = 0; i < numTracks; ++i)
        {
            Track* track = timeline.getTrack(i);
            if (track != nullptr && track->getType() == TrackType::Audio)
            {
                auto* audioTrack = static_cast<AudioTrack*>(track);
                pdcManager.setTrackLatency(i, audioTrack->getPluginLatencySamples());
            }
            else
            {
                pdcManager.setTrackLatency(i, 0);
            }
        }
        pdcManager.recalculate();
    }

    /** Get total PDC latency in samples. */
    int getPDCLatencySamples() const { return pdcManager.getTotalCompensation(); }

    /** Access the sidechain router. */
    SidechainRouter& getSidechainRouter() { return sidechainRouter; }
    const SidechainRouter& getSidechainRouter() const { return sidechainRouter; }

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

    /**
     * Update peak levels for metering.
     * Uses SIMD-optimized peak detection via findPeakSIMD helper.
     */
    void updatePeakLevels(const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();

        if (buffer.getNumChannels() >= 1)
        {
            // SIMD-optimized peak detection
            float peak = findPeakSIMD(buffer.getReadPointer(0), numSamples);
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
            // SIMD-optimized peak detection
            float peak = findPeakSIMD(buffer.getReadPointer(1), numSamples);
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

    // =====================================
    // SIMD-Optimized Helper Methods
    // =====================================

    /**
     * SIMD-optimized buffer mixing with gain.
     * Uses juce::FloatVectorOperations for platform-optimized SIMD.
     *
     * @param dest Destination buffer (accumulated into)
     * @param src Source buffer
     * @param gain Gain to apply
     * @param numSamples Number of samples to process
     */
    static void mixWithGainSIMD(float* dest, const float* src, float gain, int numSamples)
    {
        // FloatVectorOperations::addWithMultiply is SIMD-optimized on all platforms
        // dest[i] += src[i] * gain
        juce::FloatVectorOperations::addWithMultiply(dest, src, gain, numSamples);
    }

    /**
     * SIMD-optimized stereo mixing with separate L/R gains (constant power panning).
     * Mixes mono or stereo source to stereo destination with panning.
     *
     * @param destLeft Left channel destination
     * @param destRight Right channel destination
     * @param srcLeft Left channel source
     * @param srcRight Right channel source (can be same as srcLeft for mono)
     * @param leftGain Gain for left channel (from pan law)
     * @param rightGain Gain for right channel (from pan law)
     * @param numSamples Number of samples to process
     */
    static void mixStereoWithPanSIMD(float* destLeft, float* destRight,
                                      const float* srcLeft, const float* srcRight,
                                      float leftGain, float rightGain, int numSamples)
    {
        // Use SIMD-optimized addWithMultiply for both channels
        juce::FloatVectorOperations::addWithMultiply(destLeft, srcLeft, leftGain, numSamples);
        juce::FloatVectorOperations::addWithMultiply(destRight, srcRight, rightGain, numSamples);
    }

    /**
     * SIMD-optimized gain ramping with mixing.
     * Ramps gain from startGain to endGain while mixing.
     *
     * @param dest Destination buffer (accumulated into)
     * @param src Source buffer
     * @param startGain Starting gain value
     * @param endGain Ending gain value
     * @param numSamples Number of samples to process
     */
    static void mixWithGainRampSIMD(float* dest, const float* src,
                                     float startGain, float endGain, int numSamples)
    {
        if (numSamples <= 0) return;

        // If gains are essentially equal, use constant gain for efficiency
        if (std::abs(startGain - endGain) < 1e-6f)
        {
            juce::FloatVectorOperations::addWithMultiply(dest, src, startGain, numSamples);
            return;
        }

        // Process in SIMD-friendly blocks with linear interpolation
        // Block size of 8 matches AVX register width for optimal performance
        constexpr int blockSize = 8;
        const float gainIncrement = (endGain - startGain) / static_cast<float>(numSamples);

        int sample = 0;
        float currentGain = startGain;

        // Main loop with manual unrolling for SIMD auto-vectorization
        const int numBlocks = numSamples / blockSize;
        for (int block = 0; block < numBlocks; ++block)
        {
            // Process 8 samples per block
            for (int i = 0; i < blockSize; ++i)
            {
                dest[sample + i] += src[sample + i] * currentGain;
                currentGain += gainIncrement;
            }
            sample += blockSize;
        }

        // Handle remainder
        for (; sample < numSamples; ++sample)
        {
            dest[sample] += src[sample] * currentGain;
            currentGain += gainIncrement;
        }
    }

    /**
     * SIMD-optimized peak detection.
     * Uses FloatVectorOperations::findMinAndMax for fast peak finding.
     *
     * @param data Audio buffer data
     * @param numSamples Number of samples
     * @return Maximum absolute sample value (peak level)
     */
    static float findPeakSIMD(const float* data, int numSamples)
    {
        if (numSamples <= 0) return 0.0f;

        // JUCE's findMinAndMax is SIMD-optimized
        auto range = juce::FloatVectorOperations::findMinAndMax(data, numSamples);
        return juce::jmax(std::abs(range.getStart()), std::abs(range.getEnd()));
    }

    /**
     * SIMD-optimized buffer clear.
     * Uses FloatVectorOperations::clear for fast zeroing.
     *
     * @param dest Buffer to clear
     * @param numSamples Number of samples to clear
     */
    static void clearBufferSIMD(float* dest, int numSamples)
    {
        juce::FloatVectorOperations::clear(dest, numSamples);
    }

    /**
     * SIMD-optimized buffer copy with gain.
     * Copies source to dest with gain applied.
     *
     * @param dest Destination buffer
     * @param src Source buffer
     * @param gain Gain to apply
     * @param numSamples Number of samples
     */
    static void copyWithGainSIMD(float* dest, const float* src, float gain, int numSamples)
    {
        // Copy and multiply in one pass
        juce::FloatVectorOperations::copyWithMultiply(dest, src, gain, numSamples);
    }

    // =====================================
    // End SIMD-Optimized Helper Methods
    // =====================================

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
    void mixInputMonitoring(Timeline& timeline, juce::AudioBuffer<float>& outputBuffer, [[maybe_unused]] int numSamples)
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
    DelayCompensationManager pdcManager;       // Plugin Delay Compensation
    SidechainRouter sidechainRouter;           // Sidechain audio routing

    std::atomic<float> masterVolume { 1.0f };
    std::atomic<float> peakLevelLeft { 0.0f };
    std::atomic<float> peakLevelRight { 0.0f };

    // True peak metering (for clip detection)
    std::atomic<float> truePeakLeft { 0.0f };
    std::atomic<float> truePeakRight { 0.0f };
    std::atomic<bool> clippingLeft { false };
    std::atomic<bool> clippingRight { false };

    // Stereo correlation metering
    std::atomic<float> stereoCorrelation { 1.0f };  // -1.0 to +1.0

public:
    /**
     * Get the stereo correlation value.
     * @return Correlation coefficient: +1 = mono, 0 = uncorrelated, -1 = out of phase
     */
    float getStereoCorrelation() const { return stereoCorrelation.load(); }

    /**
     * Update stereo correlation from the audio buffer.
     * Called after processing to measure L/R correlation.
     * Uses the Pearson correlation coefficient formula with SIMD optimization.
     *
     * SIMD Optimization Notes:
     * - Uses computational formula: r = (n*Σxy - Σx*Σy) / sqrt((n*Σx² - (Σx)²) * (n*Σy² - (Σy)²))
     * - This avoids mean subtraction, allowing single-pass SIMD accumulation
     * - FloatVectorOperations provides platform-optimized SIMD implementations
     */
    void updateStereoCorrelation(const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        {
            stereoCorrelation.store(1.0f);  // Mono or empty = perfect correlation
            return;
        }

        const float* leftChannel = buffer.getReadPointer(0);
        const float* rightChannel = buffer.getReadPointer(1);
        int numSamples = buffer.getNumSamples();
        float n = static_cast<float>(numSamples);

        // SIMD-optimized computation using the computational formula for Pearson correlation
        // r = (n*Σxy - Σx*Σy) / sqrt((n*Σx² - (Σx)²) * (n*Σy² - (Σy)²))
        //
        // We need: sumX, sumY, sumXY, sumX2, sumY2
        // All can be computed with SIMD-optimized accumulation

        // Calculate sums in a single pass with SIMD where possible
        // Note: juce::FloatVectorOperations provides SIMD implementations on supported platforms
        float sumX = 0.0f;
        float sumY = 0.0f;
        float sumXY = 0.0f;
        float sumX2 = 0.0f;
        float sumY2 = 0.0f;

        // Process in SIMD-friendly blocks (4 floats = 128-bit SSE, 8 floats = 256-bit AVX)
        constexpr int simdBlockSize = 8;
        const int numBlocks = numSamples / simdBlockSize;

        // Block accumulation - allows better auto-vectorization by compilers
        // and reduces loop overhead
        for (int block = 0; block < numBlocks; ++block)
        {
            int offset = block * simdBlockSize;

            // Unrolled loop for SIMD vectorization
            for (int i = 0; i < simdBlockSize; ++i)
            {
                float x = leftChannel[offset + i];
                float y = rightChannel[offset + i];
                sumX += x;
                sumY += y;
                sumXY += x * y;
                sumX2 += x * x;
                sumY2 += y * y;
            }
        }

        // Handle remainder samples
        for (int i = numBlocks * simdBlockSize; i < numSamples; ++i)
        {
            float x = leftChannel[i];
            float y = rightChannel[i];
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
            sumY2 += y * y;
        }

        // Compute correlation using computational formula (avoids mean subtraction)
        // numerator = n * sumXY - sumX * sumY
        // denominator = sqrt((n * sumX2 - sumX^2) * (n * sumY2 - sumY^2))
        float numerator = n * sumXY - sumX * sumY;
        float denomX = n * sumX2 - sumX * sumX;
        float denomY = n * sumY2 - sumY * sumY;

        float correlation;
        if (denomX < 1e-10f || denomY < 1e-10f)
        {
            // Very quiet or silent - assume perfect correlation
            correlation = 1.0f;
        }
        else
        {
            // Use fast approximate square root where available
            float denominator = std::sqrt(denomX * denomY);
            correlation = numerator / denominator;
        }

        // Smooth the correlation value (low-pass filter for visual stability)
        float currentCorr = stereoCorrelation.load();
        constexpr float smoothingFactor = 0.1f;  // Adjust for faster/slower response
        correlation = currentCorr + smoothingFactor * (correlation - currentCorr);

        stereoCorrelation.store(juce::jlimit(-1.0f, 1.0f, correlation));
    }

    // === LUFS Loudness Metering ===

    /**
     * Get momentary loudness (400ms window).
     * @return Loudness in LUFS
     */
    float getMomentaryLoudness() const { return loudnessMeter.getMomentaryLoudness(); }

    /**
     * Get short-term loudness (3s window).
     * @return Loudness in LUFS
     */
    float getShortTermLoudness() const { return loudnessMeter.getShortTermLoudness(); }

    /**
     * Get integrated loudness (entire program).
     * @return Loudness in LUFS
     */
    float getIntegratedLoudness() const { return loudnessMeter.getIntegratedLoudness(); }

    /**
     * Get loudness range (LRA).
     * @return Loudness range in LU
     */
    float getLoudnessRange() const { return loudnessMeter.getLoudnessRange(); }

    /**
     * Reset integrated loudness measurement.
     * Call when starting a new measurement period.
     */
    void resetLoudnessMeter() { loudnessMeter.reset(); }

private:
    // LUFS loudness meter for master bus
    LoudnessMeter loudnessMeter;
};
