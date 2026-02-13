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

    std::vector<EffectParameter> getParameters() const override
    {
        return {
            { "lowGain",  "Low Gain",      -12.0f, 12.0f, 0.0f, lowGainDb,  0.1f, "dB", 1.0f },
            { "midGain",  "Mid Gain",      -12.0f, 12.0f, 0.0f, midGainDb,  0.1f, "dB", 1.0f },
            { "highGain", "High Gain",     -12.0f, 12.0f, 0.0f, highGainDb, 0.1f, "dB", 1.0f },
            { "midFreq",  "Mid Frequency", 200.0f, 8000.0f, 1000.0f, midFreq, 1.0f, "Hz", 2.0f }
        };
    }
    void setParameter(const juce::String& id, float value) override
    {
        if (id == "lowGain")       setLowGain(value);
        else if (id == "midGain")  setMidGain(value);
        else if (id == "highGain") setHighGain(value);
        else if (id == "midFreq")  setMidFrequency(value);
    }
    float getParameter(const juce::String& id) const override
    {
        if (id == "lowGain")       return lowGainDb;
        if (id == "midGain")       return midGainDb;
        if (id == "highGain")      return highGainDb;
        if (id == "midFreq")       return midFreq;
        return 0.0f;
    }

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
