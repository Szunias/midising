#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"

class AudioEngine;

/**
 * Timer-driven visual component showing LUFS loudness measurements.
 *
 * Compact mode (120px wide): momentary + integrated LUFS bars.
 * Full mode (200px wide): adds short-term + LRA.
 * Color coding: green < -14 LUFS, yellow -14 to -9, red > -9.
 */
class LoudnessDisplay : public juce::Component,
                         public juce::Timer
{
public:
    LoudnessDisplay();
    ~LoudnessDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setAudioEngine(AudioEngine* engine);
    void setCompactMode(bool compact);
    bool isCompactMode() const { return compactMode; }

    int getPreferredWidth() const { return compactMode ? 120 : 200; }

private:
    AudioEngine* audioEngine = nullptr;
    bool compactMode = true;

    // Cached values (updated in timer callback)
    float momentaryLUFS = -70.0f;
    float shortTermLUFS = -70.0f;
    float integratedLUFS = -70.0f;
    float loudnessRange = 0.0f;

    // Smoothed display values for bar animation
    float displayMomentary = 0.0f;
    float displayShortTerm = 0.0f;

    static constexpr float MIN_LUFS = -70.0f;
    static constexpr float MAX_LUFS = 0.0f;

    juce::Colour getLevelColour(float lufs) const;
    float lufsToNormalized(float lufs) const;
    void drawMeter(juce::Graphics& g, juce::Rectangle<int> bounds,
                   float value, const juce::String& label, const juce::String& valueText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoudnessDisplay)
};
