#pragma once

#include "Effect.h"
#include <juce_dsp/juce_dsp.h>

/**
 * Saturation/Distortion effect with oversampling.
 *
 * Supports multiple waveshaping types: Soft (tanh), Hard (clip),
 * Tape (asymmetric), and Tube (asymmetric warm).
 *
 * Uses 2x oversampling to reduce aliasing artifacts.
 *
 * Parameters: drive (dB), mix, output level.
 */
class Saturation : public Effect
{
public:
    enum class Type
    {
        Soft = 0,    // tanh waveshaping
        Hard = 1,    // hard clip
        Tape = 2,    // asymmetric tape-like
        Tube = 3     // asymmetric tube-like
    };

    Saturation();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    // Parameter setters
    void setSaturationType(Type type);
    void setDrive(float driveDb);           // 0 to 36 dB
    void setMix(float mix);                 // 0.0 to 1.0
    void setOutputLevel(float levelDb);     // -24 to 0 dB

    // Parameter getters
    Type getSaturationType() const { return satType; }
    float getDrive() const { return driveDb; }
    float getMix() const { return mix; }
    float getOutputLevel() const { return outputLevelDb; }

    std::vector<EffectParameter> getParameters() const override
    {
        return {
            { "drive",  "Drive",  0.0f,  36.0f, 12.0f, driveDb,        0.1f, "dB", 1.0f },
            { "mix",    "Mix",    0.0f,  1.0f,  1.0f,  mix,            0.01f, "",  1.0f },
            { "output", "Output", -24.0f, 0.0f, 0.0f,  outputLevelDb,  0.1f, "dB", 1.0f },
            { "type",   "Type",   0.0f,  3.0f,  0.0f,  static_cast<float>(satType), 1.0f, "", 1.0f }
        };
    }
    void setParameter(const juce::String& id, float value) override
    {
        if (id == "drive")           setDrive(value);
        else if (id == "mix")        setMix(value);
        else if (id == "output")     setOutputLevel(value);
        else if (id == "type")       setSaturationType(static_cast<Type>(static_cast<int>(value)));
    }
    float getParameter(const juce::String& id) const override
    {
        if (id == "drive")           return driveDb;
        if (id == "mix")             return mix;
        if (id == "output")          return outputLevelDb;
        if (id == "type")            return static_cast<float>(satType);
        return 0.0f;
    }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    float waveshape(float input) const;

    // Parameters
    Type satType = Type::Soft;
    float driveDb = 12.0f;
    float mix = 1.0f;
    float outputLevelDb = 0.0f;

    // Computed
    float driveLinear = 1.0f;
    float outputLinear = 1.0f;

    // Oversampling (2x)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    bool oversamplingReady = false;
};
