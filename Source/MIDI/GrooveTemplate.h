#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "../Timeline/Region.h"
#include <vector>

/**
 * A single point in a groove template.
 * Represents timing deviation and velocity scaling at a specific grid position.
 */
struct GroovePoint
{
    double gridPosition = 0.0;     // Position on the quantize grid (in ticks)
    double offset = 0.0;           // Timing offset from the grid position (in ticks)
    float velocityScale = 1.0f;    // Velocity multiplier (0.0 - 2.0)

    GroovePoint() = default;

    GroovePoint(double gridPos, double timeOffset, float velScale)
        : gridPosition(gridPos), offset(timeOffset), velocityScale(velScale)
    {
    }
};

/**
 * GrooveTemplate captures and applies rhythmic feel from MIDI performances.
 *
 * A groove template stores the timing deviations and velocity variations
 * of notes relative to a strict quantize grid. This "feel" can then be
 * applied to other MIDI regions to give them the same rhythmic character.
 *
 * Typical workflow:
 * 1. extractFromRegion() - analyze a performance to capture its groove
 * 2. applyToRegion() - apply that groove to another region
 * 3. Save/load via XML for reuse
 */
class GrooveTemplate
{
public:
    GrooveTemplate();
    ~GrooveTemplate() = default;

    /**
     * Extract a groove template from a MIDI region.
     * Analyzes note timing relative to the quantize grid to capture
     * the timing deviations and velocity patterns.
     *
     * @param region The MIDI region to analyze
     * @param quantizeGrid Grid size in ticks (e.g., 960.0 for quarter notes, 480.0 for eighths)
     * @param bpm Tempo in beats per minute
     * @param sampleRate Sample rate (used for any sample-based conversions)
     */
    void extractFromRegion(const MidiRegion& region, double quantizeGrid,
                           double bpm, double sampleRate);

    /**
     * Apply this groove template to a MIDI region.
     * Shifts note timing and scales velocity toward the groove positions.
     *
     * @param region The MIDI region to modify
     * @param strength Application strength (0.0 = no change, 1.0 = full groove)
     * @param bpm Tempo in beats per minute
     * @param sampleRate Sample rate
     */
    void applyToRegion(MidiRegion& region, float strength,
                       double bpm, double sampleRate) const;

    /**
     * Get the groove points.
     */
    const std::vector<GroovePoint>& getPoints() const { return points; }

    /**
     * Set the groove points directly.
     */
    void setPoints(const std::vector<GroovePoint>& newPoints) { points = newPoints; }

    /**
     * Clear all groove points.
     */
    void clear() { points.clear(); }

    /**
     * Check if the template has any groove data.
     */
    bool isEmpty() const { return points.empty(); }

    /**
     * Get the number of groove points.
     */
    int getNumPoints() const { return static_cast<int>(points.size()); }

    /**
     * Get/set the template name.
     */
    const juce::String& getName() const { return templateName; }
    void setName(const juce::String& name) { templateName = name; }

    /**
     * Get the quantize grid size used when this template was extracted.
     */
    double getGridSize() const { return gridSize; }

    // XML Serialization
    std::unique_ptr<juce::XmlElement> createXml() const;
    void loadFromXml(const juce::XmlElement& xml);

private:
    /**
     * Find the nearest groove point for a given grid position.
     * Returns nullptr if no suitable point is found.
     */
    const GroovePoint* findNearestPoint(double gridPosition) const;

    std::vector<GroovePoint> points;
    juce::String templateName = "Untitled Groove";
    double gridSize = 480.0;   // Grid size in ticks used during extraction
};
