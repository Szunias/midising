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
    std::function<void()> onRewind;
    std::function<void(bool)> onLoopToggle;
    std::function<void()> onSave;
    std::function<void()> onOpen;
    std::function<void(double)> onBpmChange;

    std::function<void(bool)> onMetronomeToggle;
    std::function<void(double)> onMetronomeVolumeChange;
    std::function<void(bool)> onPunchInToggle;
    std::function<void(bool)> onPunchOutToggle;

    void setMetronomeEnabled(bool enabled);
    void setLoopEnabled(bool enabled);
    void setPunchInEnabled(bool enabled);
    void setPunchOutEnabled(bool enabled);

private:
    void updateButtonStates();

    juce::TextButton rewindButton { "|<" };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton recordButton { "Rec" };
    juce::TextButton loopButton { "Loop" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton openButton { "Open" };
    juce::Slider bpmSlider;
    juce::Label bpmLabel { {}, "BPM:" };
    juce::Label positionLabel { {}, "0:00:00" };

    // Punch-in/out controls
    juce::TextButton punchInButton{ "P.In" };
    juce::TextButton punchOutButton{ "P.Out" };

    // Metronome controls
    juce::TextButton metronomeButton{ "Click" };
    juce::Slider metronomeVolumeSlider;
    juce::Label metronomeVolumeLabel;

    Transport* transportPtr = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
