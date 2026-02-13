#pragma once

#include "Effect.h"
#include <juce_audio_basics/juce_audio_basics.h>

class ReverbEffect : public Effect
{
public:
    ReverbEffect();
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    void setRoomSize(float size); // 0.0 to 1.0
    void setDamping(float damping); // 0.0 to 1.0
    void setWetLevel(float wet); // 0.0 to 1.0
    void setDryLevel(float dry); // 0.0 to 1.0
    void setWidth(float width); // 0.0 to 1.0

    std::vector<EffectParameter> getParameters() const override
    {
        return {
            { "roomSize", "Room Size", 0.0f, 1.0f, 0.5f, params.roomSize, 0.01f, "", 1.0f },
            { "damping",  "Damping",   0.0f, 1.0f, 0.5f, params.damping,  0.01f, "", 1.0f },
            { "wetLevel", "Wet Level", 0.0f, 1.0f, 0.33f, params.wetLevel, 0.01f, "", 1.0f },
            { "dryLevel", "Dry Level", 0.0f, 1.0f, 0.4f, params.dryLevel,  0.01f, "", 1.0f },
            { "width",    "Width",     0.0f, 1.0f, 1.0f, params.width,    0.01f, "", 1.0f }
        };
    }
    void setParameter(const juce::String& id, float value) override
    {
        if (id == "roomSize")      setRoomSize(value);
        else if (id == "damping")  setDamping(value);
        else if (id == "wetLevel") setWetLevel(value);
        else if (id == "dryLevel") setDryLevel(value);
        else if (id == "width")    setWidth(value);
    }
    float getParameter(const juce::String& id) const override
    {
        if (id == "roomSize")      return params.roomSize;
        if (id == "damping")       return params.damping;
        if (id == "wetLevel")      return params.wetLevel;
        if (id == "dryLevel")      return params.dryLevel;
        if (id == "width")         return params.width;
        return 0.0f;
    }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;
    void setFreezeMode(float freeze); // 0.0 to 1.0

private:
    juce::Reverb reverb;
    juce::Reverb::Parameters params;
};
