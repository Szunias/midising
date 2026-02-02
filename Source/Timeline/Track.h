#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <atomic>

/**
 * Type of track (Audio or MIDI)
 */
enum class TrackType
{
    Audio,
    MIDI
};

/**
 * Base class for all track types in the DAW.
 * Contains common properties like name, color, volume, pan, mute, solo, and arm.
 */
class Track
{
public:
    Track(const juce::String& name, TrackType type);
    virtual ~Track() = default;

    // Getters
    juce::String getName() const { return name; }
    TrackType getType() const { return type; }
    juce::Colour getColour() const { return colour; }
    float getVolume() const { return volume.load(); }
    float getPan() const { return pan.load(); }
    bool isMuted() const { return muted.load(); }
    bool isSoloed() const { return soloed.load(); }
    bool isArmed() const { return armed.load(); }
    int getHeight() const { return height; }

    // Setters
    void setName(const juce::String& newName) { name = newName; }
    void setColour(juce::Colour newColour) { colour = newColour; }
    void setVolume(float newVolume) { volume.store(juce::jlimit(0.0f, 2.0f, newVolume)); }
    void setPan(float newPan) { pan.store(juce::jlimit(-1.0f, 1.0f, newPan)); }
    void setMuted(bool shouldMute) { muted.store(shouldMute); }
    void setSoloed(bool shouldSolo) { soloed.store(shouldSolo); }
    void setArmed(bool shouldArm) { armed.store(shouldArm); }
    void setHeight(int newHeight) { height = juce::jmax(50, newHeight); }

    // Processing (to be implemented by subclasses)
    virtual void prepareToPlay(double sampleRate, int samplesPerBlock) = 0;
    virtual void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) = 0;
    virtual void releaseResources() = 0;

private:
    juce::String name;
    TrackType type;
    juce::Colour colour;
    int height = 100;

    // Thread-safe audio parameters
    std::atomic<float> volume { 1.0f };    // 0.0 to 2.0 (1.0 = unity gain)
    std::atomic<float> pan { 0.0f };       // -1.0 (left) to 1.0 (right)
    std::atomic<bool> muted { false };
    std::atomic<bool> soloed { false };
    std::atomic<bool> armed { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
};
