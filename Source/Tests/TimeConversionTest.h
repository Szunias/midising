#pragma once

#include "../Utils/TimeConversion.h"
#include <juce_core/juce_core.h>
#include <cmath>

/**
 * Unit tests for TimeConversion utilities.
 */
class TimeConversionTest
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;
        
        allPassed &= testBeatsToSamples();
        allPassed &= testSamplesToBeats();
        allPassed &= testBeatsToSeconds();
        allPassed &= testSecondsToBeats();
        allPassed &= testRoundTrip();
        
        return allPassed;
    }

private:
    static constexpr double EPSILON = 0.0001;

    static bool approximately(double a, double b)
    {
        return std::abs(a - b) < EPSILON;
    }

    static bool testBeatsToSamples()
    {
        // At 120 BPM, 44100 Hz: 1 beat = 0.5 seconds = 22050 samples
        int64_t samples = TimeConversion::beatsToSamples(1.0, 120.0, 44100.0);
        bool passed = (samples == 22050);
        
        // 4 beats at 120 BPM = 2 seconds = 88200 samples
        samples = TimeConversion::beatsToSamples(4.0, 120.0, 44100.0);
        passed &= (samples == 88200);
        
        // At 60 BPM, 1 beat = 1 second = 44100 samples
        samples = TimeConversion::beatsToSamples(1.0, 60.0, 44100.0);
        passed &= (samples == 44100);
        
        juce::Logger::writeToLog(passed ? "PASS: testBeatsToSamples" : "FAIL: testBeatsToSamples");
        return passed;
    }

    static bool testSamplesToBeats()
    {
        // 22050 samples at 120 BPM, 44100 Hz = 1 beat
        double beats = TimeConversion::samplesToBeats(22050, 120.0, 44100.0);
        bool passed = approximately(beats, 1.0);
        
        // 88200 samples = 4 beats
        beats = TimeConversion::samplesToBeats(88200, 120.0, 44100.0);
        passed &= approximately(beats, 4.0);
        
        juce::Logger::writeToLog(passed ? "PASS: testSamplesToBeats" : "FAIL: testSamplesToBeats");
        return passed;
    }

    static bool testBeatsToSeconds()
    {
        // At 120 BPM, 1 beat = 0.5 seconds
        double seconds = TimeConversion::beatsToSeconds(1.0, 120.0);
        bool passed = approximately(seconds, 0.5);
        
        // At 60 BPM, 1 beat = 1 second
        seconds = TimeConversion::beatsToSeconds(1.0, 60.0);
        passed &= approximately(seconds, 1.0);
        
        // 4 beats at 120 BPM = 2 seconds
        seconds = TimeConversion::beatsToSeconds(4.0, 120.0);
        passed &= approximately(seconds, 2.0);
        
        juce::Logger::writeToLog(passed ? "PASS: testBeatsToSeconds" : "FAIL: testBeatsToSeconds");
        return passed;
    }

    static bool testSecondsToBeats()
    {
        // At 120 BPM, 0.5 seconds = 1 beat
        double beats = TimeConversion::secondsToBeats(0.5, 120.0);
        bool passed = approximately(beats, 1.0);
        
        // At 60 BPM, 1 second = 1 beat
        beats = TimeConversion::secondsToBeats(1.0, 60.0);
        passed &= approximately(beats, 1.0);
        
        juce::Logger::writeToLog(passed ? "PASS: testSecondsToBeats" : "FAIL: testSecondsToBeats");
        return passed;
    }

    static bool testRoundTrip()
    {
        // Convert beats -> samples -> beats should give same result
        double originalBeats = 3.5;
        int64_t samples = TimeConversion::beatsToSamples(originalBeats, 120.0, 44100.0);
        double resultBeats = TimeConversion::samplesToBeats(samples, 120.0, 44100.0);
        bool passed = approximately(originalBeats, resultBeats);
        
        // Convert seconds -> beats -> seconds
        double originalSeconds = 2.5;
        double beats = TimeConversion::secondsToBeats(originalSeconds, 120.0);
        double resultSeconds = TimeConversion::beatsToSeconds(beats, 120.0);
        passed &= approximately(originalSeconds, resultSeconds);
        
        juce::Logger::writeToLog(passed ? "PASS: testRoundTrip" : "FAIL: testRoundTrip");
        return passed;
    }
};
