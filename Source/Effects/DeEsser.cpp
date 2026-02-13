#include "DeEsser.h"

DeEsser::DeEsser() : Effect("De-Esser")
{
}

void DeEsser::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    envelopeL = 0.0f;
    envelopeR = 0.0f;
    currentGainReduction = 0.0f;

    updateFilters();
    updateEnvelopeCoefficients();
}

void DeEsser::releaseResources()
{
    bandpassL.reset();
    bandpassR.reset();
    envelopeL = 0.0f;
    envelopeR = 0.0f;
    currentGainReduction = 0.0f;
}

void DeEsser::processBlock(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels == 0 || numSamples == 0)
        return;

    float thresholdLinear = juce::Decibels::decibelsToGain(thresholdDb);
    float peakGR = 0.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        // Process sidechain detection for left channel
        float gainReductionL = 0.0f;
        float gainReductionR = 0.0f;

        if (numChannels > 0)
        {
            float input = buffer.getSample(0, s);
            float sidechain = bandpassL.processSample(input);

            // Envelope follower
            float absVal = std::abs(sidechain);
            if (absVal > envelopeL)
                envelopeL += attackCoeff * (absVal - envelopeL);
            else
                envelopeL += releaseCoeff * (absVal - envelopeL);

            // Compute gain reduction
            if (envelopeL > thresholdLinear)
            {
                float excessDb = 20.0f * std::log10(envelopeL / thresholdLinear);
                gainReductionL = std::min(excessDb, reductionDb);
            }

            if (listenMode)
            {
                buffer.setSample(0, s, sidechain);
            }
            else
            {
                float gainLinear = juce::Decibels::decibelsToGain(-gainReductionL);
                buffer.setSample(0, s, input * gainLinear);
            }
        }

        if (numChannels > 1)
        {
            float input = buffer.getSample(1, s);
            float sidechain = bandpassR.processSample(input);

            float absVal = std::abs(sidechain);
            if (absVal > envelopeR)
                envelopeR += attackCoeff * (absVal - envelopeR);
            else
                envelopeR += releaseCoeff * (absVal - envelopeR);

            if (envelopeR > thresholdLinear)
            {
                float excessDb = 20.0f * std::log10(envelopeR / thresholdLinear);
                gainReductionR = std::min(excessDb, reductionDb);
            }

            if (listenMode)
            {
                buffer.setSample(1, s, sidechain);
            }
            else
            {
                float gainLinear = juce::Decibels::decibelsToGain(-gainReductionR);
                buffer.setSample(1, s, input * gainLinear);
            }
        }

        peakGR = std::max(peakGR, std::max(gainReductionL, gainReductionR));
    }

    // Smooth metering
    currentGainReduction = currentGainReduction * 0.9f + peakGR * 0.1f;
}

void DeEsser::setFrequency(float freqHz)
{
    frequency = juce::jlimit(2000.0f, 12000.0f, freqHz);
    updateFilters();
}

void DeEsser::setBandwidth(float octaves)
{
    bandwidth = juce::jlimit(0.5f, 3.0f, octaves);
    updateFilters();
}

void DeEsser::setThreshold(float threshold)
{
    thresholdDb = juce::jlimit(-40.0f, 0.0f, threshold);
}

void DeEsser::setReduction(float reduction)
{
    reductionDb = juce::jlimit(0.0f, 24.0f, reduction);
}

void DeEsser::setListenMode(bool listen)
{
    listenMode = listen;
}

void DeEsser::updateFilters()
{
    if (currentSampleRate <= 0.0)
        return;

    // Convert bandwidth in octaves to Q
    // Q = 1 / (2 * sinh(ln(2)/2 * bandwidth))
    float ln2 = std::log(2.0f);
    float q = 1.0f / (2.0f * std::sinh(ln2 / 2.0f * bandwidth));

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(
        currentSampleRate, frequency, q);

    bandpassL.coefficients = coeffs;
    bandpassR.coefficients = coeffs;
}

void DeEsser::updateEnvelopeCoefficients()
{
    if (currentSampleRate <= 0.0)
        return;

    // Fast attack (1ms), moderate release (50ms)
    float attackSamples = static_cast<float>(currentSampleRate * 0.001);
    float releaseSamples = static_cast<float>(currentSampleRate * 0.050);

    attackCoeff = 1.0f - std::exp(-1.0f / attackSamples);
    releaseCoeff = 1.0f - std::exp(-1.0f / releaseSamples);
}

std::unique_ptr<juce::XmlElement> DeEsser::createXml() const
{
    auto xml = Effect::createXml();
    xml->setAttribute("type", "DeEsser");
    xml->setAttribute("frequency", static_cast<double>(frequency));
    xml->setAttribute("bandwidth", static_cast<double>(bandwidth));
    xml->setAttribute("threshold", static_cast<double>(thresholdDb));
    xml->setAttribute("reduction", static_cast<double>(reductionDb));
    xml->setAttribute("listenMode", listenMode);
    return xml;
}

void DeEsser::loadFromXml(const juce::XmlElement& xml)
{
    Effect::loadFromXml(xml);
    frequency = static_cast<float>(xml.getDoubleAttribute("frequency", 6000.0));
    bandwidth = static_cast<float>(xml.getDoubleAttribute("bandwidth", 1.5));
    thresholdDb = static_cast<float>(xml.getDoubleAttribute("threshold", -20.0));
    reductionDb = static_cast<float>(xml.getDoubleAttribute("reduction", 6.0));
    listenMode = xml.getBoolAttribute("listenMode", false);

    updateFilters();
    updateEnvelopeCoefficients();
}
