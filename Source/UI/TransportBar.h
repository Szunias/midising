#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

/**
 * TransportBar provides playback transport controls and metronome settings.
 * Includes metronome toggle button and volume slider.
 */
class TransportBar : public juce::Component,
                     public juce::Button::Listener,
                     public juce::Slider::Listener
{
public:
    TransportBar();
    ~TransportBar() override;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Button::Listener override
    void buttonClicked(juce::Button* button) override;

    // Slider::Listener override
    void sliderValueChanged(juce::Slider* slider) override;

    // Callbacks for metronome controls
    std::function<void(bool)> onMetronomeToggle;
    std::function<void(double)> onMetronomeVolumeChange;

    // Update metronome button state
    void setMetronomeEnabled(bool enabled);

private:
    // Metronome controls
    juce::TextButton metronomeButton;
    juce::Slider metronomeVolumeSlider;
    juce::Label metronomeVolumeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
