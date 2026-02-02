#include "MidiTrack.h"
#include "../Utils/TimeConversion.h"

MidiTrack::MidiTrack(const juce::String& name, MidiEngine* engine)
    : Track(name, TrackType::MIDI),
      midiEngine(engine)
{
}

void MidiTrack::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
}

void MidiTrack::processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (midiEngine == nullptr)
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

    // Render MIDI to audio through the engine
    midiEngine->renderNextBlock(buffer, midiBuffer, numSamples);
}

void MidiTrack::releaseResources()
{
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
