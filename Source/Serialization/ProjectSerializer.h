#pragma once

#include <juce_core/juce_core.h>
#include <juce_core/juce_core.h>
#include "../Timeline/Timeline.h"
#include "../Timeline/Region.h"
#include "../Audio/Transport.h"
#include "../MIDI/MidiEngine.h"

/**
 * Handles saving and loading of the DAW project.
 * Uses XML format.
 */
class ProjectSerializer
{
public:
    static void saveProject(const Timeline& timeline, const Transport& transport, const juce::File& file);
    static void loadProject(Timeline& timeline, Transport& transport, MidiEngine* midiEngine, const juce::File& file);

private:
    static std::unique_ptr<juce::XmlElement> createTimelineXml(const Timeline& timeline, const Transport& transport);
    static std::unique_ptr<juce::XmlElement> createTrackXml(const Track& track);
    static std::unique_ptr<juce::XmlElement> createRegionXml(const Region& region);

    static void restoreTimelineFromXml(Timeline& timeline, const juce::XmlElement& xml, MidiEngine* midiEngine);
    static void restoreTrackFromXml(Timeline& timeline, const juce::XmlElement& xml, MidiEngine* midiEngine);
    static void restoreRegionFromXml(Track& track, const juce::XmlElement& xml);
};
