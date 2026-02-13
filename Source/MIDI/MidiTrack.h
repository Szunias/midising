#pragma once

#include "../Timeline/Track.h"
#include "../Timeline/Region.h"
#include "MidiEngine.h"
#include "MidiEffect.h"
#include "PluginHost.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>
#include <atomic>
#include <functional>

// Forward declaration
class PluginHost;

/**
 * MidiTrack is a track that plays MIDI data through a synthesizer.
 * Supports MIDI recording when armed.
 * Can host a VST3 instrument plugin for sound generation.
 */
class MidiTrack : public Track
{
public:
    /**
     * Callback type for async plugin loading.
     * @param success True if plugin loaded successfully
     * @param errorMessage Error message if loading failed
     */
    using PluginLoadedCallback = std::function<void(bool success, const juce::String& errorMessage)>;

    MidiTrack(const juce::String& name = "MIDI Track", MidiEngine* engine = nullptr);
    ~MidiTrack() override;

    // Track interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;

    // Set the MIDI engine to use for synthesis (fallback when no plugin loaded)
    void setMidiEngine(MidiEngine* engine) { midiEngine = engine; }

    // ========== VST3 Plugin Hosting ==========

    /**
     * Set the PluginHost reference for loading plugins.
     * @param host Pointer to the application's PluginHost
     */
    void setPluginHost(PluginHost* host) { pluginHost = host; }

    /**
     * Load a VST3 instrument plugin asynchronously.
     * The plugin will be used for sound generation instead of the built-in synth.
     * @param pluginDescription Description of the plugin to load
     * @param callback Called when loading completes (on message thread)
     */
    void loadPluginAsync(const juce::PluginDescription& pluginDescription,
                         PluginLoadedCallback callback = nullptr);

    /**
     * Load a VST3 instrument plugin synchronously.
     * @param pluginDescription Description of the plugin to load
     * @param errorMessage Output parameter for error message on failure
     * @return True if plugin loaded successfully
     */
    bool loadPluginSync(const juce::PluginDescription& pluginDescription,
                        juce::String& errorMessage);

    /**
     * Unload the current plugin and revert to built-in synth.
     */
    void unloadPlugin();

    /**
     * Check if a plugin is currently loaded.
     * @return True if a VST3 plugin is loaded
     */
    bool hasPlugin() const { return loadedPlugin != nullptr; }

    /**
     * Get the currently loaded plugin instance.
     * @return Pointer to the plugin instance, or nullptr if none loaded
     */
    juce::AudioPluginInstance* getPlugin() { return loadedPlugin.get(); }
    const juce::AudioPluginInstance* getPlugin() const { return loadedPlugin.get(); }

    /**
     * Get the description of the currently loaded plugin.
     * @return Plugin description, or empty description if no plugin loaded
     */
    const juce::PluginDescription& getPluginDescription() const { return loadedPluginDescription; }

    /**
     * Get the plugin's editor component (for showing in UI).
     * @return The plugin's editor, or nullptr if plugin has no editor
     */
    juce::AudioProcessorEditor* createPluginEditor();

    /**
     * Save the current plugin state to memory block.
     * @param destData Output memory block for plugin state
     */
    void savePluginState(juce::MemoryBlock& destData) const;

    /**
     * Load plugin state from memory block.
     * @param data Memory block containing plugin state
     */
    void loadPluginState(const void* data, int sizeInBytes);

    // ========== Deferred Plugin Loading ==========

    /**
     * Store plugin info for deferred loading (used during project restoration).
     * The plugin will be loaded when loadPendingPlugin() is called after
     * setPluginHost() has been called with a valid PluginHost.
     * @param description The plugin description
     * @param stateData The plugin state data (can be empty)
     */
    void setPendingPluginInfo(const juce::PluginDescription& description,
                               const juce::MemoryBlock& stateData);

    /**
     * Check if there's a pending plugin to load.
     */
    bool hasPendingPlugin() const { return pendingPluginDescription.name.isNotEmpty(); }

    /**
     * Get the pending plugin description (for UI display before loading).
     */
    const juce::PluginDescription& getPendingPluginDescription() const { return pendingPluginDescription; }

    /**
     * Load the pending plugin. Call this after setPluginHost() has been called.
     * @param errorMessage Output parameter for error message on failure
     * @return True if plugin loaded successfully
     */
    bool loadPendingPlugin(juce::String& errorMessage);

    /**
     * Clear any pending plugin info without loading.
     */
    void clearPendingPlugin();

    // ========== MIDI Effects Chain ==========

    void addMidiEffect(std::unique_ptr<MidiEffect> effect)
    {
        midiEffects.push_back(std::move(effect));
    }

    void removeMidiEffect(int index)
    {
        if (index >= 0 && index < static_cast<int>(midiEffects.size()))
            midiEffects.erase(midiEffects.begin() + index);
    }

    MidiEffect* getMidiEffect(int index)
    {
        if (index >= 0 && index < static_cast<int>(midiEffects.size()))
            return midiEffects[static_cast<size_t>(index)].get();
        return nullptr;
    }

    const MidiEffect* getMidiEffect(int index) const
    {
        if (index >= 0 && index < static_cast<int>(midiEffects.size()))
            return midiEffects[static_cast<size_t>(index)].get();
        return nullptr;
    }

    int getNumMidiEffects() const { return static_cast<int>(midiEffects.size()); }

    // Region management
    void addRegion(std::unique_ptr<MidiRegion> region);
    void removeRegion(int index);
    MidiRegion* getRegion(int index);
    const MidiRegion* getRegion(int index) const { return regions[static_cast<size_t>(index)].get(); }
    int getNumRegions() const { return static_cast<int>(regions.size()); }
    void clearRegions();

    // Direct note access (for piano roll editing)
    juce::MidiMessageSequence& getMidiSequence() { return midiSequence; }
    const juce::MidiMessageSequence& getMidiSequence() const { return midiSequence; }

    // ========== MIDI Recording ==========

    /**
     * Start recording MIDI input. Track must be armed first.
     * @param startPositionInSamples The timeline position where recording begins
     * @return true if recording started successfully
     */
    bool startRecording(int64_t startPositionInSamples);

    /**
     * Stop recording and create a new MidiRegion with the recorded messages.
     * @return Pointer to the newly created region, or nullptr if no messages were recorded
     */
    MidiRegion* stopRecording();

    /**
     * Check if the track is currently recording.
     */
    bool isRecording() const { return recording.load(); }

    /**
     * Add a MIDI message during recording. Messages are timestamped relative to recording start.
     * @param message The MIDI message to record
     * @param timestampInSamples Absolute timestamp in samples (will be converted to relative)
     */
    void addRecordedMessage(const juce::MidiMessage& message, int64_t timestampInSamples);

    /**
     * Get the recording start position in samples.
     */
    int64_t getRecordingStartPosition() const { return recordingStartPosition.load(); }

    /**
     * Get the number of messages recorded so far.
     */
    int getNumRecordedMessages() const;

private:
    std::vector<std::unique_ptr<MidiRegion>> regions;
    juce::MidiMessageSequence midiSequence; // Master sequence combining all regions
    MidiEngine* midiEngine = nullptr;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    // VST3 Plugin hosting
    PluginHost* pluginHost = nullptr;
    std::unique_ptr<juce::AudioPluginInstance> loadedPlugin;
    juce::PluginDescription loadedPluginDescription;
    mutable juce::CriticalSection pluginLock;  // Protects plugin instance

    // Helper to prepare loaded plugin with current audio settings
    void preparePlugin();

    // Recording state
    std::atomic<bool> recording { false };
    std::atomic<int64_t> recordingStartPosition { 0 };
    juce::MidiMessageSequence recordingBuffer;  // Buffer for incoming MIDI during recording
    mutable juce::CriticalSection recordingLock;  // Protects recordingBuffer (mutable for const methods)

    // Deferred plugin loading (for project restoration)
    juce::PluginDescription pendingPluginDescription;
    juce::MemoryBlock pendingPluginState;

    // MIDI effects chain
    std::vector<std::unique_ptr<MidiEffect>> midiEffects;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiTrack)
};
