#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <map>
#include <memory>
#include <mutex>

/**
 * SpectrogramGenerator generates spectrogram images from audio data.
 * It performs FFT analysis at regular intervals to create a time-frequency
 * representation that can be overlaid on waveform displays.
 */
class SpectrogramGenerator
{
public:
    /**
     * Configuration for spectrogram generation.
     */
    struct Config
    {
        int fftOrder = 10;                      // FFT size = 1 << fftOrder (e.g., 10 = 1024)
        int hopSize = 256;                      // Samples between FFT frames
        float minFrequency = 20.0f;             // Minimum displayed frequency (Hz)
        float maxFrequency = 8000.0f;           // Maximum displayed frequency (Hz)
        float dynamicRangeDb = 80.0f;           // Dynamic range in dB
        float referenceDb = 0.0f;               // Reference level for 0 dB
    };

    SpectrogramGenerator();
    ~SpectrogramGenerator() = default;

    /**
     * Set the sample rate for frequency calculations.
     */
    void setSampleRate(double sampleRate) { currentSampleRate = sampleRate; }

    /**
     * Get current sample rate.
     */
    double getSampleRate() const { return currentSampleRate; }

    /**
     * Set spectrogram configuration.
     */
    void setConfig(const Config& config);

    /**
     * Get current configuration.
     */
    const Config& getConfig() const { return currentConfig; }

    /**
     * Generate spectrogram data for an audio buffer.
     * This is a relatively expensive operation - results should be cached.
     *
     * @param buffer        Source audio buffer
     * @param startSample   Start sample offset within buffer
     * @param numSamples    Number of samples to analyze
     * @return              Unique ID for the generated spectrogram
     */
    int64_t generateSpectrogram(const juce::AudioBuffer<float>& buffer,
                                 int64_t startSample = 0,
                                 int64_t numSamples = -1);

    /**
     * Check if spectrogram data exists for a given ID.
     */
    bool hasSpectrogram(int64_t id) const;

    /**
     * Remove spectrogram data for a given ID.
     */
    void removeSpectrogram(int64_t id);

    /**
     * Clear all cached spectrogram data.
     */
    void clearAll();

    /**
     * Get the spectrogram image for the given ID.
     * The image is rendered as a heat map with frequency on Y-axis and time on X-axis.
     *
     * @param id            Spectrogram ID from generateSpectrogram()
     * @param width         Desired output width in pixels
     * @param height        Desired output height in pixels
     * @return              Spectrogram image (ARGB format)
     */
    juce::Image getSpectrogramImage(int64_t id, int width, int height) const;

    /**
     * Draw spectrogram directly to a graphics context.
     * More efficient than getSpectrogramImage() for real-time drawing.
     *
     * @param g             Graphics context to draw to
     * @param bounds        Rectangle to draw within
     * @param id            Spectrogram ID
     * @param startSample   Start sample offset to display
     * @param numSamples    Number of samples to display
     * @param alpha         Opacity for overlay mode (0.0 - 1.0)
     */
    void drawSpectrogram(juce::Graphics& g, juce::Rectangle<int> bounds,
                          int64_t id, int64_t startSample, int64_t numSamples,
                          float alpha = 0.7f) const;

    /**
     * Get magnitude value at a specific time/frequency location.
     *
     * @param id            Spectrogram ID
     * @param sampleIndex   Sample position in the audio
     * @param frequency     Frequency in Hz
     * @return              Magnitude in dB (normalized to 0..1 range)
     */
    float getMagnitudeAt(int64_t id, int64_t sampleIndex, float frequency) const;

    /**
     * Get the number of FFT frames in a spectrogram.
     */
    int getNumFrames(int64_t id) const;

    /**
     * Get the number of frequency bins per frame.
     */
    int getNumBins() const { return (1 << currentConfig.fftOrder) / 2 + 1; }

    /**
     * Get the frequency for a given bin index.
     */
    float binToFrequency(int bin) const
    {
        int fftSize = 1 << currentConfig.fftOrder;
        return static_cast<float>(bin) * static_cast<float>(currentSampleRate) / static_cast<float>(fftSize);
    }

    /**
     * Get the bin index for a given frequency.
     */
    int frequencyToBin(float frequency) const
    {
        int fftSize = 1 << currentConfig.fftOrder;
        return static_cast<int>(frequency * static_cast<float>(fftSize) / static_cast<float>(currentSampleRate));
    }

    // Color mapping options
    enum class ColorMap
    {
        Heat,       // Black -> Red -> Yellow -> White
        Viridis,    // Purple -> Blue -> Green -> Yellow
        Grayscale,  // Black -> White
        Plasma      // Purple -> Pink -> Orange -> Yellow
    };

    /**
     * Set the color map for spectrogram rendering.
     */
    void setColorMap(ColorMap colorMap) { currentColorMap = colorMap; }

    /**
     * Get the current color map.
     */
    ColorMap getColorMap() const { return currentColorMap; }

private:
    /**
     * Internal spectrogram data storage.
     */
    struct SpectrogramData
    {
        std::vector<std::vector<float>> magnitudes;  // [frame][bin] - normalized 0..1
        int numFrames = 0;
        int numBins = 0;
        int hopSize = 0;
        int64_t totalSamples = 0;
        double sampleRate = 44100.0;
    };

    /**
     * Generate a unique ID for a buffer.
     */
    int64_t generateId(const juce::AudioBuffer<float>& buffer, int64_t startSample, int64_t numSamples) const;

    /**
     * Map a normalized magnitude value (0..1) to a color.
     */
    juce::Colour magnitudeToColour(float magnitude) const;

    /**
     * Apply Hann window to a buffer segment.
     */
    void applyWindow(float* data, int size) const;

    /**
     * Convert linear magnitude to normalized dB scale.
     */
    float linearToNormalizedDb(float linearMag) const;

    // FFT processing
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::vector<float> window;

    // Configuration
    Config currentConfig;
    double currentSampleRate = 44100.0;
    ColorMap currentColorMap = ColorMap::Heat;

    // Cached spectrogram data
    std::map<int64_t, std::unique_ptr<SpectrogramData>> spectrograms;

    // Thread safety
    mutable std::mutex spectrogramMutex;

    // Image cache for efficient repeated draws
    mutable std::map<std::tuple<int64_t, int, int>, juce::Image> imageCache;
    mutable std::mutex imageCacheMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramGenerator)
};

// ============================================================================
// Inline implementation
// ============================================================================

inline SpectrogramGenerator::SpectrogramGenerator()
{
    setConfig(currentConfig);
}

inline void SpectrogramGenerator::setConfig(const Config& config)
{
    currentConfig = config;

    // Recreate FFT processor
    int fftSize = 1 << config.fftOrder;
    forwardFFT = std::make_unique<juce::dsp::FFT>(config.fftOrder);

    // Generate Hann window
    window.resize(static_cast<size_t>(fftSize));
    juce::dsp::WindowingFunction<float>::fillWindowingTables(
        window.data(), static_cast<size_t>(fftSize),
        juce::dsp::WindowingFunction<float>::hann);
}

inline int64_t SpectrogramGenerator::generateId(const juce::AudioBuffer<float>& buffer,
                                                  int64_t startSample, int64_t numSamples) const
{
    // Create a hash from buffer properties and sample range
    int64_t hash = static_cast<int64_t>(buffer.getNumSamples());
    hash ^= (hash << 13) ^ static_cast<int64_t>(buffer.getNumChannels());
    hash ^= (hash << 7) ^ startSample;
    hash ^= (hash << 11) ^ numSamples;

    // Include some sample data in the hash for uniqueness
    if (buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0)
    {
        const float* data = buffer.getReadPointer(0);
        int64_t step = juce::jmax((int64_t)1, numSamples / 16);
        for (int64_t i = startSample; i < startSample + numSamples && i < buffer.getNumSamples(); i += step)
        {
            union { float f; int32_t i; } u;
            u.f = data[i];
            hash ^= (hash << 5) ^ static_cast<int64_t>(u.i);
        }
    }

    return hash;
}

inline int64_t SpectrogramGenerator::generateSpectrogram(const juce::AudioBuffer<float>& buffer,
                                                          int64_t startSample,
                                                          int64_t numSamples)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return 0;

    // Default to full buffer
    if (numSamples < 0)
        numSamples = buffer.getNumSamples() - startSample;

    // Clamp to valid range
    startSample = juce::jmax((int64_t)0, startSample);
    numSamples = juce::jmin(numSamples, (int64_t)buffer.getNumSamples() - startSample);

    if (numSamples <= 0)
        return 0;

    int64_t id = generateId(buffer, startSample, numSamples);

    // Check if already exists
    {
        std::lock_guard<std::mutex> lock(spectrogramMutex);
        if (spectrograms.find(id) != spectrograms.end())
            return id;
    }

    // Generate spectrogram data
    int fftSize = 1 << currentConfig.fftOrder;
    int hopSize = currentConfig.hopSize;
    int numBins = fftSize / 2 + 1;

    int numFrames = static_cast<int>((numSamples - fftSize) / hopSize) + 1;
    if (numFrames <= 0)
        numFrames = 1;

    auto data = std::make_unique<SpectrogramData>();
    data->numFrames = numFrames;
    data->numBins = numBins;
    data->hopSize = hopSize;
    data->totalSamples = numSamples;
    data->sampleRate = currentSampleRate;
    data->magnitudes.resize(static_cast<size_t>(numFrames));

    // FFT working buffer
    std::vector<float> fftBuffer(static_cast<size_t>(fftSize * 2), 0.0f);

    // Mix down to mono if stereo
    const float* sourceData = buffer.getReadPointer(0);
    std::vector<float> monoBuffer;

    if (buffer.getNumChannels() > 1)
    {
        monoBuffer.resize(static_cast<size_t>(numSamples));
        for (int64_t i = 0; i < numSamples; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                int64_t idx = startSample + i;
                if (idx < buffer.getNumSamples())
                    sum += buffer.getReadPointer(ch)[idx];
            }
            monoBuffer[static_cast<size_t>(i)] = sum / buffer.getNumChannels();
        }
        sourceData = monoBuffer.data();
        startSample = 0;  // We're now using the mono buffer
    }

    // Process each frame
    for (int frame = 0; frame < numFrames; ++frame)
    {
        int64_t frameStart = startSample + static_cast<int64_t>(frame * hopSize);

        // Copy and window the data
        std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);

        for (int i = 0; i < fftSize; ++i)
        {
            int64_t sampleIdx = frameStart + i;
            if (sampleIdx >= 0 && sampleIdx < (monoBuffer.empty() ? buffer.getNumSamples() : static_cast<int64_t>(monoBuffer.size())))
            {
                fftBuffer[static_cast<size_t>(i)] = sourceData[sampleIdx] * window[static_cast<size_t>(i)];
            }
        }

        // Perform FFT
        forwardFFT->performFrequencyOnlyForwardTransform(fftBuffer.data());

        // Extract magnitudes and convert to normalized dB
        data->magnitudes[static_cast<size_t>(frame)].resize(static_cast<size_t>(numBins));
        for (int bin = 0; bin < numBins; ++bin)
        {
            float magnitude = fftBuffer[static_cast<size_t>(bin)];
            data->magnitudes[static_cast<size_t>(frame)][static_cast<size_t>(bin)] = linearToNormalizedDb(magnitude);
        }
    }

    // Store the data
    {
        std::lock_guard<std::mutex> lock(spectrogramMutex);
        spectrograms[id] = std::move(data);
    }

    return id;
}

inline bool SpectrogramGenerator::hasSpectrogram(int64_t id) const
{
    std::lock_guard<std::mutex> lock(spectrogramMutex);
    return spectrograms.find(id) != spectrograms.end();
}

inline void SpectrogramGenerator::removeSpectrogram(int64_t id)
{
    std::lock_guard<std::mutex> lock(spectrogramMutex);
    spectrograms.erase(id);

    // Also clear related image cache entries
    std::lock_guard<std::mutex> imgLock(imageCacheMutex);
    for (auto it = imageCache.begin(); it != imageCache.end(); )
    {
        if (std::get<0>(it->first) == id)
            it = imageCache.erase(it);
        else
            ++it;
    }
}

inline void SpectrogramGenerator::clearAll()
{
    std::lock_guard<std::mutex> lock(spectrogramMutex);
    spectrograms.clear();

    std::lock_guard<std::mutex> imgLock(imageCacheMutex);
    imageCache.clear();
}

inline int SpectrogramGenerator::getNumFrames(int64_t id) const
{
    std::lock_guard<std::mutex> lock(spectrogramMutex);
    auto it = spectrograms.find(id);
    if (it != spectrograms.end())
        return it->second->numFrames;
    return 0;
}

inline float SpectrogramGenerator::linearToNormalizedDb(float linearMag) const
{
    // Convert linear magnitude to dB, then normalize to 0..1 range
    float db = 20.0f * std::log10(juce::jmax(1e-10f, linearMag));
    db = juce::jmax(db, currentConfig.referenceDb - currentConfig.dynamicRangeDb);
    float normalized = (db - (currentConfig.referenceDb - currentConfig.dynamicRangeDb)) / currentConfig.dynamicRangeDb;
    return juce::jlimit(0.0f, 1.0f, normalized);
}

inline juce::Colour SpectrogramGenerator::magnitudeToColour(float magnitude) const
{
    magnitude = juce::jlimit(0.0f, 1.0f, magnitude);

    switch (currentColorMap)
    {
    case ColorMap::Heat:
    {
        // Black -> Red -> Yellow -> White
        if (magnitude < 0.33f)
        {
            float t = magnitude / 0.33f;
            return juce::Colour::fromFloatRGBA(t, 0.0f, 0.0f, 1.0f);
        }
        else if (magnitude < 0.66f)
        {
            float t = (magnitude - 0.33f) / 0.33f;
            return juce::Colour::fromFloatRGBA(1.0f, t, 0.0f, 1.0f);
        }
        else
        {
            float t = (magnitude - 0.66f) / 0.34f;
            return juce::Colour::fromFloatRGBA(1.0f, 1.0f, t, 1.0f);
        }
    }

    case ColorMap::Viridis:
    {
        // Purple -> Blue -> Green -> Yellow (approximation)
        if (magnitude < 0.25f)
        {
            float t = magnitude / 0.25f;
            return juce::Colour::fromFloatRGBA(0.267f + t * 0.01f, 0.004f + t * 0.2f, 0.329f + t * 0.17f, 1.0f);
        }
        else if (magnitude < 0.5f)
        {
            float t = (magnitude - 0.25f) / 0.25f;
            return juce::Colour::fromFloatRGBA(0.277f - t * 0.1f, 0.204f + t * 0.25f, 0.499f - t * 0.05f, 1.0f);
        }
        else if (magnitude < 0.75f)
        {
            float t = (magnitude - 0.5f) / 0.25f;
            return juce::Colour::fromFloatRGBA(0.177f + t * 0.3f, 0.454f + t * 0.15f, 0.449f - t * 0.15f, 1.0f);
        }
        else
        {
            float t = (magnitude - 0.75f) / 0.25f;
            return juce::Colour::fromFloatRGBA(0.477f + t * 0.5f, 0.604f + t * 0.35f, 0.299f - t * 0.1f, 1.0f);
        }
    }

    case ColorMap::Grayscale:
        return juce::Colour::fromFloatRGBA(magnitude, magnitude, magnitude, 1.0f);

    case ColorMap::Plasma:
    {
        // Purple -> Pink -> Orange -> Yellow (approximation)
        if (magnitude < 0.33f)
        {
            float t = magnitude / 0.33f;
            return juce::Colour::fromFloatRGBA(0.05f + t * 0.55f, 0.02f + t * 0.1f, 0.53f - t * 0.1f, 1.0f);
        }
        else if (magnitude < 0.66f)
        {
            float t = (magnitude - 0.33f) / 0.33f;
            return juce::Colour::fromFloatRGBA(0.6f + t * 0.3f, 0.12f + t * 0.3f, 0.43f - t * 0.3f, 1.0f);
        }
        else
        {
            float t = (magnitude - 0.66f) / 0.34f;
            return juce::Colour::fromFloatRGBA(0.9f + t * 0.05f, 0.42f + t * 0.55f, 0.13f - t * 0.05f, 1.0f);
        }
    }
    }

    return juce::Colours::black;
}

inline juce::Image SpectrogramGenerator::getSpectrogramImage(int64_t id, int width, int height) const
{
    if (width <= 0 || height <= 0)
        return juce::Image();

    // Check cache
    auto cacheKey = std::make_tuple(id, width, height);
    {
        std::lock_guard<std::mutex> lock(imageCacheMutex);
        auto it = imageCache.find(cacheKey);
        if (it != imageCache.end())
            return it->second;
    }

    // Get spectrogram data
    const SpectrogramData* data = nullptr;
    {
        std::lock_guard<std::mutex> lock(spectrogramMutex);
        auto it = spectrograms.find(id);
        if (it == spectrograms.end())
            return juce::Image();
        data = it->second.get();
    }

    if (data == nullptr || data->numFrames == 0 || data->numBins == 0)
        return juce::Image();

    // Create image
    juce::Image image(juce::Image::ARGB, width, height, true);

    // Calculate bin range for display
    int minBin = frequencyToBin(currentConfig.minFrequency);
    int maxBin = frequencyToBin(currentConfig.maxFrequency);
    minBin = juce::jmax(0, minBin);
    maxBin = juce::jmin(data->numBins - 1, maxBin);

    if (maxBin <= minBin)
        return image;

    int numDisplayBins = maxBin - minBin;

    // Render image
    juce::Image::BitmapData bitmap(image, juce::Image::BitmapData::writeOnly);

    for (int x = 0; x < width; ++x)
    {
        // Map x to frame index
        int frame = static_cast<int>(static_cast<float>(x) / width * data->numFrames);
        frame = juce::jlimit(0, data->numFrames - 1, frame);

        for (int y = 0; y < height; ++y)
        {
            // Map y to bin index (flip so low frequencies at bottom)
            int bin = minBin + static_cast<int>(static_cast<float>(height - 1 - y) / height * numDisplayBins);
            bin = juce::jlimit(minBin, maxBin, bin);

            float magnitude = data->magnitudes[static_cast<size_t>(frame)][static_cast<size_t>(bin)];
            juce::Colour colour = magnitudeToColour(magnitude);

            bitmap.setPixelColour(x, y, colour);
        }
    }

    // Cache the image
    {
        std::lock_guard<std::mutex> lock(imageCacheMutex);
        // Limit cache size
        if (imageCache.size() > 50)
            imageCache.clear();
        imageCache[cacheKey] = image;
    }

    return image;
}

inline void SpectrogramGenerator::drawSpectrogram(juce::Graphics& g, juce::Rectangle<int> bounds,
                                                   int64_t id, int64_t startSample, int64_t numSamples,
                                                   float alpha) const
{
    if (bounds.isEmpty() || numSamples <= 0)
        return;

    // Get spectrogram data
    const SpectrogramData* data = nullptr;
    {
        std::lock_guard<std::mutex> lock(spectrogramMutex);
        auto it = spectrograms.find(id);
        if (it == spectrograms.end())
            return;
        data = it->second.get();
    }

    if (data == nullptr || data->numFrames == 0 || data->numBins == 0)
        return;

    // Calculate frame range to display
    int startFrame = static_cast<int>(static_cast<double>(startSample) / data->hopSize);
    int endFrame = static_cast<int>(static_cast<double>(startSample + numSamples) / data->hopSize);

    startFrame = juce::jmax(0, startFrame);
    endFrame = juce::jmin(data->numFrames, endFrame);

    if (endFrame <= startFrame)
        return;

    int numFramesToDraw = endFrame - startFrame;

    // Calculate bin range for display
    int minBin = frequencyToBin(currentConfig.minFrequency);
    int maxBin = frequencyToBin(currentConfig.maxFrequency);
    minBin = juce::jmax(0, minBin);
    maxBin = juce::jmin(data->numBins - 1, maxBin);

    if (maxBin <= minBin)
        return;

    int numDisplayBins = maxBin - minBin;

    // Calculate pixel dimensions
    float pixelsPerFrame = static_cast<float>(bounds.getWidth()) / numFramesToDraw;
    float pixelsPerBin = static_cast<float>(bounds.getHeight()) / numDisplayBins;

    // Draw spectrogram as rectangles (optimized for sparse rendering at low zoom)
    if (pixelsPerFrame > 2.0f)
    {
        // Draw individual rectangles for each frame/bin
        for (int frame = startFrame; frame < endFrame; ++frame)
        {
            float x = bounds.getX() + (frame - startFrame) * pixelsPerFrame;
            float w = juce::jmax(1.0f, pixelsPerFrame);

            for (int bin = minBin; bin <= maxBin; ++bin)
            {
                float magnitude = data->magnitudes[static_cast<size_t>(frame)][static_cast<size_t>(bin)];

                // Skip very low magnitudes for performance
                if (magnitude < 0.01f)
                    continue;

                // Y is inverted (low frequencies at bottom)
                float y = bounds.getY() + (maxBin - bin) * pixelsPerBin;
                float h = juce::jmax(1.0f, pixelsPerBin);

                juce::Colour colour = magnitudeToColour(magnitude).withAlpha(alpha * magnitude);
                g.setColour(colour);
                g.fillRect(x, y, w, h);
            }
        }
    }
    else
    {
        // Use image-based rendering for dense display
        juce::Image img = getSpectrogramImage(id, bounds.getWidth(), bounds.getHeight());
        if (img.isValid())
        {
            g.setOpacity(alpha);
            g.drawImage(img, bounds.toFloat());
            g.setOpacity(1.0f);
        }
    }
}

inline float SpectrogramGenerator::getMagnitudeAt(int64_t id, int64_t sampleIndex, float frequency) const
{
    std::lock_guard<std::mutex> lock(spectrogramMutex);
    auto it = spectrograms.find(id);
    if (it == spectrograms.end())
        return 0.0f;

    const auto& data = it->second;

    int frame = static_cast<int>(sampleIndex / data->hopSize);
    int bin = frequencyToBin(frequency);

    if (frame < 0 || frame >= data->numFrames ||
        bin < 0 || bin >= data->numBins)
        return 0.0f;

    return data->magnitudes[static_cast<size_t>(frame)][static_cast<size_t>(bin)];
}
