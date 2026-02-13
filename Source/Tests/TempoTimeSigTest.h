#pragma once

#include "../Timeline/Timeline.h"
#include "../Timeline/TempoTrack.h"
#include "../Timeline/TimeSignatureTrack.h"
#include <juce_core/juce_core.h>

/**
 * Tests for Tempo Track and Time Signature Track functionality.
 * Verifies event management, interpolation, and time signature lookups.
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
        allPassed &= testTimeSignatureQuarterNotesPerBar();

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
        TempoEvent e1(44100, 140.0, TempoRampType::Instant);
        tempoTrack.addEvent(e1);

        passed &= (tempoTrack.getNumEvents() == 2);

        TempoEvent e2(88200, 100.0, TempoRampType::Linear);
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

        // Set initial tempo to 120 BPM (default is already 120)

        // Add event at position 44100 with 180 BPM (instant)
        TempoEvent e(44100, 180.0, TempoRampType::Instant);
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
        TimeSignatureEvent e(44100, 3, 4);
        tsTrack.addEvent(e);

        passed &= (tsTrack.getNumEvents() >= 2);

        // Get time sig at position 0 (should be 4/4)
        int num0, den0;
        tsTrack.getTimeSignatureAtPosition(0, num0, den0);
        passed &= (num0 == 4 && den0 == 4);

        // Get time sig at position 44100 (should be 3/4)
        int num1, den1;
        tsTrack.getTimeSignatureAtPosition(44100, num1, den1);
        passed &= (num1 == 3 && den1 == 4);

        DBG(juce::String("    ") + (passed ? "PASSED" : "FAILED"));
        return passed;
    }

    static bool testTimeSignatureQuarterNotesPerBar()
    {
        DBG("  Testing time signature quarter notes per bar...");

        TimeSignatureTrack tsTrack;

        // Default 4/4: quarterNotesPerBar should be 4.0
        double qnPerBar = tsTrack.getQuarterNotesPerBarAtPosition(0);
        bool passed = (std::abs(qnPerBar - 4.0) < 0.01);

        // Add 3/4 at position 44100
        TimeSignatureEvent e(44100, 3, 4);
        tsTrack.addEvent(e);

        // 3/4: quarterNotesPerBar should be 3.0
        double qnPerBar34 = tsTrack.getQuarterNotesPerBarAtPosition(44100);
        passed &= (std::abs(qnPerBar34 - 3.0) < 0.01);

        // Add 6/8 at position 88200
        TimeSignatureEvent e2(88200, 6, 8);
        tsTrack.addEvent(e2);

        // 6/8: quarterNotesPerBar should be 3.0 (6 eighth notes = 3 quarter notes)
        double qnPerBar68 = tsTrack.getQuarterNotesPerBarAtPosition(88200);
        passed &= (std::abs(qnPerBar68 - 3.0) < 0.01);

        DBG(juce::String("    ") + (passed ? "PASSED" : "FAILED"));
        return passed;
    }
};
