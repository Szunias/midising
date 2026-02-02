#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer()
    : forwardFFT(fftOrder),
      window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    std::fill(std::begin(fifo), std::end(fifo), 0.0f);
    std::fill(std::begin(fftData), std::end(fftData), 0.0f);
}

void SpectrumAnalyzer::pushNextSampleIntoFifo(float sample)
{
    // Simple FIFO implementation
    if (fifoIndex == fftSize)
    {
        if (!nextFFTBlockReady)
        {
            std::fill(std::begin(fftData), std::end(fftData), 0.0f);
            std::copy(std::begin(fifo), std::end(fifo), std::begin(fftData));
            nextFFTBlockReady = true;
        }

        fifoIndex = 0;
    }

    fifo[fifoIndex++] = sample;
}

bool SpectrumAnalyzer::getNextFFTBlock(std::vector<float>& destData)
{
    if (nextFFTBlockReady)
    {
        window.multiplyWithWindowingTable(fftData, fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform(fftData);

        // Map linear scope to destination
        // We only use the first half of the FFT data (positive frequencies)
        if (destData.size() != scopeSize)
            destData.resize(scopeSize);
            
        // Copy relevant part to destination
        // FFT bins are linear, we might want to just copy raw bins for now
        // and handle mapping in the display
        
        // Just copy the raw magnitudes for now, up to scopeSize
        // fftSize is 2048, scopeSize is 512. 
        // We'll simplisticly map or just copy the low end where most music energy is?
        // Actually, let's just assert destData is large enough or copy what fits.
        
        // Ideally we want log scale, but let's do simple linear copy first
        for (int i = 0; i < scopeSize && i < fftSize / 2; ++i)
        {
            destData[i] = fftData[i];
        }

        nextFFTBlockReady = false;
        return true;
    }

    return false;
}
