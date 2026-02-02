#include "Timeline.h"
#include "../Audio/AudioTrack.h"
#include "../MIDI/MidiTrack.h"

Timeline::Timeline()
{
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
