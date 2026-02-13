#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

/**
 * Circular buffer delay line for one stereo channel pair.
 * Used to compensate for plugin/effect processing latency.
 */
class CompensationDelay
{
public:
    CompensationDelay() = default;

    void prepare(int maxSamples, int numChannels = 2)
    {
        buffer.setSize(numChannels, maxSamples + 1);
        buffer.clear();
        bufferSize = maxSamples + 1;
        writePos = 0;
    }

    void setDelaySamples(int samples)
    {
        delaySamples = juce::jmax(0, samples);
    }

    int getDelaySamples() const { return delaySamples; }

    void processSamples(juce::AudioBuffer<float>& audioBuffer)
    {
        if (delaySamples == 0 || bufferSize <= 1)
            return;

        int numSamples = audioBuffer.getNumSamples();
        int numChannels = juce::jmin(audioBuffer.getNumChannels(), buffer.getNumChannels());

        for (int i = 0; i < numSamples; ++i)
        {
            int readPos = writePos - delaySamples;
            if (readPos < 0)
                readPos += bufferSize;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float inputSample = audioBuffer.getSample(ch, i);
                float outputSample = buffer.getSample(ch, readPos);

                buffer.setSample(ch, writePos, inputSample);
                audioBuffer.setSample(ch, i, outputSample);
            }

            writePos = (writePos + 1) % bufferSize;
        }
    }

    void reset()
    {
        buffer.clear();
        writePos = 0;
    }

private:
    juce::AudioBuffer<float> buffer;
    int bufferSize = 0;
    int writePos = 0;
    int delaySamples = 0;
};

/**
 * Manages delay compensation across all tracks.
 * Finds the maximum latency and compensates each track so they are time-aligned.
 */
class DelayCompensationManager
{
public:
    DelayCompensationManager() = default;

    void prepare(int maxLatencySamples, int numTracks, int numChannels = 2)
    {
        delays.resize(static_cast<size_t>(numTracks));
        trackLatencies.resize(static_cast<size_t>(numTracks), 0);
        for (auto& delay : delays)
            delay.prepare(maxLatencySamples, numChannels);
        totalCompensation = 0;
    }

    /**
     * Set the latency for a specific track.
     * Call recalculate() after updating all latencies.
     */
    void setTrackLatency(int trackIndex, int latencySamples)
    {
        if (trackIndex >= 0 && trackIndex < static_cast<int>(trackLatencies.size()))
            trackLatencies[static_cast<size_t>(trackIndex)] = latencySamples;
    }

    /**
     * Recalculate compensation delays based on current track latencies.
     * Each track's compensation = maxLatency - trackLatency.
     */
    void recalculate()
    {
        int maxLatency = 0;
        for (int lat : trackLatencies)
            maxLatency = juce::jmax(maxLatency, lat);

        totalCompensation = maxLatency;

        for (size_t i = 0; i < delays.size() && i < trackLatencies.size(); ++i)
        {
            int compensation = maxLatency - trackLatencies[i];
            delays[i].setDelaySamples(compensation);
        }
    }

    /**
     * Apply compensation delay to a track's audio buffer.
     */
    void processTrack(int trackIndex, juce::AudioBuffer<float>& buffer)
    {
        if (trackIndex >= 0 && trackIndex < static_cast<int>(delays.size()))
        {
            if (delays[static_cast<size_t>(trackIndex)].getDelaySamples() > 0)
                delays[static_cast<size_t>(trackIndex)].processSamples(buffer);
        }
    }

    /**
     * Get the total compensation latency (maximum across all tracks).
     */
    int getTotalCompensation() const { return totalCompensation; }

    /**
     * Get the compensation delay for a specific track.
     */
    int getTrackCompensation(int trackIndex) const
    {
        if (trackIndex >= 0 && trackIndex < static_cast<int>(delays.size()))
            return delays[static_cast<size_t>(trackIndex)].getDelaySamples();
        return 0;
    }

    void reset()
    {
        for (auto& delay : delays)
            delay.reset();
    }

private:
    std::vector<CompensationDelay> delays;
    std::vector<int> trackLatencies;
    int totalCompensation = 0;
};
