#include "StatusBar.h"
#include "../Audio/Transport.h"

StatusBar::StatusBar(juce::AudioDeviceManager& dm)
    : deviceManager(dm)
{
    // Start timer for beat indicator updates (60 FPS for smooth animation)
    startTimer(16);
}

StatusBar::~StatusBar()
{
    stopTimer();
}

void StatusBar::paint(juce::Graphics& g)
{
    // Fill background
    g.fillAll(juce::Colour(0xff2c2c2c));

    // Draw border
    g.setColour(juce::Colours::black);
    g.drawRect(getLocalBounds(), 1);

    auto bounds = getLocalBounds().reduced(4);

    // Draw beat indicator on the right side
    const int indicatorSize = 16;
    auto indicatorBounds = bounds.removeFromRight(indicatorSize).withSizeKeepingCentre(indicatorSize, indicatorSize);
    drawBeatIndicator(g, indicatorBounds);

    // Draw status text (e.g., sample rate, buffer size)
    bounds.removeFromRight(10); // Spacing
    g.setColour(juce::Colours::lightgrey);
    g.setFont(12.0f);

    juce::String statusText;
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        statusText = juce::String(device->getCurrentSampleRate(), 0) + " Hz | "
                   + juce::String(device->getCurrentBufferSizeSamples()) + " samples";
    }
    else
    {
        statusText = "No audio device";
    }

    g.drawText(statusText, bounds, juce::Justification::centredRight, true);
}

void StatusBar::resized()
{
    // Layout is handled in paint()
}

void StatusBar::timerCallback()
{
    updateBeatState();
    repaint();
}

void StatusBar::setTransport(Transport* t)
{
    transport = t;
}

void StatusBar::updateBeatState()
{
    if (transport == nullptr)
        return;

    // Get current playhead position and BPM
    int64_t playheadPosition = transport->getPlayheadPosition();
    double bpm = transport->getBPM();
    bool isPlaying = transport->isPlaying();

    // Only show beat indicator when playing
    if (!isPlaying)
    {
        isBeatActive = false;
        return;
    }

    // Calculate samples per beat
    double sampleRate = deviceManager.getCurrentAudioDevice()
        ? deviceManager.getCurrentAudioDevice()->getCurrentSampleRate()
        : 44100.0;
    // Samples per beat = (samples per second * seconds per minute) / beats per minute
    int64_t samplesPerBeat = static_cast<int64_t>((sampleRate * 60.0) / bpm);

    // Calculate current beat position
    int64_t currentBeat = playheadPosition / samplesPerBeat;
    int64_t beatStartSample = currentBeat * samplesPerBeat;

    // Check if we're on a new beat
    if (beatStartSample != lastBeatSample)
    {
        lastBeatSample = beatStartSample;

        // Calculate if this is an accent beat (first beat of measure - 4/4 time)
        isAccentBeat = (currentBeat % 4) == 0;

        // Start beat flash
        beatFlashStartTime = juce::Time::getCurrentTime().toMilliseconds();
        isBeatActive = true;
    }
    else
    {
        // Check if flash duration has elapsed
        int64_t currentTime = juce::Time::getCurrentTime().toMilliseconds();
        if (currentTime - beatFlashStartTime > flashDurationMs)
        {
            isBeatActive = false;
        }
    }
}

void StatusBar::drawBeatIndicator(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Draw LED circle background (dark)
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillEllipse(bounds.toFloat());

    // Draw LED when beat is active
    if (isBeatActive)
    {
        // Accent beat (first of measure) is brighter green
        // Normal beats are dimmer green
        juce::Colour ledColour = isAccentBeat
            ? juce::Colours::lime.brighter(0.3f)
            : juce::Colours::green;

        // Draw glow effect
        g.setColour(ledColour.withAlpha(0.3f));
        g.fillEllipse(bounds.toFloat().expanded(2.0f));

        // Draw LED
        g.setColour(ledColour);
        g.fillEllipse(bounds.toFloat());
    }
    else
    {
        // Draw dim LED when inactive
        g.setColour(juce::Colour(0xff003300));
        g.fillEllipse(bounds.toFloat());
    }

    // Draw LED border
    g.setColour(juce::Colours::black);
    g.drawEllipse(bounds.toFloat(), 1.0f);
}
