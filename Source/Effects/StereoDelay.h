#pragma once

#include "Effect.h"
#include <juce_dsp/juce_dsp.h>

/**
 * Stereo Delay effect with BPM-sync capability.
 *
 * Features:
 * - Independent left/right delay times
 * - BPM-synced delay times (note values: 1/4, 1/8, 1/16, dotted, triplet)
 * - Feedback control
 * - Low-pass filter on feedback path
 * - Ping-pong mode
 * - Dry/wet mix control
 */
class StereoDelay : public Effect
{
public:
    /**
     * Note division values for tempo-synced delays
     */
    enum class NoteDivision
    {
        Quarter = 0,        // 1/4 note
        Eighth,             // 1/8 note
        Sixteenth,          // 1/16 note
        ThirtySecond,       // 1/32 note
        DottedQuarter,      // Dotted 1/4
        DottedEighth,       // Dotted 1/8
        DottedSixteenth,    // Dotted 1/16
        TripletQuarter,     // Triplet 1/4
        TripletEighth,      // Triplet 1/8
        TripletSixteenth    // Triplet 1/16
    };

    StereoDelay();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    // Tempo sync
    void setTempoSyncEnabled(bool enabled);
    bool isTempoSyncEnabled() const { return tempoSyncEnabled; }
    void setBPM(double newBpm);
    double getBPM() const { return bpm; }

    // Delay time settings (in milliseconds, used when tempo sync is off)
    void setDelayTimeLeftMs(float timeMs);
    void setDelayTimeRightMs(float timeMs);
    float getDelayTimeLeftMs() const { return delayTimeLeftMs; }
    float getDelayTimeRightMs() const { return delayTimeRightMs; }

    // Note division settings (used when tempo sync is on)
    void setNoteDivisionLeft(NoteDivision division);
    void setNoteDivisionRight(NoteDivision division);
    NoteDivision getNoteDivisionLeft() const { return noteDivisionLeft; }
    NoteDivision getNoteDivisionRight() const { return noteDivisionRight; }

    // Common parameters
    void setFeedback(float feedback);     // 0.0 to 0.95
    void setMix(float mix);               // 0.0 to 1.0 (dry/wet)
    void setFilterCutoff(float cutoffHz); // Low-pass filter on feedback
    void setPingPong(bool enabled);

    float getFeedback() const { return feedback; }
    float getMix() const { return mix; }
    float getFilterCutoff() const { return filterCutoffHz; }
    bool isPingPong() const { return pingPongEnabled; }

    std::vector<EffectParameter> getParameters() const override
    {
        return {
            { "delayL",    "Delay L",     1.0f,   2000.0f, 250.0f, delayTimeLeftMs,  1.0f, "ms", 2.0f },
            { "delayR",    "Delay R",     1.0f,   2000.0f, 250.0f, delayTimeRightMs, 1.0f, "ms", 2.0f },
            { "feedback",  "Feedback",    0.0f,   0.95f,   0.4f,   feedback,         0.01f, "", 1.0f },
            { "mix",       "Mix",         0.0f,   1.0f,    0.5f,   mix,              0.01f, "", 1.0f },
            { "filterCutoff", "Filter",   200.0f, 16000.0f, 8000.0f, filterCutoffHz, 1.0f, "Hz", 2.0f }
        };
    }
    void setParameter(const juce::String& id, float value) override
    {
        if (id == "delayL")           setDelayTimeLeftMs(value);
        else if (id == "delayR")      setDelayTimeRightMs(value);
        else if (id == "feedback")    setFeedback(value);
        else if (id == "mix")         setMix(value);
        else if (id == "filterCutoff") setFilterCutoff(value);
    }
    float getParameter(const juce::String& id) const override
    {
        if (id == "delayL")           return delayTimeLeftMs;
        if (id == "delayR")           return delayTimeRightMs;
        if (id == "feedback")         return feedback;
        if (id == "mix")              return mix;
        if (id == "filterCutoff")     return filterCutoffHz;
        return 0.0f;
    }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

    // Helper to get note division name
    static juce::String getNoteDivisionName(NoteDivision division);

private:
    // Calculate delay time in samples based on current settings
    int calculateDelayTimeSamples(bool isLeft) const;

    // Convert note division to multiplier relative to quarter note
    static float getNoteDivisionMultiplier(NoteDivision division);

    // Update internal delay lines
    void updateDelayTimes();

    // Parameters
    bool tempoSyncEnabled = false;
    double bpm = 120.0;

    float delayTimeLeftMs = 250.0f;   // Manual delay time left
    float delayTimeRightMs = 250.0f;  // Manual delay time right

    NoteDivision noteDivisionLeft = NoteDivision::Eighth;
    NoteDivision noteDivisionRight = NoteDivision::Eighth;

    float feedback = 0.4f;            // 0.0 to 0.95
    float mix = 0.5f;                 // Dry/wet mix
    float filterCutoffHz = 8000.0f;   // Low-pass filter cutoff
    bool pingPongEnabled = false;

    // Maximum delay time (2 seconds at sample rate)
    static constexpr float maxDelayTimeMs = 2000.0f;

    // Delay buffers
    std::vector<float> delayBufferLeft;
    std::vector<float> delayBufferRight;
    int delayBufferSize = 0;
    int writePositionLeft = 0;
    int writePositionRight = 0;
    int delayTimeSamplesLeft = 0;
    int delayTimeSamplesRight = 0;

    // Feedback filters (one-pole low-pass)
    float filterStateLeft = 0.0f;
    float filterStateRight = 0.0f;
    float filterCoeff = 0.0f;

    void updateFilterCoeff();
};
