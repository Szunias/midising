#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <map>
#include <vector>

/**
 * Manages sidechain audio bus routing between tracks.
 * Stores mappings of (trackIndex, effectIndex) -> sourceTrackIndex.
 * Provides pre-rendered audio buffers from source tracks for sidechain input.
 */
class SidechainRouter
{
public:
    SidechainRouter() = default;

    /**
     * Key for a sidechain routing entry.
     */
    struct RouteKey
    {
        int trackIndex;
        int effectIndex;

        bool operator<(const RouteKey& other) const
        {
            if (trackIndex != other.trackIndex) return trackIndex < other.trackIndex;
            return effectIndex < other.effectIndex;
        }

        bool operator==(const RouteKey& other) const
        {
            return trackIndex == other.trackIndex && effectIndex == other.effectIndex;
        }
    };

    /**
     * Set the sidechain source for a specific track's effect.
     * @param trackIdx The track containing the effect
     * @param effectIdx The effect slot index
     * @param sourceTrackIdx The source track index (-1 to clear)
     */
    void setSidechainSource(int trackIdx, int effectIdx, int sourceTrackIdx)
    {
        RouteKey key { trackIdx, effectIdx };
        if (sourceTrackIdx < 0)
            routes.erase(key);
        else
            routes[key] = sourceTrackIdx;
    }

    /**
     * Get the sidechain source track index for a specific effect.
     * @return Source track index, or -1 if no sidechain is routed
     */
    int getSidechainSource(int trackIdx, int effectIdx) const
    {
        RouteKey key { trackIdx, effectIdx };
        auto it = routes.find(key);
        if (it != routes.end())
            return it->second;
        return -1;
    }

    /**
     * Check if a track is used as a sidechain source by any route.
     */
    bool isTrackUsedAsSidechainSource(int trackIdx) const
    {
        for (const auto& route : routes)
        {
            if (route.second == trackIdx)
                return true;
        }
        return false;
    }

    /**
     * Store a pre-rendered sidechain buffer for a source track.
     * Called during the mixing loop after a track is processed.
     */
    void storeSidechainBuffer(int sourceTrackIdx, const juce::AudioBuffer<float>& buffer, int numSamples)
    {
        if (sourceTrackIdx < 0) return;

        auto idx = static_cast<size_t>(sourceTrackIdx);
        if (idx >= sidechainBuffers.size())
        {
            sidechainBuffers.resize(idx + 1);
        }

        auto& scBuffer = sidechainBuffers[idx];
        if (scBuffer.getNumChannels() < buffer.getNumChannels() ||
            scBuffer.getNumSamples() < numSamples)
        {
            scBuffer.setSize(buffer.getNumChannels(), numSamples, false, false, true);
        }

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            scBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        }
    }

    /**
     * Get the pre-rendered sidechain buffer for a source track.
     * @return Pointer to the buffer, or nullptr if not available
     */
    const juce::AudioBuffer<float>* getSidechainBuffer(int sourceTrackIdx) const
    {
        if (sourceTrackIdx < 0 || sourceTrackIdx >= static_cast<int>(sidechainBuffers.size()))
            return nullptr;
        return &sidechainBuffers[static_cast<size_t>(sourceTrackIdx)];
    }

    /**
     * Clear all sidechain buffers (call at start of each processing block).
     */
    void clearBuffers()
    {
        for (auto& buf : sidechainBuffers)
            buf.clear();
    }

    /**
     * Get all routes for serialization.
     */
    const std::map<RouteKey, int>& getRoutes() const { return routes; }

    /**
     * Clear all routes.
     */
    void clearRoutes() { routes.clear(); }

    /**
     * Prepare buffers for a given number of tracks.
     */
    void prepare(int numTracks, int numChannels, int samplesPerBlock)
    {
        sidechainBuffers.resize(static_cast<size_t>(numTracks));
        for (auto& buf : sidechainBuffers)
        {
            buf.setSize(numChannels, samplesPerBlock);
            buf.clear();
        }
    }

private:
    std::map<RouteKey, int> routes;
    std::vector<juce::AudioBuffer<float>> sidechainBuffers;
};
