#pragma once

#include <cstdint>

/**
 * Utility functions for time conversion between beats, samples, and seconds.
 * Used throughout the DAW for timeline positioning and synchronization.
 */
namespace TimeConversion
{
    /**
     * Convert beats to samples based on BPM and sample rate.
     * @param beats Number of beats (can be fractional)
     * @param bpm Beats per minute
     * @param sampleRate Audio sample rate in Hz
     * @return Number of samples
     */
    inline int64_t beatsToSamples(double beats, double bpm, double sampleRate)
    {
        double beatsPerSecond = bpm / 60.0;
        double seconds = beats / beatsPerSecond;
        return static_cast<int64_t>(seconds * sampleRate);
    }

    /**
     * Convert samples to beats based on BPM and sample rate.
     * @param samples Number of samples
     * @param bpm Beats per minute
     * @param sampleRate Audio sample rate in Hz
     * @return Number of beats (can be fractional)
     */
    inline double samplesToBeats(int64_t samples, double bpm, double sampleRate)
    {
        double seconds = static_cast<double>(samples) / sampleRate;
        double beatsPerSecond = bpm / 60.0;
        return seconds * beatsPerSecond;
    }

    /**
     * Convert beats to seconds based on BPM.
     * @param beats Number of beats (can be fractional)
     * @param bpm Beats per minute
     * @return Time in seconds
     */
    inline double beatsToSeconds(double beats, double bpm)
    {
        double beatsPerSecond = bpm / 60.0;
        return beats / beatsPerSecond;
    }

    /**
     * Convert seconds to beats based on BPM.
     * @param seconds Time in seconds
     * @param bpm Beats per minute
     * @return Number of beats (can be fractional)
     */
    inline double secondsToBeats(double seconds, double bpm)
    {
        double beatsPerSecond = bpm / 60.0;
        return seconds * beatsPerSecond;
    }

    /**
     * Convert samples to seconds based on sample rate.
     * @param samples Number of samples
     * @param sampleRate Audio sample rate in Hz
     * @return Time in seconds
     */
    inline double samplesToSeconds(int64_t samples, double sampleRate)
    {
        return static_cast<double>(samples) / sampleRate;
    }

    /**
     * Convert seconds to samples based on sample rate.
     * @param seconds Time in seconds
     * @param sampleRate Audio sample rate in Hz
     * @return Number of samples
     */
    inline int64_t secondsToSamples(double seconds, double sampleRate)
    {
        return static_cast<int64_t>(seconds * sampleRate);
    }

    /**
     * Convert beat position to bar and beat within bar.
     * @param beats Total beat position
     * @param beatsPerBar Number of beats per bar (e.g., 4 for 4/4 time)
     * @param outBar Output: bar number (1-based)
     * @param outBeat Output: beat within bar (1-based)
     */
    inline void beatsToBarBeat(double beats, int beatsPerBar, int& outBar, int& outBeat)
    {
        int totalBeats = static_cast<int>(beats);
        outBar = (totalBeats / beatsPerBar) + 1;
        outBeat = (totalBeats % beatsPerBar) + 1;
    }
}
