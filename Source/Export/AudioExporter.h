#pragma once

#include "../Audio/AudioEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <functional>

/**
 * AudioExporter renders the project to an audio file (WAV).
 * Does offline rendering for accurate mixdown.
 */
class AudioExporter
{
public:
    AudioExporter();
    ~AudioExporter() = default;

    /**
     * Export timeline to WAV file.
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

private:
    juce::AudioFormatManager formatManager;
    double sampleRate = 44100.0;
    int bitDepth = 16;
    int blockSize = 1024;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioExporter)
};
