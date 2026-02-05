#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>

/**
 * LoudnessMeter implements LUFS loudness metering per ITU-R BS.1770.
 * It provides thread-safe loudness measurements for the master bus.
 *
 * Measurements provided:
 *   - Momentary loudness: 400ms sliding window
 *   - Short-term loudness: 3s sliding window
 *   - Integrated loudness: Entire program with gating
 *   - Loudness range: Dynamic range of the audio
 *
 * The K-weighting filter is a two-stage filter:
 *   Stage 1: High-shelf filter (+4dB at high frequencies)
 *   Stage 2: High-pass filter (RLB weighting, ~40Hz cutoff)
 *
 * Thread Safety:
 *   - All loudness values use std::atomic for lock-free access
 *   - processSamples() called from audio thread
 *   - get*() methods called from UI thread
 */
class LoudnessMeter
{
public:
    LoudnessMeter()
    {
        reset();
    }

    ~LoudnessMeter() = default;

    /**
     * Prepare the meter for a given sample rate.
     * Must be called before processing.
     */
    void prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate;

        // Configure K-weighting filters for each channel (stereo)
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
        spec.numChannels = 1;

        // Stage 1: High-shelf filter (+4dB boost at high frequencies)
        // ITU-R BS.1770 specifies: f0=1681.974Hz, Q=0.7071068, gain=+4dB
        for (int ch = 0; ch < 2; ++ch)
        {
            highShelfFilters[ch].reset();
            highShelfFilters[ch].prepare(spec);
            highShelfFilters[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                static_cast<float>(sampleRate), 1681.974f, 0.7071068f, juce::Decibels::decibelsToGain(4.0f));
        }

        // Stage 2: High-pass filter (RLB weighting)
        // ITU-R BS.1770 specifies: f0=38.13547Hz, Q=0.5003270
        for (int ch = 0; ch < 2; ++ch)
        {
            highPassFilters[ch].reset();
            highPassFilters[ch].prepare(spec);
            highPassFilters[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(
                static_cast<float>(sampleRate), 38.13547f, 0.5003270f);
        }

        // Calculate block sizes for momentary (400ms) and short-term (3s) windows
        momentaryBlockSize = static_cast<int>(sampleRate * 0.4);    // 400ms
        shortTermBlockSize = static_cast<int>(sampleRate * 3.0);    // 3 seconds

        // Pre-allocate ring buffers for sliding windows
        momentaryBuffer.resize(momentaryBlockSize * 2, 0.0f);  // Stereo interleaved
        shortTermBuffer.resize(shortTermBlockSize * 2, 0.0f);  // Stereo interleaved

        // Pre-allocate filtered sample buffer
        filteredSamples.resize(static_cast<size_t>(samplesPerBlock) * 2, 0.0f);

        reset();
    }

    /**
     * Process stereo samples through K-weighting and accumulate for loudness calculation.
     * Called from the audio thread.
     *
     * @param buffer Stereo audio buffer to analyze
     */
    void processSamples(const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
            return;

        int numSamples = buffer.getNumSamples();

        // Ensure filtered buffer is large enough
        if (filteredSamples.size() < static_cast<size_t>(numSamples) * 2)
            filteredSamples.resize(static_cast<size_t>(numSamples) * 2, 0.0f);

        // Apply K-weighting filter to each channel
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* input = buffer.getReadPointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                float sample = input[i];

                // Stage 1: High-shelf filter
                sample = highShelfFilters[ch].processSample(sample);

                // Stage 2: High-pass filter
                sample = highPassFilters[ch].processSample(sample);

                filteredSamples[static_cast<size_t>(i * 2 + ch)] = sample;
            }
        }

        // Accumulate filtered samples into ring buffers and calculate loudness
        for (int i = 0; i < numSamples; ++i)
        {
            float leftSample = filteredSamples[static_cast<size_t>(i * 2)];
            float rightSample = filteredSamples[static_cast<size_t>(i * 2 + 1)];

            // Calculate weighted sum of squares for this sample
            // Weighting: Left=1.0, Right=1.0 (for stereo), others have different weights
            float power = leftSample * leftSample + rightSample * rightSample;

            // Update momentary buffer (circular)
            size_t momentaryIdx = static_cast<size_t>(momentaryWriteIndex * 2);
            momentarySumOfSquares -= momentaryBuffer[momentaryIdx] * momentaryBuffer[momentaryIdx]
                                   + momentaryBuffer[momentaryIdx + 1] * momentaryBuffer[momentaryIdx + 1];
            momentaryBuffer[momentaryIdx] = leftSample;
            momentaryBuffer[momentaryIdx + 1] = rightSample;
            momentarySumOfSquares += power;
            momentaryWriteIndex = (momentaryWriteIndex + 1) % momentaryBlockSize;
            momentarySamplesAccumulated = juce::jmin(momentarySamplesAccumulated + 1, momentaryBlockSize);

            // Update short-term buffer (circular)
            size_t shortTermIdx = static_cast<size_t>(shortTermWriteIndex * 2);
            shortTermSumOfSquares -= shortTermBuffer[shortTermIdx] * shortTermBuffer[shortTermIdx]
                                   + shortTermBuffer[shortTermIdx + 1] * shortTermBuffer[shortTermIdx + 1];
            shortTermBuffer[shortTermIdx] = leftSample;
            shortTermBuffer[shortTermIdx + 1] = rightSample;
            shortTermSumOfSquares += power;
            shortTermWriteIndex = (shortTermWriteIndex + 1) % shortTermBlockSize;
            shortTermSamplesAccumulated = juce::jmin(shortTermSamplesAccumulated + 1, shortTermBlockSize);

            // Update integrated loudness (gated)
            integratedSumOfSquares += power;
            integratedSampleCount++;
        }

        // Calculate and store momentary loudness
        if (momentarySamplesAccumulated > 0)
        {
            float meanSquare = momentarySumOfSquares / static_cast<float>(momentarySamplesAccumulated);
            float lufs = calculateLUFS(meanSquare);
            momentaryLoudness.store(lufs);
        }

        // Calculate and store short-term loudness
        if (shortTermSamplesAccumulated > 0)
        {
            float meanSquare = shortTermSumOfSquares / static_cast<float>(shortTermSamplesAccumulated);
            float lufs = calculateLUFS(meanSquare);
            shortTermLoudness.store(lufs);
        }

        // Update integrated loudness (simple ungated for now, full gating would need more complexity)
        if (integratedSampleCount > 0)
        {
            float meanSquare = static_cast<float>(integratedSumOfSquares / static_cast<double>(integratedSampleCount));
            float lufs = calculateLUFS(meanSquare);
            integratedLoudness.store(lufs);

            // Track loudness range (max - min of short-term values above gate)
            float currentShortTerm = shortTermLoudness.load();
            if (currentShortTerm > gateThreshold)
            {
                float currentMax = maxShortTermLoudness.load();
                float currentMin = minShortTermLoudness.load();

                if (currentShortTerm > currentMax)
                    maxShortTermLoudness.store(currentShortTerm);
                if (currentShortTerm < currentMin)
                    minShortTermLoudness.store(currentShortTerm);
            }
        }
    }

    /**
     * Reset all measurements to initial state.
     * Call when starting a new measurement period.
     */
    void reset()
    {
        momentaryWriteIndex = 0;
        momentarySamplesAccumulated = 0;
        momentarySumOfSquares = 0.0f;
        std::fill(momentaryBuffer.begin(), momentaryBuffer.end(), 0.0f);

        shortTermWriteIndex = 0;
        shortTermSamplesAccumulated = 0;
        shortTermSumOfSquares = 0.0f;
        std::fill(shortTermBuffer.begin(), shortTermBuffer.end(), 0.0f);

        integratedSumOfSquares = 0.0;
        integratedSampleCount = 0;

        momentaryLoudness.store(minLUFS);
        shortTermLoudness.store(minLUFS);
        integratedLoudness.store(minLUFS);
        maxShortTermLoudness.store(minLUFS);
        minShortTermLoudness.store(0.0f);  // Start high, will be reduced

        // Reset filters
        for (int ch = 0; ch < 2; ++ch)
        {
            highShelfFilters[ch].reset();
            highPassFilters[ch].reset();
        }
    }

    // === Getters (thread-safe, called from UI thread) ===

    /**
     * Get momentary loudness (400ms window).
     * @return Loudness in LUFS
     */
    float getMomentaryLoudness() const { return momentaryLoudness.load(); }

    /**
     * Get short-term loudness (3s window).
     * @return Loudness in LUFS
     */
    float getShortTermLoudness() const { return shortTermLoudness.load(); }

    /**
     * Get integrated loudness (entire program).
     * @return Loudness in LUFS
     */
    float getIntegratedLoudness() const { return integratedLoudness.load(); }

    /**
     * Get loudness range (LRA).
     * @return Loudness range in LU (LUFS difference)
     */
    float getLoudnessRange() const
    {
        float maxVal = maxShortTermLoudness.load();
        float minVal = minShortTermLoudness.load();

        // Only return valid range if we have measurements above gate
        if (maxVal > minLUFS && minVal < 0.0f)
            return maxVal - minVal;

        return 0.0f;
    }

    // Minimum representable LUFS value (effectively -infinity)
    static constexpr float minLUFS = -70.0f;

private:
    /**
     * Convert mean square value to LUFS.
     * LUFS = -0.691 + 10 * log10(meanSquare)
     */
    float calculateLUFS(float meanSquare) const
    {
        if (meanSquare < 1e-10f)
            return minLUFS;

        // ITU-R BS.1770 formula: LUFS = -0.691 + 10 * log10(z)
        // where z is the mean square of the K-weighted signal
        float lufs = -0.691f + 10.0f * std::log10(meanSquare);
        return juce::jmax(minLUFS, lufs);
    }

    double currentSampleRate = 44100.0;

    // K-weighting filters (per channel)
    juce::dsp::IIR::Filter<float> highShelfFilters[2];
    juce::dsp::IIR::Filter<float> highPassFilters[2];

    // Momentary loudness (400ms window)
    int momentaryBlockSize = 17640;  // 400ms at 44100Hz
    int momentaryWriteIndex = 0;
    int momentarySamplesAccumulated = 0;
    float momentarySumOfSquares = 0.0f;
    std::vector<float> momentaryBuffer;

    // Short-term loudness (3s window)
    int shortTermBlockSize = 132300;  // 3s at 44100Hz
    int shortTermWriteIndex = 0;
    int shortTermSamplesAccumulated = 0;
    float shortTermSumOfSquares = 0.0f;
    std::vector<float> shortTermBuffer;

    // Integrated loudness (entire program)
    double integratedSumOfSquares = 0.0;
    int64_t integratedSampleCount = 0;

    // Loudness range tracking
    static constexpr float gateThreshold = -70.0f;  // Gate threshold for LRA

    // Filtered samples buffer (pre-allocated)
    std::vector<float> filteredSamples;

    // Thread-safe loudness values
    std::atomic<float> momentaryLoudness { minLUFS };
    std::atomic<float> shortTermLoudness { minLUFS };
    std::atomic<float> integratedLoudness { minLUFS };
    std::atomic<float> maxShortTermLoudness { minLUFS };
    std::atomic<float> minShortTermLoudness { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoudnessMeter)
};
