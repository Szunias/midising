#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Timeline/Region.h"
#include "../Audio/AudioTrack.h"
#include <memory>
#include <algorithm>

/**
 * RegionProcessor provides static utility methods for destructive audio
 * processing operations on AudioRegions.
 *
 * Operations include:
 * - Reverse: Reverse audio buffer in-place
 * - Normalize: Scale audio to a target peak level
 * - Consolidate: Merge multiple regions into a single new region
 *
 * These methods modify region audio data directly. For undoable versions,
 * use the corresponding actions in RegionProcessActions.h.
 */
class RegionProcessor
{
public:
    RegionProcessor() = delete; // Static-only class

    /**
     * Reverse the audio buffer of a region in-place.
     * Each channel is reversed independently using std::reverse.
     *
     * @param region The AudioRegion whose buffer to reverse
     */
    static void reverseRegion(AudioRegion& region);

    /**
     * Normalize the audio buffer of a region to a target peak level.
     * Finds the absolute peak across all channels, then applies uniform gain
     * so that the peak reaches the target level.
     *
     * @param region The AudioRegion whose buffer to normalize
     * @param targetPeak The desired peak level (default 1.0f = 0 dBFS)
     * @return The gain factor that was applied (useful for undo). Returns 1.0f if buffer is silent.
     */
    static float normalizeRegion(AudioRegion& region, float targetPeak = 1.0f);

    /**
     * Consolidate (bounce/merge) all regions within a time range into a single new AudioRegion.
     * Performs an offline render of all regions that overlap the given range,
     * mixing them down into a new stereo AudioRegion.
     *
     * @param track The AudioTrack containing the source regions
     * @param start Start position of the consolidation range (in samples)
     * @param end End position of the consolidation range (in samples)
     * @param sampleRate The sample rate for the output region
     * @return A new AudioRegion containing the consolidated audio, or nullptr if no regions overlap the range
     */
    static std::unique_ptr<AudioRegion> consolidateRegions(AudioTrack& track, int64_t start, int64_t end, double sampleRate);
};
