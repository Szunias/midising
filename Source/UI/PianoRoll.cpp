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

    // Draw resize ghost for visual feedback during note resizing
    drawResizeGhost(g, bounds);

    // Draw box selection rectangle for Select tool
    drawBoxSelection(g, bounds);
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
        // Dispatch to tool-specific handler
        switch (currentTool)
        {
            case Tool::Draw:
                handleDrawToolMouseDown(e);
                break;
            case Tool::Erase:
                handleEraseToolMouseDown(e);
                break;
            case Tool::Select:
                handleSelectToolMouseDown(e);
                break;
            case Tool::Split:
                handleSplitToolMouseDown(e);
                break;
            case Tool::Mute:
                handleMuteToolMouseDown(e);
                break;
        }
    }
}

void PianoRoll::mouseDrag(const juce::MouseEvent& e)
{
    // Handle velocity editing - drag up to increase, down to decrease
    if (isEditingVelocity && velocityEditNoteIndex >= 0 && midiRegion != nullptr)
    {
        auto& seq = midiRegion->getMidiSequence();
        auto* event = seq.getEventPointer(velocityEditNoteIndex);

        if (event != nullptr && event->message.isNoteOn())
        {
            // Calculate velocity delta based on vertical drag
            // Dragging up (negative deltaY) increases velocity
            // Use a sensitivity factor to make editing feel natural
            int deltaY = velocityEditStartY - e.y;
            float velocitySensitivity = 0.005f; // Adjust for desired sensitivity
            float newVelocity = velocityEditOriginalVelocity + (deltaY * velocitySensitivity);

            // Clamp velocity to valid range (0.0 to 1.0)
            newVelocity = juce::jlimit(0.01f, 1.0f, newVelocity);

            // Update the note's velocity
            event->message.setVelocity(newVelocity);

            repaint();
        }
        return;
    }

    // Handle note resizing
    if (isResizingNote && resizeNoteIndex >= 0 && midiRegion != nullptr)
    {
        const auto& seq = midiRegion->getMidiSequence();
        auto* event = seq.getEventPointer(resizeNoteIndex);

        if (event != nullptr && event->message.isNoteOn())
        {
            double ticksPerBeat = 960.0;
            double startBeat = event->message.getTimeStamp() / ticksPerBeat;

            // Calculate new end beat from mouse position
            double newEndBeat = xToBeat(e.x);

            // Ensure minimum length (at least 1/16 of a beat)
            double minLength = 0.0625;
            newEndBeat = juce::jmax(startBeat + minLength, newEndBeat);

            resizeCurrentEndBeat = newEndBeat;
            repaint();
        }
        return;
    }

    // Handle Select tool box selection drag
    if (currentTool == Tool::Select && isBoxSelecting)
    {
        handleSelectToolMouseDrag(e);
        return;
    }

    // Handle Draw tool drag (for note preview)
    if (currentTool == Tool::Draw && isDragging)
    {
        handleDrawToolMouseDrag(e);
        return;
    }
}

void PianoRoll::mouseUp(const juce::MouseEvent& e)
{
    // Finalize velocity editing
    if (isEditingVelocity)
    {
        // Velocity changes are already applied during drag, just reset state
        isEditingVelocity = false;
        velocityEditNoteIndex = -1;
        repaint();
        return;
    }

    // Finalize note resize operation
    if (isResizingNote && resizeNoteIndex >= 0 && midiRegion != nullptr)
    {
        auto& seq = midiRegion->getMidiSequence();
        auto* event = seq.getEventPointer(resizeNoteIndex);

        if (event != nullptr && event->message.isNoteOn() && event->noteOffObject != nullptr)
        {
            double ticksPerBeat = 960.0;

            // Update the note-off time to the new end position
            double newEndTicks = resizeCurrentEndBeat * ticksPerBeat;
            event->noteOffObject->message.setTimeStamp(newEndTicks);

            // Re-sort and update matched pairs after modifying timestamps
            seq.sort();
            seq.updateMatchedPairs();
        }

        isResizingNote = false;
        resizeNoteIndex = -1;
        repaint();
        return;
    }

    // Handle Select tool box selection completion
    if (currentTool == Tool::Select && isBoxSelecting)
    {
        handleSelectToolMouseUp(e);
        return;
    }

    // Handle Draw tool note creation
    if (currentTool == Tool::Draw)
    {
        handleDrawToolMouseUp(e);
        return;
    }

    isDragging = false;
    isCreatingNote = false;
}

void PianoRoll::mouseMove(const juce::MouseEvent& e)
{
    // Update cursor based on position (edge detection for resize)
    updateCursorForPosition(e.x, e.y);
}

void PianoRoll::mouseExit(const juce::MouseEvent& /*e*/)
{
    // Reset cursor when mouse leaves component
    setMouseCursor(juce::MouseCursor::NormalCursor);
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

        // Check if this note is selected or muted
        bool isSelected = isNoteSelected(i);
        bool isMuted = isNoteMuted(i);

        // Note colour based on velocity, selection, and mute state
        float velocity = event->message.getFloatVelocity();
        juce::Colour noteColour;

        if (isMuted)
        {
            // Muted notes are gray and semi-transparent
            noteColour = juce::Colour(0xff666666).withAlpha(0.5f);
        }
        else
        {
            noteColour = MidiSingLookAndFeel::accentColour.withAlpha(0.7f + velocity * 0.3f);
        }

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
        else if (isMuted)
        {
            // Dashed appearance for muted notes - draw lighter border
            g.setColour(juce::Colour(0xff888888));
            g.drawRoundedRectangle(noteRect.toFloat(), 2.0f, 1.0f);
        }
        else
        {
            g.setColour(noteColour.brighter(0.3f));
            g.drawRoundedRectangle(noteRect.toFloat(), 2.0f, 1.0f);
        }

        // Draw mute indicator "M" on muted notes
        if (isMuted && noteRect.getWidth() > 15)
        {
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.setFont(juce::Font(8.0f));
            g.drawText("M", noteRect.reduced(2, 0), juce::Justification::centredLeft, false);
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

PianoRoll::NoteEdge PianoRoll::getNoteEdgeAtPosition(int x, int y, int& outNoteIndex) const
{
    outNoteIndex = -1;

    if (midiRegion == nullptr || x <= KEYBOARD_WIDTH)
        return NoteEdge::None;

    const auto& seq = midiRegion->getMidiSequence();
    double ticksPerBeat = 960.0;

    // Iterate through all note events and check for edge proximity
    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        auto* event = seq.getEventPointer(i);
        if (event == nullptr || !event->message.isNoteOn())
            continue;

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

        juce::Rectangle<int> noteRect(noteX, noteY + 1, noteWidth, noteHeight - 2);

        // Check if y is within the note's vertical bounds
        if (y < noteRect.getY() || y > noteRect.getBottom())
            continue;

        // Check right edge
        int rightEdge = noteRect.getRight();
        if (x >= rightEdge - EDGE_DETECT_WIDTH / 2 &&
            x <= rightEdge + EDGE_DETECT_WIDTH / 2)
        {
            outNoteIndex = i;
            return NoteEdge::Right;
        }
    }

    return NoteEdge::None;
}

void PianoRoll::updateCursorForPosition(int x, int y)
{
    // For Draw tool, check edge detection for resize
    if (currentTool == Tool::Draw)
    {
        int noteIndex = -1;
        NoteEdge edge = getNoteEdgeAtPosition(x, y, noteIndex);

        if (edge == NoteEdge::Right)
        {
            // Show horizontal resize cursor
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
    }

    // Use tool-specific cursor
    updateCursorForTool();
}

void PianoRoll::updateCursorForTool()
{
    switch (currentTool)
    {
        case Tool::Draw:
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            break;
        case Tool::Erase:
            // No specific cursor in JUCE for eraser, using pointing hand
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            break;
        case Tool::Select:
            setMouseCursor(juce::MouseCursor::NormalCursor);
            break;
        case Tool::Split:
            // Crosshair for precision
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            break;
        case Tool::Mute:
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            break;
    }
}

void PianoRoll::drawResizeGhost(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (!isResizingNote || resizeNoteIndex < 0 || midiRegion == nullptr)
        return;

    const auto& seq = midiRegion->getMidiSequence();
    auto* event = seq.getEventPointer(resizeNoteIndex);

    if (event == nullptr || !event->message.isNoteOn())
        return;

    double ticksPerBeat = 960.0;
    int noteNumber = event->message.getNoteNumber();
    double startBeat = event->message.getTimeStamp() / ticksPerBeat;

    // Calculate the ghost note rectangle using the current resize end beat
    int x = beatToX(startBeat);
    int y = noteToY(noteNumber);
    int width = beatToX(resizeCurrentEndBeat) - x;

    if (x > bounds.getRight() || x + width < bounds.getX())
        return;
    if (y < -noteHeight || y > bounds.getHeight())
        return;

    auto ghostRect = juce::Rectangle<int>(x, y + 1, juce::jmax(4, width - 1), noteHeight - 2);

    // Draw semi-transparent ghost of the resized note
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.4f));
    g.fillRoundedRectangle(ghostRect.toFloat(), 2.0f);

    // Draw ghost border
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.9f));
    g.drawRoundedRectangle(ghostRect.toFloat(), 2.0f, 2.0f);

    // Draw indicator line at the new end position
    int endX = beatToX(resizeCurrentEndBeat);
    g.setColour(MidiSingLookAndFeel::accentColour);
    g.drawLine(static_cast<float>(endX), static_cast<float>(bounds.getY()),
               static_cast<float>(endX), static_cast<float>(bounds.getBottom()), 1.0f);
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

    // Tool switching shortcuts
    // D = Draw tool
    if (key.getTextCharacter() == 'd' || key.getTextCharacter() == 'D')
    {
        setTool(Tool::Draw);
        return true;
    }

    // E = Erase tool
    if (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E')
    {
        setTool(Tool::Erase);
        return true;
    }

    // S = Select tool
    if (key.getTextCharacter() == 's' || key.getTextCharacter() == 'S')
    {
        setTool(Tool::Select);
        return true;
    }

    // P = Split tool (P for "partition")
    if (key.getTextCharacter() == 'p' || key.getTextCharacter() == 'P')
    {
        setTool(Tool::Split);
        return true;
    }

    // M = Mute tool
    if (key.getTextCharacter() == 'm' || key.getTextCharacter() == 'M')
    {
        setTool(Tool::Mute);
        return true;
    }

    // Number keys 1-5 for quick tool selection
    if (key.getTextCharacter() == '1')
    {
        setTool(Tool::Draw);
        return true;
    }
    if (key.getTextCharacter() == '2')
    {
        setTool(Tool::Erase);
        return true;
    }
    if (key.getTextCharacter() == '3')
    {
        setTool(Tool::Select);
        return true;
    }
    if (key.getTextCharacter() == '4')
    {
        setTool(Tool::Split);
        return true;
    }
    if (key.getTextCharacter() == '5')
    {
        setTool(Tool::Mute);
        return true;
    }

    // Ctrl+A = Select all notes
    if (key.getModifiers().isCommandDown() &&
        (key.getTextCharacter() == 'a' || key.getTextCharacter() == 'A'))
    {
        if (midiRegion != nullptr)
        {
            const auto& seq = midiRegion->getMidiSequence();
            selectedNoteIndices.clear();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* event = seq.getEventPointer(i);
                if (event != nullptr && event->message.isNoteOn())
                {
                    selectedNoteIndices.insert(i);
                }
            }
            repaint();
        }
        return true;
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

// ============================================================================
// Mute functionality
// ============================================================================

bool PianoRoll::isNoteMuted(int eventIndex) const
{
    return mutedNoteIndices.count(eventIndex) > 0;
}

void PianoRoll::toggleNoteMute(int eventIndex)
{
    if (mutedNoteIndices.count(eventIndex) > 0)
    {
        mutedNoteIndices.erase(eventIndex);
    }
    else
    {
        mutedNoteIndices.insert(eventIndex);
    }
    repaint();
}

void PianoRoll::muteSelectedNotes()
{
    for (int index : selectedNoteIndices)
    {
        mutedNoteIndices.insert(index);
    }
    repaint();
}

void PianoRoll::unmuteSelectedNotes()
{
    for (int index : selectedNoteIndices)
    {
        mutedNoteIndices.erase(index);
    }
    repaint();
}

// ============================================================================
// Split functionality
// ============================================================================

void PianoRoll::splitNoteAtBeat(int eventIndex, double splitBeat)
{
    if (midiRegion == nullptr)
        return;

    auto& seq = midiRegion->getMidiSequence();
    if (eventIndex < 0 || eventIndex >= seq.getNumEvents())
        return;

    auto* event = seq.getEventPointer(eventIndex);
    if (event == nullptr || !event->message.isNoteOn())
        return;

    double ticksPerBeat = 960.0;
    double startBeat = event->message.getTimeStamp() / ticksPerBeat;

    // Get end beat
    double endBeat = startBeat + 0.5;
    if (event->noteOffObject != nullptr)
    {
        endBeat = event->noteOffObject->message.getTimeStamp() / ticksPerBeat;
    }

    // Ensure split position is within the note
    if (splitBeat <= startBeat || splitBeat >= endBeat)
        return;

    // Get note properties
    int noteNumber = event->message.getNoteNumber();
    float velocity = event->message.getFloatVelocity();
    int channel = event->message.getChannel();

    // Calculate split position in ticks
    double splitTicks = splitBeat * ticksPerBeat;
    double endTicks = endBeat * ticksPerBeat;

    // Modify the original note to end at the split point
    if (event->noteOffObject != nullptr)
    {
        event->noteOffObject->message.setTimeStamp(splitTicks);
    }

    // Add new note from split point to original end
    seq.addEvent(juce::MidiMessage::noteOn(channel, noteNumber, velocity).withTimeStamp(splitTicks));
    seq.addEvent(juce::MidiMessage::noteOff(channel, noteNumber, 0.0f).withTimeStamp(endTicks));

    // Re-sort and update matched pairs
    seq.sort();
    seq.updateMatchedPairs();

    repaint();
}

// ============================================================================
// Box selection drawing
// ============================================================================

void PianoRoll::drawBoxSelection(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (!isBoxSelecting)
        return;

    // Calculate the selection rectangle
    int left = juce::jmin(boxSelectStart.x, boxSelectEnd.x);
    int top = juce::jmin(boxSelectStart.y, boxSelectEnd.y);
    int right = juce::jmax(boxSelectStart.x, boxSelectEnd.x);
    int bottom = juce::jmax(boxSelectStart.y, boxSelectEnd.y);

    auto selectionRect = juce::Rectangle<int>(left, top, right - left, bottom - top);

    // Clip to bounds
    selectionRect = selectionRect.getIntersection(bounds);

    if (selectionRect.isEmpty())
        return;

    // Draw semi-transparent fill
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.2f));
    g.fillRect(selectionRect);

    // Draw border
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.8f));
    g.drawRect(selectionRect, 1);
}

// ============================================================================
// Tool-specific mouse handlers
// ============================================================================

void PianoRoll::handleDrawToolMouseDown(const juce::MouseEvent& e)
{
    // First, check if clicking on a note edge for resizing
    int edgeNoteIndex = -1;
    NoteEdge edge = getNoteEdgeAtPosition(e.x, e.y, edgeNoteIndex);

    if (edge == NoteEdge::Right && edgeNoteIndex >= 0)
    {
        // Start resize operation
        const auto& seq = midiRegion->getMidiSequence();
        auto* event = seq.getEventPointer(edgeNoteIndex);

        if (event != nullptr && event->message.isNoteOn())
        {
            double ticksPerBeat = 960.0;
            double startBeat = event->message.getTimeStamp() / ticksPerBeat;

            // Get current end beat
            double endBeat = startBeat + 0.5;
            if (event->noteOffObject != nullptr)
            {
                endBeat = event->noteOffObject->message.getTimeStamp() / ticksPerBeat;
            }

            // Select the note being resized
            selectedNoteIndices.clear();
            selectedNoteIndices.insert(edgeNoteIndex);

            isResizingNote = true;
            resizeNoteIndex = edgeNoteIndex;
            resizeOriginalEndBeat = endBeat;
            resizeCurrentEndBeat = endBeat;
            repaint();
            return;
        }
    }

    // Check if clicking on an existing note
    int clickedNoteIndex = getNoteAtPosition(e.x, e.y);

    if (clickedNoteIndex >= 0)
    {
        // Check if clicking on an already-selected note to start velocity editing
        if (selectedNoteIndices.count(clickedNoteIndex) > 0 && !e.mods.isShiftDown())
        {
            // Start velocity editing mode
            const auto& seq = midiRegion->getMidiSequence();
            auto* event = seq.getEventPointer(clickedNoteIndex);

            if (event != nullptr && event->message.isNoteOn())
            {
                isEditingVelocity = true;
                velocityEditNoteIndex = clickedNoteIndex;
                velocityEditStartY = e.y;
                velocityEditOriginalVelocity = event->message.getFloatVelocity();
                isDragging = false;
                isCreatingNote = false;
                return;
            }
        }

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

void PianoRoll::handleDrawToolMouseDrag(const juce::MouseEvent& /*e*/)
{
    // Could show preview of note being drawn
    // For now, we just update the view during drag
    repaint();
}

void PianoRoll::handleDrawToolMouseUp(const juce::MouseEvent& e)
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

void PianoRoll::handleEraseToolMouseDown(const juce::MouseEvent& e)
{
    // Find note at click position and delete it
    int clickedNoteIndex = getNoteAtPosition(e.x, e.y);

    if (clickedNoteIndex >= 0 && midiRegion != nullptr)
    {
        auto& seq = midiRegion->getMidiSequence();
        auto* event = seq.getEventPointer(clickedNoteIndex);

        if (event != nullptr && event->message.isNoteOn())
        {
            std::vector<int> indicesToDelete;
            indicesToDelete.push_back(clickedNoteIndex);

            // Find and add the matching note-off index
            if (event->noteOffObject != nullptr)
            {
                for (int i = 0; i < seq.getNumEvents(); ++i)
                {
                    if (seq.getEventPointer(i) == event->noteOffObject)
                    {
                        indicesToDelete.push_back(i);
                        break;
                    }
                }
            }

            // Sort in descending order
            std::sort(indicesToDelete.begin(), indicesToDelete.end(), std::greater<int>());

            // Delete events
            for (int index : indicesToDelete)
            {
                seq.deleteEvent(index, false);
            }

            seq.updateMatchedPairs();

            // Remove from selection and muted sets
            selectedNoteIndices.erase(clickedNoteIndex);
            mutedNoteIndices.erase(clickedNoteIndex);

            repaint();
        }
    }
}

void PianoRoll::handleSelectToolMouseDown(const juce::MouseEvent& e)
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
        repaint();
    }
    else
    {
        // Clicked on empty space - start box selection
        if (!e.mods.isShiftDown())
        {
            clearSelection();
        }

        isBoxSelecting = true;
        boxSelectStart = e.getPosition();
        boxSelectEnd = e.getPosition();
    }
}

void PianoRoll::handleSelectToolMouseDrag(const juce::MouseEvent& e)
{
    if (isBoxSelecting)
    {
        boxSelectEnd = e.getPosition();
        repaint();
    }
}

void PianoRoll::handleSelectToolMouseUp(const juce::MouseEvent& e)
{
    if (isBoxSelecting && midiRegion != nullptr)
    {
        boxSelectEnd = e.getPosition();

        // Calculate the selection rectangle
        int left = juce::jmin(boxSelectStart.x, boxSelectEnd.x);
        int top = juce::jmin(boxSelectStart.y, boxSelectEnd.y);
        int right = juce::jmax(boxSelectStart.x, boxSelectEnd.x);
        int bottom = juce::jmax(boxSelectStart.y, boxSelectEnd.y);

        auto selectionRect = juce::Rectangle<int>(left, top, right - left, bottom - top);

        // Find all notes that intersect with the selection rectangle
        const auto& seq = midiRegion->getMidiSequence();
        double ticksPerBeat = 960.0;

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event == nullptr || !event->message.isNoteOn())
                continue;

            // Get note rectangle
            juce::Rectangle<int> noteRect = getNoteRect(i);

            if (selectionRect.intersects(noteRect))
            {
                selectedNoteIndices.insert(i);
            }
        }

        isBoxSelecting = false;
        repaint();
    }
}

void PianoRoll::handleSplitToolMouseDown(const juce::MouseEvent& e)
{
    // Find note at click position
    int clickedNoteIndex = getNoteAtPosition(e.x, e.y);

    if (clickedNoteIndex >= 0)
    {
        // Get the beat position where the user clicked
        double splitBeat = xToBeat(e.x);

        // Split the note at this position
        splitNoteAtBeat(clickedNoteIndex, splitBeat);
    }
}

void PianoRoll::handleMuteToolMouseDown(const juce::MouseEvent& e)
{
    // Find note at click position
    int clickedNoteIndex = getNoteAtPosition(e.x, e.y);

    if (clickedNoteIndex >= 0)
    {
        // Toggle mute state of the clicked note
        toggleNoteMute(clickedNoteIndex);
    }
}
