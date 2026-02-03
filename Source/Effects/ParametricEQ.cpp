#include "ParametricEQ.h"

// Static constexpr definition (required for C++14 compatibility)
constexpr float ParametricEQ::defaultFrequencies[NumBands];

ParametricEQ::ParametricEQ()
    : Effect("Parametric EQ")
{
    // Initialize default frequencies for each band
    bands[LowShelf].frequency = defaultFrequencies[LowShelf];
    bands[LowShelf].q = 0.707f;

    bands[LowMid].frequency = defaultFrequencies[LowMid];
    bands[LowMid].q = 1.0f;

    bands[Mid].frequency = defaultFrequencies[Mid];
    bands[Mid].q = 1.0f;

    bands[HighMid].frequency = defaultFrequencies[HighMid];
    bands[HighMid].q = 1.0f;

    bands[HighShelf].frequency = defaultFrequencies[HighShelf];
    bands[HighShelf].q = 0.707f;

    updateFilters();
}

void ParametricEQ::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2; // Default to stereo

    processorChain.prepare(spec);
    updateFilters();
}

void ParametricEQ::releaseResources()
{
    processorChain.reset();
}

void ParametricEQ::processBlock(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    processorChain.process(context);
}

void ParametricEQ::setBandFrequency(int bandIndex, float frequency)
{
    if (bandIndex < 0 || bandIndex >= NumBands) return;

    // Clamp frequency to valid range (20 Hz to Nyquist/2)
    float maxFreq = static_cast<float>(currentSampleRate * 0.4);
    bands[bandIndex].frequency = juce::jlimit(20.0f, maxFreq, frequency);
    updateBand(bandIndex);
}

void ParametricEQ::setBandGain(int bandIndex, float gainDecibels)
{
    if (bandIndex < 0 || bandIndex >= NumBands) return;

    // Clamp gain to reasonable range (-24 dB to +24 dB)
    bands[bandIndex].gainDb = juce::jlimit(-24.0f, 24.0f, gainDecibels);
    updateBand(bandIndex);
}

void ParametricEQ::setBandQ(int bandIndex, float q)
{
    if (bandIndex < 0 || bandIndex >= NumBands) return;

    // Clamp Q to reasonable range (0.1 to 10)
    bands[bandIndex].q = juce::jlimit(0.1f, 10.0f, q);
    updateBand(bandIndex);
}

void ParametricEQ::setBandEnabled(int bandIndex, bool enabled)
{
    if (bandIndex < 0 || bandIndex >= NumBands) return;

    bands[bandIndex].enabled = enabled;
    updateBand(bandIndex);
}

const ParametricEQ::BandParams& ParametricEQ::getBandParams(int bandIndex) const
{
    static BandParams defaultParams;
    if (bandIndex < 0 || bandIndex >= NumBands)
        return defaultParams;
    return bands[bandIndex];
}

void ParametricEQ::setBandParams(int bandIndex, float frequency, float gainDecibels, float q)
{
    if (bandIndex < 0 || bandIndex >= NumBands) return;

    float maxFreq = static_cast<float>(currentSampleRate * 0.4);
    bands[bandIndex].frequency = juce::jlimit(20.0f, maxFreq, frequency);
    bands[bandIndex].gainDb = juce::jlimit(-24.0f, 24.0f, gainDecibels);
    bands[bandIndex].q = juce::jlimit(0.1f, 10.0f, q);
    updateBand(bandIndex);
}

void ParametricEQ::updateFilters()
{
    for (int i = 0; i < NumBands; ++i)
    {
        updateBand(i);
    }
}

void ParametricEQ::updateBand(int bandIndex)
{
    if (currentSampleRate <= 0.0) return;
    if (bandIndex < 0 || bandIndex >= NumBands) return;

    const auto& band = bands[bandIndex];
    float gain = band.enabled ? juce::Decibels::decibelsToGain(band.gainDb) : 1.0f;

    // Create coefficients based on band type
    juce::dsp::IIR::Coefficients<float>::Ptr coefficients;

    switch (bandIndex)
    {
        case LowShelf:
            coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                currentSampleRate, band.frequency, band.q, gain);
            *processorChain.get<0>().state = *coefficients;
            break;

        case LowMid:
            coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                currentSampleRate, band.frequency, band.q, gain);
            *processorChain.get<1>().state = *coefficients;
            break;

        case Mid:
            coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                currentSampleRate, band.frequency, band.q, gain);
            *processorChain.get<2>().state = *coefficients;
            break;

        case HighMid:
            coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                currentSampleRate, band.frequency, band.q, gain);
            *processorChain.get<3>().state = *coefficients;
            break;

        case HighShelf:
            coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                currentSampleRate, band.frequency, band.q, gain);
            *processorChain.get<4>().state = *coefficients;
            break;

        default:
            break;
    }
}

std::unique_ptr<juce::XmlElement> ParametricEQ::createXml() const
{
    auto xml = Effect::createXml();
    xml->setAttribute("type", "ParametricEQ");

    for (int i = 0; i < NumBands; ++i)
    {
        auto bandXml = std::make_unique<juce::XmlElement>("BAND");
        bandXml->setAttribute("index", i);
        bandXml->setAttribute("frequency", bands[i].frequency);
        bandXml->setAttribute("gainDb", bands[i].gainDb);
        bandXml->setAttribute("q", bands[i].q);
        bandXml->setAttribute("enabled", bands[i].enabled);
        xml->addChildElement(bandXml.release());
    }

    return xml;
}

void ParametricEQ::loadFromXml(const juce::XmlElement& xml)
{
    Effect::loadFromXml(xml);

    for (auto* bandXml : xml.getChildIterator())
    {
        if (bandXml->hasTagName("BAND"))
        {
            int index = bandXml->getIntAttribute("index", -1);
            if (index >= 0 && index < NumBands)
            {
                bands[index].frequency = static_cast<float>(bandXml->getDoubleAttribute("frequency", defaultFrequencies[index]));
                bands[index].gainDb = static_cast<float>(bandXml->getDoubleAttribute("gainDb", 0.0));
                bands[index].q = static_cast<float>(bandXml->getDoubleAttribute("q", index == LowShelf || index == HighShelf ? 0.707 : 1.0));
                bands[index].enabled = bandXml->getBoolAttribute("enabled", true);
            }
        }
    }

    updateFilters();
}
