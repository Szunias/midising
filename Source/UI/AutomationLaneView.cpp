#include "AutomationLaneView.h"
#include <algorithm>
#include <cmath>

AutomationLaneView::AutomationLaneView()
{
    setWantsKeyboardFocus(true);
}

void AutomationLaneView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Draw header if enabled
    juce::Rectangle<int> headerBounds;
    if (showHeader)
    {
        headerBounds = bounds.removeFromTop(HEADER_HEIGHT);
        drawHeader(g, headerBounds);
    }

    // Draw main automation area
    auto automationBounds = bounds;
    drawBackground(g, automationBounds);
    drawGrid(g, automationBounds);

    if (automationLane != nullptr)
    {
        drawAutomationCurve(g, automationBounds);
        drawAutomationPoints(g, automationBounds);
    }

    // Draw hover preview when using Draw tool
    if (showHoverPreview && currentTool == AutomationTool::Draw)
    {
        drawHoverPreview(g, automationBounds);
    }

    // Draw box selection rectangle if selecting
    if (isBoxSelecting)
    {
        g.setColour(MidiSingLookAndFeel::selectionColour.withAlpha(0.2f));
        auto selectRect = juce::Rectangle<int>(boxSelectStart, boxSelectEnd);
        g.fillRect(selectRect);
        g.setColour(MidiSingLookAndFeel::selectionColour);
        g.drawRect(selectRect, 1);
    }

    // Draw value readout during editing
    if (isShowingEditValue && showValueReadout)
    {
        drawValueReadout(g, automationBounds);
    }
}

void AutomationLaneView::resized()
{
    // Component resized - no child components to layout
}

void AutomationLaneView::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    if (automationLane == nullptr)
        return;

    switch (currentTool)
    {
        case AutomationTool::Draw:
            handleDrawToolMouseDown(e);
            break;
        case AutomationTool::Select:
            handleSelectToolMouseDown(e);
            break;
        case AutomationTool::Erase:
            handleEraseToolMouseDown(e);
            break;
    }
}

void AutomationLaneView::mouseDrag(const juce::MouseEvent& e)
{
    if (automationLane == nullptr)
        return;

    switch (currentTool)
    {
        case AutomationTool::Draw:
            handleDrawToolMouseDrag(e);
            break;
        case AutomationTool::Select:
            handleSelectToolMouseDrag(e);
            break;
        case AutomationTool::Erase:
            // Continue erasing during drag
            handleEraseToolMouseDown(e);
            break;
    }
}

void AutomationLaneView::mouseUp(const juce::MouseEvent& e)
{
    if (automationLane == nullptr)
        return;

    switch (currentTool)
    {
        case AutomationTool::Draw:
            handleDrawToolMouseUp(e);
            break;
        case AutomationTool::Select:
            handleSelectToolMouseUp(e);
            break;
        case AutomationTool::Erase:
            // Nothing special on mouse up for erase
            break;
    }

    isDragging = false;
    isDrawing = false;
    isShowingEditValue = false;
    repaint();
}

void AutomationLaneView::mouseMove(const juce::MouseEvent& e)
{
    hoverPosition = e.getPosition();

    // Update hover state
    auto bounds = getAutomationBounds();
    if (bounds.contains(e.getPosition()))
    {
        hoveredPointIndex = findPointAtPosition(e.x, e.y);
        showHoverPreview = (hoveredPointIndex < 0 && currentTool == AutomationTool::Draw);
    }
    else
    {
        hoveredPointIndex = -1;
        showHoverPreview = false;
    }

    updateCursorForPosition(e.x, e.y);
    repaint();
}

void AutomationLaneView::mouseExit(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    hoveredPointIndex = -1;
    showHoverPreview = false;
    repaint();
}

void AutomationLaneView::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (automationLane == nullptr)
        return;

    auto bounds = getAutomationBounds();
    if (!bounds.contains(e.getPosition()))
        return;

    // Double-click to delete point or create point at exact position
    int pointIndex = findPointAtPosition(e.x, e.y);
    if (pointIndex >= 0)
    {
        // Delete the point
        deletePointAtIndex(static_cast<size_t>(pointIndex));
    }
    else
    {
        // Add a new point at exact position (no snap)
        auto tempSnap = snapEnabled;
        snapEnabled = false;
        addPointAtPosition(e.x, e.y);
        snapEnabled = tempSnap;
    }
}

void AutomationLaneView::setAutomationLane(AutomationLane* lane)
{
    automationLane = lane;
    clearSelection();
    repaint();
}

void AutomationLaneView::clearSelection()
{
    selectedPointIndices.clear();
    repaint();
}

void AutomationLaneView::deleteSelectedPoints()
{
    if (automationLane == nullptr || selectedPointIndices.empty())
        return;

    // Delete in reverse order to maintain valid indices
    std::vector<size_t> sortedIndices(selectedPointIndices.begin(), selectedPointIndices.end());
    std::sort(sortedIndices.rbegin(), sortedIndices.rend());

    for (size_t index : sortedIndices)
    {
        if (index < automationLane->getNumPoints())
        {
            automationLane->removePoint(index);
        }
    }

    clearSelection();

    if (onAutomationChanged)
        onAutomationChanged();

    repaint();
}

void AutomationLaneView::addPointAtPosition(int x, int y)
{
    if (automationLane == nullptr)
        return;

    auto bounds = getAutomationBounds();
    int64_t position = xToSamplePosition(x);
    float value = yToValue(y, bounds);

    // Apply snapping if enabled
    if (snapEnabled)
    {
        position = snapSamplePosition(position);
    }

    // Clamp value to valid range
    value = juce::jlimit(0.0f, 1.0f, value);

    automationLane->addPoint(position, value, defaultCurveType);

    if (onAutomationChanged)
        onAutomationChanged();

    repaint();
}

void AutomationLaneView::deletePointAtIndex(size_t index)
{
    if (automationLane == nullptr || index >= automationLane->getNumPoints())
        return;

    automationLane->removePoint(index);

    // Remove from selection if selected
    selectedPointIndices.erase(index);

    // Update selection indices (shift down indices greater than deleted)
    std::set<size_t> newSelection;
    for (size_t idx : selectedPointIndices)
    {
        if (idx > index)
            newSelection.insert(idx - 1);
        else
            newSelection.insert(idx);
    }
    selectedPointIndices = newSelection;

    if (onAutomationChanged)
        onAutomationChanged();

    repaint();
}

// Drawing helpers

void AutomationLaneView::drawBackground(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(MidiSingLookAndFeel::backgroundDark);
    g.fillRect(bounds);

    // Draw border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRect(bounds, 1);
}

void AutomationLaneView::drawGrid(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(MidiSingLookAndFeel::gridColour);

    // Calculate visible beat range
    double startBeat = horizontalScrollOffset / pixelsPerBeat;
    double endBeat = (horizontalScrollOffset + bounds.getWidth()) / pixelsPerBeat;

    // Draw vertical grid lines at beat boundaries
    for (double beat = std::floor(startBeat); beat <= endBeat; beat += 1.0)
    {
        int x = beatToX(beat);
        if (x >= bounds.getX() && x <= bounds.getRight())
        {
            // Draw stronger lines at bar boundaries (assuming 4/4)
            if (std::fmod(beat, 4.0) < 0.01)
            {
                g.setColour(MidiSingLookAndFeel::borderColour);
                g.drawVerticalLine(x, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));
            }
            else
            {
                g.setColour(MidiSingLookAndFeel::gridColour);
                g.drawVerticalLine(x, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));
            }
        }
    }

    // Draw horizontal grid lines at value intervals (25%, 50%, 75%)
    g.setColour(MidiSingLookAndFeel::gridColour.withAlpha(0.5f));
    for (float v = 0.25f; v < 1.0f; v += 0.25f)
    {
        int y = valueToY(v, bounds);
        g.drawHorizontalLine(y, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
    }
}

void AutomationLaneView::drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(MidiSingLookAndFeel::backgroundMid);
    g.fillRect(bounds);

    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawHorizontalLine(bounds.getBottom() - 1, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));

    if (automationLane != nullptr)
    {
        g.setColour(MidiSingLookAndFeel::textColour);
        g.setFont(12.0f);
        g.drawText(automationLane->getParameterName(),
                   bounds.reduced(4, 0),
                   juce::Justification::centredLeft, true);
    }
}

void AutomationLaneView::drawAutomationCurve(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (automationLane == nullptr || automationLane->getNumPoints() == 0)
    {
        // Draw default value line if no automation
        float defaultValue = automationLane != nullptr ? automationLane->getDefaultValue() : 0.5f;
        int y = valueToY(defaultValue, bounds);
        g.setColour(MidiSingLookAndFeel::automationColour.withAlpha(0.3f));
        g.drawHorizontalLine(y, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
        return;
    }

    // Build automation path
    juce::Path curvePath;

    const auto& points = automationLane->getPoints();
    bool pathStarted = false;

    // Calculate visible sample range
    int64_t startSample = xToSamplePosition(bounds.getX());
    int64_t endSample = xToSamplePosition(bounds.getRight());

    // Draw the automation curve
    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto& point = points[i];
        int x = samplePositionToX(point.position);
        int y = valueToY(point.value, bounds);

        if (!pathStarted)
        {
            // Draw flat line from left edge to first point
            int startY = valueToY(point.value, bounds);
            curvePath.startNewSubPath(static_cast<float>(bounds.getX()), static_cast<float>(startY));
            curvePath.lineTo(static_cast<float>(x), static_cast<float>(y));
            pathStarted = true;
        }
        else
        {
            const auto& prevPoint = points[i - 1];
            int prevX = samplePositionToX(prevPoint.position);
            int prevY = valueToY(prevPoint.value, bounds);

            // Draw curve segment based on curve type
            switch (prevPoint.curve)
            {
                case AutomationCurveType::Linear:
                    curvePath.lineTo(static_cast<float>(x), static_cast<float>(y));
                    break;

                case AutomationCurveType::Step:
                    curvePath.lineTo(static_cast<float>(x), static_cast<float>(prevY));
                    curvePath.lineTo(static_cast<float>(x), static_cast<float>(y));
                    break;

                case AutomationCurveType::Exponential:
                case AutomationCurveType::Logarithmic:
                case AutomationCurveType::SCurve:
                {
                    // Draw smooth curve using multiple line segments
                    int numSegments = std::max(1, (x - prevX) / 4);
                    for (int seg = 1; seg <= numSegments; ++seg)
                    {
                        float t = static_cast<float>(seg) / static_cast<float>(numSegments);
                        float interpValue = AutomationCurveUtils::interpolate(
                            prevPoint.value, point.value, t, prevPoint.curve);
                        int segX = prevX + static_cast<int>((x - prevX) * t);
                        int segY = valueToY(interpValue, bounds);
                        curvePath.lineTo(static_cast<float>(segX), static_cast<float>(segY));
                    }
                    break;
                }
            }
        }
    }

    // Extend to right edge with last point's value
    if (!points.empty())
    {
        int lastY = valueToY(points.back().value, bounds);
        curvePath.lineTo(static_cast<float>(bounds.getRight()), static_cast<float>(lastY));
    }

    // Draw the curve
    g.setColour(MidiSingLookAndFeel::automationColour);
    g.strokePath(curvePath, juce::PathStrokeType(2.0f));

    // Draw filled area under curve (optional, for visual clarity)
    juce::Path filledPath(curvePath);
    filledPath.lineTo(static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom()));
    filledPath.lineTo(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getBottom()));
    filledPath.closeSubPath();

    g.setColour(MidiSingLookAndFeel::automationColour.withAlpha(0.1f));
    g.fillPath(filledPath);
}

void AutomationLaneView::drawAutomationPoints(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (automationLane == nullptr)
        return;

    const auto& points = automationLane->getPoints();

    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto& point = points[i];
        int x = samplePositionToX(point.position);
        int y = valueToY(point.value, bounds);

        // Check if point is visible
        if (x < bounds.getX() - POINT_RADIUS || x > bounds.getRight() + POINT_RADIUS)
            continue;

        // Determine point color based on state
        juce::Colour pointColour = MidiSingLookAndFeel::automationColour;

        bool isSelected = selectedPointIndices.count(i) > 0;
        bool isHovered = (static_cast<int>(i) == hoveredPointIndex);

        if (isSelected)
        {
            pointColour = MidiSingLookAndFeel::selectionColour;
        }
        else if (isHovered)
        {
            pointColour = MidiSingLookAndFeel::automationColour.brighter(0.3f);
        }

        // Draw outer circle (border)
        g.setColour(MidiSingLookAndFeel::backgroundDark);
        g.fillEllipse(static_cast<float>(x - POINT_RADIUS - 1),
                      static_cast<float>(y - POINT_RADIUS - 1),
                      static_cast<float>((POINT_RADIUS + 1) * 2),
                      static_cast<float>((POINT_RADIUS + 1) * 2));

        // Draw inner circle
        g.setColour(pointColour);
        g.fillEllipse(static_cast<float>(x - POINT_RADIUS),
                      static_cast<float>(y - POINT_RADIUS),
                      static_cast<float>(POINT_RADIUS * 2),
                      static_cast<float>(POINT_RADIUS * 2));

        // Draw selection indicator
        if (isSelected)
        {
            g.setColour(juce::Colours::white);
            g.drawEllipse(static_cast<float>(x - POINT_RADIUS),
                          static_cast<float>(y - POINT_RADIUS),
                          static_cast<float>(POINT_RADIUS * 2),
                          static_cast<float>(POINT_RADIUS * 2),
                          1.5f);
        }
    }
}

void AutomationLaneView::drawValueReadout(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Draw a tooltip-style readout near the editing position
    int x = samplePositionToX(currentEditPosition);
    int y = valueToY(currentEditValue, bounds);

    // Format value as percentage or dB depending on parameter type
    juce::String valueText;
    if (automationLane != nullptr)
    {
        auto paramType = automationLane->getParameterType();
        if (paramType == AutomationParameterType::Volume)
        {
            float db = AutomationCurveUtils::normalizedToDecibels(currentEditValue);
            valueText = juce::String(db, 1) + " dB";
        }
        else if (paramType == AutomationParameterType::Pan)
        {
            int pan = static_cast<int>((currentEditValue - 0.5f) * 200.0f);
            if (pan < 0)
                valueText = juce::String(std::abs(pan)) + "L";
            else if (pan > 0)
                valueText = juce::String(pan) + "R";
            else
                valueText = "C";
        }
        else
        {
            valueText = juce::String(static_cast<int>(currentEditValue * 100.0f)) + "%";
        }
    }
    else
    {
        valueText = juce::String(static_cast<int>(currentEditValue * 100.0f)) + "%";
    }

    // Calculate readout position
    auto readoutBounds = juce::Rectangle<int>(x + 10, y - 20, VALUE_LABEL_WIDTH + 10, 16);

    // Keep readout within bounds
    if (readoutBounds.getRight() > bounds.getRight())
        readoutBounds.setX(x - readoutBounds.getWidth() - 10);
    if (readoutBounds.getY() < bounds.getY())
        readoutBounds.setY(y + 10);

    // Draw background
    g.setColour(MidiSingLookAndFeel::backgroundLight);
    g.fillRoundedRectangle(readoutBounds.toFloat(), 3.0f);

    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRoundedRectangle(readoutBounds.toFloat(), 3.0f, 1.0f);

    // Draw text
    g.setColour(MidiSingLookAndFeel::textColour);
    g.setFont(11.0f);
    g.drawText(valueText, readoutBounds, juce::Justification::centred, false);
}

void AutomationLaneView::drawHoverPreview(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (!bounds.contains(hoverPosition))
        return;

    // Draw a ghost point at hover position
    int x = hoverPosition.x;
    float value = yToValue(hoverPosition.y, bounds);
    int y = valueToY(value, bounds);

    g.setColour(MidiSingLookAndFeel::automationColour.withAlpha(0.4f));
    g.fillEllipse(static_cast<float>(x - POINT_RADIUS),
                  static_cast<float>(y - POINT_RADIUS),
                  static_cast<float>(POINT_RADIUS * 2),
                  static_cast<float>(POINT_RADIUS * 2));

    // Draw crosshairs
    g.setColour(MidiSingLookAndFeel::automationColour.withAlpha(0.2f));
    g.drawHorizontalLine(y, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
    g.drawVerticalLine(x, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));
}

// Coordinate conversion

int64_t AutomationLaneView::xToSamplePosition(int x) const
{
    double beat = xToBeat(x);
    // Convert beat to samples: samples = beat * (60/bpm) * sampleRate
    double seconds = beat * 60.0 / tempo;
    return static_cast<int64_t>(seconds * sampleRate);
}

int AutomationLaneView::samplePositionToX(int64_t position) const
{
    // Convert samples to beat: beat = (position/sampleRate) * (bpm/60)
    double seconds = static_cast<double>(position) / sampleRate;
    double beat = seconds * tempo / 60.0;
    return beatToX(beat);
}

double AutomationLaneView::xToBeat(int x) const
{
    return (static_cast<double>(x) + horizontalScrollOffset) / pixelsPerBeat;
}

int AutomationLaneView::beatToX(double beat) const
{
    return static_cast<int>(beat * pixelsPerBeat - horizontalScrollOffset);
}

float AutomationLaneView::yToValue(int y, juce::Rectangle<int> bounds) const
{
    // Invert Y axis (top = max, bottom = min)
    float normalizedY = static_cast<float>(y - bounds.getY()) / static_cast<float>(bounds.getHeight());
    return 1.0f - juce::jlimit(0.0f, 1.0f, normalizedY);
}

int AutomationLaneView::valueToY(float value, juce::Rectangle<int> bounds) const
{
    // Invert Y axis (top = max, bottom = min)
    return bounds.getY() + static_cast<int>((1.0f - value) * bounds.getHeight());
}

int AutomationLaneView::findPointAtPosition(int x, int y) const
{
    if (automationLane == nullptr)
        return -1;

    auto bounds = getAutomationBounds();
    const auto& points = automationLane->getPoints();

    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto& point = points[i];
        int px = samplePositionToX(point.position);
        int py = valueToY(point.value, bounds);

        int dx = x - px;
        int dy = y - py;

        if (dx * dx + dy * dy <= POINT_HIT_RADIUS * POINT_HIT_RADIUS)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

juce::Rectangle<int> AutomationLaneView::getAutomationBounds() const
{
    auto bounds = getLocalBounds();
    if (showHeader)
    {
        bounds.removeFromTop(HEADER_HEIGHT);
    }
    return bounds;
}

void AutomationLaneView::updateCursorForTool()
{
    switch (currentTool)
    {
        case AutomationTool::Draw:
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            break;
        case AutomationTool::Select:
            setMouseCursor(juce::MouseCursor::NormalCursor);
            break;
        case AutomationTool::Erase:
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            break;
    }
}

void AutomationLaneView::updateCursorForPosition(int x, int y)
{
    if (findPointAtPosition(x, y) >= 0)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    else
    {
        updateCursorForTool();
    }
}

double AutomationLaneView::snapBeatPosition(double beat) const
{
    if (!snapEnabled)
        return beat;

    return std::round(beat / snapResolution) * snapResolution;
}

int64_t AutomationLaneView::snapSamplePosition(int64_t position) const
{
    if (!snapEnabled)
        return position;

    // Convert to beat, snap, convert back
    double seconds = static_cast<double>(position) / sampleRate;
    double beat = seconds * tempo / 60.0;
    double snappedBeat = snapBeatPosition(beat);
    double snappedSeconds = snappedBeat * 60.0 / tempo;
    return static_cast<int64_t>(snappedSeconds * sampleRate);
}

// Tool handlers

void AutomationLaneView::handleDrawToolMouseDown(const juce::MouseEvent& e)
{
    auto bounds = getAutomationBounds();
    if (!bounds.contains(e.getPosition()))
        return;

    int pointIndex = findPointAtPosition(e.x, e.y);

    if (pointIndex >= 0)
    {
        // Start dragging existing point
        isDragging = true;
        dragPointIndex = pointIndex;

        const auto& point = automationLane->getPoint(static_cast<size_t>(pointIndex));
        dragStartPosition = point.position;
        dragStartValue = point.value;

        currentEditPosition = point.position;
        currentEditValue = point.value;
        isShowingEditValue = true;

        // Add to selection if not already selected
        if (e.mods.isShiftDown())
        {
            selectedPointIndices.insert(static_cast<size_t>(pointIndex));
        }
        else if (selectedPointIndices.count(static_cast<size_t>(pointIndex)) == 0)
        {
            clearSelection();
            selectedPointIndices.insert(static_cast<size_t>(pointIndex));
        }
    }
    else
    {
        // Start drawing new points
        isDrawing = true;
        lastDragPosition = e.getPosition();

        // Add first point
        addPointAtPosition(e.x, e.y);

        // Show value readout
        currentEditPosition = xToSamplePosition(e.x);
        currentEditValue = yToValue(e.y, bounds);
        isShowingEditValue = true;
    }

    repaint();
}

void AutomationLaneView::handleDrawToolMouseDrag(const juce::MouseEvent& e)
{
    auto bounds = getAutomationBounds();

    if (isDragging && dragPointIndex >= 0 && automationLane != nullptr)
    {
        // Move the dragged point
        int64_t newPosition = xToSamplePosition(e.x);
        float newValue = yToValue(e.y, bounds);

        if (snapEnabled)
        {
            newPosition = snapSamplePosition(newPosition);
        }

        newValue = juce::jlimit(0.0f, 1.0f, newValue);

        automationLane->movePoint(static_cast<size_t>(dragPointIndex), newPosition, newValue);

        currentEditPosition = newPosition;
        currentEditValue = newValue;

        if (onAutomationChanged)
            onAutomationChanged();
    }
    else if (isDrawing)
    {
        // Add points during drag to create freehand automation
        int distance = e.getPosition().getDistanceFrom(lastDragPosition);
        if (distance >= 3)  // Only add point if moved enough
        {
            addPointAtPosition(e.x, e.y);
            lastDragPosition = e.getPosition();

            currentEditPosition = xToSamplePosition(e.x);
            currentEditValue = yToValue(e.y, bounds);
        }
    }

    repaint();
}

void AutomationLaneView::handleDrawToolMouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    isDragging = false;
    isDrawing = false;
    dragPointIndex = -1;

    // Sort points after editing
    if (automationLane != nullptr)
    {
        automationLane->sortPoints();
    }

    isShowingEditValue = false;
    repaint();
}

void AutomationLaneView::handleSelectToolMouseDown(const juce::MouseEvent& e)
{
    auto bounds = getAutomationBounds();
    if (!bounds.contains(e.getPosition()))
        return;

    int pointIndex = findPointAtPosition(e.x, e.y);

    if (pointIndex >= 0)
    {
        // Start dragging selected point(s)
        isDragging = true;
        dragPointIndex = pointIndex;

        const auto& point = automationLane->getPoint(static_cast<size_t>(pointIndex));
        dragStartPosition = point.position;
        dragStartValue = point.value;

        currentEditPosition = point.position;
        currentEditValue = point.value;
        isShowingEditValue = true;

        if (e.mods.isShiftDown())
        {
            // Toggle selection
            if (selectedPointIndices.count(static_cast<size_t>(pointIndex)) > 0)
                selectedPointIndices.erase(static_cast<size_t>(pointIndex));
            else
                selectedPointIndices.insert(static_cast<size_t>(pointIndex));
        }
        else if (selectedPointIndices.count(static_cast<size_t>(pointIndex)) == 0)
        {
            // Select only this point
            clearSelection();
            selectedPointIndices.insert(static_cast<size_t>(pointIndex));
        }
    }
    else
    {
        // Start box selection
        if (!e.mods.isShiftDown())
        {
            clearSelection();
        }

        isBoxSelecting = true;
        boxSelectStart = e.getPosition();
        boxSelectEnd = e.getPosition();
    }

    repaint();
}

void AutomationLaneView::handleSelectToolMouseDrag(const juce::MouseEvent& e)
{
    auto bounds = getAutomationBounds();

    if (isDragging && dragPointIndex >= 0 && automationLane != nullptr)
    {
        // Calculate delta from original position
        int64_t newPosition = xToSamplePosition(e.x);
        float newValue = yToValue(e.y, bounds);

        if (snapEnabled)
        {
            newPosition = snapSamplePosition(newPosition);
        }

        newValue = juce::jlimit(0.0f, 1.0f, newValue);

        // Move all selected points by the same delta
        int64_t deltaPosition = newPosition - dragStartPosition;
        float deltaValue = newValue - dragStartValue;

        const auto& points = automationLane->getPoints();
        for (size_t idx : selectedPointIndices)
        {
            if (idx < points.size())
            {
                const auto& originalPoint = points[idx];
                automationLane->movePoint(idx,
                                          originalPoint.position + deltaPosition,
                                          juce::jlimit(0.0f, 1.0f, originalPoint.value + deltaValue));
            }
        }

        currentEditPosition = newPosition;
        currentEditValue = newValue;

        if (onAutomationChanged)
            onAutomationChanged();
    }
    else if (isBoxSelecting)
    {
        boxSelectEnd = e.getPosition();

        // Update selection based on box
        auto selectRect = juce::Rectangle<int>(boxSelectStart, boxSelectEnd);

        if (automationLane != nullptr)
        {
            const auto& points = automationLane->getPoints();
            for (size_t i = 0; i < points.size(); ++i)
            {
                int px = samplePositionToX(points[i].position);
                int py = valueToY(points[i].value, bounds);

                if (selectRect.contains(px, py))
                {
                    selectedPointIndices.insert(i);
                }
            }
        }
    }

    repaint();
}

void AutomationLaneView::handleSelectToolMouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    isDragging = false;
    isBoxSelecting = false;
    dragPointIndex = -1;

    if (automationLane != nullptr)
    {
        automationLane->sortPoints();
    }

    isShowingEditValue = false;
    repaint();
}

void AutomationLaneView::handleEraseToolMouseDown(const juce::MouseEvent& e)
{
    auto bounds = getAutomationBounds();
    if (!bounds.contains(e.getPosition()))
        return;

    int pointIndex = findPointAtPosition(e.x, e.y);

    if (pointIndex >= 0)
    {
        deletePointAtIndex(static_cast<size_t>(pointIndex));
    }
}
