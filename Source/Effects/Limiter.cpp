#include "Limiter.h"

Limiter::Limiter()
    : Effect("Limiter")
{
}

void Limiter::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Calculate look-ahead in samples
    lookAheadSamples = static_cast<int>(sampleRate * lookAheadMs * 0.001);
    lookAheadSamples = juce::jmax(1, lookAheadSamples);

    // Allocate delay buffers
    delayBufferL.assign(static_cast<size_t>(lookAheadSamples), 0.0f);
    delayBufferR.assign(static_cast<size_t>(lookAheadSamples), 0.0f);
    delayWritePos = 0;

    currentGain = 1.0f;
    currentGainReduction = 0.0f;

    updateCoefficients();
}

void Limiter::releaseResources()
{
    delayBufferL.clear();
    delayBufferR.clear();
}

void Limiter::processBlock(juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    if (delayBufferL.empty())
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Apply input gain to incoming samples
        float inputL = 0.0f, inputR = 0.0f;
        if (numChannels >= 1)
            inputL = buffer.getSample(0, sample) * inputGainLinear;
        if (numChannels >= 2)
            inputR = buffer.getSample(1, sample) * inputGainLinear;

        // Peak detect the incoming signal (before delay)
        float peakLevel = juce::jmax(std::abs(inputL), std::abs(inputR));

        // Calculate desired gain for this sample
        float desiredGain = 1.0f;
        if (peakLevel > ceilingLinear)
        {
            desiredGain = ceilingLinear / peakLevel;
        }

        // Gain smoothing: instant attack, smooth release
        if (desiredGain < currentGain)
        {
            // Instant attack
            currentGain = desiredGain;
        }
        else
        {
            // Smooth release
            currentGain += releaseCoeff * (desiredGain - currentGain);
        }

        // Read from delay buffer (the delayed signal)
        int readPos = delayWritePos;
        float delayedL = delayBufferL[static_cast<size_t>(readPos)];
        float delayedR = numChannels >= 2 ? delayBufferR[static_cast<size_t>(readPos)] : 0.0f;

        // Write current input to delay buffer
        delayBufferL[static_cast<size_t>(delayWritePos)] = inputL;
        if (numChannels >= 2)
            delayBufferR[static_cast<size_t>(delayWritePos)] = inputR;

        // Advance write position
        delayWritePos = (delayWritePos + 1) % lookAheadSamples;

        // Apply gain to delayed signal
        if (numChannels >= 1)
            buffer.setSample(0, sample, delayedL * currentGain);
        if (numChannels >= 2)
            buffer.setSample(1, sample, delayedR * currentGain);

        // Update gain reduction metering
        currentGainReduction = juce::Decibels::gainToDecibels(currentGain);
    }
}

void Limiter::setCeiling(float db)
{
    ceilingDb = juce::jlimit(-12.0f, 0.0f, db);
    ceilingLinear = juce::Decibels::decibelsToGain(ceilingDb);
}

void Limiter::setRelease(float ms)
{
    releaseMs = juce::jlimit(1.0f, 500.0f, ms);
    updateCoefficients();
}

void Limiter::setInputGain(float db)
{
    inputGainDb = juce::jlimit(-12.0f, 24.0f, db);
    inputGainLinear = juce::Decibels::decibelsToGain(inputGainDb);
}

void Limiter::updateCoefficients()
{
    if (currentSampleRate <= 0.0)
        return;

    releaseCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(currentSampleRate) * releaseMs * 0.001f));
    ceilingLinear = juce::Decibels::decibelsToGain(ceilingDb);
    inputGainLinear = juce::Decibels::decibelsToGain(inputGainDb);
}

std::unique_ptr<juce::XmlElement> Limiter::createXml() const
{
    auto xml = Effect::createXml();
    xml->setAttribute("type", "Limiter");
    xml->setAttribute("ceiling", ceilingDb);
    xml->setAttribute("release", releaseMs);
    xml->setAttribute("inputGain", inputGainDb);
    return xml;
}

void Limiter::loadFromXml(const juce::XmlElement& xml)
{
    Effect::loadFromXml(xml);
    ceilingDb = static_cast<float>(xml.getDoubleAttribute("ceiling", -0.3));
    releaseMs = static_cast<float>(xml.getDoubleAttribute("release", 100.0));
    inputGainDb = static_cast<float>(xml.getDoubleAttribute("inputGain", 0.0));
    updateCoefficients();
}
