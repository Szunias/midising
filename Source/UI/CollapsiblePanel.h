#pragma once

#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * CollapsiblePanel is a wrapper component that provides show/hide functionality
 * for DAW panels like Browser, Mixer, and PianoRoll.
 * It displays a header bar with title and collapse button when visible.
 */
class CollapsiblePanel : public juce::Component
{
public:
    enum class Position
    {
        Left,   // Browser panel position
        Bottom, // Mixer/PianoRoll position
        Right   // Future panel positions
    };

    CollapsiblePanel(const juce::String& title, Position pos = Position::Bottom)
        : panelTitle(title), position(pos)
    {
        // Header button that collapses/expands the panel
        headerButton.setButtonText(title);
        headerButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::backgroundMid);
        headerButton.setColour(juce::TextButton::textColourOffId, MidiSingLookAndFeel::textColour);
        addAndMakeVisible(headerButton);

        headerButton.onClick = [this]()
        {
            if (onCollapse)
                onCollapse();
        };
    }

    ~CollapsiblePanel() override = default;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(MidiSingLookAndFeel::backgroundMid);

        // Draw border
        g.setColour(MidiSingLookAndFeel::borderColour);
        g.drawRect(getLocalBounds(), 1);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Header bar at top
        auto headerBounds = bounds.removeFromTop(HEADER_HEIGHT);
        headerButton.setBounds(headerBounds.reduced(2));

        // Content fills the rest
        if (contentComponent != nullptr)
        {
            contentComponent->setBounds(bounds);
        }
    }

    void setContent(juce::Component* content)
    {
        if (contentComponent != nullptr)
            removeChildComponent(contentComponent);

        contentComponent = content;

        if (contentComponent != nullptr)
        {
            addAndMakeVisible(contentComponent);
            resized();
        }
    }

    juce::Component* getContent() const { return contentComponent; }

    void setTitle(const juce::String& title)
    {
        panelTitle = title;
        headerButton.setButtonText(title);
    }

    juce::String getTitle() const { return panelTitle; }

    Position getPosition() const { return position; }
    void setPosition(Position pos) { position = pos; }

    // Callback when the collapse button is clicked
    std::function<void()> onCollapse;

    static constexpr int HEADER_HEIGHT = 24;

    // Get the minimum size for this panel based on position
    int getMinimumWidth() const
    {
        if (position == Position::Left || position == Position::Right)
            return 200;
        return 100;
    }

    int getMinimumHeight() const
    {
        if (position == Position::Bottom)
            return 150;
        return 100;
    }

    // Default collapsed sizes for layout calculations
    int getDefaultWidth() const
    {
        if (position == Position::Left)
            return 250;
        if (position == Position::Right)
            return 300;
        return 0; // Not applicable for bottom panels
    }

    int getDefaultHeight() const
    {
        if (position == Position::Bottom)
            return 200;
        return 0; // Not applicable for side panels
    }

private:
    juce::String panelTitle;
    Position position;
    juce::TextButton headerButton;
    juce::Component* contentComponent = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CollapsiblePanel)
};
