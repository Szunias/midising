#pragma once

#include "Effect.h"
#include <vector>
#include <cmath>

/**
 * Phaser effect using all-pass filter chain with LFO-modulated cutoff.
 *
 * Parameters: rate (0.1-5 Hz), depth (0-1), feedback (0-0.95),
 *             mix (0-1), stages (2/4/6/8/10/12), centerFreq (200-5000 Hz).
 */
class Phaser : public Effect
{
public:
    Phaser();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    // Parameter setters
    void setRate(float rateHz);           // 0.1 to 5.0 Hz
    void setDepth(float depth);           // 0.0 to 1.0
    void setFeedback(float feedback);     // 0.0 to 0.95
    void setMix(float mix);              // 0.0 to 1.0
    void setNumStages(int stages);        // 2, 4, 6, 8, 10, 12
    void setCenterFreq(float freq);       // 200 to 5000 Hz

    // Parameter getters
    float getRate() const { return rateHz; }
    float getDepth() const { return depth; }
    float getFeedback() const { return feedback; }
    float getMix() const { return mix; }
    int getNumStages() const { return numStages; }
    float getCenterFreq() const { return centerFreq; }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    struct AllPassFilter
    {
        float a1 = 0.0f;
        float zm1 = 0.0f;

        float process(float input)
        {
            float output = input * -a1 + zm1;
            zm1 = output * a1 + input;
            return output;
        }

        void reset() { zm1 = 0.0f; }
    };

    // Parameters
    float rateHz = 0.5f;
    float depth = 0.7f;
    float feedback = 0.3f;
    float mix = 0.5f;
    int numStages = 6;
    float centerFreq = 1000.0f;

    // LFO state
    float lfoPhase = 0.0f;
    float lfoIncrement = 0.0f;

    // All-pass filter stages (per channel, up to 12 stages)
    static constexpr int MAX_STAGES = 12;
    AllPassFilter allPassL[MAX_STAGES];
    AllPassFilter allPassR[MAX_STAGES];

    // Feedback state
    float feedbackSampleL = 0.0f;
    float feedbackSampleR = 0.0f;
};
