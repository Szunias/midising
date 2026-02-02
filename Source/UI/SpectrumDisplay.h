#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Audio/SpectrumAnalyzer.h"

/**
 * SpectrumDisplay visualizes the frequency data from SpectrumAnalyzer.
 */
class SpectrumDisplay : public juce::Component,
                        public juce::Timer
{
public:
    SpectrumDisplay(SpectrumAnalyzer& analyzer);
    ~SpectrumDisplay() override;

    void paint(juce::Graphics& g) override;
    void timerCallback() override;

private:
    SpectrumAnalyzer& spectrumAnalyzer;
    std::vector<float> scopeData;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumDisplay)
};
