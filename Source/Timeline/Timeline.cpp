#include "Timeline.h"
#include "../Audio/AudioTrack.h"
#include "../Audio/SendReturn.h"
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
