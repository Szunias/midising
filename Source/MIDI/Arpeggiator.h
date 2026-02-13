#pragma once

#include "MidiEffect.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>

/**
 * Arpeggiator pattern types.
 */
enum class ArpPattern
{
    Up,
    Down,
    UpDown,
    Random,
    AsPlayed
};

/**
 * Arpeggiator rate values (note divisions).
 */
enum class ArpRate
{
    Quarter,
    Eighth,
    Sixteenth,
    DottedEighth,
    DottedSixteenth
};

/**
 * Arpeggiator MIDI effect.
 * Tracks held notes and generates arpeggiated output synced to tempo.
 * Supports multiple patterns, rate divisions, octave range, and gate length.
 */
class Arpeggiator : public MidiEffect
{
public:
    Arpeggiator();
    ~Arpeggiator() override = default;

    void processMidi(juce::MidiBuffer& buffer, int numSamples,
                     double sampleRate, double bpm) override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void reset() override;

    // Pattern
    void setPattern(ArpPattern newPattern);
    ArpPattern getPattern() const;

    // Rate
    void setRate(ArpRate newRate);
    ArpRate getRate() const;

    // Octave range (1-4)
    void setOctaveRange(int octaves);
    int getOctaveRange() const;

    // Gate length (0.1 - 1.0)
    void setGateLength(float length);
    float getGateLength() const;

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    /**
     * Get the rate divisor for the current ArpRate.
     * Returns the number of beats per arpeggio step.
     */
    double getRateDivisor() const;

    /**
     * Build the full note sequence including octave expansion.
     * Returns notes ordered according to the current pattern.
     */
    std::vector<int> buildNoteSequence() const;

    /**
     * Get the next note index based on the current pattern.
     */
    int getNextNoteIndex(int currentIndex, int sequenceSize);

    ArpPattern pattern = ArpPattern::Up;
    ArpRate rate = ArpRate::Eighth;
    int octaveRange = 1;
    float gateLength = 0.8f;

    // Held notes in the order they were received
    std::vector<int> heldNotes;

    // Playback state
    double samplePosition = 0.0;
    int currentNoteIndex = -1;
    int currentPlayingNote = -1;       // Currently sounding note number (-1 = none)
    int currentPlayingChannel = 1;
    bool goingUp = true;               // Direction flag for UpDown pattern

    double currentSampleRate = 44100.0;

    juce::Random randomGenerator;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Arpeggiator)
};
