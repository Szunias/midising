#pragma once

#include "Effect.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * De-Esser effect using sidechain bandpass compression for sibilance reduction.
 *
 * Parameters:
 *   - frequency (2k-12k Hz) - center frequency for sibilance detection
 *   - bandwidth (0.5-3 octaves) - Q of the detection band
 *   - threshold (-40 to 0 dB) - level at which reduction kicks in
 *   - reduction (0-24 dB) - maximum amount of gain reduction
 *   - listen mode - audition the sidechain signal
 */
class DeEsser : public Effect
{
public:
    DeEsser();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    // Parameter setters
    void setFrequency(float freqHz);          // 2000 to 12000 Hz
    void setBandwidth(float octaves);         // 0.5 to 3.0 octaves
    void setThreshold(float thresholdDb);     // -40 to 0 dB
    void setReduction(float reductionDb);     // 0 to 24 dB
    void setListenMode(bool listen);

    // Parameter getters
    float getFrequency() const { return frequency; }
    float getBandwidth() const { return bandwidth; }
    float getThreshold() const { return thresholdDb; }
    float getReduction() const { return reductionDb; }
    bool getListenMode() const { return listenMode; }

    // Metering
    float getCurrentGainReduction() const { return currentGainReduction; }

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    // Parameters
    float frequency = 6000.0f;
    float bandwidth = 1.5f;
    float thresholdDb = -20.0f;
    float reductionDb = 6.0f;
    bool listenMode = false;

    // Sidechain bandpass filters (per channel)
    juce::dsp::IIR::Filter<float> bandpassL;
    juce::dsp::IIR::Filter<float> bandpassR;

    // Envelope follower state
    float envelopeL = 0.0f;
    float envelopeR = 0.0f;

    // Coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    // Metering
    float currentGainReduction = 0.0f;

    void updateFilters();
    void updateEnvelopeCoefficients();
};
