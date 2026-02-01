#include "Metronome.h"
#include <cmath>

Metronome::Metronome()
{
}

void Metronome::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxSamplesPerBlock = samplesPerBlock;

    // Pre-allocate click sound buffers
    generateClickSounds();
}

void Metronome::generateClickSounds()
{
    // Click parameters
    const double clickFrequency = 1000.0; // 1kHz
    const double clickDuration = 0.01; // 10ms
    const int clickLengthSamples = static_cast<int>(clickDuration * currentSampleRate);

    // Allocate buffers (mono clicks)
    normalClickBuffer.setSize(1, clickLengthSamples);
    accentClickBuffer.setSize(1, clickLengthSamples);

    // Generate normal click (sine wave with envelope)
    auto* normalData = normalClickBuffer.getWritePointer(0);
    for (int i = 0; i < clickLengthSamples; ++i)
    {
        double phase = (i / currentSampleRate) * clickFrequency * 2.0 * juce::MathConstants<double>::pi;
        double envelope = 1.0 - (static_cast<double>(i) / clickLengthSamples); // Linear decay
        normalData[i] = static_cast<float>(std::sin(phase) * envelope);
    }

    // Generate accent click (1.5x louder)
    auto* accentData = accentClickBuffer.getWritePointer(0);
    for (int i = 0; i < clickLengthSamples; ++i)
    {
        double phase = (i / currentSampleRate) * clickFrequency * 2.0 * juce::MathConstants<double>::pi;
        double envelope = 1.0 - (static_cast<double>(i) / clickLengthSamples); // Linear decay
        accentData[i] = static_cast<float>(std::sin(phase) * envelope * 1.5);
    }
}

void Metronome::processBlock(juce::AudioBuffer<float>& outputBuffer,
                             int64_t playheadPosition,
                             double bpm,
                             bool isPlaying)
{
    // Skip metronome output during export mode
    if (exportMode.load())
    {
        clickPlaybackPosition = -1;
        return;
    }

    const bool countingIn = isCountingIn.load();

    // Metronome should play during count-in OR when enabled and playing
    const bool shouldPlayClicks = (countingIn && isPlaying) || (enabled.load() && isPlaying);

    // Early exit if we shouldn't play clicks
    if (!shouldPlayClicks)
    {
        clickPlaybackPosition = -1;
        return;
    }

    const int numSamples = outputBuffer.getNumSamples();
    const int numChannels = outputBuffer.getNumChannels();
    const double vol = volume.load();

    // Check if we should trigger a new click at the start of this block
    int beatNumber = 0;
    if (shouldTriggerClick(playheadPosition, bpm, beatNumber))
    {
        // Start playing a new click
        clickPlaybackPosition = 0;
        isAccentClick = (beatNumber == 1); // First beat of measure is accent
        lastBeatSample = playheadPosition;
    }

    // If we're playing a click, mix it into the output
    if (clickPlaybackPosition >= 0)
    {
        const juce::AudioBuffer<float>& clickBuffer = isAccentClick ? accentClickBuffer : normalClickBuffer;
        const int clickLength = clickBuffer.getNumSamples();
        const auto* clickData = clickBuffer.getReadPointer(0);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (clickPlaybackPosition >= clickLength)
            {
                // Click finished
                clickPlaybackPosition = -1;
                break;
            }

            // Mix click into all output channels
            float clickSample = clickData[clickPlaybackPosition] * static_cast<float>(vol);
            for (int channel = 0; channel < numChannels; ++channel)
            {
                outputBuffer.addSample(channel, sample, clickSample);
            }

            ++clickPlaybackPosition;
        }
    }
}

bool Metronome::shouldTriggerClick(int64_t playheadPosition, double bpm, int& beatNumber)
{
    // Calculate samples per beat
    const double beatsPerSecond = bpm / 60.0;
    const double samplesPerBeat = currentSampleRate / beatsPerSecond;

    // Calculate which beat we're on
    const int64_t currentBeat = static_cast<int64_t>(playheadPosition / samplesPerBeat);
    const int64_t beatStartSample = static_cast<int64_t>(currentBeat * samplesPerBeat);

    // Avoid retriggering the same beat
    if (beatStartSample == lastBeatSample)
    {
        return false;
    }

    // Check if we're at or past a beat boundary (within a small tolerance)
    const int64_t tolerance = 128; // Samples tolerance for beat detection
    if (playheadPosition >= beatStartSample &&
        playheadPosition < beatStartSample + tolerance)
    {
        // Calculate beat number within measure (1-4 for 4/4 time)
        beatNumber = static_cast<int>((currentBeat % beatsPerMeasure) + 1);
        return true;
    }

    return false;
}

void Metronome::setCountInBars(int bars)
{
    // Clamp to valid range: 0, 1, or 2
    if (bars < 0) bars = 0;
    if (bars > 2) bars = 2;
    countInBars.store(bars);
}

void Metronome::startCountIn(int64_t startPosition)
{
    if (countInBars.load() > 0)
    {
        isCountingIn.store(true);
        countInStartPosition.store(startPosition);
    }
}

bool Metronome::isCountInComplete(int64_t currentPosition, double bpm) const
{
    if (!isCountingIn.load())
        return true;

    const int64_t startPos = countInStartPosition.load();
    if (startPos < 0)
        return true;

    // Calculate samples per bar (4 beats in 4/4 time)
    const double beatsPerSecond = bpm / 60.0;
    const double samplesPerBeat = currentSampleRate / beatsPerSecond;
    const double samplesPerBar = samplesPerBeat * beatsPerMeasure;

    // Calculate how many bars have elapsed
    const int64_t elapsedSamples = currentPosition - startPos;
    const int64_t requiredSamples = static_cast<int64_t>(samplesPerBar * countInBars.load());

    // Count-in is complete when we've elapsed the required number of bars
    if (elapsedSamples >= requiredSamples)
    {
        // Note: We don't reset isCountingIn here because this is const
        // The caller should handle resetting the count-in state
        return true;
    }

    return false;
}
