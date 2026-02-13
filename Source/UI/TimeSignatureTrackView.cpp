#include "TimeSignatureTrackView.h"
#include "../Actions/TimeSignatureActions.h"

TimeSignatureTrackView::TimeSignatureTrackView()
{
}

void TimeSignatureTrackView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.setColour(juce::Colour(0xff202830));
    g.fillRect(bounds);

    // Header area
    auto headerBounds = bounds.removeFromLeft(headerWidth);
    g.setColour(juce::Colour(0xff283038));
    g.fillRect(headerBounds);
    g.setColour(juce::Colour(0xff66aadd));
    g.setFont(11.0f);

    // Show current time sig in header
    juce::String sigText = "Time Sig";
    if (timeSigTrack != nullptr && transportPtr != nullptr)
    {
        int num, denom;
        timeSigTrack->getTimeSignatureAtPosition(transportPtr->getPlayheadPosition(), num, denom);
        sigText = juce::String(num) + "/" + juce::String(denom);
    }
    g.drawText(sigText, 4, 0, headerWidth - 8, getHeight(), juce::Justification::centredLeft);

    if (timeSigTrack == nullptr)
        return;

    auto contentBounds = getLocalBounds().withTrimmedLeft(headerWidth);
    g.reduceClipRegion(contentBounds);

    int h = getHeight();

    // Draw time signature events as flag/banners
    auto& events = timeSigTrack->getEvents();
    for (size_t i = 0; i < events.size(); ++i)
    {
        const auto& event = events[i];
        int x = sampleToPixel(event.position);

        if (x < headerWidth - 20 || x > getWidth() + 20)
            continue;

        bool isBeingDragged = isDraggingEvent && static_cast<int>(i) == dragEventIndex;

        // Draw flag triangle
        juce::Path flag;
        flag.addTriangle(static_cast<float>(x), 2.0f,
                         static_cast<float>(x + 10), 2.0f,
                         static_cast<float>(x), static_cast<float>(h / 2 + 2));

        g.setColour(isBeingDragged ? juce::Colour(0xff88ccff) : juce::Colour(0xff66aadd));
        g.fillPath(flag);

        // Draw vertical line
        g.setColour(juce::Colour(0x4066aadd));
        g.drawVerticalLine(x, 0.0f, static_cast<float>(h));

        // Draw time signature text
        juce::String text = juce::String(event.numerator) + "/" + juce::String(event.denominator);
        g.setColour(isBeingDragged ? juce::Colour(0xff88ccff) : juce::Colour(0xff66aadd));
        g.setFont(10.0f);

        // Draw banner background
        int textWidth = static_cast<int>(g.getCurrentFont().getStringWidthFloat(text)) + 8;
        g.setColour(juce::Colour(0xff283038));
        g.fillRoundedRectangle(static_cast<float>(x + 12), 2.0f,
                                static_cast<float>(textWidth), static_cast<float>(h - 4), 2.0f);
        g.setColour(isBeingDragged ? juce::Colour(0xff88ccff) : juce::Colour(0xff66aadd));
        g.drawRoundedRectangle(static_cast<float>(x + 12), 2.0f,
                                static_cast<float>(textWidth), static_cast<float>(h - 4), 2.0f, 1.0f);
        g.drawText(text, x + 16, 2, textWidth - 8, h - 4, juce::Justification::centredLeft, true);
    }

    // Bottom border
    g.setColour(juce::Colour(0xff555555));
    g.drawHorizontalLine(h - 1, 0.0f, static_cast<float>(getWidth()));
}

void TimeSignatureTrackView::mouseDown(const juce::MouseEvent& e)
{
    if (timeSigTrack == nullptr)
        return;

    int clickedEvent = findEventAtPixel(e.x);

    if (e.mods.isRightButtonDown() && clickedEvent >= 0)
    {
        showContextMenu(clickedEvent, e.getScreenPosition());
        return;
    }

    if (clickedEvent >= 0)
    {
        isDraggingEvent = true;
        dragEventIndex = clickedEvent;
        dragOriginalPosition = timeSigTrack->getEvent(static_cast<size_t>(clickedEvent)).position;
    }
    else if (e.mods.isLeftButtonDown())
    {
        if (onJumpToPosition)
            onJumpToPosition(pixelToSample(e.x));
    }
}

void TimeSignatureTrackView::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingEvent && timeSigTrack != nullptr && dragEventIndex >= 0)
    {
        int64_t newPos = juce::jmax(int64_t(0), pixelToSample(e.x));
        auto& event = timeSigTrack->getEvent(static_cast<size_t>(dragEventIndex));
        timeSigTrack->moveEvent(static_cast<size_t>(dragEventIndex), newPos, event.numerator, event.denominator);
        dragEventIndex = timeSigTrack->findEventAtPosition(newPos, 1);
        repaint();
    }
}

void TimeSignatureTrackView::mouseUp(const juce::MouseEvent& e)
{
    if (isDraggingEvent && timeSigTrack != nullptr && dragEventIndex >= 0 && undoManager != nullptr)
    {
        int64_t finalPos = timeSigTrack->getEvent(static_cast<size_t>(dragEventIndex)).position;

        // Undo: restore original position, then redo applies the new position
        auto& event = timeSigTrack->getEvent(static_cast<size_t>(dragEventIndex));
        timeSigTrack->moveEvent(static_cast<size_t>(dragEventIndex), dragOriginalPosition, event.numerator, event.denominator);
        int idx = timeSigTrack->findEventAtPosition(dragOriginalPosition, 0);
        if (idx >= 0)
            undoManager->perform(new MoveTimeSignatureEventAction(*timeSigTrack, static_cast<size_t>(idx), finalPos));
    }

    juce::ignoreUnused(e);
    isDraggingEvent = false;
    dragEventIndex = -1;
}

void TimeSignatureTrackView::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (timeSigTrack == nullptr)
        return;

    int clickedEvent = findEventAtPixel(e.x);

    if (clickedEvent >= 0)
    {
        showEditDialog(clickedEvent);
    }
    else
    {
        // Double-click on empty area = add event
        int64_t pos = pixelToSample(e.x);
        showAddDialog(pos);
    }
}

int TimeSignatureTrackView::sampleToPixel(int64_t samples) const
{
    if (sampleRate <= 0.0)
        return headerWidth;

    double bpm = 120.0;
    if (transportPtr != nullptr)
        bpm = transportPtr->getBPM();
    if (bpm <= 0.0) bpm = 120.0;

    double samplesPerBeat = (60.0 / bpm) * sampleRate;
    double beats = static_cast<double>(samples) / samplesPerBeat;
    return headerWidth + static_cast<int>((beats * pixelsPerBeat) - horizontalScrollOffset);
}

int64_t TimeSignatureTrackView::pixelToSample(int x) const
{
    if (sampleRate <= 0.0 || pixelsPerBeat <= 0.0)
        return 0;

    double bpm = 120.0;
    if (transportPtr != nullptr)
        bpm = transportPtr->getBPM();
    if (bpm <= 0.0) bpm = 120.0;

    double beats = (static_cast<double>(x - headerWidth) + horizontalScrollOffset) / pixelsPerBeat;
    double samplesPerBeat = (60.0 / bpm) * sampleRate;
    return static_cast<int64_t>(beats * samplesPerBeat);
}

int TimeSignatureTrackView::findEventAtPixel(int x) const
{
    if (timeSigTrack == nullptr)
        return -1;

    for (size_t i = 0; i < timeSigTrack->getNumEvents(); ++i)
    {
        int px = sampleToPixel(timeSigTrack->getEvent(i).position);
        if (std::abs(px - x) <= 10)
            return static_cast<int>(i);
    }
    return -1;
}

void TimeSignatureTrackView::showContextMenu(int eventIndex, juce::Point<int> position)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Edit Time Signature...");
    menu.addItem(2, "Delete");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ position.x, position.y, 1, 1 }),
        [this, eventIndex](int result)
        {
            if (timeSigTrack == nullptr)
                return;

            if (result == 1)
            {
                showEditDialog(eventIndex);
            }
            else if (result == 2)
            {
                if (undoManager != nullptr)
                    undoManager->perform(new RemoveTimeSignatureEventAction(*timeSigTrack, static_cast<size_t>(eventIndex)));
                else
                    timeSigTrack->removeEvent(static_cast<size_t>(eventIndex));
                repaint();
            }
        });
}

void TimeSignatureTrackView::showEditDialog(int eventIndex)
{
    if (timeSigTrack == nullptr || eventIndex < 0 || static_cast<size_t>(eventIndex) >= timeSigTrack->getNumEvents())
        return;

    auto& event = timeSigTrack->getEvent(static_cast<size_t>(eventIndex));

    auto* dialog = new juce::AlertWindow("Edit Time Signature", "Set time signature:", juce::MessageBoxIconType::NoIcon);
    dialog->addComboBox("numerator", { "1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","24","32" }, "Numerator:");
    dialog->addComboBox("denominator", { "1","2","4","8","16","32","64" }, "Denominator:");

    // Set current values
    auto* numCombo = dialog->getComboBoxComponent("numerator");
    auto* denomCombo = dialog->getComboBoxComponent("denominator");
    numCombo->setText(juce::String(event.numerator), juce::dontSendNotification);
    denomCombo->setText(juce::String(event.denominator), juce::dontSendNotification);

    dialog->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, dialog, eventIndex](int result)
        {
            if (result == 1 && timeSigTrack != nullptr)
            {
                int newNum = dialog->getComboBoxComponent("numerator")->getText().getIntValue();
                int newDenom = dialog->getComboBoxComponent("denominator")->getText().getIntValue();
                newNum = juce::jlimit(1, 32, newNum);
                newDenom = juce::jlimit(1, 64, newDenom);

                if (undoManager != nullptr)
                    undoManager->perform(new SetTimeSignatureAction(*timeSigTrack, static_cast<size_t>(eventIndex), newNum, newDenom));
                else
                    timeSigTrack->setEventTimeSignature(static_cast<size_t>(eventIndex), newNum, newDenom);

                repaint();
            }
            delete dialog;
        }), true);
}

void TimeSignatureTrackView::showAddDialog(int64_t position)
{
    auto* dialog = new juce::AlertWindow("Add Time Signature", "Set time signature:", juce::MessageBoxIconType::NoIcon);
    dialog->addComboBox("numerator", { "1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","24","32" }, "Numerator:");
    dialog->addComboBox("denominator", { "1","2","4","8","16","32","64" }, "Denominator:");

    // Default to 4/4
    dialog->getComboBoxComponent("numerator")->setText("4", juce::dontSendNotification);
    dialog->getComboBoxComponent("denominator")->setText("4", juce::dontSendNotification);

    dialog->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, dialog, position](int result)
        {
            if (result == 1 && timeSigTrack != nullptr)
            {
                int newNum = dialog->getComboBoxComponent("numerator")->getText().getIntValue();
                int newDenom = dialog->getComboBoxComponent("denominator")->getText().getIntValue();
                newNum = juce::jlimit(1, 32, newNum);
                newDenom = juce::jlimit(1, 64, newDenom);

                TimeSignatureEvent event(position, newNum, newDenom);
                if (undoManager != nullptr)
                    undoManager->perform(new AddTimeSignatureEventAction(*timeSigTrack, event));
                else
                    timeSigTrack->addEvent(event);

                repaint();
            }
            delete dialog;
        }), true);
}
