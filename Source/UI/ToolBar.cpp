#include "ToolBar.h"

ToolBar::ToolBar()
{
    // Select button (V) - Default tool
    selectButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::accentColour);
    selectButton.setTooltip("Select Tool (V)");
    selectButton.onClick = [this]()
    {
        setToolMode(ToolMode::Select);
    };
    addAndMakeVisible(selectButton);

    // Draw button (D)
    drawButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    drawButton.setTooltip("Draw Tool (D)");
    drawButton.onClick = [this]()
    {
        setToolMode(ToolMode::Draw);
    };
    addAndMakeVisible(drawButton);

    // Split button (S)
    splitButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    splitButton.setTooltip("Split Tool (S)");
    splitButton.onClick = [this]()
    {
        setToolMode(ToolMode::Split);
    };
    addAndMakeVisible(splitButton);

    // Erase button (E)
    eraseButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    eraseButton.setTooltip("Erase Tool (E)");
    eraseButton.onClick = [this]()
    {
        setToolMode(ToolMode::Erase);
    };
    addAndMakeVisible(eraseButton);

    // Pan button (H)
    panButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    panButton.setTooltip("Pan Tool (H)");
    panButton.onClick = [this]()
    {
        setToolMode(ToolMode::Pan);
    };
    addAndMakeVisible(panButton);

    // Zoom button (Z)
    zoomButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    zoomButton.setTooltip("Zoom Tool (Z)");
    zoomButton.onClick = [this]()
    {
        setToolMode(ToolMode::Zoom);
    };
    addAndMakeVisible(zoomButton);

    // Automation button (A)
    automationButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    automationButton.setTooltip("Automation Tool (A)");
    automationButton.onClick = [this]()
    {
        setToolMode(ToolMode::Automation);
    };
    addAndMakeVisible(automationButton);

    // Range button (R)
    rangeButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    rangeButton.setTooltip("Range Tool (R)");
    rangeButton.onClick = [this]()
    {
        setToolMode(ToolMode::Range);
    };
    addAndMakeVisible(rangeButton);
}

ToolBar::~ToolBar()
{
}

void ToolBar::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundMid);

    // Bottom border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawLine(0.0f, static_cast<float>(getHeight()) - 1.0f,
               static_cast<float>(getWidth()), static_cast<float>(getHeight()) - 1.0f);
}

void ToolBar::resized()
{
    // Fixed button dimensions - 40x30px as per spec
    constexpr int buttonWidth = 40;
    constexpr int buttonHeight = 30;
    constexpr int buttonSpacing = 5;
    constexpr int margin = 10;

    // Calculate vertical center for buttons
    const int y = (getHeight() - buttonHeight) / 2;
    int x = margin;

    // Tool buttons - fixed 40x30px with explicit pixel coordinates
    selectButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + buttonSpacing;

    drawButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + buttonSpacing;

    splitButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + buttonSpacing;

    eraseButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + buttonSpacing;

    panButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + buttonSpacing;

    zoomButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + buttonSpacing;

    automationButton.setBounds(x, y, buttonWidth, buttonHeight);
    x += buttonWidth + buttonSpacing;

    rangeButton.setBounds(x, y, buttonWidth, buttonHeight);
}

void ToolBar::setToolMode(ToolMode mode)
{
    if (currentTool != mode)
    {
        currentTool = mode;
        updateButtonStates();

        if (onToolChange)
            onToolChange(currentTool);
    }
}

void ToolBar::updateButtonStates()
{
    // Reset all buttons to default background
    selectButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    drawButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    splitButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    eraseButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    panButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    zoomButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    automationButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);
    rangeButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::buttonBackground);

    // Highlight the active tool button
    switch (currentTool)
    {
        case ToolMode::Select:
            selectButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::accentColour);
            break;
        case ToolMode::Draw:
            drawButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::accentColour);
            break;
        case ToolMode::Split:
            splitButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::accentColour);
            break;
        case ToolMode::Erase:
            eraseButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::accentColour);
            break;
        case ToolMode::Pan:
            panButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::accentColour);
            break;
        case ToolMode::Zoom:
            zoomButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::accentColour);
            break;
        case ToolMode::Automation:
            automationButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::accentColour);
            break;
        case ToolMode::Range:
            rangeButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::accentColour);
            break;
    }

    repaint();
}
