#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "../Timeline/TimeSignatureTrack.h"

class AddTimeSignatureEventAction : public juce::UndoableAction
{
public:
    AddTimeSignatureEventAction(TimeSignatureTrack& track, const TimeSignatureEvent& event)
        : timeSigTrack(track), eventData(event)
    {
    }

    bool perform() override
    {
        timeSigTrack.addEvent(eventData);
        wasPerformed = true;
        return true;
    }

    bool undo() override
    {
        if (wasPerformed)
        {
            int idx = timeSigTrack.findEventAtPosition(eventData.position, 0);
            if (idx >= 0)
                timeSigTrack.removeEvent(static_cast<size_t>(idx));
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    TimeSignatureTrack& timeSigTrack;
    TimeSignatureEvent eventData;
    bool wasPerformed = false;
};

class RemoveTimeSignatureEventAction : public juce::UndoableAction
{
public:
    RemoveTimeSignatureEventAction(TimeSignatureTrack& track, size_t index)
        : timeSigTrack(track)
    {
        if (index < track.getNumEvents())
        {
            savedEvent = track.getEvent(index);
            hasValidData = true;
        }
    }

    bool perform() override
    {
        if (hasValidData)
        {
            int idx = timeSigTrack.findEventAtPosition(savedEvent.position, 0);
            if (idx >= 0)
            {
                timeSigTrack.removeEvent(static_cast<size_t>(idx));
                wasPerformed = true;
            }
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            timeSigTrack.addEvent(savedEvent);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    TimeSignatureTrack& timeSigTrack;
    TimeSignatureEvent savedEvent;
    bool hasValidData = false;
    bool wasPerformed = false;
};

class MoveTimeSignatureEventAction : public juce::UndoableAction
{
public:
    MoveTimeSignatureEventAction(TimeSignatureTrack& track, size_t index, int64_t newPosition)
        : timeSigTrack(track), newPos(newPosition)
    {
        if (index < track.getNumEvents())
        {
            oldPos = track.getEvent(index).position;
            numerator = track.getEvent(index).numerator;
            denominator = track.getEvent(index).denominator;
            hasValidData = true;
        }
    }

    bool perform() override
    {
        if (hasValidData)
        {
            int idx = timeSigTrack.findEventAtPosition(oldPos, 0);
            if (idx >= 0)
            {
                timeSigTrack.moveEvent(static_cast<size_t>(idx), newPos, numerator, denominator);
                wasPerformed = true;
            }
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            int idx = timeSigTrack.findEventAtPosition(newPos, 0);
            if (idx >= 0)
            {
                timeSigTrack.moveEvent(static_cast<size_t>(idx), oldPos, numerator, denominator);
                wasPerformed = false;
            }
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    TimeSignatureTrack& timeSigTrack;
    int64_t oldPos = 0;
    int64_t newPos = 0;
    int numerator = 4;
    int denominator = 4;
    bool hasValidData = false;
    bool wasPerformed = false;
};

class SetTimeSignatureAction : public juce::UndoableAction
{
public:
    SetTimeSignatureAction(TimeSignatureTrack& track, size_t index, int newNumerator, int newDenominator)
        : timeSigTrack(track), newNum(newNumerator), newDenom(newDenominator)
    {
        if (index < track.getNumEvents())
        {
            eventPosition = track.getEvent(index).position;
            oldNum = track.getEvent(index).numerator;
            oldDenom = track.getEvent(index).denominator;
            hasValidData = true;
        }
    }

    bool perform() override
    {
        if (hasValidData)
        {
            int idx = timeSigTrack.findEventAtPosition(eventPosition, 0);
            if (idx >= 0)
            {
                timeSigTrack.setEventTimeSignature(static_cast<size_t>(idx), newNum, newDenom);
                wasPerformed = true;
            }
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            int idx = timeSigTrack.findEventAtPosition(eventPosition, 0);
            if (idx >= 0)
            {
                timeSigTrack.setEventTimeSignature(static_cast<size_t>(idx), oldNum, oldDenom);
                wasPerformed = false;
            }
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    TimeSignatureTrack& timeSigTrack;
    int64_t eventPosition = 0;
    int oldNum = 4;
    int oldDenom = 4;
    int newNum = 4;
    int newDenom = 4;
    bool hasValidData = false;
    bool wasPerformed = false;
};
