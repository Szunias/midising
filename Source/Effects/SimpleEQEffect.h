#pragma once

#include "Effect.h"
#include <juce_dsp/juce_dsp.h>

class SimpleEQEffect : public Effect
{
public:
    SimpleEQEffect();
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    void setLowGain(float gainDecibels);
    void setHighGain(float gainDecibels);
    void setMidGain(float gainDecibels);
    void setMidFrequency(float frequency);

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    void updateFilters();

    using FilterType = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<FilterType, Coefficients>;
    
    juce::dsp::ProcessorChain<StereoFilter, StereoFilter, StereoFilter> processorChain;
    
    // Parameters
    float lowGainDb = 0.0f;
    float highGainDb = 0.0f;
    float midGainDb = 0.0f;
    float midFreq = 1000.0f;
};
