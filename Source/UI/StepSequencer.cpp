#include "StepSequencer.h"

//==============================================================================
// Construction

StepSequencer::StepSequencer()
{
    initDefaultRows();
    setupControls();
}

void StepSequencer::initDefaultRows()
{
    // General MIDI drum map defaults (8 rows)
    rowInfos[0] = { "Kick",      36 };
    rowInfos[1] = { "Snare",     38 };
    rowInfos[2] = { "Closed HH", 42 };
    rowInfos[3] = { "Open HH",   46 };
    rowInfos[4] = { "Low Tom",   41 };
    rowInfos[5] = { "Mid Tom",   47 };
    rowInfos[6] = { "High Tom",  50 };
    rowInfos[7] = { "Ride",      51 };

    // Fill remaining rows with sensible defaults
    for (int i = 8; i < MAX_ROWS; ++i)
    {
        rowInfos[i] = { "Row " + juce::String(i + 1), 36 + i };
    }
}

void StepSequencer::setupControls()
{
    // Step count combo box
    stepCountCombo.addItem("16 Steps", 1);
    stepCountCombo.addItem("32 Steps", 2);
    stepCountCombo.setSelectedId(1, juce::dontSendNotification);
    stepCountCombo.onChange = [this]()
    {
        int id = stepCountCombo.getSelectedId();
        setNumSteps(id == 2 ? 32 : 16);
    };
    addAndMakeVisible(stepCountCombo);

    // Subdivision combo box
    subdivisionCombo.addItem("1/4 (2/beat)", 1);
    subdivisionCombo.addItem("1/8 (4/beat)", 2);
    subdivisionCombo.addItem("1/16 (8/beat)", 3);
    subdivisionCombo.setSelectedId(2, juce::dontSendNotification);
    subdivisionCombo.onChange = [this]()
    {
        int id = subdivisionCombo.getSelectedId();
        if (id == 1)      setSubdivision(2);
        else if (id == 2)  setSubdivision(4);
        else if (id == 3)  setSubdivision(8);
    };
    addAndMakeVisible(subdivisionCombo);

    // Clear button
    clearButton.onClick = [this]() { clear(); };
    addAndMakeVisible(clearButton);

    // Randomize button
    randomButton.onClick = [this]() { randomize(); };
    addAndMakeVisible(randomButton);
}

//==============================================================================
// Component overrides

void StepSequencer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.fillAll(juce::Colour(0xff1e1e1e));

    // Controls area background
    auto controlsArea = bounds.removeFromTop(CONTROLS_HEIGHT);
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(controlsArea);
    g.setColour(juce::Colour(0xff444444));
    g.drawLine(0.0f, (float)CONTROLS_HEIGHT, (float)getWidth(), (float)CONTROLS_HEIGHT, 1.0f);

    // Grid area
    auto gridArea = bounds;

    // Row headers
    auto headerArea = gridArea.removeFromLeft(headerWidth);
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(headerArea);

    for (int row = 0; row < numRows; ++row)
    {
        auto rowRect = juce::Rectangle<int>(0, CONTROLS_HEIGHT + row * rowHeight, headerWidth, rowHeight);

        // Alternating row background
        if (row % 2 == 0)
        {
            g.setColour(juce::Colour(0xff2e2e2e));
            g.fillRect(rowRect);
        }

        // Row name text
        g.setColour(juce::Colours::lightgrey);
        g.setFont(juce::Font(12.0f));
        g.drawText(rowInfos[row].name, rowRect.reduced(4, 0), juce::Justification::centredLeft, true);

        // Row separator line
        g.setColour(juce::Colour(0xff3a3a3a));
        g.drawHorizontalLine(rowRect.getBottom(), 0.0f, (float)getWidth());
    }

    // Draw cells
    for (int row = 0; row < numRows; ++row)
    {
        for (int step = 0; step < numSteps; ++step)
        {
            auto cellRect = getCellRect(row, step);

            // Cell background: alternating shade for beat grouping
            bool isBeatStart = (step % subdivision) == 0;
            if (isBeatStart)
                g.setColour(juce::Colour(0xff2c2c2c));
            else
                g.setColour(juce::Colour(0xff242424));

            g.fillRect(cellRect);

            // Active cell: filled with velocity-based colour
            if (grid[row][step].active)
            {
                auto colour = getVelocityColour(grid[row][step].velocity);
                g.setColour(colour);
                g.fillRect(cellRect.reduced(1));
            }

            // Cell border
            g.setColour(juce::Colour(0xff3a3a3a));
            g.drawRect(cellRect, 1);

            // Beat separator: slightly brighter line at beat boundaries
            if (isBeatStart && step > 0)
            {
                g.setColour(juce::Colour(0xff555555));
                g.drawVerticalLine(cellRect.getX(), (float)cellRect.getY(), (float)cellRect.getBottom());
            }
        }
    }

    // Playback indicator: semi-transparent overlay column
    if (currentStep >= 0 && currentStep < numSteps)
    {
        for (int row = 0; row < numRows; ++row)
        {
            auto cellRect = getCellRect(row, currentStep);
            g.setColour(juce::Colour(0x40ffffff));
            g.fillRect(cellRect);
        }

        // Vertical line at the leading edge of the current step column
        auto topCell = getCellRect(0, currentStep);
        auto botCell = getCellRect(numRows - 1, currentStep);
        g.setColour(juce::Colour(0xaaffffff));
        g.drawVerticalLine(topCell.getX(), (float)topCell.getY(), (float)botCell.getBottom());
    }
}

void StepSequencer::resized()
{
    auto bounds = getLocalBounds();

    // Controls area at the top
    auto controlsArea = bounds.removeFromTop(CONTROLS_HEIGHT).reduced(4, 2);

    int comboWidth = 100;
    int buttonWidth = 80;
    int spacing = 6;

    stepCountCombo.setBounds(controlsArea.removeFromLeft(comboWidth));
    controlsArea.removeFromLeft(spacing);

    subdivisionCombo.setBounds(controlsArea.removeFromLeft(comboWidth));
    controlsArea.removeFromLeft(spacing);

    clearButton.setBounds(controlsArea.removeFromLeft(buttonWidth));
    controlsArea.removeFromLeft(spacing);

    randomButton.setBounds(controlsArea.removeFromLeft(buttonWidth));

    // Recalculate pixelsPerStep to fill available width
    auto gridWidth = getWidth() - headerWidth;
    if (numSteps > 0 && gridWidth > 0)
        pixelsPerStep = static_cast<double>(gridWidth) / static_cast<double>(numSteps);
}

void StepSequencer::mouseDown(const juce::MouseEvent& e)
{
    int row, step;
    if (!screenToGrid(e.getPosition(), row, step))
        return;

    if (e.mods.isRightButtonDown())
    {
        // Right-click: show velocity slider popup
        if (!grid[row][step].active)
        {
            // Activate cell first so there is something to edit
            grid[row][step].active = true;
        }

        auto* slider = new juce::Slider(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow);
        slider->setRange(1.0, 127.0, 1.0);
        slider->setValue(static_cast<double>(grid[row][step].velocity), juce::dontSendNotification);
        slider->setSize(40, 120);

        // Capture row/step by value for the lambda
        int capturedRow = row;
        int capturedStep = step;
        slider->onValueChange = [this, slider, capturedRow, capturedStep]()
        {
            grid[capturedRow][capturedStep].velocity = static_cast<uint8_t>(slider->getValue());
            repaint();
        };

        auto& callout = juce::CallOutBox::launchAsynchronously(
            std::unique_ptr<juce::Component>(slider),
            getCellRect(row, step),
            this);
        juce::ignoreUnused(callout);
    }
    else
    {
        // Left-click: toggle cell
        grid[row][step].active = !grid[row][step].active;
        repaint();
    }
}

void StepSequencer::mouseDrag(const juce::MouseEvent& e)
{
    // Vertical drag adjusts velocity of the cell where the drag started
    if (e.mods.isLeftButtonDown())
    {
        if (!isDraggingVelocity)
        {
            int row, step;
            if (screenToGrid(e.getMouseDownPosition(), row, step) && grid[row][step].active)
            {
                isDraggingVelocity = true;
                dragRow = row;
                dragStep = step;
                dragStartY = e.getMouseDownY();
                dragStartVelocity = grid[row][step].velocity;
            }
        }

        if (isDraggingVelocity && dragRow >= 0 && dragStep >= 0)
        {
            // Dragging up increases velocity, down decreases
            int deltaY = dragStartY - e.getPosition().getY();
            int newVel = static_cast<int>(dragStartVelocity) + deltaY;
            newVel = juce::jlimit(1, 127, newVel);
            grid[dragRow][dragStep].velocity = static_cast<uint8_t>(newVel);
            repaint();
        }
    }
}

//==============================================================================
// Playback

void StepSequencer::setCurrentStep(int step)
{
    if (currentStep != step)
    {
        currentStep = step;
        repaint();
    }
}

//==============================================================================
// Pattern editing

void StepSequencer::clear()
{
    for (int r = 0; r < MAX_ROWS; ++r)
        for (int s = 0; s < MAX_STEPS; ++s)
        {
            grid[r][s].active = false;
            grid[r][s].velocity = 100;
        }

    repaint();
}

void StepSequencer::randomize()
{
    auto& rng = juce::Random::getSystemRandom();

    for (int r = 0; r < numRows; ++r)
    {
        for (int s = 0; s < numSteps; ++s)
        {
            // ~30% chance of activation
            grid[r][s].active = rng.nextFloat() < 0.3f;
            if (grid[r][s].active)
                grid[r][s].velocity = static_cast<uint8_t>(rng.nextInt({ 60, 127 }));
            else
                grid[r][s].velocity = 100;
        }
    }

    repaint();
}

//==============================================================================
// Configuration

void StepSequencer::setNumSteps(int steps)
{
    numSteps = juce::jlimit(1, MAX_STEPS, steps);
    resized();
    repaint();
}

void StepSequencer::setSubdivision(int subdiv)
{
    subdivision = juce::jlimit(1, 16, subdiv);
    repaint();
}

//==============================================================================
// MIDI export

std::unique_ptr<MidiRegion> StepSequencer::generateMidiToRegion(double bpm, double sampleRate) const
{
    if (bpm <= 0.0 || sampleRate <= 0.0)
        return nullptr;

    // Each step = one subdivision. Seconds per step = 60 / (bpm * subdivision)
    double secondsPerStep = 60.0 / (bpm * static_cast<double>(subdivision));
    double samplesPerStep = secondsPerStep * sampleRate;

    // Note duration: slightly less than a full step to avoid overlaps
    double noteDurationSeconds = secondsPerStep * 0.9;

    // Total pattern length in samples
    double totalSamples = samplesPerStep * static_cast<double>(numSteps);
    auto totalLength = static_cast<int64_t>(totalSamples);

    auto region = std::make_unique<MidiRegion>(0, totalLength);
    region->setName("Step Sequencer Pattern");

    juce::MidiMessageSequence sequence;

    for (int row = 0; row < numRows; ++row)
    {
        int midiNote = rowInfos[row].midiNote;

        for (int step = 0; step < numSteps; ++step)
        {
            if (!grid[row][step].active)
                continue;

            double stepTimeSeconds = static_cast<double>(step) * secondsPerStep;
            double noteOffTimeSeconds = stepTimeSeconds + noteDurationSeconds;

            auto noteOn = juce::MidiMessage::noteOn(10, midiNote, grid[row][step].velocity);
            noteOn.setTimeStamp(stepTimeSeconds);

            auto noteOff = juce::MidiMessage::noteOff(10, midiNote);
            noteOff.setTimeStamp(noteOffTimeSeconds);

            sequence.addEvent(noteOn);
            sequence.addEvent(noteOff);
        }
    }

    sequence.updateMatchedPairs();
    region->setMidiSequence(sequence);

    return region;
}

//==============================================================================
// Helpers

juce::Rectangle<int> StepSequencer::getCellRect(int row, int step) const
{
    int x = headerWidth + static_cast<int>(static_cast<double>(step) * pixelsPerStep);
    int y = CONTROLS_HEIGHT + row * rowHeight;
    int w = static_cast<int>(pixelsPerStep);
    int h = rowHeight;
    return { x, y, w, h };
}

bool StepSequencer::screenToGrid(juce::Point<int> pos, int& outRow, int& outStep) const
{
    int x = pos.getX() - headerWidth;
    int y = pos.getY() - CONTROLS_HEIGHT;

    if (x < 0 || y < 0)
        return false;

    outStep = static_cast<int>(static_cast<double>(x) / pixelsPerStep);
    outRow  = y / rowHeight;

    if (outStep < 0 || outStep >= numSteps || outRow < 0 || outRow >= numRows)
        return false;

    return true;
}

juce::Colour StepSequencer::getVelocityColour(uint8_t velocity)
{
    // Map velocity (1-127) to brightness: higher velocity = brighter / more saturated
    float t = static_cast<float>(velocity) / 127.0f;
    t = juce::jlimit(0.0f, 1.0f, t);

    // Base colour is a warm orange/amber, fading darker for low velocity
    auto baseColour = juce::Colour(0xffff8c00);  // Dark orange
    auto darkColour = juce::Colour(0xff663300);   // Dark brown

    return darkColour.interpolatedWith(baseColour, t);
}
