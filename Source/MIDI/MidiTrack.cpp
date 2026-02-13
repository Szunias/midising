#include "MidiTrack.h"
#include "../Utils/TimeConversion.h"

MidiTrack::MidiTrack(const juce::String& name, MidiEngine* engine)
    : Track(name, TrackType::MIDI),
      midiEngine(engine)
{
}

MidiTrack::~MidiTrack()
{
    // Release plugin resources before destruction
    unloadPlugin();
}

void MidiTrack::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    // Prepare any loaded plugin with the new settings
    preparePlugin();
}

void MidiTrack::processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    // Need either a plugin or MidiEngine to render audio
    bool hasPluginLoaded = false;
    {
        juce::ScopedLock sl(pluginLock);
        hasPluginLoaded = (loadedPlugin != nullptr);
    }

    if (!hasPluginLoaded && midiEngine == nullptr)
        return;

    int64_t startPos = static_cast<int64_t>(startSample);
    int64_t endPos = startPos + numSamples;

    // Collect MIDI events from all regions that fall within this time range
    juce::MidiBuffer midiBuffer;

    for (auto& region : regions)
    {
        // Check if region overlaps with current playback range
        if (region->getEndPosition() <= startPos || region->getStartPosition() >= endPos)
            continue;

        const juce::MidiMessageSequence& seq = region->getMidiSequence();
        int64_t regionStart = region->getStartPosition();

        // Iterate through MIDI events in the region
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event == nullptr)
                continue;

            // Calculate absolute position of this event
            int64_t eventTimeInSamples = regionStart +
                static_cast<int64_t>(event->message.getTimeStamp());

            // Check if event falls within current block
            if (eventTimeInSamples >= startPos && eventTimeInSamples < endPos)
            {
                int sampleOffsetInBlock = static_cast<int>(eventTimeInSamples - startPos);
                midiBuffer.addEvent(event->message, sampleOffsetInBlock);
            }
        }
    }

    // Apply MIDI effects chain
    for (auto& effect : midiEffects)
    {
        if (effect != nullptr && !effect->isBypassed())
            effect->processMidi(midiBuffer, numSamples, currentSampleRate, 120.0);
    }

    // Render MIDI to audio
    if (hasPluginLoaded)
    {
        // Route through the VST3 plugin
        juce::ScopedLock sl(pluginLock);
        if (loadedPlugin != nullptr)
        {
            loadedPlugin->processBlock(buffer, midiBuffer);
        }
    }
    else if (midiEngine != nullptr)
    {
        // Fallback to built-in synth
        midiEngine->renderNextBlock(buffer, midiBuffer, numSamples);
    }
}

void MidiTrack::releaseResources()
{
    juce::ScopedLock sl(pluginLock);
    if (loadedPlugin != nullptr)
    {
        loadedPlugin->releaseResources();
    }
}

void MidiTrack::addRegion(std::unique_ptr<MidiRegion> region)
{
    regions.push_back(std::move(region));
}

void MidiTrack::removeRegion(int index)
{
    if (index >= 0 && index < static_cast<int>(regions.size()))
    {
        regions.erase(regions.begin() + index);
    }
}

MidiRegion* MidiTrack::getRegion(int index)
{
    if (index >= 0 && index < static_cast<int>(regions.size()))
        return regions[static_cast<size_t>(index)].get();
    return nullptr;
}

void MidiTrack::clearRegions()
{
    regions.clear();
}

// ========== MIDI Recording Implementation ==========

bool MidiTrack::startRecording(int64_t startPositionInSamples)
{
    // Can only start recording if track is armed
    if (!isArmed())
        return false;

    // Stop any existing recording first
    if (recording.load())
        stopRecording();

    // Clear the recording buffer
    {
        juce::ScopedLock sl(recordingLock);
        recordingBuffer.clear();
    }

    // Set up recording state
    recordingStartPosition.store(startPositionInSamples);
    recording.store(true);

    return true;
}

MidiRegion* MidiTrack::stopRecording()
{
    if (!recording.load())
        return nullptr;

    recording.store(false);

    // Create a new region from the recorded messages
    juce::MidiMessageSequence capturedSequence;
    int64_t startPos = recordingStartPosition.load();
    int64_t endPos = startPos;

    {
        juce::ScopedLock sl(recordingLock);

        if (recordingBuffer.getNumEvents() == 0)
        {
            recordingBuffer.clear();
            return nullptr;
        }

        // Copy the recorded messages
        capturedSequence = recordingBuffer;

        // Find the end position (last event timestamp + some buffer)
        for (int i = 0; i < recordingBuffer.getNumEvents(); ++i)
        {
            auto* event = recordingBuffer.getEventPointer(i);
            if (event != nullptr)
            {
                int64_t eventEndTime = startPos + static_cast<int64_t>(event->message.getTimeStamp());
                if (eventEndTime > endPos)
                    endPos = eventEndTime;
            }
        }

        // Clear the recording buffer
        recordingBuffer.clear();
    }

    // Add some padding after the last note (0.5 seconds worth of samples)
    int64_t paddingSamples = static_cast<int64_t>(currentSampleRate * 0.5);
    int64_t regionLength = (endPos - startPos) + paddingSamples;

    // Ensure minimum region length (1 second)
    int64_t minLength = static_cast<int64_t>(currentSampleRate);
    if (regionLength < minLength)
        regionLength = minLength;

    // Create the new region
    auto newRegion = std::make_unique<MidiRegion>(startPos, regionLength);
    newRegion->setName("Recorded MIDI");
    newRegion->setMidiSequence(capturedSequence);

    // Add to regions and return pointer
    MidiRegion* regionPtr = newRegion.get();
    addRegion(std::move(newRegion));

    return regionPtr;
}

void MidiTrack::addRecordedMessage(const juce::MidiMessage& message, int64_t timestampInSamples)
{
    if (!recording.load())
        return;

    // Convert absolute timestamp to relative (from recording start)
    int64_t startPos = recordingStartPosition.load();
    double relativeTimestamp = static_cast<double>(timestampInSamples - startPos);

    // Clamp to non-negative (in case message arrives before recording start position)
    if (relativeTimestamp < 0.0)
        relativeTimestamp = 0.0;

    // Create a copy of the message with the relative timestamp
    juce::MidiMessage timestampedMessage = message;
    timestampedMessage.setTimeStamp(relativeTimestamp);

    // Add to buffer (thread-safe)
    {
        juce::ScopedLock sl(recordingLock);
        recordingBuffer.addEvent(timestampedMessage);
    }
}

int MidiTrack::getNumRecordedMessages() const
{
    juce::ScopedLock sl(recordingLock);
    return recordingBuffer.getNumEvents();
}

// ========== VST3 Plugin Hosting Implementation ==========

void MidiTrack::loadPluginAsync(const juce::PluginDescription& pluginDescription,
                                 PluginLoadedCallback callback)
{
    if (pluginHost == nullptr)
    {
        if (callback)
            callback(false, "PluginHost not set");
        return;
    }

    // Store description for later reference
    juce::PluginDescription descCopy = pluginDescription;

    pluginHost->loadPluginAsync(pluginDescription,
        [this, callback, descCopy](std::unique_ptr<juce::AudioPluginInstance> instance,
                                    const juce::String& errorMessage)
        {
            if (instance != nullptr)
            {
                // Unload any existing plugin first
                unloadPlugin();

                // Store the new plugin
                {
                    juce::ScopedLock sl(pluginLock);
                    loadedPlugin = std::move(instance);
                    loadedPluginDescription = descCopy;
                }

                // Prepare with current audio settings
                preparePlugin();

                if (callback)
                    callback(true, juce::String());
            }
            else
            {
                if (callback)
                    callback(false, errorMessage);
            }
        });
}

bool MidiTrack::loadPluginSync(const juce::PluginDescription& pluginDescription,
                                juce::String& errorMessage)
{
    if (pluginHost == nullptr)
    {
        errorMessage = "PluginHost not set";
        return false;
    }

    auto instance = pluginHost->loadPluginSync(pluginDescription, errorMessage);

    if (instance != nullptr)
    {
        // Unload any existing plugin first
        unloadPlugin();

        // Store the new plugin
        {
            juce::ScopedLock sl(pluginLock);
            loadedPlugin = std::move(instance);
            loadedPluginDescription = pluginDescription;
        }

        // Prepare with current audio settings
        preparePlugin();

        return true;
    }

    return false;
}

void MidiTrack::unloadPlugin()
{
    juce::ScopedLock sl(pluginLock);

    if (loadedPlugin != nullptr)
    {
        loadedPlugin->releaseResources();
        loadedPlugin.reset();
        loadedPluginDescription = juce::PluginDescription();
    }
}

juce::AudioProcessorEditor* MidiTrack::createPluginEditor()
{
    juce::ScopedLock sl(pluginLock);

    if (loadedPlugin != nullptr && loadedPlugin->hasEditor())
    {
        return loadedPlugin->createEditor();
    }

    return nullptr;
}

void MidiTrack::savePluginState(juce::MemoryBlock& destData) const
{
    juce::ScopedLock sl(pluginLock);

    if (loadedPlugin != nullptr)
    {
        loadedPlugin->getStateInformation(destData);
    }
}

void MidiTrack::loadPluginState(const void* data, int sizeInBytes)
{
    juce::ScopedLock sl(pluginLock);

    if (loadedPlugin != nullptr && data != nullptr && sizeInBytes > 0)
    {
        loadedPlugin->setStateInformation(data, sizeInBytes);
    }
}

void MidiTrack::preparePlugin()
{
    juce::ScopedLock sl(pluginLock);

    if (loadedPlugin != nullptr)
    {
        // Set up bus layout for stereo output
        loadedPlugin->setPlayConfigDetails(
            0,  // No audio inputs (MIDI instrument)
            2,  // Stereo output
            currentSampleRate,
            currentBlockSize);

        loadedPlugin->prepareToPlay(currentSampleRate, currentBlockSize);
    }
}

// ========== Deferred Plugin Loading Implementation ==========

void MidiTrack::setPendingPluginInfo(const juce::PluginDescription& description,
                                      const juce::MemoryBlock& stateData)
{
    pendingPluginDescription = description;
    pendingPluginState = stateData;
}

bool MidiTrack::loadPendingPlugin(juce::String& errorMessage)
{
    if (!hasPendingPlugin())
    {
        errorMessage = "No pending plugin to load";
        return false;
    }

    if (pluginHost == nullptr)
    {
        errorMessage = "PluginHost not set - call setPluginHost() first";
        return false;
    }

    // Try to load the plugin synchronously
    bool success = loadPluginSync(pendingPluginDescription, errorMessage);

    if (success && pendingPluginState.getSize() > 0)
    {
        // Restore the plugin state
        loadPluginState(pendingPluginState.getData(),
                        static_cast<int>(pendingPluginState.getSize()));
    }

    // Clear pending info regardless of success
    clearPendingPlugin();

    return success;
}

void MidiTrack::clearPendingPlugin()
{
    pendingPluginDescription = juce::PluginDescription();
    pendingPluginState.reset();
}
