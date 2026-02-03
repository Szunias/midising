#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"

/**
 * A draggable splitter bar for resizing adjacent panels.
 * Supports both horizontal and vertical orientations.
 */
class ResizableSplitter : public juce::Component
{
public:
    /**
     * Orientation of the splitter.
     * Vertical splits left/right panels, Horizontal splits top/bottom panels.
     */
    enum class Orientation
    {
        Vertical,    // Draggable left/right (splits left from right)
        Horizontal   // Draggable up/down (splits top from bottom)
    };

    /**
     * Constructs a resizable splitter.
     * @param orientation Whether to split vertically or horizontally
     * @param thickness The thickness of the splitter bar in pixels
     */
    explicit ResizableSplitter(Orientation orientation, int thickness = 6)
        : splitterOrientation(orientation),
          splitterThickness(thickness)
    {
        setRepaintsOnMouseActivity(true);
        updateMouseCursor();
    }

    ~ResizableSplitter() override = default;

    /**
     * Sets the minimum and maximum values for the splitter position.
     * These represent the minimum and maximum sizes of the first panel.
     */
    void setLimits(int minimum, int maximum)
    {
        minPosition = minimum;
        maxPosition = maximum;
    }

    /**
     * Gets the current position/size value.
     */
    int getCurrentPosition() const { return currentPosition; }

    /**
     * Sets the current position/size value.
     */
    void setCurrentPosition(int position)
    {
        currentPosition = juce::jlimit(minPosition, maxPosition, position);
    }

    /**
     * Callback when the splitter is dragged.
     * The int parameter is the new position/size value.
     */
    std::function<void(int)> onDrag;

    /**
     * Callback when dragging ends.
     */
    std::function<void()> onDragEnd;

    //==========================================================================
    // Component overrides

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Draw splitter background
        auto baseColour = MidiSingLookAndFeel::backgroundMid;

        if (isDragging)
            baseColour = MidiSingLookAndFeel::accentColour.withAlpha(0.3f);
        else if (isMouseOver())
            baseColour = MidiSingLookAndFeel::backgroundLight;

        g.setColour(baseColour);
        g.fillRect(bounds);

        // Draw grip dots/lines in the center
        g.setColour(MidiSingLookAndFeel::borderColour);

        if (splitterOrientation == Orientation::Vertical)
        {
            // Draw vertical grip lines
            auto centerX = bounds.getCentreX();
            auto startY = bounds.getCentreY() - 15.0f;

            for (int i = 0; i < 3; ++i)
            {
                float y = startY + i * 10.0f;
                g.fillEllipse(centerX - 1.5f, y - 1.5f, 3.0f, 3.0f);
            }
        }
        else
        {
            // Draw horizontal grip lines
            auto centerY = bounds.getCentreY();
            auto startX = bounds.getCentreX() - 15.0f;

            for (int i = 0; i < 3; ++i)
            {
                float x = startX + i * 10.0f;
                g.fillEllipse(x - 1.5f, centerY - 1.5f, 3.0f, 3.0f);
            }
        }

        // Draw border lines
        g.setColour(MidiSingLookAndFeel::borderColour);

        if (splitterOrientation == Orientation::Vertical)
        {
            g.fillRect(bounds.removeFromLeft(1.0f));
            g.fillRect(bounds.removeFromRight(1.0f));
        }
        else
        {
            g.fillRect(bounds.removeFromTop(1.0f));
            g.fillRect(bounds.removeFromBottom(1.0f));
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        isDragging = true;
        dragStartPosition = currentPosition;
        dragStartMousePos = splitterOrientation == Orientation::Vertical
                           ? event.getScreenX()
                           : event.getScreenY();
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!isDragging)
            return;

        int currentMousePos = splitterOrientation == Orientation::Vertical
                             ? event.getScreenX()
                             : event.getScreenY();

        int delta = currentMousePos - dragStartMousePos;

        // For horizontal splitters (top/bottom), we need to invert the delta
        // because dragging up should increase the bottom panel size
        if (splitterOrientation == Orientation::Horizontal)
            delta = -delta;

        int newPosition = juce::jlimit(minPosition, maxPosition, dragStartPosition + delta);

        if (newPosition != currentPosition)
        {
            currentPosition = newPosition;

            if (onDrag)
                onDrag(currentPosition);
        }
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        juce::ignoreUnused(event);

        isDragging = false;
        repaint();

        if (onDragEnd)
            onDragEnd();
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        juce::ignoreUnused(event);
        repaint();
    }

    void mouseExit(const juce::MouseEvent& event) override
    {
        juce::ignoreUnused(event);
        repaint();
    }

    int getThickness() const { return splitterThickness; }

private:
    void updateMouseCursor()
    {
        if (splitterOrientation == Orientation::Vertical)
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        else
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    }

    Orientation splitterOrientation;
    int splitterThickness;
    int minPosition = 100;
    int maxPosition = 500;
    int currentPosition = 250;
    int dragStartPosition = 0;
    int dragStartMousePos = 0;
    bool isDragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResizableSplitter)
};
