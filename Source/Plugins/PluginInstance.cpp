#include "PluginInstance.h"

//==============================================================================
// Static Clipboard Storage Definitions
//==============================================================================

juce::PluginDescription PluginInstance::clipboardDescription;
juce::MemoryBlock PluginInstance::clipboardState;
bool PluginInstance::clipboardHasData = false;
juce::CriticalSection PluginInstance::clipboardLock;

//==============================================================================
// Construction/Destruction
//==============================================================================

PluginInstance::PluginInstance(std::unique_ptr<juce::AudioPluginInstance> instance,
                               const juce::PluginDescription& description)
    : pluginInstance(std::move(instance))
    , pluginDescription(description)
{
    jassert(pluginInstance != nullptr);
}

PluginInstance::~PluginInstance()
{
    releaseResources();
}

PluginInstance::PluginInstance(PluginInstance&& other) noexcept
    : pluginInstance(std::move(other.pluginInstance))
    , pluginDescription(std::move(other.pluginDescription))
    , currentSampleRate(other.currentSampleRate)
    , currentBlockSize(other.currentBlockSize)
    , bypass(other.bypass)
    , stateA(std::move(other.stateA))
    , stateB(std::move(other.stateB))
    , currentSlot(other.currentSlot)
{
}

PluginInstance& PluginInstance::operator=(PluginInstance&& other) noexcept
{
    if (this != &other)
    {
        const juce::ScopedLock sl(stateLock);

        releaseResources();

        pluginInstance = std::move(other.pluginInstance);
        pluginDescription = std::move(other.pluginDescription);
        currentSampleRate = other.currentSampleRate;
        currentBlockSize = other.currentBlockSize;
        bypass = other.bypass;
        stateA = std::move(other.stateA);
        stateB = std::move(other.stateB);
        currentSlot = other.currentSlot;
    }
    return *this;
}

//==============================================================================
// Audio Processing
//==============================================================================

void PluginInstance::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock sl(stateLock);

    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    if (pluginInstance != nullptr)
    {
        pluginInstance->setPlayConfigDetails(
            pluginInstance->getTotalNumInputChannels(),
            pluginInstance->getTotalNumOutputChannels(),
            sampleRate,
            samplesPerBlock);

        pluginInstance->prepareToPlay(sampleRate, samplesPerBlock);
    }
}

void PluginInstance::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (pluginInstance == nullptr || bypass)
    {
        return;
    }

    pluginInstance->processBlock(buffer, midiMessages);
}

void PluginInstance::processBlock(juce::AudioBuffer<float>& buffer)
{
    juce::MidiBuffer emptyMidi;
    processBlock(buffer, emptyMidi);
}

void PluginInstance::releaseResources()
{
    const juce::ScopedLock sl(stateLock);

    if (pluginInstance != nullptr)
    {
        pluginInstance->releaseResources();
    }
}

//==============================================================================
// Plugin Information
//==============================================================================

int PluginInstance::getLatencySamples() const
{
    if (pluginInstance != nullptr)
    {
        return pluginInstance->getLatencySamples();
    }
    return 0;
}

bool PluginInstance::hasEditor() const
{
    if (pluginInstance != nullptr)
    {
        return pluginInstance->hasEditor();
    }
    return false;
}

juce::AudioProcessorEditor* PluginInstance::createEditor()
{
    if (pluginInstance != nullptr)
    {
        return pluginInstance->createEditorIfNeeded();
    }
    return nullptr;
}

//==============================================================================
// State Management
//==============================================================================

void PluginInstance::getStateInformation(juce::MemoryBlock& destData) const
{
    const juce::ScopedLock sl(stateLock);

    if (pluginInstance != nullptr)
    {
        pluginInstance->getStateInformation(destData);
    }
}

void PluginInstance::setStateInformation(const void* data, int sizeInBytes)
{
    const juce::ScopedLock sl(stateLock);

    if (pluginInstance != nullptr && data != nullptr && sizeInBytes > 0)
    {
        pluginInstance->setStateInformation(data, sizeInBytes);
    }
}

void PluginInstance::setStateInformation(const juce::MemoryBlock& data)
{
    setStateInformation(data.getData(), static_cast<int>(data.getSize()));
}

//==============================================================================
// A/B Comparison
//==============================================================================

void PluginInstance::saveToSlotA()
{
    saveStateToSlot(StateSlot::A);
}

void PluginInstance::saveToSlotB()
{
    saveStateToSlot(StateSlot::B);
}

void PluginInstance::copyToB()
{
    saveToSlotB();
}

void PluginInstance::swapAB()
{
    const juce::ScopedLock sl(stateLock);

    // Save current state to current slot
    saveStateToSlot(currentSlot);

    // Switch to the other slot
    StateSlot newSlot = (currentSlot == StateSlot::A) ? StateSlot::B : StateSlot::A;

    // Load the other slot's state
    if (loadStateFromSlot(newSlot))
    {
        currentSlot = newSlot;
    }
}

void PluginInstance::switchToSlotA()
{
    const juce::ScopedLock sl(stateLock);

    if (currentSlot != StateSlot::A)
    {
        // Save current state to current slot
        saveStateToSlot(currentSlot);

        // Load slot A
        if (loadStateFromSlot(StateSlot::A))
        {
            currentSlot = StateSlot::A;
        }
    }
}

void PluginInstance::switchToSlotB()
{
    const juce::ScopedLock sl(stateLock);

    if (currentSlot != StateSlot::B)
    {
        // Save current state to current slot
        saveStateToSlot(currentSlot);

        // Load slot B
        if (loadStateFromSlot(StateSlot::B))
        {
            currentSlot = StateSlot::B;
        }
    }
}

void PluginInstance::clearABStates()
{
    const juce::ScopedLock sl(stateLock);

    stateA.reset();
    stateB.reset();
    currentSlot = StateSlot::A;
}

void PluginInstance::saveStateToSlot(StateSlot slot)
{
    // Note: Caller should hold stateLock if needed for thread safety
    if (pluginInstance == nullptr)
        return;

    juce::MemoryBlock& targetState = (slot == StateSlot::A) ? stateA : stateB;
    targetState.reset();
    pluginInstance->getStateInformation(targetState);
}

bool PluginInstance::loadStateFromSlot(StateSlot slot)
{
    // Note: Caller should hold stateLock if needed for thread safety
    if (pluginInstance == nullptr)
        return false;

    const juce::MemoryBlock& sourceState = (slot == StateSlot::A) ? stateA : stateB;

    if (sourceState.getSize() > 0)
    {
        pluginInstance->setStateInformation(sourceState.getData(),
                                            static_cast<int>(sourceState.getSize()));
        return true;
    }

    return false;
}

//==============================================================================
// Serialization
//==============================================================================

std::unique_ptr<juce::XmlElement> PluginInstance::createXml() const
{
    const juce::ScopedLock sl(stateLock);

    auto xml = std::make_unique<juce::XmlElement>("PLUGIN_INSTANCE");

    // Store plugin identification
    xml->setAttribute("pluginId", pluginDescription.createIdentifierString());
    xml->setAttribute("pluginName", pluginDescription.name);
    xml->setAttribute("manufacturer", pluginDescription.manufacturerName);
    xml->setAttribute("pluginFormat", pluginDescription.pluginFormatName);
    xml->setAttribute("isInstrument", pluginDescription.isInstrument);

    // Store bypass state
    xml->setAttribute("bypass", bypass);

    // Store current slot
    xml->setAttribute("currentSlot", currentSlot == StateSlot::A ? "A" : "B");

    // Store current plugin state as Base64
    if (pluginInstance != nullptr)
    {
        juce::MemoryBlock currentState;
        pluginInstance->getStateInformation(currentState);
        if (currentState.getSize() > 0)
        {
            xml->setAttribute("state", currentState.toBase64Encoding());
        }
    }

    // Store A/B states as Base64 (if they have data)
    if (stateA.getSize() > 0)
    {
        xml->setAttribute("stateA", stateA.toBase64Encoding());
    }

    if (stateB.getSize() > 0)
    {
        xml->setAttribute("stateB", stateB.toBase64Encoding());
    }

    return xml;
}

void PluginInstance::loadFromXml(const juce::XmlElement& xml)
{
    const juce::ScopedLock sl(stateLock);

    // Load bypass state
    bypass = xml.getBoolAttribute("bypass", false);

    // Load current slot
    juce::String slotStr = xml.getStringAttribute("currentSlot", "A");
    currentSlot = (slotStr == "B") ? StateSlot::B : StateSlot::A;

    // Load current state
    juce::String stateBase64 = xml.getStringAttribute("state");
    if (stateBase64.isNotEmpty() && pluginInstance != nullptr)
    {
        juce::MemoryBlock stateData;
        if (stateData.fromBase64Encoding(stateBase64))
        {
            pluginInstance->setStateInformation(stateData.getData(),
                                                static_cast<int>(stateData.getSize()));
        }
    }

    // Load A/B states
    juce::String stateABase64 = xml.getStringAttribute("stateA");
    if (stateABase64.isNotEmpty())
    {
        stateA.fromBase64Encoding(stateABase64);
    }
    else
    {
        stateA.reset();
    }

    juce::String stateBBase64 = xml.getStringAttribute("stateB");
    if (stateBBase64.isNotEmpty())
    {
        stateB.fromBase64Encoding(stateBBase64);
    }
    else
    {
        stateB.reset();
    }
}

std::unique_ptr<PluginInstance> PluginInstance::createFromXml(
    const juce::XmlElement& xml,
    juce::AudioPluginFormatManager& formatManager,
    juce::String& errorMessage,
    double sampleRate,
    int blockSize)
{
    // Get plugin identifier
    juce::String pluginId = xml.getStringAttribute("pluginId");

    if (pluginId.isEmpty())
    {
        errorMessage = "No plugin identifier found in XML";
        return nullptr;
    }

    // Create a plugin description from the identifier
    juce::PluginDescription description;
    bool foundDescription = false;

    // Try to find the plugin in the format manager's known types
    for (auto* format : formatManager.getFormats())
    {
        // Check if we have enough info to recreate the description
        juce::String formatName = xml.getStringAttribute("pluginFormat");
        juce::String pluginName = xml.getStringAttribute("pluginName");
        juce::String manufacturer = xml.getStringAttribute("manufacturer");
        bool isInstrument = xml.getBoolAttribute("isInstrument", false);

        if (format->getName() == formatName)
        {
            description.pluginFormatName = formatName;
            description.name = pluginName;
            description.manufacturerName = manufacturer;
            description.isInstrument = isInstrument;

            // The identifier string contains enough info to find the plugin
            // We need to scan or check the known plugin list
            // For now, attempt to parse from the identifier string

            // Try to use the identifier directly for loading
            juce::KnownPluginList tempList;
            juce::PluginDescription tempDesc;

            // Create description from identifier (this is format-specific)
            // The identifier string format varies by plugin format
            // VST3: unique ID + manufacturer + name
            // For a proper implementation, we should use KnownPluginList

            // Fallback: reconstruct what we can from stored attributes
            description.fileOrIdentifier = pluginId;
            foundDescription = true;
            break;
        }
    }

    if (!foundDescription)
    {
        // Try to parse the identifier string directly
        // This is a fallback when we don't have format info
        description.fileOrIdentifier = pluginId;
        description.name = xml.getStringAttribute("pluginName", "Unknown");
        description.manufacturerName = xml.getStringAttribute("manufacturer", "Unknown");
        description.pluginFormatName = xml.getStringAttribute("pluginFormat", "VST3");
        description.isInstrument = xml.getBoolAttribute("isInstrument", false);
    }

    // Try to load the plugin
    std::unique_ptr<juce::AudioPluginInstance> plugin;

    formatManager.createPluginInstanceAsync(
        description,
        sampleRate,
        blockSize,
        [&plugin, &errorMessage](std::unique_ptr<juce::AudioPluginInstance> instance,
                                  const juce::String& error)
        {
            plugin = std::move(instance);
            errorMessage = error;
        });

    // Note: For synchronous behavior, we need to wait for the callback
    // In practice, this should use a proper async pattern
    // For now, try synchronous loading as a fallback

    if (plugin == nullptr)
    {
        // Try synchronous loading
        plugin = formatManager.createPluginInstance(description, sampleRate, blockSize, errorMessage);
    }

    if (plugin == nullptr)
    {
        if (errorMessage.isEmpty())
        {
            errorMessage = "Failed to load plugin: " + description.name;
        }
        return nullptr;
    }

    // Create the PluginInstance wrapper
    auto instance = std::make_unique<PluginInstance>(std::move(plugin), description);

    // Prepare for playback
    instance->prepareToPlay(sampleRate, blockSize);

    // Load state from XML
    instance->loadFromXml(xml);

    return instance;
}

//==============================================================================
// Clipboard Operations
//==============================================================================

void PluginInstance::copyToClipboard() const
{
    const juce::ScopedLock sl(clipboardLock);

    // Copy the plugin description
    clipboardDescription = pluginDescription;

    // Copy the current state
    clipboardState.reset();
    if (pluginInstance != nullptr)
    {
        pluginInstance->getStateInformation(clipboardState);
    }

    // Mark clipboard as having valid data
    clipboardHasData = true;
}

bool PluginInstance::hasClipboardData()
{
    const juce::ScopedLock sl(clipboardLock);
    return clipboardHasData;
}

const juce::PluginDescription* PluginInstance::getClipboardDescription()
{
    const juce::ScopedLock sl(clipboardLock);
    if (clipboardHasData)
    {
        return &clipboardDescription;
    }
    return nullptr;
}

const juce::MemoryBlock& PluginInstance::getClipboardState()
{
    const juce::ScopedLock sl(clipboardLock);
    return clipboardState;
}

void PluginInstance::clearClipboard()
{
    const juce::ScopedLock sl(clipboardLock);
    clipboardDescription = juce::PluginDescription();
    clipboardState.reset();
    clipboardHasData = false;
}
