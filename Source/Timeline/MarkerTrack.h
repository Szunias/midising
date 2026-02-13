#pragma once

#include "Marker.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>

class MarkerTrack
{
public:
    MarkerTrack() = default;
    ~MarkerTrack() = default;

    void addMarker(const Marker& marker)
    {
        markers.push_back(marker);
        sortMarkers();
    }

    void removeMarker(int index)
    {
        if (index >= 0 && index < static_cast<int>(markers.size()))
            markers.erase(markers.begin() + index);
    }

    void moveMarker(int index, int64_t newPosition)
    {
        if (index >= 0 && index < static_cast<int>(markers.size()))
        {
            markers[static_cast<size_t>(index)].position = newPosition;
            sortMarkers();
        }
    }

    void renameMarker(int index, const juce::String& newName)
    {
        if (index >= 0 && index < static_cast<int>(markers.size()))
            markers[static_cast<size_t>(index)].name = newName;
    }

    void setMarkerColour(int index, juce::Colour colour)
    {
        if (index >= 0 && index < static_cast<int>(markers.size()))
            markers[static_cast<size_t>(index)].colour = colour;
    }

    void clearAllMarkers() { markers.clear(); }

    int getNumMarkers() const { return static_cast<int>(markers.size()); }

    const Marker& getMarker(int index) const { return markers[static_cast<size_t>(index)]; }
    Marker& getMarker(int index) { return markers[static_cast<size_t>(index)]; }

    int findMarkerAtPosition(int64_t position, int64_t tolerance = 4410) const
    {
        for (int i = 0; i < static_cast<int>(markers.size()); ++i)
        {
            if (std::abs(markers[static_cast<size_t>(i)].position - position) <= tolerance)
                return i;
        }
        return -1;
    }

    int findNextMarker(int64_t position) const
    {
        for (int i = 0; i < static_cast<int>(markers.size()); ++i)
        {
            if (markers[static_cast<size_t>(i)].position > position)
                return i;
        }
        return -1;
    }

    int findPrevMarker(int64_t position) const
    {
        for (int i = static_cast<int>(markers.size()) - 1; i >= 0; --i)
        {
            if (markers[static_cast<size_t>(i)].position < position)
                return i;
        }
        return -1;
    }

    // Get loop range from a marker and the next marker
    bool getLoopRangeFromMarker(int index, int64_t& loopStart, int64_t& loopEnd) const
    {
        if (index < 0 || index >= static_cast<int>(markers.size()))
            return false;

        loopStart = markers[static_cast<size_t>(index)].position;

        if (index + 1 < static_cast<int>(markers.size()))
            loopEnd = markers[static_cast<size_t>(index + 1)].position;
        else
            return false;

        return loopEnd > loopStart;
    }

    const std::vector<Marker>& getMarkers() const { return markers; }

private:
    std::vector<Marker> markers;

    void sortMarkers()
    {
        std::sort(markers.begin(), markers.end());
    }
};
