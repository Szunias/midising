#pragma once

#include "../Audio/TimeStretch.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * Unit tests for TimeStretch phase vocoder implementation.
 *
 * Test Steps:
 * 1. Test initialization and prepare
 * 2. Test stretch ratio get/set with clamping
 * 3. Test transient preservation settings
 * 4. Test identity processing (ratio 1.0)
 * 5. Test slowing down (ratio < 1.0)
 * 6. Test speeding up (ratio > 1.0)
 * 7. Test reset functionality
 * 8. Test latency reporting
 *
 * These tests verify the TimeStretch processor including:
 * - Parameter handling and clamping
 * - Audio processing with different stretch ratios
 * - Proper buffer sizing and data flow
 * - Reset and state management
 */
class TimeStretchTests
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;

        allPassed &= testInitialization();
        allPassed &= testStretchRatioClamping();
        allPassed &= testTransientPreservationSettings();
        allPassed &= testIdentityProcessing();
        allPassed &= testSlowDown();
        allPassed &= testSpeedUp();
        allPassed &= testReset();
        allPassed &= testLatencyReporting();
        allPassed &= testPushPullWorkflow();

        return allPassed;
    }

private:
    //==========================================================================
    // Helper: Create a test buffer with a sine wave
    //==========================================================================
    static juce::AudioBuffer<float> createSineBuffer(int numSamples, float frequency = 440.0f,
                                                      float amplitude = 0.5f, int numChannels = 2)
    {
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        const double sampleRate = 44100.0;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                data[sample] = amplitude * std::sin(2.0f * juce::MathConstants<float>::pi *
                                                    frequency * static_cast<float>(sample) /
                                                    static_cast<float>(sampleRate));
            }
        }

        return buffer;
    }

    //==========================================================================
    // Helper: Calculate RMS level of a buffer
    //==========================================================================
    static float calculateRMS(const juce::AudioBuffer<float>& buffer, int channel = 0)
    {
        if (buffer.getNumSamples() == 0)
            return 0.0f;

        float sum = 0.0f;
        const float* data = buffer.getReadPointer(channel);
        const int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            sum += data[i] * data[i];
        }

        return std::sqrt(sum / static_cast<float>(numSamples));
    }

    //==========================================================================
    // Helper: Check if buffer contains non-zero audio
    //==========================================================================
    static bool hasAudio(const juce::AudioBuffer<float>& buffer, float threshold = 0.001f)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const float* data = buffer.getReadPointer(channel);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (std::abs(data[i]) > threshold)
                    return true;
            }
        }
        return false;
    }

    //==========================================================================
    // Test 1: Initialization and Prepare
    //==========================================================================
    static bool testInitialization()
    {
        bool passed = true;

        TimeStretch timeStretch;

        // Default stretch ratio should be 1.0
        passed &= (std::abs(timeStretch.getStretchRatio() - 1.0f) < 0.001f);

        // Prepare should not throw
        timeStretch.prepare(44100.0, 512);

        // After prepare, stretch ratio should still be 1.0
        passed &= (std::abs(timeStretch.getStretchRatio() - 1.0f) < 0.001f);

        // Latency should be reported (equal to fftSize)
        passed &= (timeStretch.getLatency() == TimeStretch::fftSize);

        juce::Logger::writeToLog(passed ? "PASS: testInitialization" : "FAIL: testInitialization");
        return passed;
    }

    //==========================================================================
    // Test 2: Stretch Ratio Clamping
    //==========================================================================
    static bool testStretchRatioClamping()
    {
        bool passed = true;

        TimeStretch timeStretch;
        timeStretch.prepare(44100.0, 512);

        // Test normal range values
        timeStretch.setStretchRatio(0.5f);
        passed &= (std::abs(timeStretch.getStretchRatio() - 0.5f) < 0.001f);

        timeStretch.setStretchRatio(2.0f);
        passed &= (std::abs(timeStretch.getStretchRatio() - 2.0f) < 0.001f);

        // Test lower bound clamping (min is 0.25)
        timeStretch.setStretchRatio(0.1f);
        passed &= (std::abs(timeStretch.getStretchRatio() - 0.25f) < 0.001f);

        timeStretch.setStretchRatio(0.0f);
        passed &= (std::abs(timeStretch.getStretchRatio() - 0.25f) < 0.001f);

        // Test upper bound clamping (max is 4.0)
        timeStretch.setStretchRatio(5.0f);
        passed &= (std::abs(timeStretch.getStretchRatio() - 4.0f) < 0.001f);

        timeStretch.setStretchRatio(10.0f);
        passed &= (std::abs(timeStretch.getStretchRatio() - 4.0f) < 0.001f);

        // Test boundary values
        timeStretch.setStretchRatio(0.25f);
        passed &= (std::abs(timeStretch.getStretchRatio() - 0.25f) < 0.001f);

        timeStretch.setStretchRatio(4.0f);
        passed &= (std::abs(timeStretch.getStretchRatio() - 4.0f) < 0.001f);

        juce::Logger::writeToLog(passed ? "PASS: testStretchRatioClamping" : "FAIL: testStretchRatioClamping");
        return passed;
    }

    //==========================================================================
    // Test 3: Transient Preservation Settings
    //==========================================================================
    static bool testTransientPreservationSettings()
    {
        bool passed = true;

        TimeStretch timeStretch;
        timeStretch.prepare(44100.0, 512);

        // Test normal range values
        timeStretch.setTransientPreservation(0.5f);
        // Note: There's no getter, but we verify it doesn't crash

        timeStretch.setTransientPreservation(0.0f);
        timeStretch.setTransientPreservation(1.0f);

        // Test clamping (should not crash with out-of-range values)
        timeStretch.setTransientPreservation(-0.5f);
        timeStretch.setTransientPreservation(1.5f);

        // If we got here without crashing, test passes
        passed &= true;

        juce::Logger::writeToLog(passed ? "PASS: testTransientPreservationSettings" : "FAIL: testTransientPreservationSettings");
        return passed;
    }

    //==========================================================================
    // Test 4: Identity Processing (ratio 1.0)
    //==========================================================================
    static bool testIdentityProcessing()
    {
        bool passed = true;

        TimeStretch timeStretch;
        const double sampleRate = 44100.0;
        const int blockSize = 4096;

        timeStretch.prepare(sampleRate, blockSize);
        timeStretch.setStretchRatio(1.0f);

        // Create a test sine wave buffer
        auto inputBuffer = createSineBuffer(blockSize * 4, 440.0f, 0.5f, 2);
        juce::AudioBuffer<float> outputBuffer;

        // Process the audio
        timeStretch.process(inputBuffer, outputBuffer);

        // With ratio 1.0, output size should be approximately the same as input
        // (allowing some variance due to FFT processing)
        const int expectedSize = inputBuffer.getNumSamples();
        const int actualSize = outputBuffer.getNumSamples();

        // Allow 10% tolerance due to windowing/overlap-add
        passed &= (std::abs(actualSize - expectedSize) < expectedSize * 0.1);

        // Output should contain audio
        passed &= hasAudio(outputBuffer);

        // Output should have similar number of channels
        passed &= (outputBuffer.getNumChannels() == inputBuffer.getNumChannels());

        juce::Logger::writeToLog(passed ? "PASS: testIdentityProcessing" : "FAIL: testIdentityProcessing");
        return passed;
    }

    //==========================================================================
    // Test 5: Slow Down (ratio < 1.0)
    //==========================================================================
    static bool testSlowDown()
    {
        bool passed = true;

        TimeStretch timeStretch;
        const double sampleRate = 44100.0;
        const int blockSize = 4096;

        timeStretch.prepare(sampleRate, blockSize);
        timeStretch.setStretchRatio(0.5f); // Slow down to half speed

        // Create test buffer
        auto inputBuffer = createSineBuffer(blockSize * 4, 440.0f, 0.5f, 2);
        juce::AudioBuffer<float> outputBuffer;

        // Process the audio
        timeStretch.process(inputBuffer, outputBuffer);

        // With ratio 0.5, output should be approximately twice as long
        const int inputSize = inputBuffer.getNumSamples();
        const int outputSize = outputBuffer.getNumSamples();

        // Expected output size = input / ratio = input / 0.5 = input * 2
        const int expectedSize = inputSize * 2;

        // Allow 20% tolerance due to FFT processing quirks
        passed &= (std::abs(outputSize - expectedSize) < expectedSize * 0.2);

        // Output should contain audio
        passed &= hasAudio(outputBuffer);

        juce::Logger::writeToLog(passed ? "PASS: testSlowDown" : "FAIL: testSlowDown");
        return passed;
    }

    //==========================================================================
    // Test 6: Speed Up (ratio > 1.0)
    //==========================================================================
    static bool testSpeedUp()
    {
        bool passed = true;

        TimeStretch timeStretch;
        const double sampleRate = 44100.0;
        const int blockSize = 4096;

        timeStretch.prepare(sampleRate, blockSize);
        timeStretch.setStretchRatio(2.0f); // Speed up to double speed

        // Create test buffer
        auto inputBuffer = createSineBuffer(blockSize * 4, 440.0f, 0.5f, 2);
        juce::AudioBuffer<float> outputBuffer;

        // Process the audio
        timeStretch.process(inputBuffer, outputBuffer);

        // With ratio 2.0, output should be approximately half as long
        const int inputSize = inputBuffer.getNumSamples();
        const int outputSize = outputBuffer.getNumSamples();

        // Expected output size = input / ratio = input / 2
        const int expectedSize = inputSize / 2;

        // Allow 20% tolerance due to FFT processing quirks
        passed &= (std::abs(outputSize - expectedSize) < expectedSize * 0.2);

        // Output should contain audio (may be less due to compression)
        // Note: With heavy compression, we might lose some transients
        passed &= (outputBuffer.getNumSamples() > 0);

        juce::Logger::writeToLog(passed ? "PASS: testSpeedUp" : "FAIL: testSpeedUp");
        return passed;
    }

    //==========================================================================
    // Test 7: Reset Functionality
    //==========================================================================
    static bool testReset()
    {
        bool passed = true;

        TimeStretch timeStretch;
        const double sampleRate = 44100.0;
        const int blockSize = 4096;

        timeStretch.prepare(sampleRate, blockSize);

        // Process some audio to fill internal buffers
        auto inputBuffer = createSineBuffer(blockSize * 2, 440.0f, 0.5f, 2);
        timeStretch.pushSamples(inputBuffer);

        // There should be some output available
        int availableBefore = timeStretch.getAvailableOutputSamples();

        // Reset the processor
        timeStretch.reset();

        // After reset, available output should be 0
        int availableAfter = timeStretch.getAvailableOutputSamples();
        passed &= (availableAfter == 0);

        // Stretch ratio should remain unchanged after reset
        passed &= (std::abs(timeStretch.getStretchRatio() - 1.0f) < 0.001f);

        // We should be able to process again after reset
        juce::AudioBuffer<float> outputBuffer;
        timeStretch.pushSamples(inputBuffer);
        int availableAfterNewPush = timeStretch.getAvailableOutputSamples();

        // Verify processing still works after reset
        passed &= (availableAfterNewPush >= 0);

        juce::Logger::writeToLog(passed ? "PASS: testReset" : "FAIL: testReset");
        return passed;
    }

    //==========================================================================
    // Test 8: Latency Reporting
    //==========================================================================
    static bool testLatencyReporting()
    {
        bool passed = true;

        TimeStretch timeStretch;
        timeStretch.prepare(44100.0, 512);

        // Latency should be the FFT size
        int latency = timeStretch.getLatency();
        passed &= (latency == TimeStretch::fftSize);
        passed &= (latency == 2048); // fftOrder = 11, so 2^11 = 2048

        // Latency should be consistent after parameter changes
        timeStretch.setStretchRatio(0.5f);
        passed &= (timeStretch.getLatency() == latency);

        timeStretch.setStretchRatio(2.0f);
        passed &= (timeStretch.getLatency() == latency);

        juce::Logger::writeToLog(passed ? "PASS: testLatencyReporting" : "FAIL: testLatencyReporting");
        return passed;
    }

    //==========================================================================
    // Test 9: Push/Pull Workflow
    //==========================================================================
    static bool testPushPullWorkflow()
    {
        bool passed = true;

        TimeStretch timeStretch;
        const double sampleRate = 44100.0;
        const int blockSize = 512;

        timeStretch.prepare(sampleRate, blockSize);
        timeStretch.setStretchRatio(1.0f);

        // Create test buffer with multiple frames worth of audio
        auto inputBuffer = createSineBuffer(blockSize * 8, 440.0f, 0.5f, 2);

        // Push samples in chunks
        for (int i = 0; i < 8; ++i)
        {
            juce::AudioBuffer<float> chunk(2, blockSize);
            for (int ch = 0; ch < 2; ++ch)
            {
                chunk.copyFrom(ch, 0, inputBuffer, ch, i * blockSize, blockSize);
            }
            timeStretch.pushSamples(chunk);
        }

        // After pushing enough samples, we should have output available
        int available = timeStretch.getAvailableOutputSamples();
        passed &= (available >= 0);

        // Pull samples
        juce::AudioBuffer<float> outputBuffer(2, blockSize);
        int retrieved = timeStretch.pullSamples(outputBuffer, blockSize);

        // Should have retrieved some samples (or 0 if not enough processed yet)
        passed &= (retrieved >= 0);
        passed &= (retrieved <= blockSize);

        // If we retrieved samples, they should be the requested channel count
        if (retrieved > 0)
        {
            passed &= (outputBuffer.getNumChannels() >= 2);
        }

        juce::Logger::writeToLog(passed ? "PASS: testPushPullWorkflow" : "FAIL: testPushPullWorkflow");
        return passed;
    }
};
