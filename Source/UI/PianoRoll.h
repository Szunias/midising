#pragma once

#include "../Timeline/Region.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * PianoRoll is an editor for MIDI notes.
 * Shows piano keys on left, note grid, and allows note editing.
 */
class PianoRoll : public juce::Component
{
public:
    PianoRoll();
    ~PianoRoll() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Mouse handling for note editing
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Set the MIDI region to edit
    void setMidiRegion(MidiRegion* region) { midiRegion = region; repaint(); }
    MidiRegion* getMidiRegion() const { return midiRegion; }

    // View settings
    void setPixelsPerBeat(double ppb) { pixelsPerBeat = juce::jlimit(10.0, 200.0, ppb); repaint(); }
    double getPixelsPerBeat() const { return pixelsPerBeat; }
    void setNoteHeight(int height) { noteHeight = juce::jlimit(6, 30, height); repaint(); }
    int getNoteHeight() const { return noteHeight; }

    // Scrolling
    void setVerticalScrollOffset(int offset) { verticalScrollOffset = juce::jmax(0, offset); repaint(); }
    int getVerticalScrollOffset() const { return verticalScrollOffset; }
    void setHorizontalScrollOffset(double offset) { horizontalScrollOffset = juce::jmax(0.0, offset); repaint(); }
    double getHorizontalScrollOffset() const { return horizontalScrollOffset; }

    static constexpr int KEYBOARD_WIDTH = 60;
    static constexpr int NUM_NOTES = 128;

private:
    void drawKeyboard(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawNoteGrid(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawNotes(juce::Graphics& g, juce::Rectangle<int> bounds);
    bool isBlackKey(int noteNumber) const;
    juce::String getNoteName(int noteNumber) const;

    int noteToY(int noteNumber) const;
    int yToNote(int y) const;
    double xToBeat(int x) const;
    int beatToX(double beat) const;

    MidiRegion* midiRegion = nullptr;

    double pixelsPerBeat = 40.0;
    int noteHeight = 12;
    int verticalScrollOffset = 60 * 12; // Start around middle C
    double horizontalScrollOffset = 0.0;

    // For note editing
    bool isDragging = false;
    int dragStartNote = -1;
    double dragStartBeat = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRoll)
};
