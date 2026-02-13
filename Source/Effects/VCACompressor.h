#pragma once

#include "Effect.h"
#include <juce_dsp/juce_dsp.h>

/**
 * VCA-style Compressor effect with sidechain support.
 *
 * Features:
 * - Threshold, Ratio, Attack, Release, Knee controls
 * - Makeup gain
 * - Sidechain input support
 * - Gain reduction metering
 */
class VCACompressor : public Effect
{
public:
    VCACompressor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    // Process with external sidechain signal
    void processBlockWithSidechain(juce::AudioBuffer<float>& buffer,
                                   const juce::AudioBuffer<float>& sidechainBuffer);

    // Parameter setters
    void setThreshold(float thresholdDb);       // -60 to 0 dB
    void setRatio(float ratio);                 // 1:1 to 20:1
    void setAttack(float attackMs);             // 0.1 to 100 ms
    void setRelease(float releaseMs);           // 10 to 1000 ms
    void setKnee(float kneeDb);                 // 0 to 12 dB (soft knee width)
    void setMakeupGain(float gainDb);           // 0 to 24 dB

    // Parameter getters
    float getThreshold() const { return thresholdDb; }
    float getRatio() const { return ratio; }
    float getAttack() const { return attackMs; }
    float getRelease() const { return releaseMs; }
    float getKnee() const { return kneeDb; }
    float getMakeupGain() const { return makeupGainDb; }

    // Metering
    float getGainReduction() const { return currentGainReduction; }

    // Sidechain control
    void setSidechainEnabled(bool enabled) { sidechainEnabled = enabled; }
    bool isSidechainEnabled() const { return sidechainEnabled; }

    std::vector<EffectParameter> getParameters() const override
    {
        return {
            { "threshold",  "Threshold",   -60.0f, 0.0f,   -20.0f, thresholdDb,   0.1f, "dB", 1.0f },
            { "ratio",      "Ratio",       1.0f,   20.0f,  4.0f,   ratio,         0.1f, ":1", 2.0f },
            { "attack",     "Attack",      0.1f,   100.0f, 10.0f,  attackMs,      0.1f, "ms", 2.0f },
            { "release",    "Release",     10.0f,  1000.0f,100.0f, releaseMs,     1.0f, "ms", 2.0f },
            { "knee",       "Knee",        0.0f,   12.0f,  3.0f,   kneeDb,        0.1f, "dB", 1.0f },
            { "makeupGain", "Makeup Gain", 0.0f,   24.0f,  0.0f,   makeupGainDb,  0.1f, "dB", 1.0f }
        };
    }
    void setParameter(const juce::String& id, float value) override
    {
        if (id == "threshold")       setThreshold(value);
        else if (id == "ratio")      setRatio(value);
        else if (id == "attack")     setAttack(value);
        else if (id == "release")    setRelease(value);
        else if (id == "knee")       setKnee(value);
        else if (id == "makeupGain") setMakeupGain(value);
    }
    float getParameter(const juce::String& id) const override
    {
        if (id == "threshold")       return thresholdDb;
        if (id == "ratio")           return ratio;
        if (id == "attack")          return attackMs;
        if (id == "release")         return releaseMs;
        if (id == "knee")            return kneeDb;
        if (id == "makeupGain")      return makeupGainDb;
        return 0.0f;
    }

    bool supportsSidechain() const override { return true; }

    void setSidechainBuffer(const juce::AudioBuffer<float>* buffer) override
    {
        sidechainInputBuffer = buffer;
    }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    // Compute gain reduction for a given input level
    float computeGainReduction(float inputLevelDb) const;

    // Envelope follower
    float detectLevel(float sample);

    // Parameters
    float thresholdDb = -20.0f;
    float ratio = 4.0f;
    float attackMs = 10.0f;
    float releaseMs = 100.0f;
    float kneeDb = 3.0f;
    float makeupGainDb = 0.0f;

    // State
    float envelopeLevel = 0.0f;
    float currentGainReduction = 0.0f;  // For metering (in dB, positive value)

    // Computed coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float makeupGainLinear = 1.0f;

    // Sidechain
    bool sidechainEnabled = false;
    const juce::AudioBuffer<float>* sidechainInputBuffer = nullptr;

    void updateCoefficients();
};
