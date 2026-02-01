#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cstdint>

/**
 * Metronome generates click sounds synchronized with the project tempo.
 * Thread-safe for access from audio thread.
 *
 * Features:
 * - Click on every beat at the project BPM
 * - Accent on first beat of each measure (louder)
 * - Adjustable volume
 * - Pre-allocated click buffers (no allocation in audio thread)
 */
class Metronome
{
public:
    Metronome();
    ~Metronome() = default;

    // Initialization
    void prepareToPlay(double sampleRate, int samplesPerBlock);

    // Audio processing (call from audio thread)
    void processBlock(juce::AudioBuffer<float>& outputBuffer,
                     int64_t playheadPosition,
                     double bpm,
                     bool isPlaying);

    // Control
    void setEnabled(bool shouldBeEnabled) { enabled.store(shouldBeEnabled); }
    bool isEnabled() const { return enabled.load(); }

    void setVolume(double newVolume) { volume.store(newVolume); }
    double getVolume() const { return volume.load(); }

    // Export mode - disables metronome output during audio export
    void setExportMode(bool isExporting) { exportMode.store(isExporting); }
    bool isInExportMode() const { return exportMode.load(); }

    // Count-in control
    void setCountInBars(int bars); // 0 = off, 1 or 2 bars
    int getCountInBars() const { return countInBars.load(); }
    void startCountIn(int64_t startPosition);
    bool isInCountIn() const { return isCountingIn.load(); }
    bool isCountInComplete(int64_t currentPosition, double bpm) const;

    // Time signature (currently fixed to 4/4)
    int getBeatsPerMeasure() const { return beatsPerMeasure; }

private:
    // Generate click sound buffers
    void generateClickSounds();

    // Calculate if we should trigger a click at current position
    bool shouldTriggerClick(int64_t playheadPosition, double bpm, int& beatNumber);

    // Thread-safe state
    std::atomic<bool> enabled { false };
    std::atomic<double> volume { 0.7 };
    std::atomic<bool> exportMode { false }; // When true, metronome is muted

    // Count-in state
    std::atomic<int> countInBars { 1 }; // 0 = off, 1 or 2 bars
    std::atomic<bool> isCountingIn { false };
    std::atomic<int64_t> countInStartPosition { -1 };

    // Audio parameters
    double currentSampleRate = 44100.0;
    int maxSamplesPerBlock = 512;

    // Time signature
    const int beatsPerMeasure = 4; // 4/4 time

    // Pre-allocated click sound buffers
    juce::AudioBuffer<float> normalClickBuffer;
    juce::AudioBuffer<float> accentClickBuffer;

    // Click playback state
    int clickPlaybackPosition = -1; // -1 when not playing a click
    bool isAccentClick = false;

    // Last beat tracking (to avoid retriggering same beat)
    int64_t lastBeatSample = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Metronome)
};
