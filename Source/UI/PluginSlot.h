#pragma once

#include "../Plugins/PluginInstance.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// Forward declarations
class PluginSlot;

/**
 * Data transferred during plugin slot drag operations.
 * Contains information about the source slot and plugin.
 */
struct PluginSlotDragData
{
    /** The slot index being dragged from */
    int sourceSlotIndex = -1;

    /** The channel/track index (for cross-channel drag) */
    int sourceChannelIndex = -1;

    /** Pointer to the source PluginSlot component */
    PluginSlot* sourceSlot = nullptr;

    /** Description of unique identifier for recreating */
    juce::String pluginId;
};

/**
 * PluginSlot represents a single plugin slot in a channel strip.
 * Enhanced version of InsertSlotButton with full drag-and-drop support.
 *
 * Features:
 *   - Drag-drop reordering between slots within same channel
 *   - Drag-drop copy/move between different channels
 *   - Visual feedback during drag operations
 *   - Click to open plugin selector popup
 *   - Right-click context menu for bypass/remove
 *   - Support for external VST3/AU plugins via PluginInstance
 *
 * Signal flow position: Insert slot in channel strip effect chain
 */
class PluginSlot : public juce::Component,
                   public juce::DragAndDropTarget
{
public:
    /** Drag identifier for plugin slot drag operations */
    static constexpr const char* DRAG_ID = "PluginSlotDrag";

    //==========================================================================
    // Construction/Destruction
    //==========================================================================

    /**
     * Create a plugin slot with the given index.
     * @param slotIndex The zero-based slot index
     * @param channelIndex Optional channel index for cross-channel operations
     */
    PluginSlot(int slotIndex, int channelIndex = -1);

    /**
     * Destructor.
     */
    ~PluginSlot() override = default;

    //==========================================================================
    // Component Overrides
    //==========================================================================

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    //==========================================================================
    // DragAndDropTarget Overrides
    //==========================================================================

    /**
     * Check if this slot can accept a drag.
     * @param dragSourceDetails Details about the drag source
     * @return true if the drag can be accepted
     */
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;

    /**
     * Called when a drag enters this component.
     * @param dragSourceDetails Details about the drag source
     */
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;

    /**
     * Called when a drag moves within this component.
     * @param dragSourceDetails Details about the drag source
     */
    void itemDragMove(const SourceDetails& dragSourceDetails) override;

    /**
     * Called when a drag exits this component.
     * @param dragSourceDetails Details about the drag source
     */
    void itemDragExit(const SourceDetails& dragSourceDetails) override;

    /**
     * Called when a drag is dropped on this component.
     * @param dragSourceDetails Details about the drag source
     */
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    //==========================================================================
    // Slot State
    //==========================================================================

    /**
     * Set the plugin instance for this slot.
     * @param instance The plugin instance (nullptr to clear)
     */
    void setPluginInstance(PluginInstance* instance);

    /**
     * Get the plugin instance for this slot.
     * @return The plugin instance, or nullptr if empty
     */
    PluginInstance* getPluginInstance() const { return pluginInstance; }

    /**
     * Set the effect/plugin name to display (for built-in effects).
     * @param name The name to display
     */
    void setEffectName(const juce::String& name);

    /**
     * Get the displayed effect name.
     * @return The effect name
     */
    const juce::String& getEffectName() const { return effectName; }

    /**
     * Set whether this slot is empty.
     * @param isEmpty true if empty
     */
    void setEmpty(bool isEmpty);

    /**
     * Check if this slot is empty.
     * @return true if empty
     */
    bool isEmpty() const { return empty; }

    /**
     * Set the bypass state.
     * @param bypassed true to show as bypassed
     */
    void setBypassed(bool bypassed);

    /**
     * Check if this slot is bypassed.
     * @return true if bypassed
     */
    bool isBypassed() const { return bypassed; }

    /**
     * Set whether this slot is an external plugin (VST3/AU).
     * @param isExternal true for external plugins
     */
    void setIsExternalPlugin(bool isExternal);

    /**
     * Check if this is an external plugin slot.
     * @return true if external plugin
     */
    bool isExternalPlugin() const { return externalPlugin; }

    /**
     * Get the slot index.
     * @return The zero-based slot index
     */
    int getSlotIndex() const { return slotIndex; }

    /**
     * Get the channel index.
     * @return The channel index, or -1 if not set
     */
    int getChannelIndex() const { return channelIndex; }

    /**
     * Set the channel index.
     * @param index The channel index
     */
    void setChannelIndex(int index) { channelIndex = index; }

    /**
     * Enable or disable drag-and-drop for this slot.
     * @param enabled true to enable drag-drop
     */
    void setDragDropEnabled(bool enabled) { dragDropEnabled = enabled; }

    /**
     * Check if drag-drop is enabled.
     * @return true if enabled
     */
    bool isDragDropEnabled() const { return dragDropEnabled; }

    //==========================================================================
    // Callbacks
    //==========================================================================

    /** Called when the slot is left-clicked (opens plugin selector) */
    std::function<void(int slotIndex)> onClicked;

    /** Called when the slot is right-clicked (shows context menu) */
    std::function<void(int slotIndex, const juce::Point<int>& position)> onRightClicked;

    /** Called when a plugin is dropped on this slot from another slot */
    std::function<void(int targetSlotIndex, int sourceSlotIndex, int sourceChannelIndex)> onPluginDropped;

    /** Called when the plugin editor window should be opened */
    std::function<void(int slotIndex)> onOpenEditorRequested;

    /** Called when bypass state should be toggled */
    std::function<void(int slotIndex)> onBypassToggled;

    /** Called when plugin should be removed */
    std::function<void(int slotIndex)> onRemoveRequested;

private:
    //==========================================================================
    // Member Variables
    //==========================================================================

    /** The slot index (0-based) */
    int slotIndex;

    /** The channel index for cross-channel operations */
    int channelIndex;

    /** Pointer to the plugin instance (not owned) */
    PluginInstance* pluginInstance = nullptr;

    /** The displayed effect name */
    juce::String effectName;

    /** Whether this slot is empty */
    bool empty = true;

    /** Whether this slot is bypassed */
    bool bypassed = false;

    /** Whether this is an external VST3/AU plugin */
    bool externalPlugin = false;

    /** Whether drag-drop is enabled */
    bool dragDropEnabled = true;

    /** Mouse hover state */
    bool mouseOver = false;

    /** Whether a drag is currently over this slot */
    bool dragOver = false;

    /** Whether this slot is being dragged */
    bool isDragging = false;

    /** Starting position for drag detection */
    juce::Point<int> dragStartPosition;

    /** Minimum distance before drag starts (in pixels) */
    static constexpr int DRAG_THRESHOLD = 5;

    //==========================================================================
    // Private Methods
    //==========================================================================

    /**
     * Create drag data for this slot.
     * @return Unique pointer to the drag data
     */
    std::unique_ptr<PluginSlotDragData> createDragData();

    /**
     * Paint the drop indicator during drag operations.
     * @param g The graphics context
     * @param bounds The bounds to paint in
     */
    void paintDropIndicator(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Get the background colour based on current state.
     * @return The background colour
     */
    juce::Colour getBackgroundColour() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSlot)
};
