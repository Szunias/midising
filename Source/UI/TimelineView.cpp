#include "TimelineView.h"
#include "Cursors.h"
#include "../Utils/TimeConversion.h"
#include "../Utils/UndoManager.h"
#include "../Actions/RegionActions.h"
#include <algorithm>
#include <cmath>

TimelineView::TimelineView()
{
    startTimer(30); // ~30 FPS for playhead updates
    setWantsKeyboardFocus(true); // Enable keyboard focus for shortcuts
}

TimelineView::~TimelineView()
{
    stopTimer();
}

void TimelineView::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark);

    if (timelinePtr == nullptr)
        return;

    auto bounds = getLocalBounds();

    // Draw time ruler at top
    auto rulerBounds = bounds.removeFromTop(RULER_HEIGHT);
    rulerBounds.removeFromLeft(HEADER_WIDTH);
    drawTimeRuler(g, rulerBounds);

    // Draw track lanes with automation lanes
    for (int i = 0; i < timelinePtr->getNumTracks(); ++i)
    {
        // Main track lane
        auto laneBounds = bounds.removeFromTop(trackHeight);
        auto headerBounds = laneBounds.removeFromLeft(HEADER_WIDTH);
        juce::ignoreUnused(headerBounds); // Headers are separate components

        drawTrackLane(g, laneBounds, i);

        // Reserve space for automation lanes (actual drawing is handled by AutomationLaneView components)
        int numAutoLanes = getNumVisibleAutomationLanes(i);
        if (numAutoLanes > 0)
        {
            // Just remove the space from bounds - AutomationLaneView components handle the drawing
            auto automationBounds = bounds.removeFromTop(numAutoLanes * AUTOMATION_LANE_HEIGHT);
            juce::ignoreUnused(automationBounds);
            // Note: drawAutomationLanes() is now handled by AutomationLaneView child components
        }
    }

    // Draw drag ghost for visual feedback during region dragging
    drawDragGhost(g);

    // Draw resize ghost for visual feedback during edge resizing
    drawResizeGhost(g);

    // Draw fade ghost for visual feedback during fade editing
    drawFadeGhost(g);

    // Draw draw ghost for visual feedback during Draw tool region creation
    drawDrawGhost(g);

    // Draw stretch ghost for visual feedback during Alt+drag edge stretching
    drawStretchGhost(g);

    // Draw playhead on top of everything
    drawPlayhead(g);
}

void TimelineView::resized()
{
    updateTrackHeaders();
    updateAutomationLaneViews();
}

void TimelineView::timerCallback()
{
    // Repaint for playhead animation
    if (transportPtr != nullptr && !transportPtr->isStopped())
    {
        // Record automation if in Write/Touch/Latch mode during playback
        if (isRecordingAutomation &&
            (automationMode == AutomationMode::Write ||
             automationMode == AutomationMode::Touch ||
             automationMode == AutomationMode::Latch))
        {
            recordAutomationAtPlayhead();
        }

        repaint();
    }
}

void TimelineView::mouseDown(const juce::MouseEvent& e)
{
    if (timelinePtr == nullptr || transportPtr == nullptr)
        return;

    // Grab keyboard focus for shortcuts
    grabKeyboardFocus();

    // Store click position for paste operations
    lastClickSamplePosition = pixelToSample(e.x);

    // Handle double-click for regions and empty areas
    if (e.getNumberOfClicks() == 2 && e.x > HEADER_WIDTH && e.y > RULER_HEIGHT)
    {
        int trackIndex = -1;
        Region* clickedRegion = getRegionAtPosition(e.x, e.y, trackIndex);

        if (clickedRegion != nullptr)
        {
            // Double-click on a region - open appropriate editor
            handleDoubleClickOnRegion(clickedRegion, trackIndex);
            return;
        }
        else
        {
            // Double-click on empty track area - create 4-bar region
            int trackIdx = getTrackIndexAtY(e.y);
            if (trackIdx >= 0 && trackIdx < timelinePtr->getNumTracks())
            {
                handleDoubleClickOnEmptyArea(trackIdx, e.x);
                return;
            }
        }
    }

    // Handle right-click for context menu
    if (e.mods.isPopupMenu())
    {
        int trackIndex = -1;
        Region* region = getRegionAtPosition(e.x, e.y, trackIndex);

        if (region != nullptr)
        {
            // Select the clicked region first
            selectedRegion = region;
            selectedTrackIndex = trackIndex;
            repaint();
        }

        // Show context menu (works on regions or empty timeline area)
        showContextMenu(e);
        return;
    }

    // Handle Draw tool - start region creation on empty track area
    if (currentToolMode == ToolMode::Draw)
    {
        // Only start drawing in the track area (not ruler, not header)
        if (e.x > HEADER_WIDTH && e.y > RULER_HEIGHT)
        {
            int trackIdx = getTrackIndexAtY(e.y);
            if (trackIdx >= 0 && trackIdx < timelinePtr->getNumTracks())
            {
                // Check if clicking on an existing region - don't start draw if so
                int regionTrackIndex = -1;
                Region* existingRegion = getRegionAtPosition(e.x, e.y, regionTrackIndex);

                if (existingRegion == nullptr)
                {
                    // Start drawing a new region
                    isDrawingRegion = true;
                    drawStartX = e.x;
                    drawTrackIndex = trackIdx;

                    // Initialize ghost preview bounds at click position
                    int trackY = RULER_HEIGHT;
                    for (int i = 0; i < trackIdx; ++i)
                    {
                        trackY += getTotalTrackHeight(i);
                    }

                    // Create initial ghost preview (will be updated in mouseDrag)
                    ghostPreviewBounds = juce::Rectangle<int>(e.x, trackY, 1, trackHeight);

                    repaint();
                    return;
                }
            }
        }
    }

    // Use Smart Tool to determine operation based on click position
    int trackIndex = -1;
    Region* clickedRegion = nullptr;
    SmartToolZone zone = getSmartToolZone(e.x, e.y, clickedRegion, trackIndex);

    if (zone != SmartToolZone::None && clickedRegion != nullptr)
    {
        // Select the region for all operations
        selectedRegion = clickedRegion;
        selectedTrackIndex = trackIndex;

        switch (zone)
        {
        case SmartToolZone::TrimLeft:
            // Alt+drag = time stretch, normal drag = trim
            if (e.mods.isAltDown())
            {
                // Start time stretch from left edge (only for AudioRegions)
                auto* audioRegion = dynamic_cast<AudioRegion*>(clickedRegion);
                if (audioRegion != nullptr)
                {
                    isStretchingRegion = true;
                    resizeEdge = RegionEdge::Left;
                    stretchOriginalRatio = audioRegion->getStretchRatio();
                    stretchOriginalLength = audioRegion->getLength();
                    stretchCurrentRatio = stretchOriginalRatio;
                    stretchCurrentLength = stretchOriginalLength;
                    stretchStartMouseX = e.x;
                    repaint();
                    return;
                }
            }
            // Start left edge trim operation
            isResizingRegion = true;
            resizeEdge = RegionEdge::Left;
            resizeOriginalStart = clickedRegion->getStartPosition();
            resizeOriginalLength = clickedRegion->getLength();
            resizeOriginalOffset = clickedRegion->getOffset();
            resizeCurrentStart = resizeOriginalStart;
            resizeCurrentLength = resizeOriginalLength;
            repaint();
            return;

        case SmartToolZone::TrimRight:
            // Alt+drag = time stretch, normal drag = trim
            if (e.mods.isAltDown())
            {
                // Start time stretch from right edge (only for AudioRegions)
                auto* audioRegion = dynamic_cast<AudioRegion*>(clickedRegion);
                if (audioRegion != nullptr)
                {
                    isStretchingRegion = true;
                    resizeEdge = RegionEdge::Right;
                    stretchOriginalRatio = audioRegion->getStretchRatio();
                    stretchOriginalLength = audioRegion->getLength();
                    stretchCurrentRatio = stretchOriginalRatio;
                    stretchCurrentLength = stretchOriginalLength;
                    stretchStartMouseX = e.x;
                    repaint();
                    return;
                }
            }
            // Start right edge trim operation
            isResizingRegion = true;
            resizeEdge = RegionEdge::Right;
            resizeOriginalStart = clickedRegion->getStartPosition();
            resizeOriginalLength = clickedRegion->getLength();
            resizeOriginalOffset = clickedRegion->getOffset();
            resizeCurrentStart = resizeOriginalStart;
            resizeCurrentLength = resizeOriginalLength;
            repaint();
            return;

        case SmartToolZone::FadeIn:
            // Start fade in editing
            isEditingFade = true;
            isFadeIn = true;
            fadeOriginalLength = clickedRegion->getFadeInLength();
            fadeCurrentLength = fadeOriginalLength;
            fadeStartMouseX = e.x;
            repaint();
            return;

        case SmartToolZone::FadeOut:
            // Start fade out editing
            isEditingFade = true;
            isFadeIn = false;
            fadeOriginalLength = clickedRegion->getFadeOutLength();
            fadeCurrentLength = fadeOriginalLength;
            fadeStartMouseX = e.x;
            repaint();
            return;

        case SmartToolZone::Move:
            // Start dragging the region
            isDraggingRegion = true;
            dragOriginalPosition = clickedRegion->getStartPosition();
            dragCurrentPosition = dragOriginalPosition;

            // Calculate offset from mouse position to region start
            {
                int64_t mouseSample = pixelToSample(e.x);
                dragStartSampleOffset = mouseSample - dragOriginalPosition;
            }
            repaint();
            return;

        case SmartToolZone::None:
        default:
            break;
        }
    }

    // Check if clicking on automation lane
    if (e.x > HEADER_WIDTH && e.y > RULER_HEIGHT)
    {
        // Find which track we're over (accounting for automation lanes)
        int trackYStart = RULER_HEIGHT;
        for (int i = 0; i < timelinePtr->getNumTracks(); ++i)
        {
            int totalHeight = getTotalTrackHeight(i);
            int automationAreaStart = trackYStart + trackHeight;
            int automationAreaEnd = trackYStart + totalHeight;

            // Check if click is in automation area for this track
            if (e.y >= automationAreaStart && e.y < automationAreaEnd)
            {
                int laneIndex = (e.y - automationAreaStart) / AUTOMATION_LANE_HEIGHT;
                if (laneIndex >= 0 && laneIndex < getNumVisibleAutomationLanes(i))
                {
                    handleAutomationMouseDown(e, i, laneIndex);
                    return;
                }
            }

            trackYStart += totalHeight;
        }
    }

    // Click on timeline area (not on a region) - set playhead and clear selection
    if (e.x > HEADER_WIDTH && e.y > RULER_HEIGHT)
    {
        clearSelection();
        int64_t newPos = pixelToSample(e.x);
        transportPtr->setPlayheadPosition(newPos);
        repaint();
    }
}

void TimelineView::mouseDrag(const juce::MouseEvent& e)
{
    // Handle automation point dragging
    if (isDraggingAutomationPoint)
    {
        handleAutomationMouseDrag(e);
        return;
    }

    // Handle fade editing
    if (isEditingFade && selectedRegion != nullptr)
    {
        // Calculate new fade length based on mouse movement
        int deltaX = e.x - fadeStartMouseX;

        // Convert pixels to samples
        int64_t deltaSamples = pixelToSample(HEADER_WIDTH + static_cast<int>(horizontalScrollOffset) + std::abs(deltaX)) -
                               pixelToSample(HEADER_WIDTH + static_cast<int>(horizontalScrollOffset));

        if (isFadeIn)
        {
            // Fade in grows to the right
            fadeCurrentLength = fadeOriginalLength + deltaSamples;
        }
        else
        {
            // Fade out grows to the left (negative deltaX increases fade)
            fadeCurrentLength = fadeOriginalLength - deltaSamples;
        }

        // Clamp fade length to valid range
        int64_t maxFadeLength = selectedRegion->getLength();
        if (isFadeIn)
        {
            maxFadeLength -= selectedRegion->getFadeOutLength();
        }
        else
        {
            maxFadeLength -= selectedRegion->getFadeInLength();
        }

        fadeCurrentLength = juce::jlimit(int64_t(0), maxFadeLength, fadeCurrentLength);
        repaint();
        return;
    }

    // Handle time stretching (Alt+drag edge)
    if (isStretchingRegion && selectedRegion != nullptr)
    {
        auto* audioRegion = dynamic_cast<AudioRegion*>(selectedRegion);
        if (audioRegion != nullptr)
        {
            // Calculate the delta in pixels
            int deltaX = e.x - stretchStartMouseX;

            // Convert to samples for calculating new length
            int64_t deltaSamples = pixelToSample(HEADER_WIDTH + static_cast<int>(horizontalScrollOffset) + std::abs(deltaX)) -
                                   pixelToSample(HEADER_WIDTH + static_cast<int>(horizontalScrollOffset));

            // Calculate new length based on edge being dragged
            int64_t newLength = stretchOriginalLength;
            if (resizeEdge == RegionEdge::Right)
            {
                // Dragging right edge: positive deltaX increases length
                newLength = stretchOriginalLength + deltaSamples * (deltaX > 0 ? 1 : -1);
            }
            else if (resizeEdge == RegionEdge::Left)
            {
                // Dragging left edge: negative deltaX increases length
                newLength = stretchOriginalLength + deltaSamples * (deltaX < 0 ? 1 : -1);
            }

            // Enforce minimum length (at least 1/4 of original at max stretch)
            int64_t minLength = static_cast<int64_t>(stretchOriginalLength * 0.25f / stretchOriginalRatio);
            minLength = juce::jmax(int64_t(1000), minLength);  // At least 1000 samples
            newLength = juce::jmax(minLength, newLength);

            // Calculate new stretch ratio
            // Original audio length = stretchOriginalLength / stretchOriginalRatio
            // New ratio = newLength / originalAudioLength
            float originalAudioLength = static_cast<float>(stretchOriginalLength) / stretchOriginalRatio;
            float newRatio = static_cast<float>(newLength) / originalAudioLength;

            // Clamp to valid stretch range (0.25x to 4.0x)
            newRatio = juce::jlimit(0.25f, 4.0f, newRatio);

            // Recalculate length based on clamped ratio
            stretchCurrentLength = static_cast<int64_t>(originalAudioLength * newRatio);
            stretchCurrentRatio = newRatio;

            repaint();
            return;
        }
    }

    // Handle region edge resizing
    if (isResizingRegion && selectedRegion != nullptr)
    {
        int64_t mouseSample = pixelToSample(e.x);

        if (resizeEdge == RegionEdge::Left)
        {
            // Resizing from left edge - changes start position and length
            int64_t newStart = mouseSample;

            // Snap to grid if enabled
            if (snapToGrid)
            {
                newStart = snapPositionToGrid(newStart);
            }

            // Clamp to non-negative
            newStart = juce::jmax(int64_t(0), newStart);

            // Don't allow moving start past original end position
            int64_t originalEnd = resizeOriginalStart + resizeOriginalLength;
            newStart = juce::jmin(newStart, originalEnd - 1);

            // Calculate new length and offset change
            int64_t deltaStart = newStart - resizeOriginalStart;
            int64_t newLength = resizeOriginalLength - deltaStart;

            // Ensure minimum length
            newLength = juce::jmax(int64_t(1), newLength);

            resizeCurrentStart = newStart;
            resizeCurrentLength = newLength;
        }
        else if (resizeEdge == RegionEdge::Right)
        {
            // Resizing from right edge - only changes length
            int64_t newEnd = mouseSample;

            // Snap to grid if enabled
            if (snapToGrid)
            {
                newEnd = snapPositionToGrid(newEnd);
            }

            // Calculate new length
            int64_t newLength = newEnd - resizeOriginalStart;

            // Ensure minimum length
            newLength = juce::jmax(int64_t(1), newLength);

            resizeCurrentStart = resizeOriginalStart;
            resizeCurrentLength = newLength;
        }

        repaint();
        return;
    }

    // Handle region dragging
    if (isDraggingRegion && selectedRegion != nullptr)
    {
        // Calculate new position based on mouse, accounting for initial offset
        int64_t mouseSample = pixelToSample(e.x);
        int64_t newPosition = mouseSample - dragStartSampleOffset;

        // Clamp to non-negative
        newPosition = juce::jmax(int64_t(0), newPosition);

        // Snap to grid if enabled
        if (snapToGrid)
        {
            newPosition = snapPositionToGrid(newPosition);
        }

        // Update drag position for visual feedback
        dragCurrentPosition = newPosition;
        repaint();
        return;
    }

    // Handle Draw tool region creation - update ghost preview
    if (isDrawingRegion && drawTrackIndex >= 0 && timelinePtr != nullptr)
    {
        // Convert start and current X to sample positions
        int64_t startSample = pixelToSample(drawStartX);
        int64_t endSample = pixelToSample(e.x);

        // Snap to grid if enabled
        if (snapToGrid)
        {
            startSample = snapPositionToGrid(startSample);
            endSample = snapPositionToGrid(endSample);
        }

        // Ensure start < end (allow dragging in either direction)
        if (endSample < startSample)
            std::swap(startSample, endSample);

        // Enforce minimum length (1 beat worth of samples)
        int64_t minLength = timelinePtr->beatsToSamples(1.0);
        if (endSample - startSample < minLength)
            endSample = startSample + minLength;

        // Calculate track Y position
        int trackY = RULER_HEIGHT;
        for (int i = 0; i < drawTrackIndex; ++i)
        {
            trackY += getTotalTrackHeight(i);
        }

        // Convert samples back to screen pixels for ghost preview
        int startPixel = sampleToPixel(startSample) - static_cast<int>(horizontalScrollOffset) + HEADER_WIDTH;
        int endPixel = sampleToPixel(endSample) - static_cast<int>(horizontalScrollOffset) + HEADER_WIDTH;

        // Update ghost preview bounds
        ghostPreviewBounds = juce::Rectangle<int>(startPixel, trackY, endPixel - startPixel, trackHeight);

        repaint();
        return;
    }

    // If no region is being dragged, allow playhead scrubbing
    if (transportPtr != nullptr && e.x > HEADER_WIDTH)
    {
        int64_t newPos = pixelToSample(e.x);
        transportPtr->setPlayheadPosition(juce::jmax(int64_t(0), newPos));
        repaint();
    }
}

void TimelineView::mouseUp(const juce::MouseEvent& e)
{
    // Finalize automation editing
    if (isDraggingAutomationPoint)
    {
        handleAutomationMouseUp(e);
        return;
    }

    // Finalize fade editing
    if (isEditingFade && selectedRegion != nullptr)
    {
        // Only create undo action if fade length actually changed
        if (fadeCurrentLength != fadeOriginalLength)
        {
            // Determine the new fade in/out values
            int64_t newFadeIn = isFadeIn ? fadeCurrentLength : selectedRegion->getFadeInLength();
            int64_t newFadeOut = isFadeIn ? selectedRegion->getFadeOutLength() : fadeCurrentLength;

            if (undoManagerPtr != nullptr)
            {
                undoManagerPtr->perform(new SetRegionFadesAction(selectedRegion, newFadeIn, newFadeOut),
                                       isFadeIn ? "Set Fade In" : "Set Fade Out");
            }
            else
            {
                // Fallback: direct modification
                if (isFadeIn)
                {
                    selectedRegion->setFadeInLength(fadeCurrentLength);
                }
                else
                {
                    selectedRegion->setFadeOutLength(fadeCurrentLength);
                }
            }
        }

        isEditingFade = false;
        fadeOriginalLength = 0;
        fadeCurrentLength = 0;
        repaint();
        return;
    }

    // Finalize time stretching
    if (isStretchingRegion && selectedRegion != nullptr)
    {
        auto* audioRegion = dynamic_cast<AudioRegion*>(selectedRegion);

        // Check if anything actually changed
        bool hasChanges = (stretchCurrentRatio != stretchOriginalRatio);

        if (hasChanges && audioRegion != nullptr)
        {
            if (undoManagerPtr != nullptr)
            {
                undoManagerPtr->perform(new StretchRegionAction(audioRegion, stretchCurrentRatio, stretchCurrentLength),
                                       "Time Stretch Region");
            }
            else
            {
                // Fallback: direct modification
                audioRegion->setStretchRatio(stretchCurrentRatio);
                audioRegion->setLength(stretchCurrentLength);
            }
        }

        isStretchingRegion = false;
        stretchOriginalRatio = 1.0f;
        stretchOriginalLength = 0;
        stretchCurrentRatio = 1.0f;
        stretchCurrentLength = 0;
        resizeEdge = RegionEdge::None;
        repaint();
        return;
    }

    // Finalize region edge resize
    if (isResizingRegion && selectedRegion != nullptr)
    {
        // Check if anything actually changed
        bool hasChanges = (resizeCurrentLength != resizeOriginalLength) ||
                          (resizeEdge == RegionEdge::Left && resizeCurrentStart != resizeOriginalStart);

        if (hasChanges)
        {
            // Check if it's an audio region for undo support
            auto* audioRegion = dynamic_cast<AudioRegion*>(selectedRegion);

            if (resizeEdge == RegionEdge::Left)
            {
                // When resizing from left, we also need to adjust the offset
                int64_t deltaStart = resizeCurrentStart - resizeOriginalStart;
                int64_t newOffset = resizeOriginalOffset + deltaStart;

                // Clamp offset to non-negative
                newOffset = juce::jmax(int64_t(0), newOffset);

                if (audioRegion != nullptr && undoManagerPtr != nullptr)
                {
                    // Begin a transaction to group the move and resize together
                    undoManagerPtr->beginNewTransaction("Trim Region Left Edge");

                    // First, perform the resize (length and offset change)
                    undoManagerPtr->perform(new ResizeRegionAction(audioRegion, resizeCurrentLength, newOffset),
                                           "Resize Region");

                    // Then perform the move (start position change)
                    // Note: We need to manually handle start position since ResizeRegionAction doesn't track it
                    // Store old start for undo by performing MoveRegionAction
                    undoManagerPtr->perform(new MoveRegionAction(audioRegion, resizeCurrentStart),
                                           "Move Region Start");
                }
                else
                {
                    // Fallback: direct modification
                    selectedRegion->setOffset(newOffset);
                    selectedRegion->setStartPosition(resizeCurrentStart);
                    selectedRegion->setLength(resizeCurrentLength);
                }
            }
            else if (resizeEdge == RegionEdge::Right)
            {
                // Right edge only changes length
                if (audioRegion != nullptr && undoManagerPtr != nullptr)
                {
                    undoManagerPtr->perform(new ResizeRegionAction(audioRegion, resizeCurrentLength),
                                           "Trim Region Right Edge");
                }
                else
                {
                    // Fallback: direct modification
                    selectedRegion->setLength(resizeCurrentLength);
                }
            }
        }

        isResizingRegion = false;
        resizeEdge = RegionEdge::None;
        repaint();
        return;
    }

    // Finalize region drag
    if (isDraggingRegion && selectedRegion != nullptr)
    {
        // Only create undo action if position actually changed
        if (dragCurrentPosition != dragOriginalPosition)
        {
            // Check if it's an audio region and use undo manager if available
            if (auto* audioRegion = dynamic_cast<AudioRegion*>(selectedRegion))
            {
                if (undoManagerPtr != nullptr)
                {
                    undoManagerPtr->perform(new MoveRegionAction(audioRegion, dragCurrentPosition),
                                           "Move Region");
                }
                else
                {
                    // Fallback: direct modification
                    audioRegion->setStartPosition(dragCurrentPosition);
                }
            }
            else
            {
                // For non-audio regions, apply directly (MIDI regions don't have undo actions yet)
                selectedRegion->setStartPosition(dragCurrentPosition);
            }
        }
        isDraggingRegion = false;
        repaint();
        return;
    }

    // Finalize Draw tool region creation
    if (isDrawingRegion && drawTrackIndex >= 0 && timelinePtr != nullptr)
    {
        // Calculate final sample positions from mouse coordinates
        int64_t startSample = pixelToSample(drawStartX);
        int64_t endSample = pixelToSample(e.x);

        // Snap to grid if enabled
        if (snapToGrid)
        {
            startSample = snapPositionToGrid(startSample);
            endSample = snapPositionToGrid(endSample);
        }

        // Ensure start < end (allow dragging in either direction)
        if (endSample < startSample)
            std::swap(startSample, endSample);

        // Enforce minimum length (1 beat worth of samples)
        int64_t minLength = timelinePtr->beatsToSamples(1.0);
        if (endSample - startSample < minLength)
            endSample = startSample + minLength;

        int64_t regionLength = endSample - startSample;

        // Get the track and create appropriate region type
        Track* track = timelinePtr->getTrack(drawTrackIndex);
        if (track != nullptr && regionLength > 0)
        {
            if (track->getType() == TrackType::Audio)
            {
                // Create empty AudioRegion on audio track
                auto* audioTrack = static_cast<AudioTrack*>(track);
                auto newRegion = std::make_unique<AudioRegion>(startSample, regionLength);
                newRegion->setName("New Audio Region");

                // Initialize empty audio buffer (silent)
                juce::AudioBuffer<float> emptyBuffer(2, static_cast<int>(regionLength));
                emptyBuffer.clear();
                newRegion->setAudioBuffer(emptyBuffer);

                if (undoManagerPtr != nullptr)
                {
                    undoManagerPtr->perform(new AddRegionAction(*audioTrack, std::move(newRegion)),
                                           "Create Audio Region");
                }
                else
                {
                    audioTrack->addRegion(std::move(newRegion));
                }
            }
            else if (track->getType() == TrackType::MIDI)
            {
                // Create empty MidiRegion on MIDI track
                // Note: MIDI regions don't have undo actions yet, so add directly
                auto* midiTrack = static_cast<MidiTrack*>(track);
                auto newRegion = std::make_unique<MidiRegion>(startSample, regionLength);
                newRegion->setName("New MIDI Region");

                midiTrack->addRegion(std::move(newRegion));
            }
        }

        // Reset drawing state
        isDrawingRegion = false;
        drawTrackIndex = -1;
        drawStartX = 0;
        ghostPreviewBounds = juce::Rectangle<int>();

        repaint();
        return;
    }

    isDraggingRegion = false;
    isResizingRegion = false;
    isEditingFade = false;
    isDrawingRegion = false;
    isStretchingRegion = false;
}

void TimelineView::mouseMove(const juce::MouseEvent& e)
{
    int trackIndex = -1;
    Region* region = getRegionAtPosition(e.x, e.y, trackIndex);

    // Update hover state if changed
    if (region != hoveredRegion || trackIndex != hoveredTrackIndex)
    {
        hoveredRegion = region;
        hoveredTrackIndex = trackIndex;
        repaint();
    }

    // Update cursor based on position (edge detection for resize)
    updateCursorForPosition(e.x, e.y);
}

void TimelineView::mouseExit(const juce::MouseEvent& /*e*/)
{
    // Clear hover state when mouse leaves component
    if (hoveredRegion != nullptr || hoveredTrackIndex != -1)
    {
        hoveredRegion = nullptr;
        hoveredTrackIndex = -1;
        repaint();
    }

    // Reset cursor to current tool mode's cursor
    setMouseCursor(Cursors::getCursorForTool(currentToolMode));
}

void TimelineView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown())
    {
        // Zoom with Ctrl+wheel
        if (wheel.deltaY > 0)
            zoomIn();
        else if (wheel.deltaY < 0)
            zoomOut();
    }
    else
    {
        // Horizontal scroll
        horizontalScrollOffset -= wheel.deltaY * 100.0;
        horizontalScrollOffset = juce::jmax(0.0, horizontalScrollOffset);
        repaint();
    }
}

bool TimelineView::keyPressed(const juce::KeyPress& key)
{
    // Delete key - delete selected region
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (selectedRegion != nullptr && selectedTrackIndex >= 0)
        {
            deleteSelectedRegion();
            return true;
        }
    }

    // Ctrl+X - cut region
    if (key == juce::KeyPress('x', juce::ModifierKeys::commandModifier, 0))
    {
        if (selectedRegion != nullptr && selectedTrackIndex >= 0)
        {
            cutSelectedRegion();
            return true;
        }
    }

    // Ctrl+C - copy region
    if (key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0))
    {
        if (selectedRegion != nullptr && selectedTrackIndex >= 0)
        {
            copySelectedRegion();
            return true;
        }
    }

    // Ctrl+V - paste region
    if (key == juce::KeyPress('v', juce::ModifierKeys::commandModifier, 0))
    {
        if (clipboard.hasData)
        {
            // Paste at playhead position if transport is available, otherwise use last click position
            int64_t pastePosition = lastClickSamplePosition;
            if (transportPtr != nullptr)
            {
                pastePosition = transportPtr->getPlayheadPosition();
            }
            pasteRegion(pastePosition);
            return true;
        }
    }

    return false;
}

void TimelineView::zoomIn()
{
    setPixelsPerBeat(pixelsPerBeat * 1.2);
}

void TimelineView::zoomOut()
{
    setPixelsPerBeat(pixelsPerBeat / 1.2);
}

void TimelineView::drawTimeRuler(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(MidiSingLookAndFeel::backgroundMid);
    g.fillRect(bounds);

    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawLine(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getBottom()),
               static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom()));

    if (timelinePtr == nullptr)
        return;

    // Draw beat markers
    g.setFont(10.0f);
    int beatsPerBar = timelinePtr->getBeatsPerBar();

    double startBeat = horizontalScrollOffset / pixelsPerBeat;
    double endBeat = startBeat + bounds.getWidth() / pixelsPerBeat;

    for (int beat = static_cast<int>(startBeat); beat <= static_cast<int>(endBeat) + 1; ++beat)
    {
        int x = bounds.getX() + static_cast<int>(beatToPixel(beat) - horizontalScrollOffset);
        
        if (x < bounds.getX() || x > bounds.getRight())
            continue;

        bool isBarStart = (beat % beatsPerBar) == 0;

        if (isBarStart)
        {
            // Bar line
            g.setColour(MidiSingLookAndFeel::textColour);
            g.drawLine(static_cast<float>(x), static_cast<float>(bounds.getY()),
                       static_cast<float>(x), static_cast<float>(bounds.getBottom()), 1.0f);

            // Bar number
            int barNumber = (beat / beatsPerBar) + 1;
            g.drawText(juce::String(barNumber), x + 2, bounds.getY(), 30, bounds.getHeight(),
                       juce::Justification::centredLeft);
        }
        else
        {
            // Beat tick
            g.setColour(MidiSingLookAndFeel::textDimColour);
            int tickHeight = bounds.getHeight() / 3;
            g.drawLine(static_cast<float>(x), static_cast<float>(bounds.getBottom() - tickHeight),
                       static_cast<float>(x), static_cast<float>(bounds.getBottom()), 0.5f);
        }
    }
}

void TimelineView::drawTrackLane(juce::Graphics& g, juce::Rectangle<int> bounds, int trackIndex)
{
    if (timelinePtr == nullptr)
        return;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return;

    // Track background
    g.setColour(MidiSingLookAndFeel::backgroundLight);
    g.fillRect(bounds);

    // Draw beat grid
    drawBeatGrid(g, bounds);

    // Bottom border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawLine(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getBottom()),
               static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom()));

    // Draw regions
    if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
    {
        for (int i = 0; i < audioTrack->getNumRegions(); ++i)
        {
            auto* region = audioTrack->getRegion(i);
            
            // Calculate pixel bounds
            int64_t startSample = region->getStartPosition();
            int64_t endSample = region->getEndPosition();
            
            int x1 = bounds.getX() + sampleToPixel(startSample) - static_cast<int>(horizontalScrollOffset);
            int x2 = bounds.getX() + sampleToPixel(endSample) - static_cast<int>(horizontalScrollOffset);
            int w = x2 - x1;
            
            if (x2 < bounds.getX() || x1 > bounds.getRight())
                continue;
                
            juce::Rectangle<int> regionBounds(x1, bounds.getY() + 2, w, bounds.getHeight() - 4);

            // Determine if this region is selected or hovered
            bool isSelected = (region == selectedRegion && trackIndex == selectedTrackIndex);
            bool isHovered = (region == hoveredRegion && trackIndex == hoveredTrackIndex);

            // Draw region box with appropriate colour
            juce::Colour fillColour = MidiSingLookAndFeel::regionColour;
            if (isSelected)
                fillColour = fillColour.brighter(0.3f);
            else if (isHovered)
                fillColour = fillColour.brighter(0.15f);

            g.setColour(fillColour);
            g.fillRect(regionBounds);

            // Draw border - brighter for selected regions
            if (isSelected)
                g.setColour(MidiSingLookAndFeel::accentColour);
            else
                g.setColour(MidiSingLookAndFeel::borderColour);
            g.drawRect(regionBounds, isSelected ? 2 : 1);

            // Draw waveform
            int64_t hash = region->getThumbnailHash();
            if (hash == 0)
            {
                hash = waveformCache.addAudioBuffer(region->getAudioBuffer());
                region->setThumbnailHash(hash);
            }

            // Draw waveform based on display mode
            if (waveformDisplayMode == WaveformDisplayMode::SpectrogramOverlay || spectrogramOverlayEnabled)
            {
                // Draw spectrogram overlay first (behind waveform)
                drawSpectrogramOverlay(g, regionBounds.reduced(1), region,
                                        region->getOffset(), region->getLength());

                // Draw waveform on top with reduced opacity
                if (waveformCache.hasMipmap(hash))
                {
                    // Use a slightly dimmed waveform over spectrogram
                    float savedAlpha = spectrogramAlpha;
                    drawWaveformRmsPlusPeak(g, regionBounds.reduced(1), hash,
                                             region->getOffset(), region->getLength());
                    juce::ignoreUnused(savedAlpha);
                }
            }
            else if (waveformDisplayMode == WaveformDisplayMode::RmsPlusPeak && waveformCache.hasMipmap(hash))
            {
                // Draw using mipmap data with RMS + Peak display
                drawWaveformRmsPlusPeak(g, regionBounds.reduced(1), hash,
                                         region->getOffset(), region->getLength());
            }
            else
            {
                // Legacy peak-only mode using AudioThumbnail
                g.setColour(juce::Colours::white.withAlpha(0.8f));
                auto& thumb = waveformCache.getThumbnail(hash);
                double thumbStart = static_cast<double>(region->getOffset()) / sampleRate;
                double thumbEnd = thumbStart + static_cast<double>(region->getLength()) / sampleRate;
                thumb.drawChannel(g, regionBounds.reduced(1), thumbStart, thumbEnd, 0, 1.0f);
            }

            // Draw fade in/out overlays
            int64_t fadeInLen = region->getFadeInLength();
            int64_t fadeOutLen = region->getFadeOutLength();

            if (fadeInLen > 0)
            {
                // Draw fade in as diagonal line with semi-transparent overlay
                int fadeInEndX = regionBounds.getX() + sampleToPixel(fadeInLen);
                juce::Path fadeInPath;
                fadeInPath.startNewSubPath(static_cast<float>(regionBounds.getX()),
                                           static_cast<float>(regionBounds.getBottom()));
                fadeInPath.lineTo(static_cast<float>(regionBounds.getX()),
                                 static_cast<float>(regionBounds.getY()));
                fadeInPath.lineTo(static_cast<float>(fadeInEndX),
                                 static_cast<float>(regionBounds.getY()));
                fadeInPath.closeSubPath();

                g.setColour(juce::Colours::black.withAlpha(0.3f));
                g.fillPath(fadeInPath);

                // Draw fade line
                g.setColour(juce::Colours::white.withAlpha(0.7f));
                g.drawLine(static_cast<float>(regionBounds.getX()), static_cast<float>(regionBounds.getBottom()),
                           static_cast<float>(fadeInEndX), static_cast<float>(regionBounds.getY()), 1.5f);
            }

            if (fadeOutLen > 0)
            {
                // Draw fade out as diagonal line with semi-transparent overlay
                int fadeOutStartX = regionBounds.getRight() - sampleToPixel(fadeOutLen);
                juce::Path fadeOutPath;
                fadeOutPath.startNewSubPath(static_cast<float>(fadeOutStartX),
                                            static_cast<float>(regionBounds.getY()));
                fadeOutPath.lineTo(static_cast<float>(regionBounds.getRight()),
                                  static_cast<float>(regionBounds.getY()));
                fadeOutPath.lineTo(static_cast<float>(regionBounds.getRight()),
                                  static_cast<float>(regionBounds.getBottom()));
                fadeOutPath.closeSubPath();

                g.setColour(juce::Colours::black.withAlpha(0.3f));
                g.fillPath(fadeOutPath);

                // Draw fade line
                g.setColour(juce::Colours::white.withAlpha(0.7f));
                g.drawLine(static_cast<float>(fadeOutStartX), static_cast<float>(regionBounds.getY()),
                           static_cast<float>(regionBounds.getRight()), static_cast<float>(regionBounds.getBottom()), 1.5f);
            }

            // Draw name
            g.setColour(juce::Colours::white);
            g.drawText(region->getName(), regionBounds.reduced(2), juce::Justification::topLeft, true);
        }
    }
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
    {
        for (int i = 0; i < midiTrack->getNumRegions(); ++i)
        {
            auto* region = midiTrack->getRegion(i);
            
            // Calculate pixel bounds
            int64_t startSample = region->getStartPosition();
            int64_t endSample = region->getEndPosition();
            
            int x1 = bounds.getX() + sampleToPixel(startSample) - static_cast<int>(horizontalScrollOffset);
            int x2 = bounds.getX() + sampleToPixel(endSample) - static_cast<int>(horizontalScrollOffset);
            int w = x2 - x1;
            
            if (x2 < bounds.getX() || x1 > bounds.getRight())
                continue;
                
            juce::Rectangle<int> regionBounds(x1, bounds.getY() + 2, w, bounds.getHeight() - 4);

            // Determine if this region is selected or hovered
            bool isSelected = (region == selectedRegion && trackIndex == selectedTrackIndex);
            bool isHovered = (region == hoveredRegion && trackIndex == hoveredTrackIndex);

            // Draw region box with appropriate colour
            juce::Colour fillColour = MidiSingLookAndFeel::regionColour.withHue(0.1f); // Different hue for MIDI
            if (isSelected)
                fillColour = fillColour.brighter(0.3f);
            else if (isHovered)
                fillColour = fillColour.brighter(0.15f);

            g.setColour(fillColour);
            g.fillRect(regionBounds);

            // Draw border - brighter for selected regions
            if (isSelected)
                g.setColour(MidiSingLookAndFeel::accentColour);
            else
                g.setColour(MidiSingLookAndFeel::borderColour);
            g.drawRect(regionBounds, isSelected ? 2 : 1);

            // Draw MIDI notes preview
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            const auto& sequence = region->getMidiSequence();
            
            double regionHeight = static_cast<double>(regionBounds.getHeight());
            double noteHeight = juce::jmax(2.0, regionHeight / 24.0); // Rough approximation
            juce::ignoreUnused(noteHeight);  // Currently unused, kept for future note length visualization

            // Calculate relative offset of the region
            int64_t regionOffset = region->getOffset();

            // Iterate through events
            for (int e = 0; e < sequence.getNumEvents(); ++e)
            {
                auto* event = sequence.getEventPointer(e);
                if (event->message.isNoteOn())
                {
                     // Time is in samples for MidiTrack logic, but in seconds or ticks in raw sequence?
                     // Verify: imported sequence has timestamps converted to samples in previous step?
                     // YES, importTrack converted seconds * sampleRate.
                     
                     // However, MidiTrack::processBlock adds regionStart + event->message.getTimeStamp().
                     // So event->message.getTimeStamp() is relative to region start IN SAMPLES.
                     
                     int64_t eventTime = static_cast<int64_t>(event->message.getTimeStamp());
                     int64_t noteStart = eventTime - regionOffset;
                     
                     // Find matching note off?
                     // For preview, fixed length or simple duration is consistent?
                     // MidiMessageSequence has paired events if matches mapped.
                     // But drawing NoteOns is simpler. Let's try to get length.
                     
                     double noteLengthSamples = 0;
                     juce::ignoreUnused(noteLengthSamples);  // Kept for future note length visualization
                     // Rough check for note length or assume default for now to be safe/fast
                     // Really we should iterate matches pairs if updated.
                     // But sequence is straight list.

                     // Just draw simple dots/lines for note ons
                     int noteX = x1 + sampleToPixel(noteStart) - sampleToPixel(0);
                     int noteY = regionBounds.getY() + static_cast<int>(regionHeight * (1.0 - (event->message.getNoteNumber() / 128.0)));

                     g.fillRect(noteX, noteY, 4, 2);
                }
            }

            // Draw fade in/out overlays for MIDI regions
            int64_t fadeInLen = region->getFadeInLength();
            int64_t fadeOutLen = region->getFadeOutLength();

            if (fadeInLen > 0)
            {
                // Draw fade in as diagonal line with semi-transparent overlay
                int fadeInEndX = regionBounds.getX() + sampleToPixel(fadeInLen);
                juce::Path fadeInPath;
                fadeInPath.startNewSubPath(static_cast<float>(regionBounds.getX()),
                                           static_cast<float>(regionBounds.getBottom()));
                fadeInPath.lineTo(static_cast<float>(regionBounds.getX()),
                                 static_cast<float>(regionBounds.getY()));
                fadeInPath.lineTo(static_cast<float>(fadeInEndX),
                                 static_cast<float>(regionBounds.getY()));
                fadeInPath.closeSubPath();

                g.setColour(juce::Colours::black.withAlpha(0.3f));
                g.fillPath(fadeInPath);

                // Draw fade line
                g.setColour(juce::Colours::white.withAlpha(0.7f));
                g.drawLine(static_cast<float>(regionBounds.getX()), static_cast<float>(regionBounds.getBottom()),
                           static_cast<float>(fadeInEndX), static_cast<float>(regionBounds.getY()), 1.5f);
            }

            if (fadeOutLen > 0)
            {
                // Draw fade out as diagonal line with semi-transparent overlay
                int fadeOutStartX = regionBounds.getRight() - sampleToPixel(fadeOutLen);
                juce::Path fadeOutPath;
                fadeOutPath.startNewSubPath(static_cast<float>(fadeOutStartX),
                                            static_cast<float>(regionBounds.getY()));
                fadeOutPath.lineTo(static_cast<float>(regionBounds.getRight()),
                                  static_cast<float>(regionBounds.getY()));
                fadeOutPath.lineTo(static_cast<float>(regionBounds.getRight()),
                                  static_cast<float>(regionBounds.getBottom()));
                fadeOutPath.closeSubPath();

                g.setColour(juce::Colours::black.withAlpha(0.3f));
                g.fillPath(fadeOutPath);

                // Draw fade line
                g.setColour(juce::Colours::white.withAlpha(0.7f));
                g.drawLine(static_cast<float>(fadeOutStartX), static_cast<float>(regionBounds.getY()),
                           static_cast<float>(regionBounds.getRight()), static_cast<float>(regionBounds.getBottom()), 1.5f);
            }

            // Draw name
            g.setColour(juce::Colours::white);
            g.drawText(region->getName(), regionBounds.reduced(2), juce::Justification::topLeft, true);
        }
    }

    // Track is muted - draw overlay
    if (track->isMuted())
    {
        g.setColour(juce::Colour(0x40000000));
        g.fillRect(bounds);
    }
}

void TimelineView::drawBeatGrid(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (timelinePtr == nullptr)
        return;

    int beatsPerBar = timelinePtr->getBeatsPerBar();
    double startBeat = horizontalScrollOffset / pixelsPerBeat;
    double endBeat = startBeat + bounds.getWidth() / pixelsPerBeat;

    // Get current snap resolution to determine sub-beat grid visibility
    SnapResolution snapRes = getZoomAwareSnapResolution();
    double snapDivision = getSnapDivisionForResolution(snapRes);

    // Draw sub-beat grid lines when zoomed in enough
    if (snapDivision < 1.0)
    {
        // Calculate sub-beat positions
        double subBeatStart = std::floor(startBeat / snapDivision) * snapDivision;
        double subBeatEnd = endBeat + snapDivision;

        for (double subBeat = subBeatStart; subBeat <= subBeatEnd; subBeat += snapDivision)
        {
            // Skip whole beats (they'll be drawn below)
            double fractionalPart = subBeat - std::floor(subBeat);
            if (fractionalPart < 0.001 || fractionalPart > 0.999)
                continue;

            int x = bounds.getX() + static_cast<int>(beatToPixel(subBeat) - horizontalScrollOffset);

            if (x < bounds.getX() || x > bounds.getRight())
                continue;

            // Lighter color for sub-beat lines, even lighter for finer divisions
            float alpha = 0.15f;
            if (snapDivision <= 0.125)
                alpha = 0.08f;
            else if (snapDivision <= 0.25)
                alpha = 0.12f;

            g.setColour(MidiSingLookAndFeel::borderColour.withAlpha(alpha));
            g.drawLine(static_cast<float>(x), static_cast<float>(bounds.getY()),
                       static_cast<float>(x), static_cast<float>(bounds.getBottom()), 0.5f);
        }
    }

    // Draw main beat and bar grid lines
    for (int beat = static_cast<int>(startBeat); beat <= static_cast<int>(endBeat) + 1; ++beat)
    {
        int x = bounds.getX() + static_cast<int>(beatToPixel(beat) - horizontalScrollOffset);

        if (x < bounds.getX() || x > bounds.getRight())
            continue;

        bool isBarStart = (beat % beatsPerBar) == 0;
        g.setColour(isBarStart ? MidiSingLookAndFeel::borderColour
                               : MidiSingLookAndFeel::borderColour.withAlpha(0.3f));
        g.drawLine(static_cast<float>(x), static_cast<float>(bounds.getY()),
                   static_cast<float>(x), static_cast<float>(bounds.getBottom()),
                   isBarStart ? 1.0f : 0.5f);
    }
}

void TimelineView::drawPlayhead(juce::Graphics& g)
{
    if (transportPtr == nullptr || timelinePtr == nullptr)
        return;

    int64_t pos = transportPtr->getPlayheadPosition();
    int x = HEADER_WIDTH + sampleToPixel(pos) - static_cast<int>(horizontalScrollOffset);

    if (x < HEADER_WIDTH || x > getWidth())
        return;

    // Playhead triangle at top
    juce::Path triangle;
    triangle.addTriangle(static_cast<float>(x - 5), static_cast<float>(RULER_HEIGHT),
                         static_cast<float>(x + 5), static_cast<float>(RULER_HEIGHT),
                         static_cast<float>(x), static_cast<float>(RULER_HEIGHT - 8));

    g.setColour(MidiSingLookAndFeel::accentColour);
    g.fillPath(triangle);

    // Playhead line
    g.drawLine(static_cast<float>(x), static_cast<float>(RULER_HEIGHT),
               static_cast<float>(x), static_cast<float>(getHeight()), 2.0f);
}

void TimelineView::updateTrackHeaders()
{
    if (timelinePtr == nullptr)
        return;

    // Ensure we have the right number of track headers
    while (static_cast<int>(trackHeaders.size()) < timelinePtr->getNumTracks())
    {
        auto header = std::make_unique<TrackHeader>();
        addAndMakeVisible(header.get());

        // Set up automation lane callback
        header->onToggleAutomationLane = [this](int trkIdx, const juce::String& paramName)
        {
            toggleAutomationLane(trkIdx, paramName);
        };

        trackHeaders.push_back(std::move(header));
    }

    while (static_cast<int>(trackHeaders.size()) > timelinePtr->getNumTracks())
    {
        trackHeaders.pop_back();
    }

    // Position and configure headers (accounting for automation lanes)
    int y = RULER_HEIGHT;
    for (size_t i = 0; i < trackHeaders.size(); ++i)
    {
        int trackIdx = static_cast<int>(i);
        trackHeaders[i]->setBounds(0, y, HEADER_WIDTH, trackHeight);
        trackHeaders[i]->setTrack(timelinePtr->getTrack(trackIdx));
        trackHeaders[i]->setTrackIndex(trackIdx);

        // Account for automation lanes height
        y += getTotalTrackHeight(trackIdx);
    }
}

void TimelineView::updateAutomationLaneViews()
{
    if (timelinePtr == nullptr)
        return;

    // Count total visible automation lanes across all tracks
    int totalVisibleLanes = 0;
    for (int i = 0; i < timelinePtr->getNumTracks(); ++i)
    {
        totalVisibleLanes += getNumVisibleAutomationLanes(i);
    }

    // Resize the vector if needed - create new views
    while (static_cast<int>(automationLaneViews.size()) < totalVisibleLanes)
    {
        auto view = std::make_unique<AutomationLaneView>();
        addAndMakeVisible(view.get());

        // Wire up the automation changed callback
        view->onAutomationChanged = [this]()
        {
            repaint();  // Trigger redraw when automation is modified
        };

        automationLaneViews.push_back(std::move(view));
    }

    // Remove excess views
    while (static_cast<int>(automationLaneViews.size()) > totalVisibleLanes)
    {
        automationLaneViews.pop_back();
    }

    // Position and configure each automation lane view
    int viewIndex = 0;
    int y = RULER_HEIGHT;

    for (int trackIdx = 0; trackIdx < timelinePtr->getNumTracks(); ++trackIdx)
    {
        Track* track = timelinePtr->getTrack(trackIdx);
        if (track == nullptr)
        {
            y += getTotalTrackHeight(trackIdx);
            continue;
        }

        // Skip past the track lane area
        y += trackHeight;

        // Position automation lane views for this track
        for (size_t laneIdx = 0; laneIdx < track->getNumAutomationLanes(); ++laneIdx)
        {
            AutomationLane* lane = track->getAutomationLane(laneIdx);
            if (lane != nullptr && lane->isVisible())
            {
                if (viewIndex < static_cast<int>(automationLaneViews.size()))
                {
                    auto& view = automationLaneViews[static_cast<size_t>(viewIndex)];

                    // Set the automation lane
                    view->setAutomationLane(lane);

                    // Position the view (after header area, at the correct Y position)
                    view->setBounds(HEADER_WIDTH, y, getWidth() - HEADER_WIDTH, AUTOMATION_LANE_HEIGHT);

                    // Sync view settings
                    view->setPixelsPerBeat(pixelsPerBeat);
                    view->setHorizontalScrollOffset(horizontalScrollOffset);
                    view->setSampleRate(sampleRate);

                    // Set tempo from timeline if available
                    if (timelinePtr != nullptr)
                    {
                        view->setTempo(timelinePtr->getTempo());
                    }

                    // Configure display options
                    view->setShowHeader(true);
                    view->setShowValueReadout(true);
                    view->setSnapEnabled(snapToGrid);

                    ++viewIndex;
                }
                y += AUTOMATION_LANE_HEIGHT;
            }
        }
    }
}

void TimelineView::syncAutomationLaneViewSettings()
{
    // Sync settings to all automation lane views without recreating them
    for (auto& view : automationLaneViews)
    {
        if (view != nullptr)
        {
            view->setPixelsPerBeat(pixelsPerBeat);
            view->setHorizontalScrollOffset(horizontalScrollOffset);
            view->setSampleRate(sampleRate);

            if (timelinePtr != nullptr)
            {
                view->setTempo(timelinePtr->getTempo());
            }

            view->setSnapEnabled(snapToGrid);
        }
    }
}

int TimelineView::sampleToPixel(int64_t samples) const
{
    if (timelinePtr == nullptr)
        return 0;

    double beats = timelinePtr->samplesToBeats(samples);
    return static_cast<int>(beats * pixelsPerBeat);
}

int64_t TimelineView::pixelToSample(int x) const
{
    if (timelinePtr == nullptr)
        return 0;

    double beats = (x - HEADER_WIDTH + horizontalScrollOffset) / pixelsPerBeat;
    return timelinePtr->beatsToSamples(beats);
}

double TimelineView::beatToPixel(double beats) const
{
    return beats * pixelsPerBeat;
}

double TimelineView::pixelToBeat(int x) const
{
    return (x - HEADER_WIDTH + horizontalScrollOffset) / pixelsPerBeat;
}

bool TimelineView::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& file : files)
    {
        if (audioImporter.isSupported(juce::File(file)) || midiImporter.isSupported(juce::File(file)))
            return true;
    }
    return false;
}

void TimelineView::filesDropped(const juce::StringArray& files, int x, int y)
{
    isDraggingFile = false;
    repaint();
    
    if (timelinePtr == nullptr)
        return;

    int trackIndex = getTrackIndexAtY(y);
    
    for (const auto& filePath : files)
    {
        juce::File file(filePath);
        if (audioImporter.isSupported(file))
        {
            importAudioFileToTrack(file, trackIndex, x);
        }
        else if (midiImporter.isSupported(file))
        {
            importMidiFileToTrack(file, trackIndex, x);
        }
    }
}

void TimelineView::fileDragEnter(const juce::StringArray& /*files*/, int /*x*/, int /*y*/)
{
    isDraggingFile = true;
    repaint();
}

void TimelineView::fileDragExit(const juce::StringArray& /*files*/)
{
    isDraggingFile = false;
    repaint();
}

int TimelineView::getTrackIndexAtY(int y) const
{
    if (timelinePtr == nullptr || y < RULER_HEIGHT)
        return -1;

    int trackY = RULER_HEIGHT;
    for (int i = 0; i < timelinePtr->getNumTracks(); ++i)
    {
        int totalHeight = getTotalTrackHeight(i);

        // Check if y is within the main track area (not automation lanes)
        if (y >= trackY && y < trackY + trackHeight)
            return i;

        trackY += totalHeight;
    }

    // If below all tracks, return last track index
    if (timelinePtr->getNumTracks() > 0)
        return timelinePtr->getNumTracks() - 1;

    return -1;
}

void TimelineView::importAudioFileToTrack(const juce::File& file, int trackIndex, int x)
{
    if (timelinePtr == nullptr)
        return;
    
    // Get or create target track
    Track* track = nullptr;
    if (trackIndex >= 0 && trackIndex < timelinePtr->getNumTracks())
    {
        track = timelinePtr->getTrack(trackIndex);
    }
    else
    {
        // Create new track if none exists
        auto* newTrack = new AudioTrack(file.getFileNameWithoutExtension());
        newTrack->setColour(juce::Colour::fromHSV(static_cast<float>(timelinePtr->getNumTracks()) * 0.15f, 0.6f, 0.8f, 1.0f));
        timelinePtr->addTrack(newTrack);
        track = newTrack;
        updateTrackHeaders();
    }
    
    // Must be an audio track
    auto* audioTrack = dynamic_cast<AudioTrack*>(track);
    if (audioTrack == nullptr)
        return;
    
    // Calculate start position from drop location
    int64_t startPosition = pixelToSample(x);
    if (startPosition < 0) startPosition = 0;
    
    // Load audio file and create region
    auto buffer = audioImporter.loadFile(file, timelinePtr->getSampleRate());
    if (buffer == nullptr)
        return;
    
    auto region = std::make_unique<AudioRegion>(startPosition, buffer->getNumSamples());
    region->setAudioBuffer(*buffer);
    region->setName(file.getFileNameWithoutExtension());
    region->setFilePath(file.getFullPathName());
    
    audioTrack->addRegion(std::move(region));
    repaint();
}

void TimelineView::importMidiFileToTrack(const juce::File& file, int trackIndex, int x)
{
    if (timelinePtr == nullptr) return;

    // Calculate start position
    int64_t startSample = pixelToSample(x - HEADER_WIDTH + static_cast<int>(horizontalScrollOffset));
    startSample = juce::jmax(int64_t(0), startSample);

    // Get or create track
    MidiTrack* track = nullptr;

    if (trackIndex >= 0 && trackIndex < timelinePtr->getNumTracks())
    {
        track = dynamic_cast<MidiTrack*>(timelinePtr->getTrack(trackIndex));
    }

    if (track == nullptr)
    {
        // Require MidiEngine
        if (midiEnginePtr == nullptr)
            return;

        auto newTrack = std::make_unique<MidiTrack>("MIDI Track", midiEnginePtr);
        track = newTrack.get();
        timelinePtr->addTrack(newTrack.release());
    }

    // Import MIDI
    int numTracks = midiImporter.getNumTracks(file);
    for (int i = 0; i < numTracks; ++i)
    {
        auto sequence = midiImporter.importTrack(file, i, sampleRate);
        if (sequence.getNumEvents() > 0)
        {
            // Calculate length
            double endTime = sequence.getEndTime();
            int64_t lengthVal = static_cast<int64_t>(endTime);

            auto region = std::make_unique<MidiRegion>(startSample, lengthVal);
            region->setMidiSequence(sequence);
            region->setName(file.getFileNameWithoutExtension());

            track->addRegion(std::move(region));
            break; // Just import first valid track for now
        }
    }

    updateTrackHeaders();
    repaint();
}

Region* TimelineView::getRegionAtPosition(int x, int y, int& outTrackIndex) const
{
    outTrackIndex = -1;

    if (timelinePtr == nullptr)
        return nullptr;

    // Check if position is in the timeline area (not ruler or headers)
    if (x <= HEADER_WIDTH || y <= RULER_HEIGHT)
        return nullptr;

    // Find which track the y position corresponds to
    int trackIndex = getTrackIndexAtY(y);
    if (trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return nullptr;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return nullptr;

    // Check audio tracks
    if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
    {
        for (int i = 0; i < audioTrack->getNumRegions(); ++i)
        {
            auto* region = audioTrack->getRegion(i);
            if (region == nullptr)
                continue;

            // Get region bounds and check if point is inside
            juce::Rectangle<int> regionBounds = getRegionBounds(region, trackIndex);
            if (regionBounds.contains(x, y))
            {
                outTrackIndex = trackIndex;
                return region;
            }
        }
    }
    // Check MIDI tracks
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
    {
        for (int i = 0; i < midiTrack->getNumRegions(); ++i)
        {
            auto* region = midiTrack->getRegion(i);
            if (region == nullptr)
                continue;

            // Get region bounds and check if point is inside
            juce::Rectangle<int> regionBounds = getRegionBounds(region, trackIndex);
            if (regionBounds.contains(x, y))
            {
                outTrackIndex = trackIndex;
                return region;
            }
        }
    }

    return nullptr;
}

juce::Rectangle<int> TimelineView::getRegionBounds(Region* region, int trackIndex) const
{
    if (region == nullptr || timelinePtr == nullptr)
        return juce::Rectangle<int>();

    int64_t startSample = region->getStartPosition();
    int64_t endSample = region->getEndPosition();

    // Calculate x coordinates
    int x1 = HEADER_WIDTH + sampleToPixel(startSample) - static_cast<int>(horizontalScrollOffset);
    int x2 = HEADER_WIDTH + sampleToPixel(endSample) - static_cast<int>(horizontalScrollOffset);
    int w = x2 - x1;

    // Calculate y coordinates (accounting for automation lanes)
    int trackY = RULER_HEIGHT;
    for (int i = 0; i < trackIndex; ++i)
    {
        trackY += getTotalTrackHeight(i);
    }

    return juce::Rectangle<int>(x1, trackY + 2, w, trackHeight - 4);
}

void TimelineView::clearSelection()
{
    if (selectedRegion != nullptr || selectedTrackIndex != -1)
    {
        selectedRegion = nullptr;
        selectedTrackIndex = -1;
        repaint();
    }
}

TimelineView::SnapResolution TimelineView::getZoomAwareSnapResolution() const
{
    // Determine snap resolution based on zoom level (pixelsPerBeat)
    // Higher zoom = finer snap resolution
    // pixelsPerBeat range is 5.0 to 100.0

    if (pixelsPerBeat >= 80.0)
    {
        // Very zoomed in - snap to 32nd notes (1/8 beat)
        return SnapResolution::EighthBeat;
    }
    else if (pixelsPerBeat >= 50.0)
    {
        // Zoomed in - snap to 16th notes (1/4 beat)
        return SnapResolution::QuarterBeat;
    }
    else if (pixelsPerBeat >= 25.0)
    {
        // Medium zoom - snap to 8th notes (1/2 beat)
        return SnapResolution::HalfBeat;
    }
    else if (pixelsPerBeat >= 12.0)
    {
        // Lower zoom - snap to whole beats
        return SnapResolution::Beat;
    }
    else
    {
        // Very zoomed out - snap to bars
        return SnapResolution::Bar;
    }
}

double TimelineView::getSnapDivisionForResolution(SnapResolution resolution) const
{
    // Returns the beat division for the given snap resolution
    // e.g., 1.0 = whole beat, 0.5 = half beat, 0.25 = quarter beat, etc.

    switch (resolution)
    {
    case SnapResolution::EighthBeat:
        return 0.125;   // 1/8 beat (32nd notes in 4/4)
    case SnapResolution::QuarterBeat:
        return 0.25;    // 1/4 beat (16th notes in 4/4)
    case SnapResolution::HalfBeat:
        return 0.5;     // 1/2 beat (8th notes in 4/4)
    case SnapResolution::Beat:
        return 1.0;     // Whole beat (quarter notes in 4/4)
    case SnapResolution::Bar:
    default:
        // Snap to bars - use beatsPerBar from timeline
        if (timelinePtr != nullptr)
            return static_cast<double>(timelinePtr->getBeatsPerBar());
        return 4.0;     // Default to 4 beats per bar
    }
}

int64_t TimelineView::snapPositionToGrid(int64_t samplePosition) const
{
    if (timelinePtr == nullptr)
        return samplePosition;

    // Get zoom-aware snap resolution
    SnapResolution resolution = getZoomAwareSnapResolution();
    double snapDivision = getSnapDivisionForResolution(resolution);

    // Convert sample position to beats
    double beats = timelinePtr->samplesToBeats(samplePosition);

    // Round to nearest snap division
    // Formula: round(beats / division) * division
    double snappedBeats = std::round(beats / snapDivision) * snapDivision;

    return timelinePtr->beatsToSamples(snappedBeats);
}

void TimelineView::drawDragGhost(juce::Graphics& g)
{
    if (!isDraggingRegion || selectedRegion == nullptr || selectedTrackIndex < 0)
        return;

    // Calculate ghost region bounds using the drag current position
    int64_t length = selectedRegion->getLength();
    int64_t endSample = dragCurrentPosition + length;

    int x1 = HEADER_WIDTH + sampleToPixel(dragCurrentPosition) - static_cast<int>(horizontalScrollOffset);
    int x2 = HEADER_WIDTH + sampleToPixel(endSample) - static_cast<int>(horizontalScrollOffset);
    int w = x2 - x1;

    int trackY = RULER_HEIGHT + selectedTrackIndex * trackHeight;
    juce::Rectangle<int> ghostBounds(x1, trackY + 2, w, trackHeight - 4);

    // Draw semi-transparent ghost of the region
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.3f));
    g.fillRect(ghostBounds);

    // Draw ghost border
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.8f));
    g.drawRect(ghostBounds, 2);

    // Draw snap indicator line at the start position
    g.setColour(MidiSingLookAndFeel::accentColour);
    g.drawLine(static_cast<float>(x1), static_cast<float>(RULER_HEIGHT),
               static_cast<float>(x1), static_cast<float>(getHeight()), 1.0f);
}

void TimelineView::drawResizeGhost(juce::Graphics& g)
{
    if (!isResizingRegion || selectedRegion == nullptr || selectedTrackIndex < 0)
        return;

    // Calculate ghost region bounds using the resize current values
    int64_t endSample = resizeCurrentStart + resizeCurrentLength;

    int x1 = HEADER_WIDTH + sampleToPixel(resizeCurrentStart) - static_cast<int>(horizontalScrollOffset);
    int x2 = HEADER_WIDTH + sampleToPixel(endSample) - static_cast<int>(horizontalScrollOffset);
    int w = x2 - x1;

    int trackY = RULER_HEIGHT + selectedTrackIndex * trackHeight;
    juce::Rectangle<int> ghostBounds(x1, trackY + 2, w, trackHeight - 4);

    // Draw semi-transparent ghost of the resized region
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.3f));
    g.fillRect(ghostBounds);

    // Draw ghost border
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.8f));
    g.drawRect(ghostBounds, 2);

    // Draw indicator line at the edge being resized
    g.setColour(MidiSingLookAndFeel::accentColour);
    if (resizeEdge == RegionEdge::Left)
    {
        g.drawLine(static_cast<float>(x1), static_cast<float>(RULER_HEIGHT),
                   static_cast<float>(x1), static_cast<float>(getHeight()), 2.0f);
    }
    else if (resizeEdge == RegionEdge::Right)
    {
        g.drawLine(static_cast<float>(x2), static_cast<float>(RULER_HEIGHT),
                   static_cast<float>(x2), static_cast<float>(getHeight()), 2.0f);
    }
}

void TimelineView::drawFadeGhost(juce::Graphics& g)
{
    if (!isEditingFade || selectedRegion == nullptr || selectedTrackIndex < 0)
        return;

    // Get region bounds
    juce::Rectangle<int> regionBounds = getRegionBounds(selectedRegion, selectedTrackIndex);

    // Calculate fade shape
    juce::Path fadePath;

    if (isFadeIn)
    {
        // Fade in: triangle from left edge going right
        int fadeEndX = regionBounds.getX() + sampleToPixel(fadeCurrentLength);

        fadePath.startNewSubPath(static_cast<float>(regionBounds.getX()),
                                  static_cast<float>(regionBounds.getBottom()));
        fadePath.lineTo(static_cast<float>(regionBounds.getX()),
                       static_cast<float>(regionBounds.getY()));
        fadePath.lineTo(static_cast<float>(fadeEndX),
                       static_cast<float>(regionBounds.getY()));
        fadePath.closeSubPath();
    }
    else
    {
        // Fade out: triangle from right edge going left
        int fadeStartX = regionBounds.getRight() - sampleToPixel(fadeCurrentLength);

        fadePath.startNewSubPath(static_cast<float>(regionBounds.getRight()),
                                  static_cast<float>(regionBounds.getY()));
        fadePath.lineTo(static_cast<float>(regionBounds.getRight()),
                       static_cast<float>(regionBounds.getBottom()));
        fadePath.lineTo(static_cast<float>(fadeStartX),
                       static_cast<float>(regionBounds.getY()));
        fadePath.closeSubPath();
    }

    // Draw the fade shape
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.4f));
    g.fillPath(fadePath);

    // Draw fade boundary line
    g.setColour(MidiSingLookAndFeel::accentColour);
    if (isFadeIn)
    {
        int fadeEndX = regionBounds.getX() + sampleToPixel(fadeCurrentLength);
        g.drawLine(static_cast<float>(regionBounds.getX()), static_cast<float>(regionBounds.getBottom()),
                   static_cast<float>(fadeEndX), static_cast<float>(regionBounds.getY()), 2.0f);
    }
    else
    {
        int fadeStartX = regionBounds.getRight() - sampleToPixel(fadeCurrentLength);
        g.drawLine(static_cast<float>(fadeStartX), static_cast<float>(regionBounds.getY()),
                   static_cast<float>(regionBounds.getRight()), static_cast<float>(regionBounds.getBottom()), 2.0f);
    }
}

void TimelineView::drawDrawGhost(juce::Graphics& g)
{
    if (!isDrawingRegion || drawTrackIndex < 0)
        return;

    // Use the pre-calculated ghost preview bounds with slight padding
    juce::Rectangle<int> ghostBounds = ghostPreviewBounds.reduced(0, 2);

    // Ensure minimum width for visibility
    if (ghostBounds.getWidth() < 2)
        ghostBounds.setWidth(2);

    // Draw semi-transparent fill for ghost preview
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.3f));
    g.fillRect(ghostBounds);

    // Draw ghost border
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.8f));
    g.drawRect(ghostBounds, 2);

    // Draw snap indicator line at the start position
    g.setColour(MidiSingLookAndFeel::accentColour);
    g.drawLine(static_cast<float>(ghostBounds.getX()), static_cast<float>(RULER_HEIGHT),
               static_cast<float>(ghostBounds.getX()), static_cast<float>(getHeight()), 1.0f);
}

void TimelineView::drawStretchGhost(juce::Graphics& g)
{
    if (!isStretchingRegion || selectedRegion == nullptr || selectedTrackIndex < 0)
        return;

    // Calculate ghost region bounds using the stretch current values
    int64_t regionStart = selectedRegion->getStartPosition();
    int64_t regionEnd = regionStart + stretchCurrentLength;

    // For left edge stretch, the start position may need adjustment
    if (resizeEdge == RegionEdge::Left)
    {
        // When stretching from left, the region end stays fixed
        int64_t originalEnd = selectedRegion->getStartPosition() + selectedRegion->getLength();
        regionEnd = originalEnd;
        regionStart = originalEnd - stretchCurrentLength;
    }

    int x1 = HEADER_WIDTH + sampleToPixel(regionStart) - static_cast<int>(horizontalScrollOffset);
    int x2 = HEADER_WIDTH + sampleToPixel(regionEnd) - static_cast<int>(horizontalScrollOffset);
    int w = x2 - x1;

    // Calculate track Y position accounting for automation lanes
    int trackY = RULER_HEIGHT;
    for (int i = 0; i < selectedTrackIndex; ++i)
    {
        trackY += getTotalTrackHeight(i);
    }

    juce::Rectangle<int> ghostBounds(x1, trackY + 2, w, trackHeight - 4);

    // Draw semi-transparent ghost with a distinct color for stretching (orange tint)
    juce::Colour stretchColour = juce::Colour(255, 165, 0);  // Orange for stretch
    g.setColour(stretchColour.withAlpha(0.3f));
    g.fillRect(ghostBounds);

    // Draw ghost border
    g.setColour(stretchColour.withAlpha(0.8f));
    g.drawRect(ghostBounds, 2);

    // Draw indicator line at the edge being stretched
    g.setColour(stretchColour);
    if (resizeEdge == RegionEdge::Left)
    {
        g.drawLine(static_cast<float>(x1), static_cast<float>(RULER_HEIGHT),
                   static_cast<float>(x1), static_cast<float>(getHeight()), 2.0f);
    }
    else if (resizeEdge == RegionEdge::Right)
    {
        g.drawLine(static_cast<float>(x2), static_cast<float>(RULER_HEIGHT),
                   static_cast<float>(x2), static_cast<float>(getHeight()), 2.0f);
    }

    // Draw stretch ratio indicator text
    juce::String ratioText = juce::String(stretchCurrentRatio, 2) + "x";
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    int textX = (resizeEdge == RegionEdge::Right) ? x2 + 5 : x1 - 50;
    g.drawText(ratioText, textX, trackY + trackHeight / 2 - 10, 45, 20, juce::Justification::centred);
}

void TimelineView::handleDoubleClickOnRegion(Region* region, int trackIndex)
{
    if (region == nullptr || timelinePtr == nullptr)
        return;

    // Select the region first
    selectedRegion = region;
    selectedTrackIndex = trackIndex;

    // Check if it's a MIDI region - if so, open Piano Roll
    Track* track = timelinePtr->getTrack(trackIndex);
    if (track != nullptr)
    {
        if (dynamic_cast<MidiTrack*>(track) != nullptr)
        {
            // MIDI region double-click: open Piano Roll with this region
            auto* midiRegion = dynamic_cast<MidiRegion*>(region);
            if (midiRegion != nullptr && onMidiRegionDoubleClicked)
            {
                onMidiRegionDoubleClicked(midiRegion);
            }
        }
        else if (dynamic_cast<AudioTrack*>(track) != nullptr)
        {
            // Audio region double-click: could open waveform editor in the future
            // For now, just select the region
        }
    }

    repaint();
}

void TimelineView::handleDoubleClickOnEmptyArea(int trackIndex, int clickX)
{
    if (timelinePtr == nullptr)
        return;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return;

    // Calculate position in samples and snap to grid
    int64_t clickSample = pixelToSample(clickX);
    if (snapToGrid)
    {
        clickSample = snapPositionToGrid(clickSample);
    }

    // Create a 4-bar region (16 beats in 4/4 time)
    const double bpm = transportPtr != nullptr ? transportPtr->getBPM() : 120.0;
    const double beatsPerBar = 4.0;
    const double numBars = 4.0;
    const double totalBeats = beatsPerBar * numBars;  // 16 beats for 4 bars
    const int64_t regionLength = static_cast<int64_t>((60.0 / bpm) * totalBeats * sampleRate);

    // Create the appropriate region type based on track type
    if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
    {
        // Create empty AudioRegion on Audio track
        auto newRegion = std::make_unique<AudioRegion>(clickSample, regionLength);
        newRegion->setName("New Audio Region");

        // Initialize empty audio buffer (silent)
        juce::AudioBuffer<float> emptyBuffer(2, static_cast<int>(regionLength));
        emptyBuffer.clear();
        newRegion->setAudioBuffer(emptyBuffer);

        selectedRegion = newRegion.get();
        selectedTrackIndex = trackIndex;
        audioTrack->addRegion(std::move(newRegion));
    }
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
    {
        // Create empty MidiRegion on MIDI track
        auto newRegion = std::make_unique<MidiRegion>(clickSample, regionLength);
        newRegion->setName("New MIDI Region");

        selectedRegion = newRegion.get();
        selectedTrackIndex = trackIndex;
        midiTrack->addRegion(std::move(newRegion));
    }

    repaint();
}

TimelineView::RegionEdge TimelineView::getRegionEdgeAtPosition(int x, int y, Region*& outRegion, int& outTrackIndex) const
{
    outRegion = nullptr;
    outTrackIndex = -1;

    if (timelinePtr == nullptr)
        return RegionEdge::None;

    // Check if position is in the timeline area (not ruler or headers)
    if (x <= HEADER_WIDTH || y <= RULER_HEIGHT)
        return RegionEdge::None;

    // Find which track the y position corresponds to
    int trackIndex = getTrackIndexAtY(y);
    if (trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return RegionEdge::None;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return RegionEdge::None;

    // Lambda to check region edges
    auto checkRegionEdge = [&](Region* region) -> RegionEdge
    {
        if (region == nullptr)
            return RegionEdge::None;

        juce::Rectangle<int> regionBounds = getRegionBounds(region, trackIndex);

        // Check if y is within the region's vertical bounds
        if (y < regionBounds.getY() || y > regionBounds.getBottom())
            return RegionEdge::None;

        // Check left edge
        if (x >= regionBounds.getX() - EDGE_DETECT_WIDTH / 2 &&
            x <= regionBounds.getX() + EDGE_DETECT_WIDTH / 2)
        {
            outRegion = region;
            outTrackIndex = trackIndex;
            return RegionEdge::Left;
        }

        // Check right edge
        if (x >= regionBounds.getRight() - EDGE_DETECT_WIDTH / 2 &&
            x <= regionBounds.getRight() + EDGE_DETECT_WIDTH / 2)
        {
            outRegion = region;
            outTrackIndex = trackIndex;
            return RegionEdge::Right;
        }

        return RegionEdge::None;
    };

    // Check audio tracks
    if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
    {
        for (int i = 0; i < audioTrack->getNumRegions(); ++i)
        {
            RegionEdge edge = checkRegionEdge(audioTrack->getRegion(i));
            if (edge != RegionEdge::None)
                return edge;
        }
    }
    // Check MIDI tracks
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
    {
        for (int i = 0; i < midiTrack->getNumRegions(); ++i)
        {
            RegionEdge edge = checkRegionEdge(midiTrack->getRegion(i));
            if (edge != RegionEdge::None)
                return edge;
        }
    }

    return RegionEdge::None;
}

TimelineView::SmartToolZone TimelineView::getSmartToolZone(int x, int y, Region*& outRegion, int& outTrackIndex) const
{
    outRegion = nullptr;
    outTrackIndex = -1;

    if (timelinePtr == nullptr)
        return SmartToolZone::None;

    // Check if position is in the timeline area (not ruler or headers)
    if (x <= HEADER_WIDTH || y <= RULER_HEIGHT)
        return SmartToolZone::None;

    // Find which track the y position corresponds to
    int trackIndex = getTrackIndexAtY(y);
    if (trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return SmartToolZone::None;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return SmartToolZone::None;

    // Lambda to check smart tool zone for a region
    auto checkRegionZone = [&](Region* region) -> SmartToolZone
    {
        if (region == nullptr)
            return SmartToolZone::None;

        juce::Rectangle<int> regionBounds = getRegionBounds(region, trackIndex);

        // Check if y is within the region's vertical bounds
        if (y < regionBounds.getY() || y > regionBounds.getBottom())
            return SmartToolZone::None;

        // Check if x is within the region's horizontal bounds (with some tolerance)
        if (x < regionBounds.getX() - EDGE_DETECT_WIDTH / 2 ||
            x > regionBounds.getRight() + EDGE_DETECT_WIDTH / 2)
            return SmartToolZone::None;

        outRegion = region;
        outTrackIndex = trackIndex;

        // Check fade zones first (corners take priority)
        bool inTopHalf = y < regionBounds.getY() + FADE_ZONE_HEIGHT;

        // Fade In zone: top-left corner
        if (inTopHalf && x >= regionBounds.getX() && x <= regionBounds.getX() + FADE_ZONE_WIDTH)
        {
            return SmartToolZone::FadeIn;
        }

        // Fade Out zone: top-right corner
        if (inTopHalf && x >= regionBounds.getRight() - FADE_ZONE_WIDTH && x <= regionBounds.getRight())
        {
            return SmartToolZone::FadeOut;
        }

        // Check trim edges (excluding fade zones)
        // Left trim edge
        if (x >= regionBounds.getX() - EDGE_DETECT_WIDTH / 2 &&
            x <= regionBounds.getX() + EDGE_DETECT_WIDTH / 2)
        {
            return SmartToolZone::TrimLeft;
        }

        // Right trim edge
        if (x >= regionBounds.getRight() - EDGE_DETECT_WIDTH / 2 &&
            x <= regionBounds.getRight() + EDGE_DETECT_WIDTH / 2)
        {
            return SmartToolZone::TrimRight;
        }

        // Center of region - move operation
        if (x > regionBounds.getX() + EDGE_DETECT_WIDTH / 2 &&
            x < regionBounds.getRight() - EDGE_DETECT_WIDTH / 2)
        {
            return SmartToolZone::Move;
        }

        return SmartToolZone::None;
    };

    // Check audio tracks
    if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
    {
        for (int i = 0; i < audioTrack->getNumRegions(); ++i)
        {
            SmartToolZone zone = checkRegionZone(audioTrack->getRegion(i));
            if (zone != SmartToolZone::None)
                return zone;
        }
    }
    // Check MIDI tracks
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
    {
        for (int i = 0; i < midiTrack->getNumRegions(); ++i)
        {
            SmartToolZone zone = checkRegionZone(midiTrack->getRegion(i));
            if (zone != SmartToolZone::None)
                return zone;
        }
    }

    return SmartToolZone::None;
}

void TimelineView::updateCursorForPosition(int x, int y)
{
    // Use Smart Tool to determine zone and set appropriate cursor
    Region* region = nullptr;
    int trackIndex = -1;
    SmartToolZone zone = getSmartToolZone(x, y, region, trackIndex);

    // Update current zone for potential status display
    currentSmartToolZone = zone;

    // In Select mode, use smart tool cursor logic based on zone
    // In other modes, use the tool-specific cursor
    if (currentToolMode == ToolMode::Select)
    {
        switch (zone)
        {
        case SmartToolZone::TrimLeft:
        case SmartToolZone::TrimRight:
            // Check if Alt is held - show different cursor for stretch mode
            if (juce::ModifierKeys::getCurrentModifiers().isAltDown())
            {
                // Use crosshair cursor for stretch operations (indicates Alt modifier active)
                // Only show stretch cursor for audio regions
                if (region != nullptr && dynamic_cast<AudioRegion*>(region) != nullptr)
                {
                    setMouseCursor(juce::MouseCursor::CrosshairCursor);
                }
                else
                {
                    // MIDI regions don't support stretching
                    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                }
            }
            else
            {
                // Horizontal resize cursor for trim operations
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            }
            break;

        case SmartToolZone::Move:
            // Pointing hand or drag cursor for move operations
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            break;

        case SmartToolZone::FadeIn:
        case SmartToolZone::FadeOut:
            // Use crosshair cursor for fade operations (diagonal resize would be ideal)
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            break;

        case SmartToolZone::None:
        default:
            // Normal cursor when not over a region
            setMouseCursor(juce::MouseCursor::NormalCursor);
            break;
        }
    }
    else
    {
        // Use the cursor associated with the current tool mode
        setMouseCursor(Cursors::getCursorForTool(currentToolMode));
    }
}

void TimelineView::showContextMenu(const juce::MouseEvent& e)
{
    juce::PopupMenu menu;

    // Menu item IDs
    enum MenuIDs
    {
        CutRegion = 1,
        CopyRegion,
        PasteRegion,
        DeleteRegion,
        SplitRegion,
        AddAudioTrack,
        AddMidiTrack,
        ImportAudio,
        ImportMidi,
        DeleteTrack,
        DuplicateTrack
    };

    bool hasSelection = (selectedRegion != nullptr && selectedTrackIndex >= 0);
    bool canPaste = clipboard.hasData;

    // Determine the track index at the click position
    int clickedTrackIndex = getTrackIndexAtY(e.y);
    contextMenuTrackIndex = clickedTrackIndex;
    bool hasTrackAtPosition = (clickedTrackIndex >= 0 && timelinePtr != nullptr &&
                               clickedTrackIndex < timelinePtr->getNumTracks());

    // Region operations section
    menu.addItem(CutRegion, "Cut", hasSelection);
    menu.addItem(CopyRegion, "Copy", hasSelection);
    menu.addItem(PasteRegion, "Paste", canPaste);
    menu.addSeparator();
    menu.addItem(DeleteRegion, "Delete Region", hasSelection);
    menu.addItem(SplitRegion, "Split Region", hasSelection);

    // Track operations section
    menu.addSeparator();
    menu.addSectionHeader("Track");
    menu.addItem(AddAudioTrack, "Add Audio Track");
    menu.addItem(AddMidiTrack, "Add MIDI Track");
    menu.addSeparator();
    menu.addItem(DeleteTrack, "Delete Track", hasTrackAtPosition);
    menu.addItem(DuplicateTrack, "Duplicate Track", hasTrackAtPosition);

    // Import operations section
    menu.addSeparator();
    menu.addSectionHeader("Import");
    menu.addItem(ImportAudio, "Import Audio...");
    menu.addItem(ImportMidi, "Import MIDI...");

    // Show menu and handle selection - use screen coordinates for proper positioning
    auto screenPos = juce::Point<int>(e.getScreenX(), e.getScreenY());
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPos, screenPos)),
        [this, clickX = e.x](int result)
        {
            switch (result)
            {
            case CutRegion:
                cutSelectedRegion();
                break;
            case CopyRegion:
                copySelectedRegion();
                break;
            case PasteRegion:
                pasteRegion(lastClickSamplePosition);
                break;
            case DeleteRegion:
                deleteSelectedRegion();
                break;
            case SplitRegion:
                splitSelectedRegion(lastClickSamplePosition);
                break;
            case AddAudioTrack:
                addAudioTrack();
                break;
            case AddMidiTrack:
                addMidiTrack();
                break;
            case ImportAudio:
                showImportAudioDialog();
                break;
            case ImportMidi:
                showImportMidiDialog();
                break;
            case DeleteTrack:
                deleteTrack(contextMenuTrackIndex);
                break;
            case DuplicateTrack:
                duplicateTrack(contextMenuTrackIndex);
                break;
            default:
                break;
            }
        });
}

void TimelineView::cutSelectedRegion()
{
    if (selectedRegion == nullptr || selectedTrackIndex < 0)
        return;

    // Copy first, then delete
    copySelectedRegion();
    deleteSelectedRegion();
}

void TimelineView::copySelectedRegion()
{
    if (selectedRegion == nullptr || selectedTrackIndex < 0 || timelinePtr == nullptr)
        return;

    // Store region data in clipboard
    clipboard.regionName = selectedRegion->getName();
    clipboard.regionLength = selectedRegion->getLength();
    clipboard.regionOffset = selectedRegion->getOffset();

    // Check if it's an audio region or MIDI region
    if (auto* audioRegion = dynamic_cast<AudioRegion*>(selectedRegion))
    {
        clipboard.isAudioRegion = true;
        clipboard.audioBuffer.makeCopyOf(audioRegion->getAudioBuffer());
        clipboard.filePath = audioRegion->getFilePath();
        clipboard.thumbnailHash = audioRegion->getThumbnailHash();
        clipboard.hasData = true;
    }
    else if (auto* midiRegion = dynamic_cast<MidiRegion*>(selectedRegion))
    {
        clipboard.isAudioRegion = false;
        clipboard.midiSequence = midiRegion->getMidiSequence();
        clipboard.hasData = true;
    }
}

void TimelineView::pasteRegion(int64_t pastePosition)
{
    if (!clipboard.hasData || timelinePtr == nullptr || selectedTrackIndex < 0)
        return;

    // Snap paste position to grid if enabled
    if (snapToGrid)
    {
        pastePosition = snapPositionToGrid(pastePosition);
    }

    // Get the target track
    Track* track = timelinePtr->getTrack(selectedTrackIndex);
    if (track == nullptr)
        return;

    if (clipboard.isAudioRegion)
    {
        // Paste audio region
        auto* audioTrack = dynamic_cast<AudioTrack*>(track);
        if (audioTrack == nullptr)
            return;

        auto newRegion = std::make_unique<AudioRegion>(pastePosition, clipboard.regionLength);
        newRegion->setAudioBuffer(clipboard.audioBuffer);
        newRegion->setName(clipboard.regionName + " (copy)");
        newRegion->setOffset(clipboard.regionOffset);
        if (clipboard.filePath.isNotEmpty())
            newRegion->setFilePath(clipboard.filePath);

        if (undoManagerPtr != nullptr)
        {
            undoManagerPtr->perform(new AddRegionAction(*audioTrack, std::move(newRegion)),
                                   "Paste Region");
        }
        else
        {
            audioTrack->addRegion(std::move(newRegion));
        }
    }
    else
    {
        // Paste MIDI region - no undo actions for MIDI regions yet
        auto* midiTrack = dynamic_cast<MidiTrack*>(track);
        if (midiTrack == nullptr)
            return;

        auto newRegion = std::make_unique<MidiRegion>(pastePosition, clipboard.regionLength);
        newRegion->setMidiSequence(clipboard.midiSequence);
        newRegion->setName(clipboard.regionName + " (copy)");
        newRegion->setOffset(clipboard.regionOffset);

        midiTrack->addRegion(std::move(newRegion));
    }

    repaint();
}

void TimelineView::deleteSelectedRegion()
{
    if (selectedRegion == nullptr || selectedTrackIndex < 0 || timelinePtr == nullptr)
        return;

    Track* track = timelinePtr->getTrack(selectedTrackIndex);
    if (track == nullptr)
        return;

    // Find and remove the region from its track
    if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
    {
        for (int i = 0; i < audioTrack->getNumRegions(); ++i)
        {
            if (audioTrack->getRegion(i) == selectedRegion)
            {
                if (undoManagerPtr != nullptr)
                {
                    undoManagerPtr->perform(new DeleteRegionAction(*audioTrack, i),
                                           "Delete Region");
                }
                else
                {
                    audioTrack->removeRegion(i);
                }
                break;
            }
        }
    }
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
    {
        // MIDI regions don't have undo actions yet, delete directly
        for (int i = 0; i < midiTrack->getNumRegions(); ++i)
        {
            if (midiTrack->getRegion(i) == selectedRegion)
            {
                midiTrack->removeRegion(i);
                break;
            }
        }
    }

    // Clear selection
    selectedRegion = nullptr;
    selectedTrackIndex = -1;
    repaint();
}

void TimelineView::splitSelectedRegion(int64_t splitPosition)
{
    if (selectedRegion == nullptr || selectedTrackIndex < 0 || timelinePtr == nullptr)
        return;

    // Ensure split position is within the region
    int64_t regionStart = selectedRegion->getStartPosition();
    int64_t regionEnd = selectedRegion->getEndPosition();

    if (splitPosition <= regionStart || splitPosition >= regionEnd)
        return;

    // Snap split position to grid if enabled
    if (snapToGrid)
    {
        splitPosition = snapPositionToGrid(splitPosition);
        // Re-check bounds after snapping
        if (splitPosition <= regionStart || splitPosition >= regionEnd)
            return;
    }

    Track* track = timelinePtr->getTrack(selectedTrackIndex);
    if (track == nullptr)
        return;

    // Calculate lengths for the two parts
    int64_t firstPartLength = splitPosition - regionStart;
    int64_t secondPartLength = regionEnd - splitPosition;
    int64_t originalOffset = selectedRegion->getOffset();

    if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
    {
        auto* audioRegion = dynamic_cast<AudioRegion*>(selectedRegion);
        if (audioRegion == nullptr)
            return;

        if (undoManagerPtr != nullptr)
        {
            // Use SplitRegionAction for undoable split
            undoManagerPtr->perform(new SplitRegionAction(*audioTrack, audioRegion, splitPosition),
                                   "Split Region");
        }
        else
        {
            // Fallback: direct modification
            // Create second part (new region after split point)
            auto secondRegion = std::make_unique<AudioRegion>(splitPosition, secondPartLength);
            secondRegion->setAudioBuffer(audioRegion->getAudioBuffer());
            secondRegion->setName(audioRegion->getName() + " (split)");
            secondRegion->setOffset(originalOffset + firstPartLength);
            if (audioRegion->getFilePath().isNotEmpty())
                secondRegion->setFilePath(audioRegion->getFilePath());

            // Modify the original region (first part)
            audioRegion->setLength(firstPartLength);

            // Add the second region
            audioTrack->addRegion(std::move(secondRegion));
        }
    }
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
    {
        auto* midiRegion = dynamic_cast<MidiRegion*>(selectedRegion);
        if (midiRegion == nullptr)
            return;

        // MIDI regions don't have undo actions yet, split directly
        // For MIDI, we need to split the sequence
        const auto& originalSequence = midiRegion->getMidiSequence();
        juce::MidiMessageSequence firstPartSequence;
        juce::MidiMessageSequence secondPartSequence;

        // Split MIDI events between the two sequences
        for (int i = 0; i < originalSequence.getNumEvents(); ++i)
        {
            auto* event = originalSequence.getEventPointer(i);
            double eventTime = event->message.getTimeStamp();

            // Events are relative to region start, so compare with firstPartLength
            if (eventTime < static_cast<double>(firstPartLength))
            {
                firstPartSequence.addEvent(event->message);
            }
            else
            {
                // Adjust timestamp for second part (relative to new region start)
                auto newMessage = event->message;
                newMessage.setTimeStamp(eventTime - static_cast<double>(firstPartLength));
                secondPartSequence.addEvent(newMessage);
            }
        }

        // Create second part
        auto secondRegion = std::make_unique<MidiRegion>(splitPosition, secondPartLength);
        secondRegion->setMidiSequence(secondPartSequence);
        secondRegion->setName(midiRegion->getName() + " (split)");
        secondRegion->setOffset(0);

        // Modify the original region
        midiRegion->setLength(firstPartLength);
        midiRegion->setMidiSequence(firstPartSequence);

        // Add the second region
        midiTrack->addRegion(std::move(secondRegion));
    }

    repaint();
}

// ============================================================================
// Public Region Editing Operations (for menu/shortcut access)
// ============================================================================

void TimelineView::deleteSelection()
{
    deleteSelectedRegion();
}

void TimelineView::splitSelectionAtPosition(int64_t samplePosition)
{
    splitSelectedRegion(samplePosition);
}

void TimelineView::selectFirstRegion()
{
    if (timelinePtr == nullptr)
        return;

    // Iterate through all tracks to find the first region
    for (int trackIndex = 0; trackIndex < timelinePtr->getNumTracks(); ++trackIndex)
    {
        Track* track = timelinePtr->getTrack(trackIndex);
        if (track == nullptr)
            continue;

        // Check for Audio regions
        if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
        {
            if (audioTrack->getNumRegions() > 0)
            {
                selectedRegion = audioTrack->getRegion(0);
                selectedTrackIndex = trackIndex;
                repaint();
                return;
            }
        }
        // Check for MIDI regions
        else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
        {
            if (midiTrack->getNumRegions() > 0)
            {
                selectedRegion = midiTrack->getRegion(0);
                selectedTrackIndex = trackIndex;
                repaint();
                return;
            }
        }
    }

    // No regions found, clear selection
    clearSelection();
}

// ============================================================================
// Track Management Operations (Context Menu)
// ============================================================================

void TimelineView::addAudioTrack()
{
    if (timelinePtr == nullptr)
        return;

    int trackNum = timelinePtr->getNumTracks() + 1;
    auto* newTrack = new AudioTrack("Audio " + juce::String(trackNum));
    newTrack->setColour(juce::Colour::fromHSV(juce::Random::getSystemRandom().nextFloat(), 0.6f, 0.8f, 1.0f));
    timelinePtr->addTrack(newTrack);

    updateTrackHeaders();
    resized();
    repaint();
}

void TimelineView::addMidiTrack()
{
    if (timelinePtr == nullptr || midiEnginePtr == nullptr)
        return;

    int trackNum = timelinePtr->getNumTracks() + 1;
    auto* newTrack = new MidiTrack("MIDI " + juce::String(trackNum), midiEnginePtr);
    newTrack->setColour(juce::Colour::fromHSV(juce::Random::getSystemRandom().nextFloat(), 0.6f, 0.8f, 1.0f));
    timelinePtr->addTrack(newTrack);

    updateTrackHeaders();
    resized();
    repaint();
}

void TimelineView::deleteTrack(int trackIndex)
{
    if (timelinePtr == nullptr || trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return;

    // Clear selection if we're deleting the selected track
    if (selectedTrackIndex == trackIndex)
    {
        selectedRegion = nullptr;
        selectedTrackIndex = -1;
    }
    // Adjust selected track index if deleting a track before it
    else if (selectedTrackIndex > trackIndex)
    {
        selectedTrackIndex--;
    }

    timelinePtr->removeTrack(trackIndex);

    updateTrackHeaders();
    resized();
    repaint();
}

void TimelineView::duplicateTrack(int trackIndex)
{
    if (timelinePtr == nullptr || trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return;

    Track* sourceTrack = timelinePtr->getTrack(trackIndex);
    if (sourceTrack == nullptr)
        return;

    // Duplicate based on track type
    if (auto* audioSource = dynamic_cast<AudioTrack*>(sourceTrack))
    {
        auto* newTrack = new AudioTrack(audioSource->getName() + " (copy)");
        newTrack->setColour(audioSource->getColour());
        newTrack->setVolume(audioSource->getVolume());
        newTrack->setPan(audioSource->getPan());
        newTrack->setMuted(audioSource->isMuted());
        newTrack->setSoloed(audioSource->isSoloed());

        // Copy all regions
        for (int i = 0; i < audioSource->getNumRegions(); ++i)
        {
            Region* region = audioSource->getRegion(i);
            if (auto* audioRegion = dynamic_cast<AudioRegion*>(region))
            {
                auto newRegion = std::make_unique<AudioRegion>(
                    audioRegion->getStartPosition(),
                    audioRegion->getLength());
                newRegion->setName(audioRegion->getName());
                newRegion->setOffset(audioRegion->getOffset());
                newRegion->setAudioBuffer(audioRegion->getAudioBuffer());
                newRegion->setFilePath(audioRegion->getFilePath());
                newRegion->setFadeInLength(audioRegion->getFadeInLength());
                newRegion->setFadeOutLength(audioRegion->getFadeOutLength());
                newTrack->addRegion(std::move(newRegion));
            }
        }

        timelinePtr->addTrack(newTrack);
    }
    else if (auto* midiSource = dynamic_cast<MidiTrack*>(sourceTrack))
    {
        if (midiEnginePtr == nullptr)
            return;

        auto* newTrack = new MidiTrack(midiSource->getName() + " (copy)", midiEnginePtr);
        newTrack->setColour(midiSource->getColour());
        newTrack->setVolume(midiSource->getVolume());
        newTrack->setPan(midiSource->getPan());
        newTrack->setMuted(midiSource->isMuted());
        newTrack->setSoloed(midiSource->isSoloed());

        // Copy all regions
        for (int i = 0; i < midiSource->getNumRegions(); ++i)
        {
            Region* region = midiSource->getRegion(i);
            if (auto* midiRegion = dynamic_cast<MidiRegion*>(region))
            {
                auto newRegion = std::make_unique<MidiRegion>(
                    midiRegion->getStartPosition(),
                    midiRegion->getLength());
                newRegion->setName(midiRegion->getName());
                newRegion->setOffset(midiRegion->getOffset());
                newRegion->setMidiSequence(midiRegion->getMidiSequence());
                newTrack->addRegion(std::move(newRegion));
            }
        }

        timelinePtr->addTrack(newTrack);
    }

    updateTrackHeaders();
    resized();
    repaint();
}

void TimelineView::showImportAudioDialog()
{
    if (timelinePtr == nullptr)
        return;

    fileChooser = std::make_unique<juce::FileChooser>(
        "Import Audio",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.wav;*.mp3;*.aif;*.aiff;*.flac;*.ogg");

    auto chooserFlags = juce::FileBrowserComponent::openMode |
                        juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File() && audioImporter.isSupported(file))
        {
            // Create a new audio track for the imported file
            addAudioTrack();

            // Import to the newly created track at position 0
            int newTrackIndex = timelinePtr->getNumTracks() - 1;
            importAudioFileToTrack(file, newTrackIndex, HEADER_WIDTH);
        }
    });
}

void TimelineView::showImportMidiDialog()
{
    if (timelinePtr == nullptr)
        return;

    fileChooser = std::make_unique<juce::FileChooser>(
        "Import MIDI",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.mid;*.midi");

    auto chooserFlags = juce::FileBrowserComponent::openMode |
                        juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File() && midiImporter.isSupported(file))
        {
            // Create a new MIDI track for the imported file
            addMidiTrack();

            // Import to the newly created track at position 0
            int newTrackIndex = timelinePtr->getNumTracks() - 1;
            importMidiFileToTrack(file, newTrackIndex, HEADER_WIDTH);
        }
    });
}

// ============================================================================
// Automation Lane UI Implementation
// ============================================================================

void TimelineView::setAutomationMode(AutomationMode mode)
{
    if (automationMode != mode)
    {
        automationMode = mode;

        // Stop recording if switching away from Write mode
        if (mode != AutomationMode::Write && mode != AutomationMode::Touch && mode != AutomationMode::Latch)
        {
            isRecordingAutomation = false;
        }

        if (onAutomationModeChanged)
            onAutomationModeChanged(mode);

        repaint();
    }
}

void TimelineView::setToolMode(ToolMode mode)
{
    if (currentToolMode != mode)
    {
        currentToolMode = mode;

        // Update cursor to reflect new tool mode
        setMouseCursor(Cursors::getCursorForTool(mode));
        repaint();
    }
}

void TimelineView::setAutomationLaneVisible(int trackIndex, const juce::String& paramName, bool visible)
{
    if (timelinePtr == nullptr || trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return;

    // Find or create the automation lane
    AutomationLane* lane = track->findAutomationLane(paramName);

    if (lane == nullptr && visible)
    {
        // Create the lane if it doesn't exist and we want to show it
        lane = track->addAutomationLane(paramName);

        // Set appropriate value range for Volume and Pan
        if (paramName.equalsIgnoreCase("Volume"))
        {
            lane->setValueRange(0.0f, 2.0f);
            lane->setDefaultValue(1.0f);
        }
        else if (paramName.equalsIgnoreCase("Pan"))
        {
            lane->setValueRange(-1.0f, 1.0f);
            lane->setDefaultValue(0.0f);
        }
    }

    if (lane != nullptr)
    {
        lane->setVisible(visible);
        resized();  // Recalculate layout for track headers
        repaint();
    }
}

bool TimelineView::isAutomationLaneVisible(int trackIndex, const juce::String& paramName) const
{
    if (timelinePtr == nullptr || trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return false;

    const Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return false;

    const AutomationLane* lane = track->findAutomationLane(paramName);
    return lane != nullptr && lane->isVisible();
}

void TimelineView::toggleAutomationLane(int trackIndex, const juce::String& paramName)
{
    bool currentlyVisible = isAutomationLaneVisible(trackIndex, paramName);
    setAutomationLaneVisible(trackIndex, paramName, !currentlyVisible);
}

void TimelineView::addAutomationPoint(int trackIndex, const juce::String& paramName, int64_t position, float value)
{
    if (timelinePtr == nullptr || trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return;

    AutomationLane* lane = track->findAutomationLane(paramName);
    if (lane == nullptr)
    {
        // Create lane if it doesn't exist
        lane = track->addAutomationLane(paramName);
        if (paramName.equalsIgnoreCase("Volume"))
        {
            lane->setValueRange(0.0f, 2.0f);
            lane->setDefaultValue(1.0f);
        }
        else if (paramName.equalsIgnoreCase("Pan"))
        {
            lane->setValueRange(-1.0f, 1.0f);
            lane->setDefaultValue(0.0f);
        }
    }

    if (lane != nullptr)
    {
        lane->addPoint(position, value);
        repaint();
    }
}

void TimelineView::deleteAutomationPointsInRange(int trackIndex, const juce::String& paramName, int64_t start, int64_t end)
{
    if (timelinePtr == nullptr || trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return;

    AutomationLane* lane = track->findAutomationLane(paramName);
    if (lane != nullptr)
    {
        lane->removePointsInRange(start, end);
        repaint();
    }
}

int TimelineView::getNumVisibleAutomationLanes(int trackIndex) const
{
    if (timelinePtr == nullptr || trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return 0;

    const Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return 0;

    int count = 0;
    for (size_t i = 0; i < track->getNumAutomationLanes(); ++i)
    {
        const AutomationLane* lane = track->getAutomationLane(i);
        if (lane != nullptr && lane->isVisible())
            ++count;
    }
    return count;
}

int TimelineView::getTotalTrackHeight(int trackIndex) const
{
    int baseHeight = trackHeight;
    int numLanes = getNumVisibleAutomationLanes(trackIndex);
    return baseHeight + (numLanes * AUTOMATION_LANE_HEIGHT);
}

AutomationLane* TimelineView::getAutomationLaneForTrack(int trackIndex, int laneIndex) const
{
    if (timelinePtr == nullptr || trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return nullptr;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return nullptr;

    // Iterate through lanes and find the nth visible one
    int visibleCount = 0;
    for (size_t i = 0; i < track->getNumAutomationLanes(); ++i)
    {
        AutomationLane* lane = track->getAutomationLane(i);
        if (lane != nullptr && lane->isVisible())
        {
            if (visibleCount == laneIndex)
                return lane;
            ++visibleCount;
        }
    }
    return nullptr;
}

juce::Rectangle<int> TimelineView::getAutomationLaneBounds(int trackIndex, int laneIndex) const
{
    if (timelinePtr == nullptr)
        return juce::Rectangle<int>();

    // Calculate Y position for the track
    int y = RULER_HEIGHT;
    for (int i = 0; i < trackIndex; ++i)
    {
        y += getTotalTrackHeight(i);
    }

    // Add track height to get to automation lanes area
    y += trackHeight;

    // Add height for previous automation lanes
    y += laneIndex * AUTOMATION_LANE_HEIGHT;

    int x = HEADER_WIDTH;
    int width = getWidth() - HEADER_WIDTH;

    return juce::Rectangle<int>(x, y, width, AUTOMATION_LANE_HEIGHT);
}

int TimelineView::getAutomationLaneAtY(int y, int trackIndex) const
{
    if (timelinePtr == nullptr)
        return -1;

    // Calculate Y position for the track
    int trackY = RULER_HEIGHT;
    for (int i = 0; i < trackIndex; ++i)
    {
        trackY += getTotalTrackHeight(i);
    }

    // Check if y is within the track's automation lanes area
    int automationAreaStart = trackY + trackHeight;
    int numLanes = getNumVisibleAutomationLanes(trackIndex);

    if (y < automationAreaStart || y >= automationAreaStart + numLanes * AUTOMATION_LANE_HEIGHT)
        return -1;

    // Calculate which lane
    int relativeY = y - automationAreaStart;
    return relativeY / AUTOMATION_LANE_HEIGHT;
}

int TimelineView::automationValueToY(float value, juce::Rectangle<int> laneBounds) const
{
    // Value is normalized 0.0 to 1.0
    // Y increases downward, so higher values should be at top
    float normalizedValue = juce::jlimit(0.0f, 1.0f, value);
    int y = laneBounds.getBottom() - static_cast<int>(normalizedValue * laneBounds.getHeight());
    return y;
}

float TimelineView::yToAutomationValue(int y, juce::Rectangle<int> laneBounds) const
{
    // Convert Y position to normalized value (0.0 to 1.0)
    float relativeY = static_cast<float>(laneBounds.getBottom() - y);
    float value = relativeY / static_cast<float>(laneBounds.getHeight());
    return juce::jlimit(0.0f, 1.0f, value);
}

int TimelineView::findAutomationPointAtPosition(AutomationLane* lane, int x, int y, juce::Rectangle<int> laneBounds) const
{
    if (lane == nullptr)
        return -1;

    for (size_t i = 0; i < lane->getNumPoints(); ++i)
    {
        const AutomationPoint& point = lane->getPoint(i);

        // Convert point position to screen coordinates
        int pointX = HEADER_WIDTH + sampleToPixel(point.position) - static_cast<int>(horizontalScrollOffset);
        int pointY = automationValueToY(point.value, laneBounds);

        // Check if mouse is within hit radius
        int dx = x - pointX;
        int dy = y - pointY;
        if (dx * dx + dy * dy <= AUTOMATION_POINT_RADIUS * AUTOMATION_POINT_RADIUS * 4)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void TimelineView::drawAutomationLanes(juce::Graphics& g, juce::Rectangle<int> bounds, int trackIndex)
{
    if (timelinePtr == nullptr)
        return;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return;

    int laneIndex = 0;
    for (size_t i = 0; i < track->getNumAutomationLanes(); ++i)
    {
        AutomationLane* lane = track->getAutomationLane(i);
        if (lane != nullptr && lane->isVisible())
        {
            juce::Rectangle<int> laneBounds = getAutomationLaneBounds(trackIndex, laneIndex);

            // Draw header area
            auto headerBounds = laneBounds.removeFromLeft(0);  // Header is handled separately
            juce::ignoreUnused(headerBounds);

            // Draw the automation lane
            drawAutomationLane(g, laneBounds, lane);

            ++laneIndex;
        }
    }
}

void TimelineView::drawAutomationLane(juce::Graphics& g, juce::Rectangle<int> bounds, AutomationLane* lane)
{
    if (lane == nullptr)
        return;

    // Background
    g.setColour(MidiSingLookAndFeel::backgroundMid.darker(0.2f));
    g.fillRect(bounds);

    // Draw beat grid
    drawBeatGrid(g, bounds);

    // Draw lane header
    drawAutomationLaneHeader(g, bounds.withWidth(60), lane);

    // Adjust bounds for content area (after header)
    auto contentBounds = bounds.withTrimmedLeft(60);

    // Draw horizontal guide lines
    g.setColour(MidiSingLookAndFeel::borderColour.withAlpha(0.3f));
    for (int i = 1; i < 4; ++i)
    {
        int lineY = bounds.getY() + (bounds.getHeight() * i) / 4;
        g.drawHorizontalLine(lineY, static_cast<float>(contentBounds.getX()), static_cast<float>(contentBounds.getRight()));
    }

    // Draw the automation curve
    drawAutomationCurve(g, contentBounds, lane);

    // Draw automation points
    drawAutomationPoints(g, contentBounds, lane);

    // Bottom border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawHorizontalLine(bounds.getBottom() - 1, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
}

void TimelineView::drawAutomationLaneHeader(juce::Graphics& g, juce::Rectangle<int> bounds, AutomationLane* lane)
{
    if (lane == nullptr)
        return;

    // Header background
    g.setColour(MidiSingLookAndFeel::backgroundMid);
    g.fillRect(bounds);

    // Parameter name
    g.setColour(MidiSingLookAndFeel::textColour);
    g.setFont(10.0f);
    g.drawText(lane->getParameterName(), bounds.reduced(4), juce::Justification::centredLeft, true);

    // Draw mode indicator
    juce::String modeText;
    juce::Colour modeColour = MidiSingLookAndFeel::textDimColour;

    switch (automationMode)
    {
    case AutomationMode::Off:
        modeText = "OFF";
        break;
    case AutomationMode::Read:
        modeText = "R";
        modeColour = juce::Colour(0xff5ad4cf);  // Cyan for read
        break;
    case AutomationMode::Write:
        modeText = "W";
        modeColour = MidiSingLookAndFeel::recordColour;  // Red for write
        break;
    case AutomationMode::Touch:
        modeText = "T";
        modeColour = juce::Colour(0xffd4a85a);  // Orange for touch
        break;
    case AutomationMode::Latch:
        modeText = "L";
        modeColour = juce::Colour(0xffa85ad4);  // Purple for latch
        break;
    }

    auto modeRect = bounds.removeFromBottom(16).reduced(4, 2);
    g.setColour(modeColour);
    g.drawText(modeText, modeRect, juce::Justification::centred, false);

    // Right border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawVerticalLine(bounds.getRight() - 1, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));
}

void TimelineView::drawAutomationCurve(juce::Graphics& g, juce::Rectangle<int> bounds, AutomationLane* lane)
{
    if (lane == nullptr || lane->getNumPoints() == 0)
        return;

    // Draw the automation curve as connected lines
    juce::Path curvePath;
    bool firstPoint = true;

    // Get visible range
    double startBeat = horizontalScrollOffset / pixelsPerBeat;
    double endBeat = startBeat + bounds.getWidth() / pixelsPerBeat;
    int64_t startSample = timelinePtr ? timelinePtr->beatsToSamples(startBeat) : 0;
    int64_t endSample = timelinePtr ? timelinePtr->beatsToSamples(endBeat) : 0;

    // Draw from default value at start if first point is not at position 0
    if (lane->getNumPoints() > 0)
    {
        const AutomationPoint& firstPt = lane->getPoint(0);
        if (firstPt.position > startSample)
        {
            // Draw flat line from start to first point at default value
            float defaultValue = lane->getDefaultValue();
            // Normalize the default value
            float minVal = lane->getMinValue();
            float maxVal = lane->getMaxValue();
            float normalizedDefault = (defaultValue - minVal) / (maxVal - minVal);

            int x = bounds.getX();
            int y = automationValueToY(normalizedDefault, bounds);
            curvePath.startNewSubPath(static_cast<float>(x), static_cast<float>(y));
            firstPoint = false;
        }
    }

    for (size_t i = 0; i < lane->getNumPoints(); ++i)
    {
        const AutomationPoint& point = lane->getPoint(i);

        int x = HEADER_WIDTH + sampleToPixel(point.position) - static_cast<int>(horizontalScrollOffset);
        int y = automationValueToY(point.value, bounds);

        // Clamp to visible area
        x = juce::jmax(bounds.getX(), juce::jmin(x, bounds.getRight()));

        if (firstPoint)
        {
            curvePath.startNewSubPath(static_cast<float>(x), static_cast<float>(y));
            firstPoint = false;
        }
        else
        {
            curvePath.lineTo(static_cast<float>(x), static_cast<float>(y));
        }
    }

    // Extend to end of visible area
    if (lane->getNumPoints() > 0)
    {
        const AutomationPoint& lastPt = lane->getPoint(lane->getNumPoints() - 1);
        int lastX = HEADER_WIDTH + sampleToPixel(lastPt.position) - static_cast<int>(horizontalScrollOffset);
        if (lastX < bounds.getRight())
        {
            int y = automationValueToY(lastPt.value, bounds);
            curvePath.lineTo(static_cast<float>(bounds.getRight()), static_cast<float>(y));
        }
    }

    // Draw the curve
    g.setColour(MidiSingLookAndFeel::accentColour);
    g.strokePath(curvePath, juce::PathStrokeType(2.0f));

    // Draw filled area below curve (optional, subtle)
    if (!curvePath.isEmpty())
    {
        juce::Path fillPath = curvePath;
        fillPath.lineTo(static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom()));
        fillPath.lineTo(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getBottom()));
        fillPath.closeSubPath();

        g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.1f));
        g.fillPath(fillPath);
    }
}

void TimelineView::drawAutomationPoints(juce::Graphics& g, juce::Rectangle<int> bounds, AutomationLane* lane)
{
    if (lane == nullptr)
        return;

    for (size_t i = 0; i < lane->getNumPoints(); ++i)
    {
        const AutomationPoint& point = lane->getPoint(i);

        int x = HEADER_WIDTH + sampleToPixel(point.position) - static_cast<int>(horizontalScrollOffset);
        int y = automationValueToY(point.value, bounds);

        // Skip if outside visible area
        if (x < bounds.getX() - AUTOMATION_POINT_RADIUS || x > bounds.getRight() + AUTOMATION_POINT_RADIUS)
            continue;

        // Determine if this point is being edited
        bool isEditing = (isDraggingAutomationPoint &&
                          automationEditLane == lane &&
                          automationEditPointIndex == static_cast<int>(i));

        // Draw point handle
        if (isEditing)
        {
            // Larger, highlighted point when editing
            g.setColour(MidiSingLookAndFeel::accentColour);
            g.fillEllipse(static_cast<float>(x - AUTOMATION_POINT_RADIUS - 1),
                          static_cast<float>(y - AUTOMATION_POINT_RADIUS - 1),
                          static_cast<float>((AUTOMATION_POINT_RADIUS + 1) * 2),
                          static_cast<float>((AUTOMATION_POINT_RADIUS + 1) * 2));
        }
        else
        {
            // Normal point
            g.setColour(MidiSingLookAndFeel::accentColour);
            g.fillEllipse(static_cast<float>(x - AUTOMATION_POINT_RADIUS),
                          static_cast<float>(y - AUTOMATION_POINT_RADIUS),
                          static_cast<float>(AUTOMATION_POINT_RADIUS * 2),
                          static_cast<float>(AUTOMATION_POINT_RADIUS * 2));

            // White outline
            g.setColour(juce::Colours::white);
            g.drawEllipse(static_cast<float>(x - AUTOMATION_POINT_RADIUS),
                          static_cast<float>(y - AUTOMATION_POINT_RADIUS),
                          static_cast<float>(AUTOMATION_POINT_RADIUS * 2),
                          static_cast<float>(AUTOMATION_POINT_RADIUS * 2),
                          1.0f);
        }
    }
}

void TimelineView::handleAutomationMouseDown(const juce::MouseEvent& e, int trackIndex, int laneIndex)
{
    AutomationLane* lane = getAutomationLaneForTrack(trackIndex, laneIndex);
    if (lane == nullptr)
        return;

    juce::Rectangle<int> laneBounds = getAutomationLaneBounds(trackIndex, laneIndex);

    // Check if clicking on an existing point
    int pointIndex = findAutomationPointAtPosition(lane, e.x, e.y, laneBounds);

    automationEditTrackIndex = trackIndex;
    automationEditLaneIndex = laneIndex;
    automationEditLane = lane;
    automationEditLaneBounds = laneBounds;

    if (pointIndex >= 0)
    {
        // Clicking on existing point - start dragging
        automationEditPointIndex = pointIndex;
        isDraggingAutomationPoint = true;
        automationDragStartPosition = lane->getPoint(static_cast<size_t>(pointIndex)).position;
        automationDragStartValue = lane->getPoint(static_cast<size_t>(pointIndex)).value;
    }
    else if (automationMode != AutomationMode::Off)
    {
        // Clicking on empty area - create new point
        int64_t position = pixelToSample(e.x);
        float value = yToAutomationValue(e.y, laneBounds);

        lane->addPoint(position, value);

        // Find the newly added point and start dragging it
        int newPointIndex = lane->findPointAtPosition(position, 100);
        if (newPointIndex >= 0)
        {
            automationEditPointIndex = newPointIndex;
            isDraggingAutomationPoint = true;
            automationDragStartPosition = position;
            automationDragStartValue = value;
        }
    }

    isEditingAutomation = true;
    repaint();
}

void TimelineView::handleAutomationMouseDrag(const juce::MouseEvent& e)
{
    if (!isDraggingAutomationPoint || automationEditLane == nullptr || automationEditPointIndex < 0)
        return;

    // Calculate new position and value
    int64_t newPosition = pixelToSample(e.x);
    float newValue = yToAutomationValue(e.y, automationEditLaneBounds);

    // Snap to grid if enabled
    if (snapToGrid)
    {
        newPosition = snapPositionToGrid(newPosition);
    }

    // Update the point
    automationEditLane->movePoint(static_cast<size_t>(automationEditPointIndex), newPosition, newValue);

    repaint();
}

void TimelineView::handleAutomationMouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    isDraggingAutomationPoint = false;
    isEditingAutomation = false;
    automationEditLane = nullptr;
    automationEditPointIndex = -1;

    repaint();
}

void TimelineView::recordAutomationAtPlayhead()
{
    if (transportPtr == nullptr || timelinePtr == nullptr)
        return;

    if (automationMode != AutomationMode::Write && automationMode != AutomationMode::Touch && automationMode != AutomationMode::Latch)
        return;

    if (transportPtr->isStopped())
        return;

    int64_t playheadPos = transportPtr->getPlayheadPosition();

    // Record automation for all visible lanes on all tracks
    for (int trackIndex = 0; trackIndex < timelinePtr->getNumTracks(); ++trackIndex)
    {
        Track* track = timelinePtr->getTrack(trackIndex);
        if (track == nullptr)
            continue;

        // Record volume automation
        AutomationLane* volumeLane = track->findAutomationLane("Volume");
        if (volumeLane != nullptr && volumeLane->isVisible() && !volumeLane->isBypassed())
        {
            // Get current fader value and record it
            float currentVolume = track->getVolume();
            // Normalize to 0-1 range for storage
            float minVal = volumeLane->getMinValue();
            float maxVal = volumeLane->getMaxValue();
            float normalizedValue = (currentVolume - minVal) / (maxVal - minVal);
            volumeLane->addPoint(playheadPos, normalizedValue);
        }

        // Record pan automation
        AutomationLane* panLane = track->findAutomationLane("Pan");
        if (panLane != nullptr && panLane->isVisible() && !panLane->isBypassed())
        {
            float currentPan = track->getPan();
            float minVal = panLane->getMinValue();
            float maxVal = panLane->getMaxValue();
            float normalizedValue = (currentPan - minVal) / (maxVal - minVal);
            panLane->addPoint(playheadPos, normalizedValue);
        }
    }

    lastAutomationRecordPosition = playheadPos;
}

void TimelineView::drawWaveformRmsPlusPeak(juce::Graphics& g, juce::Rectangle<int> bounds,
                                            int64_t cacheHash, int64_t regionOffset, int64_t regionLength)
{
    // Get mipmap data from cache
    const WaveformMipmapData* mipmap = waveformCache.getMipmap(cacheHash);
    if (mipmap == nullptr || mipmap->getNumChannels() == 0)
        return;

    int numChannels = mipmap->getNumChannels();
    int width = bounds.getWidth();
    int height = bounds.getHeight();

    if (width <= 0 || height <= 0)
        return;

    // OPTIMIZATION 1: Visible bounds culling - only render pixels in visible clip region
    auto clipBounds = g.getClipBounds();
    int visibleLeft = juce::jmax(clipBounds.getX(), bounds.getX());
    int visibleRight = juce::jmin(clipBounds.getRight(), bounds.getRight());

    // Early exit if region is completely outside visible area
    if (visibleLeft >= visibleRight)
        return;

    // Calculate relative pixel range within this region's bounds
    int startPixel = juce::jmax(0, visibleLeft - bounds.getX());
    int endPixel = juce::jmin(width, visibleRight - bounds.getX());

    if (startPixel >= endPixel)
        return;

    int visibleWidth = endPixel - startPixel;

    // Calculate samples per pixel for this zoom level
    double samplesPerPixel = static_cast<double>(regionLength) / static_cast<double>(width);

    // OPTIMIZATION 2: Pixel stride for very zoomed out views
    // When zoomed out far, drawing every pixel is wasteful - use stride to reduce workload
    int pixelStride = 1;
    if (visibleWidth > 2000)
        pixelStride = 2;
    else if (visibleWidth > 4000)
        pixelStride = 4;

    // Calculate effective number of data points we'll collect
    int numDataPoints = (visibleWidth + pixelStride - 1) / pixelStride;

    // Calculate channel heights
    int channelHeight = height / numChannels;
    bool combineChannels = channelHeight < 4;
    if (combineChannels)
        channelHeight = height;  // Combine channels if too small

    int numChannelsToDraw = combineChannels ? 1 : numChannels;

    // Draw each channel
    for (int ch = 0; ch < numChannelsToDraw; ++ch)
    {
        int channelY = bounds.getY() + ch * channelHeight;
        int centerY = channelY + channelHeight / 2;
        float halfHeight = channelHeight * 0.5f * 0.85f;  // Leave some margin

        // OPTIMIZATION 3: Pre-allocate vectors for single-pass data collection
        // This avoids recalculating peaks for the bottom half of the waveform
        struct WaveformPoint
        {
            float x;
            float peakMin;
            float peakMax;
            float rms;
            bool valid;
        };

        std::vector<WaveformPoint> points;
        points.reserve(static_cast<size_t>(numDataPoints));

        // Single pass: collect all peak data
        for (int localX = 0; localX < visibleWidth; localX += pixelStride)
        {
            int x = startPixel + localX;

            // Calculate sample range for this pixel (accounting for stride)
            int64_t startSample = regionOffset + static_cast<int64_t>(x * samplesPerPixel);
            int64_t endSample = regionOffset + static_cast<int64_t>(juce::jmin(x + pixelStride, width) * samplesPerPixel);

            // Clamp to valid range
            startSample = juce::jmax((int64_t)0, startSample);
            endSample = juce::jmin(regionOffset + regionLength, endSample);

            WaveformPoint point;
            point.x = static_cast<float>(bounds.getX() + x);
            point.valid = false;

            if (startSample < endSample)
            {
                int channelToRead = (combineChannels && numChannels > 1) ? 0 : ch;
                if (waveformCache.getPeaksForRange(cacheHash, startSample, endSample,
                                                    channelToRead, point.peakMin, point.peakMax, point.rms))
                {
                    // If stereo and combining channels, get max of both
                    if (combineChannels && numChannels > 1)
                    {
                        float peakMin2, peakMax2, rms2;
                        if (waveformCache.getPeaksForRange(cacheHash, startSample, endSample, 1,
                                                            peakMin2, peakMax2, rms2))
                        {
                            point.peakMin = juce::jmin(point.peakMin, peakMin2);
                            point.peakMax = juce::jmax(point.peakMax, peakMax2);
                            point.rms = juce::jmax(point.rms, rms2);
                        }
                    }
                    point.valid = true;
                }
            }

            points.push_back(point);
        }

        // Skip if no valid data points
        if (points.empty())
            continue;

        // OPTIMIZATION 4: Pre-allocate path memory
        // Each path needs approximately 2 * numDataPoints elements (top + bottom)
        juce::Path peakPath;
        juce::Path rmsPath;
        peakPath.preallocateSpace(static_cast<int>(points.size() * 2 * 3));  // x, y per point, top + bottom
        rmsPath.preallocateSpace(static_cast<int>(points.size() * 2 * 3));

        bool peakPathStarted = false;
        bool rmsPathStarted = false;

        // Build top half of waveform (positive values) - forward direction
        for (const auto& pt : points)
        {
            if (!pt.valid)
                continue;

            float peakTopY = centerY - pt.peakMax * halfHeight;
            float rmsTopY = centerY - pt.rms * halfHeight;

            if (!peakPathStarted)
            {
                peakPath.startNewSubPath(pt.x, peakTopY);
                peakPathStarted = true;
            }
            else
            {
                peakPath.lineTo(pt.x, peakTopY);
            }

            if (!rmsPathStarted)
            {
                rmsPath.startNewSubPath(pt.x, rmsTopY);
                rmsPathStarted = true;
            }
            else
            {
                rmsPath.lineTo(pt.x, rmsTopY);
            }
        }

        // Build bottom half of waveform (negative values) - reverse direction
        // OPTIMIZATION: Use stored data instead of recalculating
        for (auto it = points.rbegin(); it != points.rend(); ++it)
        {
            if (!it->valid)
                continue;

            // peakMin is negative, so centerY - peakMin gives bottom peak position
            float peakBottomY = centerY - it->peakMin * halfHeight;
            float rmsBottomY = centerY + it->rms * halfHeight;

            if (peakPathStarted)
            {
                peakPath.lineTo(it->x, peakBottomY);
            }

            if (rmsPathStarted)
            {
                rmsPath.lineTo(it->x, rmsBottomY);
            }
        }

        // Close the paths
        if (peakPathStarted)
            peakPath.closeSubPath();
        if (rmsPathStarted)
            rmsPath.closeSubPath();

        // Draw peak envelope as outer outline (semi-transparent)
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.fillPath(peakPath);

        // Draw RMS as solid inner fill
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillPath(rmsPath);

        // Draw peak outline for better definition
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.strokePath(peakPath, juce::PathStrokeType(0.5f));
    }
}

void TimelineView::drawSpectrogramOverlay(juce::Graphics& g, juce::Rectangle<int> bounds,
                                           Region* region, int64_t regionOffset, int64_t regionLength)
{
    if (region == nullptr || bounds.isEmpty() || regionLength <= 0)
        return;

    // Cast to AudioRegion to get the audio buffer
    auto* audioRegion = dynamic_cast<AudioRegion*>(region);
    if (audioRegion == nullptr)
        return;

    // Get or generate spectrogram ID
    int64_t thumbnailHash = region->getThumbnailHash();
    int64_t spectrogramId = 0;

    auto it = regionSpectrogramIds.find(thumbnailHash);
    if (it != regionSpectrogramIds.end())
    {
        spectrogramId = it->second;
    }
    else
    {
        // Generate spectrogram for this region
        const auto& buffer = audioRegion->getAudioBuffer();
        spectrogramGenerator.setSampleRate(sampleRate);
        spectrogramId = spectrogramGenerator.generateSpectrogram(buffer, 0, buffer.getNumSamples());

        if (spectrogramId != 0)
        {
            regionSpectrogramIds[thumbnailHash] = spectrogramId;
        }
    }

    if (spectrogramId == 0 || !spectrogramGenerator.hasSpectrogram(spectrogramId))
        return;

    // Draw the spectrogram with the configured alpha
    spectrogramGenerator.drawSpectrogram(g, bounds, spectrogramId, regionOffset, regionLength, spectrogramAlpha);
}

void TimelineView::setSpectrogramColorMap(SpectrogramGenerator::ColorMap colorMap)
{
    spectrogramGenerator.setColorMap(colorMap);
    repaint();
}

SpectrogramGenerator::ColorMap TimelineView::getSpectrogramColorMap() const
{
    return spectrogramGenerator.getColorMap();
}
