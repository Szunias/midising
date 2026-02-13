#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "../Timeline/TempoTrack.h"

class AddTempoEventAction : public juce::UndoableAction
{
public:
    AddTempoEventAction(TempoTrack& track, const TempoEvent& event)
        : tempoTrack(track), eventData(event)
    {
    }

    bool perform() override
    {
        tempoTrack.addEvent(eventData);
        wasPerformed = true;
        return true;
    }

    bool undo() override
    {
        if (wasPerformed)
        {
            int idx = tempoTrack.findEventAtPosition(eventData.position, 0);
            if (idx >= 0)
                tempoTrack.removeEvent(static_cast<size_t>(idx));
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    TempoTrack& tempoTrack;
    TempoEvent eventData;
    bool wasPerformed = false;
};

class RemoveTempoEventAction : public juce::UndoableAction
{
public:
    RemoveTempoEventAction(TempoTrack& track, size_t index)
        : tempoTrack(track)
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
            int idx = tempoTrack.findEventAtPosition(savedEvent.position, 0);
            if (idx >= 0)
            {
                tempoTrack.removeEvent(static_cast<size_t>(idx));
                wasPerformed = true;
            }
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            tempoTrack.addEvent(savedEvent);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    TempoTrack& tempoTrack;
    TempoEvent savedEvent;
    bool hasValidData = false;
    bool wasPerformed = false;
};

class MoveTempoEventAction : public juce::UndoableAction
{
public:
    MoveTempoEventAction(TempoTrack& track, size_t index, int64_t newPosition, double newBpm)
        : tempoTrack(track), newPos(newPosition), newBpmValue(newBpm)
    {
        if (index < track.getNumEvents())
        {
            oldPos = track.getEvent(index).position;
            oldBpmValue = track.getEvent(index).bpm;
            hasValidData = true;
        }
    }

    bool perform() override
    {
        if (hasValidData)
        {
            int idx = tempoTrack.findEventAtPosition(oldPos, 0);
            if (idx >= 0)
            {
                tempoTrack.moveEvent(static_cast<size_t>(idx), newPos, newBpmValue);
                wasPerformed = true;
            }
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            int idx = tempoTrack.findEventAtPosition(newPos, 0);
            if (idx >= 0)
            {
                tempoTrack.moveEvent(static_cast<size_t>(idx), oldPos, oldBpmValue);
                wasPerformed = false;
            }
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    TempoTrack& tempoTrack;
    int64_t oldPos = 0;
    int64_t newPos = 0;
    double oldBpmValue = 120.0;
    double newBpmValue = 120.0;
    bool hasValidData = false;
    bool wasPerformed = false;
};

class SetTempoRampTypeAction : public juce::UndoableAction
{
public:
    SetTempoRampTypeAction(TempoTrack& track, size_t index, TempoRampType newRampType)
        : tempoTrack(track), newRamp(newRampType)
    {
        if (index < track.getNumEvents())
        {
            eventPosition = track.getEvent(index).position;
            oldRamp = track.getEvent(index).rampType;
            hasValidData = true;
        }
    }

    bool perform() override
    {
        if (hasValidData)
        {
            int idx = tempoTrack.findEventAtPosition(eventPosition, 0);
            if (idx >= 0)
            {
                tempoTrack.setEventRampType(static_cast<size_t>(idx), newRamp);
                wasPerformed = true;
            }
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            int idx = tempoTrack.findEventAtPosition(eventPosition, 0);
            if (idx >= 0)
            {
                tempoTrack.setEventRampType(static_cast<size_t>(idx), oldRamp);
                wasPerformed = false;
            }
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    TempoTrack& tempoTrack;
    int64_t eventPosition = 0;
    TempoRampType oldRamp = TempoRampType::Instant;
    TempoRampType newRamp = TempoRampType::Instant;
    bool hasValidData = false;
    bool wasPerformed = false;
};
