#pragma once

#include "../Timeline/Track.h"
#include "../Timeline/Region.h"
#include "MidiEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>

/**
 * MidiTrack is a track that plays MIDI data through a synthesizer.
 */
class MidiTrack : public Track
{
public:
    MidiTrack(const juce::String& name = "MIDI Track", MidiEngine* engine = nullptr);
    ~MidiTrack() override = default;

    // Track interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    void releaseResources() override;

    // Set the MIDI engine to use for synthesis
    void setMidiEngine(MidiEngine* engine) { midiEngine = engine; }

    // Region management
    void addRegion(std::unique_ptr<MidiRegion> region);
    void removeRegion(int index);
    MidiRegion* getRegion(int index);
    const MidiRegion* getRegion(int index) const { return regions[static_cast<size_t>(index)].get(); }
    int getNumRegions() const { return static_cast<int>(regions.size()); }
    void clearRegions();

    // Direct note access (for piano roll editing)
    juce::MidiMessageSequence& getMidiSequence() { return midiSequence; }
    const juce::MidiMessageSequence& getMidiSequence() const { return midiSequence; }

private:
    std::vector<std::unique_ptr<MidiRegion>> regions;
    juce::MidiMessageSequence midiSequence; // Master sequence combining all regions
    MidiEngine* midiEngine = nullptr;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiTrack)
};
