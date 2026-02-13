#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "../Timeline/Region.h"
#include <array>

/**
 * Scale types for quantization.
 */
enum class ScaleType
{
    Major,
    Minor,
    HarmonicMinor,
    Pentatonic,
    Blues,
    Chromatic
};

/**
 * ScaleQuantize provides static methods for snapping MIDI notes to musical scales.
 * Can operate on individual notes or entire MidiRegions.
 */
class ScaleQuantize
{
public:
    //==========================================================================
    // Scale interval tables (semitones from root that are in-scale)
    // Each array contains the semitone offsets within one octave (0-11)
    //==========================================================================

    static constexpr std::array<int, 7> majorIntervals       = {{ 0, 2, 4, 5, 7, 9, 11 }};
    static constexpr std::array<int, 7> minorIntervals       = {{ 0, 2, 3, 5, 7, 8, 10 }};
    static constexpr std::array<int, 7> harmonicMinorIntervals = {{ 0, 2, 3, 5, 7, 8, 11 }};
    static constexpr std::array<int, 5> pentatonicIntervals  = {{ 0, 2, 4, 7, 9 }};
    static constexpr std::array<int, 6> bluesIntervals       = {{ 0, 3, 5, 6, 7, 10 }};

    /**
     * Quantize a single MIDI note number to the nearest in-scale note.
     * @param noteNumber The original MIDI note number (0-127)
     * @param rootNote The root note of the scale (0-11, where 0=C, 1=C#, etc.)
     * @param scaleType The scale to quantize to
     * @return The quantized note number (0-127)
     */
    static int quantizeNote(int noteNumber, int rootNote, ScaleType scaleType);

    /**
     * Apply scale quantization to all notes in a MidiRegion.
     * Modifies the region's MidiMessageSequence in-place.
     * @param region The MIDI region to process
     * @param rootNote The root note of the scale (0-11)
     * @param scaleType The scale to quantize to
     */
    static void applyToRegion(MidiRegion& region, int rootNote, ScaleType scaleType);

    /**
     * Check if a note is in the given scale.
     * @param noteNumber The MIDI note number (0-127)
     * @param rootNote The root note (0-11)
     * @param scaleType The scale type
     * @return true if the note is in the scale
     */
    static bool isNoteInScale(int noteNumber, int rootNote, ScaleType scaleType);

    /**
     * Get the name of a scale type as a string.
     */
    static juce::String getScaleName(ScaleType scaleType);

private:
    /**
     * Find the nearest in-scale semitone offset for a given semitone offset.
     * @param semitoneOffset The note's semitone offset from root (0-11)
     * @param intervals Pointer to the scale interval array
     * @param numIntervals Number of intervals in the array
     * @return The nearest in-scale semitone offset
     */
    static int findNearestInScale(int semitoneOffset, const int* intervals, int numIntervals);
};
