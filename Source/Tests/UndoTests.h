#pragma once

#include "../Timeline/Timeline.h"
#include "../Audio/AudioTrack.h"
#include "../Utils/UndoManager.h"
#include "../Actions/TrackActions.h"
#include "../Actions/RegionActions.h"
#include "../Actions/MixerActions.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * Unit tests for the undo system.
 *
 * Test Coverage:
 * 1. Basic DAWUndoManager operations (perform, undo, redo)
 * 2. Track actions (add, delete)
 * 3. Region actions (move, resize, split, rename, fades)
 * 4. Mixer actions (volume, pan, mute, solo)
 * 5. Transaction grouping
 * 6. History management (multiple undo, clear)
 * 7. Listener notifications
 */
class UndoTests
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;

        allPassed &= testBasicUndoRedo();
        allPassed &= testAddTrackAction();
        allPassed &= testDeleteTrackAction();
        allPassed &= testMoveRegionAction();
        allPassed &= testResizeRegionAction();
        allPassed &= testSplitRegionAction();
        allPassed &= testRenameRegionAction();
        allPassed &= testSetRegionFadesAction();
        allPassed &= testMixerVolumeAction();
        allPassed &= testMixerPanAction();
        allPassed &= testMixerMuteAction();
        allPassed &= testTransactionGrouping();
        allPassed &= testMultipleUndoRedo();
        allPassed &= testHistoryManagement();
        allPassed &= testListenerNotifications();

        return allPassed;
    }

private:
    //==========================================================================
    // Helper: Create a test audio region with test data
    //==========================================================================
    static std::unique_ptr<AudioRegion> createTestRegion(int64_t startPos, int64_t length, const juce::String& name = "Test Region")
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
    // Test 1: Basic Undo/Redo Operations
    //==========================================================================
    static bool testBasicUndoRedo()
    {
        DAWUndoManager undoManager;
        Timeline timeline;
        bool passed = true;

        // Initially no undo/redo available
        passed &= (undoManager.canUndo() == false);
        passed &= (undoManager.canRedo() == false);

        // Add a track via action
        auto* track = new AudioTrack("Test Track");
        undoManager.perform(new AddTrackAction(timeline, track), "Add Track");

        // Now can undo but not redo
        passed &= (undoManager.canUndo() == true);
        passed &= (undoManager.canRedo() == false);
        passed &= (timeline.getNumTracks() == 1);

        // Undo the action
        undoManager.undo();
        passed &= (timeline.getNumTracks() == 0);
        passed &= (undoManager.canUndo() == false);
        passed &= (undoManager.canRedo() == true);

        // Redo the action
        undoManager.redo();
        passed &= (timeline.getNumTracks() == 1);
        passed &= (undoManager.canUndo() == true);
        passed &= (undoManager.canRedo() == false);

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testBasicUndoRedo" : "FAIL: testBasicUndoRedo");
        return passed;
    }

    //==========================================================================
    // Test 2: Add Track Action
    //==========================================================================
    static bool testAddTrackAction()
    {
        DAWUndoManager undoManager;
        Timeline timeline;
        bool passed = true;

        // Add multiple tracks
        auto* track1 = new AudioTrack("Track 1");
        auto* track2 = new AudioTrack("Track 2");
        auto* track3 = new AudioTrack("Track 3");

        undoManager.perform(new AddTrackAction(timeline, track1), "Add Track 1");
        undoManager.perform(new AddTrackAction(timeline, track2), "Add Track 2");
        undoManager.perform(new AddTrackAction(timeline, track3), "Add Track 3");

        passed &= (timeline.getNumTracks() == 3);
        passed &= (timeline.getTrack(0)->getName() == "Track 1");
        passed &= (timeline.getTrack(1)->getName() == "Track 2");
        passed &= (timeline.getTrack(2)->getName() == "Track 3");

        // Undo last add
        undoManager.undo();
        passed &= (timeline.getNumTracks() == 2);

        // Undo second add
        undoManager.undo();
        passed &= (timeline.getNumTracks() == 1);

        // Redo both
        undoManager.redo();
        undoManager.redo();
        passed &= (timeline.getNumTracks() == 3);

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testAddTrackAction" : "FAIL: testAddTrackAction");
        return passed;
    }

    //==========================================================================
    // Test 3: Delete Track Action
    //==========================================================================
    static bool testDeleteTrackAction()
    {
        DAWUndoManager undoManager;
        Timeline timeline;
        bool passed = true;

        // Create tracks directly (not via undo)
        timeline.addTrack(new AudioTrack("Track A"));
        timeline.addTrack(new AudioTrack("Track B"));
        timeline.addTrack(new AudioTrack("Track C"));

        passed &= (timeline.getNumTracks() == 3);

        // Delete middle track via action
        undoManager.perform(new DeleteTrackAction(timeline, 1), "Delete Track B");

        passed &= (timeline.getNumTracks() == 2);
        passed &= (timeline.getTrack(0)->getName() == "Track A");
        passed &= (timeline.getTrack(1)->getName() == "Track C");

        // Undo delete
        undoManager.undo();
        passed &= (timeline.getNumTracks() == 3);

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testDeleteTrackAction" : "FAIL: testDeleteTrackAction");
        return passed;
    }

    //==========================================================================
    // Test 4: Move Region Action
    //==========================================================================
    static bool testMoveRegionAction()
    {
        DAWUndoManager undoManager;
        bool passed = true;

        auto region = createTestRegion(0, 4410, "Move Test");
        AudioRegion* regionPtr = region.get();

        // Initial position
        passed &= (regionPtr->getStartPosition() == 0);

        // Move region via action
        undoManager.perform(new MoveRegionAction(regionPtr, 1000), "Move Region");
        passed &= (regionPtr->getStartPosition() == 1000);

        // Undo move
        undoManager.undo();
        passed &= (regionPtr->getStartPosition() == 0);

        // Redo move
        undoManager.redo();
        passed &= (regionPtr->getStartPosition() == 1000);

        juce::Logger::writeToLog(passed ? "PASS: testMoveRegionAction" : "FAIL: testMoveRegionAction");
        return passed;
    }

    //==========================================================================
    // Test 5: Resize Region Action
    //==========================================================================
    static bool testResizeRegionAction()
    {
        DAWUndoManager undoManager;
        bool passed = true;

        auto region = createTestRegion(0, 4410, "Resize Test");
        AudioRegion* regionPtr = region.get();

        int64_t originalLength = regionPtr->getLength();
        int64_t originalOffset = regionPtr->getOffset();

        // Resize region via action
        undoManager.perform(new ResizeRegionAction(regionPtr, 2000, 500), "Resize Region");

        passed &= (regionPtr->getLength() == 2000);
        passed &= (regionPtr->getOffset() == 500);

        // Undo resize
        undoManager.undo();
        passed &= (regionPtr->getLength() == originalLength);
        passed &= (regionPtr->getOffset() == originalOffset);

        // Redo resize
        undoManager.redo();
        passed &= (regionPtr->getLength() == 2000);
        passed &= (regionPtr->getOffset() == 500);

        juce::Logger::writeToLog(passed ? "PASS: testResizeRegionAction" : "FAIL: testResizeRegionAction");
        return passed;
    }

    //==========================================================================
    // Test 6: Split Region Action
    //==========================================================================
    static bool testSplitRegionAction()
    {
        DAWUndoManager undoManager;
        bool passed = true;

        AudioTrack track("Split Test Track");
        auto region = createTestRegion(0, 4000, "Split Test");
        AudioRegion* regionPtr = region.get();
        track.addRegion(std::move(region));

        int64_t originalLength = regionPtr->getLength();
        passed &= (track.getNumRegions() == 1);

        // Split at position 2000
        undoManager.perform(new SplitRegionAction(track, regionPtr, 2000), "Split Region");

        // Should now have 2 regions
        passed &= (track.getNumRegions() == 2);
        passed &= (regionPtr->getLength() == 2000);

        // Find the second region
        AudioRegion* secondRegion = nullptr;
        for (int i = 0; i < track.getNumRegions(); ++i)
        {
            if (track.getRegion(i) != regionPtr)
            {
                secondRegion = track.getRegion(i);
                break;
            }
        }

        passed &= (secondRegion != nullptr);
        if (secondRegion != nullptr)
        {
            passed &= (secondRegion->getStartPosition() == 2000);
            passed &= (secondRegion->getLength() == 2000);
        }

        // Undo split
        undoManager.undo();
        passed &= (track.getNumRegions() == 1);
        passed &= (regionPtr->getLength() == originalLength);

        // Redo split
        undoManager.redo();
        passed &= (track.getNumRegions() == 2);

        juce::Logger::writeToLog(passed ? "PASS: testSplitRegionAction" : "FAIL: testSplitRegionAction");
        return passed;
    }

    //==========================================================================
    // Test 7: Rename Region Action
    //==========================================================================
    static bool testRenameRegionAction()
    {
        DAWUndoManager undoManager;
        bool passed = true;

        auto region = createTestRegion(0, 4410, "Original Name");
        Region* regionPtr = region.get();

        passed &= (regionPtr->getName() == "Original Name");

        // Rename via action
        undoManager.perform(new RenameRegionAction(regionPtr, "New Name"), "Rename Region");
        passed &= (regionPtr->getName() == "New Name");

        // Undo rename
        undoManager.undo();
        passed &= (regionPtr->getName() == "Original Name");

        // Redo rename
        undoManager.redo();
        passed &= (regionPtr->getName() == "New Name");

        juce::Logger::writeToLog(passed ? "PASS: testRenameRegionAction" : "FAIL: testRenameRegionAction");
        return passed;
    }

    //==========================================================================
    // Test 8: Set Region Fades Action
    //==========================================================================
    static bool testSetRegionFadesAction()
    {
        DAWUndoManager undoManager;
        bool passed = true;

        auto region = createTestRegion(0, 4410, "Fade Test");
        Region* regionPtr = region.get();

        // Set initial fades
        regionPtr->setFadeInLength(100);
        regionPtr->setFadeOutLength(200);

        int64_t originalFadeIn = regionPtr->getFadeInLength();
        int64_t originalFadeOut = regionPtr->getFadeOutLength();

        passed &= (originalFadeIn == 100);
        passed &= (originalFadeOut == 200);

        // Change fades via action
        undoManager.perform(new SetRegionFadesAction(regionPtr, 500, 600), "Set Fades");
        passed &= (regionPtr->getFadeInLength() == 500);
        passed &= (regionPtr->getFadeOutLength() == 600);

        // Undo
        undoManager.undo();
        passed &= (regionPtr->getFadeInLength() == originalFadeIn);
        passed &= (regionPtr->getFadeOutLength() == originalFadeOut);

        // Redo
        undoManager.redo();
        passed &= (regionPtr->getFadeInLength() == 500);
        passed &= (regionPtr->getFadeOutLength() == 600);

        juce::Logger::writeToLog(passed ? "PASS: testSetRegionFadesAction" : "FAIL: testSetRegionFadesAction");
        return passed;
    }

    //==========================================================================
    // Test 9: Mixer Volume Action
    //==========================================================================
    static bool testMixerVolumeAction()
    {
        DAWUndoManager undoManager;
        bool passed = true;

        AudioTrack track("Volume Test");
        float originalVolume = track.getVolume();

        // Change volume via action
        undoManager.perform(new TrackVolumeAction(&track, 0.5f), "Set Volume");
        passed &= (std::abs(track.getVolume() - 0.5f) < 0.001f);

        // Undo
        undoManager.undo();
        passed &= (std::abs(track.getVolume() - originalVolume) < 0.001f);

        // Redo
        undoManager.redo();
        passed &= (std::abs(track.getVolume() - 0.5f) < 0.001f);

        juce::Logger::writeToLog(passed ? "PASS: testMixerVolumeAction" : "FAIL: testMixerVolumeAction");
        return passed;
    }

    //==========================================================================
    // Test 10: Mixer Pan Action
    //==========================================================================
    static bool testMixerPanAction()
    {
        DAWUndoManager undoManager;
        bool passed = true;

        AudioTrack track("Pan Test");
        float originalPan = track.getPan();

        // Change pan via action
        undoManager.perform(new TrackPanAction(&track, -0.7f), "Set Pan");
        passed &= (std::abs(track.getPan() - (-0.7f)) < 0.001f);

        // Undo
        undoManager.undo();
        passed &= (std::abs(track.getPan() - originalPan) < 0.001f);

        // Redo
        undoManager.redo();
        passed &= (std::abs(track.getPan() - (-0.7f)) < 0.001f);

        juce::Logger::writeToLog(passed ? "PASS: testMixerPanAction" : "FAIL: testMixerPanAction");
        return passed;
    }

    //==========================================================================
    // Test 11: Mixer Mute Action
    //==========================================================================
    static bool testMixerMuteAction()
    {
        DAWUndoManager undoManager;
        bool passed = true;

        AudioTrack track("Mute Test");
        passed &= (track.isMuted() == false);

        // Mute via action
        undoManager.perform(new TrackMuteAction(&track, true), "Mute Track");
        passed &= (track.isMuted() == true);

        // Undo
        undoManager.undo();
        passed &= (track.isMuted() == false);

        // Redo
        undoManager.redo();
        passed &= (track.isMuted() == true);

        juce::Logger::writeToLog(passed ? "PASS: testMixerMuteAction" : "FAIL: testMixerMuteAction");
        return passed;
    }

    //==========================================================================
    // Test 12: Transaction Grouping
    //==========================================================================
    static bool testTransactionGrouping()
    {
        DAWUndoManager undoManager;
        bool passed = true;

        auto region = createTestRegion(0, 4410, "Group Test");
        AudioRegion* regionPtr = region.get();

        // Begin transaction to group multiple actions
        undoManager.beginNewTransaction("Move and Resize");

        // Both actions in same transaction
        undoManager.perform(new MoveRegionAction(regionPtr, 1000), "");
        undoManager.perform(new ResizeRegionAction(regionPtr, 2000), "");

        passed &= (regionPtr->getStartPosition() == 1000);
        passed &= (regionPtr->getLength() == 2000);

        // Single undo should revert both
        undoManager.undo();
        passed &= (regionPtr->getStartPosition() == 0);
        passed &= (regionPtr->getLength() == 4410);

        // Single redo should restore both
        undoManager.redo();
        passed &= (regionPtr->getStartPosition() == 1000);
        passed &= (regionPtr->getLength() == 2000);

        juce::Logger::writeToLog(passed ? "PASS: testTransactionGrouping" : "FAIL: testTransactionGrouping");
        return passed;
    }

    //==========================================================================
    // Test 13: Multiple Undo/Redo Operations
    //==========================================================================
    static bool testMultipleUndoRedo()
    {
        DAWUndoManager undoManager;
        bool passed = true;

        auto region = createTestRegion(0, 4410, "Multi Test");
        AudioRegion* regionPtr = region.get();

        // Perform 5 moves
        for (int i = 1; i <= 5; ++i)
        {
            undoManager.beginNewTransaction(juce::String("Move ") + juce::String(i));
            undoManager.perform(new MoveRegionAction(regionPtr, i * 100), "");
        }

        passed &= (regionPtr->getStartPosition() == 500);

        // Undo multiple (3 times)
        int undone = undoManager.undoMultiple(3);
        passed &= (undone == 3);
        passed &= (regionPtr->getStartPosition() == 200);

        // Redo multiple (2 times)
        int redone = undoManager.redoMultiple(2);
        passed &= (redone == 2);
        passed &= (regionPtr->getStartPosition() == 400);

        juce::Logger::writeToLog(passed ? "PASS: testMultipleUndoRedo" : "FAIL: testMultipleUndoRedo");
        return passed;
    }

    //==========================================================================
    // Test 14: History Management
    //==========================================================================
    static bool testHistoryManagement()
    {
        DAWUndoManager undoManager;
        bool passed = true;

        // Default should be unlimited history
        passed &= (undoManager.isUnlimitedHistory() == true);
        passed &= (undoManager.getHistoryLimit() == 0);

        Timeline timeline;

        // Add several tracks to build up history
        for (int i = 0; i < 10; ++i)
        {
            auto* track = new AudioTrack(juce::String("Track ") + juce::String(i));
            undoManager.beginNewTransaction(juce::String("Add Track ") + juce::String(i));
            undoManager.perform(new AddTrackAction(timeline, track), "");
        }

        passed &= (timeline.getNumTracks() == 10);
        passed &= (undoManager.canUndo() == true);

        // Clear history
        undoManager.clearUndoHistory();
        passed &= (undoManager.canUndo() == false);
        passed &= (undoManager.canRedo() == false);

        // Tracks should still exist (clear doesn't revert)
        passed &= (timeline.getNumTracks() == 10);

        // Test setting history limit
        undoManager.setHistoryLimit(50, 10);
        passed &= (undoManager.getHistoryLimit() == 50);
        passed &= (undoManager.isUnlimitedHistory() == false);

        // Reset to unlimited
        undoManager.setHistoryLimit(0);
        passed &= (undoManager.isUnlimitedHistory() == true);

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testHistoryManagement" : "FAIL: testHistoryManagement");
        return passed;
    }

    //==========================================================================
    // Test 15: Listener Notifications
    //==========================================================================
    static bool testListenerNotifications()
    {
        DAWUndoManager undoManager;
        bool passed = true;

        // Test listener for tracking notifications
        class TestListener : public DAWUndoManager::Listener
        {
        public:
            int undoCount = 0;
            int redoCount = 0;
            int actionCount = 0;
            int clearCount = 0;
            juce::String lastActionName;

            void undoPerformed() override { undoCount++; }
            void redoPerformed() override { redoCount++; }
            void actionPerformed(const juce::String& name) override
            {
                actionCount++;
                lastActionName = name;
            }
            void historyCleared() override { clearCount++; }
        };

        TestListener listener;
        undoManager.addListener(&listener);

        auto region = createTestRegion(0, 4410, "Listener Test");
        AudioRegion* regionPtr = region.get();

        // Perform action with notification
        undoManager.performWithNotification(new MoveRegionAction(regionPtr, 100), "Move");
        passed &= (listener.actionCount == 1);
        passed &= (listener.lastActionName == "Move");

        // Undo with notification
        undoManager.undoWithNotification();
        passed &= (listener.undoCount == 1);

        // Redo with notification
        undoManager.redoWithNotification();
        passed &= (listener.redoCount == 1);

        // Clear with notification
        undoManager.clearUndoHistoryWithNotification();
        passed &= (listener.clearCount == 1);

        undoManager.removeListener(&listener);

        // Actions after removing listener shouldn't increment counts
        undoManager.performWithNotification(new MoveRegionAction(regionPtr, 200), "Move 2");
        passed &= (listener.actionCount == 1);  // Should still be 1

        juce::Logger::writeToLog(passed ? "PASS: testListenerNotifications" : "FAIL: testListenerNotifications");
        return passed;
    }
};
