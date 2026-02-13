#include "VCACompressor.h"
#include <cmath>

VCACompressor::VCACompressor()
    : Effect("VCA Compressor")
{
    updateCoefficients();
}

void VCACompressor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    envelopeLevel = 0.0f;
    currentGainReduction = 0.0f;
    updateCoefficients();
}

void VCACompressor::releaseResources()
{
    envelopeLevel = 0.0f;
    currentGainReduction = 0.0f;
}

void VCACompressor::processBlock(juce::AudioBuffer<float>& buffer)
{
    // Use external sidechain buffer if set, otherwise use input signal
    if (sidechainInputBuffer != nullptr && sidechainInputBuffer->getNumSamples() >= buffer.getNumSamples())
        processBlockWithSidechain(buffer, *sidechainInputBuffer);
    else
        processBlockWithSidechain(buffer, buffer);

    // Clear sidechain buffer pointer after use (must be re-set each block)
    sidechainInputBuffer = nullptr;
}

void VCACompressor::processBlockWithSidechain(juce::AudioBuffer<float>& buffer,
                                               const juce::AudioBuffer<float>& sidechainBuffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int sidechainChannels = sidechainBuffer.getNumChannels();

    if (numChannels == 0 || numSamples == 0)
        return;

    float peakGainReduction = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Get sidechain detection signal (sum of all channels, then RMS-like detection)
        float sidechainSample = 0.0f;
        for (int ch = 0; ch < sidechainChannels; ++ch)
        {
            sidechainSample += std::abs(sidechainBuffer.getSample(ch, sample));
        }
        sidechainSample /= static_cast<float>(sidechainChannels);

        // Envelope follower for level detection
        float detectedLevel = detectLevel(sidechainSample);

        // Convert to dB (with safety floor)
        float inputLevelDb = (detectedLevel > 1e-10f)
            ? 20.0f * std::log10(detectedLevel)
            : -100.0f;

        // Compute gain reduction
        float gainReductionDb = computeGainReduction(inputLevelDb);

        // Track peak gain reduction for metering
        peakGainReduction = std::max(peakGainReduction, gainReductionDb);

        // Convert to linear gain
        float gainLinear = juce::Decibels::decibelsToGain(-gainReductionDb) * makeupGainLinear;

        // Apply gain to all channels
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* channelData = buffer.getWritePointer(ch);
            channelData[sample] *= gainLinear;
        }
    }

    // Update metering (smoothed)
    currentGainReduction = currentGainReduction * 0.9f + peakGainReduction * 0.1f;
}

float VCACompressor::computeGainReduction(float inputLevelDb) const
{
    if (inputLevelDb <= thresholdDb - kneeDb)
    {
        // Below knee - no compression
        return 0.0f;
    }

    float gainReduction = 0.0f;

    if (kneeDb > 0.0f && inputLevelDb < thresholdDb + kneeDb)
    {
        // In the knee region - soft knee compression
        float kneeRange = 2.0f * kneeDb;
        float kneePosition = (inputLevelDb - (thresholdDb - kneeDb)) / kneeRange;

        // Quadratic curve for soft knee
        float excessGain = (inputLevelDb - thresholdDb + kneeDb) * kneePosition;
        gainReduction = excessGain * (1.0f - 1.0f / ratio) * 0.5f;
    }
    else
    {
        // Above knee - full compression
        float excessDb = inputLevelDb - thresholdDb;
        gainReduction = excessDb * (1.0f - 1.0f / ratio);
    }

    return std::max(0.0f, gainReduction);
}

float VCACompressor::detectLevel(float sample)
{
    float absSample = std::abs(sample);

    if (absSample > envelopeLevel)
    {
        // Attack
        envelopeLevel += attackCoeff * (absSample - envelopeLevel);
    }
    else
    {
        // Release
        envelopeLevel += releaseCoeff * (absSample - envelopeLevel);
    }

    return envelopeLevel;
}

void VCACompressor::setThreshold(float threshold)
{
    thresholdDb = juce::jlimit(-60.0f, 0.0f, threshold);
}

void VCACompressor::setRatio(float newRatio)
{
    ratio = juce::jlimit(1.0f, 20.0f, newRatio);
}

void VCACompressor::setAttack(float attack)
{
    attackMs = juce::jlimit(0.1f, 100.0f, attack);
    updateCoefficients();
}

void VCACompressor::setRelease(float release)
{
    releaseMs = juce::jlimit(10.0f, 1000.0f, release);
    updateCoefficients();
}

void VCACompressor::setKnee(float knee)
{
    kneeDb = juce::jlimit(0.0f, 12.0f, knee);
}

void VCACompressor::setMakeupGain(float gain)
{
    makeupGainDb = juce::jlimit(0.0f, 24.0f, gain);
    makeupGainLinear = juce::Decibels::decibelsToGain(makeupGainDb);
}

void VCACompressor::updateCoefficients()
{
    if (currentSampleRate <= 0.0)
        return;

    // Calculate attack and release coefficients
    // Using standard one-pole filter coefficient calculation
    float attackSamples = static_cast<float>(currentSampleRate * attackMs / 1000.0);
    float releaseSamples = static_cast<float>(currentSampleRate * releaseMs / 1000.0);

    // Coefficients for exponential envelope following
    attackCoeff = std::exp(-1.0f / attackSamples);
    attackCoeff = 1.0f - attackCoeff;

    releaseCoeff = std::exp(-1.0f / releaseSamples);
    releaseCoeff = 1.0f - releaseCoeff;

    makeupGainLinear = juce::Decibels::decibelsToGain(makeupGainDb);
}

std::unique_ptr<juce::XmlElement> VCACompressor::createXml() const
{
    auto xml = Effect::createXml();
    xml->setAttribute("type", "VCACompressor");
    xml->setAttribute("threshold", thresholdDb);
    xml->setAttribute("ratio", ratio);
    xml->setAttribute("attack", attackMs);
    xml->setAttribute("release", releaseMs);
    xml->setAttribute("knee", kneeDb);
    xml->setAttribute("makeupGain", makeupGainDb);
    xml->setAttribute("sidechainEnabled", sidechainEnabled);
    return xml;
}

void VCACompressor::loadFromXml(const juce::XmlElement& xml)
{
    Effect::loadFromXml(xml);
    thresholdDb = static_cast<float>(xml.getDoubleAttribute("threshold", -20.0));
    ratio = static_cast<float>(xml.getDoubleAttribute("ratio", 4.0));
    attackMs = static_cast<float>(xml.getDoubleAttribute("attack", 10.0));
    releaseMs = static_cast<float>(xml.getDoubleAttribute("release", 100.0));
    kneeDb = static_cast<float>(xml.getDoubleAttribute("knee", 3.0));
    makeupGainDb = static_cast<float>(xml.getDoubleAttribute("makeupGain", 0.0));
    sidechainEnabled = xml.getBoolAttribute("sidechainEnabled", false);
    updateCoefficients();
}
