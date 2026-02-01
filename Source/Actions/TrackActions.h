#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "../Timeline/Timeline.h"
#include "../Audio/AudioTrack.h"

/**
 * AddTrackAction - undoable action for adding a track
 */
class AddTrackAction : public juce::UndoableAction
{
public:
    AddTrackAction(Timeline& timeline, Track* track)
        : timelineRef(timeline), addedTrack(track)
    {
    }

    bool perform() override
    {
        if (addedTrack != nullptr && !wasPerformed)
        {
            timelineRef.addTrack(addedTrack);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed)
        {
            // Find and remove the track
            for (int i = 0; i < timelineRef.getNumTracks(); ++i)
            {
                if (timelineRef.getTrack(i) == addedTrack)
                {
                    timelineRef.removeTrack(i);
                    wasPerformed = false;
                    return true;
                }
            }
        }
        return false;
    }

    int getSizeInUnits() override { return 10; }

private:
    Timeline& timelineRef;
    Track* addedTrack;
    bool wasPerformed = false;
};

/**
 * DeleteTrackAction - undoable action for deleting a track
 */
class DeleteTrackAction : public juce::UndoableAction
{
public:
    DeleteTrackAction(Timeline& timeline, int trackIndex)
        : timelineRef(timeline), originalIndex(trackIndex)
    {
        if (trackIndex >= 0 && trackIndex < timeline.getNumTracks())
        {
            deletedTrack = timeline.getTrack(trackIndex);
        }
    }

    bool perform() override
    {
        if (deletedTrack != nullptr && originalIndex >= 0)
        {
            timelineRef.removeTrack(originalIndex);
            wasDeleted = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasDeleted && deletedTrack != nullptr)
        {
            // Re-add the track (note: this creates a new track, original is lost)
            timelineRef.addTrack(deletedTrack);
            wasDeleted = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 10; }

private:
    Timeline& timelineRef;
    Track* deletedTrack = nullptr;
    int originalIndex;
    bool wasDeleted = false;
};
