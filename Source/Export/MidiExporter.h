#pragma once

#include "../Timeline/Timeline.h"
#include "../MIDI/MidiTrack.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

/**
 * MidiExporter exports MIDI tracks to a Standard MIDI File.
 */
class MidiExporter
{
public:
    MidiExporter() = default;
    ~MidiExporter() = default;

    /**
     * Export all MIDI tracks from timeline to a .mid file.
     */
    bool exportToFile(const Timeline& timeline, const juce::File& file);

    /**
     * Export a single MIDI track to a .mid file.
     */
    bool exportTrackToFile(const MidiTrack& track, const juce::File& file, double bpm = 120.0);

private:
    static constexpr int TICKS_PER_QUARTER_NOTE = 960;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiExporter)
};
