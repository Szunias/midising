#include "ReverbEffect.h"

ReverbEffect::ReverbEffect() 
    : Effect("Reverb")
{
    params.roomSize = 0.5f;
    params.damping = 0.5f;
    params.wetLevel = 0.33f;
    params.dryLevel = 0.4f;
    params.width = 1.0f;
    params.freezeMode = 0.0f;
    reverb.setParameters(params);
}

void ReverbEffect::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    reverb.setSampleRate(sampleRate);
}

void ReverbEffect::releaseResources()
{
    reverb.reset();
}

void ReverbEffect::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 1)
    {
        reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
    }
    else if (buffer.getNumChannels() == 2)
    {
        reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
    }
}

void ReverbEffect::setRoomSize(float size)
{
    params.roomSize = size;
    reverb.setParameters(params);
}

void ReverbEffect::setDamping(float damping)
{
    params.damping = damping;
    reverb.setParameters(params);
}

void ReverbEffect::setWetLevel(float wet)
{
    params.wetLevel = wet;
    reverb.setParameters(params);
}

void ReverbEffect::setDryLevel(float dry)
{
    params.dryLevel = dry;
    reverb.setParameters(params);
}

void ReverbEffect::setWidth(float width)
{
    params.width = width;
    reverb.setParameters(params);
}

void ReverbEffect::setFreezeMode(float freeze)
{
    params.freezeMode = freeze;
    reverb.setParameters(params);
}

std::unique_ptr<juce::XmlElement> ReverbEffect::createXml() const
{
    auto xml = Effect::createXml();
    xml->setAttribute("type", "ReverbEffect");
    xml->setAttribute("roomSize", params.roomSize);
    xml->setAttribute("damping", params.damping);
    xml->setAttribute("wetLevel", params.wetLevel);
    xml->setAttribute("dryLevel", params.dryLevel);
    xml->setAttribute("width", params.width);
    xml->setAttribute("freezeMode", params.freezeMode);
    return xml;
}

void ReverbEffect::loadFromXml(const juce::XmlElement& xml)
{
    Effect::loadFromXml(xml);
    params.roomSize = static_cast<float>(xml.getDoubleAttribute("roomSize", 0.5));
    params.damping = static_cast<float>(xml.getDoubleAttribute("damping", 0.5));
    params.wetLevel = static_cast<float>(xml.getDoubleAttribute("wetLevel", 0.33));
    params.dryLevel = static_cast<float>(xml.getDoubleAttribute("dryLevel", 0.4));
    params.width = static_cast<float>(xml.getDoubleAttribute("width", 1.0));
    params.freezeMode = static_cast<float>(xml.getDoubleAttribute("freezeMode", 0.0));
    reverb.setParameters(params);
}
