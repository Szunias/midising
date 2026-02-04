#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "../Timeline/Track.h"
#include "../Timeline/Timeline.h"
#include "../Audio/Mixer.h"
#include "../Audio/SendReturn.h"
#include "../Audio/GroupBus.h"

/**
 * TrackVolumeAction - undoable action for changing track volume
 */
class TrackVolumeAction : public juce::UndoableAction
{
public:
    TrackVolumeAction(Track* track, float newVolume)
        : trackRef(track), newValue(newVolume)
    {
        if (track != nullptr)
        {
            oldValue = track->getVolume();
        }
    }

    bool perform() override
    {
        if (trackRef != nullptr)
        {
            trackRef->setVolume(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (trackRef != nullptr)
        {
            trackRef->setVolume(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    Track* trackRef;
    float oldValue = 1.0f;
    float newValue = 1.0f;
};

/**
 * TrackPanAction - undoable action for changing track pan
 */
class TrackPanAction : public juce::UndoableAction
{
public:
    TrackPanAction(Track* track, float newPan)
        : trackRef(track), newValue(newPan)
    {
        if (track != nullptr)
        {
            oldValue = track->getPan();
        }
    }

    bool perform() override
    {
        if (trackRef != nullptr)
        {
            trackRef->setPan(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (trackRef != nullptr)
        {
            trackRef->setPan(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    Track* trackRef;
    float oldValue = 0.0f;
    float newValue = 0.0f;
};

/**
 * TrackMuteAction - undoable action for toggling track mute
 */
class TrackMuteAction : public juce::UndoableAction
{
public:
    TrackMuteAction(Track* track, bool shouldMute)
        : trackRef(track), newValue(shouldMute)
    {
        if (track != nullptr)
        {
            oldValue = track->isMuted();
        }
    }

    bool perform() override
    {
        if (trackRef != nullptr)
        {
            trackRef->setMuted(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (trackRef != nullptr)
        {
            trackRef->setMuted(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    Track* trackRef;
    bool oldValue = false;
    bool newValue = false;
};

/**
 * TrackSoloAction - undoable action for toggling track solo
 */
class TrackSoloAction : public juce::UndoableAction
{
public:
    TrackSoloAction(Track* track, bool shouldSolo)
        : trackRef(track), newValue(shouldSolo)
    {
        if (track != nullptr)
        {
            oldValue = track->isSoloed();
        }
    }

    bool perform() override
    {
        if (trackRef != nullptr)
        {
            trackRef->setSoloed(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (trackRef != nullptr)
        {
            trackRef->setSoloed(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    Track* trackRef;
    bool oldValue = false;
    bool newValue = false;
};

/**
 * MasterVolumeAction - undoable action for changing master volume
 */
class MasterVolumeAction : public juce::UndoableAction
{
public:
    MasterVolumeAction(Mixer& mixer, float newVolume)
        : mixerRef(mixer), newValue(newVolume)
    {
        oldValue = mixer.getMasterVolume();
    }

    bool perform() override
    {
        mixerRef.setMasterVolume(newValue);
        return true;
    }

    bool undo() override
    {
        mixerRef.setMasterVolume(oldValue);
        return true;
    }

    int getSizeInUnits() override { return 5; }

private:
    Mixer& mixerRef;
    float oldValue = 1.0f;
    float newValue = 1.0f;
};

/**
 * SendLevelAction - undoable action for changing send level
 */
class SendLevelAction : public juce::UndoableAction
{
public:
    SendLevelAction(Track* track, int sendIndex, float newLevel)
        : trackRef(track), sendIdx(sendIndex), newValue(newLevel)
    {
        if (track != nullptr)
        {
            SendManager& sendMgr = track->getSendManager();
            Send* send = sendMgr.getSend(sendIndex);
            if (send != nullptr)
            {
                oldValue = send->sendLevel.load();
            }
        }
    }

    bool perform() override
    {
        if (trackRef != nullptr)
        {
            SendManager& sendMgr = trackRef->getSendManager();
            sendMgr.setSendLevel(sendIdx, newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (trackRef != nullptr)
        {
            SendManager& sendMgr = trackRef->getSendManager();
            sendMgr.setSendLevel(sendIdx, oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    Track* trackRef;
    int sendIdx;
    float oldValue = 0.0f;
    float newValue = 0.0f;
};

/**
 * SendEnabledAction - undoable action for enabling/disabling a send
 */
class SendEnabledAction : public juce::UndoableAction
{
public:
    SendEnabledAction(Track* track, int sendIndex, bool shouldEnable)
        : trackRef(track), sendIdx(sendIndex), newValue(shouldEnable)
    {
        if (track != nullptr)
        {
            SendManager& sendMgr = track->getSendManager();
            oldValue = sendMgr.isSendEnabled(sendIndex);
        }
    }

    bool perform() override
    {
        if (trackRef != nullptr)
        {
            SendManager& sendMgr = trackRef->getSendManager();
            sendMgr.setSendEnabled(sendIdx, newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (trackRef != nullptr)
        {
            SendManager& sendMgr = trackRef->getSendManager();
            sendMgr.setSendEnabled(sendIdx, oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    Track* trackRef;
    int sendIdx;
    bool oldValue = true;
    bool newValue = true;
};

/**
 * SendPreFaderAction - undoable action for toggling pre/post fader mode
 */
class SendPreFaderAction : public juce::UndoableAction
{
public:
    SendPreFaderAction(Track* track, int sendIndex, bool shouldBePreFader)
        : trackRef(track), sendIdx(sendIndex), newValue(shouldBePreFader)
    {
        if (track != nullptr)
        {
            SendManager& sendMgr = track->getSendManager();
            oldValue = sendMgr.isSendPreFader(sendIndex);
        }
    }

    bool perform() override
    {
        if (trackRef != nullptr)
        {
            SendManager& sendMgr = trackRef->getSendManager();
            sendMgr.setSendPreFader(sendIdx, newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (trackRef != nullptr)
        {
            SendManager& sendMgr = trackRef->getSendManager();
            sendMgr.setSendPreFader(sendIdx, oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    Track* trackRef;
    int sendIdx;
    bool oldValue = false;
    bool newValue = false;
};

/**
 * TrackOutputRoutingAction - undoable action for changing track output routing (to group bus)
 */
class TrackOutputRoutingAction : public juce::UndoableAction
{
public:
    TrackOutputRoutingAction(Track* track, int newGroupBusIndex)
        : trackRef(track), newValue(newGroupBusIndex)
    {
        if (track != nullptr)
        {
            oldValue = track->getOutputGroupBus();
        }
    }

    bool perform() override
    {
        if (trackRef != nullptr)
        {
            trackRef->setOutputGroupBus(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (trackRef != nullptr)
        {
            trackRef->setOutputGroupBus(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    Track* trackRef;
    int oldValue = Track::OUTPUT_TO_MASTER;
    int newValue = Track::OUTPUT_TO_MASTER;
};

/**
 * GroupBusVolumeAction - undoable action for changing group bus volume
 */
class GroupBusVolumeAction : public juce::UndoableAction
{
public:
    GroupBusVolumeAction(GroupBus* groupBus, float newVolume)
        : groupBusRef(groupBus), newValue(newVolume)
    {
        if (groupBus != nullptr)
        {
            oldValue = groupBus->getVolume();
        }
    }

    bool perform() override
    {
        if (groupBusRef != nullptr)
        {
            groupBusRef->setVolume(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (groupBusRef != nullptr)
        {
            groupBusRef->setVolume(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    GroupBus* groupBusRef;
    float oldValue = 1.0f;
    float newValue = 1.0f;
};

/**
 * GroupBusPanAction - undoable action for changing group bus pan
 */
class GroupBusPanAction : public juce::UndoableAction
{
public:
    GroupBusPanAction(GroupBus* groupBus, float newPan)
        : groupBusRef(groupBus), newValue(newPan)
    {
        if (groupBus != nullptr)
        {
            oldValue = groupBus->getPan();
        }
    }

    bool perform() override
    {
        if (groupBusRef != nullptr)
        {
            groupBusRef->setPan(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (groupBusRef != nullptr)
        {
            groupBusRef->setPan(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    GroupBus* groupBusRef;
    float oldValue = 0.0f;
    float newValue = 0.0f;
};

/**
 * GroupBusMuteAction - undoable action for toggling group bus mute
 */
class GroupBusMuteAction : public juce::UndoableAction
{
public:
    GroupBusMuteAction(GroupBus* groupBus, bool shouldMute)
        : groupBusRef(groupBus), newValue(shouldMute)
    {
        if (groupBus != nullptr)
        {
            oldValue = groupBus->isMuted();
        }
    }

    bool perform() override
    {
        if (groupBusRef != nullptr)
        {
            groupBusRef->setMuted(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (groupBusRef != nullptr)
        {
            groupBusRef->setMuted(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    GroupBus* groupBusRef;
    bool oldValue = false;
    bool newValue = false;
};

/**
 * GroupBusSoloAction - undoable action for toggling group bus solo
 */
class GroupBusSoloAction : public juce::UndoableAction
{
public:
    GroupBusSoloAction(GroupBus* groupBus, bool shouldSolo)
        : groupBusRef(groupBus), newValue(shouldSolo)
    {
        if (groupBus != nullptr)
        {
            oldValue = groupBus->isSoloed();
        }
    }

    bool perform() override
    {
        if (groupBusRef != nullptr)
        {
            groupBusRef->setSoloed(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (groupBusRef != nullptr)
        {
            groupBusRef->setSoloed(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    GroupBus* groupBusRef;
    bool oldValue = false;
    bool newValue = false;
};

/**
 * AuxTrackVolumeAction - undoable action for changing aux track volume
 */
class AuxTrackVolumeAction : public juce::UndoableAction
{
public:
    AuxTrackVolumeAction(AuxTrack* auxTrack, float newVolume)
        : auxTrackRef(auxTrack), newValue(newVolume)
    {
        if (auxTrack != nullptr)
        {
            oldValue = auxTrack->getVolume();
        }
    }

    bool perform() override
    {
        if (auxTrackRef != nullptr)
        {
            auxTrackRef->setVolume(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (auxTrackRef != nullptr)
        {
            auxTrackRef->setVolume(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    AuxTrack* auxTrackRef;
    float oldValue = 1.0f;
    float newValue = 1.0f;
};

/**
 * AuxTrackPanAction - undoable action for changing aux track pan
 */
class AuxTrackPanAction : public juce::UndoableAction
{
public:
    AuxTrackPanAction(AuxTrack* auxTrack, float newPan)
        : auxTrackRef(auxTrack), newValue(newPan)
    {
        if (auxTrack != nullptr)
        {
            oldValue = auxTrack->getPan();
        }
    }

    bool perform() override
    {
        if (auxTrackRef != nullptr)
        {
            auxTrackRef->setPan(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (auxTrackRef != nullptr)
        {
            auxTrackRef->setPan(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    AuxTrack* auxTrackRef;
    float oldValue = 0.0f;
    float newValue = 0.0f;
};

/**
 * AuxTrackMuteAction - undoable action for toggling aux track mute
 */
class AuxTrackMuteAction : public juce::UndoableAction
{
public:
    AuxTrackMuteAction(AuxTrack* auxTrack, bool shouldMute)
        : auxTrackRef(auxTrack), newValue(shouldMute)
    {
        if (auxTrack != nullptr)
        {
            oldValue = auxTrack->isMuted();
        }
    }

    bool perform() override
    {
        if (auxTrackRef != nullptr)
        {
            auxTrackRef->setMuted(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (auxTrackRef != nullptr)
        {
            auxTrackRef->setMuted(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    AuxTrack* auxTrackRef;
    bool oldValue = false;
    bool newValue = false;
};

/**
 * AuxTrackSoloAction - undoable action for toggling aux track solo
 */
class AuxTrackSoloAction : public juce::UndoableAction
{
public:
    AuxTrackSoloAction(AuxTrack* auxTrack, bool shouldSolo)
        : auxTrackRef(auxTrack), newValue(shouldSolo)
    {
        if (auxTrack != nullptr)
        {
            oldValue = auxTrack->isSoloed();
        }
    }

    bool perform() override
    {
        if (auxTrackRef != nullptr)
        {
            auxTrackRef->setSoloed(newValue);
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (auxTrackRef != nullptr)
        {
            auxTrackRef->setSoloed(oldValue);
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    AuxTrack* auxTrackRef;
    bool oldValue = false;
    bool newValue = false;
};
