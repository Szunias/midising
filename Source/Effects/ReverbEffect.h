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

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;
    void setFreezeMode(float freeze); // 0.0 to 1.0

private:
    juce::Reverb reverb;
    juce::Reverb::Parameters params;
};
