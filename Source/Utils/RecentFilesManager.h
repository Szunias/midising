#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_extra/juce_gui_extra.h>

/**
 * Manages the list of recently opened project files.
 * Uses JUCE's RecentlyOpenedFilesList for storage and persists to application properties.
 */
class RecentFilesManager
{
public:
    RecentFilesManager();
    ~RecentFilesManager();

    /**
     * Adds a file to the recent files list.
     * The file will be moved to the top of the list.
     */
    void addFile(const juce::File& file);

    /**
     * Returns the list of recent files.
     * The most recently used file is at index 0.
     */
    juce::Array<juce::File> getRecentFiles() const;

    /**
     * Clears all recent files from the list.
     */
    void clear();

    /**
     * Returns the maximum number of recent files to track.
     */
    int getMaxNumberOfFiles() const { return maxNumberOfFiles; }

private:
    void loadFromProperties();
    void saveToProperties();

    juce::RecentlyOpenedFilesList recentFiles;
    std::unique_ptr<juce::PropertiesFile> propertiesFile;

    static constexpr int maxNumberOfFiles = 10;
    static constexpr const char* propertiesFileName = "MidiSing.settings";
    static constexpr const char* recentFilesKey = "recentFiles";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecentFilesManager)
};
