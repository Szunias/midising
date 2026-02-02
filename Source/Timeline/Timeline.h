#pragma once

#include "Track.h"
#include "../Utils/TimeConversion.h"
#include <juce_core/juce_core.h>
#include <memory>

/**
 * Timeline is the main container for all tracks in the DAW.
 * Manages BPM, time signature, and track ordering.
 */
class Timeline
{
public:
    Timeline();
    ~Timeline() = default;

    // BPM management
    double getBpm() const { return bpm; }
    void setBpm(double newBpm) { bpm = juce::jlimit(20.0, 300.0, newBpm); }

    // Time signature
    int getBeatsPerBar() const { return beatsPerBar; }
    void setBeatsPerBar(int beats) { beatsPerBar = juce::jmax(1, beats); }

    // Sample rate (set by audio engine)
    double getSampleRate() const { return sampleRate; }
    void setSampleRate(double rate) { sampleRate = rate; }

    // Track management
    int getNumTracks() const { return tracks.size(); }
    Track* getTrack(int index) { return tracks[index]; }
    const Track* getTrack(int index) const { return tracks[index]; }
    
    void addTrack(Track* track) { tracks.add(track); }
    void removeTrack(int index) { tracks.remove(index); }
    void moveTrack(int fromIndex, int toIndex) { tracks.move(fromIndex, toIndex); }
    void clearTracks() { tracks.clear(); }

    // Time conversion helpers using current BPM and sample rate
    int64_t beatsToSamples(double beats) const
    {
        return TimeConversion::beatsToSamples(beats, bpm, sampleRate);
    }

    double samplesToBeats(int64_t samples) const
    {
        return TimeConversion::samplesToBeats(samples, bpm, sampleRate);
    }

    double beatsToSeconds(double beats) const
    {
        return TimeConversion::beatsToSeconds(beats, bpm);
    }

    // Check if any track is soloed
    bool hasAnySoloedTrack() const
    {
        for (auto* track : tracks)
            if (track->isSoloed())
                return true;
        return false;
    }

private:
    juce::OwnedArray<Track> tracks;
    double bpm = 120.0;
    int beatsPerBar = 4;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Timeline)
};
