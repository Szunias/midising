#include "SimpleEQEffect.h"

SimpleEQEffect::SimpleEQEffect() 
    : Effect("Simple EQ")
{
    updateFilters();
}

void SimpleEQEffect::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2; // Default to stereo

    processorChain.prepare(spec);
    updateFilters();
}

void SimpleEQEffect::releaseResources()
{
    processorChain.reset();
}

void SimpleEQEffect::processBlock(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    processorChain.process(context);
}

void SimpleEQEffect::setLowGain(float gainDecibels)
{
    lowGainDb = gainDecibels;
    updateFilters();
}

void SimpleEQEffect::setHighGain(float gainDecibels)
{
    highGainDb = gainDecibels;
    updateFilters();
}

void SimpleEQEffect::setMidGain(float gainDecibels)
{
    midGainDb = gainDecibels;
    updateFilters();
}

void SimpleEQEffect::setMidFrequency(float frequency)
{
    midFreq = frequency;
    updateFilters();
}

void SimpleEQEffect::updateFilters()
{
    if (currentSampleRate <= 0.0) return;

    *processorChain.get<0>().state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(currentSampleRate, 200.0f, 0.707f, juce::Decibels::decibelsToGain(lowGainDb));
    *processorChain.get<1>().state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, midFreq, 1.0f, juce::Decibels::decibelsToGain(midGainDb));
    *processorChain.get<2>().state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(currentSampleRate, 4000.0f, 0.707f, juce::Decibels::decibelsToGain(highGainDb));
}

std::unique_ptr<juce::XmlElement> SimpleEQEffect::createXml() const
{
    auto xml = Effect::createXml();
    xml->setAttribute("type", "SimpleEQEffect");
    xml->setAttribute("lowGainDb", lowGainDb);
    xml->setAttribute("midGainDb", midGainDb);
    xml->setAttribute("highGainDb", highGainDb);
    xml->setAttribute("midFreq", midFreq);
    return xml;
}

void SimpleEQEffect::loadFromXml(const juce::XmlElement& xml)
{
    Effect::loadFromXml(xml);
    lowGainDb = static_cast<float>(xml.getDoubleAttribute("lowGainDb", 0.0));
    midGainDb = static_cast<float>(xml.getDoubleAttribute("midGainDb", 0.0));
    highGainDb = static_cast<float>(xml.getDoubleAttribute("highGainDb", 0.0));
    midFreq = static_cast<float>(xml.getDoubleAttribute("midFreq", 1000.0));
    updateFilters();
}
