#pragma once

#include "Effect.h"
#include <vector>

/**
 * Peak Limiter effect with look-ahead.
 *
 * Uses a small circular delay buffer (1-5ms) for look-ahead
 * to achieve brickwall limiting without harsh clipping.
 *
 * Parameters: ceiling (dB), release, input gain.
 */
class Limiter : public Effect
{
public:
    Limiter();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    // Latency reporting
    int getLatencySamples() const { return lookAheadSamples; }

    // Parameter setters
    void setCeiling(float ceilingDb);           // -12 to 0 dB
    void setRelease(float releaseMs);           // 1 to 500 ms
    void setInputGain(float gainDb);            // -12 to +24 dB

    // Parameter getters
    float getCeiling() const { return ceilingDb; }
    float getRelease() const { return releaseMs; }
    float getInputGain() const { return inputGainDb; }

    // Metering
    float getGainReduction() const { return currentGainReduction; }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    void updateCoefficients();

    // Parameters
    float ceilingDb = -0.3f;
    float releaseMs = 100.0f;
    float inputGainDb = 0.0f;

    // Look-ahead delay buffer
    static constexpr float lookAheadMs = 3.0f;
    int lookAheadSamples = 0;
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int delayWritePos = 0;

    // Gain state
    float currentGain = 1.0f;
    float currentGainReduction = 0.0f;

    // Computed coefficients
    float releaseCoeff = 0.0f;
    float inputGainLinear = 1.0f;
    float ceilingLinear = 1.0f;
};
