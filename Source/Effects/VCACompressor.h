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

    void updateCoefficients();
};
