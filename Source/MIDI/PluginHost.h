#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <functional>
#include <atomic>

/**
 * PluginHost manages VST3 plugin loading, scanning, and hosting.
 * Uses juce::AudioPluginFormatManager to load third-party VST3 instruments.
 *
 * Key features:
 * - Background plugin scanning using KnownPluginList
 * - Async plugin instantiation to avoid blocking UI
 * - Thread-safe plugin loading and unloading
 * - Plugin state serialization for project save/load
 */
class PluginHost : public juce::ChangeListener
{
public:
    /**
     * Callback types for async operations.
     */
    using PluginLoadedCallback = std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const juce::String&)>;
    using ScanProgressCallback = std::function<void(float progress, const juce::String& currentPlugin)>;
    using ScanCompleteCallback = std::function<void()>;

    PluginHost();
    ~PluginHost() override;

    //==========================================================================
    // Audio Processing Setup
    //==========================================================================

    /**
     * Prepare the host for audio processing.
     * Must be called before processBlock().
     * @param sampleRate The sample rate to use
     * @param samplesPerBlock Maximum block size
     */
    void prepareToPlay(double sampleRate, int samplesPerBlock);

    /**
     * Release all audio resources.
     */
    void releaseResources();

    //==========================================================================
    // Plugin Scanning
    //==========================================================================

    /**
     * Start scanning for VST3 plugins in the background.
     * Uses default VST3 plugin paths for the current platform.
     * @param progressCallback Called with scan progress (0.0 to 1.0) and current plugin name
     * @param completeCallback Called when scanning is complete
     */
    void startPluginScan(ScanProgressCallback progressCallback = nullptr,
                         ScanCompleteCallback completeCallback = nullptr);

    /**
     * Start scanning specific directories for VST3 plugins.
     * @param directories The directories to scan
     * @param progressCallback Called with scan progress
     * @param completeCallback Called when scanning is complete
     */
    void startPluginScan(const juce::StringArray& directories,
                         ScanProgressCallback progressCallback = nullptr,
                         ScanCompleteCallback completeCallback = nullptr);

    /**
     * Cancel any ongoing plugin scan.
     */
    void cancelPluginScan();

    /**
     * Check if a plugin scan is in progress.
     * @return true if scanning
     */
    bool isScanning() const { return scanning.load(); }

    /**
     * Get the progress of the current scan.
     * @return Progress from 0.0 to 1.0
     */
    float getScanProgress() const { return scanProgress.load(); }

    //==========================================================================
    // Plugin Loading
    //==========================================================================

    /**
     * Load a plugin asynchronously.
     * Uses createPluginInstanceAsync() to avoid blocking the UI thread.
     * @param pluginDescription Description of the plugin to load
     * @param callback Called when plugin is loaded (or failed with error message)
     */
    void loadPluginAsync(const juce::PluginDescription& pluginDescription,
                         PluginLoadedCallback callback);

    /**
     * Load a plugin synchronously (use sparingly, can block).
     * @param pluginDescription Description of the plugin to load
     * @param errorMessage Output parameter for error messages
     * @return The loaded plugin instance, or nullptr on failure
     */
    std::unique_ptr<juce::AudioPluginInstance> loadPluginSync(
        const juce::PluginDescription& pluginDescription,
        juce::String& errorMessage);

    //==========================================================================
    // Known Plugin List
    //==========================================================================

    /**
     * Get the list of known plugins.
     * @return Reference to the KnownPluginList
     */
    juce::KnownPluginList& getKnownPluginList() { return knownPluginList; }
    const juce::KnownPluginList& getKnownPluginList() const { return knownPluginList; }

    /**
     * Get plugins filtered by type.
     * @param isInstrument true for instrument plugins, false for effects
     * @return Array of matching plugin descriptions
     */
    juce::Array<juce::PluginDescription> getPluginsByType(bool isInstrument) const;

    /**
     * Get all available VST3 plugins.
     * @return Array of all VST3 plugin descriptions
     */
    juce::Array<juce::PluginDescription> getAllPlugins() const;

    /**
     * Clear the known plugin list.
     */
    void clearPluginList();

    //==========================================================================
    // Plugin Path Management
    //==========================================================================

    /**
     * Get the default VST3 plugin paths for the current platform.
     * @return Array of default plugin directories
     */
    static juce::StringArray getDefaultPluginPaths();

    /**
     * Add a custom plugin search path.
     * @param path The directory path to add
     */
    void addPluginSearchPath(const juce::File& path);

    /**
     * Get the current plugin search paths.
     * @return Array of search paths
     */
    const juce::StringArray& getPluginSearchPaths() const { return pluginSearchPaths; }

    //==========================================================================
    // Serialization
    //==========================================================================

    /**
     * Create XML representation of the known plugin list.
     * @return XML element containing plugin list data
     */
    std::unique_ptr<juce::XmlElement> createXml() const;

    /**
     * Load known plugin list from XML.
     * @param xml XML element containing plugin list data
     */
    void loadFromXml(const juce::XmlElement& xml);

    /**
     * Save plugin list to the default application data file.
     */
    void savePluginListToFile();

    /**
     * Load plugin list from the default application data file.
     */
    void loadPluginListFromFile();

    //==========================================================================
    // Change Listener (for KnownPluginList changes)
    //==========================================================================

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    /**
     * Set a callback for when the plugin list changes.
     * @param callback Function to call when plugin list is updated
     */
    void setPluginListChangedCallback(std::function<void()> callback)
    {
        pluginListChangedCallback = std::move(callback);
    }

private:
    //==========================================================================
    // Plugin Scanning Thread
    //==========================================================================

    class PluginScannerThread : public juce::Thread
    {
    public:
        PluginScannerThread(PluginHost& owner, const juce::StringArray& paths);
        ~PluginScannerThread() override;

        void run() override;

        ScanProgressCallback progressCallback;
        ScanCompleteCallback completeCallback;

    private:
        PluginHost& owner;
        juce::StringArray pathsToScan;
    };

    //==========================================================================
    // Member Variables
    //==========================================================================

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    juce::StringArray pluginSearchPaths;

    // Audio processing state
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    // Scanning state
    std::atomic<bool> scanning { false };
    std::atomic<float> scanProgress { 0.0f };
    std::unique_ptr<PluginScannerThread> scannerThread;
    juce::CriticalSection scanLock;

    // Callbacks
    std::function<void()> pluginListChangedCallback;

    // Plugin list file location
    juce::File getPluginListFile() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHost)
};
