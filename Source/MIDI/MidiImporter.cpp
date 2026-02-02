#include "MidiImporter.h"

bool MidiImporter::isSupported(const juce::File& file) const
{
    auto ext = file.getFileExtension().toLowerCase();
    return ext == ".mid" || ext == ".midi";
}

int MidiImporter::getNumTracks(const juce::File& file)
{
    juce::FileInputStream stream(file);
    if (stream.openedOk())
    {
        if (midiFile.readFrom(stream))
        {
            return midiFile.getNumTracks();
        }
    }
    return 0;
}

juce::MidiMessageSequence MidiImporter::importTrack(const juce::File& file, int trackIndex, double sampleRate)
{
    juce::FileInputStream stream(file);
    if (stream.openedOk())
    {
        if (midiFile.readFrom(stream))
        {
            if (trackIndex >= 0 && trackIndex < midiFile.getNumTracks())
            {
                const auto* track = midiFile.getTrack(trackIndex);
                if (track != nullptr)
                {
                    // Convert timestamps from ticks to seconds (modifies the midiFile tracks in-place)
                    midiFile.convertTimestampTicksToSeconds();
                    
                    // Now get the track again as it might be modified (or just use index)
                    const auto* convertedTrack = midiFile.getTrack(trackIndex);
                    
                    // Copy messages to a new sequence
                    juce::MidiMessageSequence sequence;
                    sequence.addSequence(*convertedTrack, 0.0, 0.0, 100000.0);
                    
                    // Convert seconds to samples
                    for (int i = 0; i < sequence.getNumEvents(); ++i)
                    {
                        auto* event = sequence.getEventPointer(i);
                        double seconds = event->message.getTimeStamp();
                        event->message.setTimeStamp(seconds * sampleRate);
                    }
                    
                    sequence.updateMatchedPairs();
                    return sequence;
                }
            }
        }
    }
    
    return {};
}
