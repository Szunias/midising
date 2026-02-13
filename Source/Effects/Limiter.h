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

    std::vector<EffectParameter> getParameters() const override
    {
        return {
            { "ceiling",   "Ceiling",    -12.0f, 0.0f,   -0.3f,  ceilingDb,    0.1f, "dB", 1.0f },
            { "release",   "Release",    1.0f,   500.0f, 100.0f, releaseMs,    1.0f, "ms", 2.0f },
            { "inputGain", "Input Gain", -12.0f, 24.0f,  0.0f,   inputGainDb,  0.1f, "dB", 1.0f }
        };
    }
    void setParameter(const juce::String& id, float value) override
    {
        if (id == "ceiling")         setCeiling(value);
        else if (id == "release")    setRelease(value);
        else if (id == "inputGain")  setInputGain(value);
    }
    float getParameter(const juce::String& id) const override
    {
        if (id == "ceiling")         return ceilingDb;
        if (id == "release")         return releaseMs;
        if (id == "inputGain")       return inputGainDb;
        return 0.0f;
    }

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
