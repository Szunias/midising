#include "PluginSlot.h"

//==============================================================================
// PluginSlot Implementation
//==============================================================================

PluginSlot::PluginSlot(int index, int channel)
    : slotIndex(index)
    , channelIndex(channel)
{
    setRepaintsOnMouseActivity(true);
}

void PluginSlot::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    // Get background colour based on state
    juce::Colour bgColour = getBackgroundColour();

    // Highlight on hover (unless dragging over)
    if (mouseOver && !dragOver)
        bgColour = bgColour.brighter(0.15f);

    // Draw background
    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds, 2.0f);

    // Draw border
    juce::Colour borderColour = MidiSingLookAndFeel::borderColour;

    // Highlight border during drag-over
    if (dragOver)
    {
        borderColour = MidiSingLookAndFeel::accentColour;
        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds, 2.0f, 2.0f);

        // Draw drop indicator
        paintDropIndicator(g, bounds);
    }
    else
    {
        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
    }

    // Draw text - slot number if empty, effect/plugin name if filled
    g.setFont(juce::Font(9.0f));

    if (empty)
    {
        g.setColour(MidiSingLookAndFeel::textDimColour);
        g.drawText(juce::String(slotIndex + 1), bounds, juce::Justification::centred);
    }
    else
    {
        // Determine display text
        juce::String displayText;

        if (pluginInstance != nullptr)
        {
            // Use plugin name for external plugins
            displayText = pluginInstance->getName();
        }
        else if (effectName.isNotEmpty())
        {
            // Use effect name for built-in effects
            displayText = effectName;
        }
        else
        {
            displayText = "Effect";
        }

        // Add bypass indicator
        if (bypassed)
            displayText = "(" + displayText + ")";

        // Draw text with appropriate colour
        g.setColour(bypassed ? MidiSingLookAndFeel::textDimColour : MidiSingLookAndFeel::textColour);
        g.drawText(displayText, bounds.reduced(2, 0), juce::Justification::centred, true);

        // Draw external plugin indicator (small dot)
        if (externalPlugin)
        {
            g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.7f));
            g.fillEllipse(bounds.getRight() - 5.0f, bounds.getY() + 2.0f, 3.0f, 3.0f);
        }
    }

    // Draw dragging indicator if this slot is being dragged
    if (isDragging)
    {
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds, 2.0f);
    }
}

void PluginSlot::resized()
{
    // Nothing special needed
}

void PluginSlot::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        // Right-click - show context menu
        if (onRightClicked)
            onRightClicked(slotIndex, event.getScreenPosition());
    }
    else
    {
        // Left-click - store for potential drag
        dragStartPosition = event.getPosition();
        isDragging = false;
    }
}

void PluginSlot::mouseEnter(const juce::MouseEvent&)
{
    mouseOver = true;
    repaint();
}

void PluginSlot::mouseExit(const juce::MouseEvent&)
{
    mouseOver = false;
    repaint();
}

void PluginSlot::mouseDrag(const juce::MouseEvent& event)
{
    // Only allow dragging if enabled and slot is not empty
    if (!dragDropEnabled || empty)
        return;

    // Check if we've moved far enough to start a drag
    if (!isDragging)
    {
        auto distance = event.getPosition().getDistanceFrom(dragStartPosition);
        if (distance > DRAG_THRESHOLD)
        {
            isDragging = true;

            // Create drag data
            auto dragData = createDragData();

            // Find the DragAndDropContainer (usually the parent window)
            auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this);
            if (container != nullptr)
            {
                // Create a snapshot image for the drag
                auto snapshot = createComponentSnapshot(getLocalBounds());

                // Start the drag operation
                container->startDragging(juce::var(DRAG_ID), this, juce::ScaledImage(snapshot), true, nullptr, &event.source);
            }

            repaint();
        }
    }
}

void PluginSlot::mouseUp(const juce::MouseEvent& event)
{
    if (isDragging)
    {
        isDragging = false;
        repaint();
    }
    else if (!event.mods.isRightButtonDown())
    {
        // Simple click (no drag) - open plugin selector
        if (onClicked)
            onClicked(slotIndex);
    }
}

//==============================================================================
// DragAndDropTarget Implementation
//==============================================================================

bool PluginSlot::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    // Only accept plugin slot drags
    if (!dragDropEnabled)
        return false;

    // Check if this is a plugin slot drag
    if (dragSourceDetails.description.toString() == DRAG_ID)
    {
        // Don't accept drops on self
        auto* sourceSlot = dynamic_cast<PluginSlot*>(dragSourceDetails.sourceComponent.get());
        if (sourceSlot == this)
            return false;

        return true;
    }

    return false;
}

void PluginSlot::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    juce::ignoreUnused(dragSourceDetails);
    dragOver = true;
    repaint();
}

void PluginSlot::itemDragMove(const SourceDetails& dragSourceDetails)
{
    juce::ignoreUnused(dragSourceDetails);
    // Could update drop indicator position based on mouse location
}

void PluginSlot::itemDragExit(const SourceDetails& dragSourceDetails)
{
    juce::ignoreUnused(dragSourceDetails);
    dragOver = false;
    repaint();
}

void PluginSlot::itemDropped(const SourceDetails& dragSourceDetails)
{
    dragOver = false;
    repaint();

    // Get the source slot
    auto* sourceSlot = dynamic_cast<PluginSlot*>(dragSourceDetails.sourceComponent.get());
    if (sourceSlot == nullptr)
        return;

    // Notify about the drop
    if (onPluginDropped)
    {
        onPluginDropped(slotIndex, sourceSlot->getSlotIndex(), sourceSlot->getChannelIndex());
    }
}

//==============================================================================
// Slot State
//==============================================================================

void PluginSlot::setPluginInstance(PluginInstance* instance)
{
    pluginInstance = instance;

    if (instance != nullptr)
    {
        empty = false;
        externalPlugin = true;
        effectName = instance->getName();
        bypassed = instance->isBypassed();
    }
    else
    {
        // Keep other state as-is - might be a built-in effect
        if (!effectName.isEmpty())
        {
            empty = false;
            externalPlugin = false;
        }
        else
        {
            empty = true;
            externalPlugin = false;
        }
    }

    repaint();
}

void PluginSlot::setEffectName(const juce::String& name)
{
    effectName = name;
    empty = name.isEmpty();
    repaint();
}

void PluginSlot::setEmpty(bool isEmpty)
{
    empty = isEmpty;
    if (isEmpty)
    {
        effectName.clear();
        pluginInstance = nullptr;
        externalPlugin = false;
        bypassed = false;
    }
    repaint();
}

void PluginSlot::setBypassed(bool isBypassed)
{
    bypassed = isBypassed;
    repaint();
}

void PluginSlot::setIsExternalPlugin(bool isExternal)
{
    externalPlugin = isExternal;
    repaint();
}

//==============================================================================
// Private Methods
//==============================================================================

std::unique_ptr<PluginSlotDragData> PluginSlot::createDragData()
{
    auto data = std::make_unique<PluginSlotDragData>();
    data->sourceSlotIndex = slotIndex;
    data->sourceChannelIndex = channelIndex;
    data->sourceSlot = this;

    if (pluginInstance != nullptr)
    {
        data->pluginId = pluginInstance->getIdentifierString();
    }

    return data;
}

void PluginSlot::paintDropIndicator(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Draw a glowing border to indicate valid drop target
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.3f));
    g.fillRoundedRectangle(bounds.reduced(2.0f), 2.0f);

    // Draw arrow or indicator in center if slot is empty
    if (empty)
    {
        g.setColour(MidiSingLookAndFeel::accentColour);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("+", bounds, juce::Justification::centred);
    }
}

juce::Colour PluginSlot::getBackgroundColour() const
{
    if (empty)
    {
        return MidiSingLookAndFeel::sliderBackground;
    }
    else if (bypassed)
    {
        return MidiSingLookAndFeel::buttonBackground.darker(0.3f);
    }
    else if (externalPlugin)
    {
        // Slightly different colour for external plugins
        return MidiSingLookAndFeel::accentColour.darker(0.5f);
    }
    else
    {
        // Built-in effect
        return MidiSingLookAndFeel::accentColour.darker(0.4f);
    }
}
