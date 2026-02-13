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

    std::vector<EffectParameter> getParameters() const override
    {
        std::vector<EffectParameter> p;
        p.push_back({ "lowMidFreq",  "Low/Mid Xover",  100.0f, 500.0f,  250.0f,  lowMidFreq, 1.0f, "Hz", 2.0f });
        p.push_back({ "midHighFreq", "Mid/High Xover", 1000.0f, 8000.0f, 3000.0f, midHighFreq, 1.0f, "Hz", 2.0f });
        const char* bandNames[] = { "Low", "Mid", "High" };
        for (int i = 0; i < NUM_BANDS; ++i)
        {
            juce::String bn(bandNames[i]);
            p.push_back({ bn + "_thresh",  bn + " Thresh",  -60.0f, 0.0f,   -20.0f, bands[i].threshold,    0.1f, "dB", 1.0f });
            p.push_back({ bn + "_ratio",   bn + " Ratio",   1.0f,   20.0f,  4.0f,   bands[i].ratio,        0.1f, ":1", 2.0f });
            p.push_back({ bn + "_attack",  bn + " Attack",  0.1f,   100.0f, 10.0f,  bands[i].attackMs,     0.1f, "ms", 2.0f });
            p.push_back({ bn + "_release", bn + " Release", 10.0f,  1000.0f,100.0f, bands[i].releaseMs,    1.0f, "ms", 2.0f });
            p.push_back({ bn + "_makeup",  bn + " Makeup",  0.0f,   24.0f,  0.0f,   bands[i].makeupGainDb, 0.1f, "dB", 1.0f });
        }
        return p;
    }
    void setParameter(const juce::String& id, float value) override
    {
        if (id == "lowMidFreq")  { setLowMidCrossover(value); return; }
        if (id == "midHighFreq") { setMidHighCrossover(value); return; }
        const char* bandNames[] = { "Low", "Mid", "High" };
        for (int i = 0; i < NUM_BANDS; ++i)
        {
            juce::String bn(bandNames[i]);
            if (id == bn + "_thresh")  { setBandThreshold(i, value); return; }
            if (id == bn + "_ratio")   { setBandRatio(i, value); return; }
            if (id == bn + "_attack")  { setBandAttack(i, value); return; }
            if (id == bn + "_release") { setBandRelease(i, value); return; }
            if (id == bn + "_makeup")  { setBandMakeupGain(i, value); return; }
        }
    }
    float getParameter(const juce::String& id) const override
    {
        if (id == "lowMidFreq")  return lowMidFreq;
        if (id == "midHighFreq") return midHighFreq;
        const char* bandNames[] = { "Low", "Mid", "High" };
        for (int i = 0; i < NUM_BANDS; ++i)
        {
            juce::String bn(bandNames[i]);
            if (id == bn + "_thresh")  return bands[i].threshold;
            if (id == bn + "_ratio")   return bands[i].ratio;
            if (id == bn + "_attack")  return bands[i].attackMs;
            if (id == bn + "_release") return bands[i].releaseMs;
            if (id == bn + "_makeup")  return bands[i].makeupGainDb;
        }
        return 0.0f;
    }

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
