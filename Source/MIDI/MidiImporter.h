#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

/**
 * MidiImporter handles loading standard MIDI files.
 */
class MidiImporter
{
public:
    MidiImporter() = default;
    
    // Check if the file extension is supported
    bool isSupported(const juce::File& file) const;
    
    // Returns the number of tracks in the MIDI file
    int getNumTracks(const juce::File& file);
    
    // Import MIDI data from a file for a specific track index
    // Converts timestamps to samples based on the provided sample rate
    juce::MidiMessageSequence importTrack(const juce::File& file, int trackIndex, double sampleRate);

private:
    juce::MidiFile midiFile;
};
