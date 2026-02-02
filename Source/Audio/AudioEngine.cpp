#include "AudioEngine.h"
#include "AudioTrack.h"

AudioEngine::AudioEngine()
{
}

AudioEngine::~AudioEngine()
{
    stopRecording();
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlockExpected;

    timeline.setSampleRate(sampleRate);
    mixer.prepareToPlay(sampleRate, samplesPerBlockExpected);
    midiEngine.prepareToPlay(sampleRate, samplesPerBlockExpected);
    metronome.prepareToPlay(sampleRate, samplesPerBlockExpected);

    // Prepare all tracks
    for (int i = 0; i < timeline.getNumTracks(); ++i)
    {
        timeline.getTrack(i)->prepareToPlay(sampleRate, samplesPerBlockExpected);
    }
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Clear buffer first
    bufferToFill.clearActiveBufferRegion();

    // Check if recording state changed
    bool isCurrentlyRecording = transport.isRecording();
    
    if (isCurrentlyRecording && !wasRecordingLastBlock)
    {
        // Just started recording
        handleRecordingStart();
    }
    else if (!isCurrentlyRecording && wasRecordingLastBlock)
    {
        // Just stopped recording
        handleRecordingStop();
    }
    wasRecordingLastBlock = isCurrentlyRecording;

    // Capture input for recording before any processing
    if (isCurrentlyRecording && recorder.isRecording())
    {
        recordInputBlock(*bufferToFill.buffer);
    }

    // If not playing or recording, just output silence
    if (transport.isStopped())
        return;

    // Get current playhead position
    int64_t playheadPos = transport.getPlayheadPosition();
    int numSamples = bufferToFill.numSamples;

    // Let mixer process all tracks and sum into output buffer
    mixer.processBlock(timeline, *bufferToFill.buffer, playheadPos, numSamples);

    // Update peak levels for metering
    mixer.updatePeakLevels(*bufferToFill.buffer);

    // Process metronome (mix clicks into output buffer)
    metronome.processBlock(*bufferToFill.buffer, playheadPos, transport.getBPM(), transport.isPlaying());

    // Push samples to spectrum analyzer (mix down to mono)
    auto* leftChannel = bufferToFill.buffer->getReadPointer(0);
    auto* rightChannel = bufferToFill.buffer->getNumChannels() > 1 ? bufferToFill.buffer->getReadPointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float sample = leftChannel[i];
        if (rightChannel != nullptr)
            sample = (sample + rightChannel[i]) * 0.5f;
            
        spectrumAnalyzer.pushNextSampleIntoFifo(sample);
    }

    // Advance playhead
    transport.advancePlayhead(numSamples);

    // Handle loop wrapping
    transport.handleLoopWrap();
}

void AudioEngine::releaseResources()
{
    stopRecording();
    mixer.releaseResources();
    midiEngine.releaseResources();

    // Release resources for all tracks
    for (int i = 0; i < timeline.getNumTracks(); ++i)
    {
        timeline.getTrack(i)->releaseResources();
    }
}

bool AudioEngine::startRecording(Track* targetTrack)
{
    if (recorder.isRecording())
        return false;

    recordingTargetTrack = targetTrack;
    
    // If no target track specified, find first armed audio track
    if (recordingTargetTrack == nullptr)
    {
        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            Track* track = timeline.getTrack(i);
            if (track->isArmed() && track->getType() == TrackType::Audio)
            {
                recordingTargetTrack = track;
                break;
            }
        }
    }

    recordingStartPosition = transport.getPlayheadPosition();
    lastRecordedFile = generateRecordingFilePath();
    
    if (recorder.startRecording(lastRecordedFile, currentSampleRate, 2))
    {
        transport.record();
        return true;
    }
    
    return false;
}

void AudioEngine::stopRecording()
{
    if (recorder.isRecording())
    {
        recorder.stopRecording();
        transport.stop();
    }
}

void AudioEngine::recordInputBlock(const juce::AudioBuffer<float>& inputBuffer)
{
    // Capture input buffer for recording before any processing is applied
    // This ensures we record the raw input signal
    if (recorder.isRecording() && inputBuffer.getNumChannels() > 0)
    {
        recorder.writeAudioBlock(inputBuffer);
    }
}

void AudioEngine::handleRecordingStart()
{
    if (!recorder.isRecording())
    {
        recordingStartPosition = transport.getPlayheadPosition();
        lastRecordedFile = generateRecordingFilePath();
        recorder.startRecording(lastRecordedFile, currentSampleRate, 2);
    }
}

void AudioEngine::handleRecordingStop()
{
    if (recorder.isRecording())
    {
        recorder.stopRecording();
        
        // Load recorded audio and create region on target track
        if (recordingTargetTrack != nullptr && lastRecordedFile.existsAsFile())
        {
            auto* audioTrack = dynamic_cast<AudioTrack*>(recordingTargetTrack);
            if (audioTrack != nullptr)
            {
                audioTrack->loadAudioFile(lastRecordedFile, recordingStartPosition);
            }
        }
    }
}

juce::File AudioEngine::generateRecordingFilePath()
{
    // Generate unique filename with timestamp
    auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    auto recordingsDir = tempDir.getChildFile("MidiSing_Recordings");
    
    if (!recordingsDir.exists())
        recordingsDir.createDirectory();
    
    return recordingsDir.getChildFile("recording_" + timestamp + ".wav");
}
