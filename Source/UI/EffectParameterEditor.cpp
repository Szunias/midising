#include "EffectParameterEditor.h"

EffectParameterEditor::EffectParameterEditor(Effect* effect)
    : effectPtr(effect)
{
    showSidechain = effect->supportsSidechain();

    // Title
    addAndMakeVisible(titleLabel);
    titleLabel.setText(effect->getName(), juce::dontSendNotification);
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);

    // Bypass toggle
    addAndMakeVisible(bypassToggle);
    bypassToggle.setToggleState(effect->isBypassed(), juce::dontSendNotification);
    bypassToggle.onClick = [this]()
    {
        effectPtr->setBypass(bypassToggle.getToggleState());
    };

    // Sidechain controls
    if (showSidechain)
    {
        addAndMakeVisible(sidechainLabel);
        sidechainLabel.setText("Sidechain:", juce::dontSendNotification);
        sidechainLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);

        addAndMakeVisible(sidechainCombo);
        sidechainCombo.addItem("None", 1);
        sidechainCombo.setSelectedId(1, juce::dontSendNotification);
        sidechainCombo.onChange = [this]()
        {
            int selected = sidechainCombo.getSelectedId();
            int trackIndex = selected - 2; // id 1 = None, id 2 = track 0, etc.
            if (onSidechainSourceChanged)
                onSidechainSourceChanged(trackIndex);
        };
    }

    // Viewport for scrollable param rows
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&contentComp, false);

    buildUI();
    startTimerHz(15); // Update value displays
}

EffectParameterEditor::~EffectParameterEditor()
{
    stopTimer();
}

void EffectParameterEditor::buildUI()
{
    rows.clear();
    contentComp.removeAllChildren();

    auto params = effectPtr->getParameters();

    for (const auto& param : params)
    {
        auto row = std::make_unique<ParamRow>();
        row->id = param.id;

        // Label
        row->label = std::make_unique<juce::Label>();
        row->label->setText(param.name, juce::dontSendNotification);
        row->label->setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
        contentComp.addAndMakeVisible(row->label.get());

        // Slider
        row->slider = std::make_unique<juce::Slider>();
        if (param.maxValue - param.minValue > 100.0f)
            row->slider->setSliderStyle(juce::Slider::LinearHorizontal);
        else
            row->slider->setSliderStyle(juce::Slider::LinearHorizontal);

        row->slider->setRange(static_cast<double>(param.minValue),
                              static_cast<double>(param.maxValue),
                              static_cast<double>(param.step > 0.0f ? param.step : 0.001));
        row->slider->setValue(static_cast<double>(param.currentValue), juce::dontSendNotification);

        if (param.skew != 1.0f)
            row->slider->setSkewFactor(static_cast<double>(param.skew));

        row->slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        // Capture the param id by value for the lambda
        juce::String paramId = param.id;
        row->slider->onValueChange = [this, paramId, slider = row->slider.get()]()
        {
            effectPtr->setParameter(paramId, static_cast<float>(slider->getValue()));
        };

        contentComp.addAndMakeVisible(row->slider.get());

        // Value readout
        row->valueLabel = std::make_unique<juce::Label>();
        row->valueLabel->setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour.withAlpha(0.8f));
        row->valueLabel->setJustificationType(juce::Justification::centredLeft);
        row->valueLabel->setFont(juce::Font(11.0f));
        contentComp.addAndMakeVisible(row->valueLabel.get());

        rows.push_back(std::move(row));
    }

    updateValues();
}

void EffectParameterEditor::updateValues()
{
    auto params = effectPtr->getParameters();

    for (size_t i = 0; i < rows.size() && i < params.size(); ++i)
    {
        float currentVal = effectPtr->getParameter(rows[i]->id);
        rows[i]->slider->setValue(static_cast<double>(currentVal), juce::dontSendNotification);

        // Update the EffectParameter struct for display
        auto p = params[i];
        p.currentValue = currentVal;
        rows[i]->valueLabel->setText(p.getDisplayValue(), juce::dontSendNotification);
    }

    bypassToggle.setToggleState(effectPtr->isBypassed(), juce::dontSendNotification);
}

void EffectParameterEditor::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark.brighter(0.03f));
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRect(getLocalBounds());
}

void EffectParameterEditor::resized()
{
    auto area = getLocalBounds().reduced(4);

    // Header row: title + bypass
    auto header = area.removeFromTop(HEADER_HEIGHT);
    bypassToggle.setBounds(header.removeFromRight(70));
    titleLabel.setBounds(header);

    // Sidechain row
    if (showSidechain)
    {
        auto scRow = area.removeFromTop(SIDECHAIN_HEIGHT);
        sidechainLabel.setBounds(scRow.removeFromLeft(LABEL_WIDTH));
        sidechainCombo.setBounds(scRow.reduced(2));
    }

    // Viewport for param rows
    viewport.setBounds(area);

    int contentHeight = static_cast<int>(rows.size()) * ROW_HEIGHT;
    contentComp.setSize(viewport.getWidth() - (contentHeight > area.getHeight() ? viewport.getScrollBarThickness() : 0),
                        juce::jmax(area.getHeight(), contentHeight));

    int y = 0;
    int rowWidth = contentComp.getWidth();
    for (auto& row : rows)
    {
        auto rowArea = juce::Rectangle<int>(0, y, rowWidth, ROW_HEIGHT).reduced(2, 1);
        row->label->setBounds(rowArea.removeFromLeft(LABEL_WIDTH));
        row->valueLabel->setBounds(rowArea.removeFromRight(VALUE_WIDTH));
        row->slider->setBounds(rowArea);
        y += ROW_HEIGHT;
    }
}

void EffectParameterEditor::timerCallback()
{
    updateValues();
}

void EffectParameterEditor::setSidechainTrackNames(const juce::StringArray& names)
{
    if (!showSidechain) return;

    sidechainCombo.clear(juce::dontSendNotification);
    sidechainCombo.addItem("None", 1);
    for (int i = 0; i < names.size(); ++i)
        sidechainCombo.addItem(names[i], i + 2);
}

void EffectParameterEditor::setSidechainSource(int trackIndex)
{
    if (!showSidechain) return;
    sidechainCombo.setSelectedId(trackIndex + 2, juce::dontSendNotification);
}
