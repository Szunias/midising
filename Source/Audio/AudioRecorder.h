#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <memory>

/**
 * AudioRecorder captures audio input to a WAV file.
 * Simplified version without AudioIODeviceCallback.
 * Supports latency compensation for accurate region positioning.
 */
class AudioRecorder
{
public:
    AudioRecorder();
    ~AudioRecorder();

    // Start recording to a file
    // startPositionSamples: timeline position where recording starts
    bool startRecording(const juce::File& file, double sampleRate, int numChannels,
                        int64_t startPositionSamples = 0);
    void stopRecording();
    bool isRecording() const { return recording.load(); }

    // Write audio samples (call from audio callback)
    void writeAudioBlock(const juce::AudioBuffer<float>& buffer);

    // Get recorded duration
    double getRecordedDuration() const;
    int64_t getRecordedSamples() const { return samplesRecorded.load(); }

    // Latency compensation
    void setLatencyCompensationSamples(int samples) { latencyCompensationSamples = samples; }
    int getLatencyCompensationSamples() const { return latencyCompensationSamples; }

    // Get the recording start position (in samples on the timeline)
    int64_t getRecordingStartPosition() const { return recordingStartPosition; }

    // Get the compensated start position (start position adjusted for latency)
    int64_t getCompensatedStartPosition() const;

private:
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::TimeSliceThread writerThread { "Audio Writer" };

    std::atomic<bool> recording { false };
    std::atomic<int64_t> samplesRecorded { 0 };
    double currentSampleRate = 44100.0;

    // Latency compensation
    int latencyCompensationSamples { 0 };
    int64_t recordingStartPosition { 0 };

    juce::CriticalSection writerLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioRecorder)
};
