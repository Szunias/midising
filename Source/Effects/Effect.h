#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

/**
 * Base class for all audio effects.
 */
class Effect
{
public:
    Effect(const juce::String& name) : effectName(name) {}
    virtual ~Effect() = default;

    virtual void prepareToPlay(double sampleRate, int samplesPerBlock) = 0;
    virtual void processBlock(juce::AudioBuffer<float>& buffer) = 0;
    virtual void releaseResources() = 0;

    const juce::String& getName() const { return effectName; }

    void setBypass(bool shouldBypass) { bypass = shouldBypass; }
    bool isBypassed() const { return bypass; }

    // Serialization
    virtual std::unique_ptr<juce::XmlElement> createXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("EFFECT");
        xml->setAttribute("name", effectName);
        xml->setAttribute("bypass", bypass);
        return xml;
    }

    virtual void loadFromXml(const juce::XmlElement& xml)
    {
        bypass = xml.getBoolAttribute("bypass", false);
    }

    // Parameters interface could be added here later (using AudioProcessorValueTreeState if needed)

protected:
    juce::String effectName;
    bool bypass = false;
    double currentSampleRate = 44100.0;
};
