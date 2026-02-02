#pragma once

#include "../Timeline/Timeline.h"
#include "../Audio/Transport.h"
#include "../Audio/AudioImporter.h"
#include "../MIDI/MidiImporter.h"
#include "../Audio/AudioTrack.h"
#include "../MIDI/MidiTrack.h"
#include "../Audio/WaveformCache.h"
#include "LookAndFeel.h"
#include "TrackHeader.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>

/**
 * TimelineView displays tracks, regions, time ruler, and playhead.
 * Main arrangement view of the DAW. Supports drag-and-drop for audio files.
 */
class TimelineView : public juce::Component,
                     public juce::Timer,
                     public juce::FileDragAndDropTarget
{
public:
    TimelineView();
    ~TimelineView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // Mouse handling
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // FileDragAndDropTarget interface
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;

    // Set references
    void setTimeline(Timeline* timeline) { timelinePtr = timeline; }
    void setTransport(Transport* transport) { transportPtr = transport; }
    void setMidiEngine(MidiEngine* engine) { midiEnginePtr = engine; }
    void setSampleRate(double rate) { sampleRate = rate; waveformCache.setSampleRate(rate); }

    // View settings
    void setPixelsPerBeat(double ppb) { pixelsPerBeat = juce::jlimit(5.0, 100.0, ppb); repaint(); }
    double getPixelsPerBeat() const { return pixelsPerBeat; }
    void setTrackHeight(int height) { trackHeight = juce::jmax(50, height); resized(); }
    int getTrackHeight() const { return trackHeight; }

    // Scrolling
    void setHorizontalScrollOffset(double offset) { horizontalScrollOffset = juce::jmax(0.0, offset); repaint(); }
    double getHorizontalScrollOffset() const { return horizontalScrollOffset; }

    // Zoom
    void zoomIn();
    void zoomOut();

    // Region selection
    Region* getSelectedRegion() const { return selectedRegion; }
    int getSelectedTrackIndex() const { return selectedTrackIndex; }
    void clearSelection();

    // Track header width
    static constexpr int HEADER_WIDTH = 150;
    static constexpr int RULER_HEIGHT = 30;

private:
    void drawTimeRuler(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawTrackLane(juce::Graphics& g, juce::Rectangle<int> bounds, int trackIndex);
    void drawPlayhead(juce::Graphics& g);
    void drawBeatGrid(juce::Graphics& g, juce::Rectangle<int> bounds);
    void updateTrackHeaders();
    
    int getTrackIndexAtY(int y) const;
    void importAudioFileToTrack(const juce::File& file, int trackIndex, int x);
    void importMidiFileToTrack(const juce::File& file, int trackIndex, int x);

    // Region hit testing
    Region* getRegionAtPosition(int x, int y, int& outTrackIndex) const;
    juce::Rectangle<int> getRegionBounds(Region* region, int trackIndex) const;

    int sampleToPixel(int64_t samples) const;
    int64_t pixelToSample(int x) const;
    double beatToPixel(double beats) const;
    double pixelToBeat(int x) const;

    Timeline* timelinePtr = nullptr;
    Transport* transportPtr = nullptr;

    std::vector<std::unique_ptr<TrackHeader>> trackHeaders;

    double pixelsPerBeat = 20.0;
    int trackHeight = 80;
    double horizontalScrollOffset = 0.0;
    double sampleRate = 44100.0;
    
    AudioImporter audioImporter;
    MidiImporter midiImporter;
    WaveformCache waveformCache;
    bool isDraggingFile = false;
    
    MidiEngine* midiEnginePtr = nullptr;

    // Region selection/hover state
    Region* selectedRegion = nullptr;
    int selectedTrackIndex = -1;
    Region* hoveredRegion = nullptr;
    int hoveredTrackIndex = -1;

    // Edge detection for region resize
    enum class RegionEdge { None, Left, Right };
    static constexpr int EDGE_DETECT_WIDTH = 8;  // Pixels from edge to detect resize

    // Region dragging state
    bool isDraggingRegion = false;
    int64_t dragStartSampleOffset = 0;  // Offset from mouse to region start
    int64_t dragOriginalPosition = 0;   // Original position before drag
    int64_t dragCurrentPosition = 0;    // Current dragged position (for visual feedback)
    bool snapToGrid = true;             // Enable grid snapping

    // Region edge resize state
    bool isResizingRegion = false;
    RegionEdge resizeEdge = RegionEdge::None;
    int64_t resizeOriginalStart = 0;    // Original start position before resize
    int64_t resizeOriginalLength = 0;   // Original length before resize
    int64_t resizeOriginalOffset = 0;   // Original offset before resize
    int64_t resizeCurrentStart = 0;     // Current start during resize
    int64_t resizeCurrentLength = 0;    // Current length during resize

    // Helper methods for region dragging and resizing
    int64_t snapPositionToGrid(int64_t samplePosition) const;
    void drawDragGhost(juce::Graphics& g);
    void drawResizeGhost(juce::Graphics& g);
    RegionEdge getRegionEdgeAtPosition(int x, int y, Region*& outRegion, int& outTrackIndex) const;
    void updateCursorForPosition(int x, int y);

    // Context menu and clipboard operations
    void showContextMenu(const juce::MouseEvent& e);
    void cutSelectedRegion();
    void copySelectedRegion();
    void pasteRegion(int64_t pastePosition);
    void deleteSelectedRegion();
    void splitSelectedRegion(int64_t splitPosition);

    // Clipboard data structure for copy/paste
    struct ClipboardData
    {
        bool hasData = false;
        bool isAudioRegion = true;  // true = audio, false = MIDI
        juce::String regionName;
        int64_t regionLength = 0;
        int64_t regionOffset = 0;
        // Audio region data
        juce::AudioBuffer<float> audioBuffer;
        juce::String filePath;
        int64_t thumbnailHash = 0;
        // MIDI region data
        juce::MidiMessageSequence midiSequence;
    };

    ClipboardData clipboard;
    int64_t lastClickSamplePosition = 0;  // For paste position

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineView)
};
