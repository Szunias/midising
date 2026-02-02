#pragma once

#include "../Timeline/Track.h"
#include "../Timeline/Timeline.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * Mixer processes and sums all tracks with gain, pan, mute, solo.
 * Designed for real-time audio processing (no allocations in process).
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
    }

    /**
     * Process and sum all tracks into the output buffer.
     * Respects mute, solo, gain, and pan settings.
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
            workBuffer.clear();
            track->processBlock(workBuffer, static_cast<int>(startSample), numSamples);

            // Apply gain and pan, then mix into output
            float volume = track->getVolume();
            float pan = track->getPan();

            // Calculate stereo pan gains (constant power panning)
            float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            float leftGain = volume * std::cos(angle);
            float rightGain = volume * std::sin(angle);

            // Mix into output
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

        // Apply master volume
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

    void updatePeakLevels(const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() >= 1)
            peakLevelLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
        if (buffer.getNumChannels() >= 2)
            peakLevelRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));
    }

    void releaseResources()
    {
        workBuffer.setSize(0, 0);
    }

private:
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    juce::AudioBuffer<float> workBuffer;

    std::atomic<float> masterVolume { 1.0f };
    std::atomic<float> peakLevelLeft { 0.0f };
    std::atomic<float> peakLevelRight { 0.0f };
};
