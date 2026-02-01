#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

/**
 * SpectrumAnalyzer handles the FFT processing of audio data.
 * It provides a thread-safe way to push samples from the audio thread
 * and retrieve frequency data for visualization on the UI thread.
 */
class SpectrumAnalyzer
{
public:
    SpectrumAnalyzer();
    ~SpectrumAnalyzer() = default;

    // Called from audio thread
    void pushNextSampleIntoFifo(float sample);

    // Called from UI thread to check if new data is available
    // Returns true if nextFFTBlock is filled with new data
    bool getNextFFTBlock(std::vector<float>& destData);

    enum
    {
        fftOrder = 11,
        fftSize  = 1 << fftOrder,
        scopeSize = 512
    };

private:
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    float fifo[fftSize];
    float fftData[2 * fftSize];
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
