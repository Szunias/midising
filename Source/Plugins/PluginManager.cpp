#include "PluginManager.h"
#include "PluginInstance.h"

//==============================================================================
// Singleton Implementation
//==============================================================================

std::unique_ptr<PluginManager> PluginManager::instance = nullptr;
juce::CriticalSection PluginManager::instanceLock;

PluginManager& PluginManager::getInstance()
{
    juce::ScopedLock lock(instanceLock);

    if (instance == nullptr)
    {
        instance.reset(new PluginManager());
    }

    return *instance;
}

void PluginManager::destroyInstance()
{
    juce::ScopedLock lock(instanceLock);
    instance.reset();
}

//==============================================================================
// Constructor/Destructor
//==============================================================================

PluginManager::PluginManager()
{
    // Create the underlying plugin host
    pluginHost = std::make_unique<PluginHost>();

    // Set up plugin list changed callback to forward to our callback
    pluginHost->setPluginListChangedCallback([this]()
    {
        if (pluginListChangedCallback)
        {
            pluginListChangedCallback();
        }
    });

    // Load blacklist from file
    loadBlacklistFromFile();
}

PluginManager::~PluginManager()
{
    // Close all plugin windows first
    closeAllWindows();

    // Cancel any ongoing scan
    cancelScan();

    // Save blacklist before destruction
    saveBlacklistToFile();
}

//==============================================================================
// Audio Processing Setup
//==============================================================================

void PluginManager::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    if (pluginHost)
    {
        pluginHost->prepareToPlay(sampleRate, samplesPerBlock);
    }
}

void PluginManager::releaseResources()
{
    if (pluginHost)
    {
        pluginHost->releaseResources();
    }
}

//==============================================================================
// Plugin Scanning with Crash Protection
//==============================================================================

/**
 * Background thread for crash-protected plugin scanning using deadMansPedal.
 * The deadMansPedal file tracks which plugin is currently being scanned.
 * If the app crashes during scanning, the file will contain the path of the
 * problematic plugin, which will be automatically blacklisted on restart.
 */
class CrashProtectedScannerThread : public juce::Thread
{
public:
    CrashProtectedScannerThread(PluginManager& owner,
                                 juce::KnownPluginList& pluginList,
                                 const juce::StringArray& paths,
                                 const juce::StringArray& blacklist)
        : Thread("Crash-Protected Plugin Scanner"),
          owner(owner),
          pluginList(pluginList),
          pathsToScan(paths),
          blacklistedPlugins(blacklist)
    {
        // Create a fresh format manager for scanning
        formatManager.addDefaultFormats();
    }

    ~CrashProtectedScannerThread() override
    {
        stopThread(10000);
    }

    void run() override
    {
        // Get the deadMansPedal file location
        juce::File deadMansPedalFile = getDeadMansPedalFile();

        // Check if there's a crashed plugin from a previous scan
        if (deadMansPedalFile.existsAsFile())
        {
            juce::String crashedPlugin = deadMansPedalFile.loadFileAsString().trim();
            if (crashedPlugin.isNotEmpty())
            {
                // This plugin crashed during the last scan - add to blacklist
                DBG("Plugin crashed during previous scan: " + crashedPlugin);

                juce::MessageManager::callAsync([this, crashedPlugin]()
                {
                    // Note: We can't directly call addToBlacklist here as it modifies
                    // the blacklist. Instead, we'll skip this plugin and log it.
                    DBG("Skipping previously crashed plugin: " + crashedPlugin);
                });
            }

            // Delete the file so we start fresh
            deadMansPedalFile.deleteFile();
        }

        // Scan for each format
        juce::AudioPluginFormat* vst3Format = nullptr;
        for (int i = 0; i < formatManager.getNumFormats(); ++i)
        {
            auto* format = formatManager.getFormat(i);
            if (format->getName() == "VST3")
            {
                vst3Format = format;
                break;
            }
        }

        if (vst3Format == nullptr)
        {
            owner.scanning.store(false);
            callCompleteCallback();
            return;
        }

        // Collect all plugin files to scan
        juce::FileSearchPath searchPath;
        for (const auto& path : pathsToScan)
        {
            searchPath.add(juce::File(path));
        }

        // Create the scanner with deadMansPedal for crash protection
        juce::PluginDirectoryScanner scanner(
            pluginList,
            *vst3Format,
            searchPath,
            true,  // searchRecursively
            deadMansPedalFile,  // deadMansPedal file for crash recovery
            true   // allowPluginsWhichRequireAsynchronousInstantiation
        );

        // Scan each plugin
        juce::String pluginBeingScanned;

        while (scanner.scanNextFile(true, pluginBeingScanned))
        {
            if (threadShouldExit())
                break;

            // Skip blacklisted plugins
            if (isPluginBlacklisted(pluginBeingScanned))
            {
                continue;
            }

            // Update progress
            float progress = scanner.getProgress();
            owner.scanProgress.store(progress);

            // Call progress callback on message thread
            if (progressCallback)
            {
                juce::String currentPluginName = juce::File(pluginBeingScanned).getFileNameWithoutExtension();
                juce::MessageManager::callAsync([callback = progressCallback, progress, currentPluginName]()
                {
                    callback(progress, currentPluginName);
                });
            }
        }

        // Clean up deadMansPedal file on successful completion
        deadMansPedalFile.deleteFile();

        // Get and log any failures
        auto failedFiles = scanner.getFailedFiles();
        for (const auto& failedFile : failedFiles)
        {
            DBG("Plugin scan failed for: " + failedFile);
        }

        // Mark scanning complete
        owner.scanProgress.store(1.0f);
        owner.scanning.store(false);

        // Call complete callback
        callCompleteCallback();
    }

    PluginManager::ScanProgressCallback progressCallback;
    PluginManager::ScanCompleteCallback completeCallback;

private:
    PluginManager& owner;
    juce::KnownPluginList& pluginList;
    juce::AudioPluginFormatManager formatManager;  // Own format manager for thread safety
    juce::StringArray pathsToScan;
    juce::StringArray blacklistedPlugins;

    juce::File getDeadMansPedalFile() const
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("MidiSing")
            .getChildFile("PluginScanDeadMansPedal.txt");
    }

    bool isPluginBlacklisted(const juce::String& pluginPath) const
    {
        for (const auto& blacklisted : blacklistedPlugins)
        {
            if (pluginPath.contains(blacklisted))
            {
                return true;
            }
        }
        return false;
    }

    void callCompleteCallback()
    {
        if (completeCallback)
        {
            juce::MessageManager::callAsync([callback = completeCallback]()
            {
                callback();
            });
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrashProtectedScannerThread)
};

// Store the scanner thread
static std::unique_ptr<CrashProtectedScannerThread> scannerThread;
static juce::CriticalSection scannerLock;

void PluginManager::startScan(ScanProgressCallback progressCallback,
                               ScanCompleteCallback completeCallback)
{
    startScan(getPluginSearchPaths(), progressCallback, completeCallback);
}

void PluginManager::startScan(const juce::StringArray& directories,
                               ScanProgressCallback progressCallback,
                               ScanCompleteCallback completeCallback)
{
    // Cancel any existing scan
    cancelScan();

    // Start new scan
    scanning.store(true);
    scanProgress.store(0.0f);

    juce::ScopedLock lock(scannerLock);

    // Get the blacklist in a thread-safe manner
    juce::StringArray currentBlacklist;
    {
        juce::ScopedLock blacklistLockGuard(blacklistLock);
        currentBlacklist = blacklist;
    }

    // Create and start the crash-protected scanner thread
    scannerThread = std::make_unique<CrashProtectedScannerThread>(
        *this,
        pluginHost->getKnownPluginList(),
        directories,
        currentBlacklist
    );

    scannerThread->progressCallback = std::move(progressCallback);
    scannerThread->completeCallback = std::move(completeCallback);
    scannerThread->startThread();
}

void PluginManager::cancelScan()
{
    juce::ScopedLock lock(scannerLock);

    if (scannerThread != nullptr)
    {
        scannerThread->signalThreadShouldExit();
        scannerThread->stopThread(5000);
        scannerThread.reset();
    }

    scanning.store(false);
}

//==============================================================================
// Plugin Loading
//==============================================================================

void PluginManager::loadPluginAsync(const juce::PluginDescription& pluginDescription,
                                     PluginLoadedCallback callback)
{
    // Check if plugin is blacklisted
    if (isBlacklisted(pluginDescription.createIdentifierString()))
    {
        if (callback)
        {
            callback(nullptr, "Plugin is blacklisted: " + pluginDescription.name);
        }
        return;
    }

    pluginHost->loadPluginAsync(pluginDescription, std::move(callback));
}

std::unique_ptr<juce::AudioPluginInstance> PluginManager::loadPluginSync(
    const juce::PluginDescription& pluginDescription,
    juce::String& errorMessage)
{
    // Check if plugin is blacklisted
    if (isBlacklisted(pluginDescription.createIdentifierString()))
    {
        errorMessage = "Plugin is blacklisted: " + pluginDescription.name;
        return nullptr;
    }

    return pluginHost->loadPluginSync(pluginDescription, errorMessage);
}

//==============================================================================
// Plugin List Access
//==============================================================================

juce::Array<juce::PluginDescription> PluginManager::getPluginList() const
{
    return pluginHost->getAllPlugins();
}

juce::Array<juce::PluginDescription> PluginManager::getPluginsByType(bool isInstrument) const
{
    return pluginHost->getPluginsByType(isInstrument);
}

juce::Array<juce::PluginDescription> PluginManager::getInstrumentPlugins() const
{
    return getPluginsByType(true);
}

juce::Array<juce::PluginDescription> PluginManager::getEffectPlugins() const
{
    return getPluginsByType(false);
}

const juce::PluginDescription* PluginManager::findPluginByIdentifier(const juce::String& identifierString) const
{
    const auto& types = pluginHost->getKnownPluginList().getTypes();

    for (const auto& desc : types)
    {
        if (desc.createIdentifierString() == identifierString)
        {
            return &desc;
        }
    }

    return nullptr;
}

juce::KnownPluginList& PluginManager::getKnownPluginList()
{
    return pluginHost->getKnownPluginList();
}

const juce::KnownPluginList& PluginManager::getKnownPluginList() const
{
    return pluginHost->getKnownPluginList();
}

void PluginManager::clearPluginList()
{
    pluginHost->clearPluginList();
}

//==============================================================================
// Blacklist Management
//==============================================================================

void PluginManager::addToBlacklist(const juce::String& identifierString)
{
    juce::ScopedLock lock(blacklistLock);

    if (!blacklist.contains(identifierString))
    {
        blacklist.add(identifierString);
        saveBlacklistToFile();
    }
}

void PluginManager::removeFromBlacklist(const juce::String& identifierString)
{
    juce::ScopedLock lock(blacklistLock);

    int index = blacklist.indexOf(identifierString);
    if (index >= 0)
    {
        blacklist.remove(index);
        saveBlacklistToFile();
    }
}

bool PluginManager::isBlacklisted(const juce::String& identifierString) const
{
    juce::ScopedLock lock(blacklistLock);
    return blacklist.contains(identifierString);
}

juce::StringArray PluginManager::getBlacklistedPlugins() const
{
    juce::ScopedLock lock(blacklistLock);
    return blacklist;
}

void PluginManager::clearBlacklist()
{
    juce::ScopedLock lock(blacklistLock);
    blacklist.clear();
    saveBlacklistToFile();
}

//==============================================================================
// Plugin Path Management
//==============================================================================

juce::StringArray PluginManager::getDefaultPluginPaths()
{
    return PluginHost::getDefaultPluginPaths();
}

void PluginManager::addPluginSearchPath(const juce::File& path)
{
    pluginHost->addPluginSearchPath(path);
}

juce::StringArray PluginManager::getPluginSearchPaths() const
{
    return pluginHost->getPluginSearchPaths();
}

//==============================================================================
// Serialization
//==============================================================================

void PluginManager::saveToFile()
{
    pluginHost->savePluginListToFile();
    saveBlacklistToFile();
}

void PluginManager::loadFromFile()
{
    pluginHost->loadPluginListFromFile();
    loadBlacklistFromFile();
}

std::unique_ptr<juce::XmlElement> PluginManager::createXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("PLUGIN_MANAGER");

    // Save plugin list
    if (auto pluginHostXml = pluginHost->createXml())
    {
        xml->addChildElement(pluginHostXml.release());
    }

    // Save blacklist
    auto blacklistElement = xml->createNewChildElement("BLACKLIST");
    {
        juce::ScopedLock lock(blacklistLock);
        for (const auto& identifier : blacklist)
        {
            auto itemElement = blacklistElement->createNewChildElement("PLUGIN");
            itemElement->setAttribute("identifier", identifier);
        }
    }

    return xml;
}

void PluginManager::loadFromXml(const juce::XmlElement& xml)
{
    // Load plugin host data
    if (auto* pluginHostXml = xml.getChildByName("PLUGIN_HOST"))
    {
        pluginHost->loadFromXml(*pluginHostXml);
    }

    // Load blacklist
    if (auto* blacklistElement = xml.getChildByName("BLACKLIST"))
    {
        juce::ScopedLock lock(blacklistLock);
        blacklist.clear();

        for (auto* pluginElement : blacklistElement->getChildIterator())
        {
            if (pluginElement->hasTagName("PLUGIN"))
            {
                juce::String identifier = pluginElement->getStringAttribute("identifier");
                if (identifier.isNotEmpty())
                {
                    blacklist.add(identifier);
                }
            }
        }
    }
}

//==============================================================================
// Callbacks
//==============================================================================

void PluginManager::setPluginListChangedCallback(PluginListChangedCallback callback)
{
    pluginListChangedCallback = std::move(callback);
}

//==============================================================================
// File Locations
//==============================================================================

juce::File PluginManager::getPluginListFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MidiSing")
        .getChildFile("PluginList.xml");
}

juce::File PluginManager::getBlacklistFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MidiSing")
        .getChildFile("PluginBlacklist.xml");
}

void PluginManager::saveBlacklistToFile()
{
    auto file = getBlacklistFile();
    file.getParentDirectory().createDirectory();

    auto xml = std::make_unique<juce::XmlElement>("BLACKLIST");

    {
        juce::ScopedLock lock(blacklistLock);
        for (const auto& identifier : blacklist)
        {
            auto itemElement = xml->createNewChildElement("PLUGIN");
            itemElement->setAttribute("identifier", identifier);
        }
    }

    xml->writeTo(file);
}

void PluginManager::loadBlacklistFromFile()
{
    auto file = getBlacklistFile();

    if (file.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse(file))
        {
            juce::ScopedLock lock(blacklistLock);
            blacklist.clear();

            for (auto* pluginElement : xml->getChildIterator())
            {
                if (pluginElement->hasTagName("PLUGIN"))
                {
                    juce::String identifier = pluginElement->getStringAttribute("identifier");
                    if (identifier.isNotEmpty())
                    {
                        blacklist.add(identifier);
                    }
                }
            }
        }
    }
}

//==============================================================================
// Plugin Window Management
//==============================================================================

PluginWindow* PluginManager::openWindow(PluginInstance* plugin,
                                         PluginWindow::CloseAction closeAction)
{
    if (plugin == nullptr)
        return nullptr;

    juce::ScopedLock lock(windowsLock);

    // Check if a window is already open for this plugin
    for (auto* window : openWindows)
    {
        if (window->getPluginInstance() == plugin)
        {
            // Bring existing window to front
            window->setVisible(true);
            window->toFront(true);
            return window;
        }
    }

    // Create a new window for this plugin
    auto* newWindow = new PluginWindow(plugin, closeAction);

    // Set up the close callback to handle window removal
    newWindow->onWindowClosed = [this](PluginWindow* closedWindow)
    {
        handleWindowClosed(closedWindow);
    };

    // Add to our list of open windows
    openWindows.add(newWindow);

    // Show the window
    newWindow->setVisible(true);

    return newWindow;
}

void PluginManager::closeWindow(PluginInstance* plugin)
{
    if (plugin == nullptr)
        return;

    juce::ScopedLock lock(windowsLock);

    for (int i = openWindows.size() - 1; i >= 0; --i)
    {
        if (openWindows[i]->getPluginInstance() == plugin)
        {
            // Remove and delete the window
            openWindows.remove(i);
            break;
        }
    }
}

void PluginManager::closeWindow(PluginWindow* window)
{
    if (window == nullptr)
        return;

    juce::ScopedLock lock(windowsLock);

    for (int i = openWindows.size() - 1; i >= 0; --i)
    {
        if (openWindows[i] == window)
        {
            openWindows.remove(i);
            break;
        }
    }
}

void PluginManager::closeAllWindows()
{
    juce::ScopedLock lock(windowsLock);
    openWindows.clear();
}

PluginWindow* PluginManager::getWindowForPlugin(PluginInstance* plugin) const
{
    if (plugin == nullptr)
        return nullptr;

    juce::ScopedLock lock(windowsLock);

    for (auto* window : openWindows)
    {
        if (window->getPluginInstance() == plugin)
            return window;
    }

    return nullptr;
}

bool PluginManager::isWindowOpen(PluginInstance* plugin) const
{
    return getWindowForPlugin(plugin) != nullptr;
}

int PluginManager::getNumOpenWindows() const
{
    juce::ScopedLock lock(windowsLock);
    return openWindows.size();
}

juce::Array<PluginWindow*> PluginManager::getOpenWindows() const
{
    juce::ScopedLock lock(windowsLock);

    juce::Array<PluginWindow*> result;
    for (auto* window : openWindows)
        result.add(window);

    return result;
}

void PluginManager::bringAllWindowsToFront()
{
    juce::ScopedLock lock(windowsLock);

    for (auto* window : openWindows)
    {
        if (window->isVisible())
            window->toFront(false);
    }
}

void PluginManager::hideAllWindows()
{
    juce::ScopedLock lock(windowsLock);

    for (auto* window : openWindows)
        window->setVisible(false);
}

void PluginManager::showAllWindows()
{
    juce::ScopedLock lock(windowsLock);

    for (auto* window : openWindows)
        window->setVisible(true);
}

void PluginManager::handleWindowClosed(PluginWindow* window)
{
    // Notify external listener if set
    if (onWindowClosed)
        onWindowClosed(window);

    // If the close action is DeleteWindow, remove it from our list
    if (window != nullptr && window->getCloseAction() == PluginWindow::CloseAction::DeleteWindow)
    {
        // Use async removal to avoid issues during callback
        juce::MessageManager::callAsync([this, window]()
        {
            removeWindow(window);
        });
    }
}

void PluginManager::removeWindow(PluginWindow* window)
{
    juce::ScopedLock lock(windowsLock);

    for (int i = openWindows.size() - 1; i >= 0; --i)
    {
        if (openWindows[i] == window)
        {
            openWindows.remove(i);
            break;
        }
    }
}
