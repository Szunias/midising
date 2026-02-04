#pragma once

#include "../Timeline/AutomationLane.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <memory>

/**
 * Editing tools for automation lane interaction.
 */
enum class AutomationTool
{
    Draw,       // Create/modify automation points
    Select,     // Select and move existing points
    Erase       // Delete automation points
};

/**
 * AutomationLaneView displays and edits an automation lane.
 * Shows automation curve with points, supports drawing and editing.
 * Can be embedded in track lanes or used standalone.
 */
class AutomationLaneView : public juce::Component
{
public:
    AutomationLaneView();
    ~AutomationLaneView() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Mouse handling for automation editing
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    // Set the automation lane to display/edit
    void setAutomationLane(AutomationLane* lane);
    AutomationLane* getAutomationLane() const { return automationLane; }

    // View settings
    void setPixelsPerBeat(double ppb) { pixelsPerBeat = juce::jlimit(5.0, 200.0, ppb); repaint(); }
    double getPixelsPerBeat() const { return pixelsPerBeat; }

    // Horizontal offset for synchronization with parent views
    void setHorizontalScrollOffset(double offset) { horizontalScrollOffset = juce::jmax(0.0, offset); repaint(); }
    double getHorizontalScrollOffset() const { return horizontalScrollOffset; }

    // Sample rate for time/position calculations
    void setSampleRate(double rate) { sampleRate = juce::jmax(1.0, rate); repaint(); }
    double getSampleRate() const { return sampleRate; }

    // Tempo for beat-based calculations
    void setTempo(double bpm) { tempo = juce::jmax(1.0, bpm); repaint(); }
    double getTempo() const { return tempo; }

    // Tool selection
    void setTool(AutomationTool tool) { currentTool = tool; updateCursorForTool(); repaint(); }
    AutomationTool getTool() const { return currentTool; }

    // Selection operations
    void clearSelection();
    void deleteSelectedPoints();
    int getNumSelectedPoints() const { return static_cast<int>(selectedPointIndices.size()); }
    const std::set<size_t>& getSelectedPoints() const { return selectedPointIndices; }

    // Point operations
    void addPointAtPosition(int x, int y);
    void deletePointAtIndex(size_t index);

    // Curve type for newly created points
    void setDefaultCurveType(AutomationCurveType type) { defaultCurveType = type; }
    AutomationCurveType getDefaultCurveType() const { return defaultCurveType; }

    // Show/hide header with parameter name
    void setShowHeader(bool show) { showHeader = show; resized(); repaint(); }
    bool getShowHeader() const { return showHeader; }

    // Show/hide value readout while editing
    void setShowValueReadout(bool show) { showValueReadout = show; repaint(); }
    bool getShowValueReadout() const { return showValueReadout; }

    // Snap to grid settings
    void setSnapEnabled(bool enabled) { snapEnabled = enabled; }
    bool isSnapEnabled() const { return snapEnabled; }
    void setSnapResolution(double beatsPerSnap) { snapResolution = juce::jmax(0.0625, beatsPerSnap); }
    double getSnapResolution() const { return snapResolution; }

    // Callback when automation is modified
    std::function<void()> onAutomationChanged;

    // Layout constants
    static constexpr int HEADER_HEIGHT = 20;
    static constexpr int POINT_RADIUS = 5;
    static constexpr int POINT_HIT_RADIUS = 8;  // Larger hit area for easier clicking
    static constexpr int VALUE_LABEL_WIDTH = 40;
    static constexpr int MIN_HEIGHT = 40;

private:
    // Drawing helpers
    void drawBackground(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawGrid(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawAutomationCurve(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawAutomationPoints(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawValueReadout(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawHoverPreview(juce::Graphics& g, juce::Rectangle<int> bounds);

    // Coordinate conversion
    int64_t xToSamplePosition(int x) const;
    int samplePositionToX(int64_t position) const;
    double xToBeat(int x) const;
    int beatToX(double beat) const;
    float yToValue(int y, juce::Rectangle<int> bounds) const;
    int valueToY(float value, juce::Rectangle<int> bounds) const;

    // Hit testing
    int findPointAtPosition(int x, int y) const;
    juce::Rectangle<int> getAutomationBounds() const;

    // Cursor management
    void updateCursorForTool();
    void updateCursorForPosition(int x, int y);

    // Snap position to grid
    double snapBeatPosition(double beat) const;
    int64_t snapSamplePosition(int64_t position) const;

    // Tool-specific handlers
    void handleDrawToolMouseDown(const juce::MouseEvent& e);
    void handleDrawToolMouseDrag(const juce::MouseEvent& e);
    void handleDrawToolMouseUp(const juce::MouseEvent& e);
    void handleSelectToolMouseDown(const juce::MouseEvent& e);
    void handleSelectToolMouseDrag(const juce::MouseEvent& e);
    void handleSelectToolMouseUp(const juce::MouseEvent& e);
    void handleEraseToolMouseDown(const juce::MouseEvent& e);

    // Automation lane reference
    AutomationLane* automationLane = nullptr;

    // View settings
    double pixelsPerBeat = 20.0;
    double horizontalScrollOffset = 0.0;
    double sampleRate = 44100.0;
    double tempo = 120.0;

    // Tool state
    AutomationTool currentTool = AutomationTool::Draw;
    AutomationCurveType defaultCurveType = AutomationCurveType::Linear;

    // Selection state
    std::set<size_t> selectedPointIndices;

    // Dragging state
    bool isDragging = false;
    bool isDrawing = false;
    int dragPointIndex = -1;
    int64_t dragStartPosition = 0;
    float dragStartValue = 0.0f;
    juce::Point<int> lastDragPosition;

    // Box selection state (for Select tool)
    bool isBoxSelecting = false;
    juce::Point<int> boxSelectStart;
    juce::Point<int> boxSelectEnd;

    // Hover state
    int hoveredPointIndex = -1;
    juce::Point<int> hoverPosition;
    bool showHoverPreview = false;

    // Display options
    bool showHeader = true;
    bool showValueReadout = true;
    bool snapEnabled = true;
    double snapResolution = 0.25;  // Default to 1/16 notes

    // Value being edited (for readout display)
    float currentEditValue = 0.0f;
    int64_t currentEditPosition = 0;
    bool isShowingEditValue = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationLaneView)
};
