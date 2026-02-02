#pragma once

#include "SimpleSynthSound.h"
#include "SimpleSynthVoice.h"
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * MidiEngine manages MIDI playback and synthesis.
 * Owns a Synthesiser with multiple voices for polyphonic playback.
 */
class MidiEngine
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

    ~MidiEngine() = default;

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

private:
    juce::Synthesiser synth;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEngine)
};
