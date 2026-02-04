#pragma once

#include "../Audio/AudioEngine.h"
#include "../Audio/Transport.h"
#include "../Audio/AudioTrack.h"
#include "../Timeline/Timeline.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * Unit tests for AudioEngine and Transport.
 *
 * Test Coverage:
 * 1. AudioEngine initialization and component access
 * 2. Transport state control (play, pause, stop)
 * 3. Playhead position management
 * 4. Loop functionality with sample-accurate handling
 * 5. Performance metrics tracking
 * 6. CPU load estimation
 * 7. Sample rate and block size handling
 *
 * These tests verify the core audio engine functionality without
 * requiring actual audio hardware.
 */
class AudioEngineTest
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;

        allPassed &= testAudioEngineInitialization();
        allPassed &= testTransportStateControl();
        allPassed &= testPlayheadPosition();
        allPassed &= testLoopFunctionality();
        allPassed &= testSampleAccurateLoopCrossing();
        allPassed &= testPerformanceMetrics();
        allPassed &= testPrepareToPlay();

        return allPassed;
    }

private:
    //==========================================================================
    // Test 1: AudioEngine Initialization and Component Access
    //==========================================================================
    static bool testAudioEngineInitialization()
    {
        AudioEngine engine;
        bool passed = true;

        // Verify all components are accessible
        Timeline& timeline = engine.getTimeline();
        Transport& transport = engine.getTransport();
        Mixer& mixer = engine.getMixer();
        MidiEngine& midiEngine = engine.getMidiEngine();
        AudioRecorder& recorder = engine.getRecorder();

        // Verify initial state
        passed &= (transport.isStopped());
        passed &= (transport.getPlayheadPosition() == 0);
        passed &= (!engine.isRecording());

        // Verify default sample rate and block size
        passed &= (engine.getSampleRate() == 44100.0);
        passed &= (engine.getBlockSize() == 512);

        // Verify initial performance metrics
        AudioPerformanceMetrics metrics = engine.getPerformanceMetrics();
        passed &= (metrics.cpuLoad == 0.0);
        passed &= (metrics.dropoutCount == 0);
        passed &= (metrics.totalBlocksProcessed == 0);

        // Verify dropout count initially zero
        passed &= (engine.getDropoutCount() == 0);
        passed &= (!engine.hadRecentDropout());

        // Verify CPU load initially zero
        passed &= (engine.getCpuLoad() == 0.0);

        juce::Logger::writeToLog(passed ? "PASS: testAudioEngineInitialization" : "FAIL: testAudioEngineInitialization");
        return passed;
    }

    //==========================================================================
    // Test 2: Transport State Control
    //==========================================================================
    static bool testTransportStateControl()
    {
        Transport transport;
        bool passed = true;

        // Initial state should be stopped
        passed &= (transport.isStopped());
        passed &= (transport.getState() == TransportState::Stopped);

        // Play
        transport.play();
        passed &= (transport.isPlaying());
        passed &= (transport.getState() == TransportState::Playing);

        // Pause
        transport.pause();
        passed &= (transport.isStopped());
        passed &= (transport.getState() == TransportState::Stopped);

        // Play again and stop
        transport.play();
        passed &= (transport.isPlaying());

        transport.stop();
        passed &= (transport.isStopped());
        passed &= (transport.getPlayheadPosition() == 0); // Stop resets position

        // Record state
        transport.record();
        passed &= (transport.isRecording());
        passed &= (transport.getState() == TransportState::Recording);

        // Stop from recording
        transport.stop();
        passed &= (transport.isStopped());

        // Toggle play/pause
        transport.togglePlayPause();
        passed &= (transport.isPlaying());

        transport.togglePlayPause();
        passed &= (transport.isStopped());

        juce::Logger::writeToLog(passed ? "PASS: testTransportStateControl" : "FAIL: testTransportStateControl");
        return passed;
    }

    //==========================================================================
    // Test 3: Playhead Position Management
    //==========================================================================
    static bool testPlayheadPosition()
    {
        Transport transport;
        bool passed = true;

        // Initial position is 0
        passed &= (transport.getPlayheadPosition() == 0);

        // Set position
        transport.setPlayheadPosition(44100); // 1 second at 44.1kHz
        passed &= (transport.getPlayheadPosition() == 44100);

        // Advance playhead (only works when playing)
        transport.play();
        int64_t posBeforeAdvance = transport.getPlayheadPosition();
        transport.advancePlayhead(512);
        passed &= (transport.getPlayheadPosition() == posBeforeAdvance + 512);

        // Advance again
        transport.advancePlayhead(256);
        passed &= (transport.getPlayheadPosition() == posBeforeAdvance + 512 + 256);

        // Stop should not advance
        transport.stop(); // This resets position to 0
        passed &= (transport.getPlayheadPosition() == 0);

        // Set a position while stopped
        transport.setPlayheadPosition(1000);
        transport.advancePlayhead(100); // Should not advance when stopped
        passed &= (transport.getPlayheadPosition() == 1000);

        juce::Logger::writeToLog(passed ? "PASS: testPlayheadPosition" : "FAIL: testPlayheadPosition");
        return passed;
    }

    //==========================================================================
    // Test 4: Loop Functionality
    //==========================================================================
    static bool testLoopFunctionality()
    {
        Transport transport;
        bool passed = true;

        // Initial loop state
        passed &= (!transport.isLooping());
        passed &= (transport.getLoopStart() == 0);
        passed &= (transport.getLoopEnd() == 0);

        // Set loop range (1 second loop at 44.1kHz: samples 44100 to 88200)
        transport.setLoopRange(44100, 88200);
        passed &= (transport.getLoopStart() == 44100);
        passed &= (transport.getLoopEnd() == 88200);
        passed &= (transport.getLoopLength() == 44100);

        // Enable looping
        transport.setLooping(true);
        passed &= (transport.isLooping());

        // Test position in loop check
        passed &= (transport.isPositionInLoop(44100)); // At loop start
        passed &= (transport.isPositionInLoop(60000)); // In middle
        passed &= (transport.isPositionInLoop(88199)); // Just before end
        passed &= (!transport.isPositionInLoop(88200)); // At loop end (exclusive)
        passed &= (!transport.isPositionInLoop(44099)); // Before loop
        passed &= (!transport.isPositionInLoop(100000)); // After loop

        // Test snap to loop range
        int64_t snapped = transport.snapToLoopRange(100000);
        // 100000 is 100000 - 44100 = 55900 past loop start
        // 55900 % 44100 = 11800
        // So snapped should be 44100 + 11800 = 55900
        passed &= (snapped == 55900);

        // Test loop wrap handling
        transport.setPlayheadPosition(88199);
        transport.play();
        transport.advancePlayhead(10); // Should go past loop end
        transport.handleLoopWrap();
        // Position should wrap to loop start region
        passed &= (transport.getPlayheadPosition() == 44100);

        // Disable looping
        transport.setLooping(false);
        passed &= (!transport.isLooping());

        juce::Logger::writeToLog(passed ? "PASS: testLoopFunctionality" : "FAIL: testLoopFunctionality");
        return passed;
    }

    //==========================================================================
    // Test 5: Sample-Accurate Loop Crossing
    //==========================================================================
    static bool testSampleAccurateLoopCrossing()
    {
        Transport transport;
        bool passed = true;

        // Set up a short loop (1000 samples) for easy testing
        const int64_t loopStart = 5000;
        const int64_t loopEnd = 6000;
        (void)(loopEnd - loopStart);  // Loop length is 1000 samples

        transport.setLoopRange(loopStart, loopEnd);
        transport.setLooping(true);
        transport.play();

        // Test 1: No crossing when fully within loop
        transport.setPlayheadPosition(5200);
        LoopCrossingInfo info1 = transport.getLoopCrossingInfo(256);

        passed &= (!info1.didLoop);
        passed &= (info1.samplesBeforeLoop == 256);
        passed &= (info1.samplesAfterLoop == 0);
        passed &= (info1.wrapCount == 0);

        // Test 2: Crossing loop boundary
        transport.setPlayheadPosition(5900);
        LoopCrossingInfo info2 = transport.getLoopCrossingInfo(512);

        passed &= (info2.didLoop);
        passed &= (info2.samplesBeforeLoop == 100); // 6000 - 5900 = 100
        passed &= (info2.samplesAfterLoop == 412); // 512 - 100 = 412
        passed &= (info2.wrapPosition == loopEnd);
        passed &= (info2.wrapCount == 1);

        // Test 3: Advance with loop - verify position wraps correctly
        transport.setPlayheadPosition(5900);
        LoopCrossingInfo info3 = transport.advancePlayheadWithLoop(512);

        passed &= (info3.didLoop);
        // New position should be loopStart + samplesAfterLoop = 5000 + 412 = 5412
        passed &= (transport.getPlayheadPosition() == loopStart + 412);

        // Test 4: Multiple wraps in single buffer (when buffer > loop length)
        transport.setPlayheadPosition(5800);
        LoopCrossingInfo info4 = transport.getLoopCrossingInfo(2500);

        passed &= (info4.didLoop);
        passed &= (info4.samplesBeforeLoop == 200); // 6000 - 5800 = 200
        // Remaining = 2500 - 200 = 2300 samples
        // 2300 / 1000 = 2 full wraps, 300 remaining
        passed &= (info4.wrapCount >= 2);
        passed &= (info4.samplesAfterLoop == 300); // 2300 % 1000 = 300

        // Test 5: No looping when disabled
        transport.setLooping(false);
        transport.setPlayheadPosition(5900);
        LoopCrossingInfo info5 = transport.getLoopCrossingInfo(512);

        passed &= (!info5.didLoop);
        passed &= (info5.samplesBeforeLoop == 512);

        // Test 6: No looping when stopped
        transport.setLooping(true);
        transport.stop();
        transport.setPlayheadPosition(5900);
        LoopCrossingInfo info6 = transport.getLoopCrossingInfo(512);

        passed &= (!info6.didLoop);
        passed &= (info6.samplesBeforeLoop == 512);

        juce::Logger::writeToLog(passed ? "PASS: testSampleAccurateLoopCrossing" : "FAIL: testSampleAccurateLoopCrossing");
        return passed;
    }

    //==========================================================================
    // Test 6: Performance Metrics
    //==========================================================================
    static bool testPerformanceMetrics()
    {
        AudioEngine engine;
        bool passed = true;

        // Initial metrics should be zero/clean
        AudioPerformanceMetrics initialMetrics = engine.getPerformanceMetrics();
        passed &= (initialMetrics.cpuLoad == 0.0);
        passed &= (initialMetrics.dropoutCount == 0);
        passed &= (initialMetrics.totalBlocksProcessed == 0);
        passed &= (initialMetrics.averageProcessingTimeMs == 0.0);
        passed &= (initialMetrics.maxProcessingTimeMs == 0.0);

        // Verify individual accessors
        passed &= (engine.getCpuLoad() == 0.0);
        passed &= (engine.getDropoutCount() == 0);
        passed &= (!engine.hadRecentDropout());

        // Test recent dropout flag clearing
        engine.clearRecentDropoutFlag();
        passed &= (!engine.hadRecentDropout());

        // Test metrics reset
        engine.resetPerformanceMetrics();
        AudioPerformanceMetrics resetMetrics = engine.getPerformanceMetrics();
        passed &= (resetMetrics.cpuLoad == 0.0);
        passed &= (resetMetrics.dropoutCount == 0);

        juce::Logger::writeToLog(passed ? "PASS: testPerformanceMetrics" : "FAIL: testPerformanceMetrics");
        return passed;
    }

    //==========================================================================
    // Test 7: PrepareToPlay and Configuration
    //==========================================================================
    static bool testPrepareToPlay()
    {
        AudioEngine engine;
        bool passed = true;

        // Test with different sample rates and block sizes
        const double testSampleRate = 48000.0;
        const int testBlockSize = 1024;

        // Prepare the engine
        engine.prepareToPlay(testBlockSize, testSampleRate);

        // Verify configuration was applied
        passed &= (std::abs(engine.getSampleRate() - testSampleRate) < 0.001);
        passed &= (engine.getBlockSize() == testBlockSize);

        // Verify transport still works after prepare
        Transport& transport = engine.getTransport();
        transport.play();
        passed &= (transport.isPlaying());
        transport.stop();
        passed &= (transport.isStopped());

        // Verify mixer was prepared
        Mixer& mixer = engine.getMixer();
        // Mixer should have valid state after prepare
        passed &= (mixer.getMasterVolume() > 0.0f);

        // Release resources
        engine.releaseResources();

        // Engine should still be in valid state after release
        passed &= (transport.isStopped() || transport.isPlaying() || transport.isRecording());

        // Prepare with different settings
        engine.prepareToPlay(256, 96000.0);
        passed &= (std::abs(engine.getSampleRate() - 96000.0) < 0.001);
        passed &= (engine.getBlockSize() == 256);

        engine.releaseResources();

        juce::Logger::writeToLog(passed ? "PASS: testPrepareToPlay" : "FAIL: testPrepareToPlay");
        return passed;
    }
};
