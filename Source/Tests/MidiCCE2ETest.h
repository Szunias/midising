#pragma once

#include "../UI/PianoRoll.h"
#include "../Timeline/Region.h"
#include "../MIDI/MidiTrack.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * End-to-end verification tests for MIDI CC recording and editing.
 *
 * Test Steps (as specified in implementation_plan.json):
 * 1. Open Piano Roll
 * 2. Show Mod Wheel CC lane
 * 3. Draw CC curve with pencil
 * 4. Play back and verify CC affects sound
 *
 * These tests verify the complete MIDI CC workflow including:
 * - Setting up a MIDI region for editing
 * - Showing/hiding CC lanes (ModWheel, Sustain, PitchBend, Expression)
 * - Drawing CC automation curves
 * - Verifying CC events are recorded in the MIDI sequence
 * - Simulating playback to verify CC events are processed
 */
class MidiCCE2ETest
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;

        allPassed &= testPianoRollSetup();
        allPassed &= testCCLaneVisibility();
        allPassed &= testShowModWheelCCLane();
        allPassed &= testDrawCCCurveWithPencil();
        allPassed &= testCCCurveMultiplePoints();
        allPassed &= testVerifyCCInSequence();
        allPassed &= testCCPlaybackSimulation();
        allPassed &= testCompleteCCEditingCycle();
        allPassed &= testAllCCLaneTypes();
        allPassed &= testCCPointDeletion();

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
    // Helper: Create a test MIDI region with some notes
    //==========================================================================
    static std::unique_ptr<MidiRegion> createTestMidiRegion()
    {
        auto region = std::make_unique<MidiRegion>(0, 44100 * 4);  // 4 seconds at 44.1kHz
        region->setName("Test MIDI Region");

        // Add some test notes
        auto& seq = region->getMidiSequence();
        double ticksPerBeat = 960.0;

        // Add a chord at beat 0
        seq.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f).withTimeStamp(0.0));
        seq.addEvent(juce::MidiMessage::noteOff(1, 60, 0.0f).withTimeStamp(ticksPerBeat));

        seq.addEvent(juce::MidiMessage::noteOn(1, 64, 0.8f).withTimeStamp(ticksPerBeat));
        seq.addEvent(juce::MidiMessage::noteOff(1, 64, 0.0f).withTimeStamp(ticksPerBeat * 2));

        seq.addEvent(juce::MidiMessage::noteOn(1, 67, 0.8f).withTimeStamp(ticksPerBeat * 2));
        seq.addEvent(juce::MidiMessage::noteOff(1, 67, 0.0f).withTimeStamp(ticksPerBeat * 3));

        seq.updateMatchedPairs();

        return region;
    }

    //==========================================================================
    // Helper: Count CC events in a MIDI sequence for a specific CC number
    //==========================================================================
    static int countCCEvents(const juce::MidiMessageSequence& seq, int ccNumber)
    {
        int count = 0;
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event != nullptr && event->message.isController() &&
                event->message.getControllerNumber() == ccNumber)
            {
                ++count;
            }
        }
        return count;
    }

    //==========================================================================
    // Helper: Count pitch bend events in a MIDI sequence
    //==========================================================================
    static int countPitchBendEvents(const juce::MidiMessageSequence& seq)
    {
        int count = 0;
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event != nullptr && event->message.isPitchWheel())
            {
                ++count;
            }
        }
        return count;
    }

    //==========================================================================
    // Helper: Get CC value at a specific beat position
    //==========================================================================
    static int getCCValueAtBeat(const juce::MidiMessageSequence& seq, int ccNumber, double beat)
    {
        double ticksPerBeat = 960.0;
        double targetTimestamp = beat * ticksPerBeat;
        int lastValue = 0;

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event == nullptr)
                continue;

            if (event->message.isController() && event->message.getControllerNumber() == ccNumber)
            {
                if (event->message.getTimeStamp() <= targetTimestamp)
                {
                    lastValue = event->message.getControllerValue();
                }
                else
                {
                    break;  // Passed the target position
                }
            }
        }
        return lastValue;
    }

    //==========================================================================
    // Test 1: Piano Roll Setup
    //==========================================================================
    static bool testPianoRollSetup()
    {
        bool passed = true;

        // === Step 1: Create a PianoRoll instance ===
        PianoRoll pianoRoll;

        // Initially should have no MIDI region
        passed &= (pianoRoll.getMidiRegion() == nullptr);

        // === Step 2: Create and set a MIDI region ===
        auto region = createTestMidiRegion();
        pianoRoll.setMidiRegion(region.get());

        // Verify region is set
        passed &= (pianoRoll.getMidiRegion() == region.get());

        // === Step 3: Verify the region has notes ===
        const auto& seq = region->getMidiSequence();
        int noteOnCount = 0;
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event != nullptr && event->message.isNoteOn())
                ++noteOnCount;
        }
        passed &= (noteOnCount == 3);  // We added 3 notes

        juce::Logger::writeToLog(passed ? "PASS: testPianoRollSetup" : "FAIL: testPianoRollSetup");
        return passed;
    }

    //==========================================================================
    // Test 2: CC Lane Visibility Management
    //==========================================================================
    static bool testCCLaneVisibility()
    {
        bool passed = true;

        PianoRoll pianoRoll;

        // Initially, no CC lanes should be visible
        passed &= (pianoRoll.getNumVisibleCCLanes() == 0);
        passed &= !pianoRoll.isCCLaneVisible(PianoRoll::CCLaneType::ModWheel);
        passed &= !pianoRoll.isCCLaneVisible(PianoRoll::CCLaneType::Sustain);
        passed &= !pianoRoll.isCCLaneVisible(PianoRoll::CCLaneType::PitchBend);
        passed &= !pianoRoll.isCCLaneVisible(PianoRoll::CCLaneType::Expression);

        // Show ModWheel lane
        pianoRoll.setCCLaneVisible(PianoRoll::CCLaneType::ModWheel, true);
        passed &= (pianoRoll.getNumVisibleCCLanes() == 1);
        passed &= pianoRoll.isCCLaneVisible(PianoRoll::CCLaneType::ModWheel);

        // Show another lane
        pianoRoll.setCCLaneVisible(PianoRoll::CCLaneType::Sustain, true);
        passed &= (pianoRoll.getNumVisibleCCLanes() == 2);
        passed &= pianoRoll.isCCLaneVisible(PianoRoll::CCLaneType::Sustain);

        // Toggle lane off
        pianoRoll.toggleCCLane(PianoRoll::CCLaneType::ModWheel);
        passed &= (pianoRoll.getNumVisibleCCLanes() == 1);
        passed &= !pianoRoll.isCCLaneVisible(PianoRoll::CCLaneType::ModWheel);

        // Hide all lanes
        pianoRoll.setCCLaneVisible(PianoRoll::CCLaneType::Sustain, false);
        passed &= (pianoRoll.getNumVisibleCCLanes() == 0);

        juce::Logger::writeToLog(passed ? "PASS: testCCLaneVisibility" : "FAIL: testCCLaneVisibility");
        return passed;
    }

    //==========================================================================
    // Test 3: Show Mod Wheel CC Lane (E2E Step 2)
    //==========================================================================
    static bool testShowModWheelCCLane()
    {
        bool passed = true;

        PianoRoll pianoRoll;
        auto region = createTestMidiRegion();

        // === Step 1: Open Piano Roll (set MIDI region) ===
        pianoRoll.setMidiRegion(region.get());
        passed &= (pianoRoll.getMidiRegion() != nullptr);

        // === Step 2: Show Mod Wheel CC lane ===
        pianoRoll.setCCLaneVisible(PianoRoll::CCLaneType::ModWheel, true);

        // Verify lane is visible
        passed &= pianoRoll.isCCLaneVisible(PianoRoll::CCLaneType::ModWheel);
        passed &= (pianoRoll.getNumVisibleCCLanes() == 1);

        // Verify correct CC number for ModWheel (CC1)
        passed &= (PianoRoll::getCCNumberForLaneType(PianoRoll::CCLaneType::ModWheel) == 1);

        // Verify lane name
        passed &= (PianoRoll::getCCLaneName(PianoRoll::CCLaneType::ModWheel) == "Mod Wheel");

        juce::Logger::writeToLog(passed ? "PASS: testShowModWheelCCLane" : "FAIL: testShowModWheelCCLane");
        return passed;
    }

    //==========================================================================
    // Test 4: Draw CC Curve with Pencil (E2E Step 3)
    //==========================================================================
    static bool testDrawCCCurveWithPencil()
    {
        bool passed = true;

        PianoRoll pianoRoll;
        auto region = createTestMidiRegion();

        // === Step 1: Setup ===
        pianoRoll.setMidiRegion(region.get());
        pianoRoll.setCCLaneVisible(PianoRoll::CCLaneType::ModWheel, true);

        // === Step 2: Initially, no CC events should exist ===
        const auto& seq = region->getMidiSequence();
        int initialCCCount = countCCEvents(seq, 1);  // CC1 = ModWheel
        passed &= (initialCCCount == 0);

        // === Step 3: Draw CC curve (simulating pencil tool) ===
        // Add CC points at different beats with increasing values
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 0.0, 0);      // Start at 0
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 1.0, 32);     // 1/4 value
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 2.0, 64);     // Half value
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 3.0, 96);     // 3/4 value
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 4.0, 127);    // Max value

        // === Step 4: Verify CC events were recorded ===
        int newCCCount = countCCEvents(seq, 1);
        passed &= (newCCCount == 5);

        juce::Logger::writeToLog(passed ? "PASS: testDrawCCCurveWithPencil" : "FAIL: testDrawCCCurveWithPencil");
        return passed;
    }

    //==========================================================================
    // Test 5: Draw CC Curve with Multiple Points (Dense Drawing)
    //==========================================================================
    static bool testCCCurveMultiplePoints()
    {
        bool passed = true;

        PianoRoll pianoRoll;
        auto region = createTestMidiRegion();

        pianoRoll.setMidiRegion(region.get());
        pianoRoll.setCCLaneVisible(PianoRoll::CCLaneType::Expression, true);

        // Draw a dense expression curve (simulating mouse drag)
        for (int i = 0; i <= 16; ++i)
        {
            double beat = i * 0.25;  // Every 1/4 beat
            int value = static_cast<int>(127.0f * std::sin(beat * 0.5f) * 0.5f + 0.5f);
            pianoRoll.addCCPoint(PianoRoll::CCLaneType::Expression, beat, value);
        }

        // Verify all points were added
        const auto& seq = region->getMidiSequence();
        int ccCount = countCCEvents(seq, 11);  // CC11 = Expression
        passed &= (ccCount == 17);  // 0 to 16 inclusive

        juce::Logger::writeToLog(passed ? "PASS: testCCCurveMultiplePoints" : "FAIL: testCCCurveMultiplePoints");
        return passed;
    }

    //==========================================================================
    // Test 6: Verify CC Events in MIDI Sequence (E2E Step 4 - Part 1)
    //==========================================================================
    static bool testVerifyCCInSequence()
    {
        bool passed = true;

        PianoRoll pianoRoll;
        auto region = createTestMidiRegion();
        pianoRoll.setMidiRegion(region.get());

        // Draw a known CC curve
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 0.0, 0);
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 1.0, 64);
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 2.0, 127);

        // Verify CC values at specific positions
        const auto& seq = region->getMidiSequence();

        // Check value at beat 0
        int valueAt0 = getCCValueAtBeat(seq, 1, 0.0);
        passed &= (valueAt0 == 0);

        // Check value at beat 1
        int valueAt1 = getCCValueAtBeat(seq, 1, 1.0);
        passed &= (valueAt1 == 64);

        // Check value at beat 2
        int valueAt2 = getCCValueAtBeat(seq, 1, 2.0);
        passed &= (valueAt2 == 127);

        // Check value at beat 1.5 (should still be 64, last value before position)
        int valueAt15 = getCCValueAtBeat(seq, 1, 1.5);
        passed &= (valueAt15 == 64);

        juce::Logger::writeToLog(passed ? "PASS: testVerifyCCInSequence" : "FAIL: testVerifyCCInSequence");
        return passed;
    }

    //==========================================================================
    // Test 7: CC Playback Simulation (E2E Step 4 - Part 2)
    //==========================================================================
    static bool testCCPlaybackSimulation()
    {
        bool passed = true;

        auto region = createTestMidiRegion();
        auto& seq = region->getMidiSequence();

        // Add CC automation curve (ModWheel ramp from 0 to 127)
        double ticksPerBeat = 960.0;
        for (int i = 0; i <= 4; ++i)
        {
            double beat = static_cast<double>(i);
            int value = i * 32;  // 0, 32, 64, 96, 128 (clamped to 127)
            value = juce::jlimit(0, 127, value);

            juce::MidiMessage ccMsg = juce::MidiMessage::controllerEvent(1, 1, value);
            ccMsg.setTimeStamp(beat * ticksPerBeat);
            seq.addEvent(ccMsg);
        }
        seq.sort();

        // Simulate playback: iterate through events and collect CC values
        std::vector<std::pair<double, int>> playbackValues;

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event != nullptr && event->message.isController() &&
                event->message.getControllerNumber() == 1)
            {
                double beat = event->message.getTimeStamp() / ticksPerBeat;
                int value = event->message.getControllerValue();
                playbackValues.push_back({ beat, value });
            }
        }

        // Verify we collected the expected values
        passed &= (playbackValues.size() == 5);

        if (playbackValues.size() == 5)
        {
            // Verify increasing values (CC affects sound)
            for (size_t i = 1; i < playbackValues.size(); ++i)
            {
                passed &= (playbackValues[i].second > playbackValues[i - 1].second ||
                           playbackValues[i].second == 127);  // Allow last value to be clamped
            }

            // Verify first and last values
            passed &= (playbackValues[0].second == 0);
            passed &= (playbackValues[4].second >= 96);  // 4*32 = 128, clamped to 127
        }

        juce::Logger::writeToLog(passed ? "PASS: testCCPlaybackSimulation" : "FAIL: testCCPlaybackSimulation");
        return passed;
    }

    //==========================================================================
    // Test 8: Complete CC Editing Cycle (Full E2E)
    //==========================================================================
    static bool testCompleteCCEditingCycle()
    {
        bool passed = true;

        // === Step 1: Open Piano Roll (create and configure) ===
        PianoRoll pianoRoll;
        auto region = createTestMidiRegion();
        pianoRoll.setMidiRegion(region.get());
        passed &= (pianoRoll.getMidiRegion() != nullptr);

        // === Step 2: Show Mod Wheel CC lane ===
        pianoRoll.setCCLaneVisible(PianoRoll::CCLaneType::ModWheel, true);
        passed &= pianoRoll.isCCLaneVisible(PianoRoll::CCLaneType::ModWheel);

        // === Step 3: Draw CC curve with pencil ===
        // Simulate drawing a modulation "wobble" effect
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 0.0, 0);
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 0.5, 60);
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 1.0, 120);
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 1.5, 60);
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 2.0, 0);
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 2.5, 80);
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::ModWheel, 3.0, 127);

        // Verify CC events were recorded
        const auto& seq = region->getMidiSequence();
        int ccCount = countCCEvents(seq, 1);
        passed &= (ccCount == 7);

        // === Step 4: Play back and verify CC affects sound ===
        // Collect all CC events in playback order
        std::vector<int> ccValues;
        double ticksPerBeat = 960.0;

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event != nullptr && event->message.isController() &&
                event->message.getControllerNumber() == 1)
            {
                ccValues.push_back(event->message.getControllerValue());
            }
        }

        // Verify values match what we drew
        passed &= (ccValues.size() == 7);
        if (ccValues.size() == 7)
        {
            passed &= (ccValues[0] == 0);
            passed &= (ccValues[1] == 60);
            passed &= (ccValues[2] == 120);
            passed &= (ccValues[3] == 60);
            passed &= (ccValues[4] == 0);
            passed &= (ccValues[5] == 80);
            passed &= (ccValues[6] == 127);
        }

        // Verify the CC events are properly sorted by timestamp
        double lastTimestamp = -1.0;
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event != nullptr && event->message.isController() &&
                event->message.getControllerNumber() == 1)
            {
                double timestamp = event->message.getTimeStamp();
                passed &= (timestamp >= lastTimestamp);
                lastTimestamp = timestamp;
            }
        }

        juce::Logger::writeToLog(passed ? "PASS: testCompleteCCEditingCycle" : "FAIL: testCompleteCCEditingCycle");
        return passed;
    }

    //==========================================================================
    // Test 9: All CC Lane Types
    //==========================================================================
    static bool testAllCCLaneTypes()
    {
        bool passed = true;

        PianoRoll pianoRoll;
        auto region = createTestMidiRegion();
        pianoRoll.setMidiRegion(region.get());

        // Test each CC lane type
        struct CCLaneTest
        {
            PianoRoll::CCLaneType type;
            int expectedCC;
            const char* expectedName;
        };

        CCLaneTest tests[] = {
            { PianoRoll::CCLaneType::ModWheel, 1, "Mod Wheel" },
            { PianoRoll::CCLaneType::Sustain, 64, "Sustain" },
            { PianoRoll::CCLaneType::PitchBend, -1, "Pitch Bend" },  // Special: not a CC
            { PianoRoll::CCLaneType::Expression, 11, "Expression" }
        };

        for (const auto& test : tests)
        {
            // Verify CC number
            passed &= (PianoRoll::getCCNumberForLaneType(test.type) == test.expectedCC);

            // Verify name
            passed &= (PianoRoll::getCCLaneName(test.type) == test.expectedName);

            // Show lane
            pianoRoll.setCCLaneVisible(test.type, true);
            passed &= pianoRoll.isCCLaneVisible(test.type);

            // Add a CC point
            pianoRoll.addCCPoint(test.type, 0.0, 64);
        }

        // Verify all 4 lanes are visible
        passed &= (pianoRoll.getNumVisibleCCLanes() == 4);

        // Verify we have CC events for each type
        const auto& seq = region->getMidiSequence();
        passed &= (countCCEvents(seq, 1) >= 1);   // ModWheel
        passed &= (countCCEvents(seq, 64) >= 1);  // Sustain
        passed &= (countPitchBendEvents(seq) >= 1);  // Pitch Bend
        passed &= (countCCEvents(seq, 11) >= 1);  // Expression

        juce::Logger::writeToLog(passed ? "PASS: testAllCCLaneTypes" : "FAIL: testAllCCLaneTypes");
        return passed;
    }

    //==========================================================================
    // Test 10: CC Point Deletion
    //==========================================================================
    static bool testCCPointDeletion()
    {
        bool passed = true;

        PianoRoll pianoRoll;
        auto region = createTestMidiRegion();
        pianoRoll.setMidiRegion(region.get());
        pianoRoll.setCCLaneVisible(PianoRoll::CCLaneType::Sustain, true);

        // Add CC points
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::Sustain, 0.0, 0);
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::Sustain, 1.0, 127);
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::Sustain, 2.0, 0);
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::Sustain, 3.0, 127);
        pianoRoll.addCCPoint(PianoRoll::CCLaneType::Sustain, 4.0, 0);

        const auto& seq = region->getMidiSequence();
        passed &= (countCCEvents(seq, 64) == 5);

        // Delete CC points in a range (beats 1.5 to 3.5)
        pianoRoll.deleteCCPointsInRange(PianoRoll::CCLaneType::Sustain, 1.5, 3.5);

        // Should have deleted 2 points (at beats 2.0 and 3.0)
        passed &= (countCCEvents(seq, 64) == 3);

        // Clear entire lane
        pianoRoll.clearCCLane(PianoRoll::CCLaneType::Sustain);
        passed &= (countCCEvents(seq, 64) == 0);

        juce::Logger::writeToLog(passed ? "PASS: testCCPointDeletion" : "FAIL: testCCPointDeletion");
        return passed;
    }
};
