#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/**
 * SimpleSynthSound defines which notes this synth can play.
 * Our simple synth plays all notes.
 */
class SimpleSynthSound : public juce::SynthesiserSound
{
public:
    SimpleSynthSound() = default;

    bool appliesToNote(int midiNoteNumber) override
    {
        juce::ignoreUnused(midiNoteNumber);
        return true; // Can play any note
    }

    bool appliesToChannel(int midiChannel) override
    {
        juce::ignoreUnused(midiChannel);
        return true; // Responds to all channels
    }
};
