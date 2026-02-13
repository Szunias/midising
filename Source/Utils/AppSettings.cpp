#include "AppSettings.h"

AppSettings& AppSettings::getInstance()
{
    static AppSettings instance;
    return instance;
}

AppSettings::AppSettings()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "MidiSing";
    options.filenameSuffix = ".settings";
    options.folderName = "MidiSing";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;

    propertiesFile = std::make_unique<juce::PropertiesFile>(options);
}

juce::File AppSettings::getSettingsFile()
{
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appDataDir.getChildFile("MidiSing").getChildFile("MidiSing.settings");
}

void AppSettings::save()
{
    if (propertiesFile != nullptr)
        propertiesFile->saveIfNeeded();
}

// Window settings
int AppSettings::getWindowWidth() const { return getInt("windowWidth", 1200); }
int AppSettings::getWindowHeight() const { return getInt("windowHeight", 800); }
int AppSettings::getWindowX() const { return getInt("windowX", -1); }
int AppSettings::getWindowY() const { return getInt("windowY", -1); }

void AppSettings::setWindowBounds(int x, int y, int w, int h)
{
    setInt("windowX", x);
    setInt("windowY", y);
    setInt("windowWidth", w);
    setInt("windowHeight", h);
}

// Audio device settings
juce::String AppSettings::getLastAudioDeviceType() const { return getString("lastAudioDeviceType"); }
void AppSettings::setLastAudioDeviceType(const juce::String& deviceType) { setString("lastAudioDeviceType", deviceType); }

// Plugin paths
juce::StringArray AppSettings::getPluginSearchPaths() const
{
    juce::StringArray paths;
    auto pathsStr = getString("pluginSearchPaths");
    if (pathsStr.isNotEmpty())
        paths.addTokens(pathsStr, "|", "");
    return paths;
}

void AppSettings::setPluginSearchPaths(const juce::StringArray& paths)
{
    setString("pluginSearchPaths", paths.joinIntoString("|"));
}

void AppSettings::addPluginSearchPath(const juce::String& path)
{
    auto paths = getPluginSearchPaths();
    if (!paths.contains(path))
    {
        paths.add(path);
        setPluginSearchPaths(paths);
    }
}

void AppSettings::removePluginSearchPath(const juce::String& path)
{
    auto paths = getPluginSearchPaths();
    paths.removeString(path);
    setPluginSearchPaths(paths);
}

// Auto-save settings
int AppSettings::getAutoSaveIntervalMinutes() const { return getInt("autoSaveIntervalMinutes", 5); }
void AppSettings::setAutoSaveIntervalMinutes(int minutes) { setInt("autoSaveIntervalMinutes", juce::jmax(1, minutes)); }
bool AppSettings::isAutoSaveEnabled() const { return getBool("autoSaveEnabled", true); }
void AppSettings::setAutoSaveEnabled(bool enabled) { setBool("autoSaveEnabled", enabled); }

// Metronome settings
bool AppSettings::isMetronomeEnabled() const { return getBool("metronomeEnabled", false); }
void AppSettings::setMetronomeEnabled(bool enabled) { setBool("metronomeEnabled", enabled); }
double AppSettings::getMetronomeVolume() const { return getDouble("metronomeVolume", 0.7); }
void AppSettings::setMetronomeVolume(double volume) { setDouble("metronomeVolume", volume); }
int AppSettings::getMetronomePreCountBars() const { return getInt("metronomePreCountBars", 0); }
void AppSettings::setMetronomePreCountBars(int bars) { setInt("metronomePreCountBars", juce::jmax(0, bars)); }

// Recording settings
juce::String AppSettings::getRecordingPath() const
{
    auto path = getString("recordingPath");
    if (path.isEmpty())
        return juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName();
    return path;
}
void AppSettings::setRecordingPath(const juce::String& path) { setString("recordingPath", path); }

// Default project folder
juce::String AppSettings::getDefaultProjectFolder() const
{
    auto path = getString("defaultProjectFolder");
    if (path.isEmpty())
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getFullPathName();
    return path;
}
void AppSettings::setDefaultProjectFolder(const juce::String& path) { setString("defaultProjectFolder", path); }

// Recent files
juce::StringArray AppSettings::getRecentFiles() const
{
    juce::StringArray files;
    auto filesStr = getString("recentFiles");
    if (filesStr.isNotEmpty())
        files.addTokens(filesStr, "|", "");
    return files;
}
void AppSettings::setRecentFiles(const juce::StringArray& files)
{
    setString("recentFiles", files.joinIntoString("|"));
}

// Generic access
juce::String AppSettings::getString(const juce::String& key, const juce::String& defaultValue) const
{
    if (propertiesFile != nullptr)
        return propertiesFile->getValue(key, defaultValue);
    return defaultValue;
}

void AppSettings::setString(const juce::String& key, const juce::String& value)
{
    if (propertiesFile != nullptr)
    {
        propertiesFile->setValue(key, value);
        propertiesFile->saveIfNeeded();
    }
}

int AppSettings::getInt(const juce::String& key, int defaultValue) const
{
    if (propertiesFile != nullptr)
        return propertiesFile->getIntValue(key, defaultValue);
    return defaultValue;
}

void AppSettings::setInt(const juce::String& key, int value)
{
    if (propertiesFile != nullptr)
    {
        propertiesFile->setValue(key, value);
        propertiesFile->saveIfNeeded();
    }
}

double AppSettings::getDouble(const juce::String& key, double defaultValue) const
{
    if (propertiesFile != nullptr)
        return propertiesFile->getDoubleValue(key, defaultValue);
    return defaultValue;
}

void AppSettings::setDouble(const juce::String& key, double value)
{
    if (propertiesFile != nullptr)
    {
        propertiesFile->setValue(key, value);
        propertiesFile->saveIfNeeded();
    }
}

bool AppSettings::getBool(const juce::String& key, bool defaultValue) const
{
    if (propertiesFile != nullptr)
        return propertiesFile->getBoolValue(key, defaultValue);
    return defaultValue;
}

void AppSettings::setBool(const juce::String& key, bool value)
{
    if (propertiesFile != nullptr)
    {
        propertiesFile->setValue(key, value);
        propertiesFile->saveIfNeeded();
    }
}
