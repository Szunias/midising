#include "AudioTrack.h"

AudioTrack::AudioTrack(const juce::String& name)
    : Track(name, TrackType::Audio)
{
}

void AudioTrack::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    effectChain.prepareToPlay(sampleRate, samplesPerBlock);
}

void AudioTrack::processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    // Create temp buffer for this track's processing
    juce::AudioBuffer<float> trackBuffer(buffer.getNumChannels(), numSamples);
    trackBuffer.clear();

    // Process each region that overlaps with the current time range
    int64_t startPos = static_cast<int64_t>(startSample);
    int64_t endPos = startPos + numSamples;

    bool hasInput = false;

    for (auto& region : regions)
    {
        // Check if region overlaps with current playback range
        if (region->getEndPosition() <= startPos || region->getStartPosition() >= endPos)
            continue; // No overlap

        hasInput = true;

        // Calculate the overlap
        int64_t regionStart = region->getStartPosition();
        int64_t regionEnd = region->getEndPosition();
        
        int64_t copyStart = juce::jmax(startPos, regionStart);
        int64_t copyEnd = juce::jmin(endPos, regionEnd);
        int numToCopy = static_cast<int>(copyEnd - copyStart);

        // Calculate buffer positions
        int bufferWritePos = static_cast<int>(copyStart - startPos);
        int regionReadPos = static_cast<int>(copyStart - regionStart + region->getOffset());

        // Copy from region's audio buffer to TRACK buffer
        const juce::AudioBuffer<float>& regionBuffer = region->getAudioBuffer();
        
        int numChannels = juce::jmin(trackBuffer.getNumChannels(), regionBuffer.getNumChannels());
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (regionReadPos >= 0 && regionReadPos + numToCopy <= regionBuffer.getNumSamples())
            {
                trackBuffer.addFrom(ch, bufferWritePos, regionBuffer, ch, regionReadPos, numToCopy);
            }
        }
    }

    // Apply effects chain if we have input or if effects might generate tail (for now optimize clean tracks)
    if (hasInput || effectChain.getNumEffects() > 0)
    {
        effectChain.processBlock(trackBuffer);
        
        // Sum execution result to main buffer
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            if (ch < trackBuffer.getNumChannels())
            {
                buffer.addFrom(ch, 0, trackBuffer, ch, 0, numSamples);
            }
        }
    }
}

void AudioTrack::releaseResources()
{
    effectChain.releaseResources();
}

void AudioTrack::addRegion(std::unique_ptr<AudioRegion> region)
{
    regions.push_back(std::move(region));
}

void AudioTrack::removeRegion(int index)
{
    if (index >= 0 && index < static_cast<int>(regions.size()))
    {
        regions.erase(regions.begin() + index);
    }
}

AudioRegion* AudioTrack::getRegion(int index)
{
    if (index >= 0 && index < static_cast<int>(regions.size()))
        return regions[static_cast<size_t>(index)].get();
    return nullptr;
}

void AudioTrack::clearRegions()
{
    regions.clear();
}

void AudioTrack::startRecording(int64_t startPosition)
{
    if (recording)
        return;

    recording = true;
    recordingStartPosition = startPosition;
    recordingWritePosition = 0;

    // Pre-allocate buffer for recording (10 minutes at current sample rate)
    int maxSamples = static_cast<int>(currentSampleRate * 60 * 10);
    recordingRegion = std::make_unique<AudioRegion>(startPosition, 0);
    recordingRegion->getAudioBuffer().setSize(2, maxSamples);
    recordingRegion->getAudioBuffer().clear();
}

void AudioTrack::stopRecording()
{
    if (!recording)
        return;

    recording = false;

    if (recordingRegion && recordingWritePosition > 0)
    {
        // Trim buffer to actual recorded length
        recordingRegion->setLength(recordingWritePosition);
        juce::AudioBuffer<float>& buf = recordingRegion->getAudioBuffer();
        
        // Create a properly sized buffer
        juce::AudioBuffer<float> trimmedBuffer(buf.getNumChannels(), recordingWritePosition);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            trimmedBuffer.copyFrom(ch, 0, buf, ch, 0, recordingWritePosition);
        }
        recordingRegion->setAudioBuffer(trimmedBuffer);

        // Add to regions
        regions.push_back(std::move(recordingRegion));
    }

    recordingRegion.reset();
    recordingWritePosition = 0;
}

void AudioTrack::recordSample(const float* leftChannel, const float* rightChannel, int numSamples)
{
    if (!recording || !recordingRegion)
        return;

    juce::AudioBuffer<float>& buf = recordingRegion->getAudioBuffer();
    int bufferSize = buf.getNumSamples();

    int samplesToWrite = juce::jmin(numSamples, bufferSize - recordingWritePosition);
    if (samplesToWrite <= 0)
        return; // Buffer full

    if (leftChannel && buf.getNumChannels() >= 1)
    {
        buf.copyFrom(0, recordingWritePosition, leftChannel, samplesToWrite);
    }
    if (rightChannel && buf.getNumChannels() >= 2)
    {
        buf.copyFrom(1, recordingWritePosition, rightChannel, samplesToWrite);
    }

    recordingWritePosition += samplesToWrite;
}

bool AudioTrack::loadAudioFile(const juce::File& file, int64_t startPosition)
{
    if (!file.existsAsFile())
        return false;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(file));
    
    if (reader == nullptr)
        return false;

    // Read entire file into buffer
    int numSamples = static_cast<int>(reader->lengthInSamples);
    int numChannels = static_cast<int>(reader->numChannels);
    
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    reader->read(&buffer, 0, numSamples, 0, true, true);

    // Create region with loaded audio
    auto region = std::make_unique<AudioRegion>(startPosition, numSamples);
    region->setAudioBuffer(buffer);
    region->setName(file.getFileNameWithoutExtension());
    
    // Add to track
    regions.push_back(std::move(region));
    
    return true;
}

