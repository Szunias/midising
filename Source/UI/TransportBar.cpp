#include "TransportBar.h"

TransportBar::TransportBar()
{
    // Setup metronome button
    metronomeButton.setButtonText("Click");
    metronomeButton.setClickingTogglesState(true);
    metronomeButton.addListener(this);
    addAndMakeVisible(metronomeButton);

    // Setup metronome volume slider
    metronomeVolumeSlider.setRange(0.0, 1.0, 0.01);
    metronomeVolumeSlider.setValue(0.7, juce::dontSendNotification);
    metronomeVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    metronomeVolumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    metronomeVolumeSlider.addListener(this);
    addAndMakeVisible(metronomeVolumeSlider);

    // Setup metronome volume label
    metronomeVolumeLabel.setText("Click Vol:", juce::dontSendNotification);
    metronomeVolumeLabel.setJustificationType(juce::Justification::centredRight);
    metronomeVolumeLabel.attachToComponent(&metronomeVolumeSlider, true);
    addAndMakeVisible(metronomeVolumeLabel);
}

TransportBar::~TransportBar()
{
    metronomeButton.removeListener(this);
    metronomeVolumeSlider.removeListener(this);
}

void TransportBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);

    // Draw border
    g.setColour(juce::Colours::black);
    g.drawRect(getLocalBounds(), 1);

    // Highlight metronome button when enabled
    if (metronomeButton.getToggleState())
    {
        g.setColour(juce::Colours::green.withAlpha(0.3f));
        g.fillRect(metronomeButton.getBounds().expanded(2));
    }
}

void TransportBar::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Layout controls from left to right
    const int buttonWidth = 80;
    const int sliderWidth = 200;
    const int spacing = 10;
    const int labelWidth = 70;

    // Metronome button on the left
    metronomeButton.setBounds(bounds.removeFromLeft(buttonWidth));
    bounds.removeFromLeft(spacing);

    // Metronome volume slider (label is attached, so reserve space for it)
    auto volumeArea = bounds.removeFromLeft(sliderWidth + labelWidth);
    metronomeVolumeSlider.setBounds(volumeArea);
}

void TransportBar::buttonClicked(juce::Button* button)
{
    if (button == &metronomeButton)
    {
        bool enabled = metronomeButton.getToggleState();

        // Call callback if set
        if (onMetronomeToggle)
        {
            onMetronomeToggle(enabled);
        }

        // Repaint to show highlight
        repaint();
    }
}

void TransportBar::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &metronomeVolumeSlider)
    {
        double volume = metronomeVolumeSlider.getValue();

        // Call callback if set
        if (onMetronomeVolumeChange)
        {
            onMetronomeVolumeChange(volume);
        }
    }
}

void TransportBar::setMetronomeEnabled(bool enabled)
{
    metronomeButton.setToggleState(enabled, juce::dontSendNotification);
    repaint();
}
