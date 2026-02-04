#pragma once

#include "Track.h"
#include "TempoTrack.h"
#include "../Utils/TimeConversion.h"
#include <juce_core/juce_core.h>
#include <memory>

// Forward declarations
class AuxTrack;
class GroupBus;
class MixGroup;

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
    void setBpm(double newBpm)
    {
        bpm = juce::jlimit(20.0, 300.0, newBpm);
        tempoTrack.setInitialBpm(bpm);
    }

    // Time signature
    int getBeatsPerBar() const { return beatsPerBar; }
    void setBeatsPerBar(int beats) { beatsPerBar = juce::jmax(1, beats); }

    // Sample rate (set by audio engine)
    double getSampleRate() const { return sampleRate; }
    void setSampleRate(double rate)
    {
        sampleRate = rate;
        tempoTrack.setSampleRate(rate);
    }

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

    //==========================================================================
    // Mix Group Management (Linked Faders)
    //==========================================================================

    /**
     * Get the number of mix groups.
     */
    int getNumMixGroups() const { return mixGroups.size(); }

    /**
     * Get a mix group by index.
     * @param index Index of the mix group (0-based)
     * @return Pointer to the mix group, or nullptr if invalid index
     */
    MixGroup* getMixGroup(int index);
    const MixGroup* getMixGroup(int index) const;

    /**
     * Add a new mix group.
     * @param mixGroup The mix group to add (timeline takes ownership)
     */
    void addMixGroup(MixGroup* mixGroup);

    /**
     * Create and add a new mix group with the given name.
     * @param name Name for the new mix group
     * @return Pointer to the newly created mix group
     */
    MixGroup* createMixGroup(const juce::String& name = "Mix Group");

    /**
     * Remove a mix group by index.
     * @param index Index of the mix group to remove
     */
    void removeMixGroup(int index);

    /**
     * Move a mix group from one position to another.
     */
    void moveMixGroup(int fromIndex, int toIndex);

    /**
     * Clear all mix groups.
     */
    void clearMixGroups();

    /**
     * Find which mix group a track belongs to.
     * @param trackIndex Index of the track
     * @return Pointer to the mix group, or nullptr if track is not in any group
     */
    MixGroup* findMixGroupForTrack(int trackIndex);
    const MixGroup* findMixGroupForTrack(int trackIndex) const;

    /**
     * Apply linked volume adjustment when a track's fader is moved.
     * This propagates the change to other tracks in the same mix group.
     *
     * @param trackIndex Index of the track that was adjusted
     * @param volumeDelta The change in volume
     */
    void applyLinkedVolumeChange(int trackIndex, float volumeDelta);

    /**
     * Apply linked pan adjustment when a track's pan is changed.
     * This propagates the change to other tracks in the same mix group.
     *
     * @param trackIndex Index of the track that was adjusted
     * @param panDelta The change in pan
     */
    void applyLinkedPanChange(int trackIndex, float panDelta);

    /**
     * Apply linked mute when a track's mute state is changed.
     * This propagates the change to other tracks in the same mix group.
     *
     * @param trackIndex Index of the track that was adjusted
     * @param shouldMute New mute state
     */
    void applyLinkedMute(int trackIndex, bool shouldMute);

    /**
     * Apply linked solo when a track's solo state is changed.
     * This propagates the change to other tracks in the same mix group.
     *
     * @param trackIndex Index of the track that was adjusted
     * @param shouldSolo New solo state
     */
    void applyLinkedSolo(int trackIndex, bool shouldSolo);

    //==========================================================================
    // Tempo Track Management
    //==========================================================================

    /**
     * Get the tempo track (const).
     * @return Reference to the tempo track
     */
    const TempoTrack& getTempoTrack() const { return tempoTrack; }

    /**
     * Get the tempo track (mutable).
     * @return Reference to the tempo track
     */
    TempoTrack& getTempoTrack() { return tempoTrack; }

    /**
     * Get the BPM at a specific sample position.
     * Uses the tempo track if it has tempo changes, otherwise returns the base BPM.
     * @param position Position in samples
     * @return BPM at the position
     */
    double getBpmAtPosition(int64_t position) const
    {
        if (tempoTrack.isEnabled() && tempoTrack.hasTempoChanges())
            return tempoTrack.getBpmAtPosition(position);
        return bpm;
    }

    /**
     * Convert beats to samples, accounting for tempo changes.
     * @param beats Beat position
     * @return Sample position
     */
    int64_t beatsToSamplesWithTempoTrack(double beats) const
    {
        if (tempoTrack.isEnabled() && tempoTrack.hasTempoChanges())
            return tempoTrack.beatsToSamples(beats);
        return beatsToSamples(beats);
    }

    /**
     * Convert samples to beats, accounting for tempo changes.
     * @param samples Sample position
     * @return Beat position
     */
    double samplesToBeatsWithTempoTrack(int64_t samples) const
    {
        if (tempoTrack.isEnabled() && tempoTrack.hasTempoChanges())
            return tempoTrack.samplesToBeats(samples);
        return samplesToBeats(samples);
    }

    //==========================================================================
    // Tempo Ramp Processing for Audio Buffers
    //==========================================================================

    /**
     * Get tempo values at the start and end of an audio buffer for smooth ramping.
     * Useful for tempo-dependent parameter transitions during audio processing.
     * @param startPosition Start position in samples
     * @param numSamples Size of the buffer in samples
     * @param startBpm Output: BPM at buffer start
     * @param endBpm Output: BPM at buffer end
     */
    void getTempoRampForBuffer(int64_t startPosition, int numSamples,
                               double& startBpm, double& endBpm) const;

    /**
     * Calculate the average tempo over a sample range.
     * Useful for operations that need a single tempo value over a buffer.
     * @param startPosition Start position in samples
     * @param numSamples Size of the range in samples
     * @return Average BPM over the range
     */
    double getAverageTempoForRange(int64_t startPosition, int numSamples) const;

    /**
     * Calculate the beat increment for a sample range with variable tempo.
     * This integrates tempo changes over the range for accurate beat tracking.
     * @param startPosition Start position in samples
     * @param numSamples Number of samples in the range
     * @return Number of beats elapsed over the range
     */
    double getBeatsInRange(int64_t startPosition, int numSamples) const;

    /**
     * Calculate samples per beat at a specific position, accounting for tempo ramps.
     * @param position Position in samples
     * @return Samples per beat at the position
     */
    double getSamplesPerBeatAtPosition(int64_t position) const;

    /**
     * Check if there are active tempo ramps in the project.
     * @return true if tempo track is enabled and has tempo changes
     */
    bool hasTempoRamps() const
    {
        return tempoTrack.isEnabled() && tempoTrack.hasTempoChanges();
    }

    /**
     * Get a pointer to the tempo track for use by Transport.
     * @return Pointer to the tempo track
     */
    const TempoTrack* getTempoTrackPtr() const { return &tempoTrack; }

private:
    juce::OwnedArray<Track> tracks;
    juce::OwnedArray<AuxTrack> auxTracks;    // Aux/return tracks for send routing
    juce::OwnedArray<GroupBus> groupBusses;  // Group busses for submixing
    juce::OwnedArray<MixGroup> mixGroups;    // Mix groups for linked faders
    TempoTrack tempoTrack;                   // Tempo track for tempo changes
    double bpm = 120.0;
    int beatsPerBar = 4;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Timeline)
};
