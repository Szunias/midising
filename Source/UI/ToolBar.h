#pragma once

#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

/**
 * ToolMode defines the available editing tools in the DAW.
 * Each mode changes cursor appearance and mouse interaction behavior.
 */
enum class ToolMode
{
    Select,     // V - Default selection and movement tool
    Draw,       // D - Create new regions
    Split,      // S - Split regions at click position
    Erase,      // E - Delete regions
    Pan,        // H - Pan/scroll the timeline view
    Zoom,       // Z - Zoom in/out on timeline
    Automation, // A - Edit automation curves
    Range       // R - Select time ranges
};

/**
 * ToolBar provides editing tool selection buttons: Select, Draw, Split, Erase, Pan, Zoom, Automation, Range.
 * Positioned below the TransportBar, it allows users to switch between different editing modes.
 */
class ToolBar : public juce::Component
{
public:
    ToolBar();
    ~ToolBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Get the currently selected tool
    ToolMode getCurrentTool() const { return currentTool; }

    // Set the current tool mode
    void setToolMode(ToolMode mode);

    // Callback when tool mode changes
    std::function<void(ToolMode)> onToolChange;

private:
    void updateButtonStates();

    // Tool buttons - fixed 40x30 size
    juce::TextButton selectButton { "V" };
    juce::TextButton drawButton { "D" };
    juce::TextButton splitButton { "S" };
    juce::TextButton eraseButton { "E" };
    juce::TextButton panButton { "H" };
    juce::TextButton zoomButton { "Z" };
    juce::TextButton automationButton { "A" };
    juce::TextButton rangeButton { "R" };

    ToolMode currentTool = ToolMode::Select;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolBar)
};
