#pragma once

#include "../Timeline/Region.h"
#include "../Actions/MidiActions.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * Unit tests for MIDI quantization and other MIDI operations.
 *
 * Test Coverage:
 * 1. MIDI region creation and note addition
 * 2. Quantization to various grid sizes (1/4, 1/8, 1/16 notes)
 * 3. Quantization strength parameter (0% to 100%)
 * 4. Transpose notes up/down
 * 5. Humanize notes (timing and velocity variation)
 * 6. Split notes at specific positions
 * 7. Delete single and multiple notes
 * 8. Undo/redo for all MIDI operations
 */
class MidiTests
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;

        allPassed &= testMidiRegionCreation();
        allPassed &= testAddNote();
        allPassed &= testQuantizeToQuarterNote();
        allPassed &= testQuantizeToEighthNote();
        allPassed &= testQuantizeToSixteenthNote();
        allPassed &= testQuantizeStrength();
        allPassed &= testTransposeNotes();
        allPassed &= testHumanizeNotes();
        allPassed &= testSplitNote();
        allPassed &= testDeleteNote();
        allPassed &= testDeleteMultipleNotes();
        allPassed &= testQuantizeUndo();

        return allPassed;
    }

private:
    // Standard ticks per beat (PPQ)
    static constexpr double TICKS_PER_BEAT = 960.0;

    //==========================================================================
    // Helper: Create a test MIDI region with sample notes
    //==========================================================================
    static std::unique_ptr<MidiRegion> createTestMidiRegion()
    {
        auto region = std::make_unique<MidiRegion>(0, 44100 * 4); // 4 seconds at 44.1kHz
        region->setName("Test MIDI Region");
        return region;
    }

    //==========================================================================
    // Helper: Add a note to a MIDI sequence
    //==========================================================================
    static void addNoteToSequence(juce::MidiMessageSequence& seq, int noteNumber,
                                   float velocity, int channel,
                                   double startTicks, double endTicks)
    {
        juce::MidiMessage noteOn = juce::MidiMessage::noteOn(channel, noteNumber, velocity);
        noteOn.setTimeStamp(startTicks);
        seq.addEvent(noteOn);

        juce::MidiMessage noteOff = juce::MidiMessage::noteOff(channel, noteNumber, 0.0f);
        noteOff.setTimeStamp(endTicks);
        seq.addEvent(noteOff);

        seq.sort();
        seq.updateMatchedPairs();
    }

    //==========================================================================
    // Helper: Count note-on events in a sequence
    //==========================================================================
    static int countNoteOnEvents(const juce::MidiMessageSequence& seq)
    {
        int count = 0;
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            if (seq.getEventPointer(i)->message.isNoteOn())
                ++count;
        }
        return count;
    }

    //==========================================================================
    // Helper: Find note-on event index by note number and approximate start time
    //==========================================================================
    static int findNoteOnIndex(const juce::MidiMessageSequence& seq, int noteNumber,
                                double startTicks, double tolerance = 2.0)
    {
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event->message.isNoteOn() &&
                event->message.getNoteNumber() == noteNumber &&
                std::abs(event->message.getTimeStamp() - startTicks) < tolerance)
            {
                return i;
            }
        }
        return -1;
    }

    //==========================================================================
    // Helper: Get note start time by index
    //==========================================================================
    static double getNoteStartTime(const juce::MidiMessageSequence& seq, int index)
    {
        if (index >= 0 && index < seq.getNumEvents())
        {
            auto* event = seq.getEventPointer(index);
            if (event->message.isNoteOn())
                return event->message.getTimeStamp();
        }
        return -1.0;
    }

    //==========================================================================
    // Test 1: MIDI Region Creation
    //==========================================================================
    static bool testMidiRegionCreation()
    {
        bool passed = true;

        auto region = createTestMidiRegion();

        // Verify region was created
        passed &= (region != nullptr);
        passed &= (region->getType() == RegionType::MIDI);
        passed &= (region->getName() == "Test MIDI Region");
        passed &= (region->getStartPosition() == 0);
        passed &= (region->getLength() == 44100 * 4);

        // Verify empty sequence
        passed &= (region->getMidiSequence().getNumEvents() == 0);

        juce::Logger::writeToLog(passed ? "PASS: testMidiRegionCreation" : "FAIL: testMidiRegionCreation");
        return passed;
    }

    //==========================================================================
    // Test 2: Add Note Action
    //==========================================================================
    static bool testAddNote()
    {
        bool passed = true;

        auto region = createTestMidiRegion();
        juce::UndoManager undoManager;

        // Add a note using AddNoteAction
        auto* action = new AddNoteAction(*region, 60, 0.8f, 1, 0.0, TICKS_PER_BEAT);
        undoManager.perform(action);

        // Verify note was added
        auto& seq = region->getMidiSequence();
        passed &= (countNoteOnEvents(seq) == 1);

        // Find the note
        int noteIndex = findNoteOnIndex(seq, 60, 0.0);
        passed &= (noteIndex >= 0);

        if (noteIndex >= 0)
        {
            auto* event = seq.getEventPointer(noteIndex);
            passed &= (event->message.getNoteNumber() == 60);
            passed &= (std::abs(event->message.getFloatVelocity() - 0.8f) < 0.01f);
            passed &= (event->message.getChannel() == 1);
        }

        // Test undo
        undoManager.undo();
        passed &= (countNoteOnEvents(seq) == 0);

        // Test redo
        undoManager.redo();
        passed &= (countNoteOnEvents(seq) == 1);

        juce::Logger::writeToLog(passed ? "PASS: testAddNote" : "FAIL: testAddNote");
        return passed;
    }

    //==========================================================================
    // Test 3: Quantize to Quarter Note Grid
    //==========================================================================
    static bool testQuantizeToQuarterNote()
    {
        bool passed = true;

        auto region = createTestMidiRegion();
        auto& seq = region->getMidiSequence();

        // Add a note slightly off the beat (at tick 100 instead of 0)
        double offBeatPosition = 100.0;
        addNoteToSequence(seq, 60, 0.8f, 1, offBeatPosition, offBeatPosition + TICKS_PER_BEAT);

        // Verify initial position
        int noteIndex = findNoteOnIndex(seq, 60, offBeatPosition);
        passed &= (noteIndex >= 0);

        // Quantize to quarter notes (1 beat = 960 ticks)
        std::vector<int> noteIndices = { noteIndex };
        QuantizeNotesAction action(*region, noteIndices, 1.0, 1.0f, TICKS_PER_BEAT);
        action.perform();

        // Note should now be at tick 0 (nearest quarter note)
        double quantizedStart = getNoteStartTime(seq, findNoteOnIndex(seq, 60, 0.0, 10.0));
        passed &= (std::abs(quantizedStart - 0.0) < 1.0);

        juce::Logger::writeToLog(passed ? "PASS: testQuantizeToQuarterNote" : "FAIL: testQuantizeToQuarterNote");
        return passed;
    }

    //==========================================================================
    // Test 4: Quantize to Eighth Note Grid
    //==========================================================================
    static bool testQuantizeToEighthNote()
    {
        bool passed = true;

        auto region = createTestMidiRegion();
        auto& seq = region->getMidiSequence();

        // Add a note at tick 400 (should quantize to 480 = half beat)
        double offPosition = 400.0;
        double gridSize = 0.5; // Eighth note = 0.5 beats
        double expectedQuantized = 480.0; // Nearest eighth note grid point

        addNoteToSequence(seq, 64, 0.7f, 1, offPosition, offPosition + 240.0);

        int noteIndex = findNoteOnIndex(seq, 64, offPosition);
        passed &= (noteIndex >= 0);

        // Quantize to eighth notes
        std::vector<int> noteIndices = { noteIndex };
        QuantizeNotesAction action(*region, noteIndices, gridSize, 1.0f, TICKS_PER_BEAT);
        action.perform();

        // Note should now be at the nearest eighth note
        double quantizedStart = getNoteStartTime(seq, findNoteOnIndex(seq, 64, expectedQuantized, 10.0));
        passed &= (std::abs(quantizedStart - expectedQuantized) < 1.0);

        juce::Logger::writeToLog(passed ? "PASS: testQuantizeToEighthNote" : "FAIL: testQuantizeToEighthNote");
        return passed;
    }

    //==========================================================================
    // Test 5: Quantize to Sixteenth Note Grid
    //==========================================================================
    static bool testQuantizeToSixteenthNote()
    {
        bool passed = true;

        auto region = createTestMidiRegion();
        auto& seq = region->getMidiSequence();

        // Add a note at tick 200 (should quantize to 240 = quarter of a beat)
        double offPosition = 200.0;
        double gridSize = 0.25; // Sixteenth note = 0.25 beats
        double expectedQuantized = 240.0; // Nearest sixteenth note grid point

        addNoteToSequence(seq, 67, 0.9f, 1, offPosition, offPosition + 120.0);

        int noteIndex = findNoteOnIndex(seq, 67, offPosition);
        passed &= (noteIndex >= 0);

        // Quantize to sixteenth notes
        std::vector<int> noteIndices = { noteIndex };
        QuantizeNotesAction action(*region, noteIndices, gridSize, 1.0f, TICKS_PER_BEAT);
        action.perform();

        // Note should now be at the nearest sixteenth note
        double quantizedStart = getNoteStartTime(seq, findNoteOnIndex(seq, 67, expectedQuantized, 10.0));
        passed &= (std::abs(quantizedStart - expectedQuantized) < 1.0);

        juce::Logger::writeToLog(passed ? "PASS: testQuantizeToSixteenthNote" : "FAIL: testQuantizeToSixteenthNote");
        return passed;
    }

    //==========================================================================
    // Test 6: Quantization Strength Parameter
    //==========================================================================
    static bool testQuantizeStrength()
    {
        bool passed = true;

        auto region = createTestMidiRegion();
        auto& seq = region->getMidiSequence();

        // Add a note at tick 200
        double originalPosition = 200.0;
        double gridSize = 1.0; // Quarter note
        double fullQuantized = 0.0; // Nearest quarter note

        addNoteToSequence(seq, 62, 0.8f, 1, originalPosition, originalPosition + TICKS_PER_BEAT);

        int noteIndex = findNoteOnIndex(seq, 62, originalPosition);
        passed &= (noteIndex >= 0);

        // Quantize with 50% strength
        float strength = 0.5f;
        std::vector<int> noteIndices = { noteIndex };
        QuantizeNotesAction action(*region, noteIndices, gridSize, strength, TICKS_PER_BEAT);
        action.perform();

        // Note should be halfway between original (200) and quantized (0) = 100
        double expectedPosition = originalPosition + (fullQuantized - originalPosition) * strength;
        double actualPosition = getNoteStartTime(seq, findNoteOnIndex(seq, 62, expectedPosition, 10.0));
        passed &= (std::abs(actualPosition - expectedPosition) < 1.0);

        juce::Logger::writeToLog(passed ? "PASS: testQuantizeStrength" : "FAIL: testQuantizeStrength");
        return passed;
    }

    //==========================================================================
    // Test 7: Transpose Notes
    //==========================================================================
    static bool testTransposeNotes()
    {
        bool passed = true;

        auto region = createTestMidiRegion();
        auto& seq = region->getMidiSequence();
        juce::UndoManager undoManager;

        // Add two notes
        addNoteToSequence(seq, 60, 0.8f, 1, 0.0, TICKS_PER_BEAT);
        addNoteToSequence(seq, 64, 0.7f, 1, TICKS_PER_BEAT, TICKS_PER_BEAT * 2);

        // Get indices
        int note1Index = findNoteOnIndex(seq, 60, 0.0);
        int note2Index = findNoteOnIndex(seq, 64, TICKS_PER_BEAT);
        passed &= (note1Index >= 0 && note2Index >= 0);

        // Transpose up by 5 semitones
        std::vector<int> noteIndices = { note1Index, note2Index };
        auto* action = new TransposeNotesAction(*region, noteIndices, 5);
        undoManager.perform(action);

        // Verify notes are transposed
        passed &= (findNoteOnIndex(seq, 65, 0.0) >= 0);   // 60 + 5 = 65
        passed &= (findNoteOnIndex(seq, 69, TICKS_PER_BEAT) >= 0); // 64 + 5 = 69

        // Test undo
        undoManager.undo();
        passed &= (findNoteOnIndex(seq, 60, 0.0) >= 0);
        passed &= (findNoteOnIndex(seq, 64, TICKS_PER_BEAT) >= 0);

        juce::Logger::writeToLog(passed ? "PASS: testTransposeNotes" : "FAIL: testTransposeNotes");
        return passed;
    }

    //==========================================================================
    // Test 8: Humanize Notes
    //==========================================================================
    static bool testHumanizeNotes()
    {
        bool passed = true;

        auto region = createTestMidiRegion();
        auto& seq = region->getMidiSequence();
        juce::UndoManager undoManager;

        // Add notes on exact grid positions
        addNoteToSequence(seq, 60, 0.5f, 1, 0.0, TICKS_PER_BEAT);
        addNoteToSequence(seq, 62, 0.5f, 1, TICKS_PER_BEAT, TICKS_PER_BEAT * 2);

        // Get indices
        int note1Index = findNoteOnIndex(seq, 60, 0.0);
        int note2Index = findNoteOnIndex(seq, 62, TICKS_PER_BEAT);
        passed &= (note1Index >= 0 && note2Index >= 0);

        // Store original positions and velocities
        double originalPos1 = getNoteStartTime(seq, note1Index);
        double originalPos2 = getNoteStartTime(seq, note2Index);
        float originalVel1 = seq.getEventPointer(note1Index)->message.getFloatVelocity();
        float originalVel2 = seq.getEventPointer(note2Index)->message.getFloatVelocity();

        // Humanize with timing and velocity variation
        std::vector<int> noteIndices = { note1Index, note2Index };
        auto* action = new HumanizeNotesAction(*region, noteIndices, 50.0, 0.2f, 12345);
        undoManager.perform(action);

        // Find the humanized notes (they may have moved slightly)
        // Since we used a seed, results are deterministic but positions changed
        bool foundNote1 = false;
        bool foundNote2 = false;

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (!event->message.isNoteOn()) continue;

            if (event->message.getNoteNumber() == 60)
            {
                foundNote1 = true;
                // Position should be within humanize range
                passed &= (std::abs(event->message.getTimeStamp() - originalPos1) <= 50.0);
                // Velocity should be within humanize range
                passed &= (std::abs(event->message.getFloatVelocity() - originalVel1) <= 0.2f);
            }
            if (event->message.getNoteNumber() == 62)
            {
                foundNote2 = true;
                passed &= (std::abs(event->message.getTimeStamp() - originalPos2) <= 50.0);
                passed &= (std::abs(event->message.getFloatVelocity() - originalVel2) <= 0.2f);
            }
        }

        passed &= (foundNote1 && foundNote2);

        // Test undo - notes should return to original positions
        undoManager.undo();

        int restored1 = findNoteOnIndex(seq, 60, 0.0, 1.0);
        int restored2 = findNoteOnIndex(seq, 62, TICKS_PER_BEAT, 1.0);
        passed &= (restored1 >= 0 && restored2 >= 0);

        juce::Logger::writeToLog(passed ? "PASS: testHumanizeNotes" : "FAIL: testHumanizeNotes");
        return passed;
    }

    //==========================================================================
    // Test 9: Split Note
    //==========================================================================
    static bool testSplitNote()
    {
        bool passed = true;

        auto region = createTestMidiRegion();
        auto& seq = region->getMidiSequence();
        juce::UndoManager undoManager;

        // Add a long note (2 beats)
        double noteStart = 0.0;
        double noteEnd = TICKS_PER_BEAT * 2;
        addNoteToSequence(seq, 60, 0.8f, 1, noteStart, noteEnd);

        int noteIndex = findNoteOnIndex(seq, 60, noteStart);
        passed &= (noteIndex >= 0);
        passed &= (countNoteOnEvents(seq) == 1);

        // Split at beat 1
        double splitPosition = TICKS_PER_BEAT;
        auto* action = new SplitNoteAction(*region, noteIndex, splitPosition);
        undoManager.perform(action);

        // Should now have 2 notes
        passed &= (countNoteOnEvents(seq) == 2);

        // First note: 0 to 960
        int firstNoteIndex = findNoteOnIndex(seq, 60, 0.0);
        passed &= (firstNoteIndex >= 0);
        if (firstNoteIndex >= 0)
        {
            auto* event = seq.getEventPointer(firstNoteIndex);
            if (event->noteOffObject != nullptr)
            {
                passed &= (std::abs(event->noteOffObject->message.getTimeStamp() - splitPosition) < 1.0);
            }
        }

        // Second note: 960 to 1920
        int secondNoteIndex = findNoteOnIndex(seq, 60, splitPosition);
        passed &= (secondNoteIndex >= 0);
        if (secondNoteIndex >= 0)
        {
            auto* event = seq.getEventPointer(secondNoteIndex);
            if (event->noteOffObject != nullptr)
            {
                passed &= (std::abs(event->noteOffObject->message.getTimeStamp() - noteEnd) < 1.0);
            }
        }

        // Test undo
        undoManager.undo();
        passed &= (countNoteOnEvents(seq) == 1);

        juce::Logger::writeToLog(passed ? "PASS: testSplitNote" : "FAIL: testSplitNote");
        return passed;
    }

    //==========================================================================
    // Test 10: Delete Single Note
    //==========================================================================
    static bool testDeleteNote()
    {
        bool passed = true;

        auto region = createTestMidiRegion();
        auto& seq = region->getMidiSequence();
        juce::UndoManager undoManager;

        // Add two notes
        addNoteToSequence(seq, 60, 0.8f, 1, 0.0, TICKS_PER_BEAT);
        addNoteToSequence(seq, 64, 0.7f, 1, TICKS_PER_BEAT, TICKS_PER_BEAT * 2);

        passed &= (countNoteOnEvents(seq) == 2);

        // Delete the first note
        int noteIndex = findNoteOnIndex(seq, 60, 0.0);
        passed &= (noteIndex >= 0);

        auto* action = new DeleteNoteAction(*region, noteIndex);
        undoManager.perform(action);

        // Should have 1 note left
        passed &= (countNoteOnEvents(seq) == 1);
        passed &= (findNoteOnIndex(seq, 60, 0.0) < 0); // Note 60 should be gone
        passed &= (findNoteOnIndex(seq, 64, TICKS_PER_BEAT) >= 0); // Note 64 should remain

        // Test undo
        undoManager.undo();
        passed &= (countNoteOnEvents(seq) == 2);
        passed &= (findNoteOnIndex(seq, 60, 0.0) >= 0);

        juce::Logger::writeToLog(passed ? "PASS: testDeleteNote" : "FAIL: testDeleteNote");
        return passed;
    }

    //==========================================================================
    // Test 11: Delete Multiple Notes
    //==========================================================================
    static bool testDeleteMultipleNotes()
    {
        bool passed = true;

        auto region = createTestMidiRegion();
        auto& seq = region->getMidiSequence();
        juce::UndoManager undoManager;

        // Add three notes
        addNoteToSequence(seq, 60, 0.8f, 1, 0.0, TICKS_PER_BEAT);
        addNoteToSequence(seq, 64, 0.7f, 1, TICKS_PER_BEAT, TICKS_PER_BEAT * 2);
        addNoteToSequence(seq, 67, 0.9f, 1, TICKS_PER_BEAT * 2, TICKS_PER_BEAT * 3);

        passed &= (countNoteOnEvents(seq) == 3);

        // Get indices for first two notes
        int note1Index = findNoteOnIndex(seq, 60, 0.0);
        int note2Index = findNoteOnIndex(seq, 64, TICKS_PER_BEAT);
        passed &= (note1Index >= 0 && note2Index >= 0);

        // Delete the first two notes
        std::vector<int> noteIndices = { note1Index, note2Index };
        auto* action = new DeleteMultipleNotesAction(*region, noteIndices);
        undoManager.perform(action);

        // Should have 1 note left
        passed &= (countNoteOnEvents(seq) == 1);
        passed &= (findNoteOnIndex(seq, 67, TICKS_PER_BEAT * 2) >= 0);

        // Test undo
        undoManager.undo();
        passed &= (countNoteOnEvents(seq) == 3);

        juce::Logger::writeToLog(passed ? "PASS: testDeleteMultipleNotes" : "FAIL: testDeleteMultipleNotes");
        return passed;
    }

    //==========================================================================
    // Test 12: Quantize Undo/Redo
    //==========================================================================
    static bool testQuantizeUndo()
    {
        bool passed = true;

        auto region = createTestMidiRegion();
        auto& seq = region->getMidiSequence();
        juce::UndoManager undoManager;

        // Add a note off the grid
        double originalPosition = 150.0;
        addNoteToSequence(seq, 60, 0.8f, 1, originalPosition, originalPosition + TICKS_PER_BEAT);

        int noteIndex = findNoteOnIndex(seq, 60, originalPosition);
        passed &= (noteIndex >= 0);

        // Quantize to quarter note
        std::vector<int> noteIndices = { noteIndex };
        auto* action = new QuantizeNotesAction(*region, noteIndices, 1.0, 1.0f, TICKS_PER_BEAT);
        undoManager.perform(action);

        // Note should be at 0
        double quantizedPosition = 0.0;
        int quantizedIndex = findNoteOnIndex(seq, 60, quantizedPosition, 10.0);
        passed &= (quantizedIndex >= 0);

        // Undo - note should return to original position
        undoManager.undo();
        int originalIndex = findNoteOnIndex(seq, 60, originalPosition, 10.0);
        passed &= (originalIndex >= 0);

        // Redo - note should be quantized again
        undoManager.redo();
        int redoIndex = findNoteOnIndex(seq, 60, quantizedPosition, 10.0);
        passed &= (redoIndex >= 0);

        juce::Logger::writeToLog(passed ? "PASS: testQuantizeUndo" : "FAIL: testQuantizeUndo");
        return passed;
    }
};
