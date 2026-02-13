#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "../Timeline/Region.h"
#include "../Audio/AudioTrack.h"
#include "../Audio/RegionProcessor.h"
#include <memory>
#include <vector>

/**
 * ReverseRegionAction - undoable action for reversing a region's audio.
 * Reversing is self-inverse: performing reverse again restores the original.
 */
class ReverseRegionAction : public juce::UndoableAction
{
public:
    ReverseRegionAction(AudioRegion* region)
        : regionRef(region)
    {
    }

    bool perform() override
    {
        if (regionRef != nullptr)
        {
            RegionProcessor::reverseRegion(*regionRef);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && regionRef != nullptr)
        {
            // Reverse is self-inverse: reversing again restores original order
            RegionProcessor::reverseRegion(*regionRef);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    AudioRegion* regionRef;
    bool wasPerformed = false;
};

/**
 * NormalizeRegionAction - undoable action for normalizing a region's audio level.
 * Stores the applied gain factor so the inverse can be applied on undo.
 */
class NormalizeRegionAction : public juce::UndoableAction
{
public:
    NormalizeRegionAction(AudioRegion* region, float targetPeak = 1.0f)
        : regionRef(region), targetPeakLevel(targetPeak)
    {
    }

    bool perform() override
    {
        if (regionRef != nullptr)
        {
            appliedGain = RegionProcessor::normalizeRegion(*regionRef, targetPeakLevel);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && regionRef != nullptr && appliedGain != 0.0f)
        {
            // Apply the inverse gain to restore original levels
            float inverseGain = 1.0f / appliedGain;
            auto& buffer = regionRef->getAudioBuffer();
            int numChannels = buffer.getNumChannels();
            int numSamples = buffer.getNumSamples();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                juce::FloatVectorOperations::multiply(
                    buffer.getWritePointer(ch), inverseGain, numSamples);
            }

            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

    /** Get the gain factor that was applied during normalization. */
    float getAppliedGain() const { return appliedGain; }

private:
    AudioRegion* regionRef;
    float targetPeakLevel;
    float appliedGain = 1.0f;
    bool wasPerformed = false;
};

/**
 * ConsolidateRegionsAction - undoable action for consolidating multiple regions
 * within a time range into a single new region.
 *
 * On perform: stores copies of the original regions, removes them from the track,
 * and adds the consolidated region.
 * On undo: removes the consolidated region and restores the originals.
 */
class ConsolidateRegionsAction : public juce::UndoableAction
{
public:
    ConsolidateRegionsAction(AudioTrack& track, int64_t start, int64_t end, double sampleRate)
        : trackRef(track), rangeStart(start), rangeEnd(end), consolidationSampleRate(sampleRate)
    {
    }

    bool perform() override
    {
        if (wasPerformed)
            return true;

        // First time: create the consolidated region and save originals
        if (consolidatedRegion == nullptr)
        {
            // Save copies of all regions that overlap the consolidation range
            savedRegions.clear();
            savedRegionIndices.clear();

            for (int i = 0; i < trackRef.getNumRegions(); ++i)
            {
                auto* region = trackRef.getRegion(i);
                if (region == nullptr)
                    continue;

                if (region->getStartPosition() < rangeEnd && region->getEndPosition() > rangeStart)
                {
                    // Save a copy of the region data
                    SavedRegionData saved;
                    saved.startPosition = region->getStartPosition();
                    saved.length = region->getLength();
                    saved.offset = region->getOffset();
                    saved.name = region->getName();
                    saved.fadeInLength = region->getFadeInLength();
                    saved.fadeOutLength = region->getFadeOutLength();
                    saved.filePath = region->getFilePath();
                    saved.audioBuffer = region->getAudioBuffer(); // Deep copy
                    saved.stretchRatio = region->getStretchRatio();
                    saved.pitchSemitones = region->getPitchSemitones();
                    saved.pitchCents = region->getPitchCents();
                    saved.formantPreserve = region->getFormantPreserve();

                    savedRegions.push_back(std::move(saved));
                }
            }

            // Create the consolidated region
            consolidatedRegion = RegionProcessor::consolidateRegions(
                trackRef, rangeStart, rangeEnd, consolidationSampleRate);

            if (consolidatedRegion == nullptr)
                return true; // No regions to consolidate
        }

        // Remove original overlapping regions (iterate in reverse to preserve indices)
        for (int i = trackRef.getNumRegions() - 1; i >= 0; --i)
        {
            auto* region = trackRef.getRegion(i);
            if (region == nullptr)
                continue;

            if (region->getStartPosition() < rangeEnd && region->getEndPosition() > rangeStart)
            {
                trackRef.removeRegion(i);
            }
        }

        // Add the consolidated region
        auto regionToAdd = std::make_unique<AudioRegion>(
            consolidatedRegion->getStartPosition(),
            consolidatedRegion->getLength());
        regionToAdd->setAudioBuffer(consolidatedRegion->getAudioBuffer());
        regionToAdd->setName(consolidatedRegion->getName());

        consolidatedRegionPtr = regionToAdd.get();
        trackRef.addRegion(std::move(regionToAdd));

        wasPerformed = true;
        return true;
    }

    bool undo() override
    {
        if (!wasPerformed)
            return false;

        // Remove the consolidated region
        for (int i = 0; i < trackRef.getNumRegions(); ++i)
        {
            if (trackRef.getRegion(i) == consolidatedRegionPtr)
            {
                trackRef.removeRegion(i);
                break;
            }
        }
        consolidatedRegionPtr = nullptr;

        // Restore the original regions
        for (auto& saved : savedRegions)
        {
            auto restored = std::make_unique<AudioRegion>(saved.startPosition, saved.length);
            restored->setOffset(saved.offset);
            restored->setName(saved.name);
            restored->setFadeInLength(saved.fadeInLength);
            restored->setFadeOutLength(saved.fadeOutLength);
            restored->setFilePath(saved.filePath);
            restored->setAudioBuffer(saved.audioBuffer);
            restored->setStretchRatio(saved.stretchRatio);
            restored->setPitchSemitones(saved.pitchSemitones);
            restored->setPitchCents(saved.pitchCents);
            restored->setFormantPreserve(saved.formantPreserve);

            trackRef.addRegion(std::move(restored));
        }

        wasPerformed = false;
        return true;
    }

    int getSizeInUnits() override
    {
        int size = 20;
        // Account for stored audio buffer sizes
        for (auto& saved : savedRegions)
            size += saved.audioBuffer.getNumSamples() / 1000;
        if (consolidatedRegion != nullptr)
            size += consolidatedRegion->getAudioBuffer().getNumSamples() / 1000;
        return size;
    }

private:
    /**
     * Stores all data needed to reconstruct an AudioRegion on undo.
     */
    struct SavedRegionData
    {
        int64_t startPosition = 0;
        int64_t length = 0;
        int64_t offset = 0;
        juce::String name;
        int64_t fadeInLength = 0;
        int64_t fadeOutLength = 0;
        juce::String filePath;
        juce::AudioBuffer<float> audioBuffer;
        float stretchRatio = 1.0f;
        int pitchSemitones = 0;
        int pitchCents = 0;
        bool formantPreserve = false;
    };

    AudioTrack& trackRef;
    int64_t rangeStart;
    int64_t rangeEnd;
    double consolidationSampleRate;

    std::vector<SavedRegionData> savedRegions;
    std::vector<int> savedRegionIndices;
    std::unique_ptr<AudioRegion> consolidatedRegion;
    AudioRegion* consolidatedRegionPtr = nullptr;

    bool wasPerformed = false;
};
