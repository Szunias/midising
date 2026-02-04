#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <memory>
#include "RingBuffer.h"

/**
 * AudioRecorder captures audio input to a WAV file.
 * Uses a lock-free ring buffer for thread-safe audio recording.
 *
 * Thread Safety:
 * =============
 * - writeAudioBlock() is lock-free and safe to call from audio thread
 * - Uses SPSC ring buffer: audio thread writes, background thread reads
 * - Background thread handles all file I/O operations
 *
 * Signal Flow:
 * ===========
 * Audio Thread: writeAudioBlock() -> RingBuffer (lock-free push)
 * Writer Thread: RingBuffer (pop) -> ThreadedWriter -> Disk
 *
 * Supports latency compensation for accurate region positioning.
 */
class AudioRecorder : private juce::TimeSliceClient
{
public:
    AudioRecorder();
    ~AudioRecorder();

    /**
     * Start recording to a file.
     * @param file The destination WAV file
     * @param sampleRate Sample rate for recording
     * @param numChannels Number of audio channels (1 or 2)
     * @param startPositionSamples Timeline position where recording starts
     * @return true if recording started successfully
     */
    bool startRecording(const juce::File& file, double sampleRate, int numChannels,
                        int64_t startPositionSamples = 0);

    /**
     * Stop recording and finalize the file.
     */
    void stopRecording();

    /**
     * Check if currently recording.
     */
    bool isRecording() const { return recording.load(); }

    /**
     * Write audio samples to the ring buffer (call from audio callback).
     * This method is lock-free and safe for real-time audio threads.
     *
     * @param buffer Audio buffer containing samples to record
     */
    void writeAudioBlock(const juce::AudioBuffer<float>& buffer);

    /**
     * Get recorded duration in seconds.
     */
    double getRecordedDuration() const;

    /**
     * Get total number of samples recorded.
     */
    int64_t getRecordedSamples() const { return samplesRecorded.load(); }

    // Latency compensation
    void setLatencyCompensationSamples(int samples) { latencyCompensationSamples = samples; }
    int getLatencyCompensationSamples() const { return latencyCompensationSamples; }

    /**
     * Get the recording start position (in samples on the timeline).
     */
    int64_t getRecordingStartPosition() const { return recordingStartPosition; }

    /**
     * Get the compensated start position (start position adjusted for latency).
     */
    int64_t getCompensatedStartPosition() const;

    /**
     * Get ring buffer fill level (0.0 to 1.0).
     * Useful for monitoring if audio thread is outpacing writer thread.
     */
    float getRingBufferFillLevel() const;

    /**
     * Check if any samples were dropped due to ring buffer overflow.
     */
    int64_t getDroppedSamples() const { return samplesDropped.load(); }

private:
    // TimeSliceClient interface - called by background thread to drain ring buffer
    int useTimeSlice() override;

    // Transfer samples from ring buffer to file writer
    void drainRingBuffer();

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::TimeSliceThread writerThread { "Audio Writer" };

    // Lock-free ring buffer for audio data
    // Size: ~3 seconds at 48kHz stereo = 288KB, rounded to power of 2
    static constexpr int RING_BUFFER_SIZE = 262144;  // 2^18 samples (~5.5 sec at 48kHz)
    std::unique_ptr<RingBuffer<float>> ringBuffer;

    // Temporary buffer for draining ring buffer
    juce::AudioBuffer<float> drainBuffer;
    static constexpr int DRAIN_BLOCK_SIZE = 4096;

    std::atomic<bool> recording { false };
    std::atomic<int64_t> samplesRecorded { 0 };
    std::atomic<int64_t> samplesDropped { 0 };  // Samples lost due to buffer overflow
    double currentSampleRate = 44100.0;
    int currentNumChannels = 2;

    // Latency compensation
    int latencyCompensationSamples { 0 };
    int64_t recordingStartPosition { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioRecorder)
};
