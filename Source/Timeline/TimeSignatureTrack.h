#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>
#include <cmath>

/**
 * Represents a single time signature event on the time signature track.
 * Contains position, numerator (beats per bar), and denominator (note value).
 */
struct TimeSignatureEvent
{
    int64_t position;     // Position in samples
    int numerator;        // Beats per bar (e.g., 4 in 4/4)
    int denominator;      // Note value that gets one beat (e.g., 4 in 4/4)

    TimeSignatureEvent(int64_t pos = 0, int num = 4, int denom = 4)
        : position(pos)
        , numerator(juce::jlimit(1, 32, num))
        , denominator(juce::jlimit(1, 64, denom))
    {
    }

    // Comparison operators for sorting by position
    bool operator<(const TimeSignatureEvent& other) const
    {
        return position < other.position;
    }

    bool operator==(const TimeSignatureEvent& other) const
    {
        return position == other.position;
    }

    // Check if two time signatures are equivalent
    bool isSameTimeSignature(const TimeSignatureEvent& other) const
    {
        return numerator == other.numerator && denominator == other.denominator;
    }
};

/**
 * Utility functions for time signature calculations.
 */
namespace TimeSignatureUtils
{
    /**
     * Calculate the number of quarter notes in a bar for a given time signature.
     * @param numerator Beats per bar
     * @param denominator Note value that gets one beat
     * @return Number of quarter notes per bar
     */
    inline double quarterNotesPerBar(int numerator, int denominator)
    {
        // Example: 4/4 = 4 quarter notes, 6/8 = 3 quarter notes, 3/4 = 3 quarter notes
        return static_cast<double>(numerator) * 4.0 / static_cast<double>(denominator);
    }

    /**
     * Calculate the duration of one beat in quarter notes.
     * @param denominator Note value that gets one beat
     * @return Duration of one beat in quarter notes
     */
    inline double beatLengthInQuarterNotes(int denominator)
    {
        // 4 = quarter note = 1.0, 8 = eighth note = 0.5, 2 = half note = 2.0
        return 4.0 / static_cast<double>(denominator);
    }

    /**
     * Check if a time signature is compound (like 6/8, 9/8, 12/8).
     * Compound time signatures have groupings of 3 beats.
     * @param numerator Beats per bar
     * @param denominator Note value that gets one beat
     * @return true if compound meter
     */
    inline bool isCompoundMeter(int numerator, int denominator)
    {
        // Compound meters have a numerator divisible by 3 (but not 3 itself)
        // and typically use 8th notes as the beat unit
        return denominator == 8 && numerator > 3 && numerator % 3 == 0;
    }

    /**
     * Get the beat grouping for compound time signatures.
     * @param numerator Beats per bar
     * @param denominator Note value that gets one beat
     * @return Number of beats in each group (3 for compound, 1 for simple)
     */
    inline int getBeatGrouping(int numerator, int denominator)
    {
        return isCompoundMeter(numerator, denominator) ? 3 : 1;
    }

    /**
     * Get the effective number of "big beats" for compound time.
     * @param numerator Beats per bar
     * @param denominator Note value that gets one beat
     * @return Number of big beats (e.g., 2 for 6/8, 3 for 9/8, 4 for 12/8)
     */
    inline int getEffectiveBeats(int numerator, int denominator)
    {
        if (isCompoundMeter(numerator, denominator))
            return numerator / 3;
        return numerator;
    }
}

/**
 * Represents the time signature track containing time signature change events.
 * Manages time signature variations over time for the timeline.
 */
class TimeSignatureTrack
{
public:
    TimeSignatureTrack(int initialNumerator = 4, int initialDenominator = 4);
    ~TimeSignatureTrack() = default;

    //==========================================================================
    // Getters
    //==========================================================================

    /** Get the initial/default numerator (beats per bar at position 0 if no events exist) */
    int getInitialNumerator() const { return initialNumerator; }

    /** Get the initial/default denominator (note value at position 0 if no events exist) */
    int getInitialDenominator() const { return initialDenominator; }

    /** Check if the time signature track is enabled */
    bool isEnabled() const { return enabled; }

    /** Get the sample rate used for position calculations */
    double getSampleRate() const { return sampleRate; }

    //==========================================================================
    // Setters
    //==========================================================================

    /** Set the initial/default time signature */
    void setInitialTimeSignature(int numerator, int denominator)
    {
        initialNumerator = juce::jlimit(1, 32, numerator);
        initialDenominator = juce::jlimit(1, 64, denominator);
    }

    /** Enable or disable the time signature track */
    void setEnabled(bool shouldBeEnabled) { enabled = shouldBeEnabled; }

    /** Set the sample rate for position calculations */
    void setSampleRate(double rate) { sampleRate = rate; }

    //==========================================================================
    // Event Management
    //==========================================================================

    /** Add a time signature event */
    void addEvent(const TimeSignatureEvent& event)
    {
        events.push_back(event);
        sortEvents();
    }

    /** Add a time signature event with parameters */
    void addEvent(int64_t position, int numerator, int denominator)
    {
        addEvent(TimeSignatureEvent(position, numerator, denominator));
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
                [startPosition, endPosition](const TimeSignatureEvent& e) {
                    return e.position >= startPosition && e.position <= endPosition;
                }),
            events.end());
    }

    /** Move an event to a new position and/or time signature */
    void moveEvent(size_t index, int64_t newPosition, int newNumerator, int newDenominator)
    {
        if (index < events.size())
        {
            events[index].position = newPosition;
            events[index].numerator = juce::jlimit(1, 32, newNumerator);
            events[index].denominator = juce::jlimit(1, 64, newDenominator);
            sortEvents();
        }
    }

    /** Update an event's time signature at the current position */
    void setEventTimeSignature(size_t index, int numerator, int denominator)
    {
        if (index < events.size())
        {
            events[index].numerator = juce::jlimit(1, 32, numerator);
            events[index].denominator = juce::jlimit(1, 64, denominator);
        }
    }

    /** Clear all time signature events */
    void clearAllEvents()
    {
        events.clear();
    }

    //==========================================================================
    // Event Access
    //==========================================================================

    /** Get the number of time signature events */
    size_t getNumEvents() const { return events.size(); }

    /** Get an event by index (const) */
    const TimeSignatureEvent& getEvent(size_t index) const { return events[index]; }

    /** Get an event by index (mutable) */
    TimeSignatureEvent& getEvent(size_t index) { return events[index]; }

    /** Get all events (const) */
    const std::vector<TimeSignatureEvent>& getEvents() const { return events; }

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
    // Time Signature Queries
    //==========================================================================

    /**
     * Get the time signature at a specific sample position.
     * If the track is disabled, returns the initial time signature.
     * @param position Position in samples
     * @param outNumerator Output: Numerator at the position
     * @param outDenominator Output: Denominator at the position
     */
    void getTimeSignatureAtPosition(int64_t position, int& outNumerator, int& outDenominator) const
    {
        if (!enabled || events.empty())
        {
            outNumerator = initialNumerator;
            outDenominator = initialDenominator;
            return;
        }

        // Find the event at or before this position
        int beforeIndex = findNearestEventBefore(position);

        if (beforeIndex < 0)
        {
            // No event before this position, use initial time signature
            outNumerator = initialNumerator;
            outDenominator = initialDenominator;
            return;
        }

        const TimeSignatureEvent& event = events[static_cast<size_t>(beforeIndex)];
        outNumerator = event.numerator;
        outDenominator = event.denominator;
    }

    /**
     * Get the time signature event at a specific sample position.
     * @param position Position in samples
     * @return TimeSignatureEvent at the position
     */
    TimeSignatureEvent getTimeSignatureEventAtPosition(int64_t position) const
    {
        int numerator, denominator;
        getTimeSignatureAtPosition(position, numerator, denominator);
        return TimeSignatureEvent(position, numerator, denominator);
    }

    /**
     * Get the numerator (beats per bar) at a position.
     * @param position Position in samples
     * @return Numerator at the position
     */
    int getNumeratorAtPosition(int64_t position) const
    {
        int numerator, denominator;
        getTimeSignatureAtPosition(position, numerator, denominator);
        return numerator;
    }

    /**
     * Get the denominator (note value) at a position.
     * @param position Position in samples
     * @return Denominator at the position
     */
    int getDenominatorAtPosition(int64_t position) const
    {
        int numerator, denominator;
        getTimeSignatureAtPosition(position, numerator, denominator);
        return denominator;
    }

    /**
     * Check if there are any time signature changes (events).
     * @return true if the track has time signature changes
     */
    bool hasTimeSignatureChanges() const { return !events.empty(); }

    /**
     * Get the number of quarter notes per bar at a position.
     * @param position Position in samples
     * @return Quarter notes per bar
     */
    double getQuarterNotesPerBarAtPosition(int64_t position) const
    {
        int numerator, denominator;
        getTimeSignatureAtPosition(position, numerator, denominator);
        return TimeSignatureUtils::quarterNotesPerBar(numerator, denominator);
    }

    /**
     * Check if the time signature at a position is compound meter.
     * @param position Position in samples
     * @return true if compound meter
     */
    bool isCompoundMeterAtPosition(int64_t position) const
    {
        int numerator, denominator;
        getTimeSignatureAtPosition(position, numerator, denominator);
        return TimeSignatureUtils::isCompoundMeter(numerator, denominator);
    }

    //==========================================================================
    // Bar/Beat Calculations
    //==========================================================================

    /**
     * Calculate the bar and beat position for a given sample position.
     * This accounts for time signature changes throughout the track.
     * @param samplePosition Position in samples
     * @param bpm Current tempo in BPM (needed for sample-to-beat conversion)
     * @param outBar Output: Bar number (1-based)
     * @param outBeat Output: Beat within the bar (1-based)
     * @param outTick Output: Sub-beat position (0.0 to 1.0)
     */
    void getBarBeatPosition(int64_t samplePosition, double bpm,
                            int& outBar, int& outBeat, double& outTick) const
    {
        if (bpm <= 0.0 || sampleRate <= 0.0)
        {
            outBar = 1;
            outBeat = 1;
            outTick = 0.0;
            return;
        }

        // Convert samples to quarter notes
        double samplesPerQuarterNote = (60.0 / bpm) * sampleRate;
        double totalQuarterNotes = static_cast<double>(samplePosition) / samplesPerQuarterNote;

        if (!enabled || events.empty())
        {
            // Simple case: constant time signature
            double quarterNotesPerBar = TimeSignatureUtils::quarterNotesPerBar(initialNumerator, initialDenominator);
            double barPosition = totalQuarterNotes / quarterNotesPerBar;
            outBar = static_cast<int>(barPosition) + 1;
            double beatFraction = (barPosition - static_cast<int>(barPosition)) * initialNumerator;
            outBeat = static_cast<int>(beatFraction) + 1;
            outTick = beatFraction - static_cast<int>(beatFraction);
            return;
        }

        // Complex case: time signature changes
        // We need to integrate through each time signature region
        double remainingQuarterNotes = totalQuarterNotes;
        int currentBar = 1;
        size_t eventIndex = 0;
        int currentNumerator = initialNumerator;
        int currentDenominator = initialDenominator;

        while (remainingQuarterNotes > 0.0)
        {
            // Get current time signature
            if (eventIndex < events.size())
            {
                double eventPositionInQuarterNotes =
                    static_cast<double>(events[eventIndex].position) / samplesPerQuarterNote;

                if (eventPositionInQuarterNotes <= totalQuarterNotes - remainingQuarterNotes)
                {
                    currentNumerator = events[eventIndex].numerator;
                    currentDenominator = events[eventIndex].denominator;
                    eventIndex++;
                    continue;
                }
            }

            double quarterNotesPerBar = TimeSignatureUtils::quarterNotesPerBar(currentNumerator, currentDenominator);

            // Calculate how many quarter notes until the next event (if any)
            double quarterNotesUntilNextEvent = remainingQuarterNotes;
            if (eventIndex < events.size())
            {
                double eventPositionInQuarterNotes =
                    static_cast<double>(events[eventIndex].position) / samplesPerQuarterNote;
                double currentPositionInQuarterNotes = totalQuarterNotes - remainingQuarterNotes;
                quarterNotesUntilNextEvent = eventPositionInQuarterNotes - currentPositionInQuarterNotes;
            }

            // Count complete bars
            double barsInSegment = quarterNotesUntilNextEvent / quarterNotesPerBar;
            int completeBars = static_cast<int>(barsInSegment);
            currentBar += completeBars;
            remainingQuarterNotes -= completeBars * quarterNotesPerBar;

            // If we have remaining quarter notes less than a bar, we're done
            if (remainingQuarterNotes < quarterNotesPerBar &&
                (eventIndex >= events.size() || remainingQuarterNotes < quarterNotesUntilNextEvent - completeBars * quarterNotesPerBar))
            {
                break;
            }

            // Move to next event
            if (eventIndex < events.size())
            {
                currentNumerator = events[eventIndex].numerator;
                currentDenominator = events[eventIndex].denominator;
                eventIndex++;
            }
        }

        // Calculate beat and tick from remaining quarter notes
        double beatLengthInQuarterNotes = TimeSignatureUtils::beatLengthInQuarterNotes(currentDenominator);
        double beatPosition = remainingQuarterNotes / beatLengthInQuarterNotes;
        outBar = currentBar;
        outBeat = static_cast<int>(beatPosition) + 1;
        outTick = beatPosition - static_cast<int>(beatPosition);
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
    std::vector<TimeSignatureEvent> events;

    int initialNumerator = 4;       // Default beats per bar when no events or before first event
    int initialDenominator = 4;     // Default note value when no events or before first event
    bool enabled = true;            // Whether time signature track is active
    double sampleRate = 44100.0;    // Sample rate for position calculations

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimeSignatureTrack)
};

// Inline implementation of constructor
inline TimeSignatureTrack::TimeSignatureTrack(int initialNum, int initialDenom)
    : initialNumerator(juce::jlimit(1, 32, initialNum))
    , initialDenominator(juce::jlimit(1, 64, initialDenom))
{
}
