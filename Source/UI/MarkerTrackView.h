#pragma once

#include "../Timeline/MarkerTrack.h"
#include "../Audio/Transport.h"
#include "../Utils/UndoManager.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class MarkerTrackView : public juce::Component
{
public:
    MarkerTrackView();
    ~MarkerTrackView() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    void setMarkerTrack(MarkerTrack* track) { markerTrack = track; repaint(); }
    void setTransport(Transport* transport) { transportPtr = transport; }
    void setUndoManager(DAWUndoManager* um) { undoManager = um; }

    void setPixelsPerBeat(double ppb) { pixelsPerBeat = ppb; repaint(); }
    void setHorizontalScrollOffset(double offset) { horizontalScrollOffset = offset; repaint(); }
    void setSampleRate(double rate) { sampleRate = rate; }
    void setHeaderWidth(int w) { headerWidth = w; repaint(); }

    std::function<void(int64_t)> onJumpToPosition;

    static constexpr int LANE_HEIGHT = 24;

private:
    int sampleToPixel(int64_t samples) const;
    int64_t pixelToSample(int x) const;
    void showContextMenu(int markerIndex, juce::Point<int> position);
    void showRenameDialog(int markerIndex);
    void showColourMenu(int markerIndex);

    MarkerTrack* markerTrack = nullptr;
    Transport* transportPtr = nullptr;
    DAWUndoManager* undoManager = nullptr;

    double pixelsPerBeat = 20.0;
    double horizontalScrollOffset = 0.0;
    double sampleRate = 44100.0;
    int headerWidth = 150;

    // Dragging state
    bool isDraggingMarker = false;
    int dragMarkerIndex = -1;
    int64_t dragOriginalPosition = 0;
};
