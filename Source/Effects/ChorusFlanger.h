#pragma once

#include "Effect.h"
#include <vector>

/**
 * Combined Chorus/Flanger effect.
 *
 * Mode: Chorus uses longer delay (10-30ms) with no feedback.
 *       Flanger uses short delay (1-10ms) with feedback.
 *
 * DSP: LFO-modulated delay line.
 *
 * Parameters: rate (Hz), depth, feedback, mix, delay offset.
 */
class ChorusFlanger : public Effect
{
public:
    enum class Mode
    {
        Chorus = 0,
        Flanger = 1
    };

    ChorusFlanger();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    // Parameter setters
    void setMode(Mode mode);
    void setRate(float rateHz);             // 0.1 to 10 Hz
    void setDepth(float depth);             // 0.0 to 1.0
    void setFeedback(float feedback);       // 0.0 to 0.95
    void setMix(float mix);                 // 0.0 to 1.0
    void setDelayOffset(float offsetMs);    // Base delay offset (ms)

    // Parameter getters
    Mode getMode() const { return mode; }
    float getRate() const { return rateHz; }
    float getDepth() const { return depth; }
    float getFeedback() const { return feedback; }
    float getMix() const { return mix; }
    float getDelayOffset() const { return delayOffsetMs; }

    std::vector<EffectParameter> getParameters() const override
    {
        return {
            { "rate",     "Rate",     0.1f, 10.0f,  0.5f,   rateHz,        0.01f, "Hz", 2.0f },
            { "depth",    "Depth",    0.0f, 1.0f,   0.5f,   depth,         0.01f, "",   1.0f },
            { "feedback", "Feedback", 0.0f, 0.95f,  0.0f,   feedback,      0.01f, "",   1.0f },
            { "mix",      "Mix",      0.0f, 1.0f,   0.5f,   mix,           0.01f, "",   1.0f },
            { "delay",    "Delay",    1.0f, 50.0f,  20.0f,  delayOffsetMs, 0.1f,  "ms", 2.0f }
        };
    }
    void setParameter(const juce::String& id, float value) override
    {
        if (id == "rate")            setRate(value);
        else if (id == "depth")      setDepth(value);
        else if (id == "feedback")   setFeedback(value);
        else if (id == "mix")        setMix(value);
        else if (id == "delay")      setDelayOffset(value);
    }
    float getParameter(const juce::String& id) const override
    {
        if (id == "rate")            return rateHz;
        if (id == "depth")           return depth;
        if (id == "feedback")        return feedback;
        if (id == "mix")             return mix;
        if (id == "delay")           return delayOffsetMs;
        return 0.0f;
    }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    float readDelayLine(const std::vector<float>& buffer, float delaySamples) const;

    // Parameters
    Mode mode = Mode::Chorus;
    float rateHz = 0.5f;
    float depth = 0.5f;
    float feedback = 0.0f;
    float mix = 0.5f;
    float delayOffsetMs = 20.0f;  // Default for chorus

    // LFO state
    float lfoPhase = 0.0f;
    float lfoIncrement = 0.0f;

    // Delay line
    static constexpr float maxDelayMs = 50.0f;
    std::vector<float> delayLineL;
    std::vector<float> delayLineR;
    int delayBufferSize = 0;
    int writePos = 0;

    // Feedback state
    float feedbackL = 0.0f;
    float feedbackR = 0.0f;
};
