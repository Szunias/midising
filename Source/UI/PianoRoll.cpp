#include "PianoRoll.h"
#include <algorithm>
#include <vector>
#include <cmath>
#include <random>

PianoRoll::PianoRoll()
{
    setWantsKeyboardFocus(true); // Enable keyboard focus for shortcuts
}

void PianoRoll::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark);

    auto bounds = getLocalBounds();

    // Calculate CC lanes area at the bottom
    int ccLanesHeight = getNumVisibleCCLanes() * CC_LANE_HEIGHT;
    auto ccLanesBounds = bounds.removeFromBottom(ccLanesHeight);

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

    // Draw CC lanes at the bottom
    if (ccLanesHeight > 0)
    {
        drawCCLanes(g, ccLanesBounds);
    }
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

    // Check if clicking in CC lane area
    if (isPointInCCLane(e.x, e.y))
    {
        handleCCLaneMouseDown(e);
        return;
    }

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
    // Handle CC lane editing
    if (isDraggingInCCLane && midiRegion != nullptr)
    {
        handleCCLaneMouseDrag(e);
        return;
    }

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

            // Apply quantization to end beat if enabled
            newEndBeat = quantizeBeatPosition(newEndBeat);

            // Ensure minimum length (at least 1 grid step)
            double minLength = quantizeEnabled ? getQuantizeGridSize() : 0.0625;
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
    // Finalize CC lane editing
    if (isDraggingInCCLane)
    {
        handleCCLaneMouseUp(e);
        return;
    }

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

    // Draw quantize grid lines (finer sub-beat grid)
    drawQuantizeGrid(g, bounds);

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

    // Q = Quantize selected notes
    if (key.getTextCharacter() == 'q' || key.getTextCharacter() == 'Q')
    {
        if (!selectedNoteIndices.empty())
        {
            quantizeSelectedNotes();
            return true;
        }
    }

    // G = Toggle quantize/snap-to-grid
    if (key.getTextCharacter() == 'g' || key.getTextCharacter() == 'G')
    {
        setQuantizeEnabled(!quantizeEnabled);
        return true;
    }

    // W = Toggle swing
    if ((key.getTextCharacter() == 'w' || key.getTextCharacter() == 'W') && !key.getModifiers().isShiftDown())
    {
        setSwingEnabled(!swingEnabled);
        return true;
    }

    // Shift+W = Apply swing to selected notes
    if ((key.getTextCharacter() == 'w' || key.getTextCharacter() == 'W') && key.getModifiers().isShiftDown())
    {
        if (!selectedNoteIndices.empty())
        {
            applySwingToSelectedNotes();
            return true;
        }
    }

    // Number keys 6-9 for quick quantize value selection (when Ctrl is held)
    if (key.getModifiers().isCommandDown())
    {
        if (key.getTextCharacter() == '6')
        {
            setQuantizeValue(QuantizeValue::Quarter);
            return true;
        }
        if (key.getTextCharacter() == '7')
        {
            setQuantizeValue(QuantizeValue::Eighth);
            return true;
        }
        if (key.getTextCharacter() == '8')
        {
            setQuantizeValue(QuantizeValue::Sixteenth);
            return true;
        }
        if (key.getTextCharacter() == '9')
        {
            setQuantizeValue(QuantizeValue::EighthTriplet);
            return true;
        }
    }

    // Arrow keys for transpose (when notes are selected)
    if (!selectedNoteIndices.empty())
    {
        // Up arrow = transpose up 1 semitone
        if (key == juce::KeyPress::upKey)
        {
            if (key.getModifiers().isShiftDown())
            {
                // Shift+Up = transpose up 1 octave
                transposeSelectedNotesOctaveUp();
            }
            else
            {
                transposeSelectedNotesUp();
            }
            return true;
        }

        // Down arrow = transpose down 1 semitone
        if (key == juce::KeyPress::downKey)
        {
            if (key.getModifiers().isShiftDown())
            {
                // Shift+Down = transpose down 1 octave
                transposeSelectedNotesOctaveDown();
            }
            else
            {
                transposeSelectedNotesDown();
            }
            return true;
        }

        // H = Humanize selected notes (moderate amount)
        if (key.getTextCharacter() == 'h' || key.getTextCharacter() == 'H')
        {
            humanizeSelectedNotes(0.3f, 0.3f);
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

    // Apply quantization to start and end positions if enabled
    double quantizedStartBeat = quantizeBeatPosition(dragStartBeat);
    double quantizedEndBeat = quantizeBeatPosition(endBeat);

    // Ensure minimum note length (at least 1 grid step)
    double gridSize = getQuantizeGridSize();
    if (quantizedEndBeat <= quantizedStartBeat)
    {
        quantizedEndBeat = quantizedStartBeat + gridSize;
    }

    // Create note from quantized start to end
    if (dragStartNote == endNote && quantizedEndBeat > quantizedStartBeat)
    {
        auto& seq = midiRegion->getMidiSequence();

        // Convert beats to ticks (assuming 960 ticks per beat)
        double ticksPerBeat = 960.0;
        double startTicks = quantizedStartBeat * ticksPerBeat;
        double endTicks = quantizedEndBeat * ticksPerBeat;

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

// ============================================================================
// Quantization functionality
// ============================================================================

double PianoRoll::getQuantizeGridSize() const
{
    switch (quantizeValue)
    {
        case QuantizeValue::Quarter:
            return 1.0;  // 1 beat
        case QuantizeValue::Eighth:
            return 0.5;  // 1/2 beat
        case QuantizeValue::Sixteenth:
            return 0.25; // 1/4 beat
        case QuantizeValue::QuarterTriplet:
            return 2.0 / 3.0;  // 2/3 beat (triplet feel on quarters)
        case QuantizeValue::EighthTriplet:
            return 1.0 / 3.0;  // 1/3 beat
        case QuantizeValue::SixteenthTriplet:
            return 1.0 / 6.0;  // 1/6 beat
        default:
            return 0.25; // Default to 1/16
    }
}

double PianoRoll::quantizeBeatPosition(double beat) const
{
    if (!quantizeEnabled)
        return beat;

    double gridSize = getQuantizeGridSize();
    if (gridSize <= 0.0)
        return beat;

    // Round to nearest grid position (full snap for interactive editing)
    return std::round(beat / gridSize) * gridSize;
}

double PianoRoll::quantizeBeatPositionWithStrength(double beat, float strength) const
{
    double gridSize = getQuantizeGridSize();
    if (gridSize <= 0.0 || strength <= 0.0f)
        return beat;

    // Calculate the fully quantized position
    double quantizedBeat = std::round(beat / gridSize) * gridSize;

    if (strength >= 1.0f)
        return quantizedBeat;

    // Apply partial quantization based on strength
    // newPosition = original + (quantized - original) * strength
    // This linearly interpolates between the original and quantized position
    return beat + (quantizedBeat - beat) * static_cast<double>(strength);
}

void PianoRoll::quantizeSelectedNotes()
{
    if (midiRegion == nullptr || selectedNoteIndices.empty())
        return;

    // Skip if strength is zero (no quantization)
    if (quantizeStrength <= 0.0f)
        return;

    auto& seq = midiRegion->getMidiSequence();
    double ticksPerBeat = 960.0;

    // Process each selected note
    for (int eventIndex : selectedNoteIndices)
    {
        if (eventIndex < 0 || eventIndex >= seq.getNumEvents())
            continue;

        auto* event = seq.getEventPointer(eventIndex);
        if (event == nullptr || !event->message.isNoteOn())
            continue;

        // Get current beat position
        double currentBeat = event->message.getTimeStamp() / ticksPerBeat;

        // Quantize using the strength parameter for partial quantization
        double quantizedBeat = quantizeBeatPositionWithStrength(currentBeat, quantizeStrength);

        // Update the note-on timestamp
        double newTicks = quantizedBeat * ticksPerBeat;
        event->message.setTimeStamp(newTicks);

        // If there's a note-off, adjust it to maintain note length
        if (event->noteOffObject != nullptr)
        {
            double noteOffBeat = event->noteOffObject->message.getTimeStamp() / ticksPerBeat;
            double noteLength = noteOffBeat - currentBeat;

            // Keep the same duration - note-off follows note-on
            double newNoteOffBeat = quantizedBeat + noteLength;
            event->noteOffObject->message.setTimeStamp(newNoteOffBeat * ticksPerBeat);
        }
    }

    // Re-sort and update matched pairs after modifying timestamps
    seq.sort();
    seq.updateMatchedPairs();

    repaint();
}

bool PianoRoll::isOffBeat(double beat) const
{
    double gridSize = getQuantizeGridSize();
    if (gridSize <= 0.0)
        return false;

    // Calculate position within the pair of grid units
    // For example, with 1/8 notes (0.5 beat grid):
    // Beat 0.0, 1.0, 2.0 are on-beats (first of each pair)
    // Beat 0.5, 1.5, 2.5 are off-beats (second of each pair)
    double pairSize = gridSize * 2.0;
    double positionInPair = std::fmod(beat, pairSize);

    // Normalize negative values
    if (positionInPair < 0.0)
        positionInPair += pairSize;

    // Check if we're in the second half of the pair (off-beat)
    // Allow small tolerance for floating point comparison
    double tolerance = gridSize * 0.1;
    return positionInPair >= (gridSize - tolerance) && positionInPair < (pairSize - tolerance);
}

double PianoRoll::applySwingToBeat(double beat) const
{
    if (!swingEnabled || swingAmount <= 0.0f)
        return beat;

    double gridSize = getQuantizeGridSize();
    if (gridSize <= 0.0)
        return beat;

    // Only apply swing to off-beats
    if (!isOffBeat(beat))
        return beat;

    // Calculate the swing offset
    // At swingAmount = 0.0: no swing (straight timing)
    // At swingAmount = 0.5: moderate groove (common for jazz/funk)
    // At swingAmount = 1.0: maximum swing (almost triplet feel)
    // The offset shifts the off-beat forward by up to half of the grid size
    double maxSwingOffset = gridSize * 0.5;
    double swingOffset = maxSwingOffset * static_cast<double>(swingAmount);

    return beat + swingOffset;
}

void PianoRoll::applySwingToSelectedNotes()
{
    if (midiRegion == nullptr || selectedNoteIndices.empty())
        return;

    // Skip if swing is disabled or amount is zero
    if (!swingEnabled || swingAmount <= 0.0f)
        return;

    auto& seq = midiRegion->getMidiSequence();
    double ticksPerBeat = 960.0;
    double gridSize = getQuantizeGridSize();

    // Process each selected note
    for (int eventIndex : selectedNoteIndices)
    {
        if (eventIndex < 0 || eventIndex >= seq.getNumEvents())
            continue;

        auto* event = seq.getEventPointer(eventIndex);
        if (event == nullptr || !event->message.isNoteOn())
            continue;

        // Get current beat position
        double currentBeat = event->message.getTimeStamp() / ticksPerBeat;

        // First, quantize to the grid (if close enough to an off-beat)
        double quantizedBeat = std::round(currentBeat / gridSize) * gridSize;

        // Only apply swing if the note is quantized to an off-beat position
        if (isOffBeat(quantizedBeat))
        {
            // Calculate the swing offset
            double maxSwingOffset = gridSize * 0.5;
            double swingOffset = maxSwingOffset * static_cast<double>(swingAmount);

            // Apply swing to the quantized position
            double swungBeat = quantizedBeat + swingOffset;

            // Calculate the delta between current and target
            double delta = swungBeat - currentBeat;

            // Apply the change proportional to quantize strength
            double newBeat = currentBeat + (delta * static_cast<double>(quantizeStrength));

            // Update the note-on timestamp
            double newTicks = newBeat * ticksPerBeat;
            event->message.setTimeStamp(newTicks);

            // If there's a note-off, adjust it to maintain note length
            if (event->noteOffObject != nullptr)
            {
                double noteOffBeat = event->noteOffObject->message.getTimeStamp() / ticksPerBeat;
                double noteLength = noteOffBeat - currentBeat;

                // Keep the same duration - note-off follows note-on
                double newNoteOffBeat = newBeat + noteLength;
                event->noteOffObject->message.setTimeStamp(newNoteOffBeat * ticksPerBeat);
            }
        }
    }

    // Re-sort and update matched pairs after modifying timestamps
    seq.sort();
    seq.updateMatchedPairs();

    repaint();
}

// ============================================================================
// MIDI Transform Operations
// ============================================================================

void PianoRoll::transposeSelectedNotes(int semitones)
{
    if (midiRegion == nullptr || selectedNoteIndices.empty() || semitones == 0)
        return;

    auto& seq = midiRegion->getMidiSequence();

    // Process each selected note
    for (int eventIndex : selectedNoteIndices)
    {
        if (eventIndex < 0 || eventIndex >= seq.getNumEvents())
            continue;

        auto* event = seq.getEventPointer(eventIndex);
        if (event == nullptr || !event->message.isNoteOn())
            continue;

        // Get current note number
        int currentNote = event->message.getNoteNumber();
        int newNote = currentNote + semitones;

        // Clamp to valid MIDI note range (0-127)
        newNote = juce::jlimit(0, 127, newNote);

        // Update the note number for note-on
        event->message.setNoteNumber(newNote);

        // Update the note number for matching note-off
        if (event->noteOffObject != nullptr)
        {
            event->noteOffObject->message.setNoteNumber(newNote);
        }
    }

    // Notes don't need re-sorting since timestamps didn't change
    repaint();
}

void PianoRoll::scaleVelocitySelectedNotes(float scaleFactor)
{
    if (midiRegion == nullptr || selectedNoteIndices.empty())
        return;

    // Clamp scale factor to reasonable range
    scaleFactor = juce::jlimit(0.0f, 2.0f, scaleFactor);

    auto& seq = midiRegion->getMidiSequence();

    // Process each selected note
    for (int eventIndex : selectedNoteIndices)
    {
        if (eventIndex < 0 || eventIndex >= seq.getNumEvents())
            continue;

        auto* event = seq.getEventPointer(eventIndex);
        if (event == nullptr || !event->message.isNoteOn())
            continue;

        // Get current velocity and scale it
        float currentVelocity = event->message.getFloatVelocity();
        float newVelocity = currentVelocity * scaleFactor;

        // Clamp to valid velocity range (minimum 0.01 to avoid silent notes)
        newVelocity = juce::jlimit(0.01f, 1.0f, newVelocity);

        // Update the velocity
        event->message.setVelocity(newVelocity);
    }

    repaint();
}

void PianoRoll::humanizeSelectedNotes(float timingAmount, float velocityAmount)
{
    if (midiRegion == nullptr || selectedNoteIndices.empty())
        return;

    // Clamp amounts to valid range
    timingAmount = juce::jlimit(0.0f, 1.0f, timingAmount);
    velocityAmount = juce::jlimit(0.0f, 1.0f, velocityAmount);

    // Skip if no humanization requested
    if (timingAmount <= 0.0f && velocityAmount <= 0.0f)
        return;

    auto& seq = midiRegion->getMidiSequence();
    double ticksPerBeat = 960.0;

    // Random number generator for humanization
    std::random_device rd;
    std::mt19937 gen(rd());

    // Timing variation: up to ~30ms at 120 BPM (which is about 0.06 beats)
    // At max timingAmount, vary by +/- 0.06 beats
    double maxTimingOffset = 0.06 * static_cast<double>(timingAmount);
    std::uniform_real_distribution<double> timingDist(-maxTimingOffset, maxTimingOffset);

    // Velocity variation: +/- 20% at max velocityAmount
    float maxVelocityOffset = 0.2f * velocityAmount;
    std::uniform_real_distribution<float> velocityDist(-maxVelocityOffset, maxVelocityOffset);

    // Process each selected note
    for (int eventIndex : selectedNoteIndices)
    {
        if (eventIndex < 0 || eventIndex >= seq.getNumEvents())
            continue;

        auto* event = seq.getEventPointer(eventIndex);
        if (event == nullptr || !event->message.isNoteOn())
            continue;

        // Apply timing variation if enabled
        if (timingAmount > 0.0f)
        {
            double currentBeat = event->message.getTimeStamp() / ticksPerBeat;
            double timingOffset = timingDist(gen);
            double newBeat = currentBeat + timingOffset;

            // Ensure we don't go negative
            newBeat = juce::jmax(0.0, newBeat);

            // Update note-on timestamp
            event->message.setTimeStamp(newBeat * ticksPerBeat);

            // Update note-off timestamp to maintain note length
            if (event->noteOffObject != nullptr)
            {
                double noteOffBeat = event->noteOffObject->message.getTimeStamp() / ticksPerBeat;
                double noteLength = noteOffBeat - currentBeat;
                double newNoteOffBeat = newBeat + noteLength;
                event->noteOffObject->message.setTimeStamp(newNoteOffBeat * ticksPerBeat);
            }
        }

        // Apply velocity variation if enabled
        if (velocityAmount > 0.0f)
        {
            float currentVelocity = event->message.getFloatVelocity();
            float velocityOffset = velocityDist(gen);
            float newVelocity = currentVelocity + velocityOffset;

            // Clamp to valid range (minimum 0.01 to avoid silent notes)
            newVelocity = juce::jlimit(0.01f, 1.0f, newVelocity);
            event->message.setVelocity(newVelocity);
        }
    }

    // Re-sort and update matched pairs after modifying timestamps
    seq.sort();
    seq.updateMatchedPairs();

    repaint();
}

void PianoRoll::drawQuantizeGrid(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (!quantizeEnabled)
        return;

    double gridSize = getQuantizeGridSize();
    if (gridSize <= 0.0 || gridSize >= 1.0)
        return;  // Don't draw sub-beat grid if it's beat-level or larger

    // Draw finer grid lines for quantize value
    double startBeat = horizontalScrollOffset / pixelsPerBeat;
    double endBeat = startBeat + bounds.getWidth() / pixelsPerBeat;

    // Determine grid color based on grid size - finer grids are more subtle
    float alpha = 0.15f;
    if (gridSize >= 0.5)
        alpha = 0.2f;
    else if (gridSize <= 0.2)
        alpha = 0.1f;

    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(alpha));

    // Snap start beat to grid
    double gridStart = std::floor(startBeat / gridSize) * gridSize;

    for (double beat = gridStart; beat <= endBeat; beat += gridSize)
    {
        // Skip beats that would overlap with main beat grid
        double beatFraction = std::fmod(beat, 1.0);
        if (std::abs(beatFraction) < 0.001 || std::abs(beatFraction - 1.0) < 0.001)
            continue;

        int x = beatToX(beat);
        if (x < bounds.getX() || x > bounds.getRight())
            continue;

        g.drawLine(static_cast<float>(x), static_cast<float>(bounds.getY()),
                   static_cast<float>(x), static_cast<float>(bounds.getBottom()), 0.5f);
    }
}

// ============================================================================
// CC Lane Management
// ============================================================================

void PianoRoll::setCCLaneVisible(CCLaneType laneType, bool visible)
{
    int index = static_cast<int>(laneType);
    if (index >= 0 && index < 4)
    {
        ccLaneVisible[index] = visible;
        repaint();
    }
}

bool PianoRoll::isCCLaneVisible(CCLaneType laneType) const
{
    int index = static_cast<int>(laneType);
    if (index >= 0 && index < 4)
    {
        return ccLaneVisible[index];
    }
    return false;
}

void PianoRoll::toggleCCLane(CCLaneType laneType)
{
    setCCLaneVisible(laneType, !isCCLaneVisible(laneType));
}

int PianoRoll::getNumVisibleCCLanes() const
{
    int count = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (ccLaneVisible[i])
            ++count;
    }
    return count;
}

int PianoRoll::getCCNumberForLaneType(CCLaneType laneType)
{
    switch (laneType)
    {
        case CCLaneType::ModWheel:   return 1;   // CC1 - Modulation
        case CCLaneType::Sustain:    return 64;  // CC64 - Sustain pedal
        case CCLaneType::PitchBend:  return -1;  // Special: pitch bend is not a CC
        case CCLaneType::Expression: return 11;  // CC11 - Expression
        default:                     return -1;
    }
}

juce::String PianoRoll::getCCLaneName(CCLaneType laneType)
{
    switch (laneType)
    {
        case CCLaneType::ModWheel:   return "Mod Wheel";
        case CCLaneType::Sustain:    return "Sustain";
        case CCLaneType::PitchBend:  return "Pitch Bend";
        case CCLaneType::Expression: return "Expression";
        default:                     return "Unknown";
    }
}

// ============================================================================
// CC Lane Drawing
// ============================================================================

void PianoRoll::drawCCLanes(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Draw separator line between notes and CC lanes
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawLine(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getY()),
               static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getY()), 2.0f);

    int laneIndex = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (ccLaneVisible[i])
        {
            auto laneBounds = getCCLaneBounds(laneIndex);
            CCLaneType laneType = static_cast<CCLaneType>(i);

            // Draw header area
            auto headerBounds = laneBounds.removeFromLeft(CC_LANE_HEADER_WIDTH);
            drawCCLaneHeader(g, headerBounds, laneType);

            // Draw the lane content area
            drawCCLane(g, laneBounds, laneType);

            ++laneIndex;
        }
    }
}

void PianoRoll::drawCCLaneHeader(juce::Graphics& g, juce::Rectangle<int> bounds, CCLaneType laneType)
{
    // Background
    g.setColour(MidiSingLookAndFeel::backgroundMid);
    g.fillRect(bounds);

    // Border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRect(bounds, 1);

    // Label
    g.setColour(MidiSingLookAndFeel::textColour);
    g.setFont(10.0f);
    g.drawText(getCCLaneName(laneType), bounds.reduced(4, 0),
               juce::Justification::centredLeft, true);

    // Draw value labels (0, 64, 127)
    g.setColour(MidiSingLookAndFeel::textDimColour);
    g.setFont(8.0f);
    g.drawText("127", bounds.getX() + 2, bounds.getY() + 12, 30, 10,
               juce::Justification::left, false);
    g.drawText("0", bounds.getX() + 2, bounds.getBottom() - 18, 30, 10,
               juce::Justification::left, false);
}

void PianoRoll::drawCCLane(juce::Graphics& g, juce::Rectangle<int> bounds, CCLaneType laneType)
{
    // Background
    g.setColour(MidiSingLookAndFeel::backgroundDark.brighter(0.05f));
    g.fillRect(bounds);

    // Draw grid
    drawCCLaneGrid(g, bounds);

    // Draw automation curve
    drawCCAutomation(g, bounds, laneType);

    // Border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRect(bounds, 1);
}

void PianoRoll::drawCCLaneGrid(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Draw horizontal center line (value 64)
    int centerY = bounds.getY() + bounds.getHeight() / 2;
    g.setColour(MidiSingLookAndFeel::borderColour.withAlpha(0.5f));
    g.drawHorizontalLine(centerY, static_cast<float>(bounds.getX()),
                         static_cast<float>(bounds.getRight()));

    // Draw vertical beat lines
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

void PianoRoll::drawCCAutomation(juce::Graphics& g, juce::Rectangle<int> bounds, CCLaneType laneType)
{
    if (midiRegion == nullptr)
        return;

    const auto& seq = midiRegion->getMidiSequence();
    double ticksPerBeat = 960.0;
    int ccNumber = getCCNumberForLaneType(laneType);

    // Collect CC points for this lane type
    struct CCPoint
    {
        double beat;
        int value;
    };
    std::vector<CCPoint> points;

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        auto* event = seq.getEventPointer(i);
        if (event == nullptr)
            continue;

        const auto& msg = event->message;

        if (laneType == CCLaneType::PitchBend && msg.isPitchWheel())
        {
            // Pitch bend: convert from 0-16383 to 0-127 range for display
            int pitchValue = msg.getPitchWheelValue();
            int displayValue = juce::jmap(pitchValue, 0, 16383, 0, 127);
            double beat = msg.getTimeStamp() / ticksPerBeat;
            points.push_back({ beat, displayValue });
        }
        else if (msg.isController() && msg.getControllerNumber() == ccNumber)
        {
            double beat = msg.getTimeStamp() / ticksPerBeat;
            int value = msg.getControllerValue();
            points.push_back({ beat, value });
        }
    }

    if (points.empty())
    {
        // Draw default line at 0 (or 64 for pitch bend center)
        int defaultValue = (laneType == CCLaneType::PitchBend) ? 64 : 0;
        int y = ccValueToY(defaultValue, bounds);
        g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.3f));
        g.drawHorizontalLine(y, static_cast<float>(bounds.getX()),
                             static_cast<float>(bounds.getRight()));
        return;
    }

    // Sort points by beat position
    std::sort(points.begin(), points.end(), [](const CCPoint& a, const CCPoint& b) {
        return a.beat < b.beat;
    });

    // Draw the automation curve as step lines with points
    juce::Path path;
    bool pathStarted = false;

    // Draw from start to first point
    if (!points.empty())
    {
        int startY = ccValueToY(points[0].value, bounds);
        path.startNewSubPath(static_cast<float>(bounds.getX()), static_cast<float>(startY));
        pathStarted = true;
    }

    for (size_t i = 0; i < points.size(); ++i)
    {
        int x = beatToX(points[i].beat);
        int y = ccValueToY(points[i].value, bounds);

        if (pathStarted)
        {
            // Step to new x position at current y level
            path.lineTo(static_cast<float>(x), path.getCurrentPosition().y);
            // Then step to new y value
            path.lineTo(static_cast<float>(x), static_cast<float>(y));
        }

        // Draw point marker
        g.setColour(MidiSingLookAndFeel::accentColour);
        g.fillEllipse(static_cast<float>(x - 3), static_cast<float>(y - 3), 6.0f, 6.0f);
    }

    // Extend to the end of visible area
    if (!points.empty())
    {
        path.lineTo(static_cast<float>(bounds.getRight()), path.getCurrentPosition().y);
    }

    // Draw the path
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.7f));
    g.strokePath(path, juce::PathStrokeType(1.5f));

    // Fill area under the curve
    juce::Path fillPath = path;
    fillPath.lineTo(static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom()));
    fillPath.lineTo(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getBottom()));
    fillPath.closeSubPath();

    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.15f));
    g.fillPath(fillPath);
}

// ============================================================================
// CC Lane Hit Testing
// ============================================================================

int PianoRoll::getCCLaneAtY(int y) const
{
    int numVisible = getNumVisibleCCLanes();
    if (numVisible == 0)
        return -1;

    int ccLanesStartY = getHeight() - (numVisible * CC_LANE_HEIGHT);

    if (y < ccLanesStartY)
        return -1;

    int relativeY = y - ccLanesStartY;
    int laneIndex = relativeY / CC_LANE_HEIGHT;

    if (laneIndex >= numVisible)
        return -1;

    return laneIndex;
}

PianoRoll::CCLaneType PianoRoll::getCCLaneTypeAtIndex(int index) const
{
    int currentIndex = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (ccLaneVisible[i])
        {
            if (currentIndex == index)
                return static_cast<CCLaneType>(i);
            ++currentIndex;
        }
    }
    return CCLaneType::ModWheel; // Default fallback
}

bool PianoRoll::isPointInCCLane(int x, int y) const
{
    int numVisible = getNumVisibleCCLanes();
    if (numVisible == 0)
        return false;

    int ccLanesStartY = getHeight() - (numVisible * CC_LANE_HEIGHT);
    return y >= ccLanesStartY && x > CC_LANE_HEADER_WIDTH;
}

int PianoRoll::ccValueToY(int value, juce::Rectangle<int> laneBounds) const
{
    // Map value (0-127) to Y coordinate (higher values at top)
    int padding = 4; // Small padding from edges
    int usableHeight = laneBounds.getHeight() - (padding * 2);
    int y = laneBounds.getBottom() - padding - (value * usableHeight / 127);
    return juce::jlimit(laneBounds.getY() + padding, laneBounds.getBottom() - padding, y);
}

int PianoRoll::yToCCValue(int y, juce::Rectangle<int> laneBounds) const
{
    // Map Y coordinate to value (0-127)
    int padding = 4;
    int usableHeight = laneBounds.getHeight() - (padding * 2);
    int relativeY = laneBounds.getBottom() - padding - y;
    int value = (relativeY * 127) / usableHeight;
    return juce::jlimit(0, 127, value);
}

juce::Rectangle<int> PianoRoll::getCCLaneBounds(int laneIndex) const
{
    int numVisible = getNumVisibleCCLanes();
    if (laneIndex < 0 || laneIndex >= numVisible)
        return juce::Rectangle<int>();

    int ccLanesStartY = getHeight() - (numVisible * CC_LANE_HEIGHT);
    int y = ccLanesStartY + (laneIndex * CC_LANE_HEIGHT);

    return juce::Rectangle<int>(0, y, getWidth(), CC_LANE_HEIGHT);
}

// ============================================================================
// CC Lane Mouse Handlers
// ============================================================================

void PianoRoll::handleCCLaneMouseDown(const juce::MouseEvent& e)
{
    int laneIndex = getCCLaneAtY(e.y);
    if (laneIndex < 0)
        return;

    ccEditLaneIndex = laneIndex;
    currentCCLaneType = getCCLaneTypeAtIndex(laneIndex);
    isDraggingInCCLane = true;

    // Calculate beat and value from position
    auto laneBounds = getCCLaneBounds(laneIndex);
    laneBounds.removeFromLeft(CC_LANE_HEADER_WIDTH);

    double beat = xToBeat(e.x);
    int value = yToCCValue(e.y, laneBounds);

    // Apply quantization if enabled
    beat = quantizeBeatPosition(beat);

    ccEditStartBeat = beat;
    ccEditStartValue = value;
    lastCCEditBeat = beat;

    // Add the initial CC point
    addCCPoint(currentCCLaneType, beat, value);
    repaint();
}

void PianoRoll::handleCCLaneMouseDrag(const juce::MouseEvent& e)
{
    if (ccEditLaneIndex < 0)
        return;

    auto laneBounds = getCCLaneBounds(ccEditLaneIndex);
    laneBounds.removeFromLeft(CC_LANE_HEADER_WIDTH);

    double beat = xToBeat(e.x);
    int value = yToCCValue(e.y, laneBounds);

    // Apply quantization if enabled
    beat = quantizeBeatPosition(beat);

    // Only add point if we've moved to a new grid position (when quantized)
    // or moved significantly (when not quantized)
    double beatDelta = std::abs(beat - lastCCEditBeat);
    double minDelta = quantizeEnabled ? getQuantizeGridSize() * 0.5 : 0.0625;

    if (beatDelta >= minDelta || beat != lastCCEditBeat)
    {
        addCCPoint(currentCCLaneType, beat, value);
        lastCCEditBeat = beat;
        repaint();
    }
}

void PianoRoll::handleCCLaneMouseUp(const juce::MouseEvent& /*e*/)
{
    isDraggingInCCLane = false;
    ccEditLaneIndex = -1;
    repaint();
}

// ============================================================================
// CC Lane Editing Operations
// ============================================================================

void PianoRoll::addCCPoint(CCLaneType laneType, double beat, int value)
{
    if (midiRegion == nullptr)
        return;

    auto& seq = midiRegion->getMidiSequence();
    double ticksPerBeat = 960.0;
    double timestamp = beat * ticksPerBeat;

    if (laneType == CCLaneType::PitchBend)
    {
        // Convert from 0-127 display range to 0-16383 pitch bend range
        int pitchValue = juce::jmap(value, 0, 127, 0, 16383);
        juce::MidiMessage pitchMsg = juce::MidiMessage::pitchWheel(1, pitchValue);
        pitchMsg.setTimeStamp(timestamp);
        seq.addEvent(pitchMsg);
    }
    else
    {
        int ccNumber = getCCNumberForLaneType(laneType);
        if (ccNumber >= 0)
        {
            juce::MidiMessage ccMsg = juce::MidiMessage::controllerEvent(1, ccNumber, value);
            ccMsg.setTimeStamp(timestamp);
            seq.addEvent(ccMsg);
        }
    }

    seq.sort();
}

void PianoRoll::deleteCCPointsInRange(CCLaneType laneType, double startBeat, double endBeat)
{
    if (midiRegion == nullptr)
        return;

    auto& seq = midiRegion->getMidiSequence();
    double ticksPerBeat = 960.0;
    int ccNumber = getCCNumberForLaneType(laneType);

    double startTicks = startBeat * ticksPerBeat;
    double endTicks = endBeat * ticksPerBeat;

    // Collect indices to delete (in reverse order to avoid index shifting)
    std::vector<int> indicesToDelete;

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        auto* event = seq.getEventPointer(i);
        if (event == nullptr)
            continue;

        const auto& msg = event->message;
        double timestamp = msg.getTimeStamp();

        if (timestamp < startTicks || timestamp > endTicks)
            continue;

        bool shouldDelete = false;

        if (laneType == CCLaneType::PitchBend && msg.isPitchWheel())
        {
            shouldDelete = true;
        }
        else if (msg.isController() && msg.getControllerNumber() == ccNumber)
        {
            shouldDelete = true;
        }

        if (shouldDelete)
        {
            indicesToDelete.push_back(i);
        }
    }

    // Delete in reverse order
    std::sort(indicesToDelete.begin(), indicesToDelete.end(), std::greater<int>());
    for (int index : indicesToDelete)
    {
        seq.deleteEvent(index, false);
    }

    repaint();
}

void PianoRoll::clearCCLane(CCLaneType laneType)
{
    if (midiRegion == nullptr)
        return;

    // Delete all CC events for this lane type (entire range)
    deleteCCPointsInRange(laneType, -1000.0, 10000.0);
}
