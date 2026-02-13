#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <algorithm>

struct TrackFolder
{
    juce::String name;
    juce::Colour colour { juce::Colours::grey };
    bool collapsed = false;
    std::vector<int> trackIndices;

    TrackFolder(const juce::String& folderName = "Folder", juce::Colour c = juce::Colours::grey)
        : name(folderName), colour(c)
    {
    }

    bool containsTrack(int trackIndex) const
    {
        return std::find(trackIndices.begin(), trackIndices.end(), trackIndex) != trackIndices.end();
    }

    void addTrack(int trackIndex)
    {
        if (!containsTrack(trackIndex))
            trackIndices.push_back(trackIndex);
    }

    void removeTrack(int trackIndex)
    {
        trackIndices.erase(
            std::remove(trackIndices.begin(), trackIndices.end(), trackIndex),
            trackIndices.end());
    }

    int getNumTracks() const { return static_cast<int>(trackIndices.size()); }
};
