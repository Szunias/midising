#include "Timeline.h"
#include "../Audio/AudioTrack.h"
#include "../Audio/SendReturn.h"
#include "../Audio/GroupBus.h"
#include "../Audio/MixGroup.h"
#include "../MIDI/MidiTrack.h"

Timeline::Timeline()
    : tempoTrack(120.0)  // Initialize tempo track with default BPM
{
    // Sync tempo track settings with timeline defaults
    tempoTrack.setInitialBpm(bpm);
    tempoTrack.setSampleRate(sampleRate);
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

//==============================================================================
// Mix Group Management (Linked Faders)
//==============================================================================

MixGroup* Timeline::getMixGroup(int index)
{
    if (index >= 0 && index < mixGroups.size())
        return mixGroups[index];
    return nullptr;
}

const MixGroup* Timeline::getMixGroup(int index) const
{
    if (index >= 0 && index < mixGroups.size())
        return mixGroups[index];
    return nullptr;
}

void Timeline::addMixGroup(MixGroup* mixGroup)
{
    if (mixGroup != nullptr)
        mixGroups.add(mixGroup);
}

MixGroup* Timeline::createMixGroup(const juce::String& name)
{
    auto* mixGroup = new MixGroup(name.isEmpty() ? "Mix Group " + juce::String(mixGroups.size() + 1) : name);
    mixGroups.add(mixGroup);
    return mixGroup;
}

void Timeline::removeMixGroup(int index)
{
    if (index >= 0 && index < mixGroups.size())
        mixGroups.remove(index);
}

void Timeline::moveMixGroup(int fromIndex, int toIndex)
{
    mixGroups.move(fromIndex, toIndex);
}

void Timeline::clearMixGroups()
{
    mixGroups.clear();
}

MixGroup* Timeline::findMixGroupForTrack(int trackIndex)
{
    for (auto* mixGroup : mixGroups)
    {
        if (mixGroup->containsTrack(trackIndex))
            return mixGroup;
    }
    return nullptr;
}

const MixGroup* Timeline::findMixGroupForTrack(int trackIndex) const
{
    for (auto* mixGroup : mixGroups)
    {
        if (mixGroup->containsTrack(trackIndex))
            return mixGroup;
    }
    return nullptr;
}

void Timeline::applyLinkedVolumeChange(int trackIndex, float volumeDelta)
{
    MixGroup* mixGroup = findMixGroupForTrack(trackIndex);
    if (mixGroup == nullptr || !mixGroup->isEnabled())
        return;

    // Get track array for the MixGroup to operate on
    std::vector<Track*> trackPtrs;
    trackPtrs.reserve(static_cast<size_t>(tracks.size()));
    for (auto* track : tracks)
        trackPtrs.push_back(track);

    mixGroup->adjustVolume(trackPtrs.data(), static_cast<int>(trackPtrs.size()), volumeDelta, trackIndex);
}

void Timeline::applyLinkedPanChange(int trackIndex, float panDelta)
{
    MixGroup* mixGroup = findMixGroupForTrack(trackIndex);
    if (mixGroup == nullptr || !mixGroup->isEnabled())
        return;

    std::vector<Track*> trackPtrs;
    trackPtrs.reserve(static_cast<size_t>(tracks.size()));
    for (auto* track : tracks)
        trackPtrs.push_back(track);

    mixGroup->adjustPan(trackPtrs.data(), static_cast<int>(trackPtrs.size()), panDelta, trackIndex);
}

void Timeline::applyLinkedMute(int trackIndex, bool shouldMute)
{
    MixGroup* mixGroup = findMixGroupForTrack(trackIndex);
    if (mixGroup == nullptr || !mixGroup->isEnabled())
        return;

    std::vector<Track*> trackPtrs;
    trackPtrs.reserve(static_cast<size_t>(tracks.size()));
    for (auto* track : tracks)
        trackPtrs.push_back(track);

    mixGroup->setMuteAll(trackPtrs.data(), static_cast<int>(trackPtrs.size()), shouldMute);
}

void Timeline::applyLinkedSolo(int trackIndex, bool shouldSolo)
{
    MixGroup* mixGroup = findMixGroupForTrack(trackIndex);
    if (mixGroup == nullptr || !mixGroup->isEnabled())
        return;

    std::vector<Track*> trackPtrs;
    trackPtrs.reserve(static_cast<size_t>(tracks.size()));
    for (auto* track : tracks)
        trackPtrs.push_back(track);

    mixGroup->setSoloAll(trackPtrs.data(), static_cast<int>(trackPtrs.size()), shouldSolo);
}
