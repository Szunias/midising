#pragma once

#include "Effect.h"
#include <juce_dsp/juce_dsp.h>

/**
 * 5-band Parametric EQ effect.
 *
 * Band 1: Low Shelf
 * Band 2: Low-Mid Peak
 * Band 3: Mid Peak
 * Band 4: High-Mid Peak
 * Band 5: High Shelf
 *
 * Each band has adjustable frequency, gain (dB), and Q (bandwidth).
 */
class ParametricEQ : public Effect
{
public:
    ParametricEQ();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    // Band indices
    enum Band
    {
        LowShelf = 0,
        LowMid = 1,
        Mid = 2,
        HighMid = 3,
        HighShelf = 4,
        NumBands = 5
    };

    // Band parameters
    struct BandParams
    {
        float frequency = 1000.0f;
        float gainDb = 0.0f;
        float q = 0.707f;
        bool enabled = true;
    };

    // Parameter setters for individual bands
    void setBandFrequency(int bandIndex, float frequency);
    void setBandGain(int bandIndex, float gainDecibels);
    void setBandQ(int bandIndex, float q);
    void setBandEnabled(int bandIndex, bool enabled);

    // Get band parameters
    const BandParams& getBandParams(int bandIndex) const;

    // Convenience setters for all parameters at once
    void setBandParams(int bandIndex, float frequency, float gainDecibels, float q);

    std::vector<EffectParameter> getParameters() const override
    {
        std::vector<EffectParameter> p;
        const char* bandNames[] = { "LowShelf", "LowMid", "Mid", "HighMid", "HighShelf" };
        for (int i = 0; i < NumBands; ++i)
        {
            juce::String prefix = juce::String(bandNames[i]);
            p.push_back({ prefix + "_freq", prefix + " Freq", 20.0f, 20000.0f, defaultFrequencies[i], bands[i].frequency, 1.0f, "Hz", 2.0f });
            p.push_back({ prefix + "_gain", prefix + " Gain", -18.0f, 18.0f, 0.0f, bands[i].gainDb, 0.1f, "dB", 1.0f });
            p.push_back({ prefix + "_q",    prefix + " Q",    0.1f, 10.0f, 0.707f, bands[i].q, 0.01f, "", 2.0f });
        }
        return p;
    }
    void setParameter(const juce::String& id, float value) override
    {
        const char* bandNames[] = { "LowShelf", "LowMid", "Mid", "HighMid", "HighShelf" };
        for (int i = 0; i < NumBands; ++i)
        {
            juce::String prefix = juce::String(bandNames[i]);
            if (id == prefix + "_freq") { setBandFrequency(i, value); return; }
            if (id == prefix + "_gain") { setBandGain(i, value); return; }
            if (id == prefix + "_q")    { setBandQ(i, value); return; }
        }
    }
    float getParameter(const juce::String& id) const override
    {
        const char* bandNames[] = { "LowShelf", "LowMid", "Mid", "HighMid", "HighShelf" };
        for (int i = 0; i < NumBands; ++i)
        {
            juce::String prefix = juce::String(bandNames[i]);
            if (id == prefix + "_freq") return bands[i].frequency;
            if (id == prefix + "_gain") return bands[i].gainDb;
            if (id == prefix + "_q")    return bands[i].q;
        }
        return 0.0f;
    }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    void updateFilters();
    void updateBand(int bandIndex);

    using FilterType = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<FilterType, Coefficients>;

    // 5-band processor chain
    juce::dsp::ProcessorChain<StereoFilter, StereoFilter, StereoFilter, StereoFilter, StereoFilter> processorChain;

    // Band parameters array
    BandParams bands[NumBands];

    // Default frequency values for each band
    static constexpr float defaultFrequencies[NumBands] = { 80.0f, 250.0f, 1000.0f, 4000.0f, 12000.0f };
};
