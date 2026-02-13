#include "ProjectSerializer.h"
#include "../Audio/AudioTrack.h"
#include "../MIDI/MidiTrack.h"
#include "../Audio/AudioImporter.h"
#include "../Timeline/AutomationLane.h"
#include "../Timeline/MarkerTrack.h"

//==============================================================================
// Legacy single-file format (for backwards compatibility)
//==============================================================================

void ProjectSerializer::saveProject(const Timeline& timeline, const Transport& transport, const juce::File& file)
{
    auto xml = createTimelineXml(timeline, transport);
    xml->writeTo(file);
}

//==============================================================================
// Bundle format implementation
//==============================================================================

bool ProjectSerializer::isProjectBundle(const juce::File& path)
{
    // If it's a .msproj file, check if it's inside a bundle folder
    if (path.hasFileExtension(PROJECT_EXTENSION))
    {
        // Could be standalone .msproj or the file inside a bundle
        auto parent = path.getParentDirectory();
        auto audioFilesFolder = parent.getChildFile(AUDIO_FILES_FOLDER);
        return audioFilesFolder.exists() && audioFilesFolder.isDirectory();
    }

    // If it's a folder, check if it contains the project file
    if (path.isDirectory())
    {
        auto projectFile = path.getChildFile(PROJECT_FILE_NAME);
        return projectFile.existsAsFile();
    }

    return false;
}

juce::File ProjectSerializer::getAudioFilesFolder(const juce::File& projectFolder)
{
    return projectFolder.getChildFile(AUDIO_FILES_FOLDER);
}

bool ProjectSerializer::saveProjectBundle(const Timeline& timeline, const Transport& transport, const juce::File& projectFolder)
{
    // Create project folder if it doesn't exist
    if (!projectFolder.exists())
    {
        auto result = projectFolder.createDirectory();
        if (result.failed())
        {
            DBG("Failed to create project folder: " + result.getErrorMessage());
            return false;
        }
    }

    // Create Audio Files subfolder
    auto audioFilesFolder = getAudioFilesFolder(projectFolder);
    if (!audioFilesFolder.exists())
    {
        auto result = audioFilesFolder.createDirectory();
        if (result.failed())
        {
            DBG("Failed to create Audio Files folder: " + result.getErrorMessage());
            return false;
        }
    }

    // Create the XML with relative paths and copy audio files
    auto xml = createTimelineXmlForBundle(timeline, transport, projectFolder);

    // Write the project file
    auto projectFile = projectFolder.getChildFile(PROJECT_FILE_NAME);
    if (!xml->writeTo(projectFile))
    {
        DBG("Failed to write project file");
        return false;
    }

    DBG("Project bundle saved to: " + projectFolder.getFullPathName());
    return true;
}

bool ProjectSerializer::loadProjectBundle(Timeline& timeline, Transport& transport, MidiEngine* midiEngine, const juce::File& projectFolder)
{
    juce::File actualFolder = projectFolder;

    // If a .msproj file was passed, get the parent folder
    if (projectFolder.hasFileExtension(PROJECT_EXTENSION))
    {
        actualFolder = projectFolder.getParentDirectory();
    }

    // Get the project file
    auto projectFile = actualFolder.getChildFile(PROJECT_FILE_NAME);
    if (!projectFile.existsAsFile())
    {
        // Try loading as a standalone .msproj file (legacy format)
        if (projectFolder.existsAsFile())
        {
            loadProject(timeline, transport, midiEngine, projectFolder);
            return true;
        }
        DBG("Project file not found: " + projectFile.getFullPathName());
        return false;
    }

    // Parse the XML
    auto xml = juce::XmlDocument::parse(projectFile);
    if (xml == nullptr || !xml->hasTagName("PROJECT"))
    {
        DBG("Invalid project file format");
        return false;
    }

    transport.stop();
    timeline.clearTracks();

    // Load global settings
    double bpm = xml->getDoubleAttribute("bpm", 120.0);
    int beatsPerBar = xml->getIntAttribute("beatsPerBar", 4);

    timeline.setBpm(bpm);
    timeline.setBeatsPerBar(beatsPerBar);

    // Load transport settings
    int64_t playheadPosition = static_cast<int64_t>(xml->getDoubleAttribute("playheadPosition", 0.0));
    bool looping = xml->getBoolAttribute("looping", false);
    int64_t loopStart = static_cast<int64_t>(xml->getDoubleAttribute("loopStart", 0.0));
    int64_t loopEnd = static_cast<int64_t>(xml->getDoubleAttribute("loopEnd", 0.0));

    transport.setPlayheadPosition(playheadPosition);
    transport.setLooping(looping);
    transport.setLoopRange(loopStart, loopEnd);

    // Load punch state
    transport.setPunchInEnabled(xml->getBoolAttribute("punchInEnabled", false));
    transport.setPunchOutEnabled(xml->getBoolAttribute("punchOutEnabled", false));
    transport.setPunchRange(
        static_cast<int64_t>(xml->getDoubleAttribute("punchInPosition", 0.0)),
        static_cast<int64_t>(xml->getDoubleAttribute("punchOutPosition", 0.0)));
    transport.setPreRollBeats(xml->getIntAttribute("preRollBeats", 0));

    // Check if this is a bundle format (version 2.0+) or legacy
    juce::String version = xml->getStringAttribute("version", "1.0");
    bool isBundleFormat = xml->getBoolAttribute("bundleFormat", false);

    if (isBundleFormat)
    {
        restoreTimelineFromXmlBundle(timeline, *xml, midiEngine, actualFolder);
    }
    else
    {
        // Legacy format - try to resolve paths anyway
        restoreTimelineFromXmlBundle(timeline, *xml, midiEngine, actualFolder);
    }

    DBG("Project bundle loaded from: " + actualFolder.getFullPathName());
    return true;
}

//==============================================================================
// Bundle XML creation
//==============================================================================

std::unique_ptr<juce::XmlElement> ProjectSerializer::createTimelineXmlForBundle(const Timeline& timeline, const Transport& transport, const juce::File& projectFolder)
{
    auto xml = std::make_unique<juce::XmlElement>("PROJECT");
    xml->setAttribute("version", "2.0");
    xml->setAttribute("bundleFormat", true);
    xml->setAttribute("bpm", timeline.getBpm());
    xml->setAttribute("beatsPerBar", timeline.getBeatsPerBar());

    // Save transport settings
    xml->setAttribute("playheadPosition", static_cast<double>(transport.getPlayheadPosition()));
    xml->setAttribute("looping", transport.isLooping());
    xml->setAttribute("loopStart", static_cast<double>(transport.getLoopStart()));
    xml->setAttribute("loopEnd", static_cast<double>(transport.getLoopEnd()));
    xml->setAttribute("punchInEnabled", transport.isPunchInEnabled());
    xml->setAttribute("punchOutEnabled", transport.isPunchOutEnabled());
    xml->setAttribute("punchInPosition", static_cast<double>(transport.getPunchInPosition()));
    xml->setAttribute("punchOutPosition", static_cast<double>(transport.getPunchOutPosition()));
    xml->setAttribute("preRollBeats", transport.getPreRollBeats());

    for (int i = 0; i < timeline.getNumTracks(); ++i)
    {
        xml->addChildElement(createTrackXmlForBundle(*timeline.getTrack(i), projectFolder).release());
    }

    // Save tempo track events
    const auto& tempoTrackB = timeline.getTempoTrack();
    if (tempoTrackB.getNumEvents() > 0)
    {
        auto tempoXml = std::make_unique<juce::XmlElement>("TEMPO_TRACK");
        tempoXml->setAttribute("initialBpm", tempoTrackB.getInitialBpm());
        tempoXml->setAttribute("enabled", tempoTrackB.isEnabled());
        for (size_t ti = 0; ti < tempoTrackB.getNumEvents(); ++ti)
        {
            const auto& e = tempoTrackB.getEvent(ti);
            auto eXml = std::make_unique<juce::XmlElement>("TEMPO_EVENT");
            eXml->setAttribute("position", static_cast<double>(e.position));
            eXml->setAttribute("bpm", e.bpm);
            eXml->setAttribute("rampType", static_cast<int>(e.rampType));
            tempoXml->addChildElement(eXml.release());
        }
        xml->addChildElement(tempoXml.release());
    }

    // Save time signature track events
    const auto& timeSigTrackB = timeline.getTimeSignatureTrack();
    if (timeSigTrackB.getNumEvents() > 0)
    {
        auto tsXml = std::make_unique<juce::XmlElement>("TIME_SIG_TRACK");
        tsXml->setAttribute("initialNumerator", timeSigTrackB.getInitialNumerator());
        tsXml->setAttribute("initialDenominator", timeSigTrackB.getInitialDenominator());
        tsXml->setAttribute("enabled", timeSigTrackB.isEnabled());
        for (size_t ti = 0; ti < timeSigTrackB.getNumEvents(); ++ti)
        {
            const auto& e = timeSigTrackB.getEvent(ti);
            auto eXml = std::make_unique<juce::XmlElement>("TIME_SIG_EVENT");
            eXml->setAttribute("position", static_cast<double>(e.position));
            eXml->setAttribute("numerator", e.numerator);
            eXml->setAttribute("denominator", e.denominator);
            tsXml->addChildElement(eXml.release());
        }
        xml->addChildElement(tsXml.release());
    }

    // Save track folders
    const auto& folders = timeline.getTrackFolders();
    if (!folders.empty())
    {
        auto foldersXml = std::make_unique<juce::XmlElement>("TRACK_FOLDERS");
        for (const auto& folder : folders)
        {
            auto folderXml = std::make_unique<juce::XmlElement>("FOLDER");
            folderXml->setAttribute("name", folder.name);
            folderXml->setAttribute("colour", folder.colour.toString());
            folderXml->setAttribute("collapsed", folder.collapsed);
            for (int idx : folder.trackIndices)
            {
                auto refXml = std::make_unique<juce::XmlElement>("TRACK_REF");
                refXml->setAttribute("index", idx);
                folderXml->addChildElement(refXml.release());
            }
            foldersXml->addChildElement(folderXml.release());
        }
        xml->addChildElement(foldersXml.release());
    }

    // Save markers
    const auto& markerTrack = timeline.getMarkerTrack();
    if (markerTrack.getNumMarkers() > 0)
    {
        auto markersXml = std::make_unique<juce::XmlElement>("MARKERS");
        for (int i = 0; i < markerTrack.getNumMarkers(); ++i)
        {
            const auto& m = markerTrack.getMarker(i);
            auto mXml = std::make_unique<juce::XmlElement>("MARKER");
            mXml->setAttribute("name", m.name);
            mXml->setAttribute("position", static_cast<double>(m.position));
            mXml->setAttribute("colour", m.colour.toString());
            mXml->setAttribute("type", static_cast<int>(m.type));
            markersXml->addChildElement(mXml.release());
        }
        xml->addChildElement(markersXml.release());
    }

    return xml;
}

std::unique_ptr<juce::XmlElement> ProjectSerializer::createTrackXmlForBundle(const Track& track, const juce::File& projectFolder)
{
    auto xml = std::make_unique<juce::XmlElement>("TRACK");
    xml->setAttribute("name", track.getName());
    xml->setAttribute("volume", track.getVolume());
    xml->setAttribute("pan", track.getPan());
    xml->setAttribute("muted", track.isMuted());
    xml->setAttribute("soloed", track.isSoloed());
    xml->setAttribute("armed", track.isArmed());
    xml->setAttribute("colour", track.getColour().toString());

    if (track.getType() == TrackType::Audio)
        xml->setAttribute("type", "Audio");
    else if (track.getType() == TrackType::MIDI)
        xml->setAttribute("type", "MIDI");

    // Save automation lanes
    auto automationXml = createAutomationLanesXml(track);
    if (automationXml != nullptr)
        xml->addChildElement(automationXml.release());

    // Effects
    if (track.getType() == TrackType::Audio)
    {
        auto* audioTrack = dynamic_cast<const AudioTrack*>(&track);
        if (audioTrack)
        {
            auto chainXml = audioTrack->getEffectChain().createXml();
            if (chainXml)
                xml->addChildElement(chainXml.release());

            // Regions with relative paths
            for (int i = 0; i < audioTrack->getNumRegions(); ++i)
            {
                xml->addChildElement(createRegionXmlForBundle(*audioTrack->getRegion(i), projectFolder).release());
            }
        }
    }
    else if (track.getType() == TrackType::MIDI)
    {
        auto* midiTrack = dynamic_cast<const MidiTrack*>(&track);
        if (midiTrack)
        {
            // Save VST3 plugin state if a plugin is loaded
            if (midiTrack->hasPlugin())
            {
                auto pluginXml = std::make_unique<juce::XmlElement>("VST3PLUGIN");

                // Save plugin description
                const auto& desc = midiTrack->getPluginDescription();
                pluginXml->setAttribute("name", desc.name);
                pluginXml->setAttribute("pluginFormatName", desc.pluginFormatName);
                pluginXml->setAttribute("fileOrIdentifier", desc.fileOrIdentifier);
                pluginXml->setAttribute("uid", juce::String(desc.uniqueId));
                pluginXml->setAttribute("isInstrument", desc.isInstrument);
                pluginXml->setAttribute("manufacturerName", desc.manufacturerName);
                pluginXml->setAttribute("version", desc.version);

                // Save plugin state as base64-encoded data
                juce::MemoryBlock stateData;
                midiTrack->savePluginState(stateData);
                if (stateData.getSize() > 0)
                {
                    pluginXml->setAttribute("state", stateData.toBase64Encoding());
                }

                xml->addChildElement(pluginXml.release());
            }

            // MIDI regions (same as regular since they don't reference external files)
            for (int i = 0; i < midiTrack->getNumRegions(); ++i)
            {
                xml->addChildElement(createRegionXml(*midiTrack->getRegion(i)).release());
            }

            // Save MIDI effects (arpeggiator etc.)
            for (int i = 0; i < midiTrack->getNumMidiEffects(); ++i)
            {
                auto* effect = midiTrack->getMidiEffect(i);
                if (effect != nullptr)
                {
                    auto effectXml = effect->createXml();
                    if (effectXml != nullptr)
                        xml->addChildElement(effectXml.release());
                }
            }
        }
    }

    return xml;
}

std::unique_ptr<juce::XmlElement> ProjectSerializer::createRegionXmlForBundle(const Region& region, const juce::File& projectFolder)
{
    auto xml = std::make_unique<juce::XmlElement>("REGION");
    xml->setAttribute("name", region.getName());
    xml->setAttribute("start", static_cast<double>(region.getStartPosition()));
    xml->setAttribute("length", static_cast<double>(region.getLength()));
    xml->setAttribute("offset", static_cast<double>(region.getOffset()));

    // Save fade parameters
    xml->setAttribute("fadeIn", static_cast<double>(region.getFadeInLength()));
    xml->setAttribute("fadeOut", static_cast<double>(region.getFadeOutLength()));

    if (auto* audioRegion = dynamic_cast<const AudioRegion*>(&region))
    {
        juce::String originalPath = audioRegion->getFilePath();

        if (originalPath.isNotEmpty())
        {
            juce::File sourceFile(originalPath);

            // Copy the file to the bundle and get the relative path
            juce::String relativePath = copyAudioFileToBundle(sourceFile, projectFolder);

            if (relativePath.isNotEmpty())
            {
                xml->setAttribute("file", relativePath);
                xml->setAttribute("relativePath", true);
            }
            else
            {
                // Fallback to absolute path if copy failed
                xml->setAttribute("file", originalPath);
                xml->setAttribute("relativePath", false);
            }
        }

        xml->setAttribute("crossfadeCurveType", audioRegion->getCrossfadeCurveType());
        xml->setAttribute("isReversed", audioRegion->getIsReversed());
        xml->setAttribute("stretchRatio", static_cast<double>(audioRegion->getStretchRatio()));
        xml->setAttribute("pitchSemitones", audioRegion->getPitchSemitones());
        xml->setAttribute("pitchCents", audioRegion->getPitchCents());
    }
    else if (auto* midiRegion = dynamic_cast<const MidiRegion*>(&region))
    {
        // Save MIDI events (same as regular format)
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

//==============================================================================
// Bundle XML restoration
//==============================================================================

void ProjectSerializer::restoreTimelineFromXmlBundle(Timeline& timeline, const juce::XmlElement& xml, MidiEngine* midiEngine, const juce::File& projectFolder)
{
    for (auto* child : xml.getChildIterator())
    {
        if (child->hasTagName("TRACK"))
        {
            restoreTrackFromXmlBundle(timeline, *child, midiEngine, projectFolder);
        }
        else if (child->hasTagName("TEMPO_TRACK"))
        {
            auto& tempoTrack = timeline.getTempoTrack();
            tempoTrack.clearAllEvents();
            tempoTrack.setInitialBpm(child->getDoubleAttribute("initialBpm", 120.0));
            tempoTrack.setEnabled(child->getBoolAttribute("enabled", true));
            for (auto* eXml : child->getChildIterator())
            {
                if (eXml->hasTagName("TEMPO_EVENT"))
                {
                    int64_t pos = static_cast<int64_t>(eXml->getDoubleAttribute("position", 0.0));
                    double bpm = eXml->getDoubleAttribute("bpm", 120.0);
                    auto rampType = static_cast<TempoRampType>(eXml->getIntAttribute("rampType", 0));
                    tempoTrack.addEvent(TempoEvent(pos, bpm, rampType));
                }
            }
        }
        else if (child->hasTagName("TIME_SIG_TRACK"))
        {
            auto& timeSigTrack = timeline.getTimeSignatureTrack();
            timeSigTrack.clearAllEvents();
            timeSigTrack.setInitialTimeSignature(
                child->getIntAttribute("initialNumerator", 4),
                child->getIntAttribute("initialDenominator", 4));
            timeSigTrack.setEnabled(child->getBoolAttribute("enabled", true));
            for (auto* eXml : child->getChildIterator())
            {
                if (eXml->hasTagName("TIME_SIG_EVENT"))
                {
                    int64_t pos = static_cast<int64_t>(eXml->getDoubleAttribute("position", 0.0));
                    int num = eXml->getIntAttribute("numerator", 4);
                    int denom = eXml->getIntAttribute("denominator", 4);
                    timeSigTrack.addEvent(TimeSignatureEvent(pos, num, denom));
                }
            }
        }
        else if (child->hasTagName("TRACK_FOLDERS"))
        {
            timeline.clearTrackFolders();
            for (auto* folderXml : child->getChildIterator())
            {
                if (folderXml->hasTagName("FOLDER"))
                {
                    TrackFolder folder;
                    folder.name = folderXml->getStringAttribute("name", "Folder");
                    folder.colour = juce::Colour::fromString(folderXml->getStringAttribute("colour", "ff808080"));
                    folder.collapsed = folderXml->getBoolAttribute("collapsed", false);
                    for (auto* refXml : folderXml->getChildIterator())
                    {
                        if (refXml->hasTagName("TRACK_REF"))
                            folder.trackIndices.push_back(refXml->getIntAttribute("index"));
                    }
                    timeline.addTrackFolder(folder);
                }
            }
        }
        else if (child->hasTagName("MARKERS"))
        {
            auto& markerTrack = timeline.getMarkerTrack();
            markerTrack.clearAllMarkers();
            for (auto* mXml : child->getChildIterator())
            {
                if (mXml->hasTagName("MARKER"))
                {
                    Marker m;
                    m.name = mXml->getStringAttribute("name", "Marker");
                    m.position = static_cast<int64_t>(mXml->getDoubleAttribute("position", 0.0));
                    m.colour = juce::Colour::fromString(mXml->getStringAttribute("colour", "ffffa500"));
                    m.type = static_cast<MarkerType>(mXml->getIntAttribute("type", 0));
                    markerTrack.addMarker(m);
                }
            }
        }
    }
}

void ProjectSerializer::restoreTrackFromXmlBundle(Timeline& timeline, const juce::XmlElement& xml, MidiEngine* midiEngine, const juce::File& projectFolder)
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
        auto midiTrack = std::make_unique<MidiTrack>(xml.getStringAttribute("name"), midiEngine);

        // Check for VST3 plugin info to restore
        if (auto* pluginXml = xml.getChildByName("VST3PLUGIN"))
        {
            juce::PluginDescription desc;
            desc.name = pluginXml->getStringAttribute("name");
            desc.pluginFormatName = pluginXml->getStringAttribute("pluginFormatName", "VST3");
            desc.fileOrIdentifier = pluginXml->getStringAttribute("fileOrIdentifier");
            desc.uniqueId = pluginXml->getStringAttribute("uid").getIntValue();
            desc.isInstrument = pluginXml->getBoolAttribute("isInstrument", true);
            desc.manufacturerName = pluginXml->getStringAttribute("manufacturerName");
            desc.version = pluginXml->getStringAttribute("version");

            // Restore plugin state from base64
            juce::MemoryBlock stateData;
            juce::String stateString = pluginXml->getStringAttribute("state");
            if (stateString.isNotEmpty())
            {
                stateData.fromBase64Encoding(stateString);
            }

            // Store pending plugin info for deferred loading
            midiTrack->setPendingPluginInfo(desc, stateData);
        }

        track = std::move(midiTrack);
    }

    if (track != nullptr)
    {
        track->setVolume(xml.getDoubleAttribute("volume", 1.0));
        track->setPan(static_cast<float>(xml.getDoubleAttribute("pan", 0.0)));
        track->setMuted(xml.getBoolAttribute("muted", false));
        track->setSoloed(xml.getBoolAttribute("soloed", false));
        track->setArmed(xml.getBoolAttribute("armed", false));
        track->setColour(juce::Colour::fromString(xml.getStringAttribute("colour")));

        // Restore automation lanes
        if (auto* automationXml = xml.getChildByName("AUTOMATION_LANES"))
        {
            restoreAutomationLanesFromXml(*track, *automationXml);
        }

        // Restore regions with relative path resolution
        for (auto* child : xml.getChildIterator())
        {
            if (child->hasTagName("REGION"))
            {
                restoreRegionFromXmlBundle(*track, *child, projectFolder);
            }
        }

        timeline.addTrack(track.release());
    }
}

void ProjectSerializer::restoreRegionFromXmlBundle(Track& track, const juce::XmlElement& xml, const juce::File& projectFolder)
{
    int64_t start = static_cast<int64_t>(xml.getDoubleAttribute("start"));
    int64_t length = static_cast<int64_t>(xml.getDoubleAttribute("length"));
    int64_t offset = static_cast<int64_t>(xml.getDoubleAttribute("offset"));
    juce::String name = xml.getStringAttribute("name");

    // Restore fade parameters
    int64_t fadeIn = static_cast<int64_t>(xml.getDoubleAttribute("fadeIn", 0.0));
    int64_t fadeOut = static_cast<int64_t>(xml.getDoubleAttribute("fadeOut", 0.0));

    if (auto* audioTrack = dynamic_cast<AudioTrack*>(&track))
    {
        juce::String filePath = xml.getStringAttribute("file");
        bool isRelative = xml.getBoolAttribute("relativePath", false);

        juce::File audioFile;

        if (isRelative || isRelativePath(filePath))
        {
            // Resolve relative path
            audioFile = resolveRelativePath(filePath, projectFolder);
        }
        else
        {
            // Try absolute path
            audioFile = juce::File(filePath);

            // If absolute path doesn't exist, try resolving as relative
            if (!audioFile.existsAsFile())
            {
                audioFile = resolveRelativePath(filePath, projectFolder);
            }
        }

        if (audioFile.existsAsFile())
        {
            AudioImporter importer;
            auto buffer = importer.loadFile(audioFile, 44100.0);

            if (buffer != nullptr)
            {
                auto region = std::make_unique<AudioRegion>(start, buffer->getNumSamples());
                region->setAudioBuffer(*buffer);
                region->setLength(length);
                region->setOffset(offset);
                region->setName(name);
                region->setFilePath(audioFile.getFullPathName());
                region->setFadeInLength(fadeIn);
                region->setFadeOutLength(fadeOut);
                region->setCrossfadeCurveType(xml.getIntAttribute("crossfadeCurveType", 1));
                region->setIsReversed(xml.getBoolAttribute("isReversed", false));
                region->setStretchRatio(static_cast<float>(xml.getDoubleAttribute("stretchRatio", 1.0)));
                region->setPitchSemitones(xml.getIntAttribute("pitchSemitones", 0));
                region->setPitchCents(xml.getIntAttribute("pitchCents", 0));

                audioTrack->addRegion(std::move(region));
            }
        }
        else
        {
            DBG("Audio file not found: " + filePath);
        }
    }
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(&track))
    {
        auto region = std::make_unique<MidiRegion>(start, length);
        region->setName(name);
        region->setOffset(offset);
        region->setFadeInLength(fadeIn);
        region->setFadeOutLength(fadeOut);

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

//==============================================================================
// Audio file management utilities
//==============================================================================

juce::String ProjectSerializer::copyAudioFileToBundle(const juce::File& sourceFile, const juce::File& projectFolder)
{
    if (!sourceFile.existsAsFile())
        return {};

    auto audioFilesFolder = getAudioFilesFolder(projectFolder);

    // Check if the file is already in the bundle's Audio Files folder
    if (sourceFile.getParentDirectory() == audioFilesFolder)
    {
        // Already in the right place, return relative path
        return juce::String(AUDIO_FILES_FOLDER) + "/" + sourceFile.getFileName();
    }

    // Generate unique filename in case of conflicts
    juce::String targetFilename = generateUniqueFilename(audioFilesFolder, sourceFile.getFileName());
    juce::File targetFile = audioFilesFolder.getChildFile(targetFilename);

    // Copy the file
    if (sourceFile.copyFileTo(targetFile))
    {
        return juce::String(AUDIO_FILES_FOLDER) + "/" + targetFilename;
    }

    DBG("Failed to copy audio file: " + sourceFile.getFullPathName());
    return {};
}

juce::String ProjectSerializer::makeRelativePath(const juce::File& absolutePath, const juce::File& projectFolder)
{
    return absolutePath.getRelativePathFrom(projectFolder);
}

juce::File ProjectSerializer::resolveRelativePath(const juce::String& relativePath, const juce::File& projectFolder)
{
    return projectFolder.getChildFile(relativePath);
}

bool ProjectSerializer::isRelativePath(const juce::String& path)
{
    if (path.isEmpty())
        return false;

#if JUCE_WINDOWS
    // On Windows, check for drive letter or UNC path
    if (path.length() >= 2 && path[1] == ':')
        return false;
    if (path.startsWith("\\\\"))
        return false;
#else
    // On Unix-like systems, absolute paths start with /
    if (path.startsWith("/"))
        return false;
#endif

    // Also check for our bundle folder prefix
    if (path.startsWith(AUDIO_FILES_FOLDER))
        return true;

    return true;
}

juce::String ProjectSerializer::generateUniqueFilename(const juce::File& targetFolder, const juce::String& originalName)
{
    juce::File targetFile = targetFolder.getChildFile(originalName);

    if (!targetFile.exists())
        return originalName;

    // File exists, need to add a number
    juce::String baseName = targetFile.getFileNameWithoutExtension();
    juce::String extension = targetFile.getFileExtension();

    int counter = 1;
    juce::String newName;

    do
    {
        newName = baseName + "_" + juce::String(counter) + extension;
        targetFile = targetFolder.getChildFile(newName);
        counter++;
    }
    while (targetFile.exists() && counter < 10000);

    return newName;
}

void ProjectSerializer::loadProject(Timeline& timeline, Transport& transport, MidiEngine* midiEngine, const juce::File& file)
{
    // Check if this is a bundle format (folder with project file or .msproj in bundle)
    if (isProjectBundle(file))
    {
        loadProjectBundle(timeline, transport, midiEngine, file);
        return;
    }

    // Legacy single-file format
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

        // Load transport settings
        int64_t playheadPosition = static_cast<int64_t>(xml->getDoubleAttribute("playheadPosition", 0.0));
        bool looping = xml->getBoolAttribute("looping", false);
        int64_t loopStart = static_cast<int64_t>(xml->getDoubleAttribute("loopStart", 0.0));
        int64_t loopEnd = static_cast<int64_t>(xml->getDoubleAttribute("loopEnd", 0.0));

        transport.setPlayheadPosition(playheadPosition);
        transport.setLooping(looping);
        transport.setLoopRange(loopStart, loopEnd);

        // Load punch state
        transport.setPunchInEnabled(xml->getBoolAttribute("punchInEnabled", false));
        transport.setPunchOutEnabled(xml->getBoolAttribute("punchOutEnabled", false));
        transport.setPunchRange(
            static_cast<int64_t>(xml->getDoubleAttribute("punchInPosition", 0.0)),
            static_cast<int64_t>(xml->getDoubleAttribute("punchOutPosition", 0.0)));
        transport.setPreRollBeats(xml->getIntAttribute("preRollBeats", 0));

        restoreTimelineFromXml(timeline, *xml, midiEngine);
    }
}

std::unique_ptr<juce::XmlElement> ProjectSerializer::createTimelineXml(const Timeline& timeline, const Transport& transport)
{
    auto xml = std::make_unique<juce::XmlElement>("PROJECT");
    xml->setAttribute("version", "1.0");
    xml->setAttribute("bpm", timeline.getBpm());
    xml->setAttribute("beatsPerBar", timeline.getBeatsPerBar());

    // Save transport settings
    xml->setAttribute("playheadPosition", static_cast<double>(transport.getPlayheadPosition()));
    xml->setAttribute("looping", transport.isLooping());
    xml->setAttribute("loopStart", static_cast<double>(transport.getLoopStart()));
    xml->setAttribute("loopEnd", static_cast<double>(transport.getLoopEnd()));
    xml->setAttribute("punchInEnabled", transport.isPunchInEnabled());
    xml->setAttribute("punchOutEnabled", transport.isPunchOutEnabled());
    xml->setAttribute("punchInPosition", static_cast<double>(transport.getPunchInPosition()));
    xml->setAttribute("punchOutPosition", static_cast<double>(transport.getPunchOutPosition()));
    xml->setAttribute("preRollBeats", transport.getPreRollBeats());

    for (int i = 0; i < timeline.getNumTracks(); ++i)
    {
        xml->addChildElement(createTrackXml(*timeline.getTrack(i)).release());
    }

    // Save tempo track events
    const auto& tempoTrackL = timeline.getTempoTrack();
    if (tempoTrackL.getNumEvents() > 0)
    {
        auto tempoXml = std::make_unique<juce::XmlElement>("TEMPO_TRACK");
        tempoXml->setAttribute("initialBpm", tempoTrackL.getInitialBpm());
        tempoXml->setAttribute("enabled", tempoTrackL.isEnabled());
        for (size_t ti = 0; ti < tempoTrackL.getNumEvents(); ++ti)
        {
            const auto& e = tempoTrackL.getEvent(ti);
            auto eXml = std::make_unique<juce::XmlElement>("TEMPO_EVENT");
            eXml->setAttribute("position", static_cast<double>(e.position));
            eXml->setAttribute("bpm", e.bpm);
            eXml->setAttribute("rampType", static_cast<int>(e.rampType));
            tempoXml->addChildElement(eXml.release());
        }
        xml->addChildElement(tempoXml.release());
    }

    // Save time signature track events
    const auto& timeSigTrackL = timeline.getTimeSignatureTrack();
    if (timeSigTrackL.getNumEvents() > 0)
    {
        auto tsXml = std::make_unique<juce::XmlElement>("TIME_SIG_TRACK");
        tsXml->setAttribute("initialNumerator", timeSigTrackL.getInitialNumerator());
        tsXml->setAttribute("initialDenominator", timeSigTrackL.getInitialDenominator());
        tsXml->setAttribute("enabled", timeSigTrackL.isEnabled());
        for (size_t ti = 0; ti < timeSigTrackL.getNumEvents(); ++ti)
        {
            const auto& e = timeSigTrackL.getEvent(ti);
            auto eXml = std::make_unique<juce::XmlElement>("TIME_SIG_EVENT");
            eXml->setAttribute("position", static_cast<double>(e.position));
            eXml->setAttribute("numerator", e.numerator);
            eXml->setAttribute("denominator", e.denominator);
            tsXml->addChildElement(eXml.release());
        }
        xml->addChildElement(tsXml.release());
    }

    // Save markers
    const auto& markerTrack = timeline.getMarkerTrack();
    if (markerTrack.getNumMarkers() > 0)
    {
        auto markersXml = std::make_unique<juce::XmlElement>("MARKERS");
        for (int i = 0; i < markerTrack.getNumMarkers(); ++i)
        {
            const auto& m = markerTrack.getMarker(i);
            auto mXml = std::make_unique<juce::XmlElement>("MARKER");
            mXml->setAttribute("name", m.name);
            mXml->setAttribute("position", static_cast<double>(m.position));
            mXml->setAttribute("colour", m.colour.toString());
            mXml->setAttribute("type", static_cast<int>(m.type));
            markersXml->addChildElement(mXml.release());
        }
        xml->addChildElement(markersXml.release());
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
    xml->setAttribute("armed", track.isArmed());
    xml->setAttribute("colour", track.getColour().toString());

    if (track.getType() == TrackType::Audio)
        xml->setAttribute("type", "Audio");
    else if (track.getType() == TrackType::MIDI)
        xml->setAttribute("type", "MIDI");

    // Save automation lanes
    auto automationXml = createAutomationLanesXml(track);
    if (automationXml != nullptr)
        xml->addChildElement(automationXml.release());

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
            // Save VST3 plugin state if a plugin is loaded
            if (midiTrack->hasPlugin())
            {
                auto pluginXml = std::make_unique<juce::XmlElement>("VST3PLUGIN");

                // Save plugin description
                const auto& desc = midiTrack->getPluginDescription();
                pluginXml->setAttribute("name", desc.name);
                pluginXml->setAttribute("pluginFormatName", desc.pluginFormatName);
                pluginXml->setAttribute("fileOrIdentifier", desc.fileOrIdentifier);
                pluginXml->setAttribute("uid", juce::String(desc.uniqueId));
                pluginXml->setAttribute("isInstrument", desc.isInstrument);
                pluginXml->setAttribute("manufacturerName", desc.manufacturerName);
                pluginXml->setAttribute("version", desc.version);

                // Save plugin state as base64-encoded data
                juce::MemoryBlock stateData;
                midiTrack->savePluginState(stateData);
                if (stateData.getSize() > 0)
                {
                    pluginXml->setAttribute("state", stateData.toBase64Encoding());
                }

                xml->addChildElement(pluginXml.release());
            }

            // Regions
            for (int i = 0; i < midiTrack->getNumRegions(); ++i)
            {
                xml->addChildElement(createRegionXml(*midiTrack->getRegion(i)).release());
            }

            // Save MIDI effects (arpeggiator etc.)
            for (int i = 0; i < midiTrack->getNumMidiEffects(); ++i)
            {
                auto* effect = midiTrack->getMidiEffect(i);
                if (effect != nullptr)
                {
                    auto effectXml = effect->createXml();
                    if (effectXml != nullptr)
                        xml->addChildElement(effectXml.release());
                }
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
        xml->setAttribute("file", audioRegion->getFilePath());
        xml->setAttribute("crossfadeCurveType", audioRegion->getCrossfadeCurveType());
        xml->setAttribute("isReversed", audioRegion->getIsReversed());
        xml->setAttribute("stretchRatio", static_cast<double>(audioRegion->getStretchRatio()));
        xml->setAttribute("pitchSemitones", audioRegion->getPitchSemitones());
        xml->setAttribute("pitchCents", audioRegion->getPitchCents());
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
        else if (child->hasTagName("TEMPO_TRACK"))
        {
            auto& tempoTrack = timeline.getTempoTrack();
            tempoTrack.clearAllEvents();
            tempoTrack.setInitialBpm(child->getDoubleAttribute("initialBpm", 120.0));
            tempoTrack.setEnabled(child->getBoolAttribute("enabled", true));
            for (auto* eXml : child->getChildIterator())
            {
                if (eXml->hasTagName("TEMPO_EVENT"))
                {
                    int64_t pos = static_cast<int64_t>(eXml->getDoubleAttribute("position", 0.0));
                    double bpm = eXml->getDoubleAttribute("bpm", 120.0);
                    auto rampType = static_cast<TempoRampType>(eXml->getIntAttribute("rampType", 0));
                    tempoTrack.addEvent(TempoEvent(pos, bpm, rampType));
                }
            }
        }
        else if (child->hasTagName("TIME_SIG_TRACK"))
        {
            auto& timeSigTrack = timeline.getTimeSignatureTrack();
            timeSigTrack.clearAllEvents();
            timeSigTrack.setInitialTimeSignature(
                child->getIntAttribute("initialNumerator", 4),
                child->getIntAttribute("initialDenominator", 4));
            timeSigTrack.setEnabled(child->getBoolAttribute("enabled", true));
            for (auto* eXml : child->getChildIterator())
            {
                if (eXml->hasTagName("TIME_SIG_EVENT"))
                {
                    int64_t pos = static_cast<int64_t>(eXml->getDoubleAttribute("position", 0.0));
                    int num = eXml->getIntAttribute("numerator", 4);
                    int denom = eXml->getIntAttribute("denominator", 4);
                    timeSigTrack.addEvent(TimeSignatureEvent(pos, num, denom));
                }
            }
        }
        else if (child->hasTagName("MARKERS"))
        {
            auto& markerTrack = timeline.getMarkerTrack();
            markerTrack.clearAllMarkers();
            for (auto* mXml : child->getChildIterator())
            {
                if (mXml->hasTagName("MARKER"))
                {
                    Marker m;
                    m.name = mXml->getStringAttribute("name", "Marker");
                    m.position = static_cast<int64_t>(mXml->getDoubleAttribute("position", 0.0));
                    m.colour = juce::Colour::fromString(mXml->getStringAttribute("colour", "ffffa500"));
                    m.type = static_cast<MarkerType>(mXml->getIntAttribute("type", 0));
                    markerTrack.addMarker(m);
                }
            }
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
        auto midiTrack = std::make_unique<MidiTrack>(xml.getStringAttribute("name"), midiEngine);

        // Check for VST3 plugin info to restore
        if (auto* pluginXml = xml.getChildByName("VST3PLUGIN"))
        {
            juce::PluginDescription desc;
            desc.name = pluginXml->getStringAttribute("name");
            desc.pluginFormatName = pluginXml->getStringAttribute("pluginFormatName", "VST3");
            desc.fileOrIdentifier = pluginXml->getStringAttribute("fileOrIdentifier");
            desc.uniqueId = pluginXml->getStringAttribute("uid").getIntValue();
            desc.isInstrument = pluginXml->getBoolAttribute("isInstrument", true);
            desc.manufacturerName = pluginXml->getStringAttribute("manufacturerName");
            desc.version = pluginXml->getStringAttribute("version");

            // Restore plugin state from base64
            juce::MemoryBlock stateData;
            juce::String stateString = pluginXml->getStringAttribute("state");
            if (stateString.isNotEmpty())
            {
                stateData.fromBase64Encoding(stateString);
            }

            // Store pending plugin info for deferred loading
            midiTrack->setPendingPluginInfo(desc, stateData);
        }

        track = std::move(midiTrack);
    }

    if (track != nullptr)
    {
        track->setVolume(xml.getDoubleAttribute("volume", 1.0));
        track->setPan(static_cast<float>(xml.getDoubleAttribute("pan", 0.0)));
        track->setMuted(xml.getBoolAttribute("muted", false));
        track->setSoloed(xml.getBoolAttribute("soloed", false));
        track->setArmed(xml.getBoolAttribute("armed", false));
        track->setColour(juce::Colour::fromString(xml.getStringAttribute("colour")));

        // Restore automation lanes
        if (auto* automationXml = xml.getChildByName("AUTOMATION_LANES"))
        {
            restoreAutomationLanesFromXml(*track, *automationXml);
        }

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
                region->setCrossfadeCurveType(xml.getIntAttribute("crossfadeCurveType", 1));
                region->setIsReversed(xml.getBoolAttribute("isReversed", false));
                region->setStretchRatio(static_cast<float>(xml.getDoubleAttribute("stretchRatio", 1.0)));
                region->setPitchSemitones(xml.getIntAttribute("pitchSemitones", 0));
                region->setPitchCents(xml.getIntAttribute("pitchCents", 0));

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

//==============================================================================
// Collect All Files
//==============================================================================

int ProjectSerializer::collectAllFiles(Timeline& timeline, const juce::File& projectFolder)
{
    // Create Audio Files subfolder if it doesn't exist
    auto audioFilesFolder = getAudioFilesFolder(projectFolder);
    if (!audioFilesFolder.exists())
    {
        auto result = audioFilesFolder.createDirectory();
        if (result.failed())
        {
            DBG("Failed to create Audio Files folder: " + result.getErrorMessage());
            return -1;
        }
    }

    int filesCollected = 0;

    // Iterate through all tracks
    for (int trackIndex = 0; trackIndex < timeline.getNumTracks(); ++trackIndex)
    {
        Track* track = timeline.getTrack(trackIndex);
        if (track == nullptr || track->getType() != TrackType::Audio)
            continue;

        auto* audioTrack = dynamic_cast<AudioTrack*>(track);
        if (audioTrack == nullptr)
            continue;

        // Iterate through all regions in this track
        for (int regionIndex = 0; regionIndex < audioTrack->getNumRegions(); ++regionIndex)
        {
            AudioRegion* region = audioTrack->getRegion(regionIndex);
            if (region == nullptr)
                continue;

            juce::String currentPath = region->getFilePath();
            if (currentPath.isEmpty())
                continue;

            juce::File sourceFile(currentPath);
            if (!sourceFile.existsAsFile())
                continue;

            // Check if file is already in the Audio Files folder
            if (sourceFile.getParentDirectory() == audioFilesFolder)
                continue;

            // Copy the file to the bundle
            juce::String newRelativePath = copyAudioFileToBundle(sourceFile, projectFolder);
            if (newRelativePath.isNotEmpty())
            {
                // Update the region's file path to the new location
                juce::File newFile = resolveRelativePath(newRelativePath, projectFolder);
                region->setFilePath(newFile.getFullPathName());
                filesCollected++;
            }
        }
    }

    return filesCollected;
}

//==============================================================================
// Automation Lane Serialization
//==============================================================================

std::unique_ptr<juce::XmlElement> ProjectSerializer::createAutomationLanesXml(const Track& track)
{
    if (track.getNumAutomationLanes() == 0)
        return nullptr;

    auto xml = std::make_unique<juce::XmlElement>("AUTOMATION_LANES");

    for (size_t i = 0; i < track.getNumAutomationLanes(); ++i)
    {
        const auto* lane = track.getAutomationLane(i);
        if (lane == nullptr)
            continue;

        auto laneXml = std::make_unique<juce::XmlElement>("LANE");
        laneXml->setAttribute("name", lane->getParameterName());
        laneXml->setAttribute("type", static_cast<int>(lane->getParameterType()));
        laneXml->setAttribute("visible", lane->isVisible());
        laneXml->setAttribute("bypassed", lane->isBypassed());
        laneXml->setAttribute("defaultValue", lane->getDefaultValue());
        laneXml->setAttribute("minValue", lane->getMinValue());
        laneXml->setAttribute("maxValue", lane->getMaxValue());
        laneXml->setAttribute("height", lane->getHeight());

        // Save automation points
        const auto& points = lane->getPoints();
        for (const auto& point : points)
        {
            auto pointXml = std::make_unique<juce::XmlElement>("POINT");
            pointXml->setAttribute("position", static_cast<double>(point.position));
            pointXml->setAttribute("value", point.value);
            pointXml->setAttribute("curve", static_cast<int>(point.curve));
            laneXml->addChildElement(pointXml.release());
        }

        xml->addChildElement(laneXml.release());
    }

    return xml;
}

void ProjectSerializer::restoreAutomationLanesFromXml(Track& track, const juce::XmlElement& xml)
{
    for (auto* laneXml : xml.getChildIterator())
    {
        if (!laneXml->hasTagName("LANE"))
            continue;

        juce::String paramName = laneXml->getStringAttribute("name");
        if (paramName.isEmpty())
            continue;

        // Create the automation lane
        AutomationLane* lane = track.addAutomationLane(paramName);
        if (lane == nullptr)
            continue;

        // Restore lane properties
        lane->setVisible(laneXml->getBoolAttribute("visible", true));
        lane->setBypassed(laneXml->getBoolAttribute("bypassed", false));
        lane->setDefaultValue(static_cast<float>(laneXml->getDoubleAttribute("defaultValue", 0.5)));
        lane->setValueRange(
            static_cast<float>(laneXml->getDoubleAttribute("minValue", 0.0)),
            static_cast<float>(laneXml->getDoubleAttribute("maxValue", 1.0))
        );
        lane->setHeight(laneXml->getIntAttribute("height", 60));

        // Restore automation points
        for (auto* pointXml : laneXml->getChildIterator())
        {
            if (!pointXml->hasTagName("POINT"))
                continue;

            int64_t position = static_cast<int64_t>(pointXml->getDoubleAttribute("position", 0.0));
            float value = static_cast<float>(pointXml->getDoubleAttribute("value", 0.5));
            auto curveType = static_cast<AutomationCurveType>(pointXml->getIntAttribute("curve", 0));

            lane->addPoint(position, value, curveType);
        }
    }
}
