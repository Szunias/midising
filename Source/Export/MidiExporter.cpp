#include "MidiExporter.h"

bool MidiExporter::exportToFile(const Timeline& timeline, const juce::File& file)
{
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(TICKS_PER_QUARTER_NOTE);

    // Add tempo track
    juce::MidiMessageSequence tempoTrack;
    double microsecondsPerBeat = 60000000.0 / timeline.getBpm();
    tempoTrack.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(microsecondsPerBeat)));
    midiFile.addTrack(tempoTrack);

    // Add each MIDI track
    for (int i = 0; i < timeline.getNumTracks(); ++i)
    {
        Track* track = const_cast<Timeline&>(timeline).getTrack(i);
        if (track == nullptr || track->getType() != TrackType::MIDI)
            continue;

        MidiTrack* midiTrack = dynamic_cast<MidiTrack*>(track);
        if (midiTrack == nullptr)
            continue;

        // Combine all regions in the track
        juce::MidiMessageSequence trackSequence;

        for (int r = 0; r < midiTrack->getNumRegions(); ++r)
        {
            MidiRegion* region = midiTrack->getRegion(r);
            if (region == nullptr)
                continue;

            const auto& seq = region->getMidiSequence();
            int64_t regionStartTicks = static_cast<int64_t>(
                timeline.samplesToBeats(region->getStartPosition()) * TICKS_PER_QUARTER_NOTE
            );

            for (int e = 0; e < seq.getNumEvents(); ++e)
            {
                auto* event = seq.getEventPointer(e);
                if (event != nullptr)
                {
                    juce::MidiMessage msg = event->message;
                    msg.setTimeStamp(msg.getTimeStamp() + regionStartTicks);
                    trackSequence.addEvent(msg);
                }
            }
        }

        trackSequence.updateMatchedPairs();
        midiFile.addTrack(trackSequence);
    }

    // Write to file
    juce::FileOutputStream outputStream(file);
    if (outputStream.failedToOpen())
        return false;

    return midiFile.writeTo(outputStream);
}

bool MidiExporter::exportTrackToFile(const MidiTrack& track, const juce::File& file, double bpm)
{
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(TICKS_PER_QUARTER_NOTE);

    // Add tempo track
    juce::MidiMessageSequence tempoTrack;
    double microsecondsPerBeat = 60000000.0 / bpm;
    tempoTrack.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(microsecondsPerBeat)));
    midiFile.addTrack(tempoTrack);

    // Add the track's MIDI sequence
    midiFile.addTrack(track.getMidiSequence());

    // Write to file
    juce::FileOutputStream outputStream(file);
    if (outputStream.failedToOpen())
        return false;

    return midiFile.writeTo(outputStream);
}
