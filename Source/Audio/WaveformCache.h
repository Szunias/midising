#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

/**
 * WaveformCache handles generation and caching of audio thumbnails for waveform display.
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

    // Get thumbnail for drawing
    juce::AudioThumbnail& getThumbnail(int64_t hash);

    // Call this if cache needs to be refreshed (e.g. buffer changed)
    void refreshBuffer(int64_t hash, const juce::AudioBuffer<float>& buffer);

private:
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 50 }; // Cache last 50 thumbnails
    
    // Since we are working with in-memory buffers mostly, we need a way to wrap them
    // juce::AudioThumbnail works best with files or InputSource.
    // For in-memory buffers, we might need to fake an InputSource or simply write to a temp file?
    // Or we can use `reset` on AudioThumbnail to manually feed input source.
    
    // Map of hash -> AudioThumbnail
    std::map<int64_t, std::unique_ptr<juce::AudioThumbnail>> thumbnails;
    
    double currentSampleRate = 44100.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformCache)
};
