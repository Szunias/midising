#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <memory>

/**
 * Wrapper around juce::AudioPluginInstance providing unified state management,
 * A/B comparison, and serialization for VST3/AU plugins.
 *
 * Follows the Effect pattern with prepareToPlay, processBlock, releaseResources.
 *
 * Key features:
 * - A/B state comparison for preset comparisons
 * - Bypass functionality
 * - State serialization with createXml/loadFromXml
 * - Latency reporting for PDC
 * - Thread-safe state management
 */
class PluginInstance
{
public:
    /**
     * State slot identifier for A/B comparison.
     */
    enum class StateSlot
    {
        A,  ///< Primary state slot
        B   ///< Secondary state slot for comparison
    };

    //==========================================================================
    // Construction/Destruction
    //==========================================================================

    /**
     * Create a PluginInstance wrapping an existing AudioPluginInstance.
     * @param instance The plugin instance to wrap (takes ownership)
     * @param description The plugin description for identification
     */
    PluginInstance(std::unique_ptr<juce::AudioPluginInstance> instance,
                   const juce::PluginDescription& description);

    /**
     * Destructor - releases all resources.
     */
    ~PluginInstance();

    // Non-copyable
    PluginInstance(const PluginInstance&) = delete;
    PluginInstance& operator=(const PluginInstance&) = delete;

    // Movable
    PluginInstance(PluginInstance&&) noexcept;
    PluginInstance& operator=(PluginInstance&&) noexcept;

    //==========================================================================
    // Audio Processing
    //==========================================================================

    /**
     * Prepare the plugin for audio processing.
     * @param sampleRate The sample rate to use
     * @param samplesPerBlock Maximum block size
     */
    void prepareToPlay(double sampleRate, int samplesPerBlock);

    /**
     * Process an audio block through the plugin.
     * If bypassed, audio passes through unchanged.
     * @param buffer The audio buffer to process
     * @param midiMessages MIDI messages to process (for instruments)
     */
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    /**
     * Process an audio block through the plugin (no MIDI).
     * Convenience overload for effect plugins.
     * @param buffer The audio buffer to process
     */
    void processBlock(juce::AudioBuffer<float>& buffer);

    /**
     * Release all audio resources.
     */
    void releaseResources();

    //==========================================================================
    // Bypass Control
    //==========================================================================

    /**
     * Set the bypass state.
     * @param shouldBypass true to bypass the plugin
     */
    void setBypass(bool shouldBypass) { bypass = shouldBypass; }

    /**
     * Check if the plugin is bypassed.
     * @return true if bypassed
     */
    bool isBypassed() const { return bypass; }

    //==========================================================================
    // Plugin Information
    //==========================================================================

    /**
     * Get the plugin name.
     * @return Plugin name
     */
    const juce::String& getName() const { return pluginDescription.name; }

    /**
     * Get the plugin description.
     * @return Plugin description
     */
    const juce::PluginDescription& getPluginDescription() const { return pluginDescription; }

    /**
     * Get the plugin's unique identifier string.
     * @return Identifier string suitable for serialization
     */
    juce::String getIdentifierString() const { return pluginDescription.createIdentifierString(); }

    /**
     * Get the manufacturer name.
     * @return Manufacturer name
     */
    const juce::String& getManufacturer() const { return pluginDescription.manufacturerName; }

    /**
     * Check if this is an instrument plugin (VSTi/AUi).
     * @return true if instrument
     */
    bool isInstrument() const { return pluginDescription.isInstrument; }

    /**
     * Get the plugin's latency in samples.
     * @return Latency in samples (for PDC calculations)
     */
    int getLatencySamples() const;

    /**
     * Check if the plugin has a custom editor (GUI).
     * @return true if plugin has custom GUI
     */
    bool hasEditor() const;

    /**
     * Create the plugin's editor component.
     * @return The editor component, or nullptr if none available
     */
    juce::AudioProcessorEditor* createEditor();

    /**
     * Get direct access to the underlying AudioPluginInstance.
     * Use with caution - prefer using PluginInstance methods.
     * @return Pointer to the underlying plugin, or nullptr if invalid
     */
    juce::AudioPluginInstance* getProcessor() { return pluginInstance.get(); }
    const juce::AudioPluginInstance* getProcessor() const { return pluginInstance.get(); }

    //==========================================================================
    // State Management
    //==========================================================================

    /**
     * Get the plugin's current state information.
     * @param destData Output parameter for state data
     */
    void getStateInformation(juce::MemoryBlock& destData) const;

    /**
     * Set the plugin's state from saved data.
     * @param data Pointer to the state data
     * @param sizeInBytes Size of the state data
     */
    void setStateInformation(const void* data, int sizeInBytes);

    /**
     * Set the plugin's state from a MemoryBlock.
     * @param data The state data
     */
    void setStateInformation(const juce::MemoryBlock& data);

    //==========================================================================
    // A/B Comparison
    //==========================================================================

    /**
     * Get the currently active state slot.
     * @return Current slot (A or B)
     */
    StateSlot getCurrentSlot() const { return currentSlot; }

    /**
     * Copy the current plugin state to slot A.
     */
    void saveToSlotA();

    /**
     * Copy the current plugin state to slot B.
     */
    void saveToSlotB();

    /**
     * Copy current state to the B slot (common workflow: tweak settings, then copy to B).
     * Same as saveToSlotB().
     */
    void copyToB();

    /**
     * Swap between A and B states.
     * Saves current state to current slot, then loads the other slot's state.
     */
    void swapAB();

    /**
     * Switch to slot A (saving current state to current slot first).
     */
    void switchToSlotA();

    /**
     * Switch to slot B (saving current state to current slot first).
     */
    void switchToSlotB();

    /**
     * Check if slot A has saved state.
     * @return true if slot A has data
     */
    bool hasSlotAState() const { return stateA.getSize() > 0; }

    /**
     * Check if slot B has saved state.
     * @return true if slot B has data
     */
    bool hasSlotBState() const { return stateB.getSize() > 0; }

    /**
     * Clear the A/B state data.
     */
    void clearABStates();

    //==========================================================================
    // Serialization
    //==========================================================================

    /**
     * Create XML representation of the plugin instance and its state.
     * Includes plugin identifier, current state, A/B states, and bypass state.
     * @return XML element containing plugin data
     */
    std::unique_ptr<juce::XmlElement> createXml() const;

    /**
     * Load plugin state from XML.
     * Does not load the plugin itself - only restores state.
     * @param xml XML element containing plugin data
     */
    void loadFromXml(const juce::XmlElement& xml);

    /**
     * Create a PluginInstance from XML data.
     * Loads the plugin and restores its state.
     * @param xml XML element containing plugin data
     * @param formatManager Format manager for loading the plugin
     * @param errorMessage Output parameter for error messages
     * @param sampleRate Sample rate for preparing the plugin
     * @param blockSize Block size for preparing the plugin
     * @return The created PluginInstance, or nullptr on failure
     */
    static std::unique_ptr<PluginInstance> createFromXml(
        const juce::XmlElement& xml,
        juce::AudioPluginFormatManager& formatManager,
        juce::String& errorMessage,
        double sampleRate = 44100.0,
        int blockSize = 512);

    //==========================================================================
    // Clipboard Operations (for Copy/Paste)
    //==========================================================================

    /**
     * Copy this plugin's description and current state to the clipboard.
     * The clipboard can be used to paste the plugin to another slot.
     */
    void copyToClipboard() const;

    /**
     * Check if the clipboard has plugin data that can be pasted.
     * @return true if clipboard has valid plugin data
     */
    static bool hasClipboardData();

    /**
     * Get the plugin description from the clipboard.
     * @return Pointer to the clipboard plugin description, or nullptr if empty
     */
    static const juce::PluginDescription* getClipboardDescription();

    /**
     * Get the plugin state from the clipboard.
     * @return Reference to the clipboard state MemoryBlock
     */
    static const juce::MemoryBlock& getClipboardState();

    /**
     * Clear the clipboard.
     */
    static void clearClipboard();

    //==========================================================================
    // Validity Check
    //==========================================================================

    /**
     * Check if the plugin instance is valid and ready for use.
     * @return true if valid
     */
    bool isValid() const { return pluginInstance != nullptr; }

    /**
     * Explicit conversion to bool for validity check.
     */
    explicit operator bool() const { return isValid(); }

private:
    //==========================================================================
    // Static Clipboard Storage (for Copy/Paste)
    //==========================================================================

    /** Static clipboard storage for plugin description */
    static juce::PluginDescription clipboardDescription;

    /** Static clipboard storage for plugin state */
    static juce::MemoryBlock clipboardState;

    /** Static flag indicating clipboard has valid data */
    static bool clipboardHasData;

    /** Static lock for thread-safe clipboard access */
    static juce::CriticalSection clipboardLock;

    //==========================================================================
    // Member Variables
    //==========================================================================

    // The underlying plugin instance
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;

    // Plugin identification
    juce::PluginDescription pluginDescription;

    // Audio processing state
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    bool bypass = false;

    // A/B comparison states
    juce::MemoryBlock stateA;
    juce::MemoryBlock stateB;
    StateSlot currentSlot = StateSlot::A;

    // Thread safety for state operations
    mutable juce::CriticalSection stateLock;

    //==========================================================================
    // Private Methods
    //==========================================================================

    /**
     * Save the current plugin state to the specified slot without switching.
     * @param slot The slot to save to
     */
    void saveStateToSlot(StateSlot slot);

    /**
     * Load state from the specified slot into the plugin.
     * @param slot The slot to load from
     * @return true if state was loaded successfully
     */
    bool loadStateFromSlot(StateSlot slot);

    JUCE_LEAK_DETECTOR(PluginInstance)
};
