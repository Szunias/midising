#pragma once

#include "../Timeline/Timeline.h"
#include "../Audio/AudioTrack.h"
#include <juce_core/juce_core.h>

/**
 * Unit tests for Timeline class.
 */
class TimelineTest
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;
        
        allPassed &= testAddRemoveTracks();
        allPassed &= testBpmSettings();
        allPassed &= testTimeConversion();
        allPassed &= testSoloedTrackDetection();
        
        return allPassed;
    }

private:
    static bool testAddRemoveTracks()
    {
        Timeline timeline;
        bool passed = (timeline.getNumTracks() == 0);
        
        // Add tracks
        auto* track1 = new AudioTrack("Track 1");
        timeline.addTrack(track1);
        passed &= (timeline.getNumTracks() == 1);
        
        auto* track2 = new AudioTrack("Track 2");
        timeline.addTrack(track2);
        passed &= (timeline.getNumTracks() == 2);
        
        // Get track by index
        passed &= (timeline.getTrack(0) == track1);
        passed &= (timeline.getTrack(1) == track2);
        
        // Remove track
        timeline.removeTrack(0);
        passed &= (timeline.getNumTracks() == 1);
        passed &= (timeline.getTrack(0) == track2);
        
        // Clear all tracks
        timeline.clearTracks();
        passed &= (timeline.getNumTracks() == 0);
        
        juce::Logger::writeToLog(passed ? "PASS: testAddRemoveTracks" : "FAIL: testAddRemoveTracks");
        return passed;
    }

    static bool testBpmSettings()
    {
        Timeline timeline;
        
        // Default BPM
        bool passed = (timeline.getBpm() == 120.0);
        
        // Set valid BPM
        timeline.setBpm(140.0);
        passed &= (timeline.getBpm() == 140.0);
        
        // BPM should be clamped to valid range (20-300)
        timeline.setBpm(10.0);
        passed &= (timeline.getBpm() == 20.0);
        
        timeline.setBpm(400.0);
        passed &= (timeline.getBpm() == 300.0);
        
        juce::Logger::writeToLog(passed ? "PASS: testBpmSettings" : "FAIL: testBpmSettings");
        return passed;
    }

    static bool testTimeConversion()
    {
        Timeline timeline;
        timeline.setSampleRate(44100.0);
        timeline.setBpm(120.0);
        
        // 1 beat at 120 BPM = 22050 samples
        int64_t samples = timeline.beatsToSamples(1.0);
        bool passed = (samples == 22050);
        
        // Round trip
        double beats = timeline.samplesToBeats(22050);
        passed &= (std::abs(beats - 1.0) < 0.0001);
        
        juce::Logger::writeToLog(passed ? "PASS: testTimeConversion" : "FAIL: testTimeConversion");
        return passed;
    }

    static bool testSoloedTrackDetection()
    {
        Timeline timeline;
        
        auto* track1 = new AudioTrack("Track 1");
        auto* track2 = new AudioTrack("Track 2");
        timeline.addTrack(track1);
        timeline.addTrack(track2);
        
        // No tracks soloed initially
        bool passed = !timeline.hasAnySoloedTrack();
        
        // Solo one track
        track1->setSoloed(true);
        passed &= timeline.hasAnySoloedTrack();
        
        // Unsolo
        track1->setSoloed(false);
        passed &= !timeline.hasAnySoloedTrack();
        
        timeline.clearTracks();
        
        juce::Logger::writeToLog(passed ? "PASS: testSoloedTrackDetection" : "FAIL: testSoloedTrackDetection");
        return passed;
    }
};
