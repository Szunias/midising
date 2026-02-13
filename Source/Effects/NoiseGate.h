#pragma once

#include "Effect.h"

/**
 * Noise Gate effect.
 *
 * Uses an envelope follower to detect signal level and gates
 * (attenuates) the signal when it falls below the threshold.
 *
 * Parameters: threshold, ratio, attack, release, hold time, range.
 */
class NoiseGate : public Effect
{
public:
    NoiseGate();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    // Parameter setters
    void setThreshold(float thresholdDb);       // -80 to 0 dB
    void setRatio(float ratio);                 // 1:1 to 10:1
    void setAttack(float attackMs);             // 0.01 to 50 ms
    void setRelease(float releaseMs);           // 5 to 500 ms
    void setHoldTime(float holdMs);             // 0 to 500 ms
    void setRange(float rangeDb);               // -80 to 0 dB (attenuation when closed)

    // Parameter getters
    float getThreshold() const { return thresholdDb; }
    float getRatio() const { return ratio; }
    float getAttack() const { return attackMs; }
    float getRelease() const { return releaseMs; }
    float getHoldTime() const { return holdMs; }
    float getRange() const { return rangeDb; }

    // Metering
    float getGateGain() const { return currentGateGain; }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    void updateCoefficients();

    // Parameters
    float thresholdDb = -40.0f;
    float ratio = 10.0f;
    float attackMs = 0.5f;
    float releaseMs = 50.0f;
    float holdMs = 10.0f;
    float rangeDb = -80.0f;

    // State
    float envelopeLevel = 0.0f;
    float currentGateGain = 1.0f;
    int holdCounter = 0;
    bool gateOpen = false;

    // Computed coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    int holdSamples = 0;
};
