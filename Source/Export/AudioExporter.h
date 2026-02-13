#pragma once

#include "../Audio/AudioEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <functional>

/**
 * Supported export audio formats.
 */
enum class ExportFormat
{
    WAV,
    FLAC,
    OGG
};

/**
 * AudioExporter renders the project to an audio file.
 * Supports WAV, FLAC, and OGG Vorbis formats.
 * Does offline rendering for accurate mixdown.
 */
class AudioExporter
{
public:
    AudioExporter();
    ~AudioExporter() = default;

    /**
     * Export timeline to audio file.
     * @param audioEngine The audio engine to render from
     * @param file Output file path
     * @param startSample Start position in samples
     * @param lengthSamples Length to export in samples
     * @param progressCallback Optional callback (0.0 to 1.0)
     */
    bool exportToFile(AudioEngine& audioEngine,
                      const juce::File& file,
                      int64_t startSample,
                      int64_t lengthSamples,
                      std::function<void(float)> progressCallback = nullptr);

    // Export settings
    void setSampleRate(double rate) { sampleRate = rate; }
    double getSampleRate() const { return sampleRate; }
    void setBitDepth(int bits) { bitDepth = bits; }
    int getBitDepth() const { return bitDepth; }
    void setFormat(ExportFormat fmt) { format = fmt; }
    ExportFormat getFormat() const { return format; }
    void setQuality(float q) { quality = juce::jlimit(0.0f, 1.0f, q); }
    float getQuality() const { return quality; }

    // Cancellation support
    void cancel() { shouldCancel.store(true); }
    bool isCancelled() const { return shouldCancel.load(); }
    void resetCancel() { shouldCancel.store(false); }

    // Format helpers
    static juce::String getFormatName(ExportFormat fmt);
    static juce::String getFileExtension(ExportFormat fmt);

private:
    juce::AudioFormatManager formatManager;
    double sampleRate = 44100.0;
    int bitDepth = 16;
    int blockSize = 1024;
    ExportFormat format = ExportFormat::WAV;
    float quality = 0.5f;  // OGG quality (0.0 to 1.0)
    std::atomic<bool> shouldCancel { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioExporter)
};
