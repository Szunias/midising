#include "SpectrumDisplay.h"

SpectrumDisplay::SpectrumDisplay(SpectrumAnalyzer& analyzer)
    : spectrumAnalyzer(analyzer)
{
    scopeData.resize(SpectrumAnalyzer::scopeSize, 0.0f);
    startTimerHz(30); // 30 FPS refresh rate
}

SpectrumDisplay::~SpectrumDisplay()
{
    stopTimer();
}

void SpectrumDisplay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    
    g.setOpacity(0.8f);
    g.setColour(juce::Colours::cyan);

    auto width = (float)getWidth();
    auto height = (float)getHeight();
    
    // Draw spectrum
    // Simple path for now
    juce::Path spectrumPath;
    spectrumPath.startNewSubPath(0.0f, height);

    const float spacing = width / (float)scopeData.size();

    for (size_t i = 0; i < scopeData.size(); ++i)
    {
        // Normalize value (experimentally determined scaling)
        auto val = juce::jmap(scopeData[i], 0.0f, 100.0f, 0.0f, 1.0f);
        val = juce::jlimit(0.0f, 1.0f, val);
        
        spectrumPath.lineTo((float)i * spacing, height - height * val);
    }
    
    spectrumPath.lineTo(width, height);
    spectrumPath.closeSubPath();
    
    g.fillPath(spectrumPath);
    g.strokePath(spectrumPath, juce::PathStrokeType(1.0f));
}

void SpectrumDisplay::timerCallback()
{
    if (spectrumAnalyzer.getNextFFTBlock(scopeData))
    {
        repaint();
    }
}
