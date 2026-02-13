#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include "EffectParameter.h"
#include <vector>

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

    virtual int getLatencySamples() const { return 0; }

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

    // Parameter descriptor system for auto-generated UI and automation
    virtual std::vector<EffectParameter> getParameters() const { return {}; }
    virtual void setParameter(const juce::String& id, float value) { juce::ignoreUnused(id, value); }
    virtual float getParameter(const juce::String& id) const { juce::ignoreUnused(id); return 0.0f; }

    // Sidechain support
    virtual bool supportsSidechain() const { return false; }
    virtual void setSidechainBuffer(const juce::AudioBuffer<float>* buffer) { juce::ignoreUnused(buffer); }

protected:
    juce::String effectName;
    bool bypass = false;
    double currentSampleRate = 44100.0;
};
