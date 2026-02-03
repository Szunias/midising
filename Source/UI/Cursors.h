#pragma once

#include "ToolBar.h"
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Cursors provides mapping between ToolMode and appropriate JUCE MouseCursor types.
 * Each tool mode has a distinct cursor to provide visual feedback about the current editing mode.
 */
namespace Cursors
{
    /**
     * Returns the appropriate MouseCursor for the given ToolMode.
     *
     * Cursor mappings:
     * - Select:     NormalCursor - Standard pointer for selection and movement
     * - Draw:       CrosshairCursor - Precise positioning for creating regions
     * - Split:      UpDownResizeCursor - Visual indicator for splitting at position
     * - Erase:      CrosshairCursor - Precise targeting for deletion
     * - Pan:        DraggingHandCursor - Hand cursor for panning/scrolling
     * - Zoom:       NormalCursor - Standard cursor (zoom indicated by tool state)
     * - Automation: CrosshairCursor - Precise positioning for automation points
     * - Range:      IBeamCursor - Text-style cursor for selecting time ranges
     */
    inline juce::MouseCursor getCursorForTool(ToolMode mode)
    {
        switch (mode)
        {
            case ToolMode::Select:
                return juce::MouseCursor::NormalCursor;

            case ToolMode::Draw:
                return juce::MouseCursor::CrosshairCursor;

            case ToolMode::Split:
                // Vertical split indicator - using UpDownResizeCursor as visual hint
                // that something will be divided at this position
                return juce::MouseCursor::UpDownResizeCursor;

            case ToolMode::Erase:
                // Crosshair for precise deletion targeting
                return juce::MouseCursor::CrosshairCursor;

            case ToolMode::Pan:
                // Hand cursor for panning/scrolling the view
                return juce::MouseCursor::DraggingHandCursor;

            case ToolMode::Zoom:
                // Normal cursor for zoom (visual zoom feedback comes from tool state)
                // Could consider using PointingHandCursor if custom zoom cursor unavailable
                return juce::MouseCursor::NormalCursor;

            case ToolMode::Automation:
                // Crosshair for precise automation point placement
                return juce::MouseCursor::CrosshairCursor;

            case ToolMode::Range:
                // I-beam cursor for selecting time ranges (similar to text selection)
                return juce::MouseCursor::IBeamCursor;

            default:
                return juce::MouseCursor::NormalCursor;
        }
    }

    /**
     * Returns a descriptive name for the cursor used by a given tool.
     * Useful for tooltips or status bar messages.
     */
    inline juce::String getCursorDescription(ToolMode mode)
    {
        switch (mode)
        {
            case ToolMode::Select:
                return "Select Tool - Click to select, drag to move regions";

            case ToolMode::Draw:
                return "Draw Tool - Click and drag to create new regions";

            case ToolMode::Split:
                return "Split Tool - Click to split regions at cursor position";

            case ToolMode::Erase:
                return "Erase Tool - Click to delete regions";

            case ToolMode::Pan:
                return "Pan Tool - Click and drag to scroll the timeline";

            case ToolMode::Zoom:
                return "Zoom Tool - Click to zoom in, Alt+Click to zoom out";

            case ToolMode::Automation:
                return "Automation Tool - Click to add/edit automation points";

            case ToolMode::Range:
                return "Range Tool - Click and drag to select time ranges";

            default:
                return "";
        }
    }

    /**
     * Returns the keyboard shortcut key for a given tool mode.
     */
    inline char getShortcutKey(ToolMode mode)
    {
        switch (mode)
        {
            case ToolMode::Select:     return 'V';
            case ToolMode::Draw:       return 'D';
            case ToolMode::Split:      return 'S';
            case ToolMode::Erase:      return 'E';
            case ToolMode::Pan:        return 'H';
            case ToolMode::Zoom:       return 'Z';
            case ToolMode::Automation: return 'A';
            case ToolMode::Range:      return 'R';
            default:                   return ' ';
        }
    }

    /**
     * Returns the ToolMode corresponding to a keyboard shortcut key.
     * Returns Select if no matching key is found.
     */
    inline ToolMode getToolFromKey(char key)
    {
        switch (key)
        {
            case 'V': case 'v': return ToolMode::Select;
            case 'D': case 'd': return ToolMode::Draw;
            case 'S': case 's': return ToolMode::Split;
            case 'E': case 'e': return ToolMode::Erase;
            case 'H': case 'h': return ToolMode::Pan;
            case 'Z': case 'z': return ToolMode::Zoom;
            case 'A': case 'a': return ToolMode::Automation;
            case 'R': case 'r': return ToolMode::Range;
            default:           return ToolMode::Select;
        }
    }

} // namespace Cursors
