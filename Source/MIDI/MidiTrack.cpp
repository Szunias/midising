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
