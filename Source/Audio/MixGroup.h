#pragma once

#include "../Timeline/Track.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <atomic>

/**
 * MixGroup provides linked fader functionality for multiple tracks.
 *
 * A mix group (also called a VCA group or link group) allows multiple tracks
 * to have their faders linked together, so adjusting one fader moves all
 * linked faders proportionally. This is useful for:
 *   - Controlling multiple tracks as a single unit (e.g., drum submix)
 *   - Maintaining relative balance while adjusting overall level
 *   - Quick group mute/solo operations
 *
 * Link Modes:
 *   - Relative: Maintains relative differences between fader positions
 *   - Absolute: All faders move to the same position (not commonly used)
 *
 * Linkable Parameters:
 *   - Volume (always linked in a mix group)
 *   - Pan (optional)
 *   - Mute (optional)
 *   - Solo (optional)
 *
 * Usage:
 *   1. Create MixGroup with desired link options
 *   2. Add track indices to the group
 *   3. When a track's fader is moved, call adjustVolume() with the delta
 *   4. The group will apply the delta to all member tracks
 *
 * Thread Safety:
 *   - Uses std::atomic for parameters that may be accessed from audio thread
 *   - Track membership modifications should be done on message thread
 */
class MixGroup
{
public:
    /** Maximum number of tracks in a single mix group */
    static constexpr int MAX_TRACKS_PER_GROUP = 32;

    /**
     * Create a new mix group.
     * @param name Display name for the group
     */
    MixGroup(const juce::String& name = "Mix Group");
    ~MixGroup() = default;

    //==========================================================================
    // Group identification
    //==========================================================================

    /**
     * Get the group name.
     */
    juce::String getName() const { return name; }

    /**
     * Set the group name.
     */
    void setName(const juce::String& newName) { name = newName; }

    /**
     * Get the group colour for UI display.
     */
    juce::Colour getColour() const { return colour; }

    /**
     * Set the group colour for UI display.
     */
    void setColour(juce::Colour newColour) { colour = newColour; }

    //==========================================================================
    // Track membership
    //==========================================================================

    /**
     * Add a track to this mix group by index.
     * @param trackIndex Index of the track in the Timeline
     * @return True if added successfully, false if already in group or group is full
     */
    bool addTrack(int trackIndex);

    /**
     * Remove a track from this mix group.
     * @param trackIndex Index of the track to remove
     * @return True if removed, false if track wasn't in the group
     */
    bool removeTrack(int trackIndex);

    /**
     * Check if a track is in this mix group.
     * @param trackIndex Index of the track to check
     * @return True if the track is a member of this group
     */
    bool containsTrack(int trackIndex) const;

    /**
     * Get all track indices in this group.
     * @return Vector of track indices
     */
    const std::vector<int>& getTrackIndices() const { return trackIndices; }

    /**
     * Get the number of tracks in this group.
     */
    int getNumTracks() const { return static_cast<int>(trackIndices.size()); }

    /**
     * Clear all tracks from this group.
     */
    void clearTracks() { trackIndices.clear(); }

    /**
     * Called when a track is removed from the timeline.
     * Adjusts indices and removes the track if it was in the group.
     * @param removedIndex The index of the track that was removed
     */
    void onTrackRemoved(int removedIndex);

    /**
     * Called when tracks are reordered in the timeline.
     * @param fromIndex Original track index
     * @param toIndex New track index
     */
    void onTrackMoved(int fromIndex, int toIndex);

    //==========================================================================
    // Link options
    //==========================================================================

    /**
     * Check if pan is linked in this group.
     */
    bool isPanLinked() const { return linkPan.load(); }

    /**
     * Set whether pan should be linked.
     */
    void setLinkPan(bool shouldLink) { linkPan.store(shouldLink); }

    /**
     * Check if mute is linked in this group.
     */
    bool isMuteLinked() const { return linkMute.load(); }

    /**
     * Set whether mute should be linked.
     */
    void setLinkMute(bool shouldLink) { linkMute.store(shouldLink); }

    /**
     * Check if solo is linked in this group.
     */
    bool isSoloLinked() const { return linkSolo.load(); }

    /**
     * Set whether solo should be linked.
     */
    void setLinkSolo(bool shouldLink) { linkSolo.store(shouldLink); }

    //==========================================================================
    // Linked parameter adjustment
    //==========================================================================

    /**
     * Apply a relative volume change to all tracks in the group.
     * This is the main method for linked fader operation.
     *
     * @param tracks Array of track pointers (from Timeline)
     * @param numTracks Number of tracks in the array
     * @param volumeDelta The change to apply to all track volumes
     * @param sourceTrackIndex The track that initiated the change (for UI feedback)
     */
    void adjustVolume(Track* const* tracks, int numTracks, float volumeDelta, int sourceTrackIndex = -1);

    /**
     * Apply a relative pan change to all tracks in the group.
     * Only effective if linkPan is enabled.
     *
     * @param tracks Array of track pointers (from Timeline)
     * @param numTracks Number of tracks in the array
     * @param panDelta The change to apply to all track pans
     * @param sourceTrackIndex The track that initiated the change
     */
    void adjustPan(Track* const* tracks, int numTracks, float panDelta, int sourceTrackIndex = -1);

    /**
     * Set mute state for all tracks in the group.
     * Only effective if linkMute is enabled.
     *
     * @param tracks Array of track pointers (from Timeline)
     * @param numTracks Number of tracks in the array
     * @param shouldMute New mute state
     */
    void setMuteAll(Track* const* tracks, int numTracks, bool shouldMute);

    /**
     * Set solo state for all tracks in the group.
     * Only effective if linkSolo is enabled.
     *
     * @param tracks Array of track pointers (from Timeline)
     * @param numTracks Number of tracks in the array
     * @param shouldSolo New solo state
     */
    void setSoloAll(Track* const* tracks, int numTracks, bool shouldSolo);

    //==========================================================================
    // Group enable/disable
    //==========================================================================

    /**
     * Check if the group is enabled.
     * When disabled, faders operate independently.
     */
    bool isEnabled() const { return enabled.load(); }

    /**
     * Enable or disable the group.
     * @param shouldEnable True to enable linked operation
     */
    void setEnabled(bool shouldEnable) { enabled.store(shouldEnable); }

    //==========================================================================
    // Serialization
    //==========================================================================

    /**
     * Create XML representation of this mix group.
     */
    std::unique_ptr<juce::XmlElement> createXml() const;

    /**
     * Load mix group settings from XML.
     */
    void loadFromXml(const juce::XmlElement& xml);

private:
    juce::String name;
    juce::Colour colour { juce::Colours::orange };

    // Track membership (indices into Timeline's track array)
    std::vector<int> trackIndices;

    // Link options
    std::atomic<bool> linkPan { false };
    std::atomic<bool> linkMute { true };
    std::atomic<bool> linkSolo { true };

    // Group enabled state
    std::atomic<bool> enabled { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixGroup)
};

