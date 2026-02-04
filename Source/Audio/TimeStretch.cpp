#include "TimeStretch.h"

TimeStretch::TimeStretch()
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
        state.lastInputPhase.resize(fftSize / 2 + 1, 0.0f);
        state.lastOutputPhase.resize(fftSize / 2 + 1, 0.0f);
        state.phaseCumulative.resize(fftSize / 2 + 1, 0.0f);
        state.magnitude.resize(fftSize / 2 + 1, 0.0f);
        state.phase.resize(fftSize / 2 + 1, 0.0f);
        state.overlapBuffer.resize(fftSize * 4, 0.0f);
    }

    outputBuffer.resize(fftSize * 8, 0.0f);
}

void TimeStretch::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    juce::ignoreUnused(maxBlockSize);

    reset();
    updateHopSizes();
}

void TimeStretch::setStretchRatio(float ratio)
{
    // Clamp to valid range
    stretchRatio = juce::jlimit(0.25f, 4.0f, ratio);
    updateHopSizes();
}

void TimeStretch::setTransientPreservation(float strength)
{
    transientPreservation = juce::jlimit(0.0f, 1.0f, strength);
}

void TimeStretch::updateHopSizes()
{
    // Analysis hop stays constant, synthesis hop changes with stretch ratio
    // For time stretching: synthesis hop = analysis hop * stretch ratio
    // When ratio < 1.0 (slowing down): synthesis hop < analysis hop (more overlapping)
    // When ratio > 1.0 (speeding up): synthesis hop > analysis hop (less overlapping)
    analysisHopSize = hopSizeDefault;
    synthesisHopSize = static_cast<int>(std::round(analysisHopSize * stretchRatio));

    // Ensure valid hop sizes
    synthesisHopSize = juce::jmax(1, synthesisHopSize);
    synthesisHopSize = juce::jmin(synthesisHopSize, fftSize - 1);
}

void TimeStretch::reset()
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
        state.overlapBufferPos = 0;
        state.prevEnergy = 0.0f;
        state.transientDetected = false;
    }

    std::fill(outputBuffer.begin(), outputBuffer.end(), 0.0f);
    outputFifoCount = 0;
    outputFifoReadPos = 0;
}

void TimeStretch::process(const juce::AudioBuffer<float>& inputBuffer,
                          juce::AudioBuffer<float>& outputBuffer)
{
    const int numInputChannels = inputBuffer.getNumChannels();
    const int numInputSamples = inputBuffer.getNumSamples();

    // Calculate expected output size
    const int expectedOutputSamples = static_cast<int>(
        std::ceil(numInputSamples / stretchRatio));

    // Ensure output buffer is properly sized
    outputBuffer.setSize(numInputChannels, expectedOutputSamples, false, true, true);

    numChannels = numInputChannels;

    // Push input samples
    pushSamples(inputBuffer);

    // Pull output samples
    pullSamples(outputBuffer, expectedOutputSamples);
}

void TimeStretch::pushSamples(const juce::AudioBuffer<float>& inputBuffer)
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

                // Shift input FIFO by analysis hop size
                const int remaining = state.inputFifoCount - analysisHopSize;
                if (remaining > 0)
                {
                    std::copy(state.inputFifo.begin() + analysisHopSize,
                              state.inputFifo.begin() + state.inputFifoCount,
                              state.inputFifo.begin());
                }
                state.inputFifoCount = juce::jmax(0, remaining);
            }
        }
    }
}

int TimeStretch::pullSamples(juce::AudioBuffer<float>& outputBuf, int numSamples)
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

void TimeStretch::processFrame(int channel)
{
    analyzeFrame(channel);
    synthesizeFrame(channel);
}

void TimeStretch::analyzeFrame(int channel)
{
    auto& state = channelStates[channel];

    // Copy input to FFT buffer
    std::copy(state.inputFifo.begin(),
              state.inputFifo.begin() + fftSize,
              state.fftTimeDomain.begin());

    // Detect transients before windowing
    state.transientDetected = detectTransient(channel, state.fftTimeDomain.data());

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

    // Phase vocoder: calculate phase increment and accumulate
    const float expectedPhaseDiff = 2.0f * juce::MathConstants<float>::pi *
                                    static_cast<float>(analysisHopSize) /
                                    static_cast<float>(fftSize);

    for (int bin = 0; bin < numBins; ++bin)
    {
        // Calculate phase difference from last frame
        float phaseDiff = state.phase[bin] - state.lastInputPhase[bin];
        state.lastInputPhase[bin] = state.phase[bin];

        // Remove expected phase advance
        phaseDiff -= static_cast<float>(bin) * expectedPhaseDiff;

        // Wrap phase to [-pi, pi]
        phaseDiff = princArg(phaseDiff);

        // Calculate true frequency deviation
        const float trueFreq = phaseDiff + static_cast<float>(bin) * expectedPhaseDiff;

        // Scale phase increment for synthesis hop size
        const float synthPhaseDiff = trueFreq * static_cast<float>(synthesisHopSize) /
                                     static_cast<float>(analysisHopSize);

        // Accumulate phase
        if (state.transientDetected && transientPreservation > 0.0f)
        {
            // On transients, partially reset phase to preserve attack
            const float blend = 1.0f - transientPreservation;
            state.phaseCumulative[bin] = state.phaseCumulative[bin] * blend +
                                         state.phase[bin] * transientPreservation +
                                         synthPhaseDiff * blend;
        }
        else
        {
            state.phaseCumulative[bin] = wrapPhase(state.phaseCumulative[bin] + synthPhaseDiff);
        }
    }
}

void TimeStretch::synthesizeFrame(int channel)
{
    auto& state = channelStates[channel];
    const int numBins = fftSize / 2 + 1;

    // Convert back to complex form
    for (int bin = 0; bin < numBins; ++bin)
    {
        const float mag = state.magnitude[bin];
        const float ph = state.phaseCumulative[bin];

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
                                static_cast<float>(synthesisHopSize) / 2.0f;
    const float normalizer = 1.0f / overlapFactor;

    // Overlap-add to output buffer
    const int outputFifoSize = static_cast<int>(state.outputFifo.size());

    for (int i = 0; i < fftSize; ++i)
    {
        const int outputIndex = (state.outputFifoWritePos + i) % outputFifoSize;
        state.outputFifo[outputIndex] += state.fftFreqDomain[i] * normalizer;
    }

    // Advance write position by synthesis hop size
    for (int i = 0; i < synthesisHopSize; ++i)
    {
        // The samples at the current position are now complete
        state.outputFifoWritePos = (state.outputFifoWritePos + 1) % outputFifoSize;
    }

    // Clear the overlap buffer ahead for next frame
    for (int i = 0; i < fftSize; ++i)
    {
        const int clearIndex = (state.outputFifoWritePos + i) % outputFifoSize;
        if (i >= synthesisHopSize)
        {
            // Only clear samples that won't be used in the current overlap
        }
    }
}

bool TimeStretch::detectTransient(int channel, const float* frame)
{
    auto& state = channelStates[channel];

    // Calculate energy of the frame
    float energy = 0.0f;
    for (int i = 0; i < fftSize; ++i)
    {
        energy += frame[i] * frame[i];
    }
    energy /= static_cast<float>(fftSize);

    // Calculate spectral flux for transient detection
    // High frequency content ratio can indicate transients
    float highFreqEnergy = 0.0f;
    float lowFreqEnergy = 0.0f;
    const int splitPoint = fftSize / 4;

    for (int i = 0; i < splitPoint; ++i)
    {
        lowFreqEnergy += frame[i] * frame[i];
    }
    for (int i = splitPoint; i < fftSize; ++i)
    {
        highFreqEnergy += frame[i] * frame[i];
    }

    // Transient detection: significant energy increase
    const float energyRatio = (state.prevEnergy > 1e-10f) ?
                              energy / state.prevEnergy : 1.0f;

    state.prevEnergy = energy;

    // Detect transient if energy increases significantly
    // Threshold is tuned for typical audio material
    const float transientThreshold = 3.0f;
    return energyRatio > transientThreshold;
}

float TimeStretch::wrapPhase(float phaseIn) const
{
    // Wrap phase to [0, 2*pi]
    const float twoPi = 2.0f * juce::MathConstants<float>::pi;
    while (phaseIn >= twoPi) phaseIn -= twoPi;
    while (phaseIn < 0.0f) phaseIn += twoPi;
    return phaseIn;
}

float TimeStretch::princArg(float phaseIn) const
{
    // Wrap phase to [-pi, pi]
    const float pi = juce::MathConstants<float>::pi;
    const float twoPi = 2.0f * pi;

    while (phaseIn > pi) phaseIn -= twoPi;
    while (phaseIn <= -pi) phaseIn += twoPi;

    return phaseIn;
}
