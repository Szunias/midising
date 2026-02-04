#include "MixGroup.h"

MixGroup::MixGroup(const juce::String& groupName)
    : name(groupName)
{
}

//==============================================================================
// Track membership
//==============================================================================

bool MixGroup::addTrack(int trackIndex)
{
    if (trackIndex < 0)
        return false;

    // Check if already in group
    for (int idx : trackIndices)
    {
        if (idx == trackIndex)
            return false;
    }

    // Check if group is full
    if (static_cast<int>(trackIndices.size()) >= MAX_TRACKS_PER_GROUP)
        return false;

    trackIndices.push_back(trackIndex);
    return true;
}

bool MixGroup::removeTrack(int trackIndex)
{
    for (auto it = trackIndices.begin(); it != trackIndices.end(); ++it)
    {
        if (*it == trackIndex)
        {
            trackIndices.erase(it);
            return true;
        }
    }
    return false;
}

bool MixGroup::containsTrack(int trackIndex) const
{
    for (int idx : trackIndices)
    {
        if (idx == trackIndex)
            return true;
    }
    return false;
}

void MixGroup::onTrackRemoved(int removedIndex)
{
    // Remove the track if it was in the group
    removeTrack(removedIndex);

    // Adjust indices for tracks that were after the removed one
    for (int& idx : trackIndices)
    {
        if (idx > removedIndex)
            --idx;
    }
}

void MixGroup::onTrackMoved(int fromIndex, int toIndex)
{
    // Update indices when a track is moved
    for (int& idx : trackIndices)
    {
        if (idx == fromIndex)
        {
            // This track was moved
            idx = toIndex;
        }
        else if (fromIndex < toIndex)
        {
            // Moving down: indices between from+1 and to shift up
            if (idx > fromIndex && idx <= toIndex)
                --idx;
        }
        else
        {
            // Moving up: indices between to and from-1 shift down
            if (idx >= toIndex && idx < fromIndex)
                ++idx;
        }
    }
}

//==============================================================================
// Linked parameter adjustment
//==============================================================================

void MixGroup::adjustVolume(Track* const* tracks, int numTracks, float volumeDelta, int /*sourceTrackIndex*/)
{
    if (!enabled.load())
        return;

    for (int idx : trackIndices)
    {
        if (idx >= 0 && idx < numTracks && tracks[idx] != nullptr)
        {
            float currentVolume = tracks[idx]->getVolume();
            float newVolume = juce::jlimit(0.0f, 2.0f, currentVolume + volumeDelta);
            tracks[idx]->setVolume(newVolume);
        }
    }
}

void MixGroup::adjustPan(Track* const* tracks, int numTracks, float panDelta, int /*sourceTrackIndex*/)
{
    if (!enabled.load() || !linkPan.load())
        return;

    for (int idx : trackIndices)
    {
        if (idx >= 0 && idx < numTracks && tracks[idx] != nullptr)
        {
            float currentPan = tracks[idx]->getPan();
            float newPan = juce::jlimit(-1.0f, 1.0f, currentPan + panDelta);
            tracks[idx]->setPan(newPan);
        }
    }
}

void MixGroup::setMuteAll(Track* const* tracks, int numTracks, bool shouldMute)
{
    if (!enabled.load() || !linkMute.load())
        return;

    for (int idx : trackIndices)
    {
        if (idx >= 0 && idx < numTracks && tracks[idx] != nullptr)
        {
            tracks[idx]->setMuted(shouldMute);
        }
    }
}

void MixGroup::setSoloAll(Track* const* tracks, int numTracks, bool shouldSolo)
{
    if (!enabled.load() || !linkSolo.load())
        return;

    for (int idx : trackIndices)
    {
        if (idx >= 0 && idx < numTracks && tracks[idx] != nullptr)
        {
            tracks[idx]->setSoloed(shouldSolo);
        }
    }
}

//==============================================================================
// Serialization
//==============================================================================

std::unique_ptr<juce::XmlElement> MixGroup::createXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("MixGroup");

    xml->setAttribute("name", name);
    xml->setAttribute("colour", colour.toString());
    xml->setAttribute("enabled", enabled.load());
    xml->setAttribute("linkPan", linkPan.load());
    xml->setAttribute("linkMute", linkMute.load());
    xml->setAttribute("linkSolo", linkSolo.load());

    // Save track indices
    auto* tracksXml = xml->createNewChildElement("Tracks");
    for (int idx : trackIndices)
    {
        auto* trackXml = tracksXml->createNewChildElement("Track");
        trackXml->setAttribute("index", idx);
    }

    return xml;
}

void MixGroup::loadFromXml(const juce::XmlElement& xml)
{
    name = xml.getStringAttribute("name", "Mix Group");
    colour = juce::Colour::fromString(xml.getStringAttribute("colour", juce::Colours::orange.toString()));
    enabled.store(xml.getBoolAttribute("enabled", true));
    linkPan.store(xml.getBoolAttribute("linkPan", false));
    linkMute.store(xml.getBoolAttribute("linkMute", true));
    linkSolo.store(xml.getBoolAttribute("linkSolo", true));

    // Load track indices
    trackIndices.clear();
    if (auto* tracksXml = xml.getChildByName("Tracks"))
    {
        for (auto* trackXml : tracksXml->getChildIterator())
        {
            if (trackXml->hasTagName("Track"))
            {
                int idx = trackXml->getIntAttribute("index", -1);
                if (idx >= 0)
                    trackIndices.push_back(idx);
            }
        }
    }
}
