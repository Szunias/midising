#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <memory>

/**
 * AudioImporter loads audio files (WAV, MP3, AIFF, FLAC) into AudioBuffers.
 */
class AudioImporter
{
public:
    AudioImporter();
    ~AudioImporter() = default;

    // Load audio file into buffer. Returns nullptr on failure.
    std::unique_ptr<juce::AudioBuffer<float>> loadFile(const juce::File& file);

    // Load audio file and resample to target sample rate
    std::unique_ptr<juce::AudioBuffer<float>> loadFile(const juce::File& file, double targetSampleRate);

    // Get info about a file without loading it
    struct AudioFileInfo
    {
        double sampleRate = 0.0;
        int64_t lengthInSamples = 0;
        int numChannels = 0;
        double lengthInSeconds = 0.0;
        juce::String format;
    };
    
    AudioFileInfo getFileInfo(const juce::File& file);

    // Supported file extensions
    juce::StringArray getSupportedExtensions() const;

    // Check if a file is a supported audio file
    bool isSupported(const juce::File& file) const;

private:
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioImporter)
};
