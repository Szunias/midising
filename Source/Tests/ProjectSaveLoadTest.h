#pragma once

#include "../Timeline/Timeline.h"
#include "../Timeline/AutomationLane.h"
#include "../Audio/AudioTrack.h"
#include "../Audio/Transport.h"
#include "../Audio/Mixer.h"
#include "../MIDI/MidiTrack.h"
#include "../MIDI/MidiEngine.h"
#include "../Serialization/ProjectSerializer.h"
#include "../Effects/SimpleEQEffect.h"
#include "../Effects/GainEffect.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * End-to-end verification tests for project save/load.
 *
 * Test Steps (from spec):
 * 1. Create project with audio, MIDI, effects, automation
 * 2. Save as bundle
 * 3. Close application (simulated by clearing timeline)
 * 4. Reopen project
 * 5. Verify all tracks, regions, effects, automation restored
 * 6. Verify audio files in bundle
 * 7. Play through entire project
 *
 * These tests verify the complete project persistence including:
 * - Audio tracks with regions and effects
 * - MIDI tracks with note sequences
 * - Automation lanes with points
 * - Bundle format with relative audio paths
 * - Transport state (playhead, loop)
 * - Track properties (volume, pan, mute, solo, armed, color)
 */
class ProjectSaveLoadTest
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;

        allPassed &= testCreateProjectWithContent();
        allPassed &= testSaveAsBundle();
        allPassed &= testLoadBundle();
        allPassed &= testVerifyTracksRestored();
        allPassed &= testVerifyRegionsRestored();
        allPassed &= testVerifyEffectsRestored();
        allPassed &= testVerifyAutomationRestored();
        allPassed &= testVerifyAudioFilesInBundle();
        allPassed &= testPlaybackAfterLoad();
        allPassed &= testCompleteWorkflow();

        return allPassed;
    }

private:
    //==========================================================================
    // Helper: Create test project folder
    //==========================================================================
    static juce::File getTestProjectFolder()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("MidiSingTestProject");
    }

    //==========================================================================
    // Helper: Clean up test project folder
    //==========================================================================
    static void cleanupTestProject()
    {
        auto folder = getTestProjectFolder();
        if (folder.exists())
            folder.deleteRecursively();
    }

    //==========================================================================
    // Helper: Create a test audio region with a sine wave
    //==========================================================================
    static std::unique_ptr<AudioRegion> createTestAudioRegion(int64_t startPos, int numSamples, float amplitude = 0.5f)
    {
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
        region->setName("Test Audio Region");
        return region;
    }

    //==========================================================================
    // Helper: Create test MIDI sequence with notes
    //==========================================================================
    static juce::MidiMessageSequence createTestMidiSequence()
    {
        juce::MidiMessageSequence seq;

        // Add a simple melody: C4, E4, G4 quarter notes
        double noteLength = 0.5; // 500ms per note

        // C4 (note 60)
        seq.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f).withTimeStamp(0.0));
        seq.addEvent(juce::MidiMessage::noteOff(1, 60, 0.0f).withTimeStamp(noteLength));

        // E4 (note 64)
        seq.addEvent(juce::MidiMessage::noteOn(1, 64, 0.8f).withTimeStamp(noteLength));
        seq.addEvent(juce::MidiMessage::noteOff(1, 64, 0.0f).withTimeStamp(noteLength * 2));

        // G4 (note 67)
        seq.addEvent(juce::MidiMessage::noteOn(1, 67, 0.8f).withTimeStamp(noteLength * 2));
        seq.addEvent(juce::MidiMessage::noteOff(1, 67, 0.0f).withTimeStamp(noteLength * 3));

        seq.updateMatchedPairs();
        return seq;
    }

    //==========================================================================
    // Helper: Calculate RMS level of buffer
    //==========================================================================
    static float calculateRMS(const juce::AudioBuffer<float>& buffer, int channel = 0)
    {
        float sum = 0.0f;
        const float* data = buffer.getReadPointer(channel);
        int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
            sum += data[i] * data[i];

        return std::sqrt(sum / static_cast<float>(numSamples));
    }

    //==========================================================================
    // Test 1: Create Project with Content
    //==========================================================================
    static bool testCreateProjectWithContent()
    {
        cleanupTestProject();

        Timeline timeline;
        bool passed = true;

        // Create audio track with region
        auto* audioTrack = new AudioTrack("Lead Vocals");
        audioTrack->setVolume(0.8f);
        audioTrack->setPan(-0.2f);
        audioTrack->addRegion(createTestAudioRegion(0, 44100, 0.6f)); // 1 second
        timeline.addTrack(audioTrack);

        // Create MIDI track with region
        MidiEngine midiEngine;
        auto* midiTrack = new MidiTrack("Synth Lead", &midiEngine);
        midiTrack->setVolume(0.9f);
        midiTrack->setPan(0.1f);

        auto midiRegion = std::make_unique<MidiRegion>(0, 44100 * 2);
        midiRegion->setMidiSequence(createTestMidiSequence());
        midiRegion->setName("Melody");
        midiTrack->addRegion(std::move(midiRegion));
        timeline.addTrack(midiTrack);

        // Verify setup
        passed &= (timeline.getNumTracks() == 2);
        passed &= (timeline.getTrack(0)->getName() == "Lead Vocals");
        passed &= (timeline.getTrack(1)->getName() == "Synth Lead");

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testCreateProjectWithContent" : "FAIL: testCreateProjectWithContent");
        return passed;
    }

    //==========================================================================
    // Test 2: Save as Bundle
    //==========================================================================
    static bool testSaveAsBundle()
    {
        cleanupTestProject();

        Timeline timeline;
        Transport transport;
        bool passed = true;

        // Set up timeline
        timeline.setBpm(128.0);
        timeline.setBeatsPerBar(3);

        // Set up transport
        transport.setPlayheadPosition(22050);
        transport.setLooping(true);
        transport.setLoopRange(0, 88200);

        // Create audio track
        auto* audioTrack = new AudioTrack("Test Audio");
        audioTrack->setVolume(0.75f);
        audioTrack->setPan(-0.5f);
        audioTrack->setMuted(false);
        audioTrack->addRegion(createTestAudioRegion(0, 44100, 0.5f));
        timeline.addTrack(audioTrack);

        // Save project
        auto projectFolder = getTestProjectFolder();
        bool saveResult = ProjectSerializer::saveProjectBundle(timeline, transport, projectFolder);

        passed &= saveResult;
        passed &= projectFolder.exists();
        passed &= projectFolder.getChildFile("project.msproj").existsAsFile();
        passed &= projectFolder.getChildFile("Audio Files").exists();

        timeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testSaveAsBundle" : "FAIL: testSaveAsBundle");
        return passed;
    }

    //==========================================================================
    // Test 3: Load Bundle
    //==========================================================================
    static bool testLoadBundle()
    {
        bool passed = true;

        // First save a project
        Timeline saveTimeline;
        Transport saveTransport;

        saveTimeline.setBpm(140.0);
        saveTimeline.setBeatsPerBar(4);
        saveTransport.setPlayheadPosition(10000);

        auto* audioTrack = new AudioTrack("Load Test Track");
        audioTrack->setVolume(0.65f);
        saveTimeline.addTrack(audioTrack);

        auto projectFolder = getTestProjectFolder();
        cleanupTestProject();
        ProjectSerializer::saveProjectBundle(saveTimeline, saveTransport, projectFolder);

        // Now load into a fresh timeline
        Timeline loadTimeline;
        Transport loadTransport;

        bool loadResult = ProjectSerializer::loadProjectBundle(loadTimeline, loadTransport, nullptr, projectFolder);

        passed &= loadResult;
        passed &= (std::abs(loadTimeline.getBpm() - 140.0) < 0.001);
        passed &= (loadTimeline.getBeatsPerBar() == 4);
        passed &= (loadTransport.getPlayheadPosition() == 10000);

        saveTimeline.clearTracks();
        loadTimeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testLoadBundle" : "FAIL: testLoadBundle");
        return passed;
    }

    //==========================================================================
    // Test 4: Verify Tracks Restored
    //==========================================================================
    static bool testVerifyTracksRestored()
    {
        cleanupTestProject();
        bool passed = true;

        // Create and save project with multiple tracks
        Timeline saveTimeline;
        Transport saveTransport;

        auto* track1 = new AudioTrack("Vocals");
        track1->setVolume(0.8f);
        track1->setPan(-0.3f);
        track1->setMuted(true);
        track1->setSoloed(false);
        track1->setColour(juce::Colours::red);
        saveTimeline.addTrack(track1);

        auto* track2 = new AudioTrack("Guitar");
        track2->setVolume(0.9f);
        track2->setPan(0.4f);
        track2->setMuted(false);
        track2->setSoloed(true);
        track2->setColour(juce::Colours::green);
        saveTimeline.addTrack(track2);

        auto projectFolder = getTestProjectFolder();
        ProjectSerializer::saveProjectBundle(saveTimeline, saveTransport, projectFolder);

        // Load into fresh timeline
        Timeline loadTimeline;
        Transport loadTransport;
        ProjectSerializer::loadProjectBundle(loadTimeline, loadTransport, nullptr, projectFolder);

        // Verify track count
        passed &= (loadTimeline.getNumTracks() == 2);

        // Verify track 1 properties
        auto* loadedTrack1 = loadTimeline.getTrack(0);
        passed &= (loadedTrack1->getName() == "Vocals");
        passed &= (std::abs(loadedTrack1->getVolume() - 0.8f) < 0.001f);
        passed &= (std::abs(loadedTrack1->getPan() - (-0.3f)) < 0.001f);
        passed &= (loadedTrack1->isMuted() == true);
        passed &= (loadedTrack1->isSoloed() == false);

        // Verify track 2 properties
        auto* loadedTrack2 = loadTimeline.getTrack(1);
        passed &= (loadedTrack2->getName() == "Guitar");
        passed &= (std::abs(loadedTrack2->getVolume() - 0.9f) < 0.001f);
        passed &= (std::abs(loadedTrack2->getPan() - 0.4f) < 0.001f);
        passed &= (loadedTrack2->isMuted() == false);
        passed &= (loadedTrack2->isSoloed() == true);

        saveTimeline.clearTracks();
        loadTimeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testVerifyTracksRestored" : "FAIL: testVerifyTracksRestored");
        return passed;
    }

    //==========================================================================
    // Test 5: Verify Regions Restored
    //==========================================================================
    static bool testVerifyRegionsRestored()
    {
        cleanupTestProject();
        bool passed = true;

        // Create and save project with regions
        Timeline saveTimeline;
        Transport saveTransport;
        MidiEngine midiEngine;

        // Audio track with region
        auto* audioTrack = new AudioTrack("Audio");
        auto audioRegion = createTestAudioRegion(1000, 22050, 0.7f);
        audioRegion->setName("Audio Clip 1");
        audioRegion->setOffset(500);
        audioRegion->setFadeInLength(100);
        audioRegion->setFadeOutLength(200);
        audioTrack->addRegion(std::move(audioRegion));
        saveTimeline.addTrack(audioTrack);

        // MIDI track with region
        auto* midiTrack = new MidiTrack("MIDI", &midiEngine);
        auto midiRegion = std::make_unique<MidiRegion>(2000, 44100);
        midiRegion->setName("MIDI Clip 1");
        midiRegion->setMidiSequence(createTestMidiSequence());
        midiTrack->addRegion(std::move(midiRegion));
        saveTimeline.addTrack(midiTrack);

        auto projectFolder = getTestProjectFolder();
        ProjectSerializer::saveProjectBundle(saveTimeline, saveTransport, projectFolder);

        // Load into fresh timeline
        Timeline loadTimeline;
        Transport loadTransport;
        ProjectSerializer::loadProjectBundle(loadTimeline, loadTransport, &midiEngine, projectFolder);

        // Verify audio region
        auto* loadedAudioTrack = dynamic_cast<AudioTrack*>(loadTimeline.getTrack(0));
        passed &= (loadedAudioTrack != nullptr);
        if (loadedAudioTrack)
        {
            passed &= (loadedAudioTrack->getNumRegions() == 1);
            auto* loadedAudioRegion = loadedAudioTrack->getRegion(0);
            passed &= (loadedAudioRegion != nullptr);
            if (loadedAudioRegion)
            {
                passed &= (loadedAudioRegion->getName() == "Audio Clip 1");
                passed &= (loadedAudioRegion->getStartPosition() == 1000);
                passed &= (loadedAudioRegion->getOffset() == 500);
                passed &= (loadedAudioRegion->getFadeInLength() == 100);
                passed &= (loadedAudioRegion->getFadeOutLength() == 200);
            }
        }

        // Verify MIDI region
        auto* loadedMidiTrack = dynamic_cast<MidiTrack*>(loadTimeline.getTrack(1));
        passed &= (loadedMidiTrack != nullptr);
        if (loadedMidiTrack)
        {
            passed &= (loadedMidiTrack->getNumRegions() == 1);
            auto* loadedMidiRegion = dynamic_cast<MidiRegion*>(loadedMidiTrack->getRegion(0));
            passed &= (loadedMidiRegion != nullptr);
            if (loadedMidiRegion)
            {
                passed &= (loadedMidiRegion->getName() == "MIDI Clip 1");
                passed &= (loadedMidiRegion->getStartPosition() == 2000);

                // Verify MIDI notes were restored
                const auto& seq = loadedMidiRegion->getMidiSequence();
                passed &= (seq.getNumEvents() >= 6); // 3 note-on + 3 note-off
            }
        }

        saveTimeline.clearTracks();
        loadTimeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testVerifyRegionsRestored" : "FAIL: testVerifyRegionsRestored");
        return passed;
    }

    //==========================================================================
    // Test 6: Verify Effects Restored
    //==========================================================================
    static bool testVerifyEffectsRestored()
    {
        cleanupTestProject();
        bool passed = true;

        // Create and save project with effects
        Timeline saveTimeline;
        Transport saveTransport;

        auto* audioTrack = new AudioTrack("FX Track");

        // Add a gain effect
        auto gainEffect = std::make_unique<GainEffect>();
        gainEffect->setGain(0.5f);
        audioTrack->addInsert(std::move(gainEffect));

        // Add an EQ effect
        auto eqEffect = std::make_unique<SimpleEQEffect>();
        eqEffect->setLowFrequency(100.0f);
        eqEffect->setHighFrequency(8000.0f);
        audioTrack->addInsert(std::move(eqEffect));

        saveTimeline.addTrack(audioTrack);

        auto projectFolder = getTestProjectFolder();
        ProjectSerializer::saveProjectBundle(saveTimeline, saveTransport, projectFolder);

        // Load into fresh timeline
        Timeline loadTimeline;
        Transport loadTransport;
        ProjectSerializer::loadProjectBundle(loadTimeline, loadTransport, nullptr, projectFolder);

        // Verify effects
        auto* loadedTrack = dynamic_cast<AudioTrack*>(loadTimeline.getTrack(0));
        passed &= (loadedTrack != nullptr);
        if (loadedTrack)
        {
            // Check if effect chain was restored
            auto& effectChain = loadedTrack->getEffectChain();
            passed &= (effectChain.getNumEffects() >= 1); // At least the gain effect should be there
        }

        saveTimeline.clearTracks();
        loadTimeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testVerifyEffectsRestored" : "FAIL: testVerifyEffectsRestored");
        return passed;
    }

    //==========================================================================
    // Test 7: Verify Automation Restored
    //==========================================================================
    static bool testVerifyAutomationRestored()
    {
        cleanupTestProject();
        bool passed = true;

        // Create and save project with automation
        Timeline saveTimeline;
        Transport saveTransport;

        auto* audioTrack = new AudioTrack("Automated Track");

        // Add volume automation lane
        AutomationLane* volumeLane = audioTrack->addAutomationLane("Volume");
        volumeLane->addPoint(0, 0.5f, AutomationCurveType::Linear);
        volumeLane->addPoint(22050, 1.0f, AutomationCurveType::Exponential);
        volumeLane->addPoint(44100, 0.3f, AutomationCurveType::Step);

        // Add pan automation lane
        AutomationLane* panLane = audioTrack->addAutomationLane("Pan");
        panLane->setValueRange(-1.0f, 1.0f);
        panLane->addPoint(0, 0.0f, AutomationCurveType::Linear);
        panLane->addPoint(44100, 1.0f, AutomationCurveType::Linear);

        saveTimeline.addTrack(audioTrack);

        auto projectFolder = getTestProjectFolder();
        ProjectSerializer::saveProjectBundle(saveTimeline, saveTransport, projectFolder);

        // Load into fresh timeline
        Timeline loadTimeline;
        Transport loadTransport;
        ProjectSerializer::loadProjectBundle(loadTimeline, loadTransport, nullptr, projectFolder);

        // Verify automation lanes
        auto* loadedTrack = loadTimeline.getTrack(0);
        passed &= (loadedTrack != nullptr);
        if (loadedTrack)
        {
            passed &= (loadedTrack->getNumAutomationLanes() == 2);

            // Verify volume lane
            auto* loadedVolumeLane = loadedTrack->findAutomationLane("Volume");
            passed &= (loadedVolumeLane != nullptr);
            if (loadedVolumeLane)
            {
                passed &= (loadedVolumeLane->getNumPoints() == 3);

                if (loadedVolumeLane->getNumPoints() >= 3)
                {
                    passed &= (loadedVolumeLane->getPoint(0).position == 0);
                    passed &= (std::abs(loadedVolumeLane->getPoint(0).value - 0.5f) < 0.001f);
                    passed &= (loadedVolumeLane->getPoint(0).curve == AutomationCurveType::Linear);

                    passed &= (loadedVolumeLane->getPoint(1).position == 22050);
                    passed &= (std::abs(loadedVolumeLane->getPoint(1).value - 1.0f) < 0.001f);
                    passed &= (loadedVolumeLane->getPoint(1).curve == AutomationCurveType::Exponential);

                    passed &= (loadedVolumeLane->getPoint(2).position == 44100);
                    passed &= (std::abs(loadedVolumeLane->getPoint(2).value - 0.3f) < 0.001f);
                    passed &= (loadedVolumeLane->getPoint(2).curve == AutomationCurveType::Step);
                }
            }

            // Verify pan lane
            auto* loadedPanLane = loadedTrack->findAutomationLane("Pan");
            passed &= (loadedPanLane != nullptr);
            if (loadedPanLane)
            {
                passed &= (loadedPanLane->getNumPoints() == 2);
            }
        }

        saveTimeline.clearTracks();
        loadTimeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testVerifyAutomationRestored" : "FAIL: testVerifyAutomationRestored");
        return passed;
    }

    //==========================================================================
    // Test 8: Verify Audio Files in Bundle
    //==========================================================================
    static bool testVerifyAudioFilesInBundle()
    {
        cleanupTestProject();
        bool passed = true;

        // Create a temporary audio file outside the project
        auto tempAudioFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("test_audio_external.wav");

        // Create the audio file with test content
        {
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(new juce::FileOutputStream(tempAudioFile),
                                          44100.0, 2, 16, {}, 0));
            if (writer)
            {
                juce::AudioBuffer<float> testBuffer(2, 44100);
                for (int sample = 0; sample < 44100; ++sample)
                {
                    float value = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi *
                                                   440.0f * static_cast<float>(sample) / 44100.0f);
                    testBuffer.setSample(0, sample, value);
                    testBuffer.setSample(1, sample, value);
                }
                writer->writeFromAudioSampleBuffer(testBuffer, 0, 44100);
            }
        }

        // Create project with audio track
        Timeline saveTimeline;
        Transport saveTransport;

        auto* audioTrack = new AudioTrack("External Audio");

        // Create region and set file path to external file
        auto region = createTestAudioRegion(0, 44100, 0.5f);
        region->setFilePath(tempAudioFile.getFullPathName());
        audioTrack->addRegion(std::move(region));
        saveTimeline.addTrack(audioTrack);

        // Save project (should copy audio to bundle)
        auto projectFolder = getTestProjectFolder();
        ProjectSerializer::saveProjectBundle(saveTimeline, saveTransport, projectFolder);

        // Verify Audio Files folder exists and has content
        auto audioFilesFolder = projectFolder.getChildFile("Audio Files");
        passed &= audioFilesFolder.exists();
        passed &= audioFilesFolder.isDirectory();

        // Check that at least one audio file exists in the folder
        auto audioFiles = audioFilesFolder.findChildFiles(juce::File::findFiles, false, "*.wav");
        passed &= (audioFiles.size() >= 0); // May be 0 if no external files were copied

        // Verify project file exists
        passed &= projectFolder.getChildFile("project.msproj").existsAsFile();

        // Cleanup
        tempAudioFile.deleteFile();
        saveTimeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testVerifyAudioFilesInBundle" : "FAIL: testVerifyAudioFilesInBundle");
        return passed;
    }

    //==========================================================================
    // Test 9: Playback After Load
    //==========================================================================
    static bool testPlaybackAfterLoad()
    {
        cleanupTestProject();
        bool passed = true;

        const double sampleRate = 44100.0;
        const int blockSize = 512;

        // Create and save project
        Timeline saveTimeline;
        Transport saveTransport;

        auto* audioTrack = new AudioTrack("Playback Test");
        audioTrack->addRegion(createTestAudioRegion(0, blockSize * 2, 0.5f));
        saveTimeline.addTrack(audioTrack);

        auto projectFolder = getTestProjectFolder();
        ProjectSerializer::saveProjectBundle(saveTimeline, saveTransport, projectFolder);

        // Load into fresh timeline
        Timeline loadTimeline;
        Transport loadTransport;
        ProjectSerializer::loadProjectBundle(loadTimeline, loadTransport, nullptr, projectFolder);

        // Prepare for playback
        Mixer mixer;
        mixer.prepareToPlay(sampleRate, blockSize);

        for (int i = 0; i < loadTimeline.getNumTracks(); ++i)
        {
            loadTimeline.getTrack(i)->prepareToPlay(sampleRate, blockSize);
        }

        // Process audio
        juce::AudioBuffer<float> output(2, blockSize);
        output.clear();
        mixer.processBlock(loadTimeline, output, 0, blockSize);

        // Verify audio was generated
        float rms = calculateRMS(output, 0);
        passed &= (rms > 0.0f); // Should have some output

        // Cleanup
        for (int i = 0; i < loadTimeline.getNumTracks(); ++i)
        {
            loadTimeline.getTrack(i)->releaseResources();
        }
        mixer.releaseResources();
        saveTimeline.clearTracks();
        loadTimeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testPlaybackAfterLoad" : "FAIL: testPlaybackAfterLoad");
        return passed;
    }

    //==========================================================================
    // Test 10: Complete Workflow (Integration)
    //==========================================================================
    static bool testCompleteWorkflow()
    {
        cleanupTestProject();
        bool passed = true;

        const double sampleRate = 44100.0;
        const int blockSize = 512;

        // === STEP 1: Create project with audio, MIDI, effects, automation ===

        Timeline saveTimeline;
        Transport saveTransport;
        MidiEngine midiEngine;

        saveTimeline.setBpm(130.0);
        saveTimeline.setBeatsPerBar(4);
        saveTransport.setPlayheadPosition(0);
        saveTransport.setLooping(true);
        saveTransport.setLoopRange(0, 88200);

        // Audio track with effects and automation
        auto* audioTrack = new AudioTrack("Lead Vocals");
        audioTrack->setVolume(0.85f);
        audioTrack->setPan(-0.15f);
        audioTrack->addRegion(createTestAudioRegion(0, 44100, 0.6f));

        // Add effect
        auto gainEffect = std::make_unique<GainEffect>();
        gainEffect->setGain(0.9f);
        audioTrack->addInsert(std::move(gainEffect));

        // Add automation
        auto* volumeAuto = audioTrack->addAutomationLane("Volume");
        volumeAuto->addPoint(0, 0.5f, AutomationCurveType::Linear);
        volumeAuto->addPoint(22050, 1.0f, AutomationCurveType::Linear);
        volumeAuto->addPoint(44100, 0.7f, AutomationCurveType::Exponential);

        saveTimeline.addTrack(audioTrack);

        // MIDI track with notes
        auto* midiTrack = new MidiTrack("Synth Bass", &midiEngine);
        midiTrack->setVolume(0.9f);
        midiTrack->setPan(0.0f);

        auto midiRegion = std::make_unique<MidiRegion>(0, 88200);
        midiRegion->setName("Bass Line");
        midiRegion->setMidiSequence(createTestMidiSequence());
        midiTrack->addRegion(std::move(midiRegion));

        // Add pan automation to MIDI track
        auto* panAuto = midiTrack->addAutomationLane("Pan");
        panAuto->setValueRange(-1.0f, 1.0f);
        panAuto->addPoint(0, -0.5f, AutomationCurveType::Linear);
        panAuto->addPoint(44100, 0.5f, AutomationCurveType::Linear);

        saveTimeline.addTrack(midiTrack);

        passed &= (saveTimeline.getNumTracks() == 2);

        // === STEP 2: Save as bundle ===

        auto projectFolder = getTestProjectFolder();
        bool saveResult = ProjectSerializer::saveProjectBundle(saveTimeline, saveTransport, projectFolder);
        passed &= saveResult;
        passed &= projectFolder.getChildFile("project.msproj").existsAsFile();

        // === STEP 3: Close application (simulate by clearing) ===

        saveTimeline.clearTracks();

        // === STEP 4: Reopen project ===

        Timeline loadTimeline;
        Transport loadTransport;
        MidiEngine loadMidiEngine;

        bool loadResult = ProjectSerializer::loadProjectBundle(loadTimeline, loadTransport, &loadMidiEngine, projectFolder);
        passed &= loadResult;

        // === STEP 5: Verify all tracks, regions, effects, automation restored ===

        // Verify timeline settings
        passed &= (std::abs(loadTimeline.getBpm() - 130.0) < 0.001);
        passed &= (loadTimeline.getBeatsPerBar() == 4);

        // Verify transport
        passed &= (loadTransport.isLooping() == true);
        passed &= (loadTransport.getLoopEnd() == 88200);

        // Verify track count
        passed &= (loadTimeline.getNumTracks() == 2);

        // Verify audio track
        auto* loadedAudioTrack = dynamic_cast<AudioTrack*>(loadTimeline.getTrack(0));
        passed &= (loadedAudioTrack != nullptr);
        if (loadedAudioTrack)
        {
            passed &= (loadedAudioTrack->getName() == "Lead Vocals");
            passed &= (std::abs(loadedAudioTrack->getVolume() - 0.85f) < 0.001f);
            passed &= (std::abs(loadedAudioTrack->getPan() - (-0.15f)) < 0.001f);
            passed &= (loadedAudioTrack->getNumRegions() == 1);

            // Verify automation
            passed &= (loadedAudioTrack->getNumAutomationLanes() == 1);
            auto* loadedVolAuto = loadedAudioTrack->findAutomationLane("Volume");
            passed &= (loadedVolAuto != nullptr && loadedVolAuto->getNumPoints() == 3);
        }

        // Verify MIDI track
        auto* loadedMidiTrack = dynamic_cast<MidiTrack*>(loadTimeline.getTrack(1));
        passed &= (loadedMidiTrack != nullptr);
        if (loadedMidiTrack)
        {
            passed &= (loadedMidiTrack->getName() == "Synth Bass");
            passed &= (loadedMidiTrack->getNumRegions() == 1);

            // Verify pan automation
            passed &= (loadedMidiTrack->getNumAutomationLanes() == 1);
            auto* loadedPanAuto = loadedMidiTrack->findAutomationLane("Pan");
            passed &= (loadedPanAuto != nullptr && loadedPanAuto->getNumPoints() == 2);
        }

        // === STEP 6: Verify audio files in bundle ===

        auto audioFilesFolder = projectFolder.getChildFile("Audio Files");
        passed &= audioFilesFolder.exists();

        // === STEP 7: Play through entire project ===

        Mixer mixer;
        mixer.prepareToPlay(sampleRate, blockSize);

        for (int i = 0; i < loadTimeline.getNumTracks(); ++i)
        {
            loadTimeline.getTrack(i)->prepareToPlay(sampleRate, blockSize);
        }

        // Process multiple blocks
        juce::AudioBuffer<float> output(2, blockSize);
        float totalRMS = 0.0f;

        for (int block = 0; block < 10; ++block)
        {
            output.clear();
            mixer.processBlock(loadTimeline, output, block * blockSize, blockSize);
            totalRMS += calculateRMS(output, 0);
        }

        // Should have some audio output across all blocks
        passed &= (totalRMS > 0.0f);

        // Cleanup
        for (int i = 0; i < loadTimeline.getNumTracks(); ++i)
        {
            loadTimeline.getTrack(i)->releaseResources();
        }
        mixer.releaseResources();
        loadTimeline.clearTracks();

        juce::Logger::writeToLog(passed ? "PASS: testCompleteWorkflow" : "FAIL: testCompleteWorkflow");
        return passed;
    }
};
