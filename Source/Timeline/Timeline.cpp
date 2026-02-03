#include "Timeline.h"
#include "../Audio/AudioTrack.h"
#include "../Audio/SendReturn.h"
#include "../Audio/GroupBus.h"
#include "../MIDI/MidiTrack.h"

Timeline::Timeline()
{
}

//==============================================================================
// Aux Track Management
//==============================================================================

AuxTrack* Timeline::getAuxTrack(int index)
{
    if (index >= 0 && index < auxTracks.size())
        return auxTracks[index];
    return nullptr;
}

const AuxTrack* Timeline::getAuxTrack(int index) const
{
    if (index >= 0 && index < auxTracks.size())
        return auxTracks[index];
    return nullptr;
}

void Timeline::addAuxTrack(AuxTrack* auxTrack)
{
    if (auxTrack != nullptr)
        auxTracks.add(auxTrack);
}

AuxTrack* Timeline::createAuxTrack(const juce::String& name)
{
    auto* auxTrack = new AuxTrack(name.isEmpty() ? "Aux " + juce::String(auxTracks.size() + 1) : name);
    auxTracks.add(auxTrack);
    return auxTrack;
}

void Timeline::removeAuxTrack(int index)
{
    if (index >= 0 && index < auxTracks.size())
        auxTracks.remove(index);
}

void Timeline::moveAuxTrack(int fromIndex, int toIndex)
{
    auxTracks.move(fromIndex, toIndex);
}

void Timeline::clearAuxTracks()
{
    auxTracks.clear();
}

void Timeline::prepareAuxTracksToPlay(double rate, int samplesPerBlock)
{
    for (auto* auxTrack : auxTracks)
    {
        auxTrack->prepareToPlay(rate, samplesPerBlock);
    }
}

void Timeline::releaseAuxTrackResources()
{
    for (auto* auxTrack : auxTracks)
    {
        auxTrack->releaseResources();
    }
}

bool Timeline::hasAnySoloedAuxTrack() const
{
    for (auto* auxTrack : auxTracks)
    {
        if (auxTrack->isSoloed())
            return true;
    }
    return false;
}

//==============================================================================
// Group Bus Management
//==============================================================================

GroupBus* Timeline::getGroupBus(int index)
{
    if (index >= 0 && index < groupBusses.size())
        return groupBusses[index];
    return nullptr;
}

const GroupBus* Timeline::getGroupBus(int index) const
{
    if (index >= 0 && index < groupBusses.size())
        return groupBusses[index];
    return nullptr;
}

void Timeline::addGroupBus(GroupBus* groupBus)
{
    if (groupBus != nullptr)
        groupBusses.add(groupBus);
}

GroupBus* Timeline::createGroupBus(const juce::String& name)
{
    auto* groupBus = new GroupBus(name.isEmpty() ? "Group " + juce::String(groupBusses.size() + 1) : name);
    groupBusses.add(groupBus);
    return groupBus;
}

void Timeline::removeGroupBus(int index)
{
    if (index >= 0 && index < groupBusses.size())
    {
        // Reset routing for any tracks that were routed to this group
        for (auto* track : tracks)
        {
            if (track->getOutputGroupBus() == index)
            {
                track->setOutputGroupBus(Track::OUTPUT_TO_MASTER);
            }
            else if (track->getOutputGroupBus() > index)
            {
                // Adjust indices for tracks routed to higher-numbered groups
                track->setOutputGroupBus(track->getOutputGroupBus() - 1);
            }
        }

        groupBusses.remove(index);
    }
}

void Timeline::moveGroupBus(int fromIndex, int toIndex)
{
    groupBusses.move(fromIndex, toIndex);
}

void Timeline::clearGroupBusses()
{
    // Reset all track routing to master
    for (auto* track : tracks)
    {
        track->setOutputGroupBus(Track::OUTPUT_TO_MASTER);
    }

    groupBusses.clear();
}

void Timeline::prepareGroupBussesToPlay(double rate, int samplesPerBlock)
{
    for (auto* groupBus : groupBusses)
    {
        groupBus->prepareToPlay(rate, samplesPerBlock);
    }
}

void Timeline::releaseGroupBusResources()
{
    for (auto* groupBus : groupBusses)
    {
        groupBus->releaseResources();
    }
}

bool Timeline::hasAnySoloedGroupBus() const
{
    for (auto* groupBus : groupBusses)
    {
        if (groupBus->isSoloed())
            return true;
    }
    return false;
}

int64_t Timeline::getEndSample() const
{
    int64_t endSample = 0;

    for (int i = 0; i < tracks.size(); ++i)
    {
        auto* track = tracks[i];

        if (track->getType() == TrackType::Audio)
        {
            auto* audioTrack = static_cast<const AudioTrack*>(track);
            for (int j = 0; j < audioTrack->getNumRegions(); ++j)
            {
                auto* region = audioTrack->getRegion(j);
                endSample = juce::jmax(endSample, region->getEndPosition());
            }
        }
        else if (track->getType() == TrackType::MIDI)
        {
            auto* midiTrack = static_cast<const MidiTrack*>(track);
            for (int j = 0; j < midiTrack->getNumRegions(); ++j)
            {
                auto* region = midiTrack->getRegion(j);
                endSample = juce::jmax(endSample, region->getEndPosition());
            }
        }
    }

    return endSample;
}
