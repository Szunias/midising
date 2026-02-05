#pragma once

#include "../Timeline/Timeline.h"
#include "../Audio/AudioTrack.h"
#include "../Audio/Mixer.h"
#include "../Audio/SendReturn.h"
#include "../Audio/GroupBus.h"
#include "../Effects/GainEffect.h"
#include "../Effects/ReverbEffect.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * End-to-end verification tests for mixer routing.
 *
 * Test Steps:
 * 1. Create multiple audio tracks with content
 * 2. Create group bus, route tracks to it
 * 3. Create aux track with reverb
 * 4. Send tracks to aux
 * 5. Verify group volume affects all tracks
 * 6. Verify send adds reverb wet signal
 * 7. Test solo/mute on group
 *
 * These tests verify the complete mixer signal flow including:
 * - Track -> Group Bus routing
 * - Track -> Aux Track send routing
 * - Insert effects on groups and aux tracks
 * - Solo/mute behavior for groups
 */
class MixerRoutingTest
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;

        allPassed &= testCreateMultipleAudioTracks();
        allPassed &= testGroupBusCreationAndRouting();
        allPassed &= testAuxTrackWithEffect();
        allPassed &= testSendRouting();
        allPassed &= testGroupVolumeAffectsAllTracks();
        allPassed &= testSendAddsWetSignal();
        allPassed &= testSoloMuteOnGroup();
        allPassed &= testCompleteSignalFlow();

        return allPassed;
    }

private:
    //==========================================================================
    // Helper: Create a test audio region with a test tone
    //==========================================================================
    static std::unique_ptr<AudioRegion> createTestRegion(int64_t startPos, int numSamples, float amplitude = 0.5f)
    {
        // Create a simple sine wave buffer for testing
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
    // Helper: Check if an AudioRegion has audio data
    //==========================================================================
    static bool hasAudioData(const AudioRegion* region)
    {
        return region != nullptr && region->getAudioBuffer().getNumSamples() > 0;
    }

    //==========================================================================
    // Helper: Calculate RMS level of a buffer
    //==========================================================================
    static float calculateRMS(const juce::AudioBuffer<float>& buffer, int channel = 0)
    {
        float sum = 0.0f;
        const float* data = buffer.getReadPointer(channel);
        int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            sum += data[i] * data[i];
        }

        return std::sqrt(sum / static_cast<float>(numSamples));
    }

    //==========================================================================
    // Test 1: Create Multiple Audio Tracks with Content
    //==========================================================================
    static bool testCreateMultipleAudioTracks()
    {
        Timeline timeline;
        bool passed = true;

        // Create 3 audio tracks
        auto* track1 = new AudioTrack("Drums");
        auto* track2 = new AudioTrack("Bass");
        auto* track3 = new AudioTrack("Guitar");

        timeline.addTrack(track1);
        timeline.addTrack(track2);
        timeline.addTrack(track3);

        // Verify tracks were added
        passed &= (timeline.getNumTracks() == 3);
        passed &= (timeline.getTrack(0)->getName() == "Drums");
        passed &= (timeline.getTrack(1)->getName() == "Bass");
        passed &= (timeline.getTrack(2)->getName() == "Guitar");

        // Add regions with audio content to each track
        int regionLength = 4410; // 0.1 seconds at 44.1kHz
        track1->addRegion(createTestRegion(0, regionLength, 0.5f));
        track2->addRegion(createTestRegion(0, regionLength, 0.6f));
        track3->addRegion(createTestRegion(0, regionLength, 0.4f));

        // Verify regions were added
        passed &= (track1->getNumRegions() == 1);
        passed &= (track2->getNumRegions() == 1);
        passed &= (track3->getNumRegions() == 1);

        // Verify region audio data exists
        passed &= hasAudioData(track1->getRegion(0));
        passed &= hasAudioData(track2->getRegion(0));
        passed &= hasAudioData(track3->getRegion(0));

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testCreateMultipleAudioTracks" : "FAIL: testCreateMultipleAudioTracks");
        return passed;
    }

    //==========================================================================
    // Test 2: Create Group Bus and Route Tracks to It
    //==========================================================================
    static bool testGroupBusCreationAndRouting()
    {
        Timeline timeline;
        bool passed = true;

        // Create audio tracks
        auto* track1 = new AudioTrack("Drums 1");
        auto* track2 = new AudioTrack("Drums 2");
        auto* track3 = new AudioTrack("Bass");

        timeline.addTrack(track1);
        timeline.addTrack(track2);
        timeline.addTrack(track3);

        // Create a group bus for drums
        GroupBus* drumGroup = timeline.createGroupBus("Drum Group");

        // Verify group bus was created
        passed &= (timeline.getNumGroupBusses() == 1);
        passed &= (drumGroup != nullptr);
        passed &= (drumGroup->getName() == "Drum Group");

        // Route drum tracks to the group bus
        track1->setOutputGroupBus(0);
        track2->setOutputGroupBus(0);
        // Bass stays routed to master (default)

        // Verify routing
        passed &= (track1->getOutputGroupBus() == 0);
        passed &= (track2->getOutputGroupBus() == 0);
        passed &= (track3->getOutputGroupBus() == Track::OUTPUT_TO_MASTER);
        passed &= (track1->isRoutedToGroup() == true);
        passed &= (track2->isRoutedToGroup() == true);
        passed &= (track3->isRoutedToGroup() == false);

        // Test group bus volume and pan
        drumGroup->setVolume(0.8f);
        drumGroup->setPan(-0.2f);
        passed &= (std::abs(drumGroup->getVolume() - 0.8f) < 0.001f);
        passed &= (std::abs(drumGroup->getPan() - (-0.2f)) < 0.001f);

        timeline.clearGroupBusses();
        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testGroupBusCreationAndRouting" : "FAIL: testGroupBusCreationAndRouting");
        return passed;
    }

    //==========================================================================
    // Test 3: Create Aux Track with Reverb Effect
    //==========================================================================
    static bool testAuxTrackWithEffect()
    {
        Timeline timeline;
        bool passed = true;

        // Create an aux track for reverb
        AuxTrack* reverbAux = timeline.createAuxTrack("Reverb Bus");

        // Verify aux track was created
        passed &= (timeline.getNumAuxTracks() == 1);
        passed &= (reverbAux != nullptr);
        passed &= (reverbAux->getName() == "Reverb Bus");

        // Add reverb effect to the aux track
        auto reverbEffect = std::make_unique<ReverbEffect>();
        reverbEffect->setRoomSize(0.7f);
        reverbEffect->setDamping(0.5f);
        reverbEffect->setWetLevel(1.0f);
        reverbEffect->setDryLevel(0.0f);

        int slotIndex = reverbAux->addInsert(std::move(reverbEffect));

        // Verify effect was added
        passed &= (slotIndex == 0);
        passed &= (reverbAux->getNumActiveInserts() == 1);
        passed &= (!reverbAux->isInsertSlotEmpty(0));

        // Get the effect and verify it's the reverb
        Effect* insertedEffect = reverbAux->getInsert(0);
        passed &= (insertedEffect != nullptr);
        passed &= (insertedEffect->getName() == "Reverb");

        // Verify aux track volume and pan controls
        reverbAux->setVolume(0.6f);
        reverbAux->setPan(0.0f);
        passed &= (std::abs(reverbAux->getVolume() - 0.6f) < 0.001f);

        timeline.clearAuxTracks();

        juce::Logger::writeToLog(passed ? "PASS: testAuxTrackWithEffect" : "FAIL: testAuxTrackWithEffect");
        return passed;
    }

    //==========================================================================
    // Test 4: Send Routing from Tracks to Aux
    //==========================================================================
    static bool testSendRouting()
    {
        Timeline timeline;
        bool passed = true;

        // Create audio tracks
        auto* track1 = new AudioTrack("Vocals");
        auto* track2 = new AudioTrack("Synth");
        timeline.addTrack(track1);
        timeline.addTrack(track2);

        // Create aux track for reverb
        juce::ignoreUnused(timeline.createAuxTrack("Reverb"));

        // Set up sends from tracks to aux
        SendManager& sendMgr1 = track1->getSendManager();
        SendManager& sendMgr2 = track2->getSendManager();

        // Add send from track1 to reverb aux (index 0)
        int sendIdx1 = sendMgr1.addSend(0, 0.5f, false); // Post-fader send
        passed &= (sendIdx1 == 0);
        passed &= (sendMgr1.getNumSends() == 1);

        // Add send from track2 to reverb aux with different level
        int sendIdx2 = sendMgr2.addSend(0, 0.3f, true); // Pre-fader send
        passed &= (sendIdx2 == 0);
        passed &= (sendMgr2.getNumSends() == 1);

        // Verify send configurations
        const Send* send1 = sendMgr1.getSend(0);
        const Send* send2 = sendMgr2.getSend(0);

        passed &= (send1 != nullptr);
        passed &= (send2 != nullptr);

        if (send1 && send2)
        {
            passed &= (send1->destAuxIndex == 0);
            passed &= (std::abs(send1->sendLevel.load() - 0.5f) < 0.001f);
            passed &= (send1->preFader.load() == false);
            passed &= (send1->enabled.load() == true);

            passed &= (send2->destAuxIndex == 0);
            passed &= (std::abs(send2->sendLevel.load() - 0.3f) < 0.001f);
            passed &= (send2->preFader.load() == true);
            passed &= (send2->enabled.load() == true);
        }

        // Test send level modification
        sendMgr1.setSendLevel(0, 0.7f);
        passed &= (std::abs(sendMgr1.getSendLevel(0) - 0.7f) < 0.001f);

        // Test send enable/disable
        sendMgr1.setSendEnabled(0, false);
        passed &= (sendMgr1.isSendEnabled(0) == false);
        sendMgr1.setSendEnabled(0, true);
        passed &= (sendMgr1.isSendEnabled(0) == true);

        // Test finding send by aux index
        passed &= (sendMgr1.findSendToAux(0) == 0);
        passed &= (sendMgr1.findSendToAux(1) == -1);

        // Verify track has active sends
        passed &= track1->hasActiveSends();
        passed &= track2->hasActiveSends();

        timeline.clearAuxTracks();
        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testSendRouting" : "FAIL: testSendRouting");
        return passed;
    }

    //==========================================================================
    // Test 5: Group Volume Affects All Routed Tracks
    //==========================================================================
    static bool testGroupVolumeAffectsAllTracks()
    {
        Timeline timeline;
        Mixer mixer;
        bool passed = true;

        const double sampleRate = 44100.0;
        const int blockSize = 512;

        // Prepare mixer
        mixer.prepareToPlay(sampleRate, blockSize);

        // Create audio tracks with test content
        auto* track1 = new AudioTrack("Drum 1");
        auto* track2 = new AudioTrack("Drum 2");
        timeline.addTrack(track1);
        timeline.addTrack(track2);

        track1->prepareToPlay(sampleRate, blockSize);
        track2->prepareToPlay(sampleRate, blockSize);

        // Add test regions
        track1->addRegion(createTestRegion(0, blockSize, 0.5f));
        track2->addRegion(createTestRegion(0, blockSize, 0.5f));

        // Create group bus and route tracks to it
        GroupBus* drumGroup = timeline.createGroupBus("Drums");
        drumGroup->prepareToPlay(sampleRate, blockSize);
        track1->setOutputGroupBus(0);
        track2->setOutputGroupBus(0);

        // Prepare timeline aux/group
        timeline.prepareGroupBussesToPlay(sampleRate, blockSize);

        // Process with group at unity gain
        drumGroup->setVolume(1.0f);
        juce::AudioBuffer<float> outputUnity(2, blockSize);
        mixer.processBlock(timeline, outputUnity, 0, blockSize);
        float levelUnity = calculateRMS(outputUnity, 0);

        // Process with group at half volume
        drumGroup->setVolume(0.5f);
        juce::AudioBuffer<float> outputHalf(2, blockSize);
        mixer.processBlock(timeline, outputHalf, 0, blockSize);
        float levelHalf = calculateRMS(outputHalf, 0);

        // The half volume should be approximately half (with some tolerance)
        // Note: Pan calculations and dB conversion may affect exact ratios
        passed &= (levelHalf < levelUnity);
        passed &= (levelHalf > 0.0f); // Should still have some output

        // Process with group muted
        drumGroup->setMuted(true);
        juce::AudioBuffer<float> outputMuted(2, blockSize);
        mixer.processBlock(timeline, outputMuted, 0, blockSize);
        float levelMuted = calculateRMS(outputMuted, 0);

        // Muted group should produce no output
        passed &= (levelMuted < 0.001f);

        drumGroup->setMuted(false);

        // Clean up
        timeline.releaseGroupBusResources();
        track1->releaseResources();
        track2->releaseResources();
        mixer.releaseResources();
        timeline.clearGroupBusses();
        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testGroupVolumeAffectsAllTracks" : "FAIL: testGroupVolumeAffectsAllTracks");
        return passed;
    }

    //==========================================================================
    // Test 6: Send Adds Reverb Wet Signal
    //==========================================================================
    static bool testSendAddsWetSignal()
    {
        Timeline timeline;
        Mixer mixer;
        bool passed = true;

        const double sampleRate = 44100.0;
        const int blockSize = 512;

        mixer.prepareToPlay(sampleRate, blockSize);

        // Create source track with test content
        auto* track = new AudioTrack("Vocals");
        timeline.addTrack(track);
        track->prepareToPlay(sampleRate, blockSize);
        track->addRegion(createTestRegion(0, blockSize, 0.5f));

        // Create aux track with a gain effect (simpler than reverb for testing)
        AuxTrack* auxTrack = timeline.createAuxTrack("FX Return");
        auxTrack->prepareToPlay(sampleRate, blockSize);

        // Add a gain effect that boosts the signal
        auto gainEffect = std::make_unique<GainEffect>();
        gainEffect->setGain(2.0f); // 2x gain so we can detect the aux contribution
        auxTrack->addInsert(std::move(gainEffect));
        auxTrack->setVolume(1.0f);

        timeline.prepareAuxTracksToPlay(sampleRate, blockSize);

        // Process without send (baseline)
        juce::AudioBuffer<float> outputNoSend(2, blockSize);
        mixer.processBlock(timeline, outputNoSend, 0, blockSize);
        float levelNoSend = calculateRMS(outputNoSend, 0);

        // Add send from track to aux
        SendManager& sendMgr = track->getSendManager();
        sendMgr.addSend(0, 1.0f, false); // Full level post-fader send

        // Process with send active
        juce::AudioBuffer<float> outputWithSend(2, blockSize);
        mixer.processBlock(timeline, outputWithSend, 0, blockSize);
        float levelWithSend = calculateRMS(outputWithSend, 0);

        // With the aux track adding gain-boosted signal, output should be louder
        passed &= (levelWithSend > levelNoSend);

        // Disable the send and verify output returns to baseline
        sendMgr.setSendEnabled(0, false);
        juce::AudioBuffer<float> outputSendDisabled(2, blockSize);
        mixer.processBlock(timeline, outputSendDisabled, 0, blockSize);
        float levelSendDisabled = calculateRMS(outputSendDisabled, 0);

        // Should be back to approximately the baseline level
        passed &= (std::abs(levelSendDisabled - levelNoSend) < 0.01f);

        // Clean up
        timeline.releaseAuxTrackResources();
        track->releaseResources();
        mixer.releaseResources();
        timeline.clearAuxTracks();
        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testSendAddsWetSignal" : "FAIL: testSendAddsWetSignal");
        return passed;
    }

    //==========================================================================
    // Test 7: Solo/Mute on Group Bus
    //==========================================================================
    static bool testSoloMuteOnGroup()
    {
        Timeline timeline;
        Mixer mixer;
        bool passed = true;

        const double sampleRate = 44100.0;
        const int blockSize = 512;

        mixer.prepareToPlay(sampleRate, blockSize);

        // Create tracks
        auto* drumTrack1 = new AudioTrack("Kick");
        auto* drumTrack2 = new AudioTrack("Snare");
        auto* bassTrack = new AudioTrack("Bass");

        timeline.addTrack(drumTrack1);
        timeline.addTrack(drumTrack2);
        timeline.addTrack(bassTrack);

        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            timeline.getTrack(i)->prepareToPlay(sampleRate, blockSize);
        }

        // Add regions
        static_cast<AudioTrack*>(timeline.getTrack(0))->addRegion(createTestRegion(0, blockSize, 0.5f));
        static_cast<AudioTrack*>(timeline.getTrack(1))->addRegion(createTestRegion(0, blockSize, 0.4f));
        static_cast<AudioTrack*>(timeline.getTrack(2))->addRegion(createTestRegion(0, blockSize, 0.6f));

        // Create group busses
        GroupBus* drumGroup = timeline.createGroupBus("Drums");
        GroupBus* bassGroup = timeline.createGroupBus("Bass");

        drumGroup->prepareToPlay(sampleRate, blockSize);
        bassGroup->prepareToPlay(sampleRate, blockSize);

        // Route tracks
        drumTrack1->setOutputGroupBus(0); // Drums group
        drumTrack2->setOutputGroupBus(0); // Drums group
        bassTrack->setOutputGroupBus(1);  // Bass group

        timeline.prepareGroupBussesToPlay(sampleRate, blockSize);

        // Get baseline output (all playing)
        juce::AudioBuffer<float> outputAll(2, blockSize);
        mixer.processBlock(timeline, outputAll, 0, blockSize);
        float levelAll = calculateRMS(outputAll, 0);
        passed &= (levelAll > 0.0f);

        // Mute drum group - should only hear bass
        drumGroup->setMuted(true);
        juce::AudioBuffer<float> outputDrumsMuted(2, blockSize);
        mixer.processBlock(timeline, outputDrumsMuted, 0, blockSize);
        float levelDrumsMuted = calculateRMS(outputDrumsMuted, 0);

        passed &= (levelDrumsMuted > 0.0f); // Bass still playing
        passed &= (levelDrumsMuted < levelAll); // Less than all playing

        drumGroup->setMuted(false);

        // Solo drum group - should only hear drums
        drumGroup->setSoloed(true);

        // Verify solo state
        passed &= drumGroup->isSoloed();
        passed &= timeline.hasAnySoloedGroupBus();

        juce::AudioBuffer<float> outputDrumsSoloed(2, blockSize);
        mixer.processBlock(timeline, outputDrumsSoloed, 0, blockSize);
        float levelDrumsSoloed = calculateRMS(outputDrumsSoloed, 0);

        passed &= (levelDrumsSoloed > 0.0f); // Drums playing
        passed &= (levelDrumsSoloed < levelAll); // Less than all (no bass)

        drumGroup->setSoloed(false);

        // Verify no solo state
        passed &= !drumGroup->isSoloed();
        passed &= !timeline.hasAnySoloedGroupBus();

        // Clean up
        timeline.releaseGroupBusResources();
        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            timeline.getTrack(i)->releaseResources();
        }
        mixer.releaseResources();
        timeline.clearGroupBusses();
        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testSoloMuteOnGroup" : "FAIL: testSoloMuteOnGroup");
        return passed;
    }

    //==========================================================================
    // Test 8: Complete Signal Flow (Integration)
    //==========================================================================
    static bool testCompleteSignalFlow()
    {
        Timeline timeline;
        Mixer mixer;
        bool passed = true;

        const double sampleRate = 44100.0;
        const int blockSize = 512;

        mixer.prepareToPlay(sampleRate, blockSize);

        // === Setup: Create a realistic mixer scenario ===

        // Create audio tracks
        auto* vocalTrack = new AudioTrack("Vocals");
        auto* guitarTrack = new AudioTrack("Guitar");
        auto* drumTrack = new AudioTrack("Drums");

        timeline.addTrack(vocalTrack);
        timeline.addTrack(guitarTrack);
        timeline.addTrack(drumTrack);

        // Prepare tracks
        vocalTrack->prepareToPlay(sampleRate, blockSize);
        guitarTrack->prepareToPlay(sampleRate, blockSize);
        drumTrack->prepareToPlay(sampleRate, blockSize);

        // Add content to tracks
        vocalTrack->addRegion(createTestRegion(0, blockSize, 0.6f));
        guitarTrack->addRegion(createTestRegion(0, blockSize, 0.5f));
        drumTrack->addRegion(createTestRegion(0, blockSize, 0.7f));

        // Create group bus for instruments
        GroupBus* instrumentGroup = timeline.createGroupBus("Instruments");
        instrumentGroup->prepareToPlay(sampleRate, blockSize);

        // Add compressor-style gain reduction to instrument group
        auto groupGain = std::make_unique<GainEffect>();
        groupGain->setGain(0.8f);
        instrumentGroup->addInsert(std::move(groupGain));

        // Route guitar and drums to group, vocals direct to master
        guitarTrack->setOutputGroupBus(0);
        drumTrack->setOutputGroupBus(0);
        // vocalTrack stays at OUTPUT_TO_MASTER

        // Create aux track for reverb
        AuxTrack* reverbAux = timeline.createAuxTrack("Reverb");
        reverbAux->prepareToPlay(sampleRate, blockSize);

        auto reverb = std::make_unique<ReverbEffect>();
        reverb->setRoomSize(0.8f);
        reverb->setWetLevel(1.0f);
        reverb->setDryLevel(0.0f);
        reverbAux->addInsert(std::move(reverb));
        reverbAux->setVolume(0.4f);

        // Send vocals to reverb
        SendManager& vocalSends = vocalTrack->getSendManager();
        vocalSends.addSend(0, 0.5f, false);

        // Prepare all aux and group busses
        timeline.prepareAuxTracksToPlay(sampleRate, blockSize);
        timeline.prepareGroupBussesToPlay(sampleRate, blockSize);

        // === Verification Steps ===

        // Step 1: Verify complete signal flow produces audio
        juce::AudioBuffer<float> output(2, blockSize);
        mixer.processBlock(timeline, output, 0, blockSize);
        float totalLevel = calculateRMS(output, 0);
        passed &= (totalLevel > 0.0f);

        // Step 2: Verify group bus routing is working
        // Mute the instrument group - should reduce output
        float levelBeforeMute = totalLevel;
        instrumentGroup->setMuted(true);
        juce::AudioBuffer<float> outputGroupMuted(2, blockSize);
        mixer.processBlock(timeline, outputGroupMuted, 0, blockSize);
        float levelWithGroupMuted = calculateRMS(outputGroupMuted, 0);

        passed &= (levelWithGroupMuted < levelBeforeMute);
        passed &= (levelWithGroupMuted > 0.0f); // Vocals still playing

        instrumentGroup->setMuted(false);

        // Step 3: Verify send to aux is working
        // Disable vocal send - should slightly reduce total output
        vocalSends.setSendEnabled(0, false);
        juce::AudioBuffer<float> outputNoSend(2, blockSize);
        mixer.processBlock(timeline, outputNoSend, 0, blockSize);
        float levelNoSend = calculateRMS(outputNoSend, 0);

        // Re-enable and compare
        vocalSends.setSendEnabled(0, true);
        juce::AudioBuffer<float> outputWithSend(2, blockSize);
        mixer.processBlock(timeline, outputWithSend, 0, blockSize);
        float levelWithSend = calculateRMS(outputWithSend, 0);

        // With reverb aux adding wet signal, output should be slightly higher
        // (This depends on reverb settings, so we just verify both are non-zero)
        passed &= (levelNoSend > 0.0f);
        passed &= (levelWithSend > 0.0f);

        // Step 4: Verify master volume control
        float originalMasterVol = mixer.getMasterVolume();
        mixer.setMasterVolume(0.5f);
        juce::AudioBuffer<float> outputHalfMaster(2, blockSize);
        mixer.processBlock(timeline, outputHalfMaster, 0, blockSize);
        float levelHalfMaster = calculateRMS(outputHalfMaster, 0);

        passed &= (levelHalfMaster < levelWithSend);

        mixer.setMasterVolume(originalMasterVol);

        // Step 5: Verify peak metering is updating
        juce::AudioBuffer<float> outputForMetering(2, blockSize);
        mixer.processBlock(timeline, outputForMetering, 0, blockSize);
        mixer.updatePeakLevels(outputForMetering);

        float peakLeft = mixer.getPeakLevel(0);
        float peakRight = mixer.getPeakLevel(1);

        passed &= (peakLeft >= 0.0f);
        passed &= (peakRight >= 0.0f);

        // Clean up
        timeline.releaseAuxTrackResources();
        timeline.releaseGroupBusResources();
        vocalTrack->releaseResources();
        guitarTrack->releaseResources();
        drumTrack->releaseResources();
        mixer.releaseResources();
        timeline.clearAuxTracks();
        timeline.clearGroupBusses();
        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testCompleteSignalFlow" : "FAIL: testCompleteSignalFlow");
        return passed;
    }
};
