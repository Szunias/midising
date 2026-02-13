#include "MarkerTrackView.h"

MarkerTrackView::MarkerTrackView()
{
}

void MarkerTrackView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(bounds);

    // Header area
    g.setColour(juce::Colour(0xff333333));
    g.fillRect(bounds.removeFromLeft(headerWidth));
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(11.0f);
    g.drawText("Markers", 4, 0, headerWidth - 8, getHeight(), juce::Justification::centredLeft);

    if (markerTrack == nullptr)
        return;

    // Draw marker flags
    auto contentBounds = getLocalBounds().withTrimmedLeft(headerWidth);
    g.reduceClipRegion(contentBounds);

    for (int i = 0; i < markerTrack->getNumMarkers(); ++i)
    {
        const auto& marker = markerTrack->getMarker(i);
        int x = sampleToPixel(marker.position);

        if (x < headerWidth - 10 || x > getWidth() + 10)
            continue;

        // Draw flag triangle
        juce::Path flag;
        flag.addTriangle(static_cast<float>(x), 0.0f,
                         static_cast<float>(x + 8), 0.0f,
                         static_cast<float>(x), static_cast<float>(getHeight() / 2));

        g.setColour(marker.colour);
        g.fillPath(flag);

        // Draw vertical line
        g.setColour(marker.colour.withAlpha(0.6f));
        g.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));

        // Draw name
        g.setColour(marker.colour);
        g.setFont(10.0f);
        g.drawText(marker.name, x + 10, 2, 100, getHeight() - 4, juce::Justification::centredLeft, true);
    }

    // Bottom border
    g.setColour(juce::Colour(0xff555555));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
}

void MarkerTrackView::mouseDown(const juce::MouseEvent& e)
{
    if (markerTrack == nullptr)
        return;

    int64_t clickSample = pixelToSample(e.x);

    // Check if clicking on an existing marker
    int clickedMarker = markerTrack->findMarkerAtPosition(clickSample,
        static_cast<int64_t>(sampleRate * 0.1)); // 100ms tolerance

    if (e.mods.isRightButtonDown() && clickedMarker >= 0)
    {
        showContextMenu(clickedMarker, e.getScreenPosition());
        return;
    }

    if (clickedMarker >= 0)
    {
        isDraggingMarker = true;
        dragMarkerIndex = clickedMarker;
        dragOriginalPosition = markerTrack->getMarker(clickedMarker).position;
    }
    else if (e.mods.isLeftButtonDown())
    {
        // Click on empty space = jump playhead
        if (onJumpToPosition)
            onJumpToPosition(clickSample);
    }
}

void MarkerTrackView::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingMarker && markerTrack != nullptr && dragMarkerIndex >= 0)
    {
        int64_t newPos = juce::jmax(int64_t(0), pixelToSample(e.x));
        markerTrack->moveMarker(dragMarkerIndex, newPos);
        // After sort, find the marker again by original position proximity
        dragMarkerIndex = markerTrack->findMarkerAtPosition(newPos, 1);
        repaint();
    }
}

void MarkerTrackView::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isDraggingMarker = false;
    dragMarkerIndex = -1;
}

void MarkerTrackView::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (markerTrack == nullptr || transportPtr == nullptr)
        return;

    int64_t clickSample = pixelToSample(e.x);
    int clickedMarker = markerTrack->findMarkerAtPosition(clickSample,
        static_cast<int64_t>(sampleRate * 0.1));

    if (clickedMarker >= 0)
    {
        // Double-click on marker = set loop between this and next
        int64_t loopStart, loopEnd;
        if (markerTrack->getLoopRangeFromMarker(clickedMarker, loopStart, loopEnd))
        {
            transportPtr->setLoopRange(loopStart, loopEnd);
            transportPtr->setLooping(true);
        }
    }
}

int MarkerTrackView::sampleToPixel(int64_t samples) const
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

int64_t MarkerTrackView::pixelToSample(int x) const
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

void MarkerTrackView::showContextMenu(int markerIndex, juce::Point<int> position)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Rename...");
    menu.addItem(2, "Delete");
    menu.addSeparator();

    juce::PopupMenu colourMenu;
    colourMenu.addItem(10, "Orange");
    colourMenu.addItem(11, "Red");
    colourMenu.addItem(12, "Green");
    colourMenu.addItem(13, "Blue");
    colourMenu.addItem(14, "Yellow");
    colourMenu.addItem(15, "Purple");
    menu.addSubMenu("Colour", colourMenu);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ position.x, position.y, 1, 1 }),
        [this, markerIndex](int result)
        {
            if (markerTrack == nullptr)
                return;

            if (result == 1)
                showRenameDialog(markerIndex);
            else if (result == 2)
                markerTrack->removeMarker(markerIndex);
            else if (result >= 10 && result <= 15)
            {
                juce::Colour colours[] = {
                    juce::Colours::orange, juce::Colours::red, juce::Colours::green,
                    juce::Colours::dodgerblue, juce::Colours::yellow, juce::Colours::purple
                };
                markerTrack->setMarkerColour(markerIndex, colours[result - 10]);
            }
            repaint();
        });
}

void MarkerTrackView::showRenameDialog(int markerIndex)
{
    if (markerTrack == nullptr || markerIndex < 0 || markerIndex >= markerTrack->getNumMarkers())
        return;

    auto currentName = markerTrack->getMarker(markerIndex).name;

    auto* dialog = new juce::AlertWindow("Rename Marker", "Enter new name:", juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor("name", currentName, "Name:");
    dialog->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, dialog, markerIndex](int result)
        {
            if (result == 1 && markerTrack != nullptr)
            {
                auto newName = dialog->getTextEditorContents("name");
                if (newName.isNotEmpty())
                {
                    markerTrack->renameMarker(markerIndex, newName);
                    repaint();
                }
            }
            delete dialog;
        }), true);
}
