#pragma once

#include <juce_core/juce_core.h>
#include "../Timeline/Timeline.h"
#include "../Timeline/Region.h"
#include "../Audio/Transport.h"
#include "../MIDI/MidiEngine.h"

/**
 * Handles saving and loading of the DAW project.
 * Supports both single-file XML format and bundle format.
 *
 * Bundle Format Structure:
 *   ProjectName/
 *   ├── project.msproj   (XML project data)
 *   └── Audio Files/     (copies of all audio files)
 *       ├── track1_audio.wav
 *       ├── recording_001.wav
 *       └── ...
 *
 * The bundle format uses relative paths to audio files, making projects portable.
 */
class ProjectSerializer
{
public:
    // Bundle format constants
    static constexpr const char* PROJECT_FILE_NAME = "project.msproj";
    static constexpr const char* AUDIO_FILES_FOLDER = "Audio Files";
    static constexpr const char* PROJECT_EXTENSION = ".msproj";

    /**
     * Legacy single-file save (XML only, absolute paths).
     * @deprecated Use saveProjectBundle() for new projects.
     */
    static void saveProject(const Timeline& timeline, const Transport& transport, const juce::File& file);

    /**
     * Legacy single-file load (XML only).
     * Automatically detects bundle format and delegates to loadProjectBundle() if needed.
     */
    static void loadProject(Timeline& timeline, Transport& transport, MidiEngine* midiEngine, const juce::File& file);

    /**
     * Save project as a bundle folder.
     * Creates the folder structure and copies all audio files into the bundle.
     *
     * @param timeline The timeline to save
     * @param transport The transport state to save
     * @param projectFolder The target folder for the project bundle
     * @return true if save was successful, false otherwise
     */
    static bool saveProjectBundle(const Timeline& timeline, const Transport& transport, const juce::File& projectFolder);

    /**
     * Load project from a bundle folder.
     * Resolves relative paths to audio files within the bundle.
     *
     * @param timeline The timeline to load into
     * @param transport The transport to restore state to
     * @param midiEngine The MIDI engine for MIDI track creation
     * @param projectFolder The bundle folder or .msproj file
     * @return true if load was successful, false otherwise
     */
    static bool loadProjectBundle(Timeline& timeline, Transport& transport, MidiEngine* midiEngine, const juce::File& projectFolder);

    /**
     * Check if a file/folder is a valid project bundle.
     * @param path The path to check (can be folder or .msproj file)
     * @return true if it's a valid bundle
     */
    static bool isProjectBundle(const juce::File& path);

    /**
     * Get the Audio Files folder for a project bundle.
     * @param projectFolder The project bundle folder
     * @return The Audio Files subfolder
     */
    static juce::File getAudioFilesFolder(const juce::File& projectFolder);

    /**
     * Collect all external audio files into the project bundle.
     * Copies any audio files that are not already in the project's Audio Files folder
     * and updates the region file paths to point to the collected files.
     *
     * @param timeline The timeline containing tracks and regions to process
     * @param projectFolder The project bundle folder
     * @return Number of files collected, or -1 if an error occurred
     */
    static int collectAllFiles(Timeline& timeline, const juce::File& projectFolder);

private:
    // XML creation methods
    static std::unique_ptr<juce::XmlElement> createTimelineXml(const Timeline& timeline, const Transport& transport);
    static std::unique_ptr<juce::XmlElement> createTrackXml(const Track& track);
    static std::unique_ptr<juce::XmlElement> createRegionXml(const Region& region);

    // Bundle-specific XML creation (uses relative paths)
    static std::unique_ptr<juce::XmlElement> createTimelineXmlForBundle(const Timeline& timeline, const Transport& transport, const juce::File& projectFolder);
    static std::unique_ptr<juce::XmlElement> createTrackXmlForBundle(const Track& track, const juce::File& projectFolder);
    static std::unique_ptr<juce::XmlElement> createRegionXmlForBundle(const Region& region, const juce::File& projectFolder);

    // XML restoration methods
    static void restoreTimelineFromXml(Timeline& timeline, const juce::XmlElement& xml, MidiEngine* midiEngine);
    static void restoreTrackFromXml(Timeline& timeline, const juce::XmlElement& xml, MidiEngine* midiEngine);
    static void restoreRegionFromXml(Track& track, const juce::XmlElement& xml);

    // Automation lane serialization
    static std::unique_ptr<juce::XmlElement> createAutomationLanesXml(const Track& track);
    static void restoreAutomationLanesFromXml(Track& track, const juce::XmlElement& xml);

    // Bundle-specific restoration (resolves relative paths)
    static void restoreTimelineFromXmlBundle(Timeline& timeline, const juce::XmlElement& xml, MidiEngine* midiEngine, const juce::File& projectFolder);
    static void restoreTrackFromXmlBundle(Timeline& timeline, const juce::XmlElement& xml, MidiEngine* midiEngine, const juce::File& projectFolder);
    static void restoreRegionFromXmlBundle(Track& track, const juce::XmlElement& xml, const juce::File& projectFolder);

    // Audio file management
    /**
     * Copy an audio file into the project bundle's Audio Files folder.
     * If the file already exists in the bundle, returns the existing path.
     *
     * @param sourceFile The source audio file
     * @param projectFolder The project bundle folder
     * @return The path to the file in the bundle (relative to project folder), or empty if failed
     */
    static juce::String copyAudioFileToBundle(const juce::File& sourceFile, const juce::File& projectFolder);

    /**
     * Convert an absolute file path to a relative path within the bundle.
     * @param absolutePath The absolute file path
     * @param projectFolder The project bundle folder
     * @return Relative path (e.g., "Audio Files/myaudio.wav")
     */
    static juce::String makeRelativePath(const juce::File& absolutePath, const juce::File& projectFolder);

    /**
     * Resolve a relative path to an absolute path within the bundle.
     * @param relativePath The relative path (e.g., "Audio Files/myaudio.wav")
     * @param projectFolder The project bundle folder
     * @return Absolute file path
     */
    static juce::File resolveRelativePath(const juce::String& relativePath, const juce::File& projectFolder);

    /**
     * Check if a path is already relative (doesn't start with drive letter or root).
     */
    static bool isRelativePath(const juce::String& path);

    /**
     * Generate a unique filename for an audio file in the bundle.
     * Handles conflicts by appending numbers.
     */
    static juce::String generateUniqueFilename(const juce::File& targetFolder, const juce::String& originalName);
};
