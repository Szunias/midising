#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <memory>
#include <cmath>

/**
 * Type of region (Audio or MIDI)
 */
enum class RegionType
{
    Audio,
    MIDI
};

/**
 * Default crossfade length in samples for automatic crossfades.
 * Applied when regions overlap to ensure smooth transitions.
 * At 44100 Hz, 2205 samples = 50ms crossfade.
 */
constexpr int64_t DEFAULT_CROSSFADE_SAMPLES = 2205;

/**
 * Crossfade utility functions for smooth audio transitions.
 * Uses equal-power crossfade curves to prevent amplitude dips.
 */
namespace CrossfadeUtils
{
    /**
     * Calculate equal-power crossfade gain for fade-out.
     * @param position Current position in the crossfade (0 to crossfadeLength)
     * @param crossfadeLength Total length of the crossfade in samples
     * @return Gain value (1.0 at start, 0.0 at end)
     */
    inline float calculateFadeOutGain(int64_t position, int64_t crossfadeLength)
    {
        if (crossfadeLength <= 0)
            return 1.0f;
        float t = static_cast<float>(position) / static_cast<float>(crossfadeLength);
        t = juce::jlimit(0.0f, 1.0f, t);
        // Equal-power fade out: cos(t * pi/2)
        return std::cos(t * juce::MathConstants<float>::halfPi);
    }

    /**
     * Calculate equal-power crossfade gain for fade-in.
     * @param position Current position in the crossfade (0 to crossfadeLength)
     * @param crossfadeLength Total length of the crossfade in samples
     * @return Gain value (0.0 at start, 1.0 at end)
     */
    inline float calculateFadeInGain(int64_t position, int64_t crossfadeLength)
    {
        if (crossfadeLength <= 0)
            return 1.0f;
        float t = static_cast<float>(position) / static_cast<float>(crossfadeLength);
        t = juce::jlimit(0.0f, 1.0f, t);
        // Equal-power fade in: sin(t * pi/2)
        return std::sin(t * juce::MathConstants<float>::halfPi);
    }
}

/**
 * Represents a clip/region on a track.
 * Contains position, length, and content data (audio buffer or MIDI sequence).
 */
class Region
{
public:
    Region(RegionType type, int64_t startPosition, int64_t length);
    virtual ~Region() = default;

    // Getters
    RegionType getType() const { return type; }
    int64_t getStartPosition() const { return startPosition; }
    int64_t getLength() const { return length; }
    int64_t getEndPosition() const { return startPosition + length; }
    int64_t getOffset() const { return offset; }
    juce::String getName() const { return name; }

    // Fade getters
    int64_t getFadeInLength() const { return fadeInLength; }
    int64_t getFadeOutLength() const { return fadeOutLength; }

    // Setters
    void setStartPosition(int64_t newPosition) { startPosition = newPosition; }
    void setLength(int64_t newLength) { length = juce::jmax(int64_t(0), newLength); }
    void setOffset(int64_t newOffset) { offset = juce::jmax(int64_t(0), newOffset); }
    void setName(const juce::String& newName) { name = newName; }

    // Fade setters - clamp to region length
    void setFadeInLength(int64_t newLength)
    {
        fadeInLength = juce::jlimit(int64_t(0), length - fadeOutLength, newLength);
    }
    void setFadeOutLength(int64_t newLength)
    {
        fadeOutLength = juce::jlimit(int64_t(0), length - fadeInLength, newLength);
    }

    // Check if a sample position falls within this region
    bool containsPosition(int64_t position) const
    {
        return position >= startPosition && position < getEndPosition();
    }

protected:
    RegionType type;
    int64_t startPosition;  // Start position in samples
    int64_t length;         // Length in samples
    int64_t offset = 0;     // Offset into source content (for trimmed regions)
    juce::String name;

    // Fade lengths in samples (0 = no fade)
    int64_t fadeInLength = 0;
    int64_t fadeOutLength = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Region)
};

/**
 * Audio region containing an audio buffer.
 * Supports time stretching and pitch shifting properties.
 */
class AudioRegion : public Region
{
public:
    AudioRegion(int64_t startPosition, int64_t length)
        : Region(RegionType::Audio, startPosition, length)
    {
    }

    // Get/set the audio buffer
    juce::AudioBuffer<float>& getAudioBuffer() { return audioBuffer; }
    const juce::AudioBuffer<float>& getAudioBuffer() const { return audioBuffer; }
    void setAudioBuffer(const juce::AudioBuffer<float>& buffer) { audioBuffer = buffer; }

    // Thumbnail caching
    int64_t getThumbnailHash() const { return thumbnailHash; }
    void setThumbnailHash(int64_t hash) { thumbnailHash = hash; }

    // Source file path
    void setFilePath(const juce::String& path) { filePath = path; }
    juce::String getFilePath() const { return filePath; }

    //==============================================================================
    // Time Stretch Properties

    /**
     * Get the time stretch ratio (1.0 = original speed).
     * Range: 0.25 (4x slower) to 4.0 (4x faster)
     */
    float getStretchRatio() const { return stretchRatio; }

    /**
     * Set the time stretch ratio.
     * @param ratio Stretch ratio clamped to 0.25-4.0 range
     */
    void setStretchRatio(float ratio)
    {
        stretchRatio = juce::jlimit(0.25f, 4.0f, ratio);
    }

    /**
     * Check if time stretching is applied to this region.
     */
    bool isTimeStretched() const { return stretchRatio != 1.0f; }

    //==============================================================================
    // Pitch Shift Properties

    /**
     * Get the pitch shift in semitones.
     * Range: -24 to +24 semitones (2 octaves)
     */
    int getPitchSemitones() const { return pitchSemitones; }

    /**
     * Set the pitch shift in semitones.
     * @param semitones Pitch shift clamped to -24 to +24 range
     */
    void setPitchSemitones(int semitones)
    {
        pitchSemitones = juce::jlimit(-24, 24, semitones);
    }

    /**
     * Get the fine pitch adjustment in cents.
     * Range: -100 to +100 cents (1 semitone)
     */
    int getPitchCents() const { return pitchCents; }

    /**
     * Set the fine pitch adjustment in cents.
     * @param cents Fine pitch adjustment clamped to -100 to +100 range
     */
    void setPitchCents(int cents)
    {
        pitchCents = juce::jlimit(-100, 100, cents);
    }

    /**
     * Get the total pitch shift in semitones (including cents as fractional part).
     */
    float getTotalPitchShift() const
    {
        return static_cast<float>(pitchSemitones) + static_cast<float>(pitchCents) / 100.0f;
    }

    /**
     * Check if pitch shifting is applied to this region.
     */
    bool isPitchShifted() const { return pitchSemitones != 0 || pitchCents != 0; }

    //==============================================================================
    // Formant Preservation

    /**
     * Get whether formant preservation is enabled for pitch shifting.
     * Formant preservation helps maintain natural vocal/instrument character.
     */
    bool getFormantPreserve() const { return formantPreserve; }

    /**
     * Set whether to preserve formants during pitch shifting.
     * @param preserve True to preserve formants, false for standard pitch shifting
     */
    void setFormantPreserve(bool preserve) { formantPreserve = preserve; }

    //==============================================================================
    // Combined Processing Check

    /**
     * Check if any time/pitch processing is needed for this region.
     */
    bool needsProcessing() const { return isTimeStretched() || isPitchShifted(); }

    //==============================================================================
    // Crossfade Curve Type

    int getCrossfadeCurveType() const { return crossfadeCurveType; }
    void setCrossfadeCurveType(int curveType) { crossfadeCurveType = juce::jlimit(0, 3, curveType); }

    //==============================================================================
    // Reversed flag (for visual indication)

    bool getIsReversed() const { return isReversed; }
    void setIsReversed(bool reversed) { isReversed = reversed; }

private:
    juce::AudioBuffer<float> audioBuffer;
    int64_t thumbnailHash = 0;
    juce::String filePath;

    // Time stretch / pitch shift properties
    float stretchRatio = 1.0f;      // Time stretch ratio (0.25 to 4.0, 1.0 = no stretch)
    int pitchSemitones = 0;         // Pitch shift in semitones (-24 to +24)
    int pitchCents = 0;             // Fine pitch adjustment in cents (-100 to +100)
    bool formantPreserve = false;   // Preserve formants during pitch shift

    // Crossfade curve type: 0=Linear, 1=EqualPower, 2=SCurve, 3=Logarithmic
    int crossfadeCurveType = 1;     // Default: EqualPower
    bool isReversed = false;        // Visual flag for reversed regions
};

/**
 * MIDI region containing a MIDI message sequence.
 */
class MidiRegion : public Region
{
public:
    MidiRegion(int64_t startPosition, int64_t length)
        : Region(RegionType::MIDI, startPosition, length)
    {
    }

    // Get/set the MIDI sequence
    juce::MidiMessageSequence& getMidiSequence() { return midiSequence; }
    const juce::MidiMessageSequence& getMidiSequence() const { return midiSequence; }
    void setMidiSequence(const juce::MidiMessageSequence& sequence) { midiSequence = sequence; }

private:
    juce::MidiMessageSequence midiSequence;
};
