#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

/**
 * TimeStretch provides phase vocoder-based time stretching for audio.
 * Allows stretching audio from 0.25x to 4.0x speed without changing pitch.
 *
 * The algorithm uses STFT (Short-Time Fourier Transform) with phase
 * manipulation to achieve time-domain scaling while preserving frequency
 * content and minimizing phase artifacts.
 */
class TimeStretch
{
public:
    TimeStretch();
    ~TimeStretch() = default;

    /**
     * Prepare the time stretch processor with audio specifications.
     * Must be called before processing.
     * @param sampleRate The sample rate of the audio
     * @param maxBlockSize Maximum expected block size for processing
     */
    void prepare(double sampleRate, int maxBlockSize);

    /**
     * Set the time stretch ratio.
     * @param ratio Stretch ratio (0.25 = 4x slower, 1.0 = no change, 4.0 = 4x faster)
     *              Clamped to range [0.25, 4.0]
     */
    void setStretchRatio(float ratio);

    /**
     * Get the current stretch ratio.
     * @return Current stretch ratio
     */
    float getStretchRatio() const { return stretchRatio; }

    /**
     * Process a block of audio samples with time stretching.
     * Input and output may have different sizes based on stretch ratio.
     * @param inputBuffer Source audio buffer to stretch
     * @param outputBuffer Destination buffer for stretched audio (must be pre-sized)
     */
    void process(const juce::AudioBuffer<float>& inputBuffer,
                 juce::AudioBuffer<float>& outputBuffer);

    /**
     * Process audio in-place with accumulation to internal buffer.
     * Use getOutputSamples() to retrieve stretched audio.
     * @param inputBuffer Source audio to process
     */
    void pushSamples(const juce::AudioBuffer<float>& inputBuffer);

    /**
     * Get available output samples after processing.
     * @param outputBuffer Destination buffer (will be resized if needed)
     * @param numSamples Number of samples to retrieve
     * @return Actual number of samples retrieved
     */
    int pullSamples(juce::AudioBuffer<float>& outputBuffer, int numSamples);

    /**
     * Get the number of output samples currently available.
     * @return Number of available samples
     */
    int getAvailableOutputSamples() const { return outputFifoCount; }

    /**
     * Reset the processor state, clearing all internal buffers.
     */
    void reset();

    /**
     * Set transient preservation strength.
     * @param strength 0.0 = no preservation, 1.0 = maximum preservation
     */
    void setTransientPreservation(float strength);

    /**
     * Get the latency introduced by the time stretcher in samples.
     * @return Latency in samples
     */
    int getLatency() const { return fftSize; }

    // FFT configuration
    enum
    {
        fftOrder = 11,                      // FFT order (2^11 = 2048)
        fftSize = 1 << fftOrder,            // FFT size
        hopSizeDefault = fftSize / 4,       // Default hop size (512 samples, 75% overlap)
        maxChannels = 2                     // Maximum supported channels
    };

private:
    // FFT processing
    juce::dsp::FFT forwardFFT;
    juce::dsp::FFT inverseFFT;
    juce::dsp::WindowingFunction<float> analysisWindow;
    juce::dsp::WindowingFunction<float> synthesisWindow;

    // Stretch parameters
    float stretchRatio = 1.0f;
    float transientPreservation = 0.5f;
    double currentSampleRate = 44100.0;

    // Analysis/synthesis hop sizes
    int analysisHopSize = hopSizeDefault;
    int synthesisHopSize = hopSizeDefault;

    // Per-channel processing state
    struct ChannelState
    {
        // Input FIFO for analysis
        std::vector<float> inputFifo;
        int inputFifoCount = 0;

        // Output FIFO for synthesis
        std::vector<float> outputFifo;
        int outputFifoReadPos = 0;
        int outputFifoWritePos = 0;

        // FFT buffers
        std::vector<float> fftTimeDomain;
        std::vector<float> fftFreqDomain;

        // Phase tracking for phase vocoder
        std::vector<float> lastInputPhase;
        std::vector<float> lastOutputPhase;
        std::vector<float> phaseCumulative;

        // Magnitude and phase buffers
        std::vector<float> magnitude;
        std::vector<float> phase;

        // Overlap-add buffer
        std::vector<float> overlapBuffer;
        int overlapBufferPos = 0;

        // Transient detection
        float prevEnergy = 0.0f;
        bool transientDetected = false;
    };

    std::array<ChannelState, maxChannels> channelStates;
    int numChannels = 0;

    // Output FIFO for final stretched audio
    std::vector<float> outputBuffer;
    int outputFifoCount = 0;
    int outputFifoReadPos = 0;

    // Internal processing methods
    void processFrame(int channel);
    void analyzeFrame(int channel);
    void synthesizeFrame(int channel);
    bool detectTransient(int channel, const float* frame);
    void updateHopSizes();

    // Helper methods
    float wrapPhase(float phaseIn) const;
    float princArg(float phaseIn) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimeStretch)
};
