#include "PianoRoll.h"
#include <algorithm>
#include <vector>

PianoRoll::PianoRoll()
{
    setWantsKeyboardFocus(true); // Enable keyboard focus for shortcuts
}

void PianoRoll::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark);

    auto bounds = getLocalBounds();

    // Draw piano keyboard on left
    auto keyboardBounds = bounds.removeFromLeft(KEYBOARD_WIDTH);
    drawKeyboard(g, keyboardBounds);

    // Draw note grid and notes
    drawNoteGrid(g, bounds);
    drawNotes(g, bounds);
}

void PianoRoll::resized()
{
}

void PianoRoll::mouseDown(const juce::MouseEvent& e)
{
    // Grab keyboard focus for shortcuts
    grabKeyboardFocus();

    if (midiRegion == nullptr)
        return;

    if (e.x > KEYBOARD_WIDTH)
    {
        // Check if clicking on an existing note
        int clickedNoteIndex = getNoteAtPosition(e.x, e.y);

        if (clickedNoteIndex >= 0)
        {
            // Clicked on a note - handle selection
            if (e.mods.isShiftDown())
            {
                // Shift+click: toggle selection (multi-select)
                if (selectedNoteIndices.count(clickedNoteIndex) > 0)
                {
                    selectedNoteIndices.erase(clickedNoteIndex);
                }
                else
                {
                    selectedNoteIndices.insert(clickedNoteIndex);
                }
            }
            else
            {
                // Regular click: select only this note (clear previous selection)
                selectedNoteIndices.clear();
                selectedNoteIndices.insert(clickedNoteIndex);
            }

            isDragging = false;
            isCreatingNote = false;
            repaint();
            return;
        }

        // Clicked on empty space - clear selection and start creating a new note
        if (!e.mods.isShiftDown())
        {
            clearSelection();
        }

        dragStartNote = yToNote(e.y);
        dragStartBeat = xToBeat(e.x);
        isDragging = true;
        isCreatingNote = true;
    }
}

void PianoRoll::mouseDrag(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    // Could show preview of note being drawn
}

void PianoRoll::mouseUp(const juce::MouseEvent& e)
{
    if (!isDragging || !isCreatingNote || midiRegion == nullptr)
    {
        isDragging = false;
        isCreatingNote = false;
        return;
    }

    int endNote = yToNote(e.y);
    double endBeat = xToBeat(e.x);

    // Create note from drag start to end
    if (dragStartNote == endNote && endBeat > dragStartBeat)
    {
        auto& seq = midiRegion->getMidiSequence();

        // Convert beats to ticks (assuming 960 ticks per beat)
        double ticksPerBeat = 960.0;
        double startTicks = dragStartBeat * ticksPerBeat;
        double endTicks = endBeat * ticksPerBeat;

        // Add note on
        seq.addEvent(juce::MidiMessage::noteOn(1, dragStartNote, 0.8f).withTimeStamp(startTicks));
        // Add note off
        seq.addEvent(juce::MidiMessage::noteOff(1, dragStartNote, 0.0f).withTimeStamp(endTicks));
        seq.updateMatchedPairs();

        repaint();
    }

    isDragging = false;
    isCreatingNote = false;
}

void PianoRoll::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown())
    {
        // Horizontal zoom
        if (wheel.deltaY > 0)
            setPixelsPerBeat(pixelsPerBeat * 1.1);
        else if (wheel.deltaY < 0)
            setPixelsPerBeat(pixelsPerBeat / 1.1);
    }
    else if (e.mods.isShiftDown())
    {
        // Horizontal scroll
        horizontalScrollOffset -= wheel.deltaY * 50.0;
        horizontalScrollOffset = juce::jmax(0.0, horizontalScrollOffset);
        repaint();
    }
    else
    {
        // Vertical scroll
        verticalScrollOffset -= static_cast<int>(wheel.deltaY * 50.0f);
        verticalScrollOffset = juce::jlimit(0, NUM_NOTES * noteHeight - getHeight(), verticalScrollOffset);
        repaint();
    }
}

void PianoRoll::drawKeyboard(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(MidiSingLookAndFeel::backgroundMid);
    g.fillRect(bounds);

    // Draw keys from top (high notes) to bottom (low notes)
    for (int note = 0; note < NUM_NOTES; ++note)
    {
        int y = noteToY(note);
        if (y < -noteHeight || y > bounds.getHeight())
            continue;

        bool isBlack = isBlackKey(note);
        
        // Key background
        g.setColour(isBlack ? juce::Colour(0xff2a2a2a) : juce::Colour(0xfff0f0f0));
        int keyWidth = isBlack ? bounds.getWidth() * 2 / 3 : bounds.getWidth();
        g.fillRect(bounds.getX(), y, keyWidth - 1, noteHeight - 1);

        // Note name for C notes
        if (note % 12 == 0)
        {
            g.setColour(juce::Colours::black);
            g.setFont(9.0f);
            g.drawText(getNoteName(note), bounds.getX() + 2, y, 30, noteHeight,
                       juce::Justification::centredLeft);
        }
    }

    // Right border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawLine(static_cast<float>(bounds.getRight() - 1), static_cast<float>(bounds.getY()),
               static_cast<float>(bounds.getRight() - 1), static_cast<float>(bounds.getBottom()));
}

void PianoRoll::drawNoteGrid(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Draw horizontal lines (note rows)
    for (int note = 0; note < NUM_NOTES; ++note)
    {
        int y = noteToY(note);
        if (y < -noteHeight || y > bounds.getHeight())
            continue;

        // Alternating row colours
        bool isBlack = isBlackKey(note);
        g.setColour(isBlack ? MidiSingLookAndFeel::backgroundDark.darker(0.3f)
                            : MidiSingLookAndFeel::backgroundMid);
        g.fillRect(bounds.getX(), y, bounds.getWidth(), noteHeight);

        // Row separator
        g.setColour(MidiSingLookAndFeel::borderColour.withAlpha(0.3f));
        g.drawHorizontalLine(y + noteHeight - 1, static_cast<float>(bounds.getX()),
                             static_cast<float>(bounds.getRight()));
    }

    // Draw vertical lines (beat grid)
    double startBeat = horizontalScrollOffset / pixelsPerBeat;
    double endBeat = startBeat + bounds.getWidth() / pixelsPerBeat;

    for (int beat = static_cast<int>(startBeat); beat <= static_cast<int>(endBeat) + 1; ++beat)
    {
        int x = beatToX(beat);
        if (x < bounds.getX() || x > bounds.getRight())
            continue;

        bool isBar = (beat % 4) == 0;
        g.setColour(isBar ? MidiSingLookAndFeel::borderColour
                          : MidiSingLookAndFeel::borderColour.withAlpha(0.3f));
        g.drawLine(static_cast<float>(x), static_cast<float>(bounds.getY()),
                   static_cast<float>(x), static_cast<float>(bounds.getBottom()),
                   isBar ? 1.0f : 0.5f);
    }
}

void PianoRoll::drawNotes(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (midiRegion == nullptr)
        return;

    const auto& seq = midiRegion->getMidiSequence();
    double ticksPerBeat = 960.0;

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        auto* event = seq.getEventPointer(i);
        if (event == nullptr || !event->message.isNoteOn())
            continue;

        int noteNumber = event->message.getNoteNumber();
        double startBeat = event->message.getTimeStamp() / ticksPerBeat;

        // Find matching note off
        double endBeat = startBeat + 0.5; // Default length
        if (event->noteOffObject != nullptr)
        {
            endBeat = event->noteOffObject->message.getTimeStamp() / ticksPerBeat;
        }

        // Calculate position
        int x = beatToX(startBeat);
        int y = noteToY(noteNumber);
        int width = beatToX(endBeat) - x;

        if (x > bounds.getRight() || x + width < bounds.getX())
            continue;
        if (y < -noteHeight || y > bounds.getHeight())
            continue;

        // Draw note
        auto noteRect = juce::Rectangle<int>(x, y + 1, juce::jmax(4, width - 1), noteHeight - 2);

        // Check if this note is selected
        bool isSelected = isNoteSelected(i);

        // Note colour based on velocity and selection state
        float velocity = event->message.getFloatVelocity();
        auto noteColour = MidiSingLookAndFeel::accentColour.withAlpha(0.7f + velocity * 0.3f);

        if (isSelected)
        {
            // Selected notes are brighter with a highlighted border
            noteColour = noteColour.brighter(0.4f);
        }

        g.setColour(noteColour);
        g.fillRoundedRectangle(noteRect.toFloat(), 2.0f);

        // Draw border - thicker and brighter for selected notes
        if (isSelected)
        {
            g.setColour(juce::Colours::white);
            g.drawRoundedRectangle(noteRect.toFloat(), 2.0f, 2.0f);
        }
        else
        {
            g.setColour(noteColour.brighter(0.3f));
            g.drawRoundedRectangle(noteRect.toFloat(), 2.0f, 1.0f);
        }
    }
}

bool PianoRoll::isBlackKey(int noteNumber) const
{
    int note = noteNumber % 12;
    return note == 1 || note == 3 || note == 6 || note == 8 || note == 10;
}

juce::String PianoRoll::getNoteName(int noteNumber) const
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int octave = (noteNumber / 12) - 1;
    return juce::String(names[noteNumber % 12]) + juce::String(octave);
}

int PianoRoll::noteToY(int noteNumber) const
{
    // Higher notes at top (lower Y)
    return (NUM_NOTES - 1 - noteNumber) * noteHeight - verticalScrollOffset;
}

int PianoRoll::yToNote(int y) const
{
    return NUM_NOTES - 1 - (y + verticalScrollOffset) / noteHeight;
}

double PianoRoll::xToBeat(int x) const
{
    return (x - KEYBOARD_WIDTH + horizontalScrollOffset) / pixelsPerBeat;
}

int PianoRoll::beatToX(double beat) const
{
    return KEYBOARD_WIDTH + static_cast<int>(beat * pixelsPerBeat - horizontalScrollOffset);
}

void PianoRoll::clearSelection()
{
    if (!selectedNoteIndices.empty())
    {
        selectedNoteIndices.clear();
        repaint();
    }
}

bool PianoRoll::isNoteSelected(int eventIndex) const
{
    return selectedNoteIndices.count(eventIndex) > 0;
}

int PianoRoll::getNoteAtPosition(int x, int y) const
{
    if (midiRegion == nullptr || x <= KEYBOARD_WIDTH)
        return -1;

    const auto& seq = midiRegion->getMidiSequence();
    double ticksPerBeat = 960.0;

    // Iterate through all note events and check if position is inside
    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        auto* event = seq.getEventPointer(i);
        if (event == nullptr || !event->message.isNoteOn())
            continue;

        int noteNumber = event->message.getNoteNumber();
        double startBeat = event->message.getTimeStamp() / ticksPerBeat;

        // Find matching note off
        double endBeat = startBeat + 0.5; // Default length
        if (event->noteOffObject != nullptr)
        {
            endBeat = event->noteOffObject->message.getTimeStamp() / ticksPerBeat;
        }

        // Calculate note bounds
        int noteX = beatToX(startBeat);
        int noteY = noteToY(noteNumber);
        int noteWidth = beatToX(endBeat) - noteX;
        noteWidth = juce::jmax(4, noteWidth - 1);

        juce::Rectangle<int> noteRect(noteX, noteY + 1, noteWidth, noteHeight - 2);

        if (noteRect.contains(x, y))
        {
            return i;
        }
    }

    return -1;
}

juce::Rectangle<int> PianoRoll::getNoteRect(int eventIndex) const
{
    if (midiRegion == nullptr)
        return juce::Rectangle<int>();

    const auto& seq = midiRegion->getMidiSequence();
    if (eventIndex < 0 || eventIndex >= seq.getNumEvents())
        return juce::Rectangle<int>();

    auto* event = seq.getEventPointer(eventIndex);
    if (event == nullptr || !event->message.isNoteOn())
        return juce::Rectangle<int>();

    double ticksPerBeat = 960.0;
    int noteNumber = event->message.getNoteNumber();
    double startBeat = event->message.getTimeStamp() / ticksPerBeat;

    // Find matching note off
    double endBeat = startBeat + 0.5;
    if (event->noteOffObject != nullptr)
    {
        endBeat = event->noteOffObject->message.getTimeStamp() / ticksPerBeat;
    }

    // Calculate note bounds
    int noteX = beatToX(startBeat);
    int noteY = noteToY(noteNumber);
    int noteWidth = beatToX(endBeat) - noteX;
    noteWidth = juce::jmax(4, noteWidth - 1);

    return juce::Rectangle<int>(noteX, noteY + 1, noteWidth, noteHeight - 2);
}

bool PianoRoll::keyPressed(const juce::KeyPress& key)
{
    // Delete key - delete selected notes
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (!selectedNoteIndices.empty())
        {
            deleteSelectedNotes();
            return true;
        }
    }

    return false;
}

void PianoRoll::deleteSelectedNotes()
{
    if (midiRegion == nullptr || selectedNoteIndices.empty())
        return;

    auto& seq = midiRegion->getMidiSequence();

    // Collect all events to delete (both note-on and note-off)
    // We need to delete in reverse order to maintain valid indices
    std::vector<int> indicesToDelete;

    for (int eventIndex : selectedNoteIndices)
    {
        if (eventIndex >= 0 && eventIndex < seq.getNumEvents())
        {
            auto* event = seq.getEventPointer(eventIndex);
            if (event != nullptr && event->message.isNoteOn())
            {
                // Add note-on index
                indicesToDelete.push_back(eventIndex);

                // Find and add the matching note-off index
                if (event->noteOffObject != nullptr)
                {
                    // Find the index of the note-off event
                    for (int i = 0; i < seq.getNumEvents(); ++i)
                    {
                        if (seq.getEventPointer(i) == event->noteOffObject)
                        {
                            indicesToDelete.push_back(i);
                            break;
                        }
                    }
                }
            }
        }
    }

    // Sort indices in descending order to delete from end to start
    // This prevents index shifting issues during deletion
    std::sort(indicesToDelete.begin(), indicesToDelete.end(), std::greater<int>());

    // Remove duplicates
    indicesToDelete.erase(std::unique(indicesToDelete.begin(), indicesToDelete.end()), indicesToDelete.end());

    // Delete events in reverse order
    for (int index : indicesToDelete)
    {
        seq.deleteEvent(index, false);
    }

    // Update matched pairs after deletion
    seq.updateMatchedPairs();

    // Clear selection since the notes are deleted
    selectedNoteIndices.clear();

    repaint();
}
