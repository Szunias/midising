#include "ProjectSerializer.h"
#include "../Audio/AudioTrack.h"
#include "../MIDI/MidiTrack.h"
#include "../Audio/AudioImporter.h"

void ProjectSerializer::saveProject(const Timeline& timeline, const juce::File& file)
{
    auto xml = createTimelineXml(timeline);
    xml->writeTo(file);
}

void ProjectSerializer::loadProject(Timeline& timeline, Transport& transport, MidiEngine* midiEngine, const juce::File& file)
{
    auto xml = juce::XmlDocument::parse(file);
    if (xml != nullptr && xml->hasTagName("PROJECT"))
    {
        transport.stop();
        timeline.clearTracks();
        
        // Load global settings
        double bpm = xml->getDoubleAttribute("bpm", 120.0);
        int beatsPerBar = xml->getIntAttribute("beatsPerBar", 4);
        
        timeline.setBpm(bpm);
        timeline.setBeatsPerBar(beatsPerBar);
        
        restoreTimelineFromXml(timeline, *xml, midiEngine);
    }
}

std::unique_ptr<juce::XmlElement> ProjectSerializer::createTimelineXml(const Timeline& timeline)
{
    auto xml = std::make_unique<juce::XmlElement>("PROJECT");
    xml->setAttribute("version", "1.0");
    xml->setAttribute("bpm", timeline.getBpm());
    xml->setAttribute("beatsPerBar", timeline.getBeatsPerBar());

    for (int i = 0; i < timeline.getNumTracks(); ++i)
    {
        xml->addChildElement(createTrackXml(*timeline.getTrack(i)).release());
    }

    return xml;
}

std::unique_ptr<juce::XmlElement> ProjectSerializer::createTrackXml(const Track& track)
{
    auto xml = std::make_unique<juce::XmlElement>("TRACK");
    xml->setAttribute("name", track.getName());
    xml->setAttribute("volume", track.getVolume());
    xml->setAttribute("pan", track.getPan());
    xml->setAttribute("muted", track.isMuted());
    xml->setAttribute("soloed", track.isSoloed());
    xml->setAttribute("colour", track.getColour().toString());

    if (track.getType() == TrackType::Audio)
        xml->setAttribute("type", "Audio");
    else if (track.getType() == TrackType::MIDI)
        xml->setAttribute("type", "MIDI");

    // Effects
    if (track.getType() == TrackType::Audio)
    {
        auto* audioTrack = dynamic_cast<const AudioTrack*>(&track);
        if (audioTrack)
        {
            auto chainXml = audioTrack->getEffectChain().createXml();
            if (chainXml)
                xml->addChildElement(chainXml.release());
            
            // Regions
            for (int i = 0; i < audioTrack->getNumRegions(); ++i)
            {
                xml->addChildElement(createRegionXml(*audioTrack->getRegion(i)).release());
            }
        }
    }
    else if (track.getType() == TrackType::MIDI)
    {
        auto* midiTrack = dynamic_cast<const MidiTrack*>(&track);
        if (midiTrack)
        {
            // Regions
            for (int i = 0; i < midiTrack->getNumRegions(); ++i)
            {
                xml->addChildElement(createRegionXml(*midiTrack->getRegion(i)).release());
            }
        }
    }

    return xml;
}

std::unique_ptr<juce::XmlElement> ProjectSerializer::createRegionXml(const Region& region)
{
    auto xml = std::make_unique<juce::XmlElement>("REGION");
    xml->setAttribute("name", region.getName());
    xml->setAttribute("start", static_cast<double>(region.getStartPosition()));
    xml->setAttribute("length", static_cast<double>(region.getLength()));
    xml->setAttribute("offset", static_cast<double>(region.getOffset()));

    if (auto* audioRegion = dynamic_cast<const AudioRegion*>(&region))
    {
        // Store source file path
        // We use absolute path for simplicity now. Relative paths would be better for portability.
        xml->setAttribute("file", audioRegion->getFilePath());
    }
    else if (auto* midiRegion = dynamic_cast<const MidiRegion*>(&region))
    {
        // Save MIDI events
        auto& seq = midiRegion->getMidiSequence();
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            auto msgXml = std::make_unique<juce::XmlElement>("EVENT");
            msgXml->setAttribute("time", event->message.getTimeStamp());
            msgXml->setAttribute("data", juce::String::toHexString(event->message.getRawData(), event->message.getRawDataSize()));
            xml->addChildElement(msgXml.release());
        }
    }
    
    return xml;
}

void ProjectSerializer::restoreTimelineFromXml(Timeline& timeline, const juce::XmlElement& xml, MidiEngine* midiEngine)
{
    for (auto* child : xml.getChildIterator())
    {
        if (child->hasTagName("TRACK"))
        {
            restoreTrackFromXml(timeline, *child, midiEngine);
        }
    }
}

void ProjectSerializer::restoreTrackFromXml(Timeline& timeline, const juce::XmlElement& xml, MidiEngine* midiEngine)
{
    juce::String type = xml.getStringAttribute("type");
    std::unique_ptr<Track> track;
    
    if (type == "Audio")
    {
        track = std::make_unique<AudioTrack>(xml.getStringAttribute("name"));
        // Restore effects
        if (auto* chainXml = xml.getChildByName("EFFECTCHAIN"))
        {
            static_cast<AudioTrack*>(track.get())->getEffectChain().loadFromXml(*chainXml);
        }
    }
    else if (type == "MIDI" && midiEngine != nullptr)
    {
        track = std::make_unique<MidiTrack>(xml.getStringAttribute("name"), midiEngine);
    }
    
    if (track != nullptr)
    {
        track->setVolume(xml.getDoubleAttribute("volume", 1.0));
        track->setPan(static_cast<float>(xml.getDoubleAttribute("pan", 0.0)));
        track->setMuted(xml.getBoolAttribute("muted", false));
        track->setSoloed(xml.getBoolAttribute("soloed", false));
        track->setColour(juce::Colour::fromString(xml.getStringAttribute("colour")));

        // Restore regions
        for (auto* child : xml.getChildIterator())
        {
            if (child->hasTagName("REGION"))
            {
                restoreRegionFromXml(*track, *child);
            }
        }
        
        timeline.addTrack(track.release());
    }
}

void ProjectSerializer::restoreRegionFromXml(Track& track, const juce::XmlElement& xml)
{
    int64_t start = static_cast<int64_t>(xml.getDoubleAttribute("start"));
    int64_t length = static_cast<int64_t>(xml.getDoubleAttribute("length"));
    int64_t offset = static_cast<int64_t>(xml.getDoubleAttribute("offset"));
    juce::String name = xml.getStringAttribute("name");
    
    if (auto* audioTrack = dynamic_cast<AudioTrack*>(&track))
    {
        juce::String filePath = xml.getStringAttribute("file");
        juce::File file(filePath);
        
        if (file.existsAsFile())
        {
            // We need a way to load without UI/TimelineView involvement ideally, 
            // or we use AudioImporter directly here.
            // But we don't have AudioImporter instance here.
            // Ideally ProjectSerializer should have access to an importer or we make one temporarily.
            // AudioImporter is just a helper around format manager.
            
            AudioImporter importer; // Cheap to create? It has format manager.
            // Format manager needs registration.
            // AudioImporter constructor registers basics.
            
            auto buffer = importer.loadFile(file, 44100.0); // Sample rate? We need timeline rate.
            // Timeline should have rate set.
            // But restoreTimelineFromXml passes timeline.
            
            if (buffer != nullptr)
            {
                auto region = std::make_unique<AudioRegion>(start, buffer->getNumSamples());
                region->setAudioBuffer(*buffer); // Length might differ if resampling? 
                // We should respect saved length ideally or trust the file.
                // If we resized/trimmed, start/length/offset handles it.
                // Buffer is always full source.
                
                region->setLength(length); // Restore trimmed length
                region->setOffset(offset);
                region->setName(name);
                region->setFilePath(filePath);
                
                audioTrack->addRegion(std::move(region));
            }
        }
    }
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(&track))
    {
        auto region = std::make_unique<MidiRegion>(start, length);
        region->setName(name);
        region->setOffset(offset);
        
        juce::MidiMessageSequence seq;
        for (auto* child : xml.getChildIterator())
        {
            if (child->hasTagName("EVENT"))
            {
                auto data = child->getStringAttribute("data");
                double time = child->getDoubleAttribute("time");
                
                juce::MemoryBlock mb;
                mb.loadFromHexString(data);
                juce::MidiMessage msg(mb.getData(), static_cast<int>(mb.getSize()), time);
                seq.addEvent(msg);
            }
        }
        seq.updateMatchedPairs();
        region->setMidiSequence(seq);
        midiTrack->addRegion(std::move(region));
    }
}
