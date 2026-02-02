#include "TimelineView.h"
#include "../Utils/TimeConversion.h"
#include <cmath>

TimelineView::TimelineView()
{
    startTimer(30); // ~30 FPS for playhead updates
}

TimelineView::~TimelineView()
{
    stopTimer();
}

void TimelineView::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark);

    if (timelinePtr == nullptr)
        return;

    auto bounds = getLocalBounds();

    // Draw time ruler at top
    auto rulerBounds = bounds.removeFromTop(RULER_HEIGHT);
    rulerBounds.removeFromLeft(HEADER_WIDTH);
    drawTimeRuler(g, rulerBounds);

    // Draw track lanes
    for (int i = 0; i < timelinePtr->getNumTracks(); ++i)
    {
        auto laneBounds = bounds.removeFromTop(trackHeight);
        auto headerBounds = laneBounds.removeFromLeft(HEADER_WIDTH);
        juce::ignoreUnused(headerBounds); // Headers are separate components

        drawTrackLane(g, laneBounds, i);
    }

    // Draw drag ghost for visual feedback during region dragging
    drawDragGhost(g);

    // Draw resize ghost for visual feedback during edge resizing
    drawResizeGhost(g);

    // Draw playhead on top of everything
    drawPlayhead(g);
}

void TimelineView::resized()
{
    updateTrackHeaders();
}

void TimelineView::timerCallback()
{
    // Repaint for playhead animation
    if (transportPtr != nullptr && !transportPtr->isStopped())
    {
        repaint();
    }
}

void TimelineView::mouseDown(const juce::MouseEvent& e)
{
    if (timelinePtr == nullptr || transportPtr == nullptr)
        return;

    // Store click position for paste operations
    lastClickSamplePosition = pixelToSample(e.x);

    // Handle right-click for context menu
    if (e.mods.isPopupMenu())
    {
        int trackIndex = -1;
        Region* region = getRegionAtPosition(e.x, e.y, trackIndex);

        if (region != nullptr)
        {
            // Select the clicked region first
            selectedRegion = region;
            selectedTrackIndex = trackIndex;
            repaint();
        }

        // Show context menu (works on regions or empty timeline area)
        showContextMenu(e);
        return;
    }

    // First, check if clicking on a region edge for resizing
    int trackIndex = -1;
    Region* edgeRegion = nullptr;
    RegionEdge edge = getRegionEdgeAtPosition(e.x, e.y, edgeRegion, trackIndex);

    if (edge != RegionEdge::None && edgeRegion != nullptr)
    {
        // Start edge resize operation
        selectedRegion = edgeRegion;
        selectedTrackIndex = trackIndex;
        isResizingRegion = true;
        resizeEdge = edge;
        resizeOriginalStart = edgeRegion->getStartPosition();
        resizeOriginalLength = edgeRegion->getLength();
        resizeOriginalOffset = edgeRegion->getOffset();
        resizeCurrentStart = resizeOriginalStart;
        resizeCurrentLength = resizeOriginalLength;
        repaint();
        return;
    }

    // Check if clicking on a region (for dragging)
    Region* region = getRegionAtPosition(e.x, e.y, trackIndex);

    if (region != nullptr)
    {
        // Select the clicked region
        selectedRegion = region;
        selectedTrackIndex = trackIndex;

        // Start dragging the region
        isDraggingRegion = true;
        dragOriginalPosition = region->getStartPosition();
        dragCurrentPosition = dragOriginalPosition;

        // Calculate offset from mouse position to region start
        int64_t mouseSample = pixelToSample(e.x);
        dragStartSampleOffset = mouseSample - dragOriginalPosition;

        repaint();
        return;
    }

    // Click on timeline area (not on a region) - set playhead and clear selection
    if (e.x > HEADER_WIDTH && e.y > RULER_HEIGHT)
    {
        clearSelection();
        int64_t newPos = pixelToSample(e.x);
        transportPtr->setPlayheadPosition(newPos);
        repaint();
    }
}

void TimelineView::mouseDrag(const juce::MouseEvent& e)
{
    // Handle region edge resizing
    if (isResizingRegion && selectedRegion != nullptr)
    {
        int64_t mouseSample = pixelToSample(e.x);

        if (resizeEdge == RegionEdge::Left)
        {
            // Resizing from left edge - changes start position and length
            int64_t newStart = mouseSample;

            // Snap to grid if enabled
            if (snapToGrid)
            {
                newStart = snapPositionToGrid(newStart);
            }

            // Clamp to non-negative
            newStart = juce::jmax(int64_t(0), newStart);

            // Don't allow moving start past original end position
            int64_t originalEnd = resizeOriginalStart + resizeOriginalLength;
            newStart = juce::jmin(newStart, originalEnd - 1);

            // Calculate new length and offset change
            int64_t deltaStart = newStart - resizeOriginalStart;
            int64_t newLength = resizeOriginalLength - deltaStart;

            // Ensure minimum length
            newLength = juce::jmax(int64_t(1), newLength);

            resizeCurrentStart = newStart;
            resizeCurrentLength = newLength;
        }
        else if (resizeEdge == RegionEdge::Right)
        {
            // Resizing from right edge - only changes length
            int64_t newEnd = mouseSample;

            // Snap to grid if enabled
            if (snapToGrid)
            {
                newEnd = snapPositionToGrid(newEnd);
            }

            // Calculate new length
            int64_t newLength = newEnd - resizeOriginalStart;

            // Ensure minimum length
            newLength = juce::jmax(int64_t(1), newLength);

            resizeCurrentStart = resizeOriginalStart;
            resizeCurrentLength = newLength;
        }

        repaint();
        return;
    }

    // Handle region dragging
    if (isDraggingRegion && selectedRegion != nullptr)
    {
        // Calculate new position based on mouse, accounting for initial offset
        int64_t mouseSample = pixelToSample(e.x);
        int64_t newPosition = mouseSample - dragStartSampleOffset;

        // Clamp to non-negative
        newPosition = juce::jmax(int64_t(0), newPosition);

        // Snap to grid if enabled
        if (snapToGrid)
        {
            newPosition = snapPositionToGrid(newPosition);
        }

        // Update drag position for visual feedback
        dragCurrentPosition = newPosition;
        repaint();
        return;
    }

    // If no region is being dragged, allow playhead scrubbing
    if (transportPtr != nullptr && e.x > HEADER_WIDTH)
    {
        int64_t newPos = pixelToSample(e.x);
        transportPtr->setPlayheadPosition(juce::jmax(int64_t(0), newPos));
        repaint();
    }
}

void TimelineView::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    // Finalize region edge resize
    if (isResizingRegion && selectedRegion != nullptr)
    {
        // Apply the new start position and length
        if (resizeEdge == RegionEdge::Left)
        {
            // When resizing from left, we also need to adjust the offset
            int64_t deltaStart = resizeCurrentStart - resizeOriginalStart;
            int64_t newOffset = resizeOriginalOffset + deltaStart;

            // Clamp offset to non-negative
            newOffset = juce::jmax(int64_t(0), newOffset);

            selectedRegion->setOffset(newOffset);
            selectedRegion->setStartPosition(resizeCurrentStart);
        }

        selectedRegion->setLength(resizeCurrentLength);

        isResizingRegion = false;
        resizeEdge = RegionEdge::None;
        repaint();
        return;
    }

    // Finalize region drag
    if (isDraggingRegion && selectedRegion != nullptr)
    {
        // Apply the new position to the region
        selectedRegion->setStartPosition(dragCurrentPosition);
        isDraggingRegion = false;
        repaint();
        return;
    }

    isDraggingRegion = false;
    isResizingRegion = false;
}

void TimelineView::mouseMove(const juce::MouseEvent& e)
{
    int trackIndex = -1;
    Region* region = getRegionAtPosition(e.x, e.y, trackIndex);

    // Update hover state if changed
    if (region != hoveredRegion || trackIndex != hoveredTrackIndex)
    {
        hoveredRegion = region;
        hoveredTrackIndex = trackIndex;
        repaint();
    }

    // Update cursor based on position (edge detection for resize)
    updateCursorForPosition(e.x, e.y);
}

void TimelineView::mouseExit(const juce::MouseEvent& /*e*/)
{
    // Clear hover state when mouse leaves component
    if (hoveredRegion != nullptr || hoveredTrackIndex != -1)
    {
        hoveredRegion = nullptr;
        hoveredTrackIndex = -1;
        repaint();
    }

    // Reset cursor
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void TimelineView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown())
    {
        // Zoom with Ctrl+wheel
        if (wheel.deltaY > 0)
            zoomIn();
        else if (wheel.deltaY < 0)
            zoomOut();
    }
    else
    {
        // Horizontal scroll
        horizontalScrollOffset -= wheel.deltaY * 100.0;
        horizontalScrollOffset = juce::jmax(0.0, horizontalScrollOffset);
        repaint();
    }
}

void TimelineView::zoomIn()
{
    setPixelsPerBeat(pixelsPerBeat * 1.2);
}

void TimelineView::zoomOut()
{
    setPixelsPerBeat(pixelsPerBeat / 1.2);
}

void TimelineView::drawTimeRuler(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(MidiSingLookAndFeel::backgroundMid);
    g.fillRect(bounds);

    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawLine(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getBottom()),
               static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom()));

    if (timelinePtr == nullptr)
        return;

    // Draw beat markers
    g.setFont(10.0f);
    int beatsPerBar = timelinePtr->getBeatsPerBar();

    double startBeat = horizontalScrollOffset / pixelsPerBeat;
    double endBeat = startBeat + bounds.getWidth() / pixelsPerBeat;

    for (int beat = static_cast<int>(startBeat); beat <= static_cast<int>(endBeat) + 1; ++beat)
    {
        int x = bounds.getX() + static_cast<int>(beatToPixel(beat) - horizontalScrollOffset);
        
        if (x < bounds.getX() || x > bounds.getRight())
            continue;

        bool isBarStart = (beat % beatsPerBar) == 0;

        if (isBarStart)
        {
            // Bar line
            g.setColour(MidiSingLookAndFeel::textColour);
            g.drawLine(static_cast<float>(x), static_cast<float>(bounds.getY()),
                       static_cast<float>(x), static_cast<float>(bounds.getBottom()), 1.0f);

            // Bar number
            int barNumber = (beat / beatsPerBar) + 1;
            g.drawText(juce::String(barNumber), x + 2, bounds.getY(), 30, bounds.getHeight(),
                       juce::Justification::centredLeft);
        }
        else
        {
            // Beat tick
            g.setColour(MidiSingLookAndFeel::textDimColour);
            int tickHeight = bounds.getHeight() / 3;
            g.drawLine(static_cast<float>(x), static_cast<float>(bounds.getBottom() - tickHeight),
                       static_cast<float>(x), static_cast<float>(bounds.getBottom()), 0.5f);
        }
    }
}

void TimelineView::drawTrackLane(juce::Graphics& g, juce::Rectangle<int> bounds, int trackIndex)
{
    if (timelinePtr == nullptr)
        return;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return;

    // Track background
    g.setColour(MidiSingLookAndFeel::backgroundLight);
    g.fillRect(bounds);

    // Draw beat grid
    drawBeatGrid(g, bounds);

    // Bottom border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawLine(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getBottom()),
               static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom()));

    // Draw regions
    if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
    {
        for (int i = 0; i < audioTrack->getNumRegions(); ++i)
        {
            auto* region = audioTrack->getRegion(i);
            
            // Calculate pixel bounds
            int64_t startSample = region->getStartPosition();
            int64_t endSample = region->getEndPosition();
            
            int x1 = bounds.getX() + sampleToPixel(startSample) - static_cast<int>(horizontalScrollOffset);
            int x2 = bounds.getX() + sampleToPixel(endSample) - static_cast<int>(horizontalScrollOffset);
            int w = x2 - x1;
            
            if (x2 < bounds.getX() || x1 > bounds.getRight())
                continue;
                
            juce::Rectangle<int> regionBounds(x1, bounds.getY() + 2, w, bounds.getHeight() - 4);

            // Determine if this region is selected or hovered
            bool isSelected = (region == selectedRegion && trackIndex == selectedTrackIndex);
            bool isHovered = (region == hoveredRegion && trackIndex == hoveredTrackIndex);

            // Draw region box with appropriate colour
            juce::Colour fillColour = MidiSingLookAndFeel::regionColour;
            if (isSelected)
                fillColour = fillColour.brighter(0.3f);
            else if (isHovered)
                fillColour = fillColour.brighter(0.15f);

            g.setColour(fillColour);
            g.fillRect(regionBounds);

            // Draw border - brighter for selected regions
            if (isSelected)
                g.setColour(MidiSingLookAndFeel::accentColour);
            else
                g.setColour(MidiSingLookAndFeel::borderColour);
            g.drawRect(regionBounds, isSelected ? 2 : 1);

            // Draw waveform
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            
            int64_t hash = region->getThumbnailHash();
            if (hash == 0)
            {
                hash = waveformCache.addAudioBuffer(region->getAudioBuffer());
                region->setThumbnailHash(hash);
            }
            
            auto& thumb = waveformCache.getThumbnail(hash);
            
            double thumbStart = static_cast<double>(region->getOffset()) / sampleRate;
            double thumbEnd = thumbStart + static_cast<double>(region->getLength()) / sampleRate;
            
            thumb.drawChannel(g, regionBounds.reduced(1), thumbStart, thumbEnd, 0, 1.0f);
            
            // Draw name
            g.setColour(juce::Colours::white);
            g.drawText(region->getName(), regionBounds.reduced(2), juce::Justification::topLeft, true);
        }
    }
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
    {
        for (int i = 0; i < midiTrack->getNumRegions(); ++i)
        {
            auto* region = midiTrack->getRegion(i);
            
            // Calculate pixel bounds
            int64_t startSample = region->getStartPosition();
            int64_t endSample = region->getEndPosition();
            
            int x1 = bounds.getX() + sampleToPixel(startSample) - static_cast<int>(horizontalScrollOffset);
            int x2 = bounds.getX() + sampleToPixel(endSample) - static_cast<int>(horizontalScrollOffset);
            int w = x2 - x1;
            
            if (x2 < bounds.getX() || x1 > bounds.getRight())
                continue;
                
            juce::Rectangle<int> regionBounds(x1, bounds.getY() + 2, w, bounds.getHeight() - 4);

            // Determine if this region is selected or hovered
            bool isSelected = (region == selectedRegion && trackIndex == selectedTrackIndex);
            bool isHovered = (region == hoveredRegion && trackIndex == hoveredTrackIndex);

            // Draw region box with appropriate colour
            juce::Colour fillColour = MidiSingLookAndFeel::regionColour.withHue(0.1f); // Different hue for MIDI
            if (isSelected)
                fillColour = fillColour.brighter(0.3f);
            else if (isHovered)
                fillColour = fillColour.brighter(0.15f);

            g.setColour(fillColour);
            g.fillRect(regionBounds);

            // Draw border - brighter for selected regions
            if (isSelected)
                g.setColour(MidiSingLookAndFeel::accentColour);
            else
                g.setColour(MidiSingLookAndFeel::borderColour);
            g.drawRect(regionBounds, isSelected ? 2 : 1);

            // Draw MIDI notes preview
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            const auto& sequence = region->getMidiSequence();
            
            double regionHeight = static_cast<double>(regionBounds.getHeight());
            double noteHeight = juce::jmax(2.0, regionHeight / 24.0); // Rough approximation
            
            // Calculate relative offset of the region
            int64_t regionOffset = region->getOffset();

            // Iterate through events
            for (int e = 0; e < sequence.getNumEvents(); ++e)
            {
                auto* event = sequence.getEventPointer(e);
                if (event->message.isNoteOn())
                {
                     // Time is in samples for MidiTrack logic, but in seconds or ticks in raw sequence?
                     // Verify: imported sequence has timestamps converted to samples in previous step?
                     // YES, importTrack converted seconds * sampleRate.
                     
                     // However, MidiTrack::processBlock adds regionStart + event->message.getTimeStamp().
                     // So event->message.getTimeStamp() is relative to region start IN SAMPLES.
                     
                     int64_t eventTime = static_cast<int64_t>(event->message.getTimeStamp());
                     int64_t noteStart = eventTime - regionOffset;
                     
                     // Find matching note off?
                     // For preview, fixed length or simple duration is consistent?
                     // MidiMessageSequence has paired events if matches mapped.
                     // But drawing NoteOns is simpler. Let's try to get length.
                     
                     double noteLengthSamples = 0;
                     // Rough check for note length or assume default for now to be safe/fast
                     // Really we should iterate matches pairs if updated.
                     // But sequence is straight list.
                     
                     // Just draw simple dots/lines for note ons
                     int noteX = x1 + sampleToPixel(noteStart) - sampleToPixel(0);
                     int noteY = regionBounds.getY() + static_cast<int>(regionHeight * (1.0 - (event->message.getNoteNumber() / 128.0)));
                     
                     g.fillRect(noteX, noteY, 4, 2);
                }
            }
            
            // Draw name
            g.setColour(juce::Colours::white);
            g.drawText(region->getName(), regionBounds.reduced(2), juce::Justification::topLeft, true);
        }
    }

    // Track is muted - draw overlay
    if (track->isMuted())
    {
        g.setColour(juce::Colour(0x40000000));
        g.fillRect(bounds);
    }
}

void TimelineView::drawBeatGrid(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (timelinePtr == nullptr)
        return;

    int beatsPerBar = timelinePtr->getBeatsPerBar();
    double startBeat = horizontalScrollOffset / pixelsPerBeat;
    double endBeat = startBeat + bounds.getWidth() / pixelsPerBeat;

    for (int beat = static_cast<int>(startBeat); beat <= static_cast<int>(endBeat) + 1; ++beat)
    {
        int x = bounds.getX() + static_cast<int>(beatToPixel(beat) - horizontalScrollOffset);
        
        if (x < bounds.getX() || x > bounds.getRight())
            continue;

        bool isBarStart = (beat % beatsPerBar) == 0;
        g.setColour(isBarStart ? MidiSingLookAndFeel::borderColour 
                               : MidiSingLookAndFeel::borderColour.withAlpha(0.3f));
        g.drawLine(static_cast<float>(x), static_cast<float>(bounds.getY()),
                   static_cast<float>(x), static_cast<float>(bounds.getBottom()),
                   isBarStart ? 1.0f : 0.5f);
    }
}

void TimelineView::drawPlayhead(juce::Graphics& g)
{
    if (transportPtr == nullptr || timelinePtr == nullptr)
        return;

    int64_t pos = transportPtr->getPlayheadPosition();
    int x = HEADER_WIDTH + sampleToPixel(pos) - static_cast<int>(horizontalScrollOffset);

    if (x < HEADER_WIDTH || x > getWidth())
        return;

    // Playhead triangle at top
    juce::Path triangle;
    triangle.addTriangle(static_cast<float>(x - 5), static_cast<float>(RULER_HEIGHT),
                         static_cast<float>(x + 5), static_cast<float>(RULER_HEIGHT),
                         static_cast<float>(x), static_cast<float>(RULER_HEIGHT - 8));

    g.setColour(MidiSingLookAndFeel::accentColour);
    g.fillPath(triangle);

    // Playhead line
    g.drawLine(static_cast<float>(x), static_cast<float>(RULER_HEIGHT),
               static_cast<float>(x), static_cast<float>(getHeight()), 2.0f);
}

void TimelineView::updateTrackHeaders()
{
    if (timelinePtr == nullptr)
        return;

    // Ensure we have the right number of track headers
    while (static_cast<int>(trackHeaders.size()) < timelinePtr->getNumTracks())
    {
        auto header = std::make_unique<TrackHeader>();
        addAndMakeVisible(header.get());
        trackHeaders.push_back(std::move(header));
    }

    while (static_cast<int>(trackHeaders.size()) > timelinePtr->getNumTracks())
    {
        trackHeaders.pop_back();
    }

    // Position and configure headers
    int y = RULER_HEIGHT;
    for (size_t i = 0; i < trackHeaders.size(); ++i)
    {
        trackHeaders[i]->setBounds(0, y, HEADER_WIDTH, trackHeight);
        trackHeaders[i]->setTrack(timelinePtr->getTrack(static_cast<int>(i)));
        y += trackHeight;
    }
}

int TimelineView::sampleToPixel(int64_t samples) const
{
    if (timelinePtr == nullptr)
        return 0;

    double beats = timelinePtr->samplesToBeats(samples);
    return static_cast<int>(beats * pixelsPerBeat);
}

int64_t TimelineView::pixelToSample(int x) const
{
    if (timelinePtr == nullptr)
        return 0;

    double beats = (x - HEADER_WIDTH + horizontalScrollOffset) / pixelsPerBeat;
    return timelinePtr->beatsToSamples(beats);
}

double TimelineView::beatToPixel(double beats) const
{
    return beats * pixelsPerBeat;
}

double TimelineView::pixelToBeat(int x) const
{
    return (x - HEADER_WIDTH + horizontalScrollOffset) / pixelsPerBeat;
}

bool TimelineView::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& file : files)
    {
        if (audioImporter.isSupported(juce::File(file)) || midiImporter.isSupported(juce::File(file)))
            return true;
    }
    return false;
}

void TimelineView::filesDropped(const juce::StringArray& files, int x, int y)
{
    isDraggingFile = false;
    repaint();
    
    if (timelinePtr == nullptr)
        return;

    int trackIndex = getTrackIndexAtY(y);
    
    for (const auto& filePath : files)
    {
        juce::File file(filePath);
        if (audioImporter.isSupported(file))
        {
            importAudioFileToTrack(file, trackIndex, x);
        }
        else if (midiImporter.isSupported(file))
        {
            importMidiFileToTrack(file, trackIndex, x);
        }
    }
}

void TimelineView::fileDragEnter(const juce::StringArray& /*files*/, int /*x*/, int /*y*/)
{
    isDraggingFile = true;
    repaint();
}

void TimelineView::fileDragExit(const juce::StringArray& /*files*/)
{
    isDraggingFile = false;
    repaint();
}

int TimelineView::getTrackIndexAtY(int y) const
{
    if (timelinePtr == nullptr || y < RULER_HEIGHT)
        return -1;
    
    int trackY = RULER_HEIGHT;
    for (int i = 0; i < timelinePtr->getNumTracks(); ++i)
    {
        if (y >= trackY && y < trackY + trackHeight)
            return i;
        trackY += trackHeight;
    }
    
    // If below all tracks, return last track index
    if (timelinePtr->getNumTracks() > 0)
        return timelinePtr->getNumTracks() - 1;
    
    return -1;
}

void TimelineView::importAudioFileToTrack(const juce::File& file, int trackIndex, int x)
{
    if (timelinePtr == nullptr)
        return;
    
    // Get or create target track
    Track* track = nullptr;
    if (trackIndex >= 0 && trackIndex < timelinePtr->getNumTracks())
    {
        track = timelinePtr->getTrack(trackIndex);
    }
    else
    {
        // Create new track if none exists
        auto* newTrack = new AudioTrack(file.getFileNameWithoutExtension());
        newTrack->setColour(juce::Colour::fromHSV(static_cast<float>(timelinePtr->getNumTracks()) * 0.15f, 0.6f, 0.8f, 1.0f));
        timelinePtr->addTrack(newTrack);
        track = newTrack;
        updateTrackHeaders();
    }
    
    // Must be an audio track
    auto* audioTrack = dynamic_cast<AudioTrack*>(track);
    if (audioTrack == nullptr)
        return;
    
    // Calculate start position from drop location
    int64_t startPosition = pixelToSample(x);
    if (startPosition < 0) startPosition = 0;
    
    // Load audio file and create region
    auto buffer = audioImporter.loadFile(file, timelinePtr->getSampleRate());
    if (buffer == nullptr)
        return;
    
    auto region = std::make_unique<AudioRegion>(startPosition, buffer->getNumSamples());
    region->setAudioBuffer(*buffer);
    region->setName(file.getFileNameWithoutExtension());
    region->setFilePath(file.getFullPathName());
    
    audioTrack->addRegion(std::move(region));
    repaint();
}

void TimelineView::importMidiFileToTrack(const juce::File& file, int trackIndex, int x)
{
    if (timelinePtr == nullptr) return;

    // Calculate start position
    int64_t startSample = pixelToSample(x - HEADER_WIDTH + static_cast<int>(horizontalScrollOffset));
    startSample = juce::jmax(int64_t(0), startSample);

    // Get or create track
    MidiTrack* track = nullptr;

    if (trackIndex >= 0 && trackIndex < timelinePtr->getNumTracks())
    {
        track = dynamic_cast<MidiTrack*>(timelinePtr->getTrack(trackIndex));
    }

    if (track == nullptr)
    {
        // Require MidiEngine
        if (midiEnginePtr == nullptr)
            return;

        auto newTrack = std::make_unique<MidiTrack>("MIDI Track", midiEnginePtr);
        track = newTrack.get();
        timelinePtr->addTrack(newTrack.release());
    }

    // Import MIDI
    int numTracks = midiImporter.getNumTracks(file);
    for (int i = 0; i < numTracks; ++i)
    {
        auto sequence = midiImporter.importTrack(file, i, sampleRate);
        if (sequence.getNumEvents() > 0)
        {
            // Calculate length
            double endTime = sequence.getEndTime();
            int64_t lengthVal = static_cast<int64_t>(endTime);

            auto region = std::make_unique<MidiRegion>(startSample, lengthVal);
            region->setMidiSequence(sequence);
            region->setName(file.getFileNameWithoutExtension());

            track->addRegion(std::move(region));
            break; // Just import first valid track for now
        }
    }

    updateTrackHeaders();
    repaint();
}

Region* TimelineView::getRegionAtPosition(int x, int y, int& outTrackIndex) const
{
    outTrackIndex = -1;

    if (timelinePtr == nullptr)
        return nullptr;

    // Check if position is in the timeline area (not ruler or headers)
    if (x <= HEADER_WIDTH || y <= RULER_HEIGHT)
        return nullptr;

    // Find which track the y position corresponds to
    int trackIndex = getTrackIndexAtY(y);
    if (trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return nullptr;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return nullptr;

    // Check audio tracks
    if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
    {
        for (int i = 0; i < audioTrack->getNumRegions(); ++i)
        {
            auto* region = audioTrack->getRegion(i);
            if (region == nullptr)
                continue;

            // Get region bounds and check if point is inside
            juce::Rectangle<int> regionBounds = getRegionBounds(region, trackIndex);
            if (regionBounds.contains(x, y))
            {
                outTrackIndex = trackIndex;
                return region;
            }
        }
    }
    // Check MIDI tracks
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
    {
        for (int i = 0; i < midiTrack->getNumRegions(); ++i)
        {
            auto* region = midiTrack->getRegion(i);
            if (region == nullptr)
                continue;

            // Get region bounds and check if point is inside
            juce::Rectangle<int> regionBounds = getRegionBounds(region, trackIndex);
            if (regionBounds.contains(x, y))
            {
                outTrackIndex = trackIndex;
                return region;
            }
        }
    }

    return nullptr;
}

juce::Rectangle<int> TimelineView::getRegionBounds(Region* region, int trackIndex) const
{
    if (region == nullptr || timelinePtr == nullptr)
        return juce::Rectangle<int>();

    int64_t startSample = region->getStartPosition();
    int64_t endSample = region->getEndPosition();

    // Calculate x coordinates
    int x1 = HEADER_WIDTH + sampleToPixel(startSample) - static_cast<int>(horizontalScrollOffset);
    int x2 = HEADER_WIDTH + sampleToPixel(endSample) - static_cast<int>(horizontalScrollOffset);
    int w = x2 - x1;

    // Calculate y coordinates (track lane bounds)
    int trackY = RULER_HEIGHT + trackIndex * trackHeight;

    return juce::Rectangle<int>(x1, trackY + 2, w, trackHeight - 4);
}

void TimelineView::clearSelection()
{
    if (selectedRegion != nullptr || selectedTrackIndex != -1)
    {
        selectedRegion = nullptr;
        selectedTrackIndex = -1;
        repaint();
    }
}

int64_t TimelineView::snapPositionToGrid(int64_t samplePosition) const
{
    if (timelinePtr == nullptr)
        return samplePosition;

    // Snap to beat boundaries
    double beats = timelinePtr->samplesToBeats(samplePosition);

    // Round to nearest beat
    double snappedBeats = std::round(beats);

    return timelinePtr->beatsToSamples(snappedBeats);
}

void TimelineView::drawDragGhost(juce::Graphics& g)
{
    if (!isDraggingRegion || selectedRegion == nullptr || selectedTrackIndex < 0)
        return;

    // Calculate ghost region bounds using the drag current position
    int64_t length = selectedRegion->getLength();
    int64_t endSample = dragCurrentPosition + length;

    int x1 = HEADER_WIDTH + sampleToPixel(dragCurrentPosition) - static_cast<int>(horizontalScrollOffset);
    int x2 = HEADER_WIDTH + sampleToPixel(endSample) - static_cast<int>(horizontalScrollOffset);
    int w = x2 - x1;

    int trackY = RULER_HEIGHT + selectedTrackIndex * trackHeight;
    juce::Rectangle<int> ghostBounds(x1, trackY + 2, w, trackHeight - 4);

    // Draw semi-transparent ghost of the region
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.3f));
    g.fillRect(ghostBounds);

    // Draw ghost border
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.8f));
    g.drawRect(ghostBounds, 2);

    // Draw snap indicator line at the start position
    g.setColour(MidiSingLookAndFeel::accentColour);
    g.drawLine(static_cast<float>(x1), static_cast<float>(RULER_HEIGHT),
               static_cast<float>(x1), static_cast<float>(getHeight()), 1.0f);
}

void TimelineView::drawResizeGhost(juce::Graphics& g)
{
    if (!isResizingRegion || selectedRegion == nullptr || selectedTrackIndex < 0)
        return;

    // Calculate ghost region bounds using the resize current values
    int64_t endSample = resizeCurrentStart + resizeCurrentLength;

    int x1 = HEADER_WIDTH + sampleToPixel(resizeCurrentStart) - static_cast<int>(horizontalScrollOffset);
    int x2 = HEADER_WIDTH + sampleToPixel(endSample) - static_cast<int>(horizontalScrollOffset);
    int w = x2 - x1;

    int trackY = RULER_HEIGHT + selectedTrackIndex * trackHeight;
    juce::Rectangle<int> ghostBounds(x1, trackY + 2, w, trackHeight - 4);

    // Draw semi-transparent ghost of the resized region
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.3f));
    g.fillRect(ghostBounds);

    // Draw ghost border
    g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.8f));
    g.drawRect(ghostBounds, 2);

    // Draw indicator line at the edge being resized
    g.setColour(MidiSingLookAndFeel::accentColour);
    if (resizeEdge == RegionEdge::Left)
    {
        g.drawLine(static_cast<float>(x1), static_cast<float>(RULER_HEIGHT),
                   static_cast<float>(x1), static_cast<float>(getHeight()), 2.0f);
    }
    else if (resizeEdge == RegionEdge::Right)
    {
        g.drawLine(static_cast<float>(x2), static_cast<float>(RULER_HEIGHT),
                   static_cast<float>(x2), static_cast<float>(getHeight()), 2.0f);
    }
}

TimelineView::RegionEdge TimelineView::getRegionEdgeAtPosition(int x, int y, Region*& outRegion, int& outTrackIndex) const
{
    outRegion = nullptr;
    outTrackIndex = -1;

    if (timelinePtr == nullptr)
        return RegionEdge::None;

    // Check if position is in the timeline area (not ruler or headers)
    if (x <= HEADER_WIDTH || y <= RULER_HEIGHT)
        return RegionEdge::None;

    // Find which track the y position corresponds to
    int trackIndex = getTrackIndexAtY(y);
    if (trackIndex < 0 || trackIndex >= timelinePtr->getNumTracks())
        return RegionEdge::None;

    Track* track = timelinePtr->getTrack(trackIndex);
    if (track == nullptr)
        return RegionEdge::None;

    // Lambda to check region edges
    auto checkRegionEdge = [&](Region* region) -> RegionEdge
    {
        if (region == nullptr)
            return RegionEdge::None;

        juce::Rectangle<int> regionBounds = getRegionBounds(region, trackIndex);

        // Check if y is within the region's vertical bounds
        if (y < regionBounds.getY() || y > regionBounds.getBottom())
            return RegionEdge::None;

        // Check left edge
        if (x >= regionBounds.getX() - EDGE_DETECT_WIDTH / 2 &&
            x <= regionBounds.getX() + EDGE_DETECT_WIDTH / 2)
        {
            outRegion = region;
            outTrackIndex = trackIndex;
            return RegionEdge::Left;
        }

        // Check right edge
        if (x >= regionBounds.getRight() - EDGE_DETECT_WIDTH / 2 &&
            x <= regionBounds.getRight() + EDGE_DETECT_WIDTH / 2)
        {
            outRegion = region;
            outTrackIndex = trackIndex;
            return RegionEdge::Right;
        }

        return RegionEdge::None;
    };

    // Check audio tracks
    if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
    {
        for (int i = 0; i < audioTrack->getNumRegions(); ++i)
        {
            RegionEdge edge = checkRegionEdge(audioTrack->getRegion(i));
            if (edge != RegionEdge::None)
                return edge;
        }
    }
    // Check MIDI tracks
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
    {
        for (int i = 0; i < midiTrack->getNumRegions(); ++i)
        {
            RegionEdge edge = checkRegionEdge(midiTrack->getRegion(i));
            if (edge != RegionEdge::None)
                return edge;
        }
    }

    return RegionEdge::None;
}

void TimelineView::updateCursorForPosition(int x, int y)
{
    // Check if hovering over a region edge
    Region* edgeRegion = nullptr;
    int trackIndex = -1;
    RegionEdge edge = getRegionEdgeAtPosition(x, y, edgeRegion, trackIndex);

    if (edge == RegionEdge::Left || edge == RegionEdge::Right)
    {
        // Show horizontal resize cursor
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    }
    else
    {
        // Normal cursor
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

void TimelineView::showContextMenu(const juce::MouseEvent& e)
{
    juce::PopupMenu menu;

    // Menu item IDs
    enum MenuIDs
    {
        CutRegion = 1,
        CopyRegion,
        PasteRegion,
        DeleteRegion,
        SplitRegion
    };

    bool hasSelection = (selectedRegion != nullptr && selectedTrackIndex >= 0);
    bool canPaste = clipboard.hasData;

    // Add menu items
    menu.addItem(CutRegion, "Cut", hasSelection);
    menu.addItem(CopyRegion, "Copy", hasSelection);
    menu.addItem(PasteRegion, "Paste", canPaste);
    menu.addSeparator();
    menu.addItem(DeleteRegion, "Delete", hasSelection);
    menu.addSeparator();
    menu.addItem(SplitRegion, "Split", hasSelection);

    // Show menu and handle selection
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [this, clickX = e.x](int result)
        {
            switch (result)
            {
            case CutRegion:
                cutSelectedRegion();
                break;
            case CopyRegion:
                copySelectedRegion();
                break;
            case PasteRegion:
                pasteRegion(lastClickSamplePosition);
                break;
            case DeleteRegion:
                deleteSelectedRegion();
                break;
            case SplitRegion:
                splitSelectedRegion(lastClickSamplePosition);
                break;
            default:
                break;
            }
        });
}

void TimelineView::cutSelectedRegion()
{
    if (selectedRegion == nullptr || selectedTrackIndex < 0)
        return;

    // Copy first, then delete
    copySelectedRegion();
    deleteSelectedRegion();
}

void TimelineView::copySelectedRegion()
{
    if (selectedRegion == nullptr || selectedTrackIndex < 0 || timelinePtr == nullptr)
        return;

    // Store region data in clipboard
    clipboard.regionName = selectedRegion->getName();
    clipboard.regionLength = selectedRegion->getLength();
    clipboard.regionOffset = selectedRegion->getOffset();

    // Check if it's an audio region or MIDI region
    if (auto* audioRegion = dynamic_cast<AudioRegion*>(selectedRegion))
    {
        clipboard.isAudioRegion = true;
        clipboard.audioBuffer.makeCopyOf(audioRegion->getAudioBuffer());
        clipboard.filePath = audioRegion->getFilePath();
        clipboard.thumbnailHash = audioRegion->getThumbnailHash();
        clipboard.hasData = true;
    }
    else if (auto* midiRegion = dynamic_cast<MidiRegion*>(selectedRegion))
    {
        clipboard.isAudioRegion = false;
        clipboard.midiSequence = midiRegion->getMidiSequence();
        clipboard.hasData = true;
    }
}

void TimelineView::pasteRegion(int64_t pastePosition)
{
    if (!clipboard.hasData || timelinePtr == nullptr || selectedTrackIndex < 0)
        return;

    // Snap paste position to grid if enabled
    if (snapToGrid)
    {
        pastePosition = snapPositionToGrid(pastePosition);
    }

    // Get the target track
    Track* track = timelinePtr->getTrack(selectedTrackIndex);
    if (track == nullptr)
        return;

    if (clipboard.isAudioRegion)
    {
        // Paste audio region
        auto* audioTrack = dynamic_cast<AudioTrack*>(track);
        if (audioTrack == nullptr)
            return;

        auto newRegion = std::make_unique<AudioRegion>(pastePosition, clipboard.regionLength);
        newRegion->setAudioBuffer(clipboard.audioBuffer);
        newRegion->setName(clipboard.regionName + " (copy)");
        newRegion->setOffset(clipboard.regionOffset);
        if (clipboard.filePath.isNotEmpty())
            newRegion->setFilePath(clipboard.filePath);

        audioTrack->addRegion(std::move(newRegion));
    }
    else
    {
        // Paste MIDI region
        auto* midiTrack = dynamic_cast<MidiTrack*>(track);
        if (midiTrack == nullptr)
            return;

        auto newRegion = std::make_unique<MidiRegion>(pastePosition, clipboard.regionLength);
        newRegion->setMidiSequence(clipboard.midiSequence);
        newRegion->setName(clipboard.regionName + " (copy)");
        newRegion->setOffset(clipboard.regionOffset);

        midiTrack->addRegion(std::move(newRegion));
    }

    repaint();
}

void TimelineView::deleteSelectedRegion()
{
    if (selectedRegion == nullptr || selectedTrackIndex < 0 || timelinePtr == nullptr)
        return;

    Track* track = timelinePtr->getTrack(selectedTrackIndex);
    if (track == nullptr)
        return;

    // Find and remove the region from its track
    if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
    {
        for (int i = 0; i < audioTrack->getNumRegions(); ++i)
        {
            if (audioTrack->getRegion(i) == selectedRegion)
            {
                audioTrack->removeRegion(i);
                break;
            }
        }
    }
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
    {
        for (int i = 0; i < midiTrack->getNumRegions(); ++i)
        {
            if (midiTrack->getRegion(i) == selectedRegion)
            {
                midiTrack->removeRegion(i);
                break;
            }
        }
    }

    // Clear selection
    selectedRegion = nullptr;
    selectedTrackIndex = -1;
    repaint();
}

void TimelineView::splitSelectedRegion(int64_t splitPosition)
{
    if (selectedRegion == nullptr || selectedTrackIndex < 0 || timelinePtr == nullptr)
        return;

    // Ensure split position is within the region
    int64_t regionStart = selectedRegion->getStartPosition();
    int64_t regionEnd = selectedRegion->getEndPosition();

    if (splitPosition <= regionStart || splitPosition >= regionEnd)
        return;

    // Snap split position to grid if enabled
    if (snapToGrid)
    {
        splitPosition = snapPositionToGrid(splitPosition);
        // Re-check bounds after snapping
        if (splitPosition <= regionStart || splitPosition >= regionEnd)
            return;
    }

    Track* track = timelinePtr->getTrack(selectedTrackIndex);
    if (track == nullptr)
        return;

    // Calculate lengths for the two parts
    int64_t firstPartLength = splitPosition - regionStart;
    int64_t secondPartLength = regionEnd - splitPosition;
    int64_t originalOffset = selectedRegion->getOffset();

    if (auto* audioTrack = dynamic_cast<AudioTrack*>(track))
    {
        auto* audioRegion = dynamic_cast<AudioRegion*>(selectedRegion);
        if (audioRegion == nullptr)
            return;

        // Create second part (new region after split point)
        auto secondRegion = std::make_unique<AudioRegion>(splitPosition, secondPartLength);
        secondRegion->setAudioBuffer(audioRegion->getAudioBuffer());
        secondRegion->setName(audioRegion->getName() + " (split)");
        secondRegion->setOffset(originalOffset + firstPartLength);
        if (audioRegion->getFilePath().isNotEmpty())
            secondRegion->setFilePath(audioRegion->getFilePath());

        // Modify the original region (first part)
        audioRegion->setLength(firstPartLength);

        // Add the second region
        audioTrack->addRegion(std::move(secondRegion));
    }
    else if (auto* midiTrack = dynamic_cast<MidiTrack*>(track))
    {
        auto* midiRegion = dynamic_cast<MidiRegion*>(selectedRegion);
        if (midiRegion == nullptr)
            return;

        // For MIDI, we need to split the sequence
        const auto& originalSequence = midiRegion->getMidiSequence();
        juce::MidiMessageSequence firstPartSequence;
        juce::MidiMessageSequence secondPartSequence;

        // Split MIDI events between the two sequences
        for (int i = 0; i < originalSequence.getNumEvents(); ++i)
        {
            auto* event = originalSequence.getEventPointer(i);
            double eventTime = event->message.getTimeStamp();

            // Events are relative to region start, so compare with firstPartLength
            if (eventTime < static_cast<double>(firstPartLength))
            {
                firstPartSequence.addEvent(event->message);
            }
            else
            {
                // Adjust timestamp for second part (relative to new region start)
                auto newMessage = event->message;
                newMessage.setTimeStamp(eventTime - static_cast<double>(firstPartLength));
                secondPartSequence.addEvent(newMessage);
            }
        }

        // Create second part
        auto secondRegion = std::make_unique<MidiRegion>(splitPosition, secondPartLength);
        secondRegion->setMidiSequence(secondPartSequence);
        secondRegion->setName(midiRegion->getName() + " (split)");
        secondRegion->setOffset(0);

        // Modify the original region
        midiRegion->setLength(firstPartLength);
        midiRegion->setMidiSequence(firstPartSequence);

        // Add the second region
        midiTrack->addRegion(std::move(secondRegion));
    }

    repaint();
}
