#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "AutomationTarget.h"
#include <vector>
#include <algorithm>
#include <cmath>

/**
 * Interpolation curve type for automation between points.
 */
enum class AutomationCurveType
{
    Linear,      // Straight line interpolation
    Exponential, // Exponential curve (good for volume)
    Logarithmic, // Logarithmic curve
    Step,        // Instant change at next point
    SCurve       // Smooth S-curve (ease-in-ease-out)
};

/**
 * Parameter type that can be automated.
 */
enum class AutomationParameterType
{
    Volume,
    Pan,
    Mute,
    Send,
    PluginParameter,
    Custom
};

/**
 * Represents a single automation point on a lane.
 * Contains position, value, and curve type to the next point.
 */
struct AutomationPoint
{
    int64_t position;            // Position in samples
    float value;                 // Normalized value (0.0 to 1.0)
    AutomationCurveType curve;   // Curve type to next point

    AutomationPoint(int64_t pos = 0, float val = 0.5f,
                    AutomationCurveType curveType = AutomationCurveType::Linear)
        : position(pos), value(val), curve(curveType)
    {
    }

    // Comparison operators for sorting by position
    bool operator<(const AutomationPoint& other) const
    {
        return position < other.position;
    }

    bool operator==(const AutomationPoint& other) const
    {
        return position == other.position;
    }
};

/**
 * Utility functions for automation curve interpolation.
 */
namespace AutomationCurveUtils
{
    /**
     * Interpolate between two values using the specified curve type.
     * @param startValue The starting value
     * @param endValue The ending value
     * @param t Normalized position between points (0.0 to 1.0)
     * @param curveType The interpolation curve type
     * @return Interpolated value
     */
    inline float interpolate(float startValue, float endValue, float t, AutomationCurveType curveType)
    {
        t = juce::jlimit(0.0f, 1.0f, t);

        switch (curveType)
        {
            case AutomationCurveType::Linear:
                return startValue + (endValue - startValue) * t;

            case AutomationCurveType::Exponential:
            {
                // Exponential curve (more natural for volume)
                float curved = t * t;
                return startValue + (endValue - startValue) * curved;
            }

            case AutomationCurveType::Logarithmic:
            {
                // Logarithmic curve (quick rise, slow approach)
                float curved = std::sqrt(t);
                return startValue + (endValue - startValue) * curved;
            }

            case AutomationCurveType::Step:
                // Step: return start value until we reach the end
                return (t < 1.0f) ? startValue : endValue;

            case AutomationCurveType::SCurve:
            {
                // S-Curve (smoothstep): smooth ease-in-ease-out
                // Formula: 3t² - 2t³
                float curved = t * t * (3.0f - 2.0f * t);
                return startValue + (endValue - startValue) * curved;
            }

            default:
                return startValue + (endValue - startValue) * t;
        }
    }

    /**
     * Convert a normalized value (0.0 to 1.0) to decibels.
     * @param normalizedValue Value between 0.0 and 1.0
     * @param minDb Minimum dB value (default -60 dB)
     * @param maxDb Maximum dB value (default +6 dB)
     * @return Value in decibels
     */
    inline float normalizedToDecibels(float normalizedValue, float minDb = -60.0f, float maxDb = 6.0f)
    {
        normalizedValue = juce::jlimit(0.0f, 1.0f, normalizedValue);
        if (normalizedValue <= 0.0f)
            return minDb;
        return minDb + (maxDb - minDb) * normalizedValue;
    }

    /**
     * Convert decibels to a normalized value (0.0 to 1.0).
     * @param db Value in decibels
     * @param minDb Minimum dB value (default -60 dB)
     * @param maxDb Maximum dB value (default +6 dB)
     * @return Normalized value between 0.0 and 1.0
     */
    inline float decibelsToNormalized(float db, float minDb = -60.0f, float maxDb = 6.0f)
    {
        if (db <= minDb)
            return 0.0f;
        if (db >= maxDb)
            return 1.0f;
        return (db - minDb) / (maxDb - minDb);
    }
}

/**
 * Represents an automation lane for a parameter.
 * Contains a collection of automation points that control a parameter over time.
 */
class AutomationLane
{
public:
    AutomationLane(AutomationParameterType paramType = AutomationParameterType::Volume,
                   const juce::String& paramName = "Volume");
    ~AutomationLane() = default;

    // Getters
    AutomationParameterType getParameterType() const { return parameterType; }
    juce::String getParameterName() const { return parameterName; }
    float getDefaultValue() const { return defaultValue; }
    float getMinValue() const { return minValue; }
    float getMaxValue() const { return maxValue; }
    bool isVisible() const { return visible; }
    bool isBypassed() const { return bypassed; }
    int getHeight() const { return height; }

    // Automation target
    void setTarget(const AutomationTarget& t) { target = t; }
    const AutomationTarget& getTarget() const { return target; }

    // Setters
    void setParameterType(AutomationParameterType type) { parameterType = type; }
    void setParameterName(const juce::String& name) { parameterName = name; }
    void setDefaultValue(float value) { defaultValue = juce::jlimit(minValue, maxValue, value); }
    void setValueRange(float min, float max)
    {
        minValue = min;
        maxValue = max;
        defaultValue = juce::jlimit(minValue, maxValue, defaultValue);
    }
    void setVisible(bool shouldBeVisible) { visible = shouldBeVisible; }
    void setBypassed(bool shouldBeBypassed) { bypassed = shouldBeBypassed; }
    void setHeight(int newHeight) { height = juce::jmax(30, newHeight); }

    // Point management
    void addPoint(const AutomationPoint& point);
    void addPoint(int64_t position, float value,
                  AutomationCurveType curve = AutomationCurveType::Linear);
    void removePoint(size_t index);
    void removePointsInRange(int64_t startPosition, int64_t endPosition);
    void movePoint(size_t index, int64_t newPosition, float newValue);
    void setPointCurve(size_t index, AutomationCurveType curve);
    void clearAllPoints();

    // Point access
    size_t getNumPoints() const { return points.size(); }
    const AutomationPoint& getPoint(size_t index) const { return points[index]; }
    AutomationPoint& getPoint(size_t index) { return points[index]; }
    const std::vector<AutomationPoint>& getPoints() const { return points; }

    // Find points
    int findPointAtPosition(int64_t position, int64_t tolerance = 0) const;
    int findNearestPointBefore(int64_t position) const;
    int findNearestPointAfter(int64_t position) const;

    /**
     * Get the interpolated value at a specific position.
     * @param position Position in samples
     * @return Interpolated value at the position
     */
    float getValueAtPosition(int64_t position) const;

    /**
     * Get the scaled/denormalized value at a position.
     * Applies min/max scaling to the interpolated value.
     * @param position Position in samples
     * @return Scaled value at the position
     */
    float getScaledValueAtPosition(int64_t position) const;

    /**
     * Get a block of automation values for efficient processing.
     * @param startPosition Starting position in samples
     * @param numSamples Number of samples to fill
     * @param outputBuffer Buffer to fill with values (must be at least numSamples in size)
     */
    void getValuesForBlock(int64_t startPosition, int numSamples, float* outputBuffer) const;

    // Utility
    void sortPoints();
    bool hasAutomation() const { return !points.empty(); }

private:
    AutomationParameterType parameterType;
    juce::String parameterName;
    AutomationTarget target;

    std::vector<AutomationPoint> points;

    float defaultValue = 0.5f;  // Default value when no automation
    float minValue = 0.0f;      // Minimum parameter value
    float maxValue = 1.0f;      // Maximum parameter value

    bool visible = true;        // Whether lane is visible in UI
    bool bypassed = false;      // Whether automation is bypassed
    int height = 60;            // Lane height in pixels

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationLane)
};
