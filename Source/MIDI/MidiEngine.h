#pragma once

#include "SimpleSynthSound.h"
#include "SimpleSynthVoice.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>

// Forward declaration
class MidiTrack;

/**
 * MidiEngine manages MIDI playback and synthesis.
 * Owns a Synthesiser with multiple voices for polyphonic playback.
 * Implements MidiInputCallback to receive MIDI input from devices.
 */
class MidiEngine : public juce::MidiInputCallback
{
public:
    MidiEngine()
    {
        // Add sound
        synth.addSound(new SimpleSynthSound());

        // Add voices for polyphony (16 voices)
        for (int i = 0; i < 16; ++i)
        {
            synth.addVoice(new SimpleSynthVoice());
        }
    }

    ~MidiEngine() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        synth.setCurrentPlaybackSampleRate(sampleRate);
        currentSampleRate = sampleRate;
        juce::ignoreUnused(samplesPerBlock);
    }

    /**
     * Render MIDI events to audio.
     * @param midiMessages MIDI buffer containing events for this block
     * @param audioBuffer Output audio buffer
     * @param numSamples Number of samples to render
     */
    void renderNextBlock(juce::AudioBuffer<float>& audioBuffer,
                         const juce::MidiBuffer& midiMessages,
                         int numSamples)
    {
        synth.renderNextBlock(audioBuffer, midiMessages, 0, numSamples);
    }

    /**
     * Handle a single MIDI message immediately.
     * Useful for real-time input.
     */
    void handleMidiEvent(const juce::MidiMessage& message)
    {
        if (message.isNoteOn())
        {
            synth.noteOn(message.getChannel(),
                         message.getNoteNumber(),
                         message.getFloatVelocity());
        }
        else if (message.isNoteOff())
        {
            synth.noteOff(message.getChannel(),
                          message.getNoteNumber(),
                          message.getFloatVelocity(),
                          true);
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            synth.allNotesOff(message.getChannel(), true);
        }
    }

    void allNotesOff()
    {
        for (int ch = 1; ch <= 16; ++ch)
            synth.allNotesOff(ch, true);
    }

    void releaseResources()
    {
        allNotesOff();
    }

    double getSampleRate() const { return currentSampleRate; }

    //==========================================================================
    // MidiInputCallback interface
    //==========================================================================

    /**
     * Called when a MIDI message arrives from an input device.
     * This is the core of the MidiInputCallback interface.
     * @param source The MIDI input device that sent the message
     * @param message The incoming MIDI message
     */
    void handleIncomingMidiMessage(juce::MidiInput* source,
                                   const juce::MidiMessage& message) override
    {
        juce::ignoreUnused(source);

        // Always play through the synth for monitoring
        handleMidiEvent(message);

        // If recording is active and we have an armed track, store the event
        if (isRecording && onMidiInputReceived)
        {
            // Forward the message with current timestamp to the callback
            // The callback (typically in AudioEngine) will handle storing to the armed track
            onMidiInputReceived(message, recordingTimestamp);
        }
    }

    //==========================================================================
    // MIDI Recording Control
    //==========================================================================

    /**
     * Start MIDI recording.
     * @param startTimestamp The transport position when recording starts (in samples)
     */
    void startRecording(int64_t startTimestamp)
    {
        recordingStartTimestamp = startTimestamp;
        recordingTimestamp = startTimestamp;
        isRecording = true;
    }

    /**
     * Stop MIDI recording.
     */
    void stopRecording()
    {
        isRecording = false;
    }

    /**
     * Update the current recording timestamp.
     * Should be called from the audio thread to keep timestamps in sync.
     * @param currentPosition Current transport position in samples
     */
    void updateRecordingTimestamp(int64_t currentPosition)
    {
        recordingTimestamp = currentPosition;
    }

    /**
     * Check if MIDI recording is active.
     * @return true if recording
     */
    bool isMidiRecording() const { return isRecording; }

    /**
     * Get the timestamp when recording started.
     * @return Recording start position in samples
     */
    int64_t getRecordingStartTimestamp() const { return recordingStartTimestamp; }

    /**
     * Callback function type for MIDI input events.
     * Parameters: MidiMessage, timestamp in samples
     */
    std::function<void(const juce::MidiMessage&, int64_t)> onMidiInputReceived;

private:
    juce::Synthesiser synth;
    double currentSampleRate = 44100.0;

    // MIDI recording state
    bool isRecording = false;
    int64_t recordingTimestamp = 0;
    int64_t recordingStartTimestamp = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEngine)
};
