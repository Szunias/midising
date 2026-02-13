#include "LoudnessDisplay.h"
#include "../Audio/AudioEngine.h"

LoudnessDisplay::LoudnessDisplay()
{
    startTimerHz(20); // 20 FPS update
}

LoudnessDisplay::~LoudnessDisplay()
{
    stopTimer();
}

void LoudnessDisplay::timerCallback()
{
    if (audioEngine == nullptr)
        return;

    auto& mixer = audioEngine->getMixer();

    momentaryLUFS = mixer.getMomentaryLoudness();
    shortTermLUFS = mixer.getShortTermLoudness();
    integratedLUFS = mixer.getIntegratedLoudness();
    loudnessRange = mixer.getLoudnessRange();

    // Smooth bar animation
    float targetMom = lufsToNormalized(momentaryLUFS);
    float targetShort = lufsToNormalized(shortTermLUFS);
    displayMomentary += (targetMom - displayMomentary) * 0.3f;
    displayShortTerm += (targetShort - displayShortTerm) * 0.3f;

    repaint();
}

void LoudnessDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.setColour(MidiSingLookAndFeel::backgroundDark);
    g.fillRect(bounds);

    // Border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRect(bounds);

    auto area = bounds.reduced(2);

    if (compactMode)
    {
        // Compact mode: two bars (Momentary + Integrated) side by side
        auto leftHalf = area.removeFromLeft(area.getWidth() / 2);
        auto rightHalf = area;

        // Momentary bar
        drawMeter(g, leftHalf.reduced(1),
                  displayMomentary, "M",
                  juce::String(momentaryLUFS, 1));

        // Integrated value (text only, no bar)
        g.setFont(9.0f);
        g.setColour(MidiSingLookAndFeel::textColour);
        auto intTop = rightHalf.removeFromTop(10);
        g.drawText("I", intTop, juce::Justification::centred);
        g.setColour(getLevelColour(integratedLUFS));
        g.drawText(integratedLUFS > MIN_LUFS
                       ? juce::String(integratedLUFS, 1)
                       : "-inf",
                   rightHalf, juce::Justification::centred);
    }
    else
    {
        // Full mode: four sections
        int colWidth = area.getWidth() / 4;

        auto momArea = area.removeFromLeft(colWidth);
        auto stArea = area.removeFromLeft(colWidth);
        auto intArea = area.removeFromLeft(colWidth);
        auto lraArea = area;

        // Momentary bar
        drawMeter(g, momArea.reduced(1),
                  displayMomentary, "M",
                  juce::String(momentaryLUFS, 1));

        // Short-term bar
        drawMeter(g, stArea.reduced(1),
                  displayShortTerm, "S",
                  juce::String(shortTermLUFS, 1));

        // Integrated (text)
        g.setFont(9.0f);
        g.setColour(MidiSingLookAndFeel::textColour);
        auto intTop = intArea.reduced(1).removeFromTop(10);
        g.drawText("I", intTop, juce::Justification::centred);
        g.setColour(getLevelColour(integratedLUFS));
        g.drawText(integratedLUFS > MIN_LUFS
                       ? juce::String(integratedLUFS, 1)
                       : "-inf",
                   intArea.reduced(1), juce::Justification::centred);

        // LRA (text)
        g.setColour(MidiSingLookAndFeel::textColour);
        auto lraTop = lraArea.reduced(1).removeFromTop(10);
        g.drawText("LRA", lraTop, juce::Justification::centred);
        g.setColour(MidiSingLookAndFeel::textColour.brighter(0.3f));
        g.drawText(juce::String(loudnessRange, 1) + " LU",
                   lraArea.reduced(1), juce::Justification::centred);
    }
}

void LoudnessDisplay::resized()
{
}

void LoudnessDisplay::drawMeter(juce::Graphics& g, juce::Rectangle<int> bounds,
                                 float value, const juce::String& label,
                                 const juce::String& valueText)
{
    // Label at top
    g.setFont(9.0f);
    g.setColour(MidiSingLookAndFeel::textColour);
    auto labelArea = bounds.removeFromTop(10);
    g.drawText(label, labelArea, juce::Justification::centred);

    // Value text at bottom
    auto valueArea = bounds.removeFromBottom(10);
    g.setColour(getLevelColour(momentaryLUFS));
    g.drawText(value > 0.001f ? valueText : "-inf", valueArea, juce::Justification::centred);

    // Bar
    auto barArea = bounds.reduced(2, 1);
    g.setColour(MidiSingLookAndFeel::sliderBackground);
    g.fillRect(barArea);

    if (value > 0.001f)
    {
        int barHeight = static_cast<int>(static_cast<float>(barArea.getHeight()) * value);
        auto filledArea = barArea.removeFromBottom(barHeight);

        // Determine LUFS from normalized value for coloring
        float lufs = MIN_LUFS + value * (MAX_LUFS - MIN_LUFS);
        g.setColour(getLevelColour(lufs));
        g.fillRect(filledArea);
    }
}

juce::Colour LoudnessDisplay::getLevelColour(float lufs) const
{
    if (lufs > -9.0f)
        return juce::Colours::red;
    else if (lufs > -14.0f)
        return juce::Colours::yellow;
    else
        return MidiSingLookAndFeel::playColour; // Green
}

float LoudnessDisplay::lufsToNormalized(float lufs) const
{
    if (lufs <= MIN_LUFS)
        return 0.0f;
    if (lufs >= MAX_LUFS)
        return 1.0f;
    return (lufs - MIN_LUFS) / (MAX_LUFS - MIN_LUFS);
}

void LoudnessDisplay::setAudioEngine(AudioEngine* engine)
{
    audioEngine = engine;
}

void LoudnessDisplay::setCompactMode(bool compact)
{
    compactMode = compact;
}
