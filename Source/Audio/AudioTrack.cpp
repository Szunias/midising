#include "AudioTrack.h"
#include <cmath>

AudioTrack::AudioTrack(const juce::String& name)
    : Track(name, TrackType::Audio)
{
}

void AudioTrack::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    effectChain.prepareToPlay(sampleRate, samplesPerBlock);

    // Pre-allocate monitoring buffer
    monitoringBuffer.setSize(2, samplesPerBlock);
    monitoringBuffer.clear();
    monitoringBufferValid.store(false);
    monitoringBufferSamples = 0;
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

        // Apply fade in/out envelopes
        int64_t fadeInLen = region->getFadeInLength();
        int64_t fadeOutLen = region->getFadeOutLength();

        if (fadeInLen > 0 || fadeOutLen > 0)
        {
            // Apply fades sample by sample for the copied range
            for (int i = 0; i < numToCopy; ++i)
            {
                // Position within the region (relative to region start)
                int64_t posInRegion = copyStart - regionStart + i;

                float gain = 1.0f;

                // Apply fade in (linear ramp from 0 to 1)
                if (fadeInLen > 0 && posInRegion < fadeInLen)
                {
                    gain *= static_cast<float>(posInRegion) / static_cast<float>(fadeInLen);
                }

                // Apply fade out (linear ramp from 1 to 0)
                int64_t fadeOutStart = region->getLength() - fadeOutLen;
                if (fadeOutLen > 0 && posInRegion >= fadeOutStart)
                {
                    int64_t posInFadeOut = posInRegion - fadeOutStart;
                    gain *= 1.0f - (static_cast<float>(posInFadeOut) / static_cast<float>(fadeOutLen));
                }

                // Apply gain to all channels at this sample position
                if (gain < 1.0f)
                {
                    int bufferPos = bufferWritePos + i;
                    for (int ch = 0; ch < trackBuffer.getNumChannels(); ++ch)
                    {
                        float* samples = trackBuffer.getWritePointer(ch);
                        samples[bufferPos] *= gain;
                    }
                }
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
        // Apply latency compensation - shift region start position backward
        // This compensates for the audio buffer latency during recording
        if (latencyCompensationSamples > 0)
        {
            int64_t currentStart = recordingRegion->getStartPosition();
            int64_t compensatedStart = currentStart - latencyCompensationSamples;

            // Don't allow negative start positions
            if (compensatedStart < 0)
            {
                compensatedStart = 0;
            }

            recordingRegion->setStartPosition(compensatedStart);
        }

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

void AudioTrack::setInputChannels(int leftChannel, int rightChannel)
{
    inputConfig.leftChannel = juce::jmax(0, leftChannel);

    if (rightChannel < 0)
    {
        // Mono input - use same channel for both
        inputConfig.rightChannel = -1;
        inputConfig.isMono = true;
    }
    else
    {
        inputConfig.rightChannel = rightChannel;
        inputConfig.isMono = false;
    }
}

void AudioTrack::setInputChannelConfig(const InputChannelConfig& config)
{
    inputConfig = config;
}

InputChannelConfig AudioTrack::getInputChannelConfig() const
{
    return inputConfig;
}

void AudioTrack::processInputSignal(const juce::AudioBuffer<float>& inputBuffer, int numSamples)
{
    // Only process input when track is armed
    if (!isArmed())
    {
        // Decay input levels when not armed
        inputLevel.store(inputLevel.load() * levelDecayRate);
        inputLevelLeft.store(inputLevelLeft.load() * levelDecayRate);
        inputLevelRight.store(inputLevelRight.load() * levelDecayRate);
        monitoringBufferValid.store(false);
        return;
    }

    const int numInputChannels = inputBuffer.getNumChannels();

    // Get input from configured channels
    const float* leftInput = nullptr;
    const float* rightInput = nullptr;

    if (inputConfig.leftChannel >= 0 && inputConfig.leftChannel < numInputChannels)
    {
        leftInput = inputBuffer.getReadPointer(inputConfig.leftChannel);
    }

    if (!inputConfig.isMono && inputConfig.rightChannel >= 0 && inputConfig.rightChannel < numInputChannels)
    {
        rightInput = inputBuffer.getReadPointer(inputConfig.rightChannel);
    }
    else if (inputConfig.isMono && leftInput != nullptr)
    {
        // Mono: use left channel for both
        rightInput = leftInput;
    }

    // Calculate peak levels for metering
    float peakLeft = 0.0f;
    float peakRight = 0.0f;

    if (leftInput != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float absVal = std::abs(leftInput[i]);
            if (absVal > peakLeft)
                peakLeft = absVal;
        }
    }

    if (rightInput != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float absVal = std::abs(rightInput[i]);
            if (absVal > peakRight)
                peakRight = absVal;
        }
    }

    // Update input levels with peak hold and decay
    float currentLeftLevel = inputLevelLeft.load();
    float currentRightLevel = inputLevelRight.load();

    // Use peak if higher, otherwise decay
    float newLeftLevel = (peakLeft > currentLeftLevel) ? peakLeft : (currentLeftLevel * levelDecayRate);
    float newRightLevel = (peakRight > currentRightLevel) ? peakRight : (currentRightLevel * levelDecayRate);

    inputLevelLeft.store(newLeftLevel);
    inputLevelRight.store(newRightLevel);

    // Combined input level is the max of left and right
    inputLevel.store(juce::jmax(newLeftLevel, newRightLevel));

    // Fill monitoring buffer if input monitoring is enabled
    if (inputMonitoringEnabled.load())
    {
        // Ensure monitoring buffer is large enough
        if (monitoringBuffer.getNumSamples() < numSamples)
        {
            monitoringBuffer.setSize(2, numSamples, false, false, true);
        }

        monitoringBuffer.clear();

        // Copy input to monitoring buffer
        if (leftInput != nullptr)
        {
            monitoringBuffer.copyFrom(0, 0, leftInput, numSamples);
        }
        if (rightInput != nullptr)
        {
            monitoringBuffer.copyFrom(1, 0, rightInput, numSamples);
        }
        else if (leftInput != nullptr)
        {
            // If no right input but have left, copy left to right (mono to stereo)
            monitoringBuffer.copyFrom(1, 0, leftInput, numSamples);
        }

        monitoringBufferSamples = numSamples;
        monitoringBufferValid.store(true);
    }
    else
    {
        monitoringBufferValid.store(false);
    }
}

bool AudioTrack::getMonitoringBuffer(juce::AudioBuffer<float>& outputBuffer) const
{
    if (!monitoringBufferValid.load() || monitoringBufferSamples <= 0)
    {
        return false;
    }

    // Copy monitoring buffer to output, applying track volume and pan
    int numSamples = juce::jmin(monitoringBufferSamples, outputBuffer.getNumSamples());
    int numChannels = juce::jmin(2, outputBuffer.getNumChannels());

    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (ch < monitoringBuffer.getNumChannels())
        {
            // Apply track volume
            outputBuffer.addFrom(ch, 0, monitoringBuffer, ch, 0, numSamples, getVolume());
        }
    }

    return true;
}

