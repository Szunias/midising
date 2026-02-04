#pragma once

#include "Transport.h"
#include "Mixer.h"
#include "AudioRecorder.h"
#include "SpectrumAnalyzer.h"
#include "Metronome.h"
#include "../Timeline/Timeline.h"
#include "../MIDI/MidiEngine.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <atomic>
#include <chrono>

/**
 * Performance metrics for audio engine monitoring.
 * Thread-safe for access from UI thread while audio thread updates.
 */
struct AudioPerformanceMetrics
{
    double cpuLoad = 0.0;              // Current estimated CPU load (0.0 - 1.0+)
    double averageProcessingTimeMs = 0.0; // Average block processing time
    double maxProcessingTimeMs = 0.0;  // Maximum observed processing time
    double bufferTimeMs = 0.0;         // Available time per buffer
    uint64_t dropoutCount = 0;         // Total number of dropouts detected
    uint64_t totalBlocksProcessed = 0; // Total audio blocks processed
};

/**
 * AudioEngine is the central audio processing hub.
 * Owns Timeline, Transport, Mixer, MidiEngine, and AudioRecorder.
 * Implements getNextAudioBlock for real-time audio processing.
 * Listens for audio device changes to synchronize sample rate.
 */
class AudioEngine : public juce::AudioSource,
                    public juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    // AudioSource interface
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // ChangeListener interface - for device change notifications
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Device management
    void setAudioDeviceManager(juce::AudioDeviceManager* manager);
    void removeFromDeviceManager();

    // Access to components
    Timeline& getTimeline() { return timeline; }
    const Timeline& getTimeline() const { return timeline; }
    
    Transport& getTransport() { return transport; }
    const Transport& getTransport() const { return transport; }
    
    Mixer& getMixer() { return mixer; }
    const Mixer& getMixer() const { return mixer; }

    MidiEngine& getMidiEngine() { return midiEngine; }
    const MidiEngine& getMidiEngine() const { return midiEngine; }

    AudioRecorder& getRecorder() { return recorder; }
    const AudioRecorder& getRecorder() const { return recorder; }

    SpectrumAnalyzer& getSpectrumAnalyzer() { return spectrumAnalyzer; }

    Metronome& getMetronome() { return metronome; }
    const Metronome& getMetronome() const { return metronome; }

    // Recording management
    bool startRecording(Track* targetTrack = nullptr);
    void stopRecording();
    bool isRecording() const { return recorder.isRecording(); }

    // Capture input buffer for recording (called before processing)
    void recordInputBlock(const juce::AudioBuffer<float>& inputBuffer);

    // Get recorded file after recording stops
    juce::File getLastRecordedFile() const { return lastRecordedFile; }

    // Input routing - get available input channels from audio device
    juce::StringArray getAvailableInputChannelNames() const;
    int getNumAvailableInputChannels() const;

    // Convenience methods
    double getSampleRate() const { return currentSampleRate; }
    int getBlockSize() const { return currentBlockSize; }

    // Performance monitoring
    AudioPerformanceMetrics getPerformanceMetrics() const;
    void resetPerformanceMetrics();

    // Dropout detection
    uint64_t getDropoutCount() const { return dropoutCount.load(); }
    bool hadRecentDropout() const { return recentDropout.load(); }
    void clearRecentDropoutFlag() { recentDropout.store(false); }

    // CPU load estimation (0.0 to 1.0+, >1.0 indicates overload)
    double getCpuLoad() const { return cpuLoad.load(); }

private:
    void handleRecordingStart();
    void handleRecordingStop();
    void routeInputToArmedTracks(const juce::AudioBuffer<float>& inputBuffer);
    juce::File generateRecordingFilePath();
    void updatePerformanceMetrics(double processingTimeMs);

    Timeline timeline;
    Transport transport;
    Mixer mixer;
    MidiEngine midiEngine;
    AudioRecorder recorder;
    SpectrumAnalyzer spectrumAnalyzer;
    Metronome metronome;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    // Device manager for change notifications
    juce::AudioDeviceManager* deviceManager = nullptr;

    // Recording state
    Track* recordingTargetTrack = nullptr;
    int64_t recordingStartPosition = 0;
    juce::File lastRecordedFile;
    bool wasRecordingLastBlock = false;

    // Performance monitoring (thread-safe for audio thread access)
    std::atomic<double> cpuLoad { 0.0 };
    std::atomic<double> averageProcessingTimeMs { 0.0 };
    std::atomic<double> maxProcessingTimeMs { 0.0 };
    std::atomic<uint64_t> dropoutCount { 0 };
    std::atomic<uint64_t> totalBlocksProcessed { 0 };
    std::atomic<bool> recentDropout { false };

    // Timing measurement
    using Clock = std::chrono::high_resolution_clock;
    double bufferDurationMs = 0.0;  // Time available for processing each buffer

    // Exponential moving average coefficient for CPU load smoothing
    static constexpr double kCpuLoadSmoothing = 0.95;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
