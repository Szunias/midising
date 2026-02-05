#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include "../MIDI/PluginHost.h"
#include "../UI/PluginWindow.h"
#include <functional>
#include <atomic>
#include <memory>

// Forward declarations
class PluginInstance;

/**
 * PluginManager is a singleton that provides centralized management of VST3/AU plugins.
 * Wraps PluginHost and adds additional features:
 * - Crash-protected scanning using deadMansPedal
 * - Plugin blacklist system
 * - Plugin window management
 * - Centralized plugin lifecycle management
 *
 * Usage:
 *   auto& manager = PluginManager::getInstance();
 *   manager.startScan([](float progress, const juce::String& name) {
 *       // Update UI with scan progress
 *   });
 */
class PluginManager
{
public:
    //==========================================================================
    // Singleton Access
    //==========================================================================

    /**
     * Get the singleton instance of PluginManager.
     * Thread-safe, creates instance on first call.
     * @return Reference to the singleton PluginManager instance
     */
    static PluginManager& getInstance();

    /**
     * Destroy the singleton instance.
     * Call during application shutdown.
     */
    static void destroyInstance();

    /**
     * Destructor.
     * Note: Must be public for std::unique_ptr to work with singleton pattern.
     */
    ~PluginManager();

    //==========================================================================
    // Callback Types
    //==========================================================================

    /**
     * Callback for when a plugin is loaded.
     * @param plugin The loaded plugin instance, or nullptr on failure
     * @param errorMessage Error message if loading failed
     */
    using PluginLoadedCallback = std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const juce::String& errorMessage)>;

    /**
     * Callback for scan progress updates.
     * @param progress Scan progress from 0.0 to 1.0
     * @param currentPlugin Name of the plugin currently being scanned
     */
    using ScanProgressCallback = std::function<void(float progress, const juce::String& currentPlugin)>;

    /**
     * Callback for when scanning is complete.
     */
    using ScanCompleteCallback = std::function<void()>;

    /**
     * Callback for when the plugin list changes.
     */
    using PluginListChangedCallback = std::function<void()>;

    //==========================================================================
    // Audio Processing Setup
    //==========================================================================

    /**
     * Prepare the manager for audio processing.
     * Must be called before processing audio through plugins.
     * @param sampleRate The sample rate to use
     * @param samplesPerBlock Maximum block size
     */
    void prepareToPlay(double sampleRate, int samplesPerBlock);

    /**
     * Release all audio resources.
     */
    void releaseResources();

    /**
     * Get the current sample rate.
     * @return Current sample rate
     */
    double getSampleRate() const { return currentSampleRate; }

    /**
     * Get the current block size.
     * @return Current block size
     */
    int getBlockSize() const { return currentBlockSize; }

    //==========================================================================
    // Plugin Scanning
    //==========================================================================

    /**
     * Start scanning for plugins in the default directories.
     * Uses deadMansPedal for crash protection.
     * @param progressCallback Called with scan progress
     * @param completeCallback Called when scanning is complete
     */
    void startScan(ScanProgressCallback progressCallback = nullptr,
                   ScanCompleteCallback completeCallback = nullptr);

    /**
     * Start scanning specific directories for plugins.
     * @param directories The directories to scan
     * @param progressCallback Called with scan progress
     * @param completeCallback Called when scanning is complete
     */
    void startScan(const juce::StringArray& directories,
                   ScanProgressCallback progressCallback = nullptr,
                   ScanCompleteCallback completeCallback = nullptr);

    /**
     * Cancel any ongoing plugin scan.
     */
    void cancelScan();

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
     * Always use this method for loading plugins (required for AUv3 on macOS).
     * @param pluginDescription Description of the plugin to load
     * @param callback Called when plugin is loaded (or failed with error message)
     */
    void loadPluginAsync(const juce::PluginDescription& pluginDescription,
                         PluginLoadedCallback callback);

    /**
     * Load a plugin synchronously (use sparingly, can block UI).
     * @param pluginDescription Description of the plugin to load
     * @param errorMessage Output parameter for error messages
     * @return The loaded plugin instance, or nullptr on failure
     */
    std::unique_ptr<juce::AudioPluginInstance> loadPluginSync(
        const juce::PluginDescription& pluginDescription,
        juce::String& errorMessage);

    //==========================================================================
    // Plugin List Access
    //==========================================================================

    /**
     * Get the list of all known plugins.
     * @return Array of plugin descriptions
     */
    juce::Array<juce::PluginDescription> getPluginList() const;

    /**
     * Get plugins filtered by type.
     * @param isInstrument true for instrument plugins, false for effects
     * @return Array of matching plugin descriptions
     */
    juce::Array<juce::PluginDescription> getPluginsByType(bool isInstrument) const;

    /**
     * Get instrument plugins (VSTi, AUi).
     * @return Array of instrument plugin descriptions
     */
    juce::Array<juce::PluginDescription> getInstrumentPlugins() const;

    /**
     * Get effect plugins.
     * @return Array of effect plugin descriptions
     */
    juce::Array<juce::PluginDescription> getEffectPlugins() const;

    /**
     * Find a plugin by its identifier string.
     * @param identifierString The plugin identifier
     * @return Plugin description, or nullptr if not found
     */
    const juce::PluginDescription* findPluginByIdentifier(const juce::String& identifierString) const;

    /**
     * Get access to the underlying KnownPluginList.
     * @return Reference to the KnownPluginList
     */
    juce::KnownPluginList& getKnownPluginList();
    const juce::KnownPluginList& getKnownPluginList() const;

    /**
     * Clear all known plugins.
     */
    void clearPluginList();

    //==========================================================================
    // Blacklist Management
    //==========================================================================

    /**
     * Add a plugin to the blacklist.
     * Blacklisted plugins will be skipped during scanning.
     * @param identifierString Plugin identifier string
     */
    void addToBlacklist(const juce::String& identifierString);

    /**
     * Remove a plugin from the blacklist.
     * @param identifierString Plugin identifier string
     */
    void removeFromBlacklist(const juce::String& identifierString);

    /**
     * Check if a plugin is blacklisted.
     * @param identifierString Plugin identifier string
     * @return true if blacklisted
     */
    bool isBlacklisted(const juce::String& identifierString) const;

    /**
     * Get all blacklisted plugin identifiers.
     * @return Array of blacklisted plugin identifier strings
     */
    juce::StringArray getBlacklistedPlugins() const;

    /**
     * Clear the blacklist.
     */
    void clearBlacklist();

    //==========================================================================
    // Plugin Path Management
    //==========================================================================

    /**
     * Get the default plugin paths for the current platform.
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
    juce::StringArray getPluginSearchPaths() const;

    //==========================================================================
    // Serialization
    //==========================================================================

    /**
     * Save the plugin list and blacklist to the default files.
     */
    void saveToFile();

    /**
     * Load the plugin list and blacklist from the default files.
     */
    void loadFromFile();

    /**
     * Create XML representation of the plugin list and blacklist.
     * @return XML element containing plugin data
     */
    std::unique_ptr<juce::XmlElement> createXml() const;

    /**
     * Load plugin list and blacklist from XML.
     * @param xml XML element containing plugin data
     */
    void loadFromXml(const juce::XmlElement& xml);

    //==========================================================================
    // Callbacks
    //==========================================================================

    /**
     * Set a callback for when the plugin list changes.
     * @param callback Function to call when plugin list is updated
     */
    void setPluginListChangedCallback(PluginListChangedCallback callback);

    //==========================================================================
    // Access to Internal PluginHost
    //==========================================================================

    /**
     * Get direct access to the underlying PluginHost.
     * Use with caution - prefer using PluginManager methods.
     * @return Reference to the internal PluginHost
     */
    PluginHost& getPluginHost() { return *pluginHost; }
    const PluginHost& getPluginHost() const { return *pluginHost; }

    //==========================================================================
    // Plugin Window Management
    //==========================================================================

    /**
     * Open a window for a plugin instance.
     * If a window is already open for this plugin, brings it to front.
     * @param plugin The plugin instance to show (must not be null)
     * @param closeAction What to do when the window close button is pressed
     * @return Pointer to the plugin window (owned by PluginManager)
     */
    PluginWindow* openWindow(PluginInstance* plugin,
                             PluginWindow::CloseAction closeAction = PluginWindow::CloseAction::Hide);

    /**
     * Close the window for a specific plugin instance.
     * @param plugin The plugin instance whose window to close
     */
    void closeWindow(PluginInstance* plugin);

    /**
     * Close a specific plugin window.
     * @param window The window to close
     */
    void closeWindow(PluginWindow* window);

    /**
     * Close all open plugin windows.
     */
    void closeAllWindows();

    /**
     * Get the window for a plugin instance, if one is open.
     * @param plugin The plugin instance to find the window for
     * @return Pointer to the window, or nullptr if no window is open
     */
    PluginWindow* getWindowForPlugin(PluginInstance* plugin) const;

    /**
     * Check if a window is open for the given plugin.
     * @param plugin The plugin instance to check
     * @return true if a window is open for this plugin
     */
    bool isWindowOpen(PluginInstance* plugin) const;

    /**
     * Get the number of open plugin windows.
     * @return Number of open windows
     */
    int getNumOpenWindows() const;

    /**
     * Get all open plugin windows.
     * @return Array of pointers to open windows (do not delete)
     */
    juce::Array<PluginWindow*> getOpenWindows() const;

    /**
     * Bring all plugin windows to front.
     * Useful when the main application window is activated.
     */
    void bringAllWindowsToFront();

    /**
     * Hide all plugin windows (without destroying them).
     */
    void hideAllWindows();

    /**
     * Show all plugin windows that were previously hidden.
     */
    void showAllWindows();

    /**
     * Callback invoked when a plugin window is closed.
     * Can be used to update UI state.
     */
    std::function<void(PluginWindow*)> onWindowClosed;

private:
    // Forward declare and friend the scanner thread
    friend class CrashProtectedScannerThread;

    //==========================================================================
    // Private Constructor (Singleton Pattern)
    //==========================================================================

    PluginManager();

    // Non-copyable
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    //==========================================================================
    // Member Variables
    //==========================================================================

    // Underlying plugin host
    std::unique_ptr<PluginHost> pluginHost;

    // Open plugin windows (owned by this manager)
    juce::OwnedArray<PluginWindow> openWindows;
    mutable juce::CriticalSection windowsLock;

    // Blacklist of plugin identifiers to skip during scanning
    juce::StringArray blacklist;
    mutable juce::CriticalSection blacklistLock;

    // Audio processing state
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    // Scanning state
    std::atomic<bool> scanning { false };
    std::atomic<float> scanProgress { 0.0f };

    // Callbacks
    PluginListChangedCallback pluginListChangedCallback;

    // File locations
    juce::File getPluginListFile() const;
    juce::File getBlacklistFile() const;

    // Save/load blacklist
    void saveBlacklistToFile();
    void loadBlacklistFromFile();

    // Window management helpers
    void handleWindowClosed(PluginWindow* window);
    void removeWindow(PluginWindow* window);

    //==========================================================================
    // Singleton Instance
    //==========================================================================

    static std::unique_ptr<PluginManager> instance;
    static juce::CriticalSection instanceLock;
};
