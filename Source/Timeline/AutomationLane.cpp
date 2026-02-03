#include "AutomationLane.h"

AutomationLane::AutomationLane(AutomationParameterType paramType, const juce::String& paramName)
    : parameterType(paramType),
      parameterName(paramName)
{
    // Set default value ranges based on parameter type
    switch (parameterType)
    {
        case AutomationParameterType::Volume:
            minValue = 0.0f;
            maxValue = 2.0f;      // 0 to +6dB
            defaultValue = 1.0f;  // Unity gain
            break;

        case AutomationParameterType::Pan:
            minValue = -1.0f;
            maxValue = 1.0f;
            defaultValue = 0.0f;  // Center
            break;

        case AutomationParameterType::Mute:
            minValue = 0.0f;
            maxValue = 1.0f;
            defaultValue = 0.0f;  // Not muted
            break;

        case AutomationParameterType::Send:
            minValue = 0.0f;
            maxValue = 1.0f;
            defaultValue = 0.0f;  // No send
            break;

        case AutomationParameterType::PluginParameter:
        case AutomationParameterType::Custom:
        default:
            minValue = 0.0f;
            maxValue = 1.0f;
            defaultValue = 0.5f;
            break;
    }
}

void AutomationLane::addPoint(const AutomationPoint& point)
{
    // Check if a point already exists at this position
    for (auto& existingPoint : points)
    {
        if (existingPoint.position == point.position)
        {
            // Update existing point
            existingPoint.value = point.value;
            existingPoint.curve = point.curve;
            return;
        }
    }

    // Add new point and keep sorted
    points.push_back(point);
    sortPoints();
}

void AutomationLane::addPoint(int64_t position, float value, AutomationCurveType curve)
{
    addPoint(AutomationPoint(position, value, curve));
}

void AutomationLane::removePoint(size_t index)
{
    if (index < points.size())
    {
        points.erase(points.begin() + static_cast<long>(index));
    }
}

void AutomationLane::removePointsInRange(int64_t startPosition, int64_t endPosition)
{
    points.erase(
        std::remove_if(points.begin(), points.end(),
            [startPosition, endPosition](const AutomationPoint& point)
            {
                return point.position >= startPosition && point.position <= endPosition;
            }),
        points.end());
}

void AutomationLane::movePoint(size_t index, int64_t newPosition, float newValue)
{
    if (index < points.size())
    {
        points[index].position = newPosition;
        points[index].value = juce::jlimit(0.0f, 1.0f, newValue);
        sortPoints();
    }
}

void AutomationLane::setPointCurve(size_t index, AutomationCurveType curve)
{
    if (index < points.size())
    {
        points[index].curve = curve;
    }
}

void AutomationLane::clearAllPoints()
{
    points.clear();
}

int AutomationLane::findPointAtPosition(int64_t position, int64_t tolerance) const
{
    for (size_t i = 0; i < points.size(); ++i)
    {
        int64_t diff = std::abs(points[i].position - position);
        if (diff <= tolerance)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int AutomationLane::findNearestPointBefore(int64_t position) const
{
    int result = -1;
    for (size_t i = 0; i < points.size(); ++i)
    {
        if (points[i].position <= position)
        {
            result = static_cast<int>(i);
        }
        else
        {
            break;  // Points are sorted, so we can stop
        }
    }
    return result;
}

int AutomationLane::findNearestPointAfter(int64_t position) const
{
    for (size_t i = 0; i < points.size(); ++i)
    {
        if (points[i].position > position)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

float AutomationLane::getValueAtPosition(int64_t position) const
{
    // If bypassed, return default value
    if (bypassed)
        return defaultValue;

    // If no points, return default value
    if (points.empty())
        return defaultValue;

    // If before first point, return first point's value
    if (position <= points.front().position)
        return points.front().value;

    // If after last point, return last point's value
    if (position >= points.back().position)
        return points.back().value;

    // Find surrounding points and interpolate
    for (size_t i = 0; i < points.size() - 1; ++i)
    {
        if (position >= points[i].position && position < points[i + 1].position)
        {
            const auto& startPoint = points[i];
            const auto& endPoint = points[i + 1];

            int64_t range = endPoint.position - startPoint.position;
            if (range <= 0)
                return startPoint.value;

            float t = static_cast<float>(position - startPoint.position) / static_cast<float>(range);

            return AutomationCurveUtils::interpolate(
                startPoint.value, endPoint.value, t, startPoint.curve);
        }
    }

    return defaultValue;
}

float AutomationLane::getScaledValueAtPosition(int64_t position) const
{
    float normalizedValue = getValueAtPosition(position);
    return minValue + normalizedValue * (maxValue - minValue);
}

void AutomationLane::getValuesForBlock(int64_t startPosition, int numSamples, float* outputBuffer) const
{
    if (outputBuffer == nullptr || numSamples <= 0)
        return;

    // If bypassed or no points, fill with default value
    if (bypassed || points.empty())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            outputBuffer[i] = defaultValue;
        }
        return;
    }

    // Optimization: check if entire block is before first point or after last point
    int64_t endPosition = startPosition + numSamples - 1;

    if (endPosition <= points.front().position)
    {
        float value = points.front().value;
        for (int i = 0; i < numSamples; ++i)
        {
            outputBuffer[i] = value;
        }
        return;
    }

    if (startPosition >= points.back().position)
    {
        float value = points.back().value;
        for (int i = 0; i < numSamples; ++i)
        {
            outputBuffer[i] = value;
        }
        return;
    }

    // General case: interpolate for each sample
    // TODO: Optimize by finding segments and filling in batches
    for (int i = 0; i < numSamples; ++i)
    {
        outputBuffer[i] = getValueAtPosition(startPosition + i);
    }
}

void AutomationLane::sortPoints()
{
    std::sort(points.begin(), points.end());
}
