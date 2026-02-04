#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "../Timeline/Region.h"
#include "../Audio/AudioTrack.h"

/**
 * AddRegionAction - undoable action for adding a region to a track
 */
class AddRegionAction : public juce::UndoableAction
{
public:
    AddRegionAction(AudioTrack& track, std::unique_ptr<AudioRegion> region)
        : trackRef(track), addedRegion(std::move(region))
    {
    }

    bool perform() override
    {
        if (addedRegion != nullptr && !wasPerformed)
        {
            regionPtr = addedRegion.get();
            trackRef.addRegion(std::move(addedRegion));
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && regionPtr != nullptr)
        {
            // Find and remove the region
            for (int i = 0; i < trackRef.getNumRegions(); ++i)
            {
                if (trackRef.getRegion(i) == regionPtr)
                {
                    trackRef.removeRegion(i);
                    wasPerformed = false;
                    return true;
                }
            }
        }
        return false;
    }

    int getSizeInUnits() override { return 10; }

private:
    AudioTrack& trackRef;
    std::unique_ptr<AudioRegion> addedRegion;  // Owned until perform()
    AudioRegion* regionPtr = nullptr;           // Raw pointer for undo lookup
    bool wasPerformed = false;
};

/**
 * DeleteRegionAction - undoable action for deleting a region from a track
 */
class DeleteRegionAction : public juce::UndoableAction
{
public:
    DeleteRegionAction(AudioTrack& track, int regionIndex)
        : trackRef(track), originalIndex(regionIndex)
    {
        if (regionIndex >= 0 && regionIndex < track.getNumRegions())
        {
            // Store the region's data for restoration
            AudioRegion* region = track.getRegion(regionIndex);
            if (region != nullptr)
            {
                savedStartPosition = region->getStartPosition();
                savedLength = region->getLength();
                savedOffset = region->getOffset();
                savedName = region->getName();
                savedFadeInLength = region->getFadeInLength();
                savedFadeOutLength = region->getFadeOutLength();
                savedFilePath = region->getFilePath();
                // Deep copy the audio buffer
                savedAudioBuffer = region->getAudioBuffer();
                hasValidData = true;
            }
        }
    }

    bool perform() override
    {
        if (hasValidData && originalIndex >= 0 && originalIndex < trackRef.getNumRegions())
        {
            trackRef.removeRegion(originalIndex);
            wasDeleted = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasDeleted && hasValidData)
        {
            // Recreate the region with saved data
            auto restoredRegion = std::make_unique<AudioRegion>(savedStartPosition, savedLength);
            restoredRegion->setOffset(savedOffset);
            restoredRegion->setName(savedName);
            restoredRegion->setFadeInLength(savedFadeInLength);
            restoredRegion->setFadeOutLength(savedFadeOutLength);
            restoredRegion->setFilePath(savedFilePath);
            restoredRegion->setAudioBuffer(savedAudioBuffer);

            trackRef.addRegion(std::move(restoredRegion));
            wasDeleted = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 10 + (savedAudioBuffer.getNumSamples() / 1000); }

private:
    AudioTrack& trackRef;
    int originalIndex;
    bool wasDeleted = false;
    bool hasValidData = false;

    // Saved region data for restoration
    int64_t savedStartPosition = 0;
    int64_t savedLength = 0;
    int64_t savedOffset = 0;
    juce::String savedName;
    int64_t savedFadeInLength = 0;
    int64_t savedFadeOutLength = 0;
    juce::String savedFilePath;
    juce::AudioBuffer<float> savedAudioBuffer;
};

/**
 * MoveRegionAction - undoable action for moving a region to a new position
 */
class MoveRegionAction : public juce::UndoableAction
{
public:
    MoveRegionAction(AudioRegion* region, int64_t newPosition)
        : regionRef(region), newStartPosition(newPosition)
    {
        if (region != nullptr)
        {
            oldStartPosition = region->getStartPosition();
        }
    }

    bool perform() override
    {
        if (regionRef != nullptr)
        {
            regionRef->setStartPosition(newStartPosition);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && regionRef != nullptr)
        {
            regionRef->setStartPosition(oldStartPosition);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    AudioRegion* regionRef;
    int64_t oldStartPosition = 0;
    int64_t newStartPosition = 0;
    bool wasPerformed = false;
};

/**
 * ResizeRegionAction - undoable action for resizing a region
 * Supports both length changes and offset changes (for trimming the start)
 */
class ResizeRegionAction : public juce::UndoableAction
{
public:
    ResizeRegionAction(AudioRegion* region, int64_t newLength, int64_t newOffset = -1)
        : regionRef(region), newRegionLength(newLength)
    {
        if (region != nullptr)
        {
            oldRegionLength = region->getLength();
            oldRegionOffset = region->getOffset();

            // If newOffset is -1, keep the existing offset
            newRegionOffset = (newOffset >= 0) ? newOffset : oldRegionOffset;
        }
    }

    bool perform() override
    {
        if (regionRef != nullptr)
        {
            regionRef->setLength(newRegionLength);
            regionRef->setOffset(newRegionOffset);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && regionRef != nullptr)
        {
            regionRef->setLength(oldRegionLength);
            regionRef->setOffset(oldRegionOffset);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    AudioRegion* regionRef;
    int64_t oldRegionLength = 0;
    int64_t newRegionLength = 0;
    int64_t oldRegionOffset = 0;
    int64_t newRegionOffset = 0;
    bool wasPerformed = false;
};

/**
 * SplitRegionAction - undoable action for splitting a region into two at a position
 * Creates a new region starting at the split point and trims the original
 */
class SplitRegionAction : public juce::UndoableAction
{
public:
    SplitRegionAction(AudioTrack& track, AudioRegion* region, int64_t splitPosition)
        : trackRef(track), originalRegion(region), splitPos(splitPosition)
    {
        if (region != nullptr && splitPosition > region->getStartPosition()
            && splitPosition < region->getEndPosition())
        {
            // Store original state
            originalStartPosition = region->getStartPosition();
            originalLength = region->getLength();
            originalOffset = region->getOffset();

            // Calculate new lengths
            newFirstLength = splitPosition - originalStartPosition;
            newSecondLength = originalLength - newFirstLength;
            newSecondOffset = originalOffset + newFirstLength;

            isValidSplit = true;
        }
    }

    bool perform() override
    {
        if (!isValidSplit || originalRegion == nullptr)
            return true;  // Nothing to do

        if (!wasPerformed)
        {
            // Resize the original region to end at the split point
            originalRegion->setLength(newFirstLength);

            // Create the second region starting at split point
            auto secondRegion = std::make_unique<AudioRegion>(splitPos, newSecondLength);
            secondRegion->setOffset(newSecondOffset);
            secondRegion->setName(originalRegion->getName() + " (split)");
            secondRegion->setAudioBuffer(originalRegion->getAudioBuffer());
            secondRegion->setFilePath(originalRegion->getFilePath());

            secondRegionPtr = secondRegion.get();
            trackRef.addRegion(std::move(secondRegion));

            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (!isValidSplit || !wasPerformed || originalRegion == nullptr)
            return false;

        // Find and remove the second region
        for (int i = 0; i < trackRef.getNumRegions(); ++i)
        {
            if (trackRef.getRegion(i) == secondRegionPtr)
            {
                trackRef.removeRegion(i);
                break;
            }
        }

        // Restore original region to its original size
        originalRegion->setLength(originalLength);
        originalRegion->setOffset(originalOffset);

        wasPerformed = false;
        secondRegionPtr = nullptr;
        return true;
    }

    int getSizeInUnits() override { return 15; }

private:
    AudioTrack& trackRef;
    AudioRegion* originalRegion;
    AudioRegion* secondRegionPtr = nullptr;
    int64_t splitPos;

    // Original state
    int64_t originalStartPosition = 0;
    int64_t originalLength = 0;
    int64_t originalOffset = 0;

    // New state
    int64_t newFirstLength = 0;
    int64_t newSecondLength = 0;
    int64_t newSecondOffset = 0;

    bool isValidSplit = false;
    bool wasPerformed = false;
};

/**
 * RenameRegionAction - undoable action for renaming a region
 */
class RenameRegionAction : public juce::UndoableAction
{
public:
    RenameRegionAction(Region* region, const juce::String& newName)
        : regionRef(region), newRegionName(newName)
    {
        if (region != nullptr)
        {
            oldRegionName = region->getName();
        }
    }

    bool perform() override
    {
        if (regionRef != nullptr)
        {
            regionRef->setName(newRegionName);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && regionRef != nullptr)
        {
            regionRef->setName(oldRegionName);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 2; }

private:
    Region* regionRef;
    juce::String oldRegionName;
    juce::String newRegionName;
    bool wasPerformed = false;
};

/**
 * SetRegionFadesAction - undoable action for changing region fade in/out lengths
 */
class SetRegionFadesAction : public juce::UndoableAction
{
public:
    SetRegionFadesAction(Region* region, int64_t newFadeIn, int64_t newFadeOut)
        : regionRef(region), newFadeInLength(newFadeIn), newFadeOutLength(newFadeOut)
    {
        if (region != nullptr)
        {
            oldFadeInLength = region->getFadeInLength();
            oldFadeOutLength = region->getFadeOutLength();
        }
    }

    bool perform() override
    {
        if (regionRef != nullptr)
        {
            regionRef->setFadeInLength(newFadeInLength);
            regionRef->setFadeOutLength(newFadeOutLength);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && regionRef != nullptr)
        {
            regionRef->setFadeInLength(oldFadeInLength);
            regionRef->setFadeOutLength(oldFadeOutLength);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 3; }

private:
    Region* regionRef;
    int64_t oldFadeInLength = 0;
    int64_t newFadeInLength = 0;
    int64_t oldFadeOutLength = 0;
    int64_t newFadeOutLength = 0;
    bool wasPerformed = false;
};
