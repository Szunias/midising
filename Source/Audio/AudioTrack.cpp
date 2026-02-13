#include "AudioTrack.h"
#include "../Timeline/AutomationLane.h"
#include <cmath>
#include <algorithm>

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

    // Prepare time stretch and pitch shift processors
    timeStretchProcessor.prepare(sampleRate, samplesPerBlock);
    pitchShiftProcessor.prepare(sampleRate, samplesPerBlock);

    // Pre-allocate processing buffers for time stretch/pitch shift
    // Use larger buffer to accommodate extreme stretch ratios (up to 4x)
    int maxProcessingSize = samplesPerBlock * 8;
    processingBuffer.setSize(2, maxProcessingSize);
    stretchInputBuffer.setSize(2, maxProcessingSize);
    stretchOutputBuffer.setSize(2, maxProcessingSize);
    processingBuffer.clear();
    stretchInputBuffer.clear();
    stretchOutputBuffer.clear();

    // Reset processing state
    lastProcessedRegionId = -1;
    lastProcessedPosition = -1;
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

    // First pass: collect active regions and sort by start position for crossfade detection
    std::vector<AudioRegion*> activeRegions;
    for (auto& region : regions)
    {
        if (region->getEndPosition() > startPos && region->getStartPosition() < endPos)
        {
            activeRegions.push_back(region.get());
        }
    }

    // Sort by start position for proper overlap detection
    std::sort(activeRegions.begin(), activeRegions.end(),
              [](const AudioRegion* a, const AudioRegion* b) {
                  return a->getStartPosition() < b->getStartPosition();
              });

    // Process each active region with automatic crossfade support
    for (size_t regionIdx = 0; regionIdx < activeRegions.size(); ++regionIdx)
    {
        AudioRegion* region = activeRegions[regionIdx];
        hasInput = true;

        // Calculate the overlap with playback range
        int64_t regionStart = region->getStartPosition();
        int64_t regionEnd = region->getEndPosition();

        int64_t copyStart = juce::jmax(startPos, regionStart);
        int64_t copyEnd = juce::jmin(endPos, regionEnd);
        int numToCopy = static_cast<int>(copyEnd - copyStart);

        // Calculate buffer positions
        int bufferWritePos = static_cast<int>(copyStart - startPos);

        // Get source buffer
        const juce::AudioBuffer<float>& regionBuffer = region->getAudioBuffer();
        int numChannels = juce::jmin(trackBuffer.getNumChannels(), regionBuffer.getNumChannels());

        // Check if this region needs time stretch or pitch shift processing
        bool needsTimeStretch = region->isTimeStretched();
        bool needsPitchShift = region->isPitchShifted();

        if (needsTimeStretch || needsPitchShift)
        {
            // Process with time stretch and/or pitch shift
            float stretchRatio = region->getStretchRatio();

            // Calculate source position accounting for stretch ratio
            // Position in region (timeline) maps to source position via stretch ratio
            int64_t posInRegion = copyStart - regionStart;

            // For time stretching: source position = timeline position * stretchRatio
            // If stretchRatio < 1.0 (slower playback), we read fewer source samples
            // If stretchRatio > 1.0 (faster playback), we read more source samples
            double sourceStartPos = static_cast<double>(posInRegion) * stretchRatio + region->getOffset();
            double sourceEndPos = static_cast<double>(posInRegion + numToCopy) * stretchRatio + region->getOffset();
            int numSourceSamples = static_cast<int>(std::ceil(sourceEndPos - sourceStartPos));

            // Clamp to available source samples
            int sourceReadPos = static_cast<int>(sourceStartPos);
            numSourceSamples = juce::jmin(numSourceSamples, regionBuffer.getNumSamples() - sourceReadPos);

            if (numSourceSamples > 0 && sourceReadPos >= 0 && sourceReadPos < regionBuffer.getNumSamples())
            {
                // Prepare input buffer for processors
                stretchInputBuffer.setSize(numChannels, numSourceSamples, false, false, true);
                stretchInputBuffer.clear();

                // Copy source samples to input buffer
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    int safeSamples = juce::jmin(numSourceSamples, regionBuffer.getNumSamples() - sourceReadPos);
                    if (safeSamples > 0)
                    {
                        stretchInputBuffer.copyFrom(ch, 0, regionBuffer, ch, sourceReadPos, safeSamples);
                    }
                }

                // Process through time stretch if needed
                if (needsTimeStretch)
                {
                    timeStretchProcessor.setStretchRatio(stretchRatio);
                    stretchOutputBuffer.setSize(numChannels, numToCopy, false, false, true);
                    stretchOutputBuffer.clear();
                    timeStretchProcessor.process(stretchInputBuffer, stretchOutputBuffer);

                    // Use stretched output for further processing
                    stretchInputBuffer.makeCopyOf(stretchOutputBuffer);
                }

                // Process through pitch shift if needed
                if (needsPitchShift)
                {
                    pitchShiftProcessor.setPitchShiftSemitones(region->getTotalPitchShift());
                    pitchShiftProcessor.setFormantPreservation(region->getFormantPreserve() ? 1.0f : 0.0f);

                    // Pitch shift preserves duration, so input and output have same size
                    int inputSamples = stretchInputBuffer.getNumSamples();
                    stretchOutputBuffer.setSize(numChannels, inputSamples, false, false, true);
                    stretchOutputBuffer.clear();
                    pitchShiftProcessor.process(stretchInputBuffer, stretchOutputBuffer);

                    // Copy to track buffer
                    int samplesToCopy = juce::jmin(inputSamples, numToCopy);
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        trackBuffer.addFrom(ch, bufferWritePos, stretchOutputBuffer, ch, 0, samplesToCopy);
                    }
                }
                else
                {
                    // Copy time-stretched output directly to track buffer
                    int samplesToCopy = juce::jmin(stretchInputBuffer.getNumSamples(), numToCopy);
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        trackBuffer.addFrom(ch, bufferWritePos, stretchInputBuffer, ch, 0, samplesToCopy);
                    }
                }
            }
        }
        else
        {
            // Standard processing without time stretch or pitch shift
            int regionReadPos = static_cast<int>(copyStart - regionStart + region->getOffset());

            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (regionReadPos >= 0 && regionReadPos + numToCopy <= regionBuffer.getNumSamples())
                {
                    trackBuffer.addFrom(ch, bufferWritePos, regionBuffer, ch, regionReadPos, numToCopy);
                }
            }
        }

        // Get manual fade settings
        int64_t fadeInLen = region->getFadeInLength();
        int64_t fadeOutLen = region->getFadeOutLength();

        // Detect automatic crossfade with previous overlapping region
        int64_t autoCrossfadeInStart = -1;
        int64_t autoCrossfadeInLen = 0;
        if (regionIdx > 0)
        {
            AudioRegion* prevRegion = activeRegions[regionIdx - 1];
            int64_t prevEnd = prevRegion->getEndPosition();
            if (prevEnd > regionStart)
            {
                // Regions overlap - calculate automatic crossfade zone
                int64_t overlapLength = prevEnd - regionStart;
                autoCrossfadeInLen = juce::jmin(overlapLength, DEFAULT_CROSSFADE_SAMPLES);
                autoCrossfadeInStart = regionStart;
            }
        }

        // Detect automatic crossfade with next overlapping region
        int64_t autoCrossfadeOutStart = -1;
        int64_t autoCrossfadeOutLen = 0;
        if (regionIdx + 1 < activeRegions.size())
        {
            AudioRegion* nextRegion = activeRegions[regionIdx + 1];
            int64_t nextStart = nextRegion->getStartPosition();
            if (regionEnd > nextStart)
            {
                // Regions overlap - calculate automatic crossfade zone
                int64_t overlapLength = regionEnd - nextStart;
                autoCrossfadeOutLen = juce::jmin(overlapLength, DEFAULT_CROSSFADE_SAMPLES);
                autoCrossfadeOutStart = regionEnd - autoCrossfadeOutLen;
            }
        }

        // Apply fade in/out envelopes (both manual and automatic crossfades)
        bool needsFadeProcessing = (fadeInLen > 0 || fadeOutLen > 0 ||
                                    autoCrossfadeInLen > 0 || autoCrossfadeOutLen > 0);

        if (needsFadeProcessing)
        {
            // Apply fades sample by sample for the copied range
            for (int i = 0; i < numToCopy; ++i)
            {
                // Absolute timeline position
                int64_t timelinePos = copyStart + i;
                // Position within the region (relative to region start)
                int64_t posInRegion = copyStart - regionStart + i;

                float gain = 1.0f;

                // Apply manual fade in (linear ramp from 0 to 1)
                if (fadeInLen > 0 && posInRegion < fadeInLen)
                {
                    gain *= static_cast<float>(posInRegion) / static_cast<float>(fadeInLen);
                }

                // Apply manual fade out (linear ramp from 1 to 0)
                int64_t fadeOutStart = region->getLength() - fadeOutLen;
                if (fadeOutLen > 0 && posInRegion >= fadeOutStart)
                {
                    int64_t posInFadeOut = posInRegion - fadeOutStart;
                    gain *= 1.0f - (static_cast<float>(posInFadeOut) / static_cast<float>(fadeOutLen));
                }

                // Apply automatic crossfade-in (equal-power fade for smooth overlap)
                if (autoCrossfadeInLen > 0 && timelinePos >= autoCrossfadeInStart &&
                    timelinePos < autoCrossfadeInStart + autoCrossfadeInLen)
                {
                    int64_t posInCrossfade = timelinePos - autoCrossfadeInStart;
                    gain *= CrossfadeUtils::calculateFadeInGain(posInCrossfade, autoCrossfadeInLen);
                }

                // Apply automatic crossfade-out (equal-power fade for smooth overlap)
                if (autoCrossfadeOutLen > 0 && timelinePos >= autoCrossfadeOutStart &&
                    timelinePos < autoCrossfadeOutStart + autoCrossfadeOutLen)
                {
                    int64_t posInCrossfade = timelinePos - autoCrossfadeOutStart;
                    gain *= CrossfadeUtils::calculateFadeOutGain(posInCrossfade, autoCrossfadeOutLen);
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

    // Update last processed position for discontinuity detection
    lastProcessedPosition = endPos;

    // Apply effects chain if we have input or if effects might generate tail (for now optimize clean tracks)
    if (hasInput || effectChain.getNumEffects() > 0)
    {
        // Apply effect parameter automation before processing
        if (isAutomationReading())
            applyEffectAutomation(startPos);

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

    // Reset time stretch and pitch shift processors
    timeStretchProcessor.reset();
    pitchShiftProcessor.reset();

    // Clear processing buffers
    processingBuffer.clear();
    stretchInputBuffer.clear();
    stretchOutputBuffer.clear();

    // Reset processing state
    lastProcessedRegionId = -1;
    lastProcessedPosition = -1;
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

//==============================================================================
// Insert Slot Management
//==============================================================================

int AudioTrack::addInsert(std::unique_ptr<Effect> effect)
{
    if (effectChain.getNumEffects() >= MAX_INSERT_SLOTS)
        return -1; // All slots full

    int slotIndex = effectChain.getNumEffects();
    effectChain.addEffect(std::move(effect));
    return slotIndex;
}

bool AudioTrack::addInsertAtSlot(int slotIndex, std::unique_ptr<Effect> effect)
{
    if (slotIndex < 0 || slotIndex > effectChain.getNumEffects())
        return false;

    if (effectChain.getNumEffects() >= MAX_INSERT_SLOTS)
        return false;

    // Add to chain - effects are processed in order so we add at the end
    // and then move if needed
    effectChain.addEffect(std::move(effect));

    // If not adding at the end, move to correct position
    int currentIndex = effectChain.getNumEffects() - 1;
    if (currentIndex != slotIndex)
    {
        effectChain.moveEffect(currentIndex, slotIndex);
    }

    return true;
}

void AudioTrack::removeInsert(int slotIndex)
{
    if (slotIndex >= 0 && slotIndex < effectChain.getNumEffects())
    {
        effectChain.removeEffect(slotIndex);
    }
}

void AudioTrack::moveInsert(int fromSlot, int toSlot)
{
    effectChain.moveEffect(fromSlot, toSlot);
}

Effect* AudioTrack::getInsert(int slotIndex)
{
    if (slotIndex >= 0 && slotIndex < effectChain.getNumEffects())
    {
        return effectChain.getEffect(slotIndex);
    }
    return nullptr;
}

const Effect* AudioTrack::getInsert(int slotIndex) const
{
    if (slotIndex >= 0 && slotIndex < effectChain.getNumEffects())
    {
        // Cast away constness for the getter since EffectChain doesn't have const getEffect
        return const_cast<EffectChain&>(effectChain).getEffect(slotIndex);
    }
    return nullptr;
}

bool AudioTrack::isInsertSlotEmpty(int slotIndex) const
{
    // A slot is empty if it's beyond the current number of effects
    return slotIndex >= effectChain.getNumEffects();
}

void AudioTrack::clearInserts()
{
    effectChain.clear();
}

void AudioTrack::setInsertBypassed(int slotIndex, bool bypassed)
{
    Effect* effect = getInsert(slotIndex);
    if (effect != nullptr)
    {
        effect->setBypass(bypassed);
    }
}

bool AudioTrack::isInsertBypassed(int slotIndex) const
{
    const Effect* effect = getInsert(slotIndex);
    if (effect != nullptr)
    {
        return effect->isBypassed();
    }
    return false;
}

void AudioTrack::applyEffectAutomation(int64_t position)
{
    for (size_t i = 0; i < getNumAutomationLanes(); ++i)
    {
        AutomationLane* lane = getAutomationLane(i);
        if (lane == nullptr || lane->isBypassed() || !lane->hasAutomation())
            continue;

        const AutomationTarget& target = lane->getTarget();
        if (!target.isEffectParam())
            continue;

        int fxIdx = target.effectIndex;
        Effect* fx = effectChain.getEffect(fxIdx);
        if (fx == nullptr)
            continue;

        // Get the normalized value from the automation lane and convert to parameter range
        float normalizedValue = lane->getValueAtPosition(position);

        // Find the parameter descriptor to get min/max for denormalization
        auto params = fx->getParameters();
        for (const auto& param : params)
        {
            if (param.id == target.parameterId)
            {
                float denormalized = param.minValue + normalizedValue * (param.maxValue - param.minValue);
                fx->setParameter(target.parameterId, denormalized);
                break;
            }
        }
    }
}

