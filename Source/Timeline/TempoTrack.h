#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>
#include <cmath>

/**
 * Ramp type for tempo transitions between points.
 */
enum class TempoRampType
{
    Instant,     // Immediate tempo change (step)
    Linear,      // Linear ramp between tempos
    SCurve       // Smooth S-curve transition (ease-in-ease-out)
};

/**
 * Represents a single tempo event on the tempo track.
 * Contains position, BPM value, and ramp type to the next point.
 */
struct TempoEvent
{
    int64_t position;        // Position in samples
    double bpm;              // Tempo in beats per minute (20.0 to 300.0)
    TempoRampType rampType;  // Ramp type to next point

    TempoEvent(int64_t pos = 0, double tempo = 120.0,
               TempoRampType ramp = TempoRampType::Instant)
        : position(pos), bpm(juce::jlimit(20.0, 300.0, tempo)), rampType(ramp)
    {
    }

    // Comparison operators for sorting by position
    bool operator<(const TempoEvent& other) const
    {
        return position < other.position;
    }

    bool operator==(const TempoEvent& other) const
    {
        return position == other.position;
    }
};

/**
 * Utility functions for tempo ramp interpolation.
 */
namespace TempoRampUtils
{
    /**
     * Interpolate between two BPM values using the specified ramp type.
     * @param startBpm The starting tempo
     * @param endBpm The ending tempo
     * @param t Normalized position between points (0.0 to 1.0)
     * @param rampType The interpolation ramp type
     * @return Interpolated BPM value
     */
    inline double interpolate(double startBpm, double endBpm, double t, TempoRampType rampType)
    {
        t = juce::jlimit(0.0, 1.0, t);

        switch (rampType)
        {
            case TempoRampType::Instant:
                // Step: return start tempo until we reach the end
                return (t < 1.0) ? startBpm : endBpm;

            case TempoRampType::Linear:
                return startBpm + (endBpm - startBpm) * t;

            case TempoRampType::SCurve:
            {
                // S-Curve (smoothstep): smooth ease-in-ease-out
                // Formula: 3t^2 - 2t^3
                double curved = t * t * (3.0 - 2.0 * t);
                return startBpm + (endBpm - startBpm) * curved;
            }

            default:
                return startBpm;
        }
    }

    /**
     * Convert BPM to seconds per beat.
     * @param bpm Tempo in beats per minute
     * @return Seconds per beat
     */
    inline double bpmToSecondsPerBeat(double bpm)
    {
        if (bpm <= 0.0)
            return 0.5; // Default to 120 BPM
        return 60.0 / bpm;
    }

    /**
     * Convert BPM to samples per beat.
     * @param bpm Tempo in beats per minute
     * @param sampleRate Sample rate in Hz
     * @return Samples per beat
     */
    inline double bpmToSamplesPerBeat(double bpm, double sampleRate)
    {
        return bpmToSecondsPerBeat(bpm) * sampleRate;
    }
}

/**
 * Represents the tempo track containing tempo change events.
 * Manages tempo variations over time for the timeline.
 */
class TempoTrack
{
public:
    TempoTrack(double bpm = 120.0)
        : initialBpm(juce::jlimit(20.0, 300.0, bpm))
    {
    }
    ~TempoTrack() = default;

    //==========================================================================
    // Getters
    //==========================================================================

    /** Get the initial/default BPM (tempo at position 0 if no events exist) */
    double getInitialBpm() const { return initialBpm; }

    /** Check if the tempo track is enabled */
    bool isEnabled() const { return enabled; }

    /** Get the sample rate used for position calculations */
    double getSampleRate() const { return sampleRate; }

    //==========================================================================
    // Setters
    //==========================================================================

    /** Set the initial/default BPM */
    void setInitialBpm(double bpm) { initialBpm = juce::jlimit(20.0, 300.0, bpm); }

    /** Enable or disable the tempo track */
    void setEnabled(bool shouldBeEnabled) { enabled = shouldBeEnabled; }

    /** Set the sample rate for position calculations */
    void setSampleRate(double rate) { sampleRate = rate; }

    //==========================================================================
    // Event Management
    //==========================================================================

    /** Add a tempo event */
    void addEvent(const TempoEvent& event)
    {
        events.push_back(event);
        sortEvents();
    }

    /** Add a tempo event with parameters */
    void addEvent(int64_t position, double bpm, TempoRampType ramp = TempoRampType::Instant)
    {
        addEvent(TempoEvent(position, bpm, ramp));
    }

    /** Remove an event by index */
    void removeEvent(size_t index)
    {
        if (index < events.size())
            events.erase(events.begin() + static_cast<std::ptrdiff_t>(index));
    }

    /** Remove events in a sample range */
    void removeEventsInRange(int64_t startPosition, int64_t endPosition)
    {
        events.erase(
            std::remove_if(events.begin(), events.end(),
                [startPosition, endPosition](const TempoEvent& e) {
                    return e.position >= startPosition && e.position <= endPosition;
                }),
            events.end());
    }

    /** Move an event to a new position and/or BPM */
    void moveEvent(size_t index, int64_t newPosition, double newBpm)
    {
        if (index < events.size())
        {
            events[index].position = newPosition;
            events[index].bpm = juce::jlimit(20.0, 300.0, newBpm);
            sortEvents();
        }
    }

    /** Set the ramp type for an event */
    void setEventRampType(size_t index, TempoRampType ramp)
    {
        if (index < events.size())
            events[index].rampType = ramp;
    }

    /** Clear all tempo events */
    void clearAllEvents()
    {
        events.clear();
    }

    //==========================================================================
    // Event Access
    //==========================================================================

    /** Get the number of tempo events */
    size_t getNumEvents() const { return events.size(); }

    /** Get an event by index (const) */
    const TempoEvent& getEvent(size_t index) const { return events[index]; }

    /** Get an event by index (mutable) */
    TempoEvent& getEvent(size_t index) { return events[index]; }

    /** Get all events (const) */
    const std::vector<TempoEvent>& getEvents() const { return events; }

    //==========================================================================
    // Find Events
    //==========================================================================

    /**
     * Find an event at or near a position.
     * @param position Position in samples
     * @param tolerance Tolerance in samples for matching
     * @return Index of the event, or -1 if not found
     */
    int findEventAtPosition(int64_t position, int64_t tolerance = 0) const
    {
        for (size_t i = 0; i < events.size(); ++i)
        {
            int64_t diff = std::abs(events[i].position - position);
            if (diff <= tolerance)
                return static_cast<int>(i);
        }
        return -1;
    }

    /**
     * Find the nearest event before a position.
     * @param position Position in samples
     * @return Index of the event, or -1 if no event before position
     */
    int findNearestEventBefore(int64_t position) const
    {
        int result = -1;
        for (size_t i = 0; i < events.size(); ++i)
        {
            if (events[i].position <= position)
                result = static_cast<int>(i);
            else
                break; // Events are sorted, so we can stop
        }
        return result;
    }

    /**
     * Find the nearest event after a position.
     * @param position Position in samples
     * @return Index of the event, or -1 if no event after position
     */
    int findNearestEventAfter(int64_t position) const
    {
        for (size_t i = 0; i < events.size(); ++i)
        {
            if (events[i].position > position)
                return static_cast<int>(i);
        }
        return -1;
    }

    //==========================================================================
    // Tempo Queries
    //==========================================================================

    /**
     * Get the BPM at a specific sample position.
     * If the tempo track is disabled, returns the initial BPM.
     * @param position Position in samples
     * @return BPM at the position
     */
    double getBpmAtPosition(int64_t position) const
    {
        if (!enabled || events.empty())
            return initialBpm;

        // Find the event before this position
        int beforeIndex = findNearestEventBefore(position);

        if (beforeIndex < 0)
        {
            // No event before this position, use initial BPM
            return initialBpm;
        }

        const TempoEvent& beforeEvent = events[static_cast<size_t>(beforeIndex)];

        // Check if there's an event after this position
        int afterIndex = findNearestEventAfter(position);

        if (afterIndex < 0)
        {
            // No event after, use the tempo of the last event
            return beforeEvent.bpm;
        }

        // Check if we're exactly at the before event
        if (position == beforeEvent.position)
            return beforeEvent.bpm;

        const TempoEvent& afterEvent = events[static_cast<size_t>(afterIndex)];

        // Calculate interpolation factor
        double t = static_cast<double>(position - beforeEvent.position) /
                   static_cast<double>(afterEvent.position - beforeEvent.position);

        // Interpolate using the ramp type of the before event
        return TempoRampUtils::interpolate(beforeEvent.bpm, afterEvent.bpm, t, beforeEvent.rampType);
    }

    /**
     * Get the samples per beat at a specific position.
     * @param position Position in samples
     * @return Samples per beat at the position
     */
    double getSamplesPerBeatAtPosition(int64_t position) const
    {
        double bpm = getBpmAtPosition(position);
        return TempoRampUtils::bpmToSamplesPerBeat(bpm, sampleRate);
    }

    /**
     * Check if there are any tempo changes (events).
     * @return true if the tempo track has tempo changes
     */
    bool hasTempoChanges() const { return !events.empty(); }

    //==========================================================================
    // Position Conversion
    //==========================================================================

    /**
     * Convert a beat position to sample position, accounting for tempo changes.
     * This is more accurate than simple conversion when tempo varies.
     * @param beats Beat position
     * @return Sample position
     */
    int64_t beatsToSamples(double beats) const
    {
        if (!enabled || events.empty())
        {
            // Simple conversion with constant tempo
            return static_cast<int64_t>(beats * TempoRampUtils::bpmToSamplesPerBeat(initialBpm, sampleRate));
        }

        // For tempo tracks with changes, we need to integrate through the tempo map
        // This is an approximation using discrete steps
        double currentBeat = 0.0;
        int64_t currentSample = 0;

        // Start with initial tempo up to first event
        if (!events.empty() && events[0].position > 0)
        {
            double samplesPerBeat = TempoRampUtils::bpmToSamplesPerBeat(initialBpm, sampleRate);
            double beatsToFirstEvent = static_cast<double>(events[0].position) / samplesPerBeat;

            if (beats <= beatsToFirstEvent)
            {
                return static_cast<int64_t>(beats * samplesPerBeat);
            }

            currentBeat = beatsToFirstEvent;
            currentSample = events[0].position;
        }

        // Process through events
        for (size_t i = 0; i < events.size(); ++i)
        {
            double currentBpm = events[i].bpm;
            double samplesPerBeat = TempoRampUtils::bpmToSamplesPerBeat(currentBpm, sampleRate);

            int64_t nextEventPosition;
            if (i + 1 < events.size())
                nextEventPosition = events[i + 1].position;
            else
                nextEventPosition = std::numeric_limits<int64_t>::max();

            double beatsInSegment = static_cast<double>(nextEventPosition - currentSample) / samplesPerBeat;

            if (beats <= currentBeat + beatsInSegment)
            {
                // Target is in this segment
                double beatsRemaining = beats - currentBeat;
                return currentSample + static_cast<int64_t>(beatsRemaining * samplesPerBeat);
            }

            currentBeat += beatsInSegment;
            currentSample = nextEventPosition;
        }

        // Beyond all events, use last tempo
        double lastBpm = events.empty() ? initialBpm : events.back().bpm;
        double samplesPerBeat = TempoRampUtils::bpmToSamplesPerBeat(lastBpm, sampleRate);
        double beatsRemaining = beats - currentBeat;
        return currentSample + static_cast<int64_t>(beatsRemaining * samplesPerBeat);
    }

    /**
     * Convert a sample position to beat position, accounting for tempo changes.
     * @param samples Sample position
     * @return Beat position
     */
    double samplesToBeats(int64_t samples) const
    {
        if (!enabled || events.empty())
        {
            // Simple conversion with constant tempo
            double samplesPerBeat = TempoRampUtils::bpmToSamplesPerBeat(initialBpm, sampleRate);
            return static_cast<double>(samples) / samplesPerBeat;
        }

        // Integrate through the tempo map
        double currentBeat = 0.0;
        int64_t currentSample = 0;

        // Start with initial tempo up to first event
        if (!events.empty() && events[0].position > 0)
        {
            double samplesPerBeat = TempoRampUtils::bpmToSamplesPerBeat(initialBpm, sampleRate);

            if (samples <= events[0].position)
            {
                return static_cast<double>(samples) / samplesPerBeat;
            }

            currentBeat = static_cast<double>(events[0].position) / samplesPerBeat;
            currentSample = events[0].position;
        }

        // Process through events
        for (size_t i = 0; i < events.size(); ++i)
        {
            double currentBpm = events[i].bpm;
            double samplesPerBeat = TempoRampUtils::bpmToSamplesPerBeat(currentBpm, sampleRate);

            int64_t nextEventPosition;
            if (i + 1 < events.size())
                nextEventPosition = events[i + 1].position;
            else
                nextEventPosition = std::numeric_limits<int64_t>::max();

            if (samples <= nextEventPosition)
            {
                // Target is in this segment
                double samplesInSegment = static_cast<double>(samples - currentSample);
                return currentBeat + samplesInSegment / samplesPerBeat;
            }

            double samplesInSegment = static_cast<double>(nextEventPosition - currentSample);
            currentBeat += samplesInSegment / samplesPerBeat;
            currentSample = nextEventPosition;
        }

        // Beyond all events, use last tempo
        double lastBpm = events.empty() ? initialBpm : events.back().bpm;
        double samplesPerBeat = TempoRampUtils::bpmToSamplesPerBeat(lastBpm, sampleRate);
        double samplesRemaining = static_cast<double>(samples - currentSample);
        return currentBeat + samplesRemaining / samplesPerBeat;
    }

    //==========================================================================
    // Utility
    //==========================================================================

    /** Sort events by position */
    void sortEvents()
    {
        std::sort(events.begin(), events.end());
    }

private:
    std::vector<TempoEvent> events;

    double initialBpm = 120.0;      // Default tempo when no events or before first event
    bool enabled = true;            // Whether tempo track is active
    double sampleRate = 44100.0;    // Sample rate for position calculations

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TempoTrack)
};
