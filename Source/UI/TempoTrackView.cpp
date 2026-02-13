#include "TempoTrackView.h"
#include "../Actions/TempoActions.h"

TempoTrackView::TempoTrackView()
{
}

void TempoTrackView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.setColour(juce::Colour(0xff2d2420));
    g.fillRect(bounds);

    // Header area
    auto headerBounds = bounds.removeFromLeft(headerWidth);
    g.setColour(juce::Colour(0xff3a3028));
    g.fillRect(headerBounds);
    g.setColour(juce::Colour(0xffddaa66));
    g.setFont(11.0f);

    // Show current BPM in header
    juce::String bpmText = "Tempo";
    if (tempoTrack != nullptr && transportPtr != nullptr)
    {
        double currentBpm = tempoTrack->getBpmAtPosition(transportPtr->getPlayheadPosition());
        bpmText = juce::String(currentBpm, 1) + " BPM";
    }
    g.drawText(bpmText, 4, 0, headerWidth - 8, getHeight(), juce::Justification::centredLeft);

    if (tempoTrack == nullptr)
        return;

    auto contentBounds = getLocalBounds().withTrimmedLeft(headerWidth);
    g.reduceClipRegion(contentBounds);

    int h = getHeight();

    // Draw interpolation curves between events
    auto& events = tempoTrack->getEvents();
    for (size_t i = 0; i < events.size(); ++i)
    {
        int x1 = sampleToPixel(events[i].position);
        double bpm1 = events[i].bpm;

        int x2;
        double bpm2;
        if (i + 1 < events.size())
        {
            x2 = sampleToPixel(events[i + 1].position);
            bpm2 = events[i + 1].bpm;
        }
        else
        {
            x2 = getWidth();
            bpm2 = bpm1;
        }

        if (x2 < headerWidth || x1 > getWidth())
            continue;

        g.setColour(juce::Colour(0x40ddaa66));

        if (events[i].rampType == TempoRampType::Instant)
        {
            // Step: horizontal line then vertical
            int y1 = static_cast<int>(bpmToY(bpm1));
            int y2 = static_cast<int>(bpmToY(bpm2));
            g.drawHorizontalLine(y1, static_cast<float>(x1), static_cast<float>(x2));
            if (i + 1 < events.size())
                g.drawVerticalLine(x2, static_cast<float>(juce::jmin(y1, y2)), static_cast<float>(juce::jmax(y1, y2)));
        }
        else if (events[i].rampType == TempoRampType::Linear)
        {
            // Linear line
            int y1 = static_cast<int>(bpmToY(bpm1));
            int y2 = static_cast<int>(bpmToY(bpm2));
            g.drawLine(static_cast<float>(x1), static_cast<float>(y1),
                       static_cast<float>(x2), static_cast<float>(y2), 1.5f);
        }
        else if (events[i].rampType == TempoRampType::SCurve)
        {
            // S-Curve: draw smooth path
            juce::Path path;
            int y1 = static_cast<int>(bpmToY(bpm1));
            path.startNewSubPath(static_cast<float>(x1), static_cast<float>(y1));
            int steps = juce::jmax(10, (x2 - x1) / 4);
            for (int s = 1; s <= steps; ++s)
            {
                double t = static_cast<double>(s) / static_cast<double>(steps);
                double interpolatedBpm = TempoRampUtils::interpolate(bpm1, bpm2, t, TempoRampType::SCurve);
                int px = x1 + static_cast<int>(t * (x2 - x1));
                int py = static_cast<int>(bpmToY(interpolatedBpm));
                path.lineTo(static_cast<float>(px), static_cast<float>(py));
            }
            g.strokePath(path, juce::PathStrokeType(1.5f));
        }
    }

    // Draw tempo event nodes
    for (size_t i = 0; i < events.size(); ++i)
    {
        const auto& event = events[i];
        int x = sampleToPixel(event.position);
        int y = static_cast<int>(bpmToY(event.bpm));

        if (x < headerWidth - 10 || x > getWidth() + 10)
            continue;

        // Draw node circle
        bool isBeingDragged = isDraggingEvent && static_cast<int>(i) == dragEventIndex;
        g.setColour(isBeingDragged ? juce::Colour(0xffffd080) : juce::Colour(0xffddaa66));
        g.fillEllipse(static_cast<float>(x - 4), static_cast<float>(y - 4), 8.0f, 8.0f);
        g.setColour(juce::Colour(0xff000000));
        g.drawEllipse(static_cast<float>(x - 4), static_cast<float>(y - 4), 8.0f, 8.0f, 1.0f);

        // Draw BPM label
        g.setColour(juce::Colour(0xffddaa66));
        g.setFont(9.0f);
        g.drawText(juce::String(event.bpm, 1), x + 6, y - 6, 50, 12, juce::Justification::centredLeft, true);

        // Draw vertical line at event
        g.setColour(juce::Colour(0x30ddaa66));
        g.drawVerticalLine(x, 0.0f, static_cast<float>(h));
    }

    // Bottom border
    g.setColour(juce::Colour(0xff555555));
    g.drawHorizontalLine(h - 1, 0.0f, static_cast<float>(getWidth()));
}

void TempoTrackView::mouseDown(const juce::MouseEvent& e)
{
    if (tempoTrack == nullptr)
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
        dragOriginalPosition = tempoTrack->getEvent(static_cast<size_t>(clickedEvent)).position;
        dragOriginalBpm = tempoTrack->getEvent(static_cast<size_t>(clickedEvent)).bpm;
    }
    else if (e.mods.isLeftButtonDown())
    {
        if (onJumpToPosition)
            onJumpToPosition(pixelToSample(e.x));
    }
}

void TempoTrackView::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingEvent && tempoTrack != nullptr && dragEventIndex >= 0)
    {
        int64_t newPos = juce::jmax(int64_t(0), pixelToSample(e.x));
        double newBpm = yToBpm(e.y);
        tempoTrack->moveEvent(static_cast<size_t>(dragEventIndex), newPos, newBpm);
        dragEventIndex = tempoTrack->findEventAtPosition(newPos, 1);
        repaint();
    }
}

void TempoTrackView::mouseUp(const juce::MouseEvent& e)
{
    if (isDraggingEvent && tempoTrack != nullptr && dragEventIndex >= 0 && undoManager != nullptr)
    {
        int64_t finalPos = tempoTrack->getEvent(static_cast<size_t>(dragEventIndex)).position;
        double finalBpm = tempoTrack->getEvent(static_cast<size_t>(dragEventIndex)).bpm;

        // Undo: restore original, then redo applies the final state
        tempoTrack->moveEvent(static_cast<size_t>(dragEventIndex), dragOriginalPosition, dragOriginalBpm);
        int idx = tempoTrack->findEventAtPosition(dragOriginalPosition, 0);
        if (idx >= 0)
            undoManager->perform(new MoveTempoEventAction(*tempoTrack, static_cast<size_t>(idx), finalPos, finalBpm));
    }

    isDraggingEvent = false;
    dragEventIndex = -1;
}

void TempoTrackView::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (tempoTrack == nullptr)
        return;

    int clickedEvent = findEventAtPixel(e.x);

    if (clickedEvent >= 0)
    {
        showEditBpmDialog(clickedEvent);
    }
    else
    {
        // Double-click on empty area = add event at default BPM
        int64_t pos = pixelToSample(e.x);
        double bpm = yToBpm(e.y);

        if (undoManager != nullptr)
            undoManager->perform(new AddTempoEventAction(*tempoTrack, TempoEvent(pos, bpm)));
        else
            tempoTrack->addEvent(pos, bpm);

        repaint();
    }
}

int TempoTrackView::sampleToPixel(int64_t samples) const
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

int64_t TempoTrackView::pixelToSample(int x) const
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

double TempoTrackView::bpmToY(double bpm) const
{
    // Map BPM range (20-300) to vertical pixel range
    double minBpm = 20.0;
    double maxBpm = 300.0;
    double normalized = (bpm - minBpm) / (maxBpm - minBpm);
    // Invert so higher BPM is at top
    return (1.0 - normalized) * (getHeight() - 4) + 2;
}

double TempoTrackView::yToBpm(int y) const
{
    double minBpm = 20.0;
    double maxBpm = 300.0;
    double normalized = 1.0 - (static_cast<double>(y - 2) / (getHeight() - 4));
    return juce::jlimit(minBpm, maxBpm, minBpm + normalized * (maxBpm - minBpm));
}

int TempoTrackView::findEventAtPixel(int x) const
{
    if (tempoTrack == nullptr)
        return -1;

    for (size_t i = 0; i < tempoTrack->getNumEvents(); ++i)
    {
        int px = sampleToPixel(tempoTrack->getEvent(i).position);
        if (std::abs(px - x) <= 8)
            return static_cast<int>(i);
    }
    return -1;
}

void TempoTrackView::showContextMenu(int eventIndex, juce::Point<int> position)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Edit BPM...");
    menu.addItem(2, "Delete");
    menu.addSeparator();

    juce::PopupMenu rampMenu;
    rampMenu.addItem(10, "Instant (Step)");
    rampMenu.addItem(11, "Linear");
    rampMenu.addItem(12, "S-Curve");
    menu.addSubMenu("Ramp Type", rampMenu);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ position.x, position.y, 1, 1 }),
        [this, eventIndex](int result)
        {
            if (tempoTrack == nullptr)
                return;

            if (result == 1)
            {
                showEditBpmDialog(eventIndex);
            }
            else if (result == 2)
            {
                if (undoManager != nullptr)
                    undoManager->perform(new RemoveTempoEventAction(*tempoTrack, static_cast<size_t>(eventIndex)));
                else
                    tempoTrack->removeEvent(static_cast<size_t>(eventIndex));
                repaint();
            }
            else if (result >= 10 && result <= 12)
            {
                TempoRampType ramps[] = { TempoRampType::Instant, TempoRampType::Linear, TempoRampType::SCurve };
                if (undoManager != nullptr)
                    undoManager->perform(new SetTempoRampTypeAction(*tempoTrack, static_cast<size_t>(eventIndex), ramps[result - 10]));
                else
                    tempoTrack->setEventRampType(static_cast<size_t>(eventIndex), ramps[result - 10]);
                repaint();
            }
        });
}

void TempoTrackView::showEditBpmDialog(int eventIndex)
{
    if (tempoTrack == nullptr || eventIndex < 0 || static_cast<size_t>(eventIndex) >= tempoTrack->getNumEvents())
        return;

    double currentBpm = tempoTrack->getEvent(static_cast<size_t>(eventIndex)).bpm;

    auto* dialog = new juce::AlertWindow("Edit Tempo", "Enter BPM (20-300):", juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor("bpm", juce::String(currentBpm, 1), "BPM:");
    dialog->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, dialog, eventIndex](int result)
        {
            if (result == 1 && tempoTrack != nullptr)
            {
                double newBpm = dialog->getTextEditorContents("bpm").getDoubleValue();
                newBpm = juce::jlimit(20.0, 300.0, newBpm);
                auto& event = tempoTrack->getEvent(static_cast<size_t>(eventIndex));

                if (undoManager != nullptr)
                    undoManager->perform(new MoveTempoEventAction(*tempoTrack, static_cast<size_t>(eventIndex), event.position, newBpm));
                else
                    tempoTrack->moveEvent(static_cast<size_t>(eventIndex), event.position, newBpm);

                repaint();
            }
            delete dialog;
        }), true);
}
