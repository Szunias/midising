#pragma once

#include "Track.h"
#include "../Utils/TimeConversion.h"
#include <juce_core/juce_core.h>
#include <memory>

// Forward declaration
class AuxTrack;

/**
 * Timeline is the main container for all tracks in the DAW.
 * Manages BPM, time signature, track ordering, and aux tracks for send/return routing.
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

    // Get the end sample position of the last region across all tracks
    int64_t getEndSample() const;

    //==========================================================================
    // Aux Track Management (Send/Return)
    //==========================================================================

    /**
     * Get the number of aux tracks.
     */
    int getNumAuxTracks() const { return auxTracks.size(); }

    /**
     * Get an aux track by index.
     * @param index Index of the aux track (0-based)
     * @return Pointer to the aux track, or nullptr if invalid index
     */
    AuxTrack* getAuxTrack(int index);
    const AuxTrack* getAuxTrack(int index) const;

    /**
     * Add a new aux track.
     * @param auxTrack The aux track to add (timeline takes ownership)
     */
    void addAuxTrack(AuxTrack* auxTrack);

    /**
     * Create and add a new aux track with the given name.
     * @param name Name for the new aux track
     * @return Pointer to the newly created aux track
     */
    AuxTrack* createAuxTrack(const juce::String& name = "Aux");

    /**
     * Remove an aux track by index.
     * @param index Index of the aux track to remove
     */
    void removeAuxTrack(int index);

    /**
     * Move an aux track from one position to another.
     */
    void moveAuxTrack(int fromIndex, int toIndex);

    /**
     * Clear all aux tracks.
     */
    void clearAuxTracks();

    /**
     * Prepare all aux tracks for playback.
     */
    void prepareAuxTracksToPlay(double sampleRate, int samplesPerBlock);

    /**
     * Release resources for all aux tracks.
     */
    void releaseAuxTrackResources();

    /**
     * Check if any aux track is soloed.
     */
    bool hasAnySoloedAuxTrack() const;

private:
    juce::OwnedArray<Track> tracks;
    juce::OwnedArray<AuxTrack> auxTracks;  // Aux/return tracks for send routing
    double bpm = 120.0;
    int beatsPerBar = 4;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Timeline)
};
