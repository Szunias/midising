#include "SettingsPanel.h"

SettingsPanel::SettingsPanel()
{
    // Setup count-in combo box
    countInComboBox.addItem("Off", 1);
    countInComboBox.addItem("1 bar", 2);
    countInComboBox.addItem("2 bars", 3);
    countInComboBox.setSelectedId(2, juce::dontSendNotification); // Default to '1 bar'
    countInComboBox.addListener(this);
    addAndMakeVisible(countInComboBox);

    // Setup count-in label
    countInLabel.setText("Count-in:", juce::dontSendNotification);
    countInLabel.setJustificationType(juce::Justification::centredRight);
    countInLabel.attachToComponent(&countInComboBox, true);
    addAndMakeVisible(countInLabel);
}

SettingsPanel::~SettingsPanel()
{
    countInComboBox.removeListener(this);
}

void SettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);

    // Draw border
    g.setColour(juce::Colours::black);
    g.drawRect(getLocalBounds(), 1);
}

void SettingsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Layout controls
    const int comboBoxWidth = 150;
    const int labelWidth = 70;

    // Count-in combo box (label is attached, so reserve space for it)
    auto countInArea = bounds.removeFromTop(30);
    countInComboBox.setBounds(countInArea.removeFromRight(comboBoxWidth));
}

void SettingsPanel::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &countInComboBox)
    {
        int bars = getCountInBars();

        // Call callback if set
        if (onCountInChange)
        {
            onCountInChange(bars);
        }
    }
}

int SettingsPanel::getCountInBars() const
{
    int selectedId = countInComboBox.getSelectedId();

    // Map combo box selection to bar count
    // ID 1 = "Off" -> 0 bars
    // ID 2 = "1 bar" -> 1 bar
    // ID 3 = "2 bars" -> 2 bars
    switch (selectedId)
    {
        case 1: return 0;
        case 2: return 1;
        case 3: return 2;
        default: return 1; // Default to 1 bar if somehow invalid
    }
}
