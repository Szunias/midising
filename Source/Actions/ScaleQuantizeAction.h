#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Timeline/Region.h"
#include "../MIDI/ScaleQuantize.h"
#include <vector>
#include <utility>
#include <cmath>

/**
 * ScaleQuantizeAction - undoable action for applying scale quantization to a MIDI region.
 *
 * Stores the original note numbers for all affected note-on and note-off events
 * so that the quantization can be fully reversed on undo.
 */
class ScaleQuantizeAction : public juce::UndoableAction
{
public:
    /**
     * @param region The MIDI region to quantize
     * @param rootNote The root note of the scale (0-11, where 0=C)
     * @param scaleType The scale type to quantize to
     */
    ScaleQuantizeAction(MidiRegion& region, int rootNote, ScaleType scaleType)
        : regionRef(region), root(rootNote), scale(scaleType)
    {
        auto& seq = region.getMidiSequence();

        // Store the original note number for every note-on event
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event == nullptr)
                continue;

            const auto& msg = event->message;

            if (msg.isNoteOn())
            {
                int originalNote = msg.getNoteNumber();
                int quantizedNote = ScaleQuantize::quantizeNote(originalNote, rootNote, scaleType);

                // Only store if the note will actually change
                if (originalNote != quantizedNote)
                {
                    NoteChange change;
                    change.eventIndex = i;
                    change.originalNoteNumber = originalNote;
                    change.quantizedNoteNumber = quantizedNote;
                    change.channel = msg.getChannel();
                    change.startTimeTicks = msg.getTimeStamp();

                    noteChanges.push_back(change);
                }
            }
        }
    }

    bool perform() override
    {
        if (!wasPerformed && !noteChanges.empty())
        {
            applyNoteChanges(true);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && !noteChanges.empty())
        {
            applyNoteChanges(false);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override
    {
        return static_cast<int>(5 + noteChanges.size() * 2);
    }

private:
    struct NoteChange
    {
        int eventIndex;
        int originalNoteNumber;
        int quantizedNoteNumber;
        int channel;
        double startTimeTicks;
    };

    void applyNoteChanges(bool forward)
    {
        auto& seq = regionRef.getMidiSequence();

        for (const auto& change : noteChanges)
        {
            int fromNote = forward ? change.originalNoteNumber : change.quantizedNoteNumber;
            int toNote = forward ? change.quantizedNoteNumber : change.originalNoteNumber;

            // Find the note-on event by matching properties
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* event = seq.getEventPointer(i);
                if (event == nullptr)
                    continue;

                auto& msg = event->message;

                if (msg.isNoteOn() &&
                    msg.getNoteNumber() == fromNote &&
                    msg.getChannel() == change.channel &&
                    std::abs(msg.getTimeStamp() - change.startTimeTicks) < 1.0)
                {
                    // Update note-on
                    msg.setNoteNumber(toNote);

                    // Update matching note-off
                    if (event->noteOffObject != nullptr)
                    {
                        event->noteOffObject->message.setNoteNumber(toNote);
                    }
                    break;
                }
            }
        }

        seq.updateMatchedPairs();
    }

    MidiRegion& regionRef;
    int root;
    ScaleType scale;
    std::vector<NoteChange> noteChanges;
    bool wasPerformed = false;
};
