#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

/**
 * Base class for all MIDI effects.
 * Mirrors the Effect.h pattern but operates on MIDI data instead of audio.
 * Subclasses implement processMidi() to transform MIDI buffers in real-time.
 */
class MidiEffect
{
public:
    MidiEffect(const juce::String& name) : effectName(name) {}
    virtual ~MidiEffect() = default;

    /**
     * Process a MIDI buffer in real-time.
     * @param buffer The MIDI buffer to process (modified in-place)
     * @param numSamples Number of samples in the current block
     * @param sampleRate Current sample rate
     * @param bpm Current tempo in beats per minute
     */
    virtual void processMidi(juce::MidiBuffer& buffer, int numSamples,
                             double sampleRate, double bpm) = 0;

    /**
     * Called before playback starts to allow the effect to prepare.
     * @param sampleRate The sample rate that will be used
     * @param samplesPerBlock Maximum expected block size
     */
    virtual void prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        juce::ignoreUnused(sampleRate, samplesPerBlock);
    }

    /**
     * Reset the effect state (e.g., when transport stops or loops).
     */
    virtual void reset() {}

    const juce::String& getName() const { return effectName; }

    void setBypass(bool shouldBypass) { bypass = shouldBypass; }
    bool isBypassed() const { return bypass; }

    // Serialization
    virtual std::unique_ptr<juce::XmlElement> createXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("MIDI_EFFECT");
        xml->setAttribute("name", effectName);
        xml->setAttribute("bypass", bypass);
        return xml;
    }

    virtual void loadFromXml(const juce::XmlElement& xml)
    {
        bypass = xml.getBoolAttribute("bypass", false);
    }

protected:
    juce::String effectName;
    bool bypass = false;
};
