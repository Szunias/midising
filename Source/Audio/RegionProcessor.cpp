#include "RegionProcessor.h"

void RegionProcessor::reverseRegion(AudioRegion& region)
{
    auto& buffer = region.getAudioBuffer();
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    if (numSamples <= 1)
        return;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        std::reverse(channelData, channelData + numSamples);
    }
}

float RegionProcessor::normalizeRegion(AudioRegion& region, float targetPeak)
{
    auto& buffer = region.getAudioBuffer();
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return 1.0f;

    // Find the absolute peak across all channels
    float peak = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto range = juce::FloatVectorOperations::findMinAndMax(
            buffer.getReadPointer(ch), numSamples);

        float channelPeak = juce::jmax(std::abs(range.getStart()), std::abs(range.getEnd()));
        peak = juce::jmax(peak, channelPeak);
    }

    // Avoid division by zero for silent buffers
    if (peak < 1.0e-10f)
        return 1.0f;

    // Calculate and apply the gain factor
    float gainFactor = targetPeak / peak;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        juce::FloatVectorOperations::multiply(
            buffer.getWritePointer(ch), gainFactor, numSamples);
    }

    return gainFactor;
}

std::unique_ptr<AudioRegion> RegionProcessor::consolidateRegions(AudioTrack& track,
                                                                  int64_t start,
                                                                  int64_t end,
                                                                  double sampleRate)
{
    juce::ignoreUnused(sampleRate);

    if (end <= start)
        return nullptr;

    int64_t consolidatedLength = end - start;
    int numRegions = track.getNumRegions();

    // Find regions that overlap with the consolidation range
    bool hasOverlap = false;
    for (int i = 0; i < numRegions; ++i)
    {
        auto* region = track.getRegion(i);
        if (region == nullptr)
            continue;

        // Check if this region overlaps with [start, end)
        if (region->getStartPosition() < end && region->getEndPosition() > start)
        {
            hasOverlap = true;
            break;
        }
    }

    if (!hasOverlap)
        return nullptr;

    // Create output buffer (stereo)
    const int numOutputChannels = 2;
    juce::AudioBuffer<float> outputBuffer(numOutputChannels,
                                           static_cast<int>(consolidatedLength));
    outputBuffer.clear();

    // Render each overlapping region into the output buffer
    for (int i = 0; i < numRegions; ++i)
    {
        auto* region = track.getRegion(i);
        if (region == nullptr)
            continue;

        int64_t regionStart = region->getStartPosition();
        int64_t regionEnd = region->getEndPosition();

        // Skip regions that don't overlap with the consolidation range
        if (regionStart >= end || regionEnd <= start)
            continue;

        const auto& srcBuffer = region->getAudioBuffer();
        int srcChannels = srcBuffer.getNumChannels();
        int srcSamples = srcBuffer.getNumSamples();

        if (srcChannels == 0 || srcSamples == 0)
            continue;

        // Calculate the overlap between region and consolidation range
        int64_t overlapStart = juce::jmax(regionStart, start);
        int64_t overlapEnd = juce::jmin(regionEnd, end);
        int64_t overlapLength = overlapEnd - overlapStart;

        if (overlapLength <= 0)
            continue;

        // Calculate source and destination offsets
        int64_t srcOffset = overlapStart - regionStart + region->getOffset();
        int64_t dstOffset = overlapStart - start;

        // Clamp to buffer bounds
        int srcStartSample = static_cast<int>(juce::jmax(int64_t(0), srcOffset));
        int dstStartSample = static_cast<int>(juce::jmax(int64_t(0), dstOffset));
        int samplesToCopy = static_cast<int>(juce::jmin(overlapLength,
                                              static_cast<int64_t>(srcSamples) - srcStartSample));
        samplesToCopy = juce::jmin(samplesToCopy,
                                    static_cast<int>(consolidatedLength) - dstStartSample);

        if (samplesToCopy <= 0)
            continue;

        // Mix region audio into the output buffer
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            // If source is mono, use channel 0 for both output channels
            int srcCh = juce::jmin(ch, srcChannels - 1);

            const float* srcData = srcBuffer.getReadPointer(srcCh) + srcStartSample;
            float* dstData = outputBuffer.getWritePointer(ch) + dstStartSample;

            // Apply fade gains during mixing
            for (int s = 0; s < samplesToCopy; ++s)
            {
                int64_t posInRegion = (overlapStart - regionStart) + s;
                float fadeGain = 1.0f;

                // Apply fade-in
                int64_t fadeInLen = region->getFadeInLength();
                if (fadeInLen > 0 && posInRegion < fadeInLen)
                {
                    fadeGain *= CrossfadeUtils::calculateFadeInGain(posInRegion, fadeInLen);
                }

                // Apply fade-out
                int64_t fadeOutLen = region->getFadeOutLength();
                int64_t regionLength = region->getLength();
                if (fadeOutLen > 0 && posInRegion >= (regionLength - fadeOutLen))
                {
                    int64_t fadeOutPos = posInRegion - (regionLength - fadeOutLen);
                    fadeGain *= CrossfadeUtils::calculateFadeOutGain(fadeOutPos, fadeOutLen);
                }

                dstData[s] += srcData[s] * fadeGain;
            }
        }
    }

    // Create the consolidated AudioRegion
    auto consolidated = std::make_unique<AudioRegion>(start, consolidatedLength);
    consolidated->setAudioBuffer(outputBuffer);
    consolidated->setName("Consolidated");

    return consolidated;
}
