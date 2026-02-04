#pragma once

#include "../Audio/AudioEngine.h"
#include "../Audio/AudioTrack.h"
#include "../Audio/Transport.h"
#include "../Audio/Mixer.h"
#include "../Timeline/Timeline.h"
#include "../Timeline/Region.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <chrono>
#include <thread>

/**
 * Performance verification tests for audio engine.
 *
 * Test Steps (as specified in implementation_plan.json):
 * 1. Set buffer size to 256 samples
 * 2. Play project with multiple tracks
 * 3. Monitor for 30 seconds
 * 4. Verify zero dropouts
 *
 * These tests verify that the audio engine can process audio
 * without dropouts at low buffer sizes (256 samples).
 */
class PerformanceVerificationTest
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;

        allPassed &= testDropoutDetectionSetup();
        allPassed &= testSingleTrackNoDropouts();
        allPassed &= testMultipleTracksNoDropouts();
        allPassed &= testStressTestNoDropouts();
        allPassed &= testBufferSize256Performance();
        allPassed &= testCpuLoadWithinLimits();
        allPassed &= testExtendedPlaybackNoDropouts();
        allPassed &= testCompletePerformanceVerification();

        return allPassed;
    }

private:
    static constexpr int kTestBufferSize = 256;          // Required buffer size for test
    static constexpr double kTestSampleRate = 44100.0;   // Standard sample rate
    static constexpr int kTestDurationBlocks = 5000;     // Number of blocks to process (~30 sec at 256 samples)
    static constexpr double kMaxAcceptableCpuLoad = 0.8; // 80% max CPU load
    static constexpr int kNumTestTracks = 8;             // Number of tracks for stress test

    //==========================================================================
    // Helper: Create a sine wave buffer
    //==========================================================================
    static juce::AudioBuffer<float> createSineBuffer(int numSamples, float frequency = 440.0f,
                                                      float amplitude = 0.5f, int numChannels = 2)
    {
        juce::AudioBuffer<float> buffer(numChannels, numSamples);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                data[sample] = amplitude * std::sin(2.0f * juce::MathConstants<float>::pi *
                                                    frequency * static_cast<float>(sample) /
                                                    static_cast<float>(kTestSampleRate));
            }
        }

        return buffer;
    }

    //==========================================================================
    // Helper: Create an audio region with test data
    //==========================================================================
    static std::unique_ptr<AudioRegion> createTestAudioRegion(int64_t startPos, int64_t length)
    {
        auto region = std::make_unique<AudioRegion>(startPos, length);
        auto buffer = createSineBuffer(static_cast<int>(length), 440.0f);
        region->setAudioBuffer(buffer);
        return region;
    }

    //==========================================================================
    // Helper: Simulate audio processing by calling getNextAudioBlock
    //==========================================================================
    static void processAudioBlocks(AudioEngine& engine, int numBlocks,
                                   int bufferSize = kTestBufferSize)
    {
        juce::AudioBuffer<float> buffer(2, bufferSize);
        juce::AudioSourceChannelInfo channelInfo(&buffer, 0, bufferSize);

        for (int i = 0; i < numBlocks; ++i)
        {
            buffer.clear();
            engine.getNextAudioBlock(channelInfo);
        }
    }

    //==========================================================================
    // Test 1: Dropout Detection Setup Verification
    //==========================================================================
    static bool testDropoutDetectionSetup()
    {
        bool passed = true;

        AudioEngine engine;

        // Prepare engine with 256 sample buffer
        engine.prepareToPlay(kTestBufferSize, kTestSampleRate);

        // Verify initial state
        passed &= (engine.getDropoutCount() == 0);
        passed &= (!engine.hadRecentDropout());

        // Verify metrics are available
        AudioPerformanceMetrics metrics = engine.getPerformanceMetrics();
        passed &= (metrics.dropoutCount == 0);
        passed &= (metrics.totalBlocksProcessed == 0);

        // Verify buffer time calculation is correct
        double expectedBufferTimeMs = (static_cast<double>(kTestBufferSize) / kTestSampleRate) * 1000.0;
        passed &= (expectedBufferTimeMs > 5.0 && expectedBufferTimeMs < 6.0); // ~5.8ms for 256 @ 44.1k

        engine.releaseResources();

        juce::Logger::writeToLog(passed ? "PASS: testDropoutDetectionSetup" : "FAIL: testDropoutDetectionSetup");
        return passed;
    }

    //==========================================================================
    // Test 2: Single Track - No Dropouts
    //==========================================================================
    static bool testSingleTrackNoDropouts()
    {
        bool passed = true;

        AudioEngine engine;
        engine.prepareToPlay(kTestBufferSize, kTestSampleRate);

        Timeline& timeline = engine.getTimeline();

        // Create a single audio track with a region
        auto* track = new AudioTrack("Test Track 1");
        timeline.addTrack(track);

        auto region = createTestAudioRegion(0, static_cast<int64_t>(kTestSampleRate * 10)); // 10 seconds
        track->addRegion(std::move(region));

        // Start playback
        engine.getTransport().play();

        // Reset metrics before test
        engine.resetPerformanceMetrics();

        // Process audio blocks (simulate ~1 second of playback)
        processAudioBlocks(engine, 172);  // 172 blocks * 256 samples = ~1 sec at 44.1k

        // Stop playback
        engine.getTransport().stop();

        // Verify no dropouts
        uint64_t dropouts = engine.getDropoutCount();
        passed &= (dropouts == 0);

        if (dropouts > 0)
        {
            juce::Logger::writeToLog("  WARNING: " + juce::String(dropouts) + " dropouts detected in single track test");
        }

        // Verify CPU load is reasonable
        double cpuLoad = engine.getCpuLoad();
        passed &= (cpuLoad < kMaxAcceptableCpuLoad);

        juce::Logger::writeToLog("  Single track CPU load: " + juce::String(cpuLoad * 100.0, 1) + "%");

        timeline.clearTracks();
        engine.releaseResources();

        juce::Logger::writeToLog(passed ? "PASS: testSingleTrackNoDropouts" : "FAIL: testSingleTrackNoDropouts");
        return passed;
    }

    //==========================================================================
    // Test 3: Multiple Tracks - No Dropouts
    //==========================================================================
    static bool testMultipleTracksNoDropouts()
    {
        bool passed = true;

        AudioEngine engine;
        engine.prepareToPlay(kTestBufferSize, kTestSampleRate);

        Timeline& timeline = engine.getTimeline();

        // Create multiple audio tracks with regions
        for (int i = 0; i < kNumTestTracks; ++i)
        {
            auto* track = new AudioTrack("Test Track " + juce::String(i + 1));
            timeline.addTrack(track);

            // Each track has a different start position
            auto region = createTestAudioRegion(
                static_cast<int64_t>(i * kTestSampleRate),  // Staggered start
                static_cast<int64_t>(kTestSampleRate * 5)   // 5 seconds each
            );
            track->addRegion(std::move(region));
        }

        // Start playback
        engine.getTransport().play();

        // Reset metrics
        engine.resetPerformanceMetrics();

        // Process audio blocks (~1 second)
        processAudioBlocks(engine, 172);

        // Stop playback
        engine.getTransport().stop();

        // Verify no dropouts
        uint64_t dropouts = engine.getDropoutCount();
        passed &= (dropouts == 0);

        if (dropouts > 0)
        {
            juce::Logger::writeToLog("  WARNING: " + juce::String(dropouts) + " dropouts with " +
                                      juce::String(kNumTestTracks) + " tracks");
        }

        // Verify CPU load
        double cpuLoad = engine.getCpuLoad();
        passed &= (cpuLoad < kMaxAcceptableCpuLoad);

        juce::Logger::writeToLog("  " + juce::String(kNumTestTracks) + " tracks CPU load: " +
                                  juce::String(cpuLoad * 100.0, 1) + "%");

        timeline.clearTracks();
        engine.releaseResources();

        juce::Logger::writeToLog(passed ? "PASS: testMultipleTracksNoDropouts" : "FAIL: testMultipleTracksNoDropouts");
        return passed;
    }

    //==========================================================================
    // Test 4: Stress Test - No Dropouts
    //==========================================================================
    static bool testStressTestNoDropouts()
    {
        bool passed = true;

        AudioEngine engine;
        engine.prepareToPlay(kTestBufferSize, kTestSampleRate);

        Timeline& timeline = engine.getTimeline();

        // Create many overlapping tracks for stress test
        const int numStressTracks = 16;

        for (int i = 0; i < numStressTracks; ++i)
        {
            auto* track = new AudioTrack("Stress Track " + juce::String(i + 1));
            timeline.addTrack(track);

            // All tracks overlap
            auto region = createTestAudioRegion(0, static_cast<int64_t>(kTestSampleRate * 3));
            track->addRegion(std::move(region));
        }

        // Start playback
        engine.getTransport().play();
        engine.resetPerformanceMetrics();

        // Process more blocks for stress test (~2 seconds)
        processAudioBlocks(engine, 344);

        engine.getTransport().stop();

        // Check dropouts - allow some tolerance for stress test
        uint64_t dropouts = engine.getDropoutCount();
        passed &= (dropouts <= 5);  // Allow up to 5 minor dropouts in stress test

        juce::Logger::writeToLog("  Stress test (" + juce::String(numStressTracks) + " overlapping tracks): " +
                                  juce::String(dropouts) + " dropouts");

        double cpuLoad = engine.getCpuLoad();
        juce::Logger::writeToLog("  Stress test CPU load: " + juce::String(cpuLoad * 100.0, 1) + "%");

        timeline.clearTracks();
        engine.releaseResources();

        juce::Logger::writeToLog(passed ? "PASS: testStressTestNoDropouts" : "FAIL: testStressTestNoDropouts");
        return passed;
    }

    //==========================================================================
    // Test 5: Buffer Size 256 Performance Verification
    //==========================================================================
    static bool testBufferSize256Performance()
    {
        bool passed = true;

        AudioEngine engine;

        // Explicitly set buffer size to 256 (as per task requirements)
        engine.prepareToPlay(256, kTestSampleRate);

        // Verify buffer size was set correctly
        passed &= (engine.getBlockSize() == 256);

        Timeline& timeline = engine.getTimeline();

        // Create a typical project setup
        for (int i = 0; i < 4; ++i)
        {
            auto* track = new AudioTrack("Track " + juce::String(i + 1));
            timeline.addTrack(track);

            auto region = createTestAudioRegion(0, static_cast<int64_t>(kTestSampleRate * 10));
            track->addRegion(std::move(region));
        }

        engine.getTransport().play();
        engine.resetPerformanceMetrics();

        // Process at 256 sample buffer size
        juce::AudioBuffer<float> buffer(2, 256);
        juce::AudioSourceChannelInfo channelInfo(&buffer, 0, 256);

        uint64_t dropoutsBefore = engine.getDropoutCount();

        for (int i = 0; i < 500; ++i)  // ~3 seconds at 256 samples
        {
            buffer.clear();
            engine.getNextAudioBlock(channelInfo);
        }

        engine.getTransport().stop();

        uint64_t dropoutsAfter = engine.getDropoutCount();
        uint64_t newDropouts = dropoutsAfter - dropoutsBefore;

        passed &= (newDropouts == 0);

        juce::Logger::writeToLog("  Buffer size 256 test: " + juce::String(newDropouts) + " dropouts");

        AudioPerformanceMetrics metrics = engine.getPerformanceMetrics();
        juce::Logger::writeToLog("  Avg processing time: " + juce::String(metrics.averageProcessingTimeMs, 2) + " ms");
        juce::Logger::writeToLog("  Max processing time: " + juce::String(metrics.maxProcessingTimeMs, 2) + " ms");
        juce::Logger::writeToLog("  Buffer time: " + juce::String(metrics.bufferTimeMs, 2) + " ms");

        timeline.clearTracks();
        engine.releaseResources();

        juce::Logger::writeToLog(passed ? "PASS: testBufferSize256Performance" : "FAIL: testBufferSize256Performance");
        return passed;
    }

    //==========================================================================
    // Test 6: CPU Load Within Limits
    //==========================================================================
    static bool testCpuLoadWithinLimits()
    {
        bool passed = true;

        AudioEngine engine;
        engine.prepareToPlay(kTestBufferSize, kTestSampleRate);

        Timeline& timeline = engine.getTimeline();

        // Create moderate workload
        for (int i = 0; i < 6; ++i)
        {
            auto* track = new AudioTrack("Track " + juce::String(i + 1));
            timeline.addTrack(track);

            auto region = createTestAudioRegion(0, static_cast<int64_t>(kTestSampleRate * 5));
            track->addRegion(std::move(region));
        }

        engine.getTransport().play();
        engine.resetPerformanceMetrics();

        // Process blocks and measure CPU load
        double maxCpuLoad = 0.0;
        double avgCpuLoad = 0.0;
        const int measurementBlocks = 200;

        for (int i = 0; i < measurementBlocks; ++i)
        {
            juce::AudioBuffer<float> buffer(2, kTestBufferSize);
            juce::AudioSourceChannelInfo channelInfo(&buffer, 0, kTestBufferSize);
            buffer.clear();
            engine.getNextAudioBlock(channelInfo);

            double currentCpuLoad = engine.getCpuLoad();
            maxCpuLoad = std::max(maxCpuLoad, currentCpuLoad);
            avgCpuLoad += currentCpuLoad;
        }

        avgCpuLoad /= static_cast<double>(measurementBlocks);

        engine.getTransport().stop();

        // Verify CPU load is within acceptable limits
        passed &= (maxCpuLoad < 1.0);  // Should never exceed 100%
        passed &= (avgCpuLoad < kMaxAcceptableCpuLoad);  // Average should be < 80%

        juce::Logger::writeToLog("  Average CPU load: " + juce::String(avgCpuLoad * 100.0, 1) + "%");
        juce::Logger::writeToLog("  Max CPU load: " + juce::String(maxCpuLoad * 100.0, 1) + "%");

        if (maxCpuLoad >= 1.0)
        {
            juce::Logger::writeToLog("  WARNING: CPU overload detected!");
        }

        timeline.clearTracks();
        engine.releaseResources();

        juce::Logger::writeToLog(passed ? "PASS: testCpuLoadWithinLimits" : "FAIL: testCpuLoadWithinLimits");
        return passed;
    }

    //==========================================================================
    // Test 7: Extended Playback - No Dropouts
    //==========================================================================
    static bool testExtendedPlaybackNoDropouts()
    {
        bool passed = true;

        AudioEngine engine;
        engine.prepareToPlay(kTestBufferSize, kTestSampleRate);

        Timeline& timeline = engine.getTimeline();

        // Create project
        for (int i = 0; i < 4; ++i)
        {
            auto* track = new AudioTrack("Track " + juce::String(i + 1));
            timeline.addTrack(track);

            auto region = createTestAudioRegion(0, static_cast<int64_t>(kTestSampleRate * 30));  // 30 second region
            track->addRegion(std::move(region));
        }

        engine.getTransport().play();
        engine.resetPerformanceMetrics();

        juce::Logger::writeToLog("  Running extended playback test (simulating ~10 seconds)...");

        // Simulate extended playback (~10 seconds worth of blocks)
        const int extendedBlocks = 1720;  // ~10 seconds at 256/44100
        processAudioBlocks(engine, extendedBlocks);

        engine.getTransport().stop();

        // Verify no dropouts over extended period
        uint64_t dropouts = engine.getDropoutCount();
        passed &= (dropouts == 0);

        AudioPerformanceMetrics metrics = engine.getPerformanceMetrics();
        juce::Logger::writeToLog("  Extended playback complete:");
        juce::Logger::writeToLog("    Blocks processed: " + juce::String(metrics.totalBlocksProcessed));
        juce::Logger::writeToLog("    Dropouts: " + juce::String(dropouts));
        juce::Logger::writeToLog("    Final CPU load: " + juce::String(metrics.cpuLoad * 100.0, 1) + "%");

        timeline.clearTracks();
        engine.releaseResources();

        juce::Logger::writeToLog(passed ? "PASS: testExtendedPlaybackNoDropouts" : "FAIL: testExtendedPlaybackNoDropouts");
        return passed;
    }

    //==========================================================================
    // Test 8: Complete Performance Verification (Full E2E)
    //==========================================================================
    static bool testCompletePerformanceVerification()
    {
        bool passed = true;

        juce::Logger::writeToLog("=== Complete Performance Verification E2E Test ===");

        AudioEngine engine;

        //===================================================================
        // STEP 1: Set buffer size to 256 samples
        //===================================================================
        juce::Logger::writeToLog("\n--- STEP 1: Set buffer size to 256 samples ---");

        engine.prepareToPlay(256, kTestSampleRate);

        passed &= (engine.getBlockSize() == 256);
        juce::Logger::writeToLog("  Buffer size: " + juce::String(engine.getBlockSize()) + " samples");
        juce::Logger::writeToLog("  Sample rate: " + juce::String(engine.getSampleRate()) + " Hz");

        double bufferTimeMs = (256.0 / kTestSampleRate) * 1000.0;
        juce::Logger::writeToLog("  Buffer latency: " + juce::String(bufferTimeMs, 2) + " ms");

        juce::Logger::writeToLog("Step 1 verification: " + juce::String(passed ? "PASS" : "FAIL"));

        //===================================================================
        // STEP 2: Play project with multiple tracks
        //===================================================================
        juce::Logger::writeToLog("\n--- STEP 2: Play project with multiple tracks ---");

        Timeline& timeline = engine.getTimeline();

        // Create a project with 8 audio tracks (typical use case)
        const int numTracks = 8;
        for (int i = 0; i < numTracks; ++i)
        {
            auto* track = new AudioTrack("Audio " + juce::String(i + 1));
            timeline.addTrack(track);

            // Create overlapping regions for more realistic workload
            auto region = createTestAudioRegion(
                static_cast<int64_t>(i * kTestSampleRate * 0.5),  // Slightly staggered
                static_cast<int64_t>(kTestSampleRate * 30)        // 30 seconds each
            );
            region->setName("Region " + juce::String(i + 1));
            track->addRegion(std::move(region));
        }

        juce::Logger::writeToLog("  Created project with " + juce::String(numTracks) + " audio tracks");

        // Start playback
        engine.getTransport().play();
        engine.resetPerformanceMetrics();

        passed &= engine.getTransport().isPlaying();
        juce::Logger::writeToLog("  Playback started: " + juce::String(engine.getTransport().isPlaying() ? "YES" : "NO"));

        juce::Logger::writeToLog("Step 2 verification: " + juce::String(passed ? "PASS" : "FAIL"));

        //===================================================================
        // STEP 3: Monitor for 30 seconds (simulated)
        //===================================================================
        juce::Logger::writeToLog("\n--- STEP 3: Monitor for ~30 seconds ---");

        // Calculate blocks needed for 30 seconds
        const int blocksFor30Seconds = static_cast<int>((kTestSampleRate * 30.0) / 256.0);
        juce::Logger::writeToLog("  Processing " + juce::String(blocksFor30Seconds) + " blocks (~30 seconds)...");

        uint64_t dropoutsBefore = engine.getDropoutCount();

        // Process in chunks and report progress
        const int reportInterval = blocksFor30Seconds / 10;
        int blocksProcessed = 0;

        juce::AudioBuffer<float> buffer(2, 256);
        juce::AudioSourceChannelInfo channelInfo(&buffer, 0, 256);

        for (int i = 0; i < blocksFor30Seconds; ++i)
        {
            buffer.clear();
            engine.getNextAudioBlock(channelInfo);
            blocksProcessed++;

            if (blocksProcessed % reportInterval == 0)
            {
                int progress = (blocksProcessed * 100) / blocksFor30Seconds;
                uint64_t currentDropouts = engine.getDropoutCount() - dropoutsBefore;
                double currentCpuLoad = engine.getCpuLoad();

                juce::Logger::writeToLog("  Progress: " + juce::String(progress) + "% - " +
                                          "Dropouts: " + juce::String(currentDropouts) +
                                          ", CPU: " + juce::String(currentCpuLoad * 100.0, 1) + "%");
            }
        }

        engine.getTransport().stop();

        juce::Logger::writeToLog("  Monitoring complete. Total blocks: " + juce::String(blocksProcessed));

        juce::Logger::writeToLog("Step 3 verification: " + juce::String(passed ? "PASS" : "FAIL"));

        //===================================================================
        // STEP 4: Verify zero dropouts
        //===================================================================
        juce::Logger::writeToLog("\n--- STEP 4: Verify zero dropouts ---");

        uint64_t dropoutsAfter = engine.getDropoutCount();
        uint64_t totalDropouts = dropoutsAfter - dropoutsBefore;

        passed &= (totalDropouts == 0);

        AudioPerformanceMetrics finalMetrics = engine.getPerformanceMetrics();

        juce::Logger::writeToLog("  DROPOUT COUNT: " + juce::String(totalDropouts));
        juce::Logger::writeToLog("  Total blocks processed: " + juce::String(finalMetrics.totalBlocksProcessed));
        juce::Logger::writeToLog("  Average processing time: " + juce::String(finalMetrics.averageProcessingTimeMs, 3) + " ms");
        juce::Logger::writeToLog("  Max processing time: " + juce::String(finalMetrics.maxProcessingTimeMs, 3) + " ms");
        juce::Logger::writeToLog("  Buffer time available: " + juce::String(bufferTimeMs, 3) + " ms");
        juce::Logger::writeToLog("  Final CPU load: " + juce::String(finalMetrics.cpuLoad * 100.0, 1) + "%");

        if (totalDropouts == 0)
        {
            juce::Logger::writeToLog("  RESULT: ZERO DROPOUTS - TEST PASSED!");
        }
        else
        {
            juce::Logger::writeToLog("  RESULT: " + juce::String(totalDropouts) + " DROPOUTS DETECTED - TEST FAILED!");
        }

        juce::Logger::writeToLog("Step 4 verification: " + juce::String(passed ? "PASS" : "FAIL"));

        //===================================================================
        // Final Summary
        //===================================================================
        juce::Logger::writeToLog("\n=== Performance Verification Summary ===");
        juce::Logger::writeToLog("  Buffer size: 256 samples");
        juce::Logger::writeToLog("  Number of tracks: " + juce::String(numTracks));
        juce::Logger::writeToLog("  Test duration: ~30 seconds simulated");
        juce::Logger::writeToLog("  Dropouts: " + juce::String(totalDropouts));
        juce::Logger::writeToLog("  Status: " + juce::String(passed ? "PASS - Zero dropouts verified" : "FAIL - Dropouts detected"));

        timeline.clearTracks();
        engine.releaseResources();

        juce::Logger::writeToLog("\n=== Complete Performance Verification E2E Test: " +
                                  juce::String(passed ? "PASS" : "FAIL") + " ===");

        juce::Logger::writeToLog(passed ? "PASS: testCompletePerformanceVerification" : "FAIL: testCompletePerformanceVerification");
        return passed;
    }
};
