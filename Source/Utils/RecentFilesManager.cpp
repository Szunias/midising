#include "RecentFilesManager.h"

RecentFilesManager::RecentFilesManager()
{
    // Set up properties file options
    juce::PropertiesFile::Options options;
    options.applicationName = "MidiSing";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.folderName = "MidiSing";
    options.storageFormat = juce::PropertiesFile::storeAsXML;

    // Create the properties file
    propertiesFile = std::make_unique<juce::PropertiesFile>(options);

    // Load recent files from properties
    loadFromProperties();
}

RecentFilesManager::~RecentFilesManager()
{
    // Save recent files before destroying
    if (propertiesFile != nullptr)
    {
        saveToProperties();
        propertiesFile->saveIfNeeded();
    }
}

void RecentFilesManager::addFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return;

    recentFiles.addFile(file);
    saveToProperties();
}

juce::Array<juce::File> RecentFilesManager::getRecentFiles() const
{
    juce::Array<juce::File> files;

    for (int i = 0; i < recentFiles.getNumFiles(); ++i)
    {
        files.add(recentFiles.getFile(i));
    }

    return files;
}

void RecentFilesManager::clear()
{
    recentFiles.clear();
    saveToProperties();
}

void RecentFilesManager::loadFromProperties()
{
    if (propertiesFile == nullptr)
        return;

    // Load the recent files string from properties
    juce::String recentFilesString = propertiesFile->getValue(recentFilesKey);

    if (recentFilesString.isNotEmpty())
    {
        recentFiles.restoreFromString(recentFilesString);
        recentFiles.setMaxNumberOfItems(maxNumberOfFiles);
    }
}

void RecentFilesManager::saveToProperties()
{
    if (propertiesFile == nullptr)
        return;

    // Save the recent files list as a string
    juce::String recentFilesString = recentFiles.toString();
    propertiesFile->setValue(recentFilesKey, recentFilesString);
    propertiesFile->saveIfNeeded();
}
