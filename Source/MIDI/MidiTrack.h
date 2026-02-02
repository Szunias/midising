#pragma once

#include "../Timeline/Track.h"
#include "../Timeline/Region.h"
#include "MidiEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>
#include <atomic>

/**
 * MidiTrack is a track that plays MIDI data through a synthesizer.
 * Supports MIDI recording when armed.
 */
class MidiTrack : public Track
{
public:
    MidiTrack(const juce::String& name = "MIDI Track", MidiEngine* engine = nullptr);
    ~MidiTrack() override = default;

    // Track interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;

    // Set the MIDI engine to use for synthesis
    void setMidiEngine(MidiEngine* engine) { midiEngine = engine; }

    // Region management
    void addRegion(std::unique_ptr<MidiRegion> region);
    void removeRegion(int index);
    MidiRegion* getRegion(int index);
    const MidiRegion* getRegion(int index) const { return regions[static_cast<size_t>(index)].get(); }
    int getNumRegions() const { return static_cast<int>(regions.size()); }
    void clearRegions();

    // Direct note access (for piano roll editing)
    juce::MidiMessageSequence& getMidiSequence() { return midiSequence; }
    const juce::MidiMessageSequence& getMidiSequence() const { return midiSequence; }

    // ========== MIDI Recording ==========

    /**
     * Start recording MIDI input. Track must be armed first.
     * @param startPositionInSamples The timeline position where recording begins
     * @return true if recording started successfully
     */
    bool startRecording(int64_t startPositionInSamples);

    /**
     * Stop recording and create a new MidiRegion with the recorded messages.
     * @return Pointer to the newly created region, or nullptr if no messages were recorded
     */
    MidiRegion* stopRecording();

    /**
     * Check if the track is currently recording.
     */
    bool isRecording() const { return recording.load(); }

    /**
     * Add a MIDI message during recording. Messages are timestamped relative to recording start.
     * @param message The MIDI message to record
     * @param timestampInSamples Absolute timestamp in samples (will be converted to relative)
     */
    void addRecordedMessage(const juce::MidiMessage& message, int64_t timestampInSamples);

    /**
     * Get the recording start position in samples.
     */
    int64_t getRecordingStartPosition() const { return recordingStartPosition.load(); }

    /**
     * Get the number of messages recorded so far.
     */
    int getNumRecordedMessages() const;

private:
    std::vector<std::unique_ptr<MidiRegion>> regions;
    juce::MidiMessageSequence midiSequence; // Master sequence combining all regions
    MidiEngine* midiEngine = nullptr;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    // Recording state
    std::atomic<bool> recording { false };
    std::atomic<int64_t> recordingStartPosition { 0 };
    juce::MidiMessageSequence recordingBuffer;  // Buffer for incoming MIDI during recording
    mutable juce::CriticalSection recordingLock;  // Protects recordingBuffer (mutable for const methods)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiTrack)
};
