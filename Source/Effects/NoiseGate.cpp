#include "NoiseGate.h"

NoiseGate::NoiseGate()
    : Effect("Noise Gate")
{
}

void NoiseGate::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    envelopeLevel = 0.0f;
    currentGateGain = 1.0f;
    holdCounter = 0;
    gateOpen = false;
    updateCoefficients();
}

void NoiseGate::releaseResources()
{
}

void NoiseGate::processBlock(juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    float thresholdLinear = juce::Decibels::decibelsToGain(thresholdDb);
    float rangeLinear = juce::Decibels::decibelsToGain(rangeDb);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Detect peak level across all channels
        float inputLevel = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float absSample = std::abs(buffer.getSample(ch, sample));
            inputLevel = juce::jmax(inputLevel, absSample);
        }

        // Envelope follower
        if (inputLevel > envelopeLevel)
            envelopeLevel += attackCoeff * (inputLevel - envelopeLevel);
        else
            envelopeLevel += releaseCoeff * (inputLevel - envelopeLevel);

        // Gate logic
        if (envelopeLevel >= thresholdLinear)
        {
            gateOpen = true;
            holdCounter = holdSamples;
        }
        else if (holdCounter > 0)
        {
            holdCounter--;
            // Gate stays open during hold
        }
        else
        {
            gateOpen = false;
        }

        // Calculate target gain
        float targetGain;
        if (gateOpen)
        {
            targetGain = 1.0f;
        }
        else
        {
            // Apply range (attenuation when closed)
            if (ratio >= 10.0f)
            {
                targetGain = rangeLinear;
            }
            else
            {
                // Soft gate - partial attenuation based on ratio
                float reductionDb = (thresholdDb - juce::Decibels::gainToDecibels(envelopeLevel + 1e-10f)) * (1.0f - 1.0f / ratio);
                targetGain = juce::jmax(rangeLinear, juce::Decibels::decibelsToGain(-reductionDb));
            }
        }

        // Smooth gain changes
        currentGateGain += (targetGain - currentGateGain) * 0.01f;

        // Apply gain
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.setSample(ch, sample, buffer.getSample(ch, sample) * currentGateGain);
        }
    }
}

void NoiseGate::setThreshold(float db)
{
    thresholdDb = juce::jlimit(-80.0f, 0.0f, db);
}

void NoiseGate::setRatio(float r)
{
    ratio = juce::jlimit(1.0f, 10.0f, r);
}

void NoiseGate::setAttack(float ms)
{
    attackMs = juce::jlimit(0.01f, 50.0f, ms);
    updateCoefficients();
}

void NoiseGate::setRelease(float ms)
{
    releaseMs = juce::jlimit(5.0f, 500.0f, ms);
    updateCoefficients();
}

void NoiseGate::setHoldTime(float ms)
{
    holdMs = juce::jlimit(0.0f, 500.0f, ms);
    updateCoefficients();
}

void NoiseGate::setRange(float db)
{
    rangeDb = juce::jlimit(-80.0f, 0.0f, db);
}

void NoiseGate::updateCoefficients()
{
    if (currentSampleRate <= 0.0)
        return;

    attackCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(currentSampleRate) * attackMs * 0.001f));
    releaseCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(currentSampleRate) * releaseMs * 0.001f));
    holdSamples = static_cast<int>(currentSampleRate * holdMs * 0.001);
}

std::unique_ptr<juce::XmlElement> NoiseGate::createXml() const
{
    auto xml = Effect::createXml();
    xml->setAttribute("type", "NoiseGate");
    xml->setAttribute("threshold", thresholdDb);
    xml->setAttribute("ratio", ratio);
    xml->setAttribute("attack", attackMs);
    xml->setAttribute("release", releaseMs);
    xml->setAttribute("hold", holdMs);
    xml->setAttribute("range", rangeDb);
    return xml;
}

void NoiseGate::loadFromXml(const juce::XmlElement& xml)
{
    Effect::loadFromXml(xml);
    thresholdDb = static_cast<float>(xml.getDoubleAttribute("threshold", -40.0));
    ratio = static_cast<float>(xml.getDoubleAttribute("ratio", 10.0));
    attackMs = static_cast<float>(xml.getDoubleAttribute("attack", 0.5));
    releaseMs = static_cast<float>(xml.getDoubleAttribute("release", 50.0));
    holdMs = static_cast<float>(xml.getDoubleAttribute("hold", 10.0));
    rangeDb = static_cast<float>(xml.getDoubleAttribute("range", -80.0));
    updateCoefficients();
}
