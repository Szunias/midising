#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Timeline/Region.h"

/**
 * PianoRollAddNoteAction - undoable action for adding a MIDI note
 */
class PianoRollAddNoteAction : public juce::UndoableAction
{
public:
    PianoRollAddNoteAction(MidiRegion& region, const juce::MidiMessage& noteOn,
                           const juce::MidiMessage& noteOff)
        : midiRegion(region), noteOnMsg(noteOn), noteOffMsg(noteOff)
    {
    }

    bool perform() override
    {
        auto& seq = midiRegion.getMidiSequence();
        seq.addEvent(noteOnMsg);
        seq.addEvent(noteOffMsg);
        seq.updateMatchedPairs();
        return true;
    }

    bool undo() override
    {
        auto& seq = midiRegion.getMidiSequence();
        // Find and remove the matching note events
        for (int i = seq.getNumEvents() - 1; i >= 0; --i)
        {
            auto* event = seq.getEventPointer(i);
            if (event != nullptr && event->message.isNoteOn()
                && event->message.getNoteNumber() == noteOnMsg.getNoteNumber()
                && std::abs(event->message.getTimeStamp() - noteOnMsg.getTimeStamp()) < 0.001)
            {
                // Remove the note-off first if matched
                if (event->noteOffObject != nullptr)
                {
                    for (int j = seq.getNumEvents() - 1; j >= 0; --j)
                    {
                        if (seq.getEventPointer(j) == event->noteOffObject)
                        {
                            seq.deleteEvent(j, false);
                            break;
                        }
                    }
                }
                seq.deleteEvent(i, false);
                seq.updateMatchedPairs();
                return true;
            }
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    MidiRegion& midiRegion;
    juce::MidiMessage noteOnMsg;
    juce::MidiMessage noteOffMsg;
};

/**
 * PianoRollDeleteNoteAction - undoable action for deleting a MIDI note
 */
class PianoRollDeleteNoteAction : public juce::UndoableAction
{
public:
    PianoRollDeleteNoteAction(MidiRegion& region, int eventIndex)
        : midiRegion(region)
    {
        auto& seq = region.getMidiSequence();
        auto* event = seq.getEventPointer(eventIndex);
        if (event != nullptr)
        {
            savedNoteOn = event->message;
            if (event->noteOffObject != nullptr)
                savedNoteOff = event->noteOffObject->message;
            else
                savedNoteOff = juce::MidiMessage::noteOff(
                    savedNoteOn.getChannel(), savedNoteOn.getNoteNumber());
        }
    }

    bool perform() override
    {
        auto& seq = midiRegion.getMidiSequence();
        for (int i = seq.getNumEvents() - 1; i >= 0; --i)
        {
            auto* event = seq.getEventPointer(i);
            if (event != nullptr && event->message.isNoteOn()
                && event->message.getNoteNumber() == savedNoteOn.getNoteNumber()
                && std::abs(event->message.getTimeStamp() - savedNoteOn.getTimeStamp()) < 0.001)
            {
                if (event->noteOffObject != nullptr)
                {
                    for (int j = seq.getNumEvents() - 1; j >= 0; --j)
                    {
                        if (seq.getEventPointer(j) == event->noteOffObject)
                        {
                            seq.deleteEvent(j, false);
                            break;
                        }
                    }
                }
                seq.deleteEvent(i, false);
                seq.updateMatchedPairs();
                return true;
            }
        }
        return false;
    }

    bool undo() override
    {
        auto& seq = midiRegion.getMidiSequence();
        seq.addEvent(savedNoteOn);
        seq.addEvent(savedNoteOff);
        seq.updateMatchedPairs();
        return true;
    }

    int getSizeInUnits() override { return 5; }

private:
    MidiRegion& midiRegion;
    juce::MidiMessage savedNoteOn;
    juce::MidiMessage savedNoteOff;
};

/**
 * PianoRollMoveNoteAction - undoable action for moving a note (pitch and/or time)
 */
class PianoRollMoveNoteAction : public juce::UndoableAction
{
public:
    PianoRollMoveNoteAction(MidiRegion& region, int eventIndex,
                            int newNoteNumber, double newTimestamp,
                            double newNoteOffTimestamp)
        : midiRegion(region), newNote(newNoteNumber),
          newTime(newTimestamp), newOffTime(newNoteOffTimestamp)
    {
        auto& seq = region.getMidiSequence();
        auto* event = seq.getEventPointer(eventIndex);
        if (event != nullptr)
        {
            oldNote = event->message.getNoteNumber();
            oldTime = event->message.getTimeStamp();
            oldVelocity = event->message.getFloatVelocity();
            oldChannel = event->message.getChannel();
            if (event->noteOffObject != nullptr)
                oldOffTime = event->noteOffObject->message.getTimeStamp();
            else
                oldOffTime = oldTime + 1.0;
        }
    }

    bool perform() override
    {
        return applyNoteChange(newNote, newTime, newOffTime);
    }

    bool undo() override
    {
        return applyNoteChange(oldNote, oldTime, oldOffTime);
    }

    int getSizeInUnits() override { return 5; }

private:
    bool applyNoteChange(int noteNum, double onTime, double offTime)
    {
        auto& seq = midiRegion.getMidiSequence();
        // Find the note
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event != nullptr && event->message.isNoteOn()
                && event->message.getNoteNumber() == oldNote
                && std::abs(event->message.getTimeStamp() - oldTime) < 0.001)
            {
                // Remove old events
                juce::MidiMessage offMsg;
                if (event->noteOffObject != nullptr)
                {
                    offMsg = event->noteOffObject->message;
                    for (int j = seq.getNumEvents() - 1; j >= 0; --j)
                    {
                        if (seq.getEventPointer(j) == event->noteOffObject)
                        {
                            seq.deleteEvent(j, false);
                            break;
                        }
                    }
                }
                seq.deleteEvent(i, false);

                // Add new events
                auto newOnMsg = juce::MidiMessage::noteOn(oldChannel, noteNum, oldVelocity);
                newOnMsg.setTimeStamp(onTime);
                auto newOffMsg = juce::MidiMessage::noteOff(oldChannel, noteNum);
                newOffMsg.setTimeStamp(offTime);

                seq.addEvent(newOnMsg);
                seq.addEvent(newOffMsg);
                seq.updateMatchedPairs();

                // Update oldNote/oldTime for the next undo/redo cycle
                oldNote = noteNum;
                oldTime = onTime;
                oldOffTime = offTime;
                return true;
            }
        }
        return false;
    }

    MidiRegion& midiRegion;
    int oldNote = 60;
    int newNote;
    double oldTime = 0.0;
    double newTime;
    double oldOffTime = 1.0;
    double newOffTime;
    float oldVelocity = 0.8f;
    int oldChannel = 1;
};

/**
 * PianoRollVelocityAction - undoable action for changing note velocity
 */
class PianoRollVelocityAction : public juce::UndoableAction
{
public:
    PianoRollVelocityAction(MidiRegion& region, int eventIndex, float newVelocity)
        : midiRegion(region), newVel(newVelocity)
    {
        auto& seq = region.getMidiSequence();
        auto* event = seq.getEventPointer(eventIndex);
        if (event != nullptr)
        {
            oldVel = event->message.getFloatVelocity();
            noteNumber = event->message.getNoteNumber();
            timestamp = event->message.getTimeStamp();
            channel = event->message.getChannel();
        }
    }

    bool perform() override { return setVelocity(newVel); }
    bool undo() override { return setVelocity(oldVel); }
    int getSizeInUnits() override { return 3; }

private:
    bool setVelocity(float vel)
    {
        auto& seq = midiRegion.getMidiSequence();
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event != nullptr && event->message.isNoteOn()
                && event->message.getNoteNumber() == noteNumber
                && std::abs(event->message.getTimeStamp() - timestamp) < 0.001)
            {
                // Replace with new velocity
                double offTime = timestamp + 1.0;
                if (event->noteOffObject)
                    offTime = event->noteOffObject->message.getTimeStamp();

                // Remove old
                if (event->noteOffObject)
                {
                    for (int j = seq.getNumEvents() - 1; j >= 0; --j)
                    {
                        if (seq.getEventPointer(j) == event->noteOffObject)
                        {
                            seq.deleteEvent(j, false);
                            break;
                        }
                    }
                }
                seq.deleteEvent(i, false);

                // Add new
                auto onMsg = juce::MidiMessage::noteOn(channel, noteNumber, vel);
                onMsg.setTimeStamp(timestamp);
                auto offMsg = juce::MidiMessage::noteOff(channel, noteNumber);
                offMsg.setTimeStamp(offTime);
                seq.addEvent(onMsg);
                seq.addEvent(offMsg);
                seq.updateMatchedPairs();
                return true;
            }
        }
        return false;
    }

    MidiRegion& midiRegion;
    float oldVel = 0.8f;
    float newVel;
    int noteNumber = 60;
    double timestamp = 0.0;
    int channel = 1;
};
