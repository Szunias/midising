#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

/**
 * PitchShift provides phase vocoder-based pitch shifting for audio.
 * Allows shifting pitch by +/- 24 semitones without changing tempo.
 *
 * The algorithm uses STFT (Short-Time Fourier Transform) with frequency
 * bin shifting and phase manipulation to achieve pitch scaling while
 * preserving timing and minimizing phase artifacts.
 */
class PitchShift
{
public:
    PitchShift();
    ~PitchShift() = default;

    /**
     * Prepare the pitch shift processor with audio specifications.
     * Must be called before processing.
     * @param sampleRate The sample rate of the audio
     * @param maxBlockSize Maximum expected block size for processing
     */
    void prepare(double sampleRate, int maxBlockSize);

    /**
     * Set the pitch shift amount in semitones.
     * @param semitones Pitch shift in semitones (-24.0 to +24.0)
     *                  Clamped to range [-24.0, 24.0]
     */
    void setPitchShiftSemitones(float semitones);

    /**
     * Set the pitch shift ratio directly.
     * @param ratio Pitch shift ratio (0.25 = 2 octaves down, 1.0 = no change, 4.0 = 2 octaves up)
     *              Clamped to range [0.25, 4.0]
     */
    void setPitchRatio(float ratio);

    /**
     * Get the current pitch shift in semitones.
     * @return Current pitch shift in semitones
     */
    float getPitchShiftSemitones() const { return pitchShiftSemitones; }

    /**
     * Get the current pitch ratio.
     * @return Current pitch ratio
     */
    float getPitchRatio() const { return pitchRatio; }

    /**
     * Process a block of audio samples with pitch shifting.
     * Input and output have the same size (tempo preserved).
     * @param inputBuffer Source audio buffer to pitch shift
     * @param outputBuffer Destination buffer for pitch shifted audio (must be pre-sized)
     */
    void process(const juce::AudioBuffer<float>& inputBuffer,
                 juce::AudioBuffer<float>& outputBuffer);

    /**
     * Process audio in-place with accumulation to internal buffer.
     * Use pullSamples() to retrieve pitch shifted audio.
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
     * Set formant preservation strength.
     * @param strength 0.0 = no preservation (chipmunk effect), 1.0 = full preservation
     */
    void setFormantPreservation(float strength);

    /**
     * Get formant preservation strength.
     * @return Current formant preservation strength
     */
    float getFormantPreservation() const { return formantPreservation; }

    /**
     * Get the latency introduced by the pitch shifter in samples.
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

    // Pitch shift parameters
    float pitchRatio = 1.0f;
    float pitchShiftSemitones = 0.0f;
    float formantPreservation = 0.0f;
    double currentSampleRate = 44100.0;

    // Hop size (same for analysis and synthesis to preserve tempo)
    int hopSize = hopSizeDefault;

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
        std::vector<float> fftShifted;

        // Phase tracking for phase vocoder
        std::vector<float> lastInputPhase;
        std::vector<float> lastOutputPhase;
        std::vector<float> phaseCumulative;

        // Magnitude and phase buffers
        std::vector<float> magnitude;
        std::vector<float> phase;
        std::vector<float> shiftedMagnitude;
        std::vector<float> shiftedPhase;

        // Formant envelope
        std::vector<float> spectralEnvelope;
        std::vector<float> shiftedEnvelope;

        // Overlap-add buffer
        std::vector<float> overlapBuffer;
        int overlapBufferPos = 0;
    };

    std::array<ChannelState, maxChannels> channelStates;
    int numChannels = 0;

    // Output FIFO for final pitch shifted audio
    std::vector<float> outputBuffer;
    int outputFifoCount = 0;
    int outputFifoReadPos = 0;

    // Internal processing methods
    void processFrame(int channel);
    void analyzeFrame(int channel);
    void shiftPitch(int channel);
    void synthesizeFrame(int channel);
    void extractSpectralEnvelope(int channel);
    void applyFormantCorrection(int channel);

    // Helper methods
    float wrapPhase(float phaseIn) const;
    float princArg(float phaseIn) const;
    float semitonesToRatio(float semitones) const;
    float ratioToSemitones(float ratio) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchShift)
};
