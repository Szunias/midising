#pragma once

#include "../Timeline/TempoTrack.h"
#include "../Audio/Transport.h"
#include "../Utils/UndoManager.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class TempoTrackView : public juce::Component
{
public:
    TempoTrackView();
    ~TempoTrackView() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    void setTempoTrack(TempoTrack* track) { tempoTrack = track; repaint(); }
    void setTransport(Transport* transport) { transportPtr = transport; }
    void setUndoManager(DAWUndoManager* um) { undoManager = um; }

    void setPixelsPerBeat(double ppb) { pixelsPerBeat = ppb; repaint(); }
    void setHorizontalScrollOffset(double offset) { horizontalScrollOffset = offset; repaint(); }
    void setSampleRate(double rate) { sampleRate = rate; }
    void setHeaderWidth(int w) { headerWidth = w; repaint(); }

    std::function<void(int64_t)> onJumpToPosition;

    static constexpr int LANE_HEIGHT = 30;

private:
    int sampleToPixel(int64_t samples) const;
    int64_t pixelToSample(int x) const;
    double bpmToY(double bpm) const;
    double yToBpm(int y) const;
    void showContextMenu(int eventIndex, juce::Point<int> position);
    void showEditBpmDialog(int eventIndex);
    int findEventAtPixel(int x) const;

    TempoTrack* tempoTrack = nullptr;
    Transport* transportPtr = nullptr;
    DAWUndoManager* undoManager = nullptr;

    double pixelsPerBeat = 20.0;
    double horizontalScrollOffset = 0.0;
    double sampleRate = 44100.0;
    int headerWidth = 150;

    // Dragging state
    bool isDraggingEvent = false;
    int dragEventIndex = -1;
    int64_t dragOriginalPosition = 0;
    double dragOriginalBpm = 120.0;
};
