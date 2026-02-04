#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

/**
 * Stores min/max peak data at a single resolution level.
 * Each entry represents the peak values over a fixed number of samples.
 */
struct WaveformMipmapLevel
{
    int samplesPerPoint = 1;                     // Number of source samples per data point
    int numChannels = 0;
    int numPoints = 0;
    std::vector<float> minValues;                // Interleaved: [ch0_p0, ch1_p0, ch0_p1, ch1_p1, ...]
    std::vector<float> maxValues;                // Same layout as minValues
    std::vector<float> rmsValues;                // RMS values for each point (optional, for RMS display mode)

    void allocate(int channels, int points)
    {
        numChannels = channels;
        numPoints = points;
        size_t size = static_cast<size_t>(channels * points);
        minValues.resize(size, 0.0f);
        maxValues.resize(size, 0.0f);
        rmsValues.resize(size, 0.0f);
    }

    void clear()
    {
        std::fill(minValues.begin(), minValues.end(), 0.0f);
        std::fill(maxValues.begin(), maxValues.end(), 0.0f);
        std::fill(rmsValues.begin(), rmsValues.end(), 0.0f);
    }

    // Get peak values for a specific point and channel
    void getPeakAt(int pointIndex, int channel, float& outMin, float& outMax) const
    {
        if (pointIndex < 0 || pointIndex >= numPoints || channel < 0 || channel >= numChannels)
        {
            outMin = outMax = 0.0f;
            return;
        }
        int idx = pointIndex * numChannels + channel;
        outMin = minValues[static_cast<size_t>(idx)];
        outMax = maxValues[static_cast<size_t>(idx)];
    }

    float getRmsAt(int pointIndex, int channel) const
    {
        if (pointIndex < 0 || pointIndex >= numPoints || channel < 0 || channel >= numChannels)
            return 0.0f;
        return rmsValues[static_cast<size_t>(pointIndex * numChannels + channel)];
    }

    void setPeakAt(int pointIndex, int channel, float minVal, float maxVal, float rms = 0.0f)
    {
        if (pointIndex < 0 || pointIndex >= numPoints || channel < 0 || channel >= numChannels)
            return;
        int idx = pointIndex * numChannels + channel;
        minValues[static_cast<size_t>(idx)] = minVal;
        maxValues[static_cast<size_t>(idx)] = maxVal;
        rmsValues[static_cast<size_t>(idx)] = rms;
    }
};

/**
 * Holds all mipmap levels for a single audio buffer.
 * Provides efficient peak data retrieval at any zoom level.
 */
class WaveformMipmapData
{
public:
    // Standard mipmap resolutions: 1:1, 1:4, 1:16, 1:64, 1:256, 1:1024
    static constexpr int kNumLevels = 6;
    static constexpr int kMipmapFactors[kNumLevels] = { 1, 4, 16, 64, 256, 1024 };

    WaveformMipmapData() = default;
    ~WaveformMipmapData() = default;

    /**
     * Generate all mipmap levels from source audio buffer.
     */
    void generateFromBuffer(const juce::AudioBuffer<float>& buffer)
    {
        numChannels = buffer.getNumChannels();
        totalSamples = buffer.getNumSamples();

        if (numChannels == 0 || totalSamples == 0)
            return;

        // Generate level 0 (1:1) - full resolution peaks
        generateLevel0(buffer);

        // Generate subsequent levels by downsampling from previous level
        for (int level = 1; level < kNumLevels; ++level)
        {
            generateLevelFromPrevious(level);
        }
    }

    /**
     * Get the optimal mipmap level index for the given samples-per-pixel ratio.
     * Returns the highest resolution level that has samplesPerPoint <= samplesPerPixel.
     */
    int getOptimalLevelIndex(double samplesPerPixel) const
    {
        // Find the highest resolution level where samplesPerPoint <= samplesPerPixel
        // This ensures we always have enough data points to draw smoothly
        for (int i = kNumLevels - 1; i >= 0; --i)
        {
            if (levels[i].samplesPerPoint <= samplesPerPixel)
                return i;
        }
        return 0;  // Return full resolution if very zoomed in
    }

    /**
     * Get the mipmap level at the specified index.
     */
    const WaveformMipmapLevel& getLevel(int levelIndex) const
    {
        if (levelIndex < 0 || levelIndex >= kNumLevels)
            return levels[0];
        return levels[levelIndex];
    }

    /**
     * Get aggregated min/max peaks for a sample range, using the optimal mipmap level.
     * Returns the peak values across all samples in [startSample, endSample).
     */
    void getPeaksForRange(int64_t startSample, int64_t endSample, int channel,
                          float& outMin, float& outMax, float& outRms) const
    {
        outMin = 0.0f;
        outMax = 0.0f;
        outRms = 0.0f;

        if (startSample >= endSample || channel < 0 || channel >= numChannels)
            return;

        // Clamp to valid range
        startSample = juce::jmax((int64_t)0, startSample);
        endSample = juce::jmin((int64_t)totalSamples, endSample);

        if (startSample >= endSample)
            return;

        // Choose optimal level based on range size
        double rangeSize = static_cast<double>(endSample - startSample);
        int levelIndex = 0;
        for (int i = kNumLevels - 1; i >= 0; --i)
        {
            // Use a level if it gives us at least 1 point for this range
            if (kMipmapFactors[i] <= rangeSize)
            {
                levelIndex = i;
                break;
            }
        }

        const auto& level = levels[levelIndex];
        int factor = level.samplesPerPoint;

        // Calculate point range
        int startPoint = static_cast<int>(startSample / factor);
        int endPoint = static_cast<int>((endSample + factor - 1) / factor);  // Round up

        startPoint = juce::jmax(0, startPoint);
        endPoint = juce::jmin(level.numPoints, endPoint);

        if (startPoint >= endPoint)
            return;

        // Aggregate peaks
        outMin = std::numeric_limits<float>::max();
        outMax = std::numeric_limits<float>::lowest();
        float rmsSum = 0.0f;
        int count = 0;

        for (int p = startPoint; p < endPoint; ++p)
        {
            float pMin, pMax;
            level.getPeakAt(p, channel, pMin, pMax);
            outMin = juce::jmin(outMin, pMin);
            outMax = juce::jmax(outMax, pMax);

            float rms = level.getRmsAt(p, channel);
            rmsSum += rms * rms;  // Sum of squared RMS
            ++count;
        }

        if (count > 0)
            outRms = std::sqrt(rmsSum / count);  // RMS of RMS values
        else
            outMin = outMax = 0.0f;
    }

    int getNumChannels() const { return numChannels; }
    int64_t getTotalSamples() const { return totalSamples; }

private:
    void generateLevel0(const juce::AudioBuffer<float>& buffer)
    {
        // Level 0 stores every sample as its own peak (1:1)
        // But for efficiency, we actually use 1 sample = 1 point
        auto& level = levels[0];
        level.samplesPerPoint = kMipmapFactors[0];
        level.allocate(numChannels, totalSamples);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int s = 0; s < totalSamples; ++s)
            {
                float sample = data[s];
                level.setPeakAt(s, ch, sample, sample, std::abs(sample));
            }
        }
    }

    void generateLevelFromPrevious(int levelIndex)
    {
        if (levelIndex <= 0 || levelIndex >= kNumLevels)
            return;

        const auto& prevLevel = levels[levelIndex - 1];
        auto& level = levels[levelIndex];

        int factor = kMipmapFactors[levelIndex];
        int prevFactor = kMipmapFactors[levelIndex - 1];
        int downsampleRatio = factor / prevFactor;  // Always 4 in our case

        int numPoints = (prevLevel.numPoints + downsampleRatio - 1) / downsampleRatio;

        level.samplesPerPoint = factor;
        level.allocate(numChannels, numPoints);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int p = 0; p < numPoints; ++p)
            {
                int startPoint = p * downsampleRatio;
                int endPoint = juce::jmin(startPoint + downsampleRatio, prevLevel.numPoints);

                float minVal = std::numeric_limits<float>::max();
                float maxVal = std::numeric_limits<float>::lowest();
                float rmsSum = 0.0f;
                int count = 0;

                for (int pp = startPoint; pp < endPoint; ++pp)
                {
                    float pMin, pMax;
                    prevLevel.getPeakAt(pp, ch, pMin, pMax);
                    minVal = juce::jmin(minVal, pMin);
                    maxVal = juce::jmax(maxVal, pMax);

                    float rms = prevLevel.getRmsAt(pp, ch);
                    rmsSum += rms * rms;
                    ++count;
                }

                if (count > 0)
                {
                    level.setPeakAt(p, ch, minVal, maxVal, std::sqrt(rmsSum / count));
                }
            }
        }
    }

    WaveformMipmapLevel levels[kNumLevels];
    int numChannels = 0;
    int64_t totalSamples = 0;
};

/**
 * WaveformCache handles generation and caching of audio thumbnails for waveform display.
 * Now with multi-resolution mipmap caching for efficient display at any zoom level.
 */
class WaveformCache
{
public:
    WaveformCache();
    ~WaveformCache();

    void setSampleRate(double sampleRate);

    // Add a new source to the cache (from AudioBuffer)
    // Returns a unique identifier hash for this buffer
    int64_t addAudioBuffer(const juce::AudioBuffer<float>& buffer);

    // Get thumbnail for drawing (legacy API, still functional)
    juce::AudioThumbnail& getThumbnail(int64_t hash);

    // Call this if cache needs to be refreshed (e.g. buffer changed)
    void refreshBuffer(int64_t hash, const juce::AudioBuffer<float>& buffer);

    // ========== NEW MIPMAP API ==========

    /**
     * Check if mipmap data exists for the given buffer ID.
     */
    bool hasMipmap(int64_t id) const;

    /**
     * Get mipmap data for a buffer. Returns nullptr if not found.
     */
    const WaveformMipmapData* getMipmap(int64_t id) const;

    /**
     * Get peaks for a sample range, using optimal mipmap level.
     * This is the preferred API for waveform drawing.
     *
     * @param id           Buffer identifier
     * @param startSample  Start of sample range
     * @param endSample    End of sample range (exclusive)
     * @param channel      Audio channel
     * @param outMin       Output: minimum peak value
     * @param outMax       Output: maximum peak value
     * @param outRms       Output: RMS value
     * @return             true if peaks were found, false otherwise
     */
    bool getPeaksForRange(int64_t id, int64_t startSample, int64_t endSample, int channel,
                          float& outMin, float& outMax, float& outRms) const;

    /**
     * Get the optimal mipmap level index for a given samples-per-pixel ratio.
     */
    int getOptimalMipmapLevel(int64_t id, double samplesPerPixel) const;

    /**
     * Remove cached data for a buffer.
     */
    void removeBuffer(int64_t id);

    /**
     * Clear all cached data.
     */
    void clearAll();

    /**
     * Get memory usage statistics.
     */
    size_t getMemoryUsageBytes() const;

private:
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 50 }; // Cache last 50 thumbnails

    // Legacy thumbnail storage
    std::map<int64_t, std::unique_ptr<juce::AudioThumbnail>> thumbnails;

    // New mipmap storage
    std::map<int64_t, std::unique_ptr<WaveformMipmapData>> mipmaps;

    double currentSampleRate = 44100.0;

    // Thread safety for mipmap access
    mutable juce::CriticalSection mipmapLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformCache)
};
