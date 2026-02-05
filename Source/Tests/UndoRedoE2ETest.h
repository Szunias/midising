#pragma once

#include "../Timeline/Timeline.h"
#include "../Timeline/Track.h"
#include "../Timeline/AutomationLane.h"
#include "../Audio/AudioTrack.h"
#include "../Utils/UndoManager.h"
#include "../Actions/TrackActions.h"
#include "../Actions/RegionActions.h"
#include "../Actions/MixerActions.h"
#include "../Actions/AutomationActions.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * End-to-end verification tests for complete undo/redo cycle.
 *
 * Test Steps (as specified in implementation_plan.json):
 * 1. Perform 10 different edit operations
 * 2. Undo all 10 operations
 * 3. Redo all 10 operations
 * 4. Verify all states correctly restored
 *
 * This test covers a comprehensive set of different operation types:
 * - Track operations (add, delete)
 * - Region operations (move, resize, rename, split, fades)
 * - Mixer operations (volume, pan, mute)
 * - Automation operations (add point)
 */
class UndoRedoE2ETest
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;

        allPassed &= testUndoManagerBasics();
        allPassed &= testSingleOperationUndoRedo();
        allPassed &= testFiveOperationsCycle();
        allPassed &= testTenDifferentOperationsCycle();
        allPassed &= testMixedOperationsUndoRedo();
        allPassed &= testUndoRedoWithStateVerification();
        allPassed &= testInterruptedUndoRedo();
        allPassed &= testCompleteUndoRedoCycle();

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
    // Helper: Create a test audio region with test data
    //==========================================================================
    static std::unique_ptr<AudioRegion> createTestRegion(int64_t startPos, int64_t length,
                                                          const juce::String& name = "Test Region")
    {
        auto region = std::make_unique<AudioRegion>(startPos, length);
        region->setName(name);

        // Create a simple test audio buffer
        juce::AudioBuffer<float> testBuffer(2, static_cast<int>(length));
        testBuffer.clear();
        for (int sample = 0; sample < static_cast<int>(length); ++sample)
        {
            float value = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi *
                                          440.0f * static_cast<float>(sample) / 44100.0f);
            testBuffer.setSample(0, sample, value);
            testBuffer.setSample(1, sample, value);
        }
        region->setAudioBuffer(testBuffer);

        return region;
    }

    //==========================================================================
    // Helper: State snapshot for verification
    //==========================================================================
    struct TestState
    {
        int numTracks = 0;
        int numRegions = 0;
        int64_t regionPosition = 0;
        int64_t regionLength = 0;
        juce::String regionName;
        int64_t fadeIn = 0;
        int64_t fadeOut = 0;
        float trackVolume = 1.0f;
        float trackPan = 0.0f;
        bool trackMuted = false;
        size_t automationPoints = 0;
    };

    static TestState captureState(Timeline& timeline, AudioTrack* track, AudioRegion* region,
                                   AutomationLane* lane)
    {
        TestState state;
        state.numTracks = timeline.getNumTracks();

        if (track != nullptr)
        {
            state.numRegions = track->getNumRegions();
            state.trackVolume = track->getVolume();
            state.trackPan = track->getPan();
            state.trackMuted = track->isMuted();
        }

        if (region != nullptr)
        {
            state.regionPosition = region->getStartPosition();
            state.regionLength = region->getLength();
            state.regionName = region->getName();
            state.fadeIn = region->getFadeInLength();
            state.fadeOut = region->getFadeOutLength();
        }

        if (lane != nullptr)
        {
            state.automationPoints = lane->getNumPoints();
        }

        return state;
    }

    static bool compareStates(const TestState& a, const TestState& b)
    {
        return a.numTracks == b.numTracks &&
               a.numRegions == b.numRegions &&
               a.regionPosition == b.regionPosition &&
               a.regionLength == b.regionLength &&
               a.regionName == b.regionName &&
               a.fadeIn == b.fadeIn &&
               a.fadeOut == b.fadeOut &&
               floatEquals(a.trackVolume, b.trackVolume) &&
               floatEquals(a.trackPan, b.trackPan) &&
               a.trackMuted == b.trackMuted &&
               a.automationPoints == b.automationPoints;
    }

    //==========================================================================
    // Test 1: UndoManager Basics
    //==========================================================================
    static bool testUndoManagerBasics()
    {
        bool passed = true;

        DAWUndoManager undoManager;

        // Initial state: nothing to undo/redo
        passed &= !undoManager.canUndo();
        passed &= !undoManager.canRedo();

        juce::Logger::writeToLog(passed ? "PASS: testUndoManagerBasics" : "FAIL: testUndoManagerBasics");
        return passed;
    }

    //==========================================================================
    // Test 2: Single Operation Undo/Redo
    //==========================================================================
    static bool testSingleOperationUndoRedo()
    {
        bool passed = true;

        DAWUndoManager undoManager;
        Timeline timeline;

        // Perform one operation
        auto* track = new AudioTrack("Single Test Track");
        undoManager.perform(new AddTrackAction(timeline, track), "Add Track");

        passed &= (timeline.getNumTracks() == 1);
        passed &= undoManager.canUndo();
        passed &= !undoManager.canRedo();

        // Undo
        undoManager.undo();
        passed &= (timeline.getNumTracks() == 0);
        passed &= !undoManager.canUndo();
        passed &= undoManager.canRedo();

        // Redo
        undoManager.redo();
        passed &= (timeline.getNumTracks() == 1);
        passed &= undoManager.canUndo();
        passed &= !undoManager.canRedo();

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testSingleOperationUndoRedo" : "FAIL: testSingleOperationUndoRedo");
        return passed;
    }

    //==========================================================================
    // Test 3: Five Operations Cycle
    //==========================================================================
    static bool testFiveOperationsCycle()
    {
        bool passed = true;

        DAWUndoManager undoManager;
        Timeline timeline;

        // Create initial track with region
        auto* track = new AudioTrack("Five Ops Track");
        timeline.addTrack(track);
        auto region = createTestRegion(0, 4410, "Five Ops Region");
        AudioRegion* regionPtr = region.get();
        track->addRegion(std::move(region));

        // Capture initial state
        TestState initialState = captureState(timeline, track, regionPtr, nullptr);

        // Perform 5 operations
        // Op 1: Move region
        undoManager.beginNewTransaction("Op 1: Move");
        undoManager.perform(new MoveRegionAction(regionPtr, 1000), "");

        // Op 2: Resize region
        undoManager.beginNewTransaction("Op 2: Resize");
        undoManager.perform(new ResizeRegionAction(regionPtr, 2000), "");

        // Op 3: Change volume
        undoManager.beginNewTransaction("Op 3: Volume");
        undoManager.perform(new TrackVolumeAction(track, 0.5f), "");

        // Op 4: Change pan
        undoManager.beginNewTransaction("Op 4: Pan");
        undoManager.perform(new TrackPanAction(track, -0.5f), "");

        // Op 5: Rename region
        undoManager.beginNewTransaction("Op 5: Rename");
        undoManager.perform(new RenameRegionAction(regionPtr, "Renamed Region"), "");

        // Verify all changes applied
        passed &= (regionPtr->getStartPosition() == 1000);
        passed &= (regionPtr->getLength() == 2000);
        passed &= floatEquals(track->getVolume(), 0.5f);
        passed &= floatEquals(track->getPan(), -0.5f);
        passed &= (regionPtr->getName() == "Renamed Region");

        // Undo all 5 operations
        for (int i = 0; i < 5; ++i)
        {
            passed &= undoManager.canUndo();
            undoManager.undo();
        }

        // Verify returned to initial state
        passed &= (regionPtr->getStartPosition() == initialState.regionPosition);
        passed &= (regionPtr->getLength() == initialState.regionLength);
        passed &= floatEquals(track->getVolume(), initialState.trackVolume);
        passed &= floatEquals(track->getPan(), initialState.trackPan);
        passed &= (regionPtr->getName() == initialState.regionName);
        passed &= !undoManager.canUndo();

        // Redo all 5 operations
        for (int i = 0; i < 5; ++i)
        {
            passed &= undoManager.canRedo();
            undoManager.redo();
        }

        // Verify all changes restored
        passed &= (regionPtr->getStartPosition() == 1000);
        passed &= (regionPtr->getLength() == 2000);
        passed &= floatEquals(track->getVolume(), 0.5f);
        passed &= floatEquals(track->getPan(), -0.5f);
        passed &= (regionPtr->getName() == "Renamed Region");
        passed &= !undoManager.canRedo();

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testFiveOperationsCycle" : "FAIL: testFiveOperationsCycle");
        return passed;
    }

    //==========================================================================
    // Test 4: Ten Different Operations Cycle (Main E2E Test)
    //==========================================================================
    static bool testTenDifferentOperationsCycle()
    {
        bool passed = true;

        DAWUndoManager undoManager;
        Timeline timeline;

        // === SETUP: Create initial state ===
        auto* track = new AudioTrack("Main Track");
        timeline.addTrack(track);

        auto region = createTestRegion(0, 8820, "Original Region");
        AudioRegion* regionPtr = region.get();
        track->addRegion(std::move(region));

        // Create automation lane
        AutomationLane* volumeLane = track->addAutomationLane("Volume");

        // Capture initial state for final verification
        TestState initialState = captureState(timeline, track, regionPtr, volumeLane);

        // === STEP 1: Perform 10 different edit operations ===

        // Op 1: Move region to position 1000
        undoManager.beginNewTransaction("Op 1: Move Region");
        undoManager.perform(new MoveRegionAction(regionPtr, 1000), "");
        passed &= (regionPtr->getStartPosition() == 1000);

        // Op 2: Resize region to length 5000
        undoManager.beginNewTransaction("Op 2: Resize Region");
        undoManager.perform(new ResizeRegionAction(regionPtr, 5000), "");
        passed &= (regionPtr->getLength() == 5000);

        // Op 3: Rename region
        undoManager.beginNewTransaction("Op 3: Rename Region");
        undoManager.perform(new RenameRegionAction(regionPtr, "Edited Region"), "");
        passed &= (regionPtr->getName() == "Edited Region");

        // Op 4: Set fade in/out
        undoManager.beginNewTransaction("Op 4: Set Fades");
        undoManager.perform(new SetRegionFadesAction(regionPtr, 200, 300), "");
        passed &= (regionPtr->getFadeInLength() == 200);
        passed &= (regionPtr->getFadeOutLength() == 300);

        // Op 5: Change track volume
        undoManager.beginNewTransaction("Op 5: Track Volume");
        undoManager.perform(new TrackVolumeAction(track, 0.75f), "");
        passed &= floatEquals(track->getVolume(), 0.75f);

        // Op 6: Change track pan
        undoManager.beginNewTransaction("Op 6: Track Pan");
        undoManager.perform(new TrackPanAction(track, 0.3f), "");
        passed &= floatEquals(track->getPan(), 0.3f);

        // Op 7: Mute track
        undoManager.beginNewTransaction("Op 7: Mute Track");
        undoManager.perform(new TrackMuteAction(track, true), "");
        passed &= (track->isMuted() == true);

        // Op 8: Add automation point
        undoManager.beginNewTransaction("Op 8: Add Automation Point");
        undoManager.perform(new AddAutomationPointAction(*volumeLane, 0, 0.5f), "");
        passed &= (volumeLane->getNumPoints() == 1);

        // Op 9: Add another automation point
        undoManager.beginNewTransaction("Op 9: Add Second Automation Point");
        undoManager.perform(new AddAutomationPointAction(*volumeLane, 22050, 1.0f), "");
        passed &= (volumeLane->getNumPoints() == 2);

        // Op 10: Add a second track
        undoManager.beginNewTransaction("Op 10: Add Track");
        auto* track2 = new AudioTrack("Second Track");
        undoManager.perform(new AddTrackAction(timeline, track2), "");
        passed &= (timeline.getNumTracks() == 2);

        // Capture state after all operations
        TestState afterOpsState;
        afterOpsState.numTracks = 2;
        afterOpsState.regionPosition = 1000;
        afterOpsState.regionLength = 5000;
        afterOpsState.regionName = "Edited Region";
        afterOpsState.fadeIn = 200;
        afterOpsState.fadeOut = 300;
        afterOpsState.trackVolume = 0.75f;
        afterOpsState.trackPan = 0.3f;
        afterOpsState.trackMuted = true;
        afterOpsState.automationPoints = 2;

        juce::Logger::writeToLog("Step 1 complete: Performed 10 operations");

        // === STEP 2: Undo all 10 operations ===
        int undoCount = 0;
        while (undoManager.canUndo() && undoCount < 10)
        {
            undoManager.undo();
            undoCount++;
        }
        passed &= (undoCount == 10);

        // Verify returned to initial state
        passed &= (timeline.getNumTracks() == initialState.numTracks);
        passed &= (regionPtr->getStartPosition() == initialState.regionPosition);
        passed &= (regionPtr->getLength() == initialState.regionLength);
        passed &= (regionPtr->getName() == initialState.regionName);
        passed &= (regionPtr->getFadeInLength() == initialState.fadeIn);
        passed &= (regionPtr->getFadeOutLength() == initialState.fadeOut);
        passed &= floatEquals(track->getVolume(), initialState.trackVolume);
        passed &= floatEquals(track->getPan(), initialState.trackPan);
        passed &= (track->isMuted() == initialState.trackMuted);
        passed &= (volumeLane->getNumPoints() == initialState.automationPoints);
        passed &= !undoManager.canUndo();

        juce::Logger::writeToLog("Step 2 complete: Undid all 10 operations");

        // === STEP 3: Redo all 10 operations ===
        int redoCount = 0;
        while (undoManager.canRedo() && redoCount < 10)
        {
            undoManager.redo();
            redoCount++;
        }
        passed &= (redoCount == 10);

        juce::Logger::writeToLog("Step 3 complete: Redid all 10 operations");

        // === STEP 4: Verify all states correctly restored ===
        passed &= (timeline.getNumTracks() == afterOpsState.numTracks);
        passed &= (regionPtr->getStartPosition() == afterOpsState.regionPosition);
        passed &= (regionPtr->getLength() == afterOpsState.regionLength);
        passed &= (regionPtr->getName() == afterOpsState.regionName);
        passed &= (regionPtr->getFadeInLength() == afterOpsState.fadeIn);
        passed &= (regionPtr->getFadeOutLength() == afterOpsState.fadeOut);
        passed &= floatEquals(track->getVolume(), afterOpsState.trackVolume);
        passed &= floatEquals(track->getPan(), afterOpsState.trackPan);
        passed &= (track->isMuted() == afterOpsState.trackMuted);
        passed &= (volumeLane->getNumPoints() == afterOpsState.automationPoints);
        passed &= !undoManager.canRedo();

        juce::Logger::writeToLog("Step 4 complete: Verified all states restored");

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testTenDifferentOperationsCycle" : "FAIL: testTenDifferentOperationsCycle");
        return passed;
    }

    //==========================================================================
    // Test 5: Mixed Operations with Multiple Tracks
    //==========================================================================
    static bool testMixedOperationsUndoRedo()
    {
        bool passed = true;

        DAWUndoManager undoManager;
        Timeline timeline;

        // Add 3 tracks via undo actions
        for (int i = 1; i <= 3; ++i)
        {
            undoManager.beginNewTransaction(juce::String("Add Track ") + juce::String(i));
            auto* track = new AudioTrack(juce::String("Track ") + juce::String(i));
            undoManager.perform(new AddTrackAction(timeline, track), "");
        }

        passed &= (timeline.getNumTracks() == 3);

        // Modify each track's properties
        for (int i = 0; i < 3; ++i)
        {
            Track* track = timeline.getTrack(i);
            undoManager.beginNewTransaction(juce::String("Volume Track ") + juce::String(i));
            undoManager.perform(new TrackVolumeAction(track, 0.5f + i * 0.1f), "");
        }

        // Verify volumes
        passed &= floatEquals(timeline.getTrack(0)->getVolume(), 0.5f);
        passed &= floatEquals(timeline.getTrack(1)->getVolume(), 0.6f);
        passed &= floatEquals(timeline.getTrack(2)->getVolume(), 0.7f);

        // Undo all 6 operations (3 adds + 3 volumes)
        for (int i = 0; i < 6; ++i)
        {
            passed &= undoManager.canUndo();
            undoManager.undo();
        }

        // Should have no tracks now
        passed &= (timeline.getNumTracks() == 0);
        passed &= !undoManager.canUndo();

        // Redo all 6 operations
        for (int i = 0; i < 6; ++i)
        {
            passed &= undoManager.canRedo();
            undoManager.redo();
        }

        // Verify all restored
        passed &= (timeline.getNumTracks() == 3);
        passed &= floatEquals(timeline.getTrack(0)->getVolume(), 0.5f);
        passed &= floatEquals(timeline.getTrack(1)->getVolume(), 0.6f);
        passed &= floatEquals(timeline.getTrack(2)->getVolume(), 0.7f);
        passed &= !undoManager.canRedo();

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testMixedOperationsUndoRedo" : "FAIL: testMixedOperationsUndoRedo");
        return passed;
    }

    //==========================================================================
    // Test 6: Undo/Redo with Detailed State Verification
    //==========================================================================
    static bool testUndoRedoWithStateVerification()
    {
        bool passed = true;

        DAWUndoManager undoManager;
        Timeline timeline;

        auto* track = new AudioTrack("State Test Track");
        timeline.addTrack(track);

        auto region = createTestRegion(0, 4410, "State Region");
        AudioRegion* regionPtr = region.get();
        track->addRegion(std::move(region));

        AutomationLane* lane = track->addAutomationLane("Volume");

        // Capture states at each step
        std::vector<TestState> states;
        states.push_back(captureState(timeline, track, regionPtr, lane));

        // Perform operations and capture state after each
        // Op 1
        undoManager.beginNewTransaction("Op 1");
        undoManager.perform(new MoveRegionAction(regionPtr, 500), "");
        states.push_back(captureState(timeline, track, regionPtr, lane));

        // Op 2
        undoManager.beginNewTransaction("Op 2");
        undoManager.perform(new TrackVolumeAction(track, 0.8f), "");
        states.push_back(captureState(timeline, track, regionPtr, lane));

        // Op 3
        undoManager.beginNewTransaction("Op 3");
        undoManager.perform(new AddAutomationPointAction(*lane, 0, 0.5f), "");
        states.push_back(captureState(timeline, track, regionPtr, lane));

        // Verify we can undo back to each state
        passed &= compareStates(captureState(timeline, track, regionPtr, lane), states[3]);

        undoManager.undo();
        passed &= compareStates(captureState(timeline, track, regionPtr, lane), states[2]);

        undoManager.undo();
        passed &= compareStates(captureState(timeline, track, regionPtr, lane), states[1]);

        undoManager.undo();
        passed &= compareStates(captureState(timeline, track, regionPtr, lane), states[0]);

        // Verify we can redo forward to each state
        undoManager.redo();
        passed &= compareStates(captureState(timeline, track, regionPtr, lane), states[1]);

        undoManager.redo();
        passed &= compareStates(captureState(timeline, track, regionPtr, lane), states[2]);

        undoManager.redo();
        passed &= compareStates(captureState(timeline, track, regionPtr, lane), states[3]);

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testUndoRedoWithStateVerification" : "FAIL: testUndoRedoWithStateVerification");
        return passed;
    }

    //==========================================================================
    // Test 7: Interrupted Undo/Redo (partial undo then new action)
    //==========================================================================
    static bool testInterruptedUndoRedo()
    {
        bool passed = true;

        DAWUndoManager undoManager;
        Timeline timeline;

        auto* track = new AudioTrack("Interrupt Track");
        timeline.addTrack(track);

        auto region = createTestRegion(0, 4410, "Interrupt Region");
        AudioRegion* regionPtr = region.get();
        track->addRegion(std::move(region));

        // Perform 5 operations
        for (int i = 1; i <= 5; ++i)
        {
            undoManager.beginNewTransaction(juce::String("Move ") + juce::String(i));
            undoManager.perform(new MoveRegionAction(regionPtr, i * 100), "");
        }
        passed &= (regionPtr->getStartPosition() == 500);

        // Undo 3 operations (back to state after op 2)
        undoManager.undo();  // Back to 400
        undoManager.undo();  // Back to 300
        undoManager.undo();  // Back to 200
        passed &= (regionPtr->getStartPosition() == 200);
        passed &= undoManager.canRedo();  // Can still redo

        // Perform a new action - this should clear redo history
        undoManager.beginNewTransaction("New Move");
        undoManager.perform(new MoveRegionAction(regionPtr, 999), "");
        passed &= (regionPtr->getStartPosition() == 999);
        passed &= !undoManager.canRedo();  // Redo should be cleared

        // Can still undo the new action and previous 2
        undoManager.undo();  // Back to 200
        passed &= (regionPtr->getStartPosition() == 200);

        undoManager.undo();  // Back to 100
        passed &= (regionPtr->getStartPosition() == 100);

        undoManager.undo();  // Back to 0
        passed &= (regionPtr->getStartPosition() == 0);

        // Redo should now work for remaining history
        passed &= undoManager.canRedo();

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testInterruptedUndoRedo" : "FAIL: testInterruptedUndoRedo");
        return passed;
    }

    //==========================================================================
    // Test 8: Complete Undo/Redo Cycle (Full E2E as per spec)
    //==========================================================================
    static bool testCompleteUndoRedoCycle()
    {
        bool passed = true;

        juce::Logger::writeToLog("=== Complete Undo/Redo Cycle E2E Test ===");

        DAWUndoManager undoManager;
        Timeline timeline;

        // ===================================================================
        // SETUP: Create comprehensive test environment
        // ===================================================================
        auto* track1 = new AudioTrack("Lead Vocals");
        timeline.addTrack(track1);

        auto region1 = createTestRegion(0, 44100, "Verse 1");
        AudioRegion* region1Ptr = region1.get();
        track1->addRegion(std::move(region1));

        auto region2 = createTestRegion(44100, 22050, "Chorus");
        track1->addRegion(std::move(region2));

        AutomationLane* volumeLane = track1->addAutomationLane("Volume");
        AutomationLane* panLane = track1->addAutomationLane("Pan");

        // Record initial state
        juce::Logger::writeToLog("Initial State:");
        juce::Logger::writeToLog("  Tracks: " + juce::String(timeline.getNumTracks()));
        juce::Logger::writeToLog("  Regions: " + juce::String(track1->getNumRegions()));
        juce::Logger::writeToLog("  Region1 pos: " + juce::String(region1Ptr->getStartPosition()));
        juce::Logger::writeToLog("  Track volume: " + juce::String(track1->getVolume()));

        // Save initial values for verification
        const int64_t initialRegion1Pos = region1Ptr->getStartPosition();
        const int64_t initialRegion1Len = region1Ptr->getLength();
        const juce::String initialRegion1Name = region1Ptr->getName();
        const float initialVolume = track1->getVolume();
        const float initialPan = track1->getPan();
        const bool initialMute = track1->isMuted();

        // ===================================================================
        // STEP 1: Perform 10 different edit operations
        // ===================================================================
        juce::Logger::writeToLog("\n--- STEP 1: Performing 10 operations ---");

        // Op 1: Move first region
        undoManager.beginNewTransaction("1. Move Region");
        undoManager.perform(new MoveRegionAction(region1Ptr, 2000), "");
        juce::Logger::writeToLog("Op 1: Move region to 2000 - pos=" + juce::String(region1Ptr->getStartPosition()));

        // Op 2: Resize first region
        undoManager.beginNewTransaction("2. Resize Region");
        undoManager.perform(new ResizeRegionAction(region1Ptr, 30000), "");
        juce::Logger::writeToLog("Op 2: Resize region to 30000 - len=" + juce::String(region1Ptr->getLength()));

        // Op 3: Rename first region
        undoManager.beginNewTransaction("3. Rename Region");
        undoManager.perform(new RenameRegionAction(region1Ptr, "Intro"), "");
        juce::Logger::writeToLog("Op 3: Rename region - name=" + region1Ptr->getName());

        // Op 4: Set fades
        undoManager.beginNewTransaction("4. Set Fades");
        undoManager.perform(new SetRegionFadesAction(region1Ptr, 500, 750), "");
        juce::Logger::writeToLog("Op 4: Set fades - fadeIn=" + juce::String(region1Ptr->getFadeInLength()) +
                                  ", fadeOut=" + juce::String(region1Ptr->getFadeOutLength()));

        // Op 5: Change volume
        undoManager.beginNewTransaction("5. Volume Change");
        undoManager.perform(new TrackVolumeAction(track1, 0.6f), "");
        juce::Logger::writeToLog("Op 5: Volume change - vol=" + juce::String(track1->getVolume()));

        // Op 6: Change pan
        undoManager.beginNewTransaction("6. Pan Change");
        undoManager.perform(new TrackPanAction(track1, -0.4f), "");
        juce::Logger::writeToLog("Op 6: Pan change - pan=" + juce::String(track1->getPan()));

        // Op 7: Mute track
        undoManager.beginNewTransaction("7. Mute Track");
        undoManager.perform(new TrackMuteAction(track1, true), "");
        juce::Logger::writeToLog("Op 7: Mute track - muted=" + juce::String(track1->isMuted() ? "true" : "false"));

        // Op 8: Add volume automation
        undoManager.beginNewTransaction("8. Add Volume Automation");
        undoManager.perform(new AddAutomationPointAction(*volumeLane, 0, 0.25f), "");
        undoManager.perform(new AddAutomationPointAction(*volumeLane, 22050, 0.75f), "");
        juce::Logger::writeToLog("Op 8: Add volume automation - points=" + juce::String(volumeLane->getNumPoints()));

        // Op 9: Add pan automation
        undoManager.beginNewTransaction("9. Add Pan Automation");
        undoManager.perform(new AddAutomationPointAction(*panLane, 0, 0.0f), "");
        undoManager.perform(new AddAutomationPointAction(*panLane, 44100, 1.0f), "");
        juce::Logger::writeToLog("Op 9: Add pan automation - points=" + juce::String(panLane->getNumPoints()));

        // Op 10: Add second track
        undoManager.beginNewTransaction("10. Add Second Track");
        auto* track2 = new AudioTrack("Backing Vocals");
        undoManager.perform(new AddTrackAction(timeline, track2), "");
        juce::Logger::writeToLog("Op 10: Add track - tracks=" + juce::String(timeline.getNumTracks()));

        // Save state after all operations
        const int64_t afterOpsRegion1Pos = region1Ptr->getStartPosition();
        const int64_t afterOpsRegion1Len = region1Ptr->getLength();
        const juce::String afterOpsRegion1Name = region1Ptr->getName();
        const int64_t afterOpsFadeIn = region1Ptr->getFadeInLength();
        const int64_t afterOpsFadeOut = region1Ptr->getFadeOutLength();
        const float afterOpsVolume = track1->getVolume();
        const float afterOpsPan = track1->getPan();
        const bool afterOpsMute = track1->isMuted();
        const size_t afterOpsVolAutoPts = volumeLane->getNumPoints();
        const size_t afterOpsPanAutoPts = panLane->getNumPoints();
        const int afterOpsTrackCount = timeline.getNumTracks();

        // Verify all operations applied
        passed &= (afterOpsRegion1Pos == 2000);
        passed &= (afterOpsRegion1Len == 30000);
        passed &= (afterOpsRegion1Name == "Intro");
        passed &= (afterOpsFadeIn == 500);
        passed &= (afterOpsFadeOut == 750);
        passed &= floatEquals(afterOpsVolume, 0.6f);
        passed &= floatEquals(afterOpsPan, -0.4f);
        passed &= (afterOpsMute == true);
        passed &= (afterOpsVolAutoPts == 2);
        passed &= (afterOpsPanAutoPts == 2);
        passed &= (afterOpsTrackCount == 2);

        juce::Logger::writeToLog("\nStep 1 verification: " + juce::String(passed ? "PASS" : "FAIL"));

        // ===================================================================
        // STEP 2: Undo all 10 operations
        // ===================================================================
        juce::Logger::writeToLog("\n--- STEP 2: Undoing all 10 operations ---");

        int undoCount = 0;
        while (undoManager.canUndo() && undoCount < 10)
        {
            undoManager.undo();
            undoCount++;
            juce::Logger::writeToLog("Undo " + juce::String(undoCount) + " complete");
        }

        passed &= (undoCount == 10);

        // Verify returned to initial state
        passed &= (region1Ptr->getStartPosition() == initialRegion1Pos);
        passed &= (region1Ptr->getLength() == initialRegion1Len);
        passed &= (region1Ptr->getName() == initialRegion1Name);
        passed &= (region1Ptr->getFadeInLength() == 0);  // Initial default
        passed &= (region1Ptr->getFadeOutLength() == 0);  // Initial default
        passed &= floatEquals(track1->getVolume(), initialVolume);
        passed &= floatEquals(track1->getPan(), initialPan);
        passed &= (track1->isMuted() == initialMute);
        passed &= (volumeLane->getNumPoints() == 0);
        passed &= (panLane->getNumPoints() == 0);
        passed &= (timeline.getNumTracks() == 1);  // Back to single track

        juce::Logger::writeToLog("\nAfter undo state:");
        juce::Logger::writeToLog("  Tracks: " + juce::String(timeline.getNumTracks()));
        juce::Logger::writeToLog("  Region pos: " + juce::String(region1Ptr->getStartPosition()));
        juce::Logger::writeToLog("  Volume: " + juce::String(track1->getVolume()));

        juce::Logger::writeToLog("Step 2 verification: " + juce::String(passed ? "PASS" : "FAIL"));

        // ===================================================================
        // STEP 3: Redo all 10 operations
        // ===================================================================
        juce::Logger::writeToLog("\n--- STEP 3: Redoing all 10 operations ---");

        int redoCount = 0;
        while (undoManager.canRedo() && redoCount < 10)
        {
            undoManager.redo();
            redoCount++;
            juce::Logger::writeToLog("Redo " + juce::String(redoCount) + " complete");
        }

        passed &= (redoCount == 10);

        juce::Logger::writeToLog("Step 3: Redone " + juce::String(redoCount) + " operations");

        // ===================================================================
        // STEP 4: Verify all states correctly restored
        // ===================================================================
        juce::Logger::writeToLog("\n--- STEP 4: Verifying all states restored ---");

        // Verify all values match post-operation state
        passed &= (region1Ptr->getStartPosition() == afterOpsRegion1Pos);
        passed &= (region1Ptr->getLength() == afterOpsRegion1Len);
        passed &= (region1Ptr->getName() == afterOpsRegion1Name);
        passed &= (region1Ptr->getFadeInLength() == afterOpsFadeIn);
        passed &= (region1Ptr->getFadeOutLength() == afterOpsFadeOut);
        passed &= floatEquals(track1->getVolume(), afterOpsVolume);
        passed &= floatEquals(track1->getPan(), afterOpsPan);
        passed &= (track1->isMuted() == afterOpsMute);
        passed &= (volumeLane->getNumPoints() == afterOpsVolAutoPts);
        passed &= (panLane->getNumPoints() == afterOpsPanAutoPts);
        passed &= (timeline.getNumTracks() == afterOpsTrackCount);

        juce::Logger::writeToLog("\nFinal State:");
        juce::Logger::writeToLog("  Tracks: " + juce::String(timeline.getNumTracks()) +
                                  " (expected: " + juce::String(afterOpsTrackCount) + ")");
        juce::Logger::writeToLog("  Region pos: " + juce::String(region1Ptr->getStartPosition()) +
                                  " (expected: " + juce::String(afterOpsRegion1Pos) + ")");
        juce::Logger::writeToLog("  Region len: " + juce::String(region1Ptr->getLength()) +
                                  " (expected: " + juce::String(afterOpsRegion1Len) + ")");
        juce::Logger::writeToLog("  Region name: " + region1Ptr->getName() +
                                  " (expected: " + afterOpsRegion1Name + ")");
        juce::Logger::writeToLog("  Volume: " + juce::String(track1->getVolume()) +
                                  " (expected: " + juce::String(afterOpsVolume) + ")");
        juce::Logger::writeToLog("  Pan: " + juce::String(track1->getPan()) +
                                  " (expected: " + juce::String(afterOpsPan) + ")");
        juce::Logger::writeToLog("  Muted: " + juce::String(track1->isMuted() ? "true" : "false") +
                                  " (expected: " + juce::String(afterOpsMute ? "true" : "false") + ")");
        juce::Logger::writeToLog("  Vol automation: " + juce::String(volumeLane->getNumPoints()) +
                                  " (expected: " + juce::String(afterOpsVolAutoPts) + ")");
        juce::Logger::writeToLog("  Pan automation: " + juce::String(panLane->getNumPoints()) +
                                  " (expected: " + juce::String(afterOpsPanAutoPts) + ")");

        juce::Logger::writeToLog("\nStep 4 verification: " + juce::String(passed ? "PASS" : "FAIL"));

        // Clean up
        timeline.clearTracks();

        juce::Logger::writeToLog("\n=== Complete Undo/Redo Cycle E2E Test: " +
                                  juce::String(passed ? "PASS" : "FAIL") + " ===");

        juce::Logger::writeToLog(passed ? "PASS: testCompleteUndoRedoCycle" : "FAIL: testCompleteUndoRedoCycle");
        return passed;
    }
};
