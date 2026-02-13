#include "MultibandCompressor.h"

MultibandCompressor::MultibandCompressor() : Effect("Multiband Compressor")
{
}

void MultibandCompressor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    // Reset all filter states
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;

    lowPassL1.reset();  lowPassL2.reset();
    lowPassR1.reset();  lowPassR2.reset();
    highPassL1.reset(); highPassL2.reset();
    highPassR1.reset(); highPassR2.reset();
    midLowPassL1.reset();  midLowPassL2.reset();
    midLowPassR1.reset();  midLowPassR2.reset();
    midHighPassL1.reset(); midHighPassL2.reset();
    midHighPassR1.reset(); midHighPassR2.reset();

    for (int i = 0; i < NUM_BANDS; ++i)
    {
        bands[i].envelopeLevel = 0.0f;
        bands[i].gainReduction = 0.0f;
        updateBandCoefficients(i);
    }

    updateCrossoverFilters();
}

void MultibandCompressor::releaseResources()
{
    for (int i = 0; i < NUM_BANDS; ++i)
    {
        bands[i].envelopeLevel = 0.0f;
        bands[i].gainReduction = 0.0f;
    }
}

void MultibandCompressor::processBlock(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels == 0 || numSamples == 0)
        return;

    // Create temporary buffers for each band
    juce::AudioBuffer<float> lowBand(numChannels, numSamples);
    juce::AudioBuffer<float> midBand(numChannels, numSamples);
    juce::AudioBuffer<float> highBand(numChannels, numSamples);

    // Copy input to band buffers
    for (int ch = 0; ch < numChannels; ++ch)
    {
        lowBand.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        midBand.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }

    // Split into low and mid+high using low-mid crossover
    // Linkwitz-Riley: cascade two Butterworth filters
    for (int s = 0; s < numSamples; ++s)
    {
        if (numChannels > 0)
        {
            float inputL = buffer.getSample(0, s);

            // Low band (two cascaded lowpass)
            float lowL = lowPassL1.processSample(inputL);
            lowL = lowPassL2.processSample(lowL);
            lowBand.setSample(0, s, lowL);

            // Mid+High (two cascaded highpass)
            float midHighL = highPassL1.processSample(inputL);
            midHighL = highPassL2.processSample(midHighL);
            midBand.setSample(0, s, midHighL);
        }

        if (numChannels > 1)
        {
            float inputR = buffer.getSample(1, s);

            float lowR = lowPassR1.processSample(inputR);
            lowR = lowPassR2.processSample(lowR);
            lowBand.setSample(1, s, lowR);

            float midHighR = highPassR1.processSample(inputR);
            midHighR = highPassR2.processSample(midHighR);
            midBand.setSample(1, s, midHighR);
        }
    }

    // Split mid+high into mid and high using mid-high crossover
    for (int ch = 0; ch < numChannels; ++ch)
        highBand.copyFrom(ch, 0, midBand, ch, 0, numSamples);

    for (int s = 0; s < numSamples; ++s)
    {
        if (numChannels > 0)
        {
            float inputL = midBand.getSample(0, s);

            float mL = midLowPassL1.processSample(inputL);
            mL = midLowPassL2.processSample(mL);
            midBand.setSample(0, s, mL);

            float hL = midHighPassL1.processSample(inputL);
            hL = midHighPassL2.processSample(hL);
            highBand.setSample(0, s, hL);
        }

        if (numChannels > 1)
        {
            float inputR = midBand.getSample(1, s);

            float mR = midLowPassR1.processSample(inputR);
            mR = midLowPassR2.processSample(mR);
            midBand.setSample(1, s, mR);

            float hR = midHighPassR1.processSample(inputR);
            hR = midHighPassR2.processSample(hR);
            highBand.setSample(1, s, hR);
        }
    }

    // Compress each band
    juce::AudioBuffer<float>* bandBuffers[NUM_BANDS] = { &lowBand, &midBand, &highBand };

    bool anySolo = false;
    for (int b = 0; b < NUM_BANDS; ++b)
        if (bands[b].solo) anySolo = true;

    for (int b = 0; b < NUM_BANDS; ++b)
    {
        auto& bp = bands[b];
        auto& buf = *bandBuffers[b];

        if (!bp.bandBypass)
        {
            float peakGR = 0.0f;

            for (int s = 0; s < numSamples; ++s)
            {
                // Detect level from all channels
                float level = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    level = std::max(level, std::abs(buf.getSample(ch, s)));

                float gainDb = computeCompression(bp, level);
                peakGR = std::max(peakGR, std::abs(gainDb));

                float gainLinear = juce::Decibels::decibelsToGain(gainDb) * bp.makeupGainLinear;

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    float* data = buf.getWritePointer(ch);
                    data[s] *= gainLinear;
                }
            }

            bp.gainReduction = bp.gainReduction * 0.9f + peakGR * 0.1f;
        }
    }

    // Sum bands back together
    buffer.clear();
    for (int b = 0; b < NUM_BANDS; ++b)
    {
        // If any band is soloed, only include soloed bands
        if (anySolo && !bands[b].solo)
            continue;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.addFrom(ch, 0, *bandBuffers[b], ch, 0, numSamples);
        }
    }
}

float MultibandCompressor::computeCompression(BandParams& bp, float inputLevel)
{
    // Envelope follower
    float absSample = inputLevel;
    if (absSample > bp.envelopeLevel)
        bp.envelopeLevel += bp.attackCoeff * (absSample - bp.envelopeLevel);
    else
        bp.envelopeLevel += bp.releaseCoeff * (absSample - bp.envelopeLevel);

    // Convert to dB
    float levelDb = (bp.envelopeLevel > 1e-10f)
        ? 20.0f * std::log10(bp.envelopeLevel)
        : -100.0f;

    // Compute gain reduction
    if (levelDb <= bp.threshold)
        return 0.0f;

    float excessDb = levelDb - bp.threshold;
    float reductionDb = excessDb * (1.0f - 1.0f / bp.ratio);
    return -reductionDb;
}

void MultibandCompressor::updateCrossoverFilters()
{
    if (currentSampleRate <= 0.0)
        return;

    // Low-mid crossover: Butterworth LP/HP (cascaded = Linkwitz-Riley)
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, lowMidFreq);
    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, lowMidFreq);

    lowPassL1.coefficients = lpCoeffs;
    lowPassL2.coefficients = lpCoeffs;
    lowPassR1.coefficients = lpCoeffs;
    lowPassR2.coefficients = lpCoeffs;
    highPassL1.coefficients = hpCoeffs;
    highPassL2.coefficients = hpCoeffs;
    highPassR1.coefficients = hpCoeffs;
    highPassR2.coefficients = hpCoeffs;

    // Mid-high crossover
    auto midLpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, midHighFreq);
    auto midHpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, midHighFreq);

    midLowPassL1.coefficients = midLpCoeffs;
    midLowPassL2.coefficients = midLpCoeffs;
    midLowPassR1.coefficients = midLpCoeffs;
    midLowPassR2.coefficients = midLpCoeffs;
    midHighPassL1.coefficients = midHpCoeffs;
    midHighPassL2.coefficients = midHpCoeffs;
    midHighPassR1.coefficients = midHpCoeffs;
    midHighPassR2.coefficients = midHpCoeffs;
}

void MultibandCompressor::updateBandCoefficients(int band)
{
    if (band < 0 || band >= NUM_BANDS || currentSampleRate <= 0.0)
        return;

    auto& bp = bands[band];
    float attackSamples = static_cast<float>(currentSampleRate * bp.attackMs / 1000.0);
    float releaseSamples = static_cast<float>(currentSampleRate * bp.releaseMs / 1000.0);

    bp.attackCoeff = 1.0f - std::exp(-1.0f / attackSamples);
    bp.releaseCoeff = 1.0f - std::exp(-1.0f / releaseSamples);
    bp.makeupGainLinear = juce::Decibels::decibelsToGain(bp.makeupGainDb);
}

void MultibandCompressor::setLowMidCrossover(float freqHz)
{
    lowMidFreq = juce::jlimit(100.0f, 500.0f, freqHz);
    updateCrossoverFilters();
}

void MultibandCompressor::setMidHighCrossover(float freqHz)
{
    midHighFreq = juce::jlimit(1000.0f, 8000.0f, freqHz);
    updateCrossoverFilters();
}

void MultibandCompressor::setBandThreshold(int band, float dB)
{
    if (band >= 0 && band < NUM_BANDS)
        bands[band].threshold = juce::jlimit(-60.0f, 0.0f, dB);
}

void MultibandCompressor::setBandRatio(int band, float ratio)
{
    if (band >= 0 && band < NUM_BANDS)
        bands[band].ratio = juce::jlimit(1.0f, 20.0f, ratio);
}

void MultibandCompressor::setBandAttack(int band, float ms)
{
    if (band >= 0 && band < NUM_BANDS)
    {
        bands[band].attackMs = juce::jlimit(0.1f, 100.0f, ms);
        updateBandCoefficients(band);
    }
}

void MultibandCompressor::setBandRelease(int band, float ms)
{
    if (band >= 0 && band < NUM_BANDS)
    {
        bands[band].releaseMs = juce::jlimit(10.0f, 1000.0f, ms);
        updateBandCoefficients(band);
    }
}

void MultibandCompressor::setBandMakeupGain(int band, float dB)
{
    if (band >= 0 && band < NUM_BANDS)
    {
        bands[band].makeupGainDb = juce::jlimit(0.0f, 24.0f, dB);
        bands[band].makeupGainLinear = juce::Decibels::decibelsToGain(dB);
    }
}

void MultibandCompressor::setBandSolo(int band, bool solo)
{
    if (band >= 0 && band < NUM_BANDS)
        bands[band].solo = solo;
}

void MultibandCompressor::setBandBypass(int band, bool shouldBypass)
{
    if (band >= 0 && band < NUM_BANDS)
        bands[band].bandBypass = shouldBypass;
}

float MultibandCompressor::getBandThreshold(int band) const
{
    return (band >= 0 && band < NUM_BANDS) ? bands[band].threshold : 0.0f;
}

float MultibandCompressor::getBandRatio(int band) const
{
    return (band >= 0 && band < NUM_BANDS) ? bands[band].ratio : 1.0f;
}

float MultibandCompressor::getBandAttack(int band) const
{
    return (band >= 0 && band < NUM_BANDS) ? bands[band].attackMs : 10.0f;
}

float MultibandCompressor::getBandRelease(int band) const
{
    return (band >= 0 && band < NUM_BANDS) ? bands[band].releaseMs : 100.0f;
}

float MultibandCompressor::getBandMakeupGain(int band) const
{
    return (band >= 0 && band < NUM_BANDS) ? bands[band].makeupGainDb : 0.0f;
}

bool MultibandCompressor::getBandSolo(int band) const
{
    return (band >= 0 && band < NUM_BANDS) ? bands[band].solo : false;
}

bool MultibandCompressor::getBandBypass(int band) const
{
    return (band >= 0 && band < NUM_BANDS) ? bands[band].bandBypass : false;
}

float MultibandCompressor::getBandGainReduction(int band) const
{
    return (band >= 0 && band < NUM_BANDS) ? bands[band].gainReduction : 0.0f;
}

std::unique_ptr<juce::XmlElement> MultibandCompressor::createXml() const
{
    auto xml = Effect::createXml();
    xml->setAttribute("type", "MultibandCompressor");
    xml->setAttribute("lowMidFreq", static_cast<double>(lowMidFreq));
    xml->setAttribute("midHighFreq", static_cast<double>(midHighFreq));

    for (int i = 0; i < NUM_BANDS; ++i)
    {
        auto bandXml = std::make_unique<juce::XmlElement>("BAND");
        bandXml->setAttribute("index", i);
        bandXml->setAttribute("threshold", static_cast<double>(bands[i].threshold));
        bandXml->setAttribute("ratio", static_cast<double>(bands[i].ratio));
        bandXml->setAttribute("attack", static_cast<double>(bands[i].attackMs));
        bandXml->setAttribute("release", static_cast<double>(bands[i].releaseMs));
        bandXml->setAttribute("makeupGain", static_cast<double>(bands[i].makeupGainDb));
        bandXml->setAttribute("solo", bands[i].solo);
        bandXml->setAttribute("bandBypass", bands[i].bandBypass);
        xml->addChildElement(bandXml.release());
    }

    return xml;
}

void MultibandCompressor::loadFromXml(const juce::XmlElement& xml)
{
    Effect::loadFromXml(xml);
    lowMidFreq = static_cast<float>(xml.getDoubleAttribute("lowMidFreq", 250.0));
    midHighFreq = static_cast<float>(xml.getDoubleAttribute("midHighFreq", 3000.0));

    for (auto* child : xml.getChildIterator())
    {
        if (child->hasTagName("BAND"))
        {
            int idx = child->getIntAttribute("index", -1);
            if (idx >= 0 && idx < NUM_BANDS)
            {
                bands[idx].threshold = static_cast<float>(child->getDoubleAttribute("threshold", -20.0));
                bands[idx].ratio = static_cast<float>(child->getDoubleAttribute("ratio", 4.0));
                bands[idx].attackMs = static_cast<float>(child->getDoubleAttribute("attack", 10.0));
                bands[idx].releaseMs = static_cast<float>(child->getDoubleAttribute("release", 100.0));
                bands[idx].makeupGainDb = static_cast<float>(child->getDoubleAttribute("makeupGain", 0.0));
                bands[idx].solo = child->getBoolAttribute("solo", false);
                bands[idx].bandBypass = child->getBoolAttribute("bandBypass", false);
            }
        }
    }

    updateCrossoverFilters();
    for (int i = 0; i < NUM_BANDS; ++i)
        updateBandCoefficients(i);
}
