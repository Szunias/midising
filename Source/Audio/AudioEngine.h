#pragma once

#include "Transport.h"
#include "Mixer.h"
#include "Transport.h"
#include "Mixer.h"
#include "AudioRecorder.h"
#include "SpectrumAnalyzer.h"
#include "Metronome.h"
#include "../Timeline/Timeline.h"
#include "../MIDI/MidiEngine.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

/**
 * AudioEngine is the central audio processing hub.
 * Owns Timeline, Transport, Mixer, MidiEngine, and AudioRecorder.
 * Implements getNextAudioBlock for real-time audio processing.
 */
class AudioEngine : public juce::AudioSource
{
public:
    AudioEngine();
    ~AudioEngine() override;

    // AudioSource interface
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // Access to components
    Timeline& getTimeline() { return timeline; }
    const Timeline& getTimeline() const { return timeline; }
    
    Transport& getTransport() { return transport; }
    const Transport& getTransport() const { return transport; }
    
    Mixer& getMixer() { return mixer; }
    const Mixer& getMixer() const { return mixer; }

    MidiEngine& getMidiEngine() { return midiEngine; }
    const MidiEngine& getMidiEngine() const { return midiEngine; }

    AudioRecorder& getRecorder() { return recorder; }
    const AudioRecorder& getRecorder() const { return recorder; }

    SpectrumAnalyzer& getSpectrumAnalyzer() { return spectrumAnalyzer; }

    Metronome& getMetronome() { return metronome; }
    const Metronome& getMetronome() const { return metronome; }

    // Recording management
    bool startRecording(Track* targetTrack = nullptr);
    void stopRecording();
    bool isRecording() const { return recorder.isRecording(); }
    
    // Get recorded file after recording stops
    juce::File getLastRecordedFile() const { return lastRecordedFile; }

    // Convenience methods
    double getSampleRate() const { return currentSampleRate; }
    int getBlockSize() const { return currentBlockSize; }

private:
    void handleRecordingStart();
    void handleRecordingStop();
    juce::File generateRecordingFilePath();

    Timeline timeline;
    Transport transport;
    Mixer mixer;
    MidiEngine midiEngine;
    AudioRecorder recorder;
    SpectrumAnalyzer spectrumAnalyzer;
    Metronome metronome;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    
    // Recording state
    Track* recordingTargetTrack = nullptr;
    int64_t recordingStartPosition = 0;
    juce::File lastRecordedFile;
    bool wasRecordingLastBlock = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
