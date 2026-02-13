#pragma once

#include "Effect.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * 3-band multiband compressor using Linkwitz-Riley crossover filters.
 *
 * Parameters:
 *   - lowMidCrossover (100-500 Hz)
 *   - midHighCrossover (1k-8k Hz)
 *   - Per-band: threshold, ratio, attack, release, makeupGain, solo, bypass
 */
class MultibandCompressor : public Effect
{
public:
    MultibandCompressor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    // Crossover frequencies
    void setLowMidCrossover(float freqHz);
    void setMidHighCrossover(float freqHz);
    float getLowMidCrossover() const { return lowMidFreq; }
    float getMidHighCrossover() const { return midHighFreq; }

    // Per-band parameters (band: 0=low, 1=mid, 2=high)
    void setBandThreshold(int band, float dB);
    void setBandRatio(int band, float ratio);
    void setBandAttack(int band, float ms);
    void setBandRelease(int band, float ms);
    void setBandMakeupGain(int band, float dB);
    void setBandSolo(int band, bool solo);
    void setBandBypass(int band, bool bypass);

    float getBandThreshold(int band) const;
    float getBandRatio(int band) const;
    float getBandAttack(int band) const;
    float getBandRelease(int band) const;
    float getBandMakeupGain(int band) const;
    bool getBandSolo(int band) const;
    bool getBandBypass(int band) const;

    // Metering
    float getBandGainReduction(int band) const;

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    static constexpr int NUM_BANDS = 3;

    struct BandParams
    {
        float threshold = -20.0f;
        float ratio = 4.0f;
        float attackMs = 10.0f;
        float releaseMs = 100.0f;
        float makeupGainDb = 0.0f;
        bool solo = false;
        bool bandBypass = false;

        // State
        float envelopeLevel = 0.0f;
        float gainReduction = 0.0f;
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        float makeupGainLinear = 1.0f;
    };

    float lowMidFreq = 250.0f;
    float midHighFreq = 3000.0f;

    BandParams bands[NUM_BANDS];

    // Crossover filters (Linkwitz-Riley = two cascaded Butterworth)
    // Low/Mid split
    juce::dsp::IIR::Filter<float> lowPassL1, lowPassL2;   // left channel
    juce::dsp::IIR::Filter<float> lowPassR1, lowPassR2;   // right channel
    juce::dsp::IIR::Filter<float> highPassL1, highPassL2;
    juce::dsp::IIR::Filter<float> highPassR1, highPassR2;

    // Mid/High split
    juce::dsp::IIR::Filter<float> midLowPassL1, midLowPassL2;
    juce::dsp::IIR::Filter<float> midLowPassR1, midLowPassR2;
    juce::dsp::IIR::Filter<float> midHighPassL1, midHighPassL2;
    juce::dsp::IIR::Filter<float> midHighPassR1, midHighPassR2;

    int currentBlockSize = 512;

    void updateCrossoverFilters();
    void updateBandCoefficients(int band);
    float computeCompression(BandParams& bp, float inputLevel);
};
