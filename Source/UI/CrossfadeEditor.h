#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Timeline/CrossfadeManager.h"
#include <functional>

/**
 * CrossfadeEditor is a popup component for editing crossfade parameters
 * between overlapping audio regions.
 *
 * Displays a visual preview of the fade-in and fade-out curves,
 * allows curve type selection, and crossfade length adjustment.
 *
 * Size: 300x200 pixels
 */
class CrossfadeEditor : public juce::Component
{
public:
    CrossfadeEditor();
    ~CrossfadeEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Get the currently selected crossfade curve type. */
    CrossfadeCurveType getSelectedCurveType() const;

    /** Set the crossfade curve type (updates ComboBox and repaints). */
    void setSelectedCurveType(CrossfadeCurveType type);

    /** Get the current crossfade length in samples. */
    int64_t getCrossfadeLength() const;

    /** Set the crossfade length in samples (updates slider and repaints). */
    void setCrossfadeLength(int64_t lengthInSamples);

    /**
     * Callback invoked when the user clicks Apply.
     * Parameters: (CrossfadeCurveType curveType, int64_t crossfadeLengthSamples)
     */
    std::function<void(CrossfadeCurveType, int64_t)> onCrossfadeChanged;

private:
    /** Draw the crossfade curves in the given display area. */
    void drawCrossfadeCurves(juce::Graphics& g, juce::Rectangle<int> area);

    juce::ComboBox curveTypeCombo;
    juce::Slider lengthSlider;
    juce::Label curveTypeLabel;
    juce::Label lengthLabel;
    juce::TextButton applyButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrossfadeEditor)
};
