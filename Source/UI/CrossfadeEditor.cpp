#include "CrossfadeEditor.h"
#include "LookAndFeel.h"

CrossfadeEditor::CrossfadeEditor()
{
    setSize(300, 200);

    // Curve type label
    addAndMakeVisible(curveTypeLabel);
    curveTypeLabel.setText("Curve Type:", juce::dontSendNotification);
    curveTypeLabel.setJustificationType(juce::Justification::centredRight);
    curveTypeLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);

    // Curve type combo box
    addAndMakeVisible(curveTypeCombo);
    curveTypeCombo.addItem("Linear", 1);
    curveTypeCombo.addItem("Equal Power", 2);
    curveTypeCombo.addItem("S-Curve", 3);
    curveTypeCombo.addItem("Logarithmic", 4);
    curveTypeCombo.setSelectedId(2); // Default to Equal Power
    curveTypeCombo.onChange = [this]() { repaint(); };

    // Length label
    addAndMakeVisible(lengthLabel);
    lengthLabel.setText("Length (ms):", juce::dontSendNotification);
    lengthLabel.setJustificationType(juce::Justification::centredRight);
    lengthLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);

    // Length slider (in milliseconds, converted to samples on output)
    // Range: 1ms to 500ms, default 50ms (matching DEFAULT_CROSSFADE_SAMPLES at 44100 Hz)
    addAndMakeVisible(lengthSlider);
    lengthSlider.setRange(1.0, 500.0, 1.0);
    lengthSlider.setValue(50.0);
    lengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    lengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    lengthSlider.onValueChange = [this]() { repaint(); };

    // Apply button
    addAndMakeVisible(applyButton);
    applyButton.setButtonText("Apply");
    applyButton.onClick = [this]()
    {
        if (onCrossfadeChanged)
        {
            auto curveType = getSelectedCurveType();
            auto lengthSamples = getCrossfadeLength();
            onCrossfadeChanged(curveType, lengthSamples);
        }
    };
}

void CrossfadeEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(MidiSingLookAndFeel::backgroundMid);

    // Border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRect(getLocalBounds(), 1);

    // Title
    g.setColour(MidiSingLookAndFeel::textColour);
    g.setFont(15.0f);
    g.drawText("Crossfade Editor", 0, 2, getWidth(), 20, juce::Justification::centred);

    // Draw the crossfade curve display area
    auto displayArea = getLocalBounds().reduced(10).withTop(22).withBottom(getHeight() - 80);
    drawCrossfadeCurves(g, displayArea);
}

void CrossfadeEditor::resized()
{
    auto area = getLocalBounds().reduced(10);

    // Skip title area and display area
    area.removeFromTop(22);

    // Display area takes up the top portion
    auto displayHeight = getHeight() - 80 - 22;
    area.removeFromTop(displayHeight);

    // Controls below the display
    area.removeFromTop(5);

    // Curve type row
    auto curveRow = area.removeFromTop(24);
    curveTypeLabel.setBounds(curveRow.removeFromLeft(80));
    curveTypeCombo.setBounds(curveRow.reduced(2, 0));

    area.removeFromTop(4);

    // Length row
    auto lengthRow = area.removeFromTop(24);
    lengthLabel.setBounds(lengthRow.removeFromLeft(80));
    lengthSlider.setBounds(lengthRow.reduced(2, 0));

    area.removeFromTop(4);

    // Apply button - centered at bottom
    auto buttonRow = area.removeFromTop(24);
    auto buttonWidth = 80;
    applyButton.setBounds(buttonRow.withSizeKeepingCentre(buttonWidth, 24));
}

CrossfadeCurveType CrossfadeEditor::getSelectedCurveType() const
{
    switch (curveTypeCombo.getSelectedId())
    {
        case 1:  return CrossfadeCurveType::Linear;
        case 2:  return CrossfadeCurveType::EqualPower;
        case 3:  return CrossfadeCurveType::SCurve;
        case 4:  return CrossfadeCurveType::Logarithmic;
        default: return CrossfadeCurveType::EqualPower;
    }
}

void CrossfadeEditor::setSelectedCurveType(CrossfadeCurveType type)
{
    switch (type)
    {
        case CrossfadeCurveType::Linear:      curveTypeCombo.setSelectedId(1); break;
        case CrossfadeCurveType::EqualPower:  curveTypeCombo.setSelectedId(2); break;
        case CrossfadeCurveType::SCurve:      curveTypeCombo.setSelectedId(3); break;
        case CrossfadeCurveType::Logarithmic: curveTypeCombo.setSelectedId(4); break;
    }
    repaint();
}

int64_t CrossfadeEditor::getCrossfadeLength() const
{
    // Convert milliseconds to samples at 44100 Hz
    double ms = lengthSlider.getValue();
    return static_cast<int64_t>((ms / 1000.0) * 44100.0);
}

void CrossfadeEditor::setCrossfadeLength(int64_t lengthInSamples)
{
    // Convert samples to milliseconds at 44100 Hz
    double ms = (static_cast<double>(lengthInSamples) / 44100.0) * 1000.0;
    lengthSlider.setValue(ms, juce::dontSendNotification);
    repaint();
}

void CrossfadeEditor::drawCrossfadeCurves(juce::Graphics& g, juce::Rectangle<int> area)
{
    // Draw display background
    g.setColour(MidiSingLookAndFeel::backgroundDark);
    g.fillRoundedRectangle(area.toFloat(), 4.0f);

    // Draw display border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRoundedRectangle(area.toFloat(), 4.0f, 1.0f);

    auto curveType = getSelectedCurveType();
    auto innerArea = area.reduced(4);

    float left = static_cast<float>(innerArea.getX());
    float right = static_cast<float>(innerArea.getRight());
    float top = static_cast<float>(innerArea.getY());
    float bottom = static_cast<float>(innerArea.getBottom());
    float width = right - left;
    float height = bottom - top;

    const int numPoints = static_cast<int>(width);

    // Build fade-out path (outgoing region: starts at 1.0, ends at 0.0)
    juce::Path fadeOutPath;
    fadeOutPath.startNewSubPath(left, top); // Gain = 1.0 at left edge

    for (int i = 1; i <= numPoints; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(numPoints);
        float gain = CrossfadeManager::calculateFadeGain(t, curveType, true);
        float x = left + t * width;
        float y = bottom - gain * height;
        fadeOutPath.lineTo(x, y);
    }

    // Build fade-in path (incoming region: starts at 0.0, ends at 1.0)
    juce::Path fadeInPath;
    fadeInPath.startNewSubPath(left, bottom); // Gain = 0.0 at left edge

    for (int i = 1; i <= numPoints; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(numPoints);
        float gain = CrossfadeManager::calculateFadeGain(t, curveType, false);
        float x = left + t * width;
        float y = bottom - gain * height;
        fadeInPath.lineTo(x, y);
    }

    // Draw fade-out curve (red/warm color for outgoing)
    g.setColour(MidiSingLookAndFeel::recordColour.withAlpha(0.9f));
    g.strokePath(fadeOutPath, juce::PathStrokeType(2.0f));

    // Draw fade-in curve (green color for incoming)
    g.setColour(MidiSingLookAndFeel::playColour.withAlpha(0.9f));
    g.strokePath(fadeInPath, juce::PathStrokeType(2.0f));

    // Draw crossover point indicator (where both curves are ~0.5)
    g.setColour(MidiSingLookAndFeel::textDimColour.withAlpha(0.5f));
    float midX = left + width * 0.5f;
    g.drawVerticalLine(static_cast<int>(midX), top, bottom);

    // Draw horizontal reference line at 0.5 gain
    float midY = top + height * 0.5f;
    float dashLengths[] = { 4.0f, 4.0f };
    juce::Path refLine;
    refLine.startNewSubPath(left, midY);
    refLine.lineTo(right, midY);
    g.setColour(juce::Colour(0x40ffffff));
    g.strokePath(refLine, juce::PathStrokeType(1.0f));
}
