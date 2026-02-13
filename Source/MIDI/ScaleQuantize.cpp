#include "ScaleQuantize.h"
#include <cmath>
#include <climits>

int ScaleQuantize::quantizeNote(int noteNumber, int rootNote, ScaleType scaleType)
{
    // Chromatic scale contains all notes, no quantization needed
    if (scaleType == ScaleType::Chromatic)
        return juce::jlimit(0, 127, noteNumber);

    rootNote = juce::jlimit(0, 11, rootNote);

    // Calculate the semitone offset relative to the root note within one octave
    int semitoneOffset = ((noteNumber - rootNote) % 12 + 12) % 12;

    // Find the nearest in-scale semitone
    int quantizedOffset = 0;

    switch (scaleType)
    {
        case ScaleType::Major:
            quantizedOffset = findNearestInScale(semitoneOffset,
                majorIntervals.data(), static_cast<int>(majorIntervals.size()));
            break;

        case ScaleType::Minor:
            quantizedOffset = findNearestInScale(semitoneOffset,
                minorIntervals.data(), static_cast<int>(minorIntervals.size()));
            break;

        case ScaleType::HarmonicMinor:
            quantizedOffset = findNearestInScale(semitoneOffset,
                harmonicMinorIntervals.data(), static_cast<int>(harmonicMinorIntervals.size()));
            break;

        case ScaleType::Pentatonic:
            quantizedOffset = findNearestInScale(semitoneOffset,
                pentatonicIntervals.data(), static_cast<int>(pentatonicIntervals.size()));
            break;

        case ScaleType::Blues:
            quantizedOffset = findNearestInScale(semitoneOffset,
                bluesIntervals.data(), static_cast<int>(bluesIntervals.size()));
            break;

        case ScaleType::Chromatic:
            return juce::jlimit(0, 127, noteNumber);
    }

    // Reconstruct the note: find the octave base and apply quantized offset
    // The octave base is the nearest root note at or below the original note
    int octaveBase = noteNumber - semitoneOffset + rootNote;

    // Adjust if the root offset puts us in the wrong octave
    while (octaveBase > noteNumber)
        octaveBase -= 12;
    while (octaveBase + 12 <= noteNumber - 6)
        octaveBase += 12;

    int quantizedNote = octaveBase - rootNote + quantizedOffset;

    // Ensure octave alignment: the quantized note should be in the same octave region
    // as the original note. Recalculate from the original note's octave.
    int originalOctaveBase = noteNumber - semitoneOffset;
    quantizedNote = originalOctaveBase + quantizedOffset;

    return juce::jlimit(0, 127, quantizedNote);
}

void ScaleQuantize::applyToRegion(MidiRegion& region, int rootNote, ScaleType scaleType)
{
    if (scaleType == ScaleType::Chromatic)
        return;

    auto& seq = region.getMidiSequence();

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        auto* event = seq.getEventPointer(i);
        if (event == nullptr)
            continue;

        auto& msg = event->message;

        if (msg.isNoteOn() || msg.isNoteOff())
        {
            int originalNote = msg.getNoteNumber();
            int quantizedNote = quantizeNote(originalNote, rootNote, scaleType);
            msg.setNoteNumber(quantizedNote);
        }
    }

    seq.updateMatchedPairs();
}

bool ScaleQuantize::isNoteInScale(int noteNumber, int rootNote, ScaleType scaleType)
{
    if (scaleType == ScaleType::Chromatic)
        return true;

    rootNote = juce::jlimit(0, 11, rootNote);
    int semitoneOffset = ((noteNumber - rootNote) % 12 + 12) % 12;

    const int* intervals = nullptr;
    int numIntervals = 0;

    switch (scaleType)
    {
        case ScaleType::Major:
            intervals = majorIntervals.data();
            numIntervals = static_cast<int>(majorIntervals.size());
            break;
        case ScaleType::Minor:
            intervals = minorIntervals.data();
            numIntervals = static_cast<int>(minorIntervals.size());
            break;
        case ScaleType::HarmonicMinor:
            intervals = harmonicMinorIntervals.data();
            numIntervals = static_cast<int>(harmonicMinorIntervals.size());
            break;
        case ScaleType::Pentatonic:
            intervals = pentatonicIntervals.data();
            numIntervals = static_cast<int>(pentatonicIntervals.size());
            break;
        case ScaleType::Blues:
            intervals = bluesIntervals.data();
            numIntervals = static_cast<int>(bluesIntervals.size());
            break;
        case ScaleType::Chromatic:
            return true;
    }

    if (intervals == nullptr)
        return true;

    for (int i = 0; i < numIntervals; ++i)
    {
        if (intervals[i] == semitoneOffset)
            return true;
    }

    return false;
}

juce::String ScaleQuantize::getScaleName(ScaleType scaleType)
{
    switch (scaleType)
    {
        case ScaleType::Major:          return "Major";
        case ScaleType::Minor:          return "Minor";
        case ScaleType::HarmonicMinor:  return "Harmonic Minor";
        case ScaleType::Pentatonic:     return "Pentatonic";
        case ScaleType::Blues:          return "Blues";
        case ScaleType::Chromatic:      return "Chromatic";
        default:                        return "Unknown";
    }
}

int ScaleQuantize::findNearestInScale(int semitoneOffset, const int* intervals, int numIntervals)
{
    int bestInterval = 0;
    int bestDistance = INT_MAX;

    for (int i = 0; i < numIntervals; ++i)
    {
        // Calculate distance considering octave wrapping
        int distance = std::abs(semitoneOffset - intervals[i]);
        int wrappedDistance = 12 - distance;
        int minDistance = juce::jmin(distance, wrappedDistance);

        if (minDistance < bestDistance)
        {
            bestDistance = minDistance;
            bestInterval = intervals[i];
        }
        else if (minDistance == bestDistance)
        {
            // Prefer the lower note when equidistant
            if (intervals[i] < bestInterval)
                bestInterval = intervals[i];
        }
    }

    return bestInterval;
}
