#include "EffectChain.h"

void EffectChain::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock sl(processLock);
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    for (auto& effect : effects)
    {
        effect->prepareToPlay(sampleRate, samplesPerBlock);
    }
}

void EffectChain::processBlock(juce::AudioBuffer<float>& buffer)
{
    const juce::ScopedLock sl(processLock);
    
    for (auto& effect : effects)
    {
        if (!effect->isBypassed())
        {
            effect->processBlock(buffer);
        }
    }
}

void EffectChain::releaseResources()
{
    const juce::ScopedLock sl(processLock);
    
    for (auto& effect : effects)
    {
        effect->releaseResources();
    }
}

void EffectChain::addEffect(std::unique_ptr<Effect> effect)
{
    const juce::ScopedLock sl(processLock);
    effect->prepareToPlay(currentSampleRate, currentBlockSize);
    effects.push_back(std::move(effect));
}

void EffectChain::removeEffect(int index)
{
    const juce::ScopedLock sl(processLock);
    if (index >= 0 && index < static_cast<int>(effects.size()))
    {
        effects.erase(effects.begin() + index);
    }
}

std::unique_ptr<juce::XmlElement> EffectChain::createXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("EFFECTCHAIN");
    
    // Lock during iteration? Maybe not needed if we assume save happens on message thread/stop
    // But to be safe if playing
    // const juce::ScopedLock sl(processLock); // processLock is mutable or we create accessor
    
    for (const auto& effect : effects)
    {
        xml->addChildElement(effect->createXml().release());
    }
    
    return xml;
}

#include "GainEffect.h"
#include "SimpleEQEffect.h"
#include "ReverbEffect.h"
#include "ParametricEQ.h"
#include "VCACompressor.h"
#include "StereoDelay.h"
#include "NoiseGate.h"
#include "Limiter.h"
#include "ChorusFlanger.h"
#include "Saturation.h"
#include "Phaser.h"
#include "MultibandCompressor.h"
#include "DeEsser.h"

void EffectChain::loadFromXml(const juce::XmlElement& xml)
{
    clear();

    for (auto* child : xml.getChildIterator())
    {
        if (child->hasTagName("EFFECT"))
        {
            juce::String type = child->getStringAttribute("type");
            std::unique_ptr<Effect> effect;

            if (type == "GainEffect")
                effect = std::make_unique<GainEffect>();
            else if (type == "SimpleEQEffect")
                effect = std::make_unique<SimpleEQEffect>();
            else if (type == "ReverbEffect")
                effect = std::make_unique<ReverbEffect>();
            else if (type == "ParametricEQ")
                effect = std::make_unique<ParametricEQ>();
            else if (type == "VCACompressor")
                effect = std::make_unique<VCACompressor>();
            else if (type == "StereoDelay")
                effect = std::make_unique<StereoDelay>();
            else if (type == "NoiseGate")
                effect = std::make_unique<NoiseGate>();
            else if (type == "Limiter")
                effect = std::make_unique<Limiter>();
            else if (type == "ChorusFlanger")
                effect = std::make_unique<ChorusFlanger>();
            else if (type == "Saturation")
                effect = std::make_unique<Saturation>();
            else if (type == "Phaser")
                effect = std::make_unique<Phaser>();
            else if (type == "MultibandCompressor")
                effect = std::make_unique<MultibandCompressor>();
            else if (type == "DeEsser")
                effect = std::make_unique<DeEsser>();

            if (effect != nullptr)
            {
                effect->loadFromXml(*child);
                addEffect(std::move(effect));
            }
        }
    }
}

void EffectChain::moveEffect(int fromIndex, int toIndex)
{
    const juce::ScopedLock sl(processLock);
    if (fromIndex == toIndex) return;
    if (fromIndex < 0 || fromIndex >= static_cast<int>(effects.size())) return;
    if (toIndex < 0 || toIndex >= static_cast<int>(effects.size())) return;

    auto effect = std::move(effects[fromIndex]);
    effects.erase(effects.begin() + fromIndex);
    effects.insert(effects.begin() + toIndex, std::move(effect));
}

Effect* EffectChain::getEffect(int index)
{
    const juce::ScopedLock sl(processLock);
    if (index >= 0 && index < static_cast<int>(effects.size()))
        return effects[static_cast<size_t>(index)].get();
    return nullptr;
}

void EffectChain::clear()
{
    const juce::ScopedLock sl(processLock);
    effects.clear();
}
