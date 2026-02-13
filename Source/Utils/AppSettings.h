#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

/**
 * AppSettings provides centralized persistent user preferences.
 * Wraps juce::PropertiesFile for type-safe access to settings.
 * Singleton pattern - use AppSettings::getInstance().
 */
class AppSettings
{
public:
    static AppSettings& getInstance();

    // Window settings
    int getWindowWidth() const;
    int getWindowHeight() const;
    int getWindowX() const;
    int getWindowY() const;
    void setWindowBounds(int x, int y, int w, int h);

    // Audio device settings
    juce::String getLastAudioDeviceType() const;
    void setLastAudioDeviceType(const juce::String& deviceType);

    // Plugin paths
    juce::StringArray getPluginSearchPaths() const;
    void setPluginSearchPaths(const juce::StringArray& paths);
    void addPluginSearchPath(const juce::String& path);
    void removePluginSearchPath(const juce::String& path);

    // Auto-save settings
    int getAutoSaveIntervalMinutes() const;
    void setAutoSaveIntervalMinutes(int minutes);
    bool isAutoSaveEnabled() const;
    void setAutoSaveEnabled(bool enabled);

    // Metronome settings
    bool isMetronomeEnabled() const;
    void setMetronomeEnabled(bool enabled);
    double getMetronomeVolume() const;
    void setMetronomeVolume(double volume);
    int getMetronomePreCountBars() const;
    void setMetronomePreCountBars(int bars);

    // Recording settings
    juce::String getRecordingPath() const;
    void setRecordingPath(const juce::String& path);

    // Default project folder
    juce::String getDefaultProjectFolder() const;
    void setDefaultProjectFolder(const juce::String& path);

    // Recent files (delegated but stored here)
    juce::StringArray getRecentFiles() const;
    void setRecentFiles(const juce::StringArray& files);

    // Generic access
    juce::String getString(const juce::String& key, const juce::String& defaultValue = {}) const;
    void setString(const juce::String& key, const juce::String& value);
    int getInt(const juce::String& key, int defaultValue = 0) const;
    void setInt(const juce::String& key, int value);
    double getDouble(const juce::String& key, double defaultValue = 0.0) const;
    void setDouble(const juce::String& key, double value);
    bool getBool(const juce::String& key, bool defaultValue = false) const;
    void setBool(const juce::String& key, bool value);

    // Force save to disk
    void save();

private:
    AppSettings();
    ~AppSettings() = default;

    std::unique_ptr<juce::PropertiesFile> propertiesFile;

    static juce::File getSettingsFile();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppSettings)
};
