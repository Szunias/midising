#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "../Timeline/MarkerTrack.h"

class AddMarkerAction : public juce::UndoableAction
{
public:
    AddMarkerAction(MarkerTrack& track, const Marker& marker)
        : markerTrack(track), markerData(marker)
    {
    }

    bool perform() override
    {
        markerTrack.addMarker(markerData);
        wasPerformed = true;
        return true;
    }

    bool undo() override
    {
        if (wasPerformed)
        {
            int idx = markerTrack.findMarkerAtPosition(markerData.position, 0);
            if (idx >= 0)
                markerTrack.removeMarker(idx);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    MarkerTrack& markerTrack;
    Marker markerData;
    bool wasPerformed = false;
};

class RemoveMarkerAction : public juce::UndoableAction
{
public:
    RemoveMarkerAction(MarkerTrack& track, int index)
        : markerTrack(track)
    {
        if (index >= 0 && index < track.getNumMarkers())
        {
            savedMarker = track.getMarker(index);
            hasValidData = true;
        }
    }

    bool perform() override
    {
        if (hasValidData)
        {
            int idx = markerTrack.findMarkerAtPosition(savedMarker.position, 0);
            if (idx >= 0)
            {
                markerTrack.removeMarker(idx);
                wasPerformed = true;
            }
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            markerTrack.addMarker(savedMarker);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    MarkerTrack& markerTrack;
    Marker savedMarker;
    bool hasValidData = false;
    bool wasPerformed = false;
};

class MoveMarkerAction : public juce::UndoableAction
{
public:
    MoveMarkerAction(MarkerTrack& track, int index, int64_t newPosition)
        : markerTrack(track), newPos(newPosition)
    {
        if (index >= 0 && index < track.getNumMarkers())
        {
            oldPos = track.getMarker(index).position;
            hasValidData = true;
        }
    }

    bool perform() override
    {
        if (hasValidData)
        {
            int idx = markerTrack.findMarkerAtPosition(oldPos, 0);
            if (idx >= 0)
            {
                markerTrack.moveMarker(idx, newPos);
                wasPerformed = true;
            }
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            int idx = markerTrack.findMarkerAtPosition(newPos, 0);
            if (idx >= 0)
            {
                markerTrack.moveMarker(idx, oldPos);
                wasPerformed = false;
            }
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    MarkerTrack& markerTrack;
    int64_t oldPos = 0;
    int64_t newPos = 0;
    bool hasValidData = false;
    bool wasPerformed = false;
};

class RenameMarkerAction : public juce::UndoableAction
{
public:
    RenameMarkerAction(MarkerTrack& track, int index, const juce::String& newName)
        : markerTrack(track), newMarkerName(newName)
    {
        if (index >= 0 && index < track.getNumMarkers())
        {
            markerPosition = track.getMarker(index).position;
            oldMarkerName = track.getMarker(index).name;
            hasValidData = true;
        }
    }

    bool perform() override
    {
        if (hasValidData)
        {
            int idx = markerTrack.findMarkerAtPosition(markerPosition, 0);
            if (idx >= 0)
            {
                markerTrack.renameMarker(idx, newMarkerName);
                wasPerformed = true;
            }
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            int idx = markerTrack.findMarkerAtPosition(markerPosition, 0);
            if (idx >= 0)
            {
                markerTrack.renameMarker(idx, oldMarkerName);
                wasPerformed = false;
            }
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    MarkerTrack& markerTrack;
    int64_t markerPosition = 0;
    juce::String oldMarkerName;
    juce::String newMarkerName;
    bool hasValidData = false;
    bool wasPerformed = false;
};
