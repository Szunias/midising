#pragma once

#include "../Audio/Transport.h"
#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

/**
 * TransportBar provides playback controls: play, pause, stop, record, BPM.
 */
class TransportBar : public juce::Component,
                     public juce::Timer
{
public:
    TransportBar();
    ~TransportBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // Set transport reference for display updates
    void setTransport(Transport* transport) { transportPtr = transport; }

    // Callbacks for button actions
    std::function<void()> onPlay;
    std::function<void()> onStop;
    std::function<void()> onRecord;
    std::function<void()> onSave;
    std::function<void()> onOpen;
    std::function<void(double)> onBpmChange;

private:
    void updateButtonStates();

    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton recordButton { "Rec" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton openButton { "Open" };
    juce::Slider bpmSlider;
    juce::Label bpmLabel { {}, "BPM:" };
    juce::Label positionLabel { {}, "0:00:00" };

    Transport* transportPtr = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
