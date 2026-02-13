#pragma once

#include "Effect.h"

class GainEffect : public Effect
{
public:
    GainEffect() : Effect("Gain") {}
    
    void prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/) override {}
    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer) override
    {
        buffer.applyGain(gain);
    }

    void setGain(float newGain) { gain = newGain; }
    void setGainDecibels(float db) { gain = juce::Decibels::decibelsToGain(db); }
    float getGain() const { return gain; }

    std::vector<EffectParameter> getParameters() const override
    {
        return { { "gain", "Gain", -24.0f, 24.0f, 0.0f, juce::Decibels::gainToDecibels(gain), 0.1f, "dB", 1.0f } };
    }
    void setParameter(const juce::String& id, float value) override
    {
        if (id == "gain") setGainDecibels(value);
    }
    float getParameter(const juce::String& id) const override
    {
        if (id == "gain") return juce::Decibels::gainToDecibels(gain);
        return 0.0f;
    }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override
    {
        auto xml = Effect::createXml();
        xml->setAttribute("type", "GainEffect");
        xml->setAttribute("gain", gain);
        return xml;
    }

    void loadFromXml(const juce::XmlElement& xml) override
    {
        Effect::loadFromXml(xml);
        gain = static_cast<float>(xml.getDoubleAttribute("gain", 1.0));
    }

private:
    float gain = 1.0f;
};
