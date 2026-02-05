#include "PitchShift.h"

PitchShift::PitchShift()
    : forwardFFT(fftOrder),
      inverseFFT(fftOrder),
      analysisWindow(fftSize, juce::dsp::WindowingFunction<float>::hann),
      synthesisWindow(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    // Initialize channel states
    for (auto& state : channelStates)
    {
        state.inputFifo.resize(fftSize * 2, 0.0f);
        state.outputFifo.resize(fftSize * 8, 0.0f);
        state.fftTimeDomain.resize(fftSize * 2, 0.0f);
        state.fftFreqDomain.resize(fftSize * 2, 0.0f);
        state.fftShifted.resize(fftSize * 2, 0.0f);
        state.lastInputPhase.resize(fftSize / 2 + 1, 0.0f);
        state.lastOutputPhase.resize(fftSize / 2 + 1, 0.0f);
        state.phaseCumulative.resize(fftSize / 2 + 1, 0.0f);
        state.magnitude.resize(fftSize / 2 + 1, 0.0f);
        state.phase.resize(fftSize / 2 + 1, 0.0f);
        state.shiftedMagnitude.resize(fftSize / 2 + 1, 0.0f);
        state.shiftedPhase.resize(fftSize / 2 + 1, 0.0f);
        state.spectralEnvelope.resize(fftSize / 2 + 1, 0.0f);
        state.shiftedEnvelope.resize(fftSize / 2 + 1, 0.0f);
        state.overlapBuffer.resize(fftSize * 4, 0.0f);
    }

    outputBuffer.resize(fftSize * 8, 0.0f);
}

void PitchShift::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    juce::ignoreUnused(maxBlockSize);

    reset();
}

void PitchShift::setPitchShiftSemitones(float semitones)
{
    // Clamp to valid range (+/- 2 octaves)
    pitchShiftSemitones = juce::jlimit(-24.0f, 24.0f, semitones);
    pitchRatio = semitonesToRatio(pitchShiftSemitones);
}

void PitchShift::setPitchRatio(float ratio)
{
    // Clamp to valid range
    pitchRatio = juce::jlimit(0.25f, 4.0f, ratio);
    pitchShiftSemitones = ratioToSemitones(pitchRatio);
}

void PitchShift::setFormantPreservation(float strength)
{
    formantPreservation = juce::jlimit(0.0f, 1.0f, strength);
}

float PitchShift::semitonesToRatio(float semitones) const
{
    // Convert semitones to frequency ratio
    // ratio = 2^(semitones/12)
    return std::pow(2.0f, semitones / 12.0f);
}

float PitchShift::ratioToSemitones(float ratio) const
{
    // Convert frequency ratio to semitones
    // semitones = 12 * log2(ratio)
    if (ratio <= 0.0f)
        return 0.0f;
    return 12.0f * std::log2(ratio);
}

void PitchShift::reset()
{
    for (auto& state : channelStates)
    {
        std::fill(state.inputFifo.begin(), state.inputFifo.end(), 0.0f);
        state.inputFifoCount = 0;

        std::fill(state.outputFifo.begin(), state.outputFifo.end(), 0.0f);
        state.outputFifoReadPos = 0;
        state.outputFifoWritePos = 0;

        std::fill(state.lastInputPhase.begin(), state.lastInputPhase.end(), 0.0f);
        std::fill(state.lastOutputPhase.begin(), state.lastOutputPhase.end(), 0.0f);
        std::fill(state.phaseCumulative.begin(), state.phaseCumulative.end(), 0.0f);
        std::fill(state.overlapBuffer.begin(), state.overlapBuffer.end(), 0.0f);
        std::fill(state.spectralEnvelope.begin(), state.spectralEnvelope.end(), 0.0f);
        state.overlapBufferPos = 0;
    }

    std::fill(outputBuffer.begin(), outputBuffer.end(), 0.0f);
    outputFifoCount = 0;
    outputFifoReadPos = 0;
}

void PitchShift::process(const juce::AudioBuffer<float>& inputBuffer,
                          juce::AudioBuffer<float>& outputBuf)
{
    const int numInputChannels = inputBuffer.getNumChannels();
    const int numInputSamples = inputBuffer.getNumSamples();

    // Pitch shifting preserves length - output size equals input size
    outputBuf.setSize(numInputChannels, numInputSamples, false, true, true);

    numChannels = numInputChannels;

    // Push input samples
    pushSamples(inputBuffer);

    // Pull output samples
    pullSamples(outputBuf, numInputSamples);
}

void PitchShift::pushSamples(const juce::AudioBuffer<float>& inputBuffer)
{
    const int numInputChannels = inputBuffer.getNumChannels();
    const int numInputSamples = inputBuffer.getNumSamples();

    numChannels = juce::jmin(numInputChannels, static_cast<int>(maxChannels));

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto& state = channelStates[channel];
        const float* inputData = inputBuffer.getReadPointer(channel);

        // Push samples to input FIFO
        for (int i = 0; i < numInputSamples; ++i)
        {
            state.inputFifo[state.inputFifoCount++] = inputData[i];

            // When we have enough samples for a frame, process it
            if (state.inputFifoCount >= fftSize)
            {
                processFrame(channel);

                // Shift input FIFO by hop size
                const int remaining = state.inputFifoCount - hopSize;
                if (remaining > 0)
                {
                    std::copy(state.inputFifo.begin() + hopSize,
                              state.inputFifo.begin() + state.inputFifoCount,
                              state.inputFifo.begin());
                }
                state.inputFifoCount = juce::jmax(0, remaining);
            }
        }
    }
}

int PitchShift::pullSamples(juce::AudioBuffer<float>& outputBuf, int numSamples)
{
    if (numChannels == 0)
        return 0;

    // Ensure output buffer is properly sized
    if (outputBuf.getNumChannels() < numChannels ||
        outputBuf.getNumSamples() < numSamples)
    {
        outputBuf.setSize(numChannels, numSamples, false, true, true);
    }

    int samplesRetrieved = 0;

    // Find the minimum available samples across all channels
    int minAvailable = INT_MAX;
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto& state = channelStates[channel];
        int available = (state.outputFifoWritePos - state.outputFifoReadPos +
                         static_cast<int>(state.outputFifo.size())) %
                         static_cast<int>(state.outputFifo.size());
        minAvailable = juce::jmin(minAvailable, available);
    }

    samplesRetrieved = juce::jmin(numSamples, minAvailable);

    // Pull samples from each channel's output FIFO
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto& state = channelStates[channel];
        float* outputData = outputBuf.getWritePointer(channel);

        for (int i = 0; i < samplesRetrieved; ++i)
        {
            outputData[i] = state.outputFifo[state.outputFifoReadPos];
            state.outputFifoReadPos = (state.outputFifoReadPos + 1) %
                                      static_cast<int>(state.outputFifo.size());
        }

        // Fill remaining with zeros if needed
        for (int i = samplesRetrieved; i < numSamples; ++i)
        {
            outputData[i] = 0.0f;
        }
    }

    return samplesRetrieved;
}

void PitchShift::processFrame(int channel)
{
    analyzeFrame(channel);
    shiftPitch(channel);
    synthesizeFrame(channel);
}

void PitchShift::analyzeFrame(int channel)
{
    auto& state = channelStates[channel];

    // Copy input to FFT buffer
    std::copy(state.inputFifo.begin(),
              state.inputFifo.begin() + fftSize,
              state.fftTimeDomain.begin());

    // Apply analysis window
    analysisWindow.multiplyWithWindowingTable(state.fftTimeDomain.data(), fftSize);

    // Clear imaginary part
    std::fill(state.fftTimeDomain.begin() + fftSize,
              state.fftTimeDomain.end(), 0.0f);

    // Perform forward FFT
    forwardFFT.performRealOnlyForwardTransform(state.fftTimeDomain.data());

    // Extract magnitude and phase
    const int numBins = fftSize / 2 + 1;
    for (int bin = 0; bin < numBins; ++bin)
    {
        const float real = state.fftTimeDomain[bin * 2];
        const float imag = state.fftTimeDomain[bin * 2 + 1];

        state.magnitude[bin] = std::sqrt(real * real + imag * imag);
        state.phase[bin] = std::atan2(imag, real);
    }

    // Extract spectral envelope for formant preservation
    if (formantPreservation > 0.0f)
    {
        extractSpectralEnvelope(channel);
    }
}

void PitchShift::extractSpectralEnvelope(int channel)
{
    auto& state = channelStates[channel];
    const int numBins = fftSize / 2 + 1;

    // Simple cepstral-based envelope extraction
    // Use smoothing window to estimate spectral envelope

    // Copy magnitude to envelope
    std::copy(state.magnitude.begin(), state.magnitude.end(),
              state.spectralEnvelope.begin());

    // Apply smoothing filter to get envelope
    const int smoothingWindow = 32; // Number of bins to smooth over

    for (int bin = 0; bin < numBins; ++bin)
    {
        float sum = 0.0f;
        int count = 0;

        for (int i = -smoothingWindow; i <= smoothingWindow; ++i)
        {
            const int idx = bin + i;
            if (idx >= 0 && idx < numBins)
            {
                // Use log magnitude for smoother envelope
                const float logMag = state.magnitude[idx] > 1e-10f ?
                    std::log(state.magnitude[idx]) : -23.0f; // ~= log(1e-10)
                sum += logMag;
                count++;
            }
        }

        // Store smoothed envelope (convert back from log)
        state.spectralEnvelope[bin] = count > 0 ?
            std::exp(sum / static_cast<float>(count)) : 0.0f;
    }
}

void PitchShift::shiftPitch(int channel)
{
    auto& state = channelStates[channel];
    const int numBins = fftSize / 2 + 1;

    // Clear shifted buffers
    std::fill(state.shiftedMagnitude.begin(), state.shiftedMagnitude.end(), 0.0f);
    std::fill(state.shiftedPhase.begin(), state.shiftedPhase.end(), 0.0f);
    std::fill(state.shiftedEnvelope.begin(), state.shiftedEnvelope.end(), 0.0f);

    // Calculate expected phase advance per bin
    const float expectedPhaseDiff = 2.0f * juce::MathConstants<float>::pi *
                                    static_cast<float>(hopSize) /
                                    static_cast<float>(fftSize);

    // Shift bins according to pitch ratio
    for (int srcBin = 0; srcBin < numBins; ++srcBin)
    {
        // Calculate destination bin
        const float destBinFloat = static_cast<float>(srcBin) * pitchRatio;
        const int destBin = static_cast<int>(std::round(destBinFloat));

        if (destBin < 0 || destBin >= numBins)
            continue;

        // Calculate phase difference from last frame
        float phaseDiff = state.phase[srcBin] - state.lastInputPhase[srcBin];

        // Remove expected phase advance for this bin
        phaseDiff -= static_cast<float>(srcBin) * expectedPhaseDiff;

        // Wrap to [-pi, pi]
        phaseDiff = princArg(phaseDiff);

        // Calculate true frequency deviation
        const float trueFreq = phaseDiff + static_cast<float>(srcBin) * expectedPhaseDiff;

        // Scale frequency for the destination bin
        const float scaledFreq = trueFreq * pitchRatio;

        // Calculate phase increment
        const float phaseIncrement = scaledFreq;

        // Accumulate to destination bin (handle overlapping bins)
        state.shiftedMagnitude[destBin] += state.magnitude[srcBin];

        // Accumulate phase
        state.phaseCumulative[destBin] = wrapPhase(
            state.lastOutputPhase[destBin] + phaseIncrement);
        state.shiftedPhase[destBin] = state.phaseCumulative[destBin];

        // Shift envelope for formant preservation
        if (formantPreservation > 0.0f)
        {
            state.shiftedEnvelope[destBin] += state.spectralEnvelope[srcBin];
        }
    }

    // Update phase tracking
    for (int bin = 0; bin < numBins; ++bin)
    {
        state.lastInputPhase[bin] = state.phase[bin];
        state.lastOutputPhase[bin] = state.shiftedPhase[bin];
    }

    // Apply formant correction if enabled
    if (formantPreservation > 0.0f)
    {
        applyFormantCorrection(channel);
    }
}

void PitchShift::applyFormantCorrection(int channel)
{
    auto& state = channelStates[channel];
    const int numBins = fftSize / 2 + 1;

    // Apply formant preservation by restoring original spectral envelope
    for (int bin = 0; bin < numBins; ++bin)
    {
        if (state.shiftedEnvelope[bin] > 1e-10f && state.spectralEnvelope[bin] > 1e-10f)
        {
            // Calculate the correction factor
            const float correction = state.spectralEnvelope[bin] / state.shiftedEnvelope[bin];

            // Apply correction with formant preservation strength
            const float blendedCorrection = 1.0f + (correction - 1.0f) * formantPreservation;

            state.shiftedMagnitude[bin] *= blendedCorrection;
        }
    }
}

void PitchShift::synthesizeFrame(int channel)
{
    auto& state = channelStates[channel];
    const int numBins = fftSize / 2 + 1;

    // Convert shifted magnitude and phase back to complex form
    for (int bin = 0; bin < numBins; ++bin)
    {
        const float mag = state.shiftedMagnitude[bin];
        const float ph = state.shiftedPhase[bin];

        state.fftFreqDomain[bin * 2] = mag * std::cos(ph);
        state.fftFreqDomain[bin * 2 + 1] = mag * std::sin(ph);
    }

    // Perform inverse FFT
    inverseFFT.performRealOnlyInverseTransform(state.fftFreqDomain.data());

    // Apply synthesis window
    synthesisWindow.multiplyWithWindowingTable(state.fftFreqDomain.data(), fftSize);

    // Calculate overlap-add normalization factor
    // For Hann window with 75% overlap, the factor is approximately 1.5
    const float overlapFactor = static_cast<float>(fftSize) /
                                static_cast<float>(hopSize) / 2.0f;
    const float normalizer = 1.0f / overlapFactor;

    // Overlap-add to output buffer
    const int outputFifoSize = static_cast<int>(state.outputFifo.size());

    for (int i = 0; i < fftSize; ++i)
    {
        const int outputIndex = (state.outputFifoWritePos + i) % outputFifoSize;
        state.outputFifo[outputIndex] += state.fftFreqDomain[i] * normalizer;
    }

    // Advance write position by hop size
    for (int i = 0; i < hopSize; ++i)
    {
        // The samples at the current position are now complete
        state.outputFifoWritePos = (state.outputFifoWritePos + 1) % outputFifoSize;
    }
}

float PitchShift::wrapPhase(float phaseIn) const
{
    // Wrap phase to [0, 2*pi]
    const float twoPi = 2.0f * juce::MathConstants<float>::pi;
    while (phaseIn >= twoPi) phaseIn -= twoPi;
    while (phaseIn < 0.0f) phaseIn += twoPi;
    return phaseIn;
}

float PitchShift::princArg(float phaseIn) const
{
    // Wrap phase to [-pi, pi]
    const float pi = juce::MathConstants<float>::pi;
    const float twoPi = 2.0f * pi;

    while (phaseIn > pi) phaseIn -= twoPi;
    while (phaseIn <= -pi) phaseIn += twoPi;

    return phaseIn;
}
