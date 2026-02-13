#pragma once

#include "Effect.h"
#include <vector>
#include <memory>

/**
 * EffectChain holds a series of Effects and processes them sequentially.
 */
class EffectChain
{
public:
    EffectChain() = default;
    ~EffectChain() = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void releaseResources();

    void addEffect(std::unique_ptr<Effect> effect);
    void removeEffect(int index);
    void moveEffect(int fromIndex, int toIndex);
    
    Effect* getEffect(int index);
    int getNumEffects() const { return static_cast<int>(effects.size()); }
    void clear();

    /** Sum of all effect latencies in the chain. */
    int getTotalLatencySamples() const
    {
        int total = 0;
        for (const auto& effect : effects)
            total += effect->getLatencySamples();
        return total;
    }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const;
    void loadFromXml(const juce::XmlElement& xml);

private:
    std::vector<std::unique_ptr<Effect>> effects;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    
    juce::CriticalSection processLock;
};
