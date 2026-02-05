#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Timeline/Region.h"
#include <vector>
#include <algorithm>
#include <random>
#include <ctime>
#include <cmath>

/**
 * NoteData - holds all data needed to recreate a MIDI note
 */
struct NoteData
{
    int noteNumber = 60;
    float velocity = 0.8f;
    int channel = 1;
    double startTimeTicks = 0.0;
    double endTimeTicks = 0.0;

    NoteData() = default;

    NoteData(int note, float vel, int ch, double startTicks, double endTicks)
        : noteNumber(note), velocity(vel), channel(ch),
          startTimeTicks(startTicks), endTimeTicks(endTicks)
    {
    }

    // Helper to calculate duration in ticks
    double getDurationTicks() const { return endTimeTicks - startTimeTicks; }
};

/**
 * AddNoteAction - undoable action for adding a MIDI note
 */
class AddNoteAction : public juce::UndoableAction
{
public:
    AddNoteAction(MidiRegion& region, const NoteData& note)
        : regionRef(region), noteToAdd(note)
    {
    }

    AddNoteAction(MidiRegion& region, int noteNumber, float velocity, int channel,
                  double startTicks, double endTicks)
        : regionRef(region),
          noteToAdd(noteNumber, velocity, channel, startTicks, endTicks)
    {
    }

    bool perform() override
    {
        if (!wasPerformed)
        {
            auto& seq = regionRef.getMidiSequence();

            // Add note-on event
            juce::MidiMessage noteOn = juce::MidiMessage::noteOn(
                noteToAdd.channel, noteToAdd.noteNumber, noteToAdd.velocity);
            noteOn.setTimeStamp(noteToAdd.startTimeTicks);
            seq.addEvent(noteOn);

            // Add note-off event
            juce::MidiMessage noteOff = juce::MidiMessage::noteOff(
                noteToAdd.channel, noteToAdd.noteNumber, 0.0f);
            noteOff.setTimeStamp(noteToAdd.endTimeTicks);
            seq.addEvent(noteOff);

            // Sort and update matched pairs
            seq.sort();
            seq.updateMatchedPairs();

            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed)
        {
            auto& seq = regionRef.getMidiSequence();

            // Find and remove the note-on and note-off events
            // We search by matching timestamp, channel, and note number
            std::vector<int> indicesToDelete;

            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* event = seq.getEventPointer(i);
                if (event == nullptr)
                    continue;

                const auto& msg = event->message;

                // Check for matching note-on
                if (msg.isNoteOn() &&
                    msg.getNoteNumber() == noteToAdd.noteNumber &&
                    msg.getChannel() == noteToAdd.channel &&
                    std::abs(msg.getTimeStamp() - noteToAdd.startTimeTicks) < 1.0)
                {
                    indicesToDelete.push_back(i);

                    // Also add the matched note-off
                    if (event->noteOffObject != nullptr)
                    {
                        for (int j = 0; j < seq.getNumEvents(); ++j)
                        {
                            if (seq.getEventPointer(j) == event->noteOffObject)
                            {
                                indicesToDelete.push_back(j);
                                break;
                            }
                        }
                    }
                    break;
                }
            }

            // Delete in reverse order
            std::sort(indicesToDelete.begin(), indicesToDelete.end(), std::greater<int>());
            indicesToDelete.erase(std::unique(indicesToDelete.begin(), indicesToDelete.end()),
                                  indicesToDelete.end());

            for (int index : indicesToDelete)
            {
                seq.deleteEvent(index, false);
            }

            seq.updateMatchedPairs();
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    MidiRegion& regionRef;
    NoteData noteToAdd;
    bool wasPerformed = false;
};

/**
 * DeleteNoteAction - undoable action for deleting a MIDI note
 * Stores the note data for restoration on undo
 */
class DeleteNoteAction : public juce::UndoableAction
{
public:
    DeleteNoteAction(MidiRegion& region, int eventIndex)
        : regionRef(region), originalIndex(eventIndex)
    {
        auto& seq = region.getMidiSequence();

        if (eventIndex >= 0 && eventIndex < seq.getNumEvents())
        {
            auto* event = seq.getEventPointer(eventIndex);

            if (event != nullptr && event->message.isNoteOn())
            {
                savedNote.noteNumber = event->message.getNoteNumber();
                savedNote.velocity = event->message.getFloatVelocity();
                savedNote.channel = event->message.getChannel();
                savedNote.startTimeTicks = event->message.getTimeStamp();

                // Get note-off time
                if (event->noteOffObject != nullptr)
                {
                    savedNote.endTimeTicks = event->noteOffObject->message.getTimeStamp();
                }
                else
                {
                    // Default to half beat duration if no note-off
                    savedNote.endTimeTicks = savedNote.startTimeTicks + 480.0;
                }

                hasValidData = true;
            }
        }
    }

    bool perform() override
    {
        if (hasValidData && !wasDeleted)
        {
            auto& seq = regionRef.getMidiSequence();

            // Find the note by matching properties
            std::vector<int> indicesToDelete;

            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* event = seq.getEventPointer(i);
                if (event == nullptr)
                    continue;

                const auto& msg = event->message;

                if (msg.isNoteOn() &&
                    msg.getNoteNumber() == savedNote.noteNumber &&
                    msg.getChannel() == savedNote.channel &&
                    std::abs(msg.getTimeStamp() - savedNote.startTimeTicks) < 1.0)
                {
                    indicesToDelete.push_back(i);

                    // Find matching note-off
                    if (event->noteOffObject != nullptr)
                    {
                        for (int j = 0; j < seq.getNumEvents(); ++j)
                        {
                            if (seq.getEventPointer(j) == event->noteOffObject)
                            {
                                indicesToDelete.push_back(j);
                                break;
                            }
                        }
                    }
                    break;
                }
            }

            // Delete in reverse order
            std::sort(indicesToDelete.begin(), indicesToDelete.end(), std::greater<int>());
            indicesToDelete.erase(std::unique(indicesToDelete.begin(), indicesToDelete.end()),
                                  indicesToDelete.end());

            for (int index : indicesToDelete)
            {
                seq.deleteEvent(index, false);
            }

            seq.updateMatchedPairs();
            wasDeleted = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasDeleted && hasValidData)
        {
            auto& seq = regionRef.getMidiSequence();

            // Re-add the note
            juce::MidiMessage noteOn = juce::MidiMessage::noteOn(
                savedNote.channel, savedNote.noteNumber, savedNote.velocity);
            noteOn.setTimeStamp(savedNote.startTimeTicks);
            seq.addEvent(noteOn);

            juce::MidiMessage noteOff = juce::MidiMessage::noteOff(
                savedNote.channel, savedNote.noteNumber, 0.0f);
            noteOff.setTimeStamp(savedNote.endTimeTicks);
            seq.addEvent(noteOff);

            seq.sort();
            seq.updateMatchedPairs();

            wasDeleted = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    MidiRegion& regionRef;
    int originalIndex;
    NoteData savedNote;
    bool hasValidData = false;
    bool wasDeleted = false;
};

/**
 * MoveNoteAction - undoable action for moving a note to a new position and/or pitch
 */
class MoveNoteAction : public juce::UndoableAction
{
public:
    MoveNoteAction(MidiRegion& region, int eventIndex, double newStartTicks, int newNoteNumber)
        : regionRef(region), targetIndex(eventIndex),
          newStart(newStartTicks), newPitch(newNoteNumber)
    {
        auto& seq = region.getMidiSequence();

        if (eventIndex >= 0 && eventIndex < seq.getNumEvents())
        {
            auto* event = seq.getEventPointer(eventIndex);

            if (event != nullptr && event->message.isNoteOn())
            {
                oldStart = event->message.getTimeStamp();
                oldPitch = event->message.getNoteNumber();
                channel = event->message.getChannel();
                velocity = event->message.getFloatVelocity();

                // Get note duration
                if (event->noteOffObject != nullptr)
                {
                    duration = event->noteOffObject->message.getTimeStamp() - oldStart;
                }
                else
                {
                    duration = 480.0; // Default half beat
                }

                hasValidData = true;
            }
        }
    }

    bool perform() override
    {
        if (hasValidData && !wasPerformed)
        {
            return applyMove(oldStart, oldPitch, newStart, newPitch);
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            return applyMove(newStart, newPitch, oldStart, oldPitch);
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    bool applyMove(double fromStart, int fromPitch, double toStart, int toPitch)
    {
        auto& seq = regionRef.getMidiSequence();

        // Find the note by properties
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event == nullptr)
                continue;

            auto& msg = event->message;

            if (msg.isNoteOn() &&
                msg.getNoteNumber() == fromPitch &&
                msg.getChannel() == channel &&
                std::abs(msg.getTimeStamp() - fromStart) < 1.0)
            {
                // Update note-on
                msg.setNoteNumber(toPitch);
                msg.setTimeStamp(toStart);

                // Update note-off
                if (event->noteOffObject != nullptr)
                {
                    event->noteOffObject->message.setNoteNumber(toPitch);
                    event->noteOffObject->message.setTimeStamp(toStart + duration);
                }

                seq.sort();
                seq.updateMatchedPairs();

                wasPerformed = !wasPerformed;
                return true;
            }
        }
        return false;
    }

    MidiRegion& regionRef;
    int targetIndex;

    double oldStart = 0.0;
    int oldPitch = 60;
    double newStart = 0.0;
    int newPitch = 60;
    double duration = 480.0;
    int channel = 1;
    float velocity = 0.8f;

    bool hasValidData = false;
    bool wasPerformed = false;
};

/**
 * ResizeNoteAction - undoable action for changing a note's duration
 */
class ResizeNoteAction : public juce::UndoableAction
{
public:
    ResizeNoteAction(MidiRegion& region, int eventIndex, double newEndTicks)
        : regionRef(region), targetIndex(eventIndex), newEnd(newEndTicks)
    {
        auto& seq = region.getMidiSequence();

        if (eventIndex >= 0 && eventIndex < seq.getNumEvents())
        {
            auto* event = seq.getEventPointer(eventIndex);

            if (event != nullptr && event->message.isNoteOn())
            {
                startTicks = event->message.getTimeStamp();
                noteNumber = event->message.getNoteNumber();
                channel = event->message.getChannel();

                if (event->noteOffObject != nullptr)
                {
                    oldEnd = event->noteOffObject->message.getTimeStamp();
                }
                else
                {
                    oldEnd = startTicks + 480.0;
                }

                hasValidData = true;
            }
        }
    }

    bool perform() override
    {
        if (hasValidData && !wasPerformed)
        {
            return applyResize(oldEnd, newEnd);
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            return applyResize(newEnd, oldEnd);
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    bool applyResize(double fromEnd, double toEnd)
    {
        auto& seq = regionRef.getMidiSequence();

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event == nullptr)
                continue;

            const auto& msg = event->message;

            if (msg.isNoteOn() &&
                msg.getNoteNumber() == noteNumber &&
                msg.getChannel() == channel &&
                std::abs(msg.getTimeStamp() - startTicks) < 1.0)
            {
                if (event->noteOffObject != nullptr &&
                    std::abs(event->noteOffObject->message.getTimeStamp() - fromEnd) < 1.0)
                {
                    event->noteOffObject->message.setTimeStamp(toEnd);
                    seq.sort();
                    seq.updateMatchedPairs();
                    wasPerformed = !wasPerformed;
                    return true;
                }
            }
        }
        return false;
    }

    MidiRegion& regionRef;
    int targetIndex;

    double startTicks = 0.0;
    int noteNumber = 60;
    int channel = 1;
    double oldEnd = 0.0;
    double newEnd = 0.0;

    bool hasValidData = false;
    bool wasPerformed = false;
};

/**
 * SetNoteVelocityAction - undoable action for changing a note's velocity
 */
class SetNoteVelocityAction : public juce::UndoableAction
{
public:
    SetNoteVelocityAction(MidiRegion& region, int eventIndex, float newVelocity)
        : regionRef(region), targetIndex(eventIndex), newVel(newVelocity)
    {
        auto& seq = region.getMidiSequence();

        if (eventIndex >= 0 && eventIndex < seq.getNumEvents())
        {
            auto* event = seq.getEventPointer(eventIndex);

            if (event != nullptr && event->message.isNoteOn())
            {
                startTicks = event->message.getTimeStamp();
                noteNumber = event->message.getNoteNumber();
                channel = event->message.getChannel();
                oldVel = event->message.getFloatVelocity();
                hasValidData = true;
            }
        }
    }

    bool perform() override
    {
        if (hasValidData && !wasPerformed)
        {
            return applyVelocity(oldVel, newVel);
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            return applyVelocity(newVel, oldVel);
        }
        return false;
    }

    int getSizeInUnits() override { return 3; }

private:
    bool applyVelocity(float fromVel, float toVel)
    {
        auto& seq = regionRef.getMidiSequence();

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event == nullptr)
                continue;

            auto& msg = event->message;

            if (msg.isNoteOn() &&
                msg.getNoteNumber() == noteNumber &&
                msg.getChannel() == channel &&
                std::abs(msg.getTimeStamp() - startTicks) < 1.0 &&
                std::abs(msg.getFloatVelocity() - fromVel) < 0.001f)
            {
                msg.setVelocity(toVel);
                wasPerformed = !wasPerformed;
                return true;
            }
        }
        return false;
    }

    MidiRegion& regionRef;
    int targetIndex;

    double startTicks = 0.0;
    int noteNumber = 60;
    int channel = 1;
    float oldVel = 0.8f;
    float newVel = 0.8f;

    bool hasValidData = false;
    bool wasPerformed = false;
};

/**
 * DeleteMultipleNotesAction - undoable action for deleting multiple notes
 */
class DeleteMultipleNotesAction : public juce::UndoableAction
{
public:
    DeleteMultipleNotesAction(MidiRegion& region, const std::vector<int>& noteIndices)
        : regionRef(region)
    {
        auto& seq = region.getMidiSequence();

        // Collect data for all notes to delete
        for (int eventIndex : noteIndices)
        {
            if (eventIndex >= 0 && eventIndex < seq.getNumEvents())
            {
                auto* event = seq.getEventPointer(eventIndex);

                if (event != nullptr && event->message.isNoteOn())
                {
                    NoteData noteData;
                    noteData.noteNumber = event->message.getNoteNumber();
                    noteData.velocity = event->message.getFloatVelocity();
                    noteData.channel = event->message.getChannel();
                    noteData.startTimeTicks = event->message.getTimeStamp();

                    if (event->noteOffObject != nullptr)
                    {
                        noteData.endTimeTicks = event->noteOffObject->message.getTimeStamp();
                    }
                    else
                    {
                        noteData.endTimeTicks = noteData.startTimeTicks + 480.0;
                    }

                    savedNotes.push_back(noteData);
                }
            }
        }
    }

    bool perform() override
    {
        if (!wasPerformed && !savedNotes.empty())
        {
            auto& seq = regionRef.getMidiSequence();
            std::vector<int> indicesToDelete;

            // Find all matching notes and their note-offs
            for (const auto& noteData : savedNotes)
            {
                for (int i = 0; i < seq.getNumEvents(); ++i)
                {
                    auto* event = seq.getEventPointer(i);
                    if (event == nullptr)
                        continue;

                    const auto& msg = event->message;

                    if (msg.isNoteOn() &&
                        msg.getNoteNumber() == noteData.noteNumber &&
                        msg.getChannel() == noteData.channel &&
                        std::abs(msg.getTimeStamp() - noteData.startTimeTicks) < 1.0)
                    {
                        indicesToDelete.push_back(i);

                        if (event->noteOffObject != nullptr)
                        {
                            for (int j = 0; j < seq.getNumEvents(); ++j)
                            {
                                if (seq.getEventPointer(j) == event->noteOffObject)
                                {
                                    indicesToDelete.push_back(j);
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
            }

            // Delete in reverse order
            std::sort(indicesToDelete.begin(), indicesToDelete.end(), std::greater<int>());
            indicesToDelete.erase(std::unique(indicesToDelete.begin(), indicesToDelete.end()),
                                  indicesToDelete.end());

            for (int index : indicesToDelete)
            {
                seq.deleteEvent(index, false);
            }

            seq.updateMatchedPairs();
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && !savedNotes.empty())
        {
            auto& seq = regionRef.getMidiSequence();

            // Re-add all notes
            for (const auto& noteData : savedNotes)
            {
                juce::MidiMessage noteOn = juce::MidiMessage::noteOn(
                    noteData.channel, noteData.noteNumber, noteData.velocity);
                noteOn.setTimeStamp(noteData.startTimeTicks);
                seq.addEvent(noteOn);

                juce::MidiMessage noteOff = juce::MidiMessage::noteOff(
                    noteData.channel, noteData.noteNumber, 0.0f);
                noteOff.setTimeStamp(noteData.endTimeTicks);
                seq.addEvent(noteOff);
            }

            seq.sort();
            seq.updateMatchedPairs();

            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override
    {
        return static_cast<int>(5 + savedNotes.size() * 3);
    }

private:
    MidiRegion& regionRef;
    std::vector<NoteData> savedNotes;
    bool wasPerformed = false;
};

/**
 * QuantizeNotesAction - undoable action for quantizing multiple notes to a grid
 */
class QuantizeNotesAction : public juce::UndoableAction
{
public:
    /**
     * Stores original and quantized positions for a note
     */
    struct NoteQuantization
    {
        int noteNumber;
        int channel;
        double originalStartTicks;
        double originalEndTicks;
        double quantizedStartTicks;
        double quantizedEndTicks;
    };

    /**
     * @param region The MIDI region containing notes to quantize
     * @param noteIndices Indices of note-on events to quantize
     * @param gridSizeBeats Grid size in beats (e.g., 0.25 for 1/16 notes)
     * @param strength Quantization strength (0.0 = no change, 1.0 = full quantize)
     * @param ticksPerBeat Ticks per beat (typically 960)
     */
    QuantizeNotesAction(MidiRegion& region, const std::vector<int>& noteIndices,
                        double gridSizeBeats, float strength = 1.0f,
                        double ticksPerBeat = 960.0)
        : regionRef(region), gridSize(gridSizeBeats * ticksPerBeat),
          quantizeStrength(strength)
    {
        auto& seq = region.getMidiSequence();

        for (int eventIndex : noteIndices)
        {
            if (eventIndex >= 0 && eventIndex < seq.getNumEvents())
            {
                auto* event = seq.getEventPointer(eventIndex);

                if (event != nullptr && event->message.isNoteOn())
                {
                    NoteQuantization quant;
                    quant.noteNumber = event->message.getNoteNumber();
                    quant.channel = event->message.getChannel();
                    quant.originalStartTicks = event->message.getTimeStamp();

                    if (event->noteOffObject != nullptr)
                    {
                        quant.originalEndTicks = event->noteOffObject->message.getTimeStamp();
                    }
                    else
                    {
                        quant.originalEndTicks = quant.originalStartTicks + 480.0;
                    }

                    // Calculate quantized position
                    double quantizedStart = std::round(quant.originalStartTicks / gridSize) * gridSize;

                    // Apply strength (interpolate between original and quantized)
                    quant.quantizedStartTicks = quant.originalStartTicks +
                        (quantizedStart - quant.originalStartTicks) * quantizeStrength;

                    // Maintain original duration
                    double duration = quant.originalEndTicks - quant.originalStartTicks;
                    quant.quantizedEndTicks = quant.quantizedStartTicks + duration;

                    quantizations.push_back(quant);
                }
            }
        }
    }

    bool perform() override
    {
        if (!wasPerformed && !quantizations.empty())
        {
            applyQuantization(true);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && !quantizations.empty())
        {
            applyQuantization(false);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override
    {
        return static_cast<int>(5 + quantizations.size() * 2);
    }

private:
    void applyQuantization(bool forward)
    {
        auto& seq = regionRef.getMidiSequence();

        for (const auto& quant : quantizations)
        {
            double searchStart = forward ? quant.originalStartTicks : quant.quantizedStartTicks;
            double targetStart = forward ? quant.quantizedStartTicks : quant.originalStartTicks;
            double targetEnd = forward ? quant.quantizedEndTicks : quant.originalEndTicks;

            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* event = seq.getEventPointer(i);
                if (event == nullptr)
                    continue;

                auto& msg = event->message;

                if (msg.isNoteOn() &&
                    msg.getNoteNumber() == quant.noteNumber &&
                    msg.getChannel() == quant.channel &&
                    std::abs(msg.getTimeStamp() - searchStart) < 1.0)
                {
                    msg.setTimeStamp(targetStart);

                    if (event->noteOffObject != nullptr)
                    {
                        event->noteOffObject->message.setTimeStamp(targetEnd);
                    }
                    break;
                }
            }
        }

        seq.sort();
        seq.updateMatchedPairs();
    }

    MidiRegion& regionRef;
    double gridSize;
    float quantizeStrength;
    std::vector<NoteQuantization> quantizations;
    bool wasPerformed = false;
};

/**
 * TransposeNotesAction - undoable action for transposing multiple notes
 */
class TransposeNotesAction : public juce::UndoableAction
{
public:
    /**
     * @param region The MIDI region containing notes to transpose
     * @param noteIndices Indices of note-on events to transpose
     * @param semitones Number of semitones to transpose (positive = up, negative = down)
     */
    TransposeNotesAction(MidiRegion& region, const std::vector<int>& noteIndices,
                          int semitones)
        : regionRef(region), transposeSemitones(semitones)
    {
        auto& seq = region.getMidiSequence();

        for (int eventIndex : noteIndices)
        {
            if (eventIndex >= 0 && eventIndex < seq.getNumEvents())
            {
                auto* event = seq.getEventPointer(eventIndex);

                if (event != nullptr && event->message.isNoteOn())
                {
                    NoteIdentifier id;
                    id.noteNumber = event->message.getNoteNumber();
                    id.channel = event->message.getChannel();
                    id.startTicks = event->message.getTimeStamp();
                    noteIdentifiers.push_back(id);
                }
            }
        }
    }

    bool perform() override
    {
        if (!wasPerformed && !noteIdentifiers.empty())
        {
            applyTranspose(transposeSemitones);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && !noteIdentifiers.empty())
        {
            applyTranspose(-transposeSemitones);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override
    {
        return static_cast<int>(5 + noteIdentifiers.size());
    }

private:
    struct NoteIdentifier
    {
        int noteNumber;
        int channel;
        double startTicks;
    };

    void applyTranspose(int semitones)
    {
        auto& seq = regionRef.getMidiSequence();

        for (auto& id : noteIdentifiers)
        {
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* event = seq.getEventPointer(i);
                if (event == nullptr)
                    continue;

                auto& msg = event->message;

                if (msg.isNoteOn() &&
                    msg.getNoteNumber() == id.noteNumber &&
                    msg.getChannel() == id.channel &&
                    std::abs(msg.getTimeStamp() - id.startTicks) < 1.0)
                {
                    // Calculate new note number with clamping
                    int newNote = juce::jlimit(0, 127, id.noteNumber + semitones);
                    msg.setNoteNumber(newNote);

                    // Update note-off as well
                    if (event->noteOffObject != nullptr)
                    {
                        event->noteOffObject->message.setNoteNumber(newNote);
                    }

                    // Update identifier for next operation
                    id.noteNumber = newNote;
                    break;
                }
            }
        }

        seq.updateMatchedPairs();
    }

    MidiRegion& regionRef;
    int transposeSemitones;
    std::vector<NoteIdentifier> noteIdentifiers;
    bool wasPerformed = false;
};

/**
 * SplitNoteAction - undoable action for splitting a note at a specific position
 */
class SplitNoteAction : public juce::UndoableAction
{
public:
    SplitNoteAction(MidiRegion& region, int eventIndex, double splitTicks)
        : regionRef(region), targetIndex(eventIndex), splitPosition(splitTicks)
    {
        auto& seq = region.getMidiSequence();

        if (eventIndex >= 0 && eventIndex < seq.getNumEvents())
        {
            auto* event = seq.getEventPointer(eventIndex);

            if (event != nullptr && event->message.isNoteOn())
            {
                originalNote.noteNumber = event->message.getNoteNumber();
                originalNote.velocity = event->message.getFloatVelocity();
                originalNote.channel = event->message.getChannel();
                originalNote.startTimeTicks = event->message.getTimeStamp();

                if (event->noteOffObject != nullptr)
                {
                    originalNote.endTimeTicks = event->noteOffObject->message.getTimeStamp();
                }
                else
                {
                    originalNote.endTimeTicks = originalNote.startTimeTicks + 480.0;
                }

                // Validate split position
                if (splitPosition > originalNote.startTimeTicks &&
                    splitPosition < originalNote.endTimeTicks)
                {
                    hasValidData = true;
                }
            }
        }
    }

    bool perform() override
    {
        if (!hasValidData || wasPerformed)
            return true;

        auto& seq = regionRef.getMidiSequence();

        // Find and modify the original note
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event == nullptr)
                continue;

            const auto& msg = event->message;

            if (msg.isNoteOn() &&
                msg.getNoteNumber() == originalNote.noteNumber &&
                msg.getChannel() == originalNote.channel &&
                std::abs(msg.getTimeStamp() - originalNote.startTimeTicks) < 1.0)
            {
                // Shorten the original note to end at split point
                if (event->noteOffObject != nullptr)
                {
                    event->noteOffObject->message.setTimeStamp(splitPosition);
                }

                // Add the second note (from split point to original end)
                juce::MidiMessage noteOn = juce::MidiMessage::noteOn(
                    originalNote.channel, originalNote.noteNumber, originalNote.velocity);
                noteOn.setTimeStamp(splitPosition);
                seq.addEvent(noteOn);

                juce::MidiMessage noteOff = juce::MidiMessage::noteOff(
                    originalNote.channel, originalNote.noteNumber, 0.0f);
                noteOff.setTimeStamp(originalNote.endTimeTicks);
                seq.addEvent(noteOff);

                seq.sort();
                seq.updateMatchedPairs();

                wasPerformed = true;
                return true;
            }
        }
        return true;
    }

    bool undo() override
    {
        if (!hasValidData || !wasPerformed)
            return false;

        auto& seq = regionRef.getMidiSequence();

        // Find and delete the second note (starting at split position)
        std::vector<int> indicesToDelete;

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event == nullptr)
                continue;

            const auto& msg = event->message;

            if (msg.isNoteOn() &&
                msg.getNoteNumber() == originalNote.noteNumber &&
                msg.getChannel() == originalNote.channel &&
                std::abs(msg.getTimeStamp() - splitPosition) < 1.0)
            {
                indicesToDelete.push_back(i);

                if (event->noteOffObject != nullptr)
                {
                    for (int j = 0; j < seq.getNumEvents(); ++j)
                    {
                        if (seq.getEventPointer(j) == event->noteOffObject)
                        {
                            indicesToDelete.push_back(j);
                            break;
                        }
                    }
                }
                break;
            }
        }

        // Delete in reverse order
        std::sort(indicesToDelete.begin(), indicesToDelete.end(), std::greater<int>());
        for (int index : indicesToDelete)
        {
            seq.deleteEvent(index, false);
        }

        // Restore original note duration
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event == nullptr)
                continue;

            const auto& msg = event->message;

            if (msg.isNoteOn() &&
                msg.getNoteNumber() == originalNote.noteNumber &&
                msg.getChannel() == originalNote.channel &&
                std::abs(msg.getTimeStamp() - originalNote.startTimeTicks) < 1.0)
            {
                if (event->noteOffObject != nullptr)
                {
                    event->noteOffObject->message.setTimeStamp(originalNote.endTimeTicks);
                }
                break;
            }
        }

        seq.sort();
        seq.updateMatchedPairs();

        wasPerformed = false;
        return true;
    }

    int getSizeInUnits() override { return 10; }

private:
    MidiRegion& regionRef;
    int targetIndex;
    double splitPosition;

    NoteData originalNote;
    bool hasValidData = false;
    bool wasPerformed = false;
};

/**
 * HumanizeNotesAction - undoable action for adding random timing and velocity variation
 */
class HumanizeNotesAction : public juce::UndoableAction
{
public:
    /**
     * @param region The MIDI region containing notes to humanize
     * @param noteIndices Indices of note-on events to humanize
     * @param timingVariationTicks Maximum random timing shift in ticks (+/-)
     * @param velocityVariation Maximum random velocity shift (+/-)
     * @param seed Random seed for reproducibility
     */
    HumanizeNotesAction(MidiRegion& region, const std::vector<int>& noteIndices,
                         double timingVariationTicks, float velocityVariation,
                         unsigned int seed = 0)
        : regionRef(region),
          timingRange(timingVariationTicks),
          velocityRange(velocityVariation),
          randomSeed(seed == 0 ? static_cast<unsigned int>(std::time(nullptr)) : seed)
    {
        auto& seq = region.getMidiSequence();

        // Initialize random generator
        std::mt19937 rng(randomSeed);
        std::uniform_real_distribution<double> timingDist(-timingRange, timingRange);
        std::uniform_real_distribution<float> velocityDist(-velocityRange, velocityRange);

        for (int eventIndex : noteIndices)
        {
            if (eventIndex >= 0 && eventIndex < seq.getNumEvents())
            {
                auto* event = seq.getEventPointer(eventIndex);

                if (event != nullptr && event->message.isNoteOn())
                {
                    NoteHumanization human;
                    human.noteNumber = event->message.getNoteNumber();
                    human.channel = event->message.getChannel();
                    human.originalStartTicks = event->message.getTimeStamp();
                    human.originalVelocity = event->message.getFloatVelocity();

                    if (event->noteOffObject != nullptr)
                    {
                        human.originalEndTicks = event->noteOffObject->message.getTimeStamp();
                    }
                    else
                    {
                        human.originalEndTicks = human.originalStartTicks + 480.0;
                    }

                    // Calculate humanized values
                    human.timingOffset = timingDist(rng);
                    human.velocityOffset = velocityDist(rng);

                    // Apply limits
                    human.humanizedStartTicks = juce::jmax(0.0,
                        human.originalStartTicks + human.timingOffset);
                    human.humanizedEndTicks = human.humanizedStartTicks +
                        (human.originalEndTicks - human.originalStartTicks);
                    human.humanizedVelocity = juce::jlimit(0.01f, 1.0f,
                        human.originalVelocity + human.velocityOffset);

                    humanizations.push_back(human);
                }
            }
        }
    }

    bool perform() override
    {
        if (!wasPerformed && !humanizations.empty())
        {
            applyHumanization(true);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && !humanizations.empty())
        {
            applyHumanization(false);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override
    {
        return static_cast<int>(5 + humanizations.size() * 3);
    }

private:
    struct NoteHumanization
    {
        int noteNumber;
        int channel;
        double originalStartTicks;
        double originalEndTicks;
        float originalVelocity;
        double humanizedStartTicks;
        double humanizedEndTicks;
        float humanizedVelocity;
        double timingOffset;
        float velocityOffset;
    };

    void applyHumanization(bool forward)
    {
        auto& seq = regionRef.getMidiSequence();

        for (const auto& human : humanizations)
        {
            double searchStart = forward ? human.originalStartTicks : human.humanizedStartTicks;

            double targetStart = forward ? human.humanizedStartTicks : human.originalStartTicks;
            double targetEnd = forward ? human.humanizedEndTicks : human.originalEndTicks;
            float targetVel = forward ? human.humanizedVelocity : human.originalVelocity;

            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* event = seq.getEventPointer(i);
                if (event == nullptr)
                    continue;

                auto& msg = event->message;

                if (msg.isNoteOn() &&
                    msg.getNoteNumber() == human.noteNumber &&
                    msg.getChannel() == human.channel &&
                    std::abs(msg.getTimeStamp() - searchStart) < 2.0)
                {
                    msg.setTimeStamp(targetStart);
                    msg.setVelocity(targetVel);

                    if (event->noteOffObject != nullptr)
                    {
                        event->noteOffObject->message.setTimeStamp(targetEnd);
                    }
                    break;
                }
            }
        }

        seq.sort();
        seq.updateMatchedPairs();
    }

    MidiRegion& regionRef;
    double timingRange;
    float velocityRange;
    unsigned int randomSeed;
    std::vector<NoteHumanization> humanizations;
    bool wasPerformed = false;
};
