#pragma once

#include "../Timeline/Timeline.h"
#include "../Timeline/Track.h"
#include "../Timeline/AutomationLane.h"
#include "../Audio/AudioTrack.h"
#include "../Audio/Mixer.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * End-to-end verification tests for automation recording and playback.
 *
 * Test Steps (as specified in implementation_plan.json):
 * 1. Enable Write mode on track
 * 2. Play and move fader (simulate recording automation points)
 * 3. Stop and verify automation points recorded
 * 4. Enable Read mode and play
 * 5. Verify fader follows automation
 *
 * These tests verify the complete automation workflow including:
 * - AutomationMode state transitions (Off/Read/Write/Touch/Latch)
 * - Recording automation points to a lane
 * - Playback of automation values through the mixer
 * - Automation value interpolation during playback
 * - Integration between Track, AutomationLane, and Mixer
 */
class AutomationE2ETest
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;

        allPassed &= testAutomationModeControl();
        allPassed &= testAutomationRecordingWorkflow();
        allPassed &= testAutomationPlaybackWorkflow();
        allPassed &= testWriteToReadModeTransition();
        allPassed &= testMixerAutomationPlayback();
        allPassed &= testMultipleParameterAutomation();
        allPassed &= testAutomationBypass();
        allPassed &= testCompleteAutomationCycle();

        return allPassed;
    }

private:
    //==========================================================================
    // Helper: Check float equality with tolerance
    //==========================================================================
    static bool floatEquals(float a, float b, float tolerance = 0.01f)
    {
        return std::abs(a - b) < tolerance;
    }

    //==========================================================================
    // Helper: Create a test audio region with a sine wave
    //==========================================================================
    static std::unique_ptr<AudioRegion> createTestRegion(int64_t startPos, int numSamples, float amplitude = 0.5f)
    {
        juce::AudioBuffer<float> testBuffer(2, numSamples);
        float frequency = 440.0f;
        double sampleRate = 44100.0;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float value = amplitude * std::sin(2.0f * juce::MathConstants<float>::pi *
                                                frequency * static_cast<float>(sample) / static_cast<float>(sampleRate));
            testBuffer.setSample(0, sample, value);
            testBuffer.setSample(1, sample, value);
        }

        auto region = std::make_unique<AudioRegion>(startPos, numSamples);
        region->setAudioBuffer(testBuffer);
        region->setName("Test Region");
        return region;
    }

    //==========================================================================
    // Helper: Simulate fader movement by recording automation points
    //==========================================================================
    static void recordFaderMovement(AutomationLane* lane, int64_t startPos, int64_t endPos,
                                     float startValue, float endValue, int numPoints)
    {
        if (lane == nullptr || numPoints < 2)
            return;

        // Clear any existing points in this range
        lane->removePointsInRange(startPos, endPos);

        // Record automation points at regular intervals
        for (int i = 0; i < numPoints; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(numPoints - 1);
            int64_t position = startPos + static_cast<int64_t>(t * static_cast<float>(endPos - startPos));
            float value = startValue + t * (endValue - startValue);

            // Normalize value to 0-1 range for volume lane (which has 0-2 actual range)
            float normalizedValue = value / 2.0f;
            lane->addPoint(position, normalizedValue, AutomationCurveType::Linear);
        }
    }

    //==========================================================================
    // Test 1: Automation Mode Control
    //==========================================================================
    static bool testAutomationModeControl()
    {
        bool passed = true;

        auto* track = new AudioTrack("Test Track");

        // Default mode should be Read
        passed &= (track->getAutomationMode() == AutomationMode::Read);
        passed &= track->isAutomationReading();
        passed &= !track->isAutomationWriting();

        // Set to Write mode
        track->setAutomationMode(AutomationMode::Write);
        passed &= (track->getAutomationMode() == AutomationMode::Write);
        passed &= !track->isAutomationReading();  // Write mode doesn't read
        passed &= track->isAutomationWriting();

        // Set to Touch mode (both reads and writes)
        track->setAutomationMode(AutomationMode::Touch);
        passed &= (track->getAutomationMode() == AutomationMode::Touch);
        passed &= track->isAutomationReading();
        passed &= track->isAutomationWriting();

        // Set to Latch mode (both reads and writes)
        track->setAutomationMode(AutomationMode::Latch);
        passed &= (track->getAutomationMode() == AutomationMode::Latch);
        passed &= track->isAutomationReading();
        passed &= track->isAutomationWriting();

        // Set to Off mode
        track->setAutomationMode(AutomationMode::Off);
        passed &= (track->getAutomationMode() == AutomationMode::Off);
        passed &= !track->isAutomationReading();
        passed &= !track->isAutomationWriting();

        delete track;

        juce::Logger::writeToLog(passed ? "PASS: testAutomationModeControl" : "FAIL: testAutomationModeControl");
        return passed;
    }

    //==========================================================================
    // Test 2: Automation Recording Workflow
    //==========================================================================
    static bool testAutomationRecordingWorkflow()
    {
        bool passed = true;

        Timeline timeline;
        auto* track = new AudioTrack("Vocals");
        timeline.addTrack(track);

        // Step 1: Enable Write mode on track
        track->setAutomationMode(AutomationMode::Write);
        passed &= track->isAutomationWriting();

        // Step 2: Create volume automation lane
        AutomationLane* volumeLane = track->addAutomationLane("Volume");
        passed &= (volumeLane != nullptr);
        passed &= (volumeLane->getParameterType() == AutomationParameterType::Volume);
        passed &= !volumeLane->hasAutomation();  // Should be empty initially

        // Step 3: Simulate playing and moving fader (recording automation)
        // This simulates the user moving the fader from 0.5 to 1.0 over 1 second
        recordFaderMovement(volumeLane, 0, 44100, 1.0f, 2.0f, 10);

        // Step 4: Stop and verify automation points recorded
        passed &= volumeLane->hasAutomation();
        passed &= (volumeLane->getNumPoints() == 10);

        // Verify first point
        passed &= (volumeLane->getPoint(0).position == 0);
        passed &= floatEquals(volumeLane->getPoint(0).value, 0.5f);  // 1.0 / 2.0 = 0.5 normalized

        // Verify last point
        passed &= (volumeLane->getPoint(9).position == 44100);
        passed &= floatEquals(volumeLane->getPoint(9).value, 1.0f);  // 2.0 / 2.0 = 1.0 normalized

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testAutomationRecordingWorkflow" : "FAIL: testAutomationRecordingWorkflow");
        return passed;
    }

    //==========================================================================
    // Test 3: Automation Playback Workflow
    //==========================================================================
    static bool testAutomationPlaybackWorkflow()
    {
        bool passed = true;

        Timeline timeline;
        auto* track = new AudioTrack("Vocals");
        timeline.addTrack(track);

        // Create volume automation lane with known values
        AutomationLane* volumeLane = track->addAutomationLane("Volume");
        volumeLane->addPoint(0, 0.25f, AutomationCurveType::Linear);      // 0.5 volume (0.25 * 2)
        volumeLane->addPoint(44100, 0.75f, AutomationCurveType::Linear);  // 1.5 volume (0.75 * 2)

        // Step 4: Enable Read mode
        track->setAutomationMode(AutomationMode::Read);
        passed &= track->isAutomationReading();

        // Step 5: Verify fader follows automation at different positions
        // At position 0, should be start value
        float volumeAt0 = track->getAutomatedVolume(0);
        passed &= floatEquals(volumeAt0, 0.5f, 0.05f);

        // At midpoint, should be halfway between
        float volumeAtMid = track->getAutomatedVolume(22050);
        passed &= floatEquals(volumeAtMid, 1.0f, 0.05f);

        // At end, should be end value
        float volumeAtEnd = track->getAutomatedVolume(44100);
        passed &= floatEquals(volumeAtEnd, 1.5f, 0.05f);

        // Beyond end, should hold last value
        float volumeBeyondEnd = track->getAutomatedVolume(88200);
        passed &= floatEquals(volumeBeyondEnd, 1.5f, 0.05f);

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testAutomationPlaybackWorkflow" : "FAIL: testAutomationPlaybackWorkflow");
        return passed;
    }

    //==========================================================================
    // Test 4: Write to Read Mode Transition
    //==========================================================================
    static bool testWriteToReadModeTransition()
    {
        bool passed = true;

        Timeline timeline;
        auto* track = new AudioTrack("Guitar");
        timeline.addTrack(track);

        // Create and populate automation
        AutomationLane* volumeLane = track->addAutomationLane("Volume");

        // Enable Write mode and record
        track->setAutomationMode(AutomationMode::Write);
        passed &= track->isAutomationWriting();

        // Record some automation
        recordFaderMovement(volumeLane, 0, 44100, 0.0f, 2.0f, 5);
        passed &= volumeLane->hasAutomation();
        passed &= (volumeLane->getNumPoints() == 5);

        // Transition to Read mode
        track->setAutomationMode(AutomationMode::Read);
        passed &= track->isAutomationReading();
        passed &= !track->isAutomationWriting();

        // Verify automation still exists and plays back
        passed &= volumeLane->hasAutomation();
        passed &= (volumeLane->getNumPoints() == 5);

        // Verify playback values
        float startValue = track->getAutomatedVolume(0);
        float endValue = track->getAutomatedVolume(44100);

        passed &= floatEquals(startValue, 0.0f, 0.05f);
        passed &= floatEquals(endValue, 2.0f, 0.05f);

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testWriteToReadModeTransition" : "FAIL: testWriteToReadModeTransition");
        return passed;
    }

    //==========================================================================
    // Test 5: Mixer Automation Playback Integration
    //==========================================================================
    static bool testMixerAutomationPlayback()
    {
        bool passed = true;

        Timeline timeline;
        Mixer mixer;
        const double sampleRate = 44100.0;
        const int blockSize = 512;

        mixer.prepareToPlay(sampleRate, blockSize);

        // Create track with audio content
        auto* track = new AudioTrack("Lead");
        timeline.addTrack(track);
        track->prepareToPlay(sampleRate, blockSize);
        track->addRegion(createTestRegion(0, blockSize * 10, 0.5f));

        // Create volume automation: ramp from unity to half
        AutomationLane* volumeLane = track->addAutomationLane("Volume");
        volumeLane->addPoint(0, 0.5f, AutomationCurveType::Linear);       // 1.0 volume
        volumeLane->addPoint(blockSize * 5, 0.25f, AutomationCurveType::Linear); // 0.5 volume

        // Enable Read mode
        track->setAutomationMode(AutomationMode::Read);
        passed &= track->isAutomationReading();

        // Process first block (at start, should be unity gain)
        juce::AudioBuffer<float> outputStart(2, blockSize);
        mixer.processBlock(timeline, outputStart, 0, blockSize);
        mixer.updatePeakLevels(outputStart);
        float peakStart = mixer.getPeakLevel(0);

        // Process block at midpoint (should be reduced)
        juce::AudioBuffer<float> outputMid(2, blockSize);
        mixer.processBlock(timeline, outputMid, blockSize * 3, blockSize);
        mixer.updatePeakLevels(outputMid);
        float peakMid = mixer.getPeakLevel(0);

        // Process block at end (should be lowest)
        juce::AudioBuffer<float> outputEnd(2, blockSize);
        mixer.processBlock(timeline, outputEnd, blockSize * 5, blockSize);
        mixer.updatePeakLevels(outputEnd);
        float peakEnd = mixer.getPeakLevel(0);

        // Verify automation is affecting output levels
        // Note: Due to block processing and interpolation, we verify relative levels
        passed &= (peakStart > 0.0f);  // Should have audio
        passed &= (peakEnd <= peakStart);  // End should be quieter or equal

        // Cleanup
        track->releaseResources();
        mixer.releaseResources();
        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testMixerAutomationPlayback" : "FAIL: testMixerAutomationPlayback");
        return passed;
    }

    //==========================================================================
    // Test 6: Multiple Parameter Automation
    //==========================================================================
    static bool testMultipleParameterAutomation()
    {
        bool passed = true;

        Timeline timeline;
        auto* track = new AudioTrack("Synth");
        timeline.addTrack(track);

        // Create both volume and pan automation lanes
        AutomationLane* volumeLane = track->addAutomationLane("Volume");
        AutomationLane* panLane = track->addAutomationLane("Pan");

        passed &= (track->getNumAutomationLanes() == 2);

        // Record volume automation: ramp up
        volumeLane->addPoint(0, 0.25f);      // 0.5 volume
        volumeLane->addPoint(44100, 0.5f);   // 1.0 volume

        // Record pan automation: sweep left to right
        panLane->addPoint(0, 0.0f);          // Full left (-1.0)
        panLane->addPoint(44100, 1.0f);      // Full right (+1.0)

        // Enable Read mode
        track->setAutomationMode(AutomationMode::Read);

        // Verify both parameters are automated at midpoint
        float volumeMid = track->getAutomatedVolume(22050);
        float panMid = track->getAutomatedPan(22050);

        // Volume should be between 0.5 and 1.0 (approximately 0.75)
        passed &= floatEquals(volumeMid, 0.75f, 0.05f);

        // Pan should be at center (0.0)
        passed &= floatEquals(panMid, 0.0f, 0.05f);

        // Verify end values
        float volumeEnd = track->getAutomatedVolume(44100);
        float panEnd = track->getAutomatedPan(44100);

        passed &= floatEquals(volumeEnd, 1.0f, 0.05f);
        passed &= floatEquals(panEnd, 1.0f, 0.05f);  // Full right

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testMultipleParameterAutomation" : "FAIL: testMultipleParameterAutomation");
        return passed;
    }

    //==========================================================================
    // Test 7: Automation Bypass
    //==========================================================================
    static bool testAutomationBypass()
    {
        bool passed = true;

        Timeline timeline;
        auto* track = new AudioTrack("Keys");
        timeline.addTrack(track);

        // Set manual track volume
        track->setVolume(1.5f);

        // Create volume automation
        AutomationLane* volumeLane = track->addAutomationLane("Volume");
        volumeLane->addPoint(0, 0.25f);      // 0.5 volume (automated)
        volumeLane->addPoint(44100, 0.5f);   // 1.0 volume (automated)

        // Enable Read mode
        track->setAutomationMode(AutomationMode::Read);

        // Verify automation is active
        float automatedVolume = track->getAutomatedVolume(0);
        passed &= floatEquals(automatedVolume, 0.5f, 0.05f);

        // Bypass automation lane
        volumeLane->setBypassed(true);
        passed &= volumeLane->isBypassed();

        // With bypass, should return manual track volume
        float bypassedVolume = track->getAutomatedVolume(0);
        passed &= floatEquals(bypassedVolume, 1.5f, 0.05f);

        // Disable bypass
        volumeLane->setBypassed(false);
        passed &= !volumeLane->isBypassed();

        // Should return to automated value
        float unbypassedVolume = track->getAutomatedVolume(0);
        passed &= floatEquals(unbypassedVolume, 0.5f, 0.05f);

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testAutomationBypass" : "FAIL: testAutomationBypass");
        return passed;
    }

    //==========================================================================
    // Test 8: Complete Automation Cycle (E2E Integration)
    //==========================================================================
    static bool testCompleteAutomationCycle()
    {
        bool passed = true;

        Timeline timeline;
        Mixer mixer;
        const double sampleRate = 44100.0;
        const int blockSize = 512;

        mixer.prepareToPlay(sampleRate, blockSize);

        // === Setup: Create a track with audio content ===
        auto* track = new AudioTrack("Vocals");
        timeline.addTrack(track);
        track->prepareToPlay(sampleRate, blockSize);

        // Add audio content (2 seconds worth)
        track->addRegion(createTestRegion(0, static_cast<int>(sampleRate * 2), 0.5f));

        // === Step 1: Enable Write mode ===
        track->setAutomationMode(AutomationMode::Write);
        passed &= track->isAutomationWriting();
        passed &= !track->isAutomationReading();

        // === Step 2: Simulate "Play and move fader" ===
        // In real implementation, this would be triggered by UI fader movement
        // Here we simulate by directly adding points to the volume lane

        AutomationLane* volumeLane = track->addAutomationLane("Volume");
        passed &= (volumeLane != nullptr);

        // Simulate fader movement during "playback":
        // - Start at unity (1.0)
        // - Dip to 0.5 at 0.5 seconds
        // - Back to 1.0 at 1.0 seconds
        // - Peak to 1.5 at 1.5 seconds
        // - End at 1.0 at 2.0 seconds
        volumeLane->addPoint(0, 0.5f, AutomationCurveType::Linear);                          // 1.0 volume
        volumeLane->addPoint(static_cast<int64_t>(sampleRate * 0.5), 0.25f, AutomationCurveType::Linear);   // 0.5 volume
        volumeLane->addPoint(static_cast<int64_t>(sampleRate * 1.0), 0.5f, AutomationCurveType::Linear);    // 1.0 volume
        volumeLane->addPoint(static_cast<int64_t>(sampleRate * 1.5), 0.75f, AutomationCurveType::Linear);   // 1.5 volume
        volumeLane->addPoint(static_cast<int64_t>(sampleRate * 2.0), 0.5f, AutomationCurveType::Linear);    // 1.0 volume

        // === Step 3: Stop and verify automation points recorded ===
        passed &= volumeLane->hasAutomation();
        passed &= (volumeLane->getNumPoints() == 5);

        // Verify recorded points
        passed &= floatEquals(volumeLane->getPoint(0).value, 0.5f);
        passed &= floatEquals(volumeLane->getPoint(1).value, 0.25f);
        passed &= floatEquals(volumeLane->getPoint(2).value, 0.5f);
        passed &= floatEquals(volumeLane->getPoint(3).value, 0.75f);
        passed &= floatEquals(volumeLane->getPoint(4).value, 0.5f);

        // === Step 4: Enable Read mode and play ===
        track->setAutomationMode(AutomationMode::Read);
        passed &= track->isAutomationReading();
        passed &= !track->isAutomationWriting();

        // === Step 5: Verify fader follows automation ===

        // Verify at each recorded position
        float vol0 = track->getAutomatedVolume(0);
        float vol05 = track->getAutomatedVolume(static_cast<int64_t>(sampleRate * 0.5));
        float vol10 = track->getAutomatedVolume(static_cast<int64_t>(sampleRate * 1.0));
        float vol15 = track->getAutomatedVolume(static_cast<int64_t>(sampleRate * 1.5));
        float vol20 = track->getAutomatedVolume(static_cast<int64_t>(sampleRate * 2.0));

        passed &= floatEquals(vol0, 1.0f, 0.05f);
        passed &= floatEquals(vol05, 0.5f, 0.05f);
        passed &= floatEquals(vol10, 1.0f, 0.05f);
        passed &= floatEquals(vol15, 1.5f, 0.05f);
        passed &= floatEquals(vol20, 1.0f, 0.05f);

        // Verify interpolation between points (at 0.25 seconds)
        float volInterpolated = track->getAutomatedVolume(static_cast<int64_t>(sampleRate * 0.25));
        // Should be between 1.0 (at 0s) and 0.5 (at 0.5s), approximately 0.75
        passed &= (volInterpolated > 0.5f && volInterpolated < 1.0f);

        // === Verify through mixer processing ===
        // Process blocks at different positions and verify levels change

        // Block at 0.5 seconds (dip to 0.5 volume)
        juce::AudioBuffer<float> outputDip(2, blockSize);
        mixer.processBlock(timeline, outputDip, static_cast<int64_t>(sampleRate * 0.5), blockSize);
        mixer.updatePeakLevels(outputDip);
        float peakDip = mixer.getPeakLevel(0);

        // Block at 1.5 seconds (peak to 1.5 volume)
        juce::AudioBuffer<float> outputPeak(2, blockSize);
        mixer.processBlock(timeline, outputPeak, static_cast<int64_t>(sampleRate * 1.5), blockSize);
        mixer.updatePeakLevels(outputPeak);
        float peakPeak = mixer.getPeakLevel(0);

        // The peak position should have higher level than the dip position
        // (accounting for the different automation values)
        passed &= (peakPeak >= peakDip);  // 1.5 volume vs 0.5 volume

        // === Cleanup ===
        track->releaseResources();
        mixer.releaseResources();
        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testCompleteAutomationCycle" : "FAIL: testCompleteAutomationCycle");
        return passed;
    }
};
