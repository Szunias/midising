#pragma once

#include "Track.h"
#include "../Utils/TimeConversion.h"
#include <juce_core/juce_core.h>
#include <memory>

// Forward declarations
class AuxTrack;
class GroupBus;

/**
 * Timeline is the main container for all tracks in the DAW.
 * Manages BPM, time signature, track ordering, aux tracks for send/return routing,
 * and group busses for submixing.
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

    //==========================================================================
    // Group Bus Management (Submixing)
    //==========================================================================

    /**
     * Get the number of group busses.
     */
    int getNumGroupBusses() const { return groupBusses.size(); }

    /**
     * Get a group bus by index.
     * @param index Index of the group bus (0-based)
     * @return Pointer to the group bus, or nullptr if invalid index
     */
    GroupBus* getGroupBus(int index);
    const GroupBus* getGroupBus(int index) const;

    /**
     * Add a new group bus.
     * @param groupBus The group bus to add (timeline takes ownership)
     */
    void addGroupBus(GroupBus* groupBus);

    /**
     * Create and add a new group bus with the given name.
     * @param name Name for the new group bus
     * @return Pointer to the newly created group bus
     */
    GroupBus* createGroupBus(const juce::String& name = "Group");

    /**
     * Remove a group bus by index.
     * Note: Tracks routed to this group will have their routing reset to master.
     * @param index Index of the group bus to remove
     */
    void removeGroupBus(int index);

    /**
     * Move a group bus from one position to another.
     */
    void moveGroupBus(int fromIndex, int toIndex);

    /**
     * Clear all group busses.
     * Note: All track routing will be reset to master.
     */
    void clearGroupBusses();

    /**
     * Prepare all group busses for playback.
     */
    void prepareGroupBussesToPlay(double sampleRate, int samplesPerBlock);

    /**
     * Release resources for all group busses.
     */
    void releaseGroupBusResources();

    /**
     * Check if any group bus is soloed.
     */
    bool hasAnySoloedGroupBus() const;

private:
    juce::OwnedArray<Track> tracks;
    juce::OwnedArray<AuxTrack> auxTracks;    // Aux/return tracks for send routing
    juce::OwnedArray<GroupBus> groupBusses;  // Group busses for submixing
    double bpm = 120.0;
    int beatsPerBar = 4;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Timeline)
};
