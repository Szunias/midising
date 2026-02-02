#include "PluginHost.h"

//==============================================================================
// PluginScannerThread Implementation
//==============================================================================

PluginHost::PluginScannerThread::PluginScannerThread(PluginHost& owner,
                                                      const juce::StringArray& paths)
    : Thread("Plugin Scanner"),
      owner(owner),
      pathsToScan(paths)
{
}

PluginHost::PluginScannerThread::~PluginScannerThread()
{
    stopThread(5000);
}

void PluginHost::PluginScannerThread::run()
{
    // Get the VST3 format from the format manager
    juce::AudioPluginFormat* vst3Format = nullptr;
    for (int i = 0; i < owner.formatManager.getNumFormats(); ++i)
    {
        auto* format = owner.formatManager.getFormat(i);
        if (format->getName() == "VST3")
        {
            vst3Format = format;
            break;
        }
    }

    if (vst3Format == nullptr)
    {
        owner.scanning.store(false);
        if (completeCallback)
        {
            juce::MessageManager::callAsync([callback = completeCallback]()
            {
                callback();
            });
        }
        return;
    }

    // Collect all plugin files to scan
    juce::StringArray filesToScan;
    for (const auto& path : pathsToScan)
    {
        juce::File dir(path);
        if (dir.isDirectory())
        {
            auto files = dir.findChildFiles(juce::File::findFiles, true, "*.vst3");
            for (const auto& file : files)
            {
                filesToScan.add(file.getFullPathName());
            }
        }
    }

    // Scan each file
    int totalFiles = filesToScan.size();
    int currentFile = 0;

    for (const auto& filePath : filesToScan)
    {
        if (threadShouldExit())
            break;

        juce::File file(filePath);
        juce::String currentPluginName = file.getFileNameWithoutExtension();

        // Update progress
        float progress = totalFiles > 0 ? static_cast<float>(currentFile) / static_cast<float>(totalFiles) : 0.0f;
        owner.scanProgress.store(progress);

        if (progressCallback)
        {
            juce::MessageManager::callAsync([callback = progressCallback, progress, currentPluginName]()
            {
                callback(progress, currentPluginName);
            });
        }

        // Scan this plugin file
        juce::OwnedArray<juce::PluginDescription> results;
        vst3Format->findAllTypesForFile(results, filePath);

        // Add discovered plugins to the known plugin list
        {
            juce::ScopedLock lock(owner.scanLock);
            for (auto* desc : results)
            {
                owner.knownPluginList.addType(*desc);
            }
        }

        currentFile++;
    }

    // Mark scanning complete
    owner.scanProgress.store(1.0f);
    owner.scanning.store(false);

    // Call complete callback on the message thread
    if (completeCallback)
    {
        juce::MessageManager::callAsync([callback = completeCallback]()
        {
            callback();
        });
    }
}

//==============================================================================
// PluginHost Implementation
//==============================================================================

PluginHost::PluginHost()
{
    // Add VST3 format support
    formatManager.addDefaultFormats();

    // Listen for changes to the known plugin list
    knownPluginList.addChangeListener(this);

    // Initialize default search paths
    pluginSearchPaths = getDefaultPluginPaths();

    // Try to load cached plugin list
    loadPluginListFromFile();
}

PluginHost::~PluginHost()
{
    // Cancel any ongoing scan
    cancelPluginScan();

    // Remove change listener
    knownPluginList.removeChangeListener(this);

    // Save the plugin list before destruction
    savePluginListToFile();
}

//==============================================================================
// Audio Processing Setup
//==============================================================================

void PluginHost::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
}

void PluginHost::releaseResources()
{
    // Nothing to release in the host itself
    // Individual plugin instances manage their own resources
}

//==============================================================================
// Plugin Scanning
//==============================================================================

void PluginHost::startPluginScan(ScanProgressCallback progressCallback,
                                  ScanCompleteCallback completeCallback)
{
    startPluginScan(pluginSearchPaths, progressCallback, completeCallback);
}

void PluginHost::startPluginScan(const juce::StringArray& directories,
                                  ScanProgressCallback progressCallback,
                                  ScanCompleteCallback completeCallback)
{
    // Cancel any existing scan
    cancelPluginScan();

    // Start new scan
    scanning.store(true);
    scanProgress.store(0.0f);

    scannerThread = std::make_unique<PluginScannerThread>(*this, directories);
    scannerThread->progressCallback = std::move(progressCallback);
    scannerThread->completeCallback = std::move(completeCallback);
    scannerThread->startThread();
}

void PluginHost::cancelPluginScan()
{
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

void PluginHost::loadPluginAsync(const juce::PluginDescription& pluginDescription,
                                  PluginLoadedCallback callback)
{
    // Use JUCE's async plugin loading to avoid blocking the UI
    formatManager.createPluginInstanceAsync(
        pluginDescription,
        currentSampleRate,
        currentBlockSize,
        [callback = std::move(callback)](std::unique_ptr<juce::AudioPluginInstance> instance,
                                          const juce::String& errorMessage)
        {
            // This callback is called on the message thread
            callback(std::move(instance), errorMessage);
        });
}

std::unique_ptr<juce::AudioPluginInstance> PluginHost::loadPluginSync(
    const juce::PluginDescription& pluginDescription,
    juce::String& errorMessage)
{
    return formatManager.createPluginInstance(
        pluginDescription,
        currentSampleRate,
        currentBlockSize,
        errorMessage);
}

//==============================================================================
// Known Plugin List
//==============================================================================

juce::Array<juce::PluginDescription> PluginHost::getPluginsByType(bool isInstrument) const
{
    juce::Array<juce::PluginDescription> result;

    for (const auto& desc : knownPluginList.getTypes())
    {
        if (desc.isInstrument == isInstrument)
        {
            result.add(desc);
        }
    }

    return result;
}

juce::Array<juce::PluginDescription> PluginHost::getAllPlugins() const
{
    return knownPluginList.getTypes();
}

void PluginHost::clearPluginList()
{
    juce::ScopedLock lock(scanLock);
    knownPluginList.clear();
}

//==============================================================================
// Plugin Path Management
//==============================================================================

juce::StringArray PluginHost::getDefaultPluginPaths()
{
    juce::StringArray paths;

#if JUCE_WINDOWS
    // Windows VST3 locations
    paths.add("C:\\Program Files\\Common Files\\VST3");
    paths.add("C:\\Program Files (x86)\\Common Files\\VST3");

    // Also check user-specific location
    juce::File userVst3(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("VST3"));
    if (userVst3.isDirectory())
        paths.add(userVst3.getFullPathName());

#elif JUCE_MAC
    // macOS VST3 locations
    paths.add("/Library/Audio/Plug-Ins/VST3");

    // User-specific location
    juce::File userVst3(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                            .getChildFile("Library/Audio/Plug-Ins/VST3"));
    if (userVst3.isDirectory())
        paths.add(userVst3.getFullPathName());

#elif JUCE_LINUX
    // Linux VST3 locations
    juce::File userVst3(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                            .getChildFile(".vst3"));
    if (userVst3.isDirectory())
        paths.add(userVst3.getFullPathName());

    paths.add("/usr/lib/vst3");
    paths.add("/usr/local/lib/vst3");
#endif

    return paths;
}

void PluginHost::addPluginSearchPath(const juce::File& path)
{
    if (path.isDirectory())
    {
        juce::String pathStr = path.getFullPathName();
        if (!pluginSearchPaths.contains(pathStr))
        {
            pluginSearchPaths.add(pathStr);
        }
    }
}

//==============================================================================
// Serialization
//==============================================================================

std::unique_ptr<juce::XmlElement> PluginHost::createXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("PLUGIN_HOST");

    // Save known plugin list
    if (auto pluginListXml = knownPluginList.createXml())
    {
        xml->addChildElement(pluginListXml.release());
    }

    // Save custom search paths
    auto pathsElement = xml->createNewChildElement("SEARCH_PATHS");
    for (const auto& path : pluginSearchPaths)
    {
        auto pathElement = pathsElement->createNewChildElement("PATH");
        pathElement->setAttribute("value", path);
    }

    return xml;
}

void PluginHost::loadFromXml(const juce::XmlElement& xml)
{
    // Load known plugin list
    if (auto* pluginListXml = xml.getChildByName("KNOWNPLUGINS"))
    {
        knownPluginList.recreateFromXml(*pluginListXml);
    }

    // Load custom search paths
    if (auto* pathsElement = xml.getChildByName("SEARCH_PATHS"))
    {
        pluginSearchPaths.clear();

        for (auto* pathElement : pathsElement->getChildIterator())
        {
            if (pathElement->hasTagName("PATH"))
            {
                juce::String path = pathElement->getStringAttribute("value");
                if (path.isNotEmpty())
                {
                    pluginSearchPaths.add(path);
                }
            }
        }

        // Ensure default paths are always included
        auto defaultPaths = getDefaultPluginPaths();
        for (const auto& path : defaultPaths)
        {
            if (!pluginSearchPaths.contains(path))
            {
                pluginSearchPaths.add(path);
            }
        }
    }
}

juce::File PluginHost::getPluginListFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MidiSing")
        .getChildFile("PluginList.xml");
}

void PluginHost::savePluginListToFile()
{
    auto file = getPluginListFile();

    // Create directory if it doesn't exist
    file.getParentDirectory().createDirectory();

    if (auto xml = createXml())
    {
        xml->writeTo(file);
    }
}

void PluginHost::loadPluginListFromFile()
{
    auto file = getPluginListFile();

    if (file.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse(file))
        {
            loadFromXml(*xml);
        }
    }
}

//==============================================================================
// Change Listener
//==============================================================================

void PluginHost::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &knownPluginList)
    {
        // Save the updated plugin list
        savePluginListToFile();

        // Notify listeners
        if (pluginListChangedCallback)
        {
            pluginListChangedCallback();
        }
    }
}
