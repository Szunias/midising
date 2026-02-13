#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Timeline/Region.h"
#include "../Audio/AudioTrack.h"
#include <vector>
#include <cmath>

/**
 * Type of crossfade curve used for transitions between overlapping regions.
 */
enum class CrossfadeCurveType
{
    Linear,       ///< Straight line fade (simple but may cause amplitude dip)
    EqualPower,   ///< Equal-power (cosine/sine) fade - maintains perceived loudness
    SCurve,       ///< Smooth S-shaped curve using hermite interpolation
    Logarithmic   ///< Logarithmic fade - slow start, fast end (or vice versa)
};

/**
 * Describes a crossfade overlap between two adjacent audio regions.
 * The outgoing region is fading out while the incoming region is fading in.
 */
struct CrossfadeInfo
{
    AudioRegion* outgoingRegion = nullptr;   ///< Region that is fading out (ends later)
    AudioRegion* incomingRegion = nullptr;   ///< Region that is fading in (starts earlier in overlap)
    int64_t overlapStart = 0;                ///< Sample position where overlap begins
    int64_t overlapEnd = 0;                  ///< Sample position where overlap ends

    /** Get the length of the overlap in samples. */
    int64_t getOverlapLength() const { return overlapEnd - overlapStart; }

    /** Check if this crossfade info is valid. */
    bool isValid() const
    {
        return outgoingRegion != nullptr
            && incomingRegion != nullptr
            && overlapEnd > overlapStart;
    }
};

/**
 * CrossfadeManager provides utilities for detecting region overlaps on an AudioTrack
 * and calculating crossfade gain values for different curve types.
 *
 * Usage:
 *   auto overlaps = CrossfadeManager::detectOverlaps(track);
 *   for (auto& info : overlaps) {
 *       float gain = CrossfadeManager::calculateFadeGain(t, curveType, true);
 *       // apply gain to outgoing region...
 *   }
 */
class CrossfadeManager
{
public:
    CrossfadeManager() = delete; // Static-only class

    /**
     * Detect all overlapping region pairs on an AudioTrack.
     * Regions overlap when one region's start falls before another region's end.
     * Results are sorted by overlap start position.
     *
     * @param track The AudioTrack to scan for overlapping regions
     * @return Vector of CrossfadeInfo structs describing each overlap
     */
    static std::vector<CrossfadeInfo> detectOverlaps(AudioTrack& track)
    {
        std::vector<CrossfadeInfo> overlaps;

        int numRegions = track.getNumRegions();
        if (numRegions < 2)
            return overlaps;

        // Compare all region pairs to find overlaps
        for (int i = 0; i < numRegions; ++i)
        {
            auto* regionA = track.getRegion(i);
            if (regionA == nullptr)
                continue;

            for (int j = i + 1; j < numRegions; ++j)
            {
                auto* regionB = track.getRegion(j);
                if (regionB == nullptr)
                    continue;

                // Determine which region starts first
                AudioRegion* first = regionA;
                AudioRegion* second = regionB;
                if (second->getStartPosition() < first->getStartPosition())
                    std::swap(first, second);

                // Check for overlap: first region's end extends past second region's start
                if (first->getEndPosition() > second->getStartPosition())
                {
                    CrossfadeInfo info;
                    info.outgoingRegion = first;
                    info.incomingRegion = second;
                    info.overlapStart = second->getStartPosition();
                    info.overlapEnd = juce::jmin(first->getEndPosition(), second->getEndPosition());

                    overlaps.push_back(info);
                }
            }
        }

        // Sort by overlap start position
        std::sort(overlaps.begin(), overlaps.end(),
                  [](const CrossfadeInfo& a, const CrossfadeInfo& b)
                  {
                      return a.overlapStart < b.overlapStart;
                  });

        return overlaps;
    }

    /**
     * Calculate the fade gain for a given normalized position and curve type.
     *
     * @param t Normalized position in the crossfade (0.0 to 1.0)
     * @param curveType The type of crossfade curve to use
     * @param isFadeOut If true, returns fade-out gain (1->0); if false, returns fade-in gain (0->1)
     * @return Gain value between 0.0 and 1.0
     */
    static float calculateFadeGain(float t, CrossfadeCurveType curveType, bool isFadeOut)
    {
        t = juce::jlimit(0.0f, 1.0f, t);

        float gain = 0.0f;

        switch (curveType)
        {
            case CrossfadeCurveType::Linear:
                gain = isFadeOut ? (1.0f - t) : t;
                break;

            case CrossfadeCurveType::EqualPower:
                // Equal-power crossfade using cosine/sine curves
                if (isFadeOut)
                    gain = std::cos(t * juce::MathConstants<float>::halfPi);
                else
                    gain = std::sin(t * juce::MathConstants<float>::halfPi);
                break;

            case CrossfadeCurveType::SCurve:
                // Hermite S-curve: 3t^2 - 2t^3 (smoothstep)
                {
                    float s = t * t * (3.0f - 2.0f * t);
                    gain = isFadeOut ? (1.0f - s) : s;
                }
                break;

            case CrossfadeCurveType::Logarithmic:
                // Attempt a logarithmic feel using power curve
                if (isFadeOut)
                {
                    // Slow initial decay, then rapid drop
                    gain = std::pow(1.0f - t, 0.5f);
                }
                else
                {
                    // Rapid initial rise, then slow approach to 1.0
                    gain = std::pow(t, 0.5f);
                }
                break;
        }

        return juce::jlimit(0.0f, 1.0f, gain);
    }
};
