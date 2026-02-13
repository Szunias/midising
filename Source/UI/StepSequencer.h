#pragma once

#include "../Timeline/Region.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

/**
 * Grid-based drum pattern editor component.
 * Displays a step sequencer grid with configurable rows (drum instruments)
 * and steps. Each cell can be toggled active/inactive with adjustable velocity.
 * Supports playback indication, randomization, and export to MidiRegion.
 *
 * Default configuration uses General MIDI drum map for 8 common percussion sounds.
 */
class StepSequencer : public juce::Component
{
public:
    static constexpr int MAX_STEPS = 32;
    static constexpr int MAX_ROWS  = 16;

    /** Single cell in the step sequencer grid. */
    struct StepCell
    {
        bool active = false;
        uint8_t velocity = 100;
    };

    /** Metadata for a single row (instrument). */
    struct RowInfo
    {
        juce::String name;
        int midiNote;
    };

    StepSequencer();
    ~StepSequencer() override = default;

    //==============================================================================
    // juce::Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    //==============================================================================
    // Playback

    /** Set the currently playing step index for visual highlighting (-1 = none). */
    void setCurrentStep(int step);
    int getCurrentStep() const { return currentStep; }

    //==============================================================================
    // Pattern editing

    /** Clear all cells in the grid. */
    void clear();

    /** Randomly activate approximately 30% of cells with random velocities. */
    void randomize();

    //==============================================================================
    // Configuration

    /** Set the number of active steps (16 or 32). Clamped to [1, MAX_STEPS]. */
    void setNumSteps(int steps);
    int getNumSteps() const { return numSteps; }

    /** Set the subdivision (steps per beat). Common values: 2, 4, 8. */
    void setSubdivision(int subdiv);
    int getSubdivision() const { return subdivision; }

    int getNumRows() const { return numRows; }

    //==============================================================================
    // MIDI export

    /**
     * Generate a MidiRegion from the current step pattern.
     * Each active cell creates a MIDI note-on/note-off pair at the correct
     * timing based on BPM and subdivision.
     *
     * @param bpm        Tempo in beats per minute
     * @param sampleRate Sample rate for calculating sample positions
     * @return A new MidiRegion containing the pattern, starting at position 0
     */
    std::unique_ptr<MidiRegion> generateMidiToRegion(double bpm, double sampleRate) const;

private:
    //==============================================================================
    // Grid data

    StepCell grid[MAX_ROWS][MAX_STEPS];
    int numSteps = 16;
    int numRows  = 8;
    int currentStep = -1;

    // Row instrument definitions (GM drum map defaults)
    RowInfo rowInfos[MAX_ROWS];

    // Timing
    int subdivision = 4;  // Steps per beat

    // Layout
    double pixelsPerStep = 24.0;
    int rowHeight   = 24;
    int headerWidth = 80;

    static constexpr int CONTROLS_HEIGHT = 32;

    //==============================================================================
    // UI controls

    juce::ComboBox stepCountCombo;
    juce::ComboBox subdivisionCombo;
    juce::TextButton clearButton   { "Clear" };
    juce::TextButton randomButton  { "Randomize" };

    //==============================================================================
    // Helpers

    /** Convert a grid coordinate to a screen rectangle for the cell. */
    juce::Rectangle<int> getCellRect(int row, int step) const;

    /** Convert a screen position to grid coordinates. Returns false if out of bounds. */
    bool screenToGrid(juce::Point<int> pos, int& outRow, int& outStep) const;

    /** Get an interpolated colour for a given velocity (0-127). */
    static juce::Colour getVelocityColour(uint8_t velocity);

    /** Initialize default GM drum row definitions. */
    void initDefaultRows();

    /** Set up control widgets and attach listeners. */
    void setupControls();

    // Velocity drag state
    bool isDraggingVelocity = false;
    int dragRow = -1;
    int dragStep = -1;
    int dragStartY = 0;
    uint8_t dragStartVelocity = 100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencer)
};
