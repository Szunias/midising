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
