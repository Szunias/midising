#pragma once

#include "../Timeline/Track.h"
#include "../Timeline/Region.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include "../Effects/EffectChain.h"

/**
 * AudioTrack is a track that plays and records audio.
 * Contains audio regions and handles audio playback/recording.
 */
class AudioTrack : public Track
{
public:
    AudioTrack(const juce::String& name = "Audio Track");
    ~AudioTrack() override = default;

    // Track interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;

    // Region management
    void addRegion(std::unique_ptr<AudioRegion> region);
    void removeRegion(int index);
    AudioRegion* getRegion(int index);
    const AudioRegion* getRegion(int index) const { return regions[static_cast<size_t>(index)].get(); }
    int getNumRegions() const { return static_cast<int>(regions.size()); }
    void clearRegions();

    // Recording
    void startRecording(int64_t startPosition);
    void stopRecording();
    bool isCurrentlyRecording() const { return recording; }
    
    // Record audio sample (call from audio thread during recording)
    void recordSample(const float* leftChannel, const float* rightChannel, int numSamples);

    // Load audio from file and create region
    bool loadAudioFile(const juce::File& file, int64_t startPosition);

    // Effects
    EffectChain& getEffectChain() { return effectChain; }
    const EffectChain& getEffectChain() const { return effectChain; }

private:
    std::vector<std::unique_ptr<AudioRegion>> regions;
    EffectChain effectChain;
    
    // Recording state
    bool recording = false;
    int64_t recordingStartPosition = 0;
    std::unique_ptr<AudioRegion> recordingRegion;
    static constexpr int MAX_RECORDING_SAMPLES = 44100 * 60 * 10; // 10 minutes at 44.1kHz
    int recordingWritePosition = 0;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioTrack)
};
