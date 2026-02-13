#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

/**
 * Describes a single automatable parameter on an Effect.
 * Used to auto-generate UI and drive automation.
 */
struct EffectParameter
{
    juce::String id;           // Unique identifier (e.g. "threshold")
    juce::String name;         // Display name (e.g. "Threshold")
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    float currentValue = 0.0f;
    float step = 0.0f;        // 0 = continuous
    juce::String unit;         // e.g. "dB", "Hz", "%", "ms"
    float skew = 1.0f;        // >1 = more resolution at low end, <1 = more at high end

    /** Convert real value to normalized 0-1 range (accounting for skew). */
    float getNormalized() const
    {
        if (maxValue <= minValue) return 0.0f;
        float proportion = (currentValue - minValue) / (maxValue - minValue);
        proportion = juce::jlimit(0.0f, 1.0f, proportion);
        if (skew != 1.0f && skew > 0.0f)
            proportion = std::pow(proportion, 1.0f / skew);
        return proportion;
    }

    /** Set the real value from a normalized 0-1 value (accounting for skew). */
    void setFromNormalized(float normalized)
    {
        normalized = juce::jlimit(0.0f, 1.0f, normalized);
        if (skew != 1.0f && skew > 0.0f)
            normalized = std::pow(normalized, skew);
        currentValue = minValue + normalized * (maxValue - minValue);
        if (step > 0.0f)
            currentValue = std::round(currentValue / step) * step;
        currentValue = juce::jlimit(minValue, maxValue, currentValue);
    }

    /** Get a formatted string of the current value with unit. */
    juce::String getDisplayValue() const
    {
        juce::String text;
        if (step >= 1.0f)
            text = juce::String(static_cast<int>(currentValue));
        else
            text = juce::String(currentValue, 2);
        if (unit.isNotEmpty())
            text += " " + unit;
        return text;
    }
};
