#pragma once

#include "AudioEngine.h"
#include "../Timeline/Track.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <functional>

/**
 * TrackFreezer handles offline rendering of a single track to a WAV file.
 * Used for Freeze (reversible) and Bounce (permanent) operations.
 */
class TrackFreezer
{
public:
    TrackFreezer() = default;
    ~TrackFreezer() = default;

    /**
     * Freeze a track by rendering it to a WAV file.
     * @param track The track to freeze
     * @param timeline The timeline (for BPM, sample rate, etc.)
     * @param outputFile Where to write the frozen audio
     * @param lengthSamples Length to render
     * @param sampleRate Sample rate for rendering
     * @param progressCallback Optional progress callback
     * @return true if successful
     */
    bool freezeTrack(Track& track,
                     Timeline& timeline,
                     const juce::File& outputFile,
                     int64_t lengthSamples,
                     double sampleRate = 44100.0,
                     std::function<void(float)> progressCallback = nullptr);

    /**
     * Get the default freeze folder for storing frozen audio files.
     */
    static juce::File getFreezeFolder();

    /**
     * Generate a unique filename for a frozen track.
     */
    static juce::File getFreezeFile(const juce::String& trackName);

private:
    static constexpr int blockSize = 1024;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackFreezer)
};
