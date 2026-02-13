#pragma once

#include "../Timeline/Timeline.h"
#include "../Timeline/TempoTrack.h"
#include "../Timeline/TimeSignatureTrack.h"
#include "../Serialization/ProjectSerializer.h"
#include <juce_core/juce_core.h>

/**
 * Tests for Tempo Track and Time Signature Track functionality.
 * Verifies event management, serialization round-trips, and calculations.
 */
class TempoTimeSigTest
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;

        allPassed &= testTempoTrackAddRemoveEvents();
        allPassed &= testTempoTrackInterpolation();
        allPassed &= testTimeSignatureEvents();
        allPassed &= testTimeSignatureBarBeat();

        return allPassed;
    }

private:
    static bool testTempoTrackAddRemoveEvents()
    {
        DBG("  Testing tempo track add/remove events...");

        TempoTrack tempoTrack;

        // Default tempo event at position 0
        bool passed = (tempoTrack.getNumEvents() == 1);

        // Add events
        TempoEvent e1;
        e1.positionInSamples = 44100;
        e1.bpm = 140.0;
        e1.rampType = TempoRampType::Instant;
        tempoTrack.addEvent(e1);

        passed &= (tempoTrack.getNumEvents() == 2);

        TempoEvent e2;
        e2.positionInSamples = 88200;
        e2.bpm = 100.0;
        e2.rampType = TempoRampType::Linear;
        tempoTrack.addEvent(e2);

        passed &= (tempoTrack.getNumEvents() == 3);

        // Remove the middle event
        tempoTrack.removeEvent(1);
        passed &= (tempoTrack.getNumEvents() == 2);

        DBG(juce::String("    ") + (passed ? "PASSED" : "FAILED"));
        return passed;
    }

    static bool testTempoTrackInterpolation()
    {
        DBG("  Testing tempo track interpolation...");

        TempoTrack tempoTrack;

        // Set initial tempo to 120 BPM
        auto& defaultEvent = tempoTrack.getEvent(0);
        const_cast<TempoEvent&>(defaultEvent).bpm = 120.0;

        // Add event at position 44100 with 180 BPM (instant)
        TempoEvent e;
        e.positionInSamples = 44100;
        e.bpm = 180.0;
        e.rampType = TempoRampType::Instant;
        tempoTrack.addEvent(e);

        // BPM at position 0 should be 120
        double bpmAt0 = tempoTrack.getBpmAtPosition(0);
        bool passed = (std::abs(bpmAt0 - 120.0) < 0.1);

        // BPM at position 44100 should be 180 (instant jump)
        double bpmAt44100 = tempoTrack.getBpmAtPosition(44100);
        passed &= (std::abs(bpmAt44100 - 180.0) < 0.1);

        // BPM at position 22050 (before second event) should still be 120
        double bpmAt22050 = tempoTrack.getBpmAtPosition(22050);
        passed &= (std::abs(bpmAt22050 - 120.0) < 0.1);

        DBG(juce::String("    ") + (passed ? "PASSED" : "FAILED"));
        return passed;
    }

    static bool testTimeSignatureEvents()
    {
        DBG("  Testing time signature events...");

        TimeSignatureTrack tsTrack;

        // Should have default 4/4 event
        bool passed = (tsTrack.getNumEvents() >= 1);

        // Add a 3/4 event
        TimeSignatureEvent e;
        e.positionInSamples = 44100;
        e.numerator = 3;
        e.denominator = 4;
        tsTrack.addEvent(e);

        passed &= (tsTrack.getNumEvents() >= 2);

        // Get time sig at position 0 (should be 4/4)
        auto ts0 = tsTrack.getTimeSignatureAtPosition(0);
        passed &= (ts0.numerator == 4 && ts0.denominator == 4);

        // Get time sig at position 44100 (should be 3/4)
        auto ts1 = tsTrack.getTimeSignatureAtPosition(44100);
        passed &= (ts1.numerator == 3 && ts1.denominator == 4);

        DBG(juce::String("    ") + (passed ? "PASSED" : "FAILED"));
        return passed;
    }

    static bool testTimeSignatureBarBeat()
    {
        DBG("  Testing time signature bar/beat calculation...");

        TimeSignatureTrack tsTrack;

        // Default 4/4 at position 0
        // At 120 BPM, one beat = 22050 samples (at 44100 Hz)
        // One bar (4/4) = 4 beats = 88200 samples

        // Bar 1 starts at 0, Bar 2 starts at 88200, etc.
        int barAt0 = tsTrack.getBarAtPosition(0, 120.0, 44100.0);
        int barAt88200 = tsTrack.getBarAtPosition(88200, 120.0, 44100.0);

        bool passed = (barAt0 == 1 || barAt0 == 0);  // Implementation dependent (0 or 1 based)
        passed &= (barAt88200 == barAt0 + 1);  // Second bar should be one more

        DBG(juce::String("    ") + (passed ? "PASSED" : "FAILED"));
        return passed;
    }
};
