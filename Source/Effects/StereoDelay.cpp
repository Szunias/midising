#include "StereoDelay.h"
#include <cmath>

StereoDelay::StereoDelay()
    : Effect("Stereo Delay")
{
}

void StereoDelay::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Calculate buffer size for maximum delay time
    delayBufferSize = static_cast<int>(std::ceil(maxDelayTimeMs * sampleRate / 1000.0)) + 1;

    delayBufferLeft.resize(delayBufferSize, 0.0f);
    delayBufferRight.resize(delayBufferSize, 0.0f);

    // Reset write positions
    writePositionLeft = 0;
    writePositionRight = 0;

    // Reset filter states
    filterStateLeft = 0.0f;
    filterStateRight = 0.0f;

    updateFilterCoeff();
    updateDelayTimes();
}

void StereoDelay::releaseResources()
{
    delayBufferLeft.clear();
    delayBufferRight.clear();
    delayBufferSize = 0;
    filterStateLeft = 0.0f;
    filterStateRight = 0.0f;
}

void StereoDelay::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (delayBufferSize == 0)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels == 0 || numSamples == 0)
        return;

    // Get write pointers
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = (numChannels >= 2) ? buffer.getWritePointer(1) : nullptr;

    // Process each sample
    for (int i = 0; i < numSamples; ++i)
    {
        // Read input samples
        float inputLeft = leftChannel[i];
        float inputRight = rightChannel ? rightChannel[i] : inputLeft;

        // Calculate read positions for left and right
        int readPositionLeft = writePositionLeft - delayTimeSamplesLeft;
        if (readPositionLeft < 0)
            readPositionLeft += delayBufferSize;

        int readPositionRight = writePositionRight - delayTimeSamplesRight;
        if (readPositionRight < 0)
            readPositionRight += delayBufferSize;

        // Read delayed samples
        float delayedLeft = delayBufferLeft[static_cast<size_t>(readPositionLeft)];
        float delayedRight = delayBufferRight[static_cast<size_t>(readPositionRight)];

        // Apply low-pass filter to delayed signal (for feedback path)
        filterStateLeft += filterCoeff * (delayedLeft - filterStateLeft);
        filterStateRight += filterCoeff * (delayedRight - filterStateRight);

        float filteredLeft = filterStateLeft;
        float filteredRight = filterStateRight;

        // Calculate what goes into the delay buffers
        float toBufferLeft, toBufferRight;

        if (pingPongEnabled)
        {
            // Ping-pong: left feedback goes to right, right feedback goes to left
            toBufferLeft = inputLeft + filteredRight * feedback;
            toBufferRight = inputRight + filteredLeft * feedback;
        }
        else
        {
            // Standard stereo delay: each channel feeds back to itself
            toBufferLeft = inputLeft + filteredLeft * feedback;
            toBufferRight = inputRight + filteredRight * feedback;
        }

        // Write to delay buffers
        delayBufferLeft[static_cast<size_t>(writePositionLeft)] = toBufferLeft;
        delayBufferRight[static_cast<size_t>(writePositionRight)] = toBufferRight;

        // Increment write positions
        writePositionLeft = (writePositionLeft + 1) % delayBufferSize;
        writePositionRight = (writePositionRight + 1) % delayBufferSize;

        // Mix dry and wet signals
        float outputLeft = inputLeft * (1.0f - mix) + delayedLeft * mix;
        float outputRight = inputRight * (1.0f - mix) + delayedRight * mix;

        // Write output
        leftChannel[i] = outputLeft;
        if (rightChannel)
            rightChannel[i] = outputRight;
    }
}

void StereoDelay::setTempoSyncEnabled(bool enabled)
{
    tempoSyncEnabled = enabled;
    updateDelayTimes();
}

void StereoDelay::setBPM(double newBpm)
{
    bpm = juce::jlimit(20.0, 300.0, newBpm);
    if (tempoSyncEnabled)
        updateDelayTimes();
}

void StereoDelay::setDelayTimeLeftMs(float timeMs)
{
    delayTimeLeftMs = juce::jlimit(1.0f, maxDelayTimeMs, timeMs);
    if (!tempoSyncEnabled)
        updateDelayTimes();
}

void StereoDelay::setDelayTimeRightMs(float timeMs)
{
    delayTimeRightMs = juce::jlimit(1.0f, maxDelayTimeMs, timeMs);
    if (!tempoSyncEnabled)
        updateDelayTimes();
}

void StereoDelay::setNoteDivisionLeft(NoteDivision division)
{
    noteDivisionLeft = division;
    if (tempoSyncEnabled)
        updateDelayTimes();
}

void StereoDelay::setNoteDivisionRight(NoteDivision division)
{
    noteDivisionRight = division;
    if (tempoSyncEnabled)
        updateDelayTimes();
}

void StereoDelay::setFeedback(float newFeedback)
{
    feedback = juce::jlimit(0.0f, 0.95f, newFeedback);
}

void StereoDelay::setMix(float newMix)
{
    mix = juce::jlimit(0.0f, 1.0f, newMix);
}

void StereoDelay::setFilterCutoff(float cutoffHz)
{
    filterCutoffHz = juce::jlimit(100.0f, 20000.0f, cutoffHz);
    updateFilterCoeff();
}

void StereoDelay::setPingPong(bool enabled)
{
    pingPongEnabled = enabled;
}

int StereoDelay::calculateDelayTimeSamples(bool isLeft) const
{
    float delayTimeMs;

    if (tempoSyncEnabled)
    {
        // Calculate delay time based on BPM and note division
        // Quarter note duration in ms = 60000 / BPM
        float quarterNoteMs = 60000.0f / static_cast<float>(bpm);
        float multiplier = getNoteDivisionMultiplier(isLeft ? noteDivisionLeft : noteDivisionRight);
        delayTimeMs = quarterNoteMs * multiplier;

        // Clamp to max delay time
        delayTimeMs = juce::jlimit(1.0f, maxDelayTimeMs, delayTimeMs);
    }
    else
    {
        delayTimeMs = isLeft ? delayTimeLeftMs : delayTimeRightMs;
    }

    // Convert to samples
    return static_cast<int>(std::round(delayTimeMs * currentSampleRate / 1000.0));
}

float StereoDelay::getNoteDivisionMultiplier(NoteDivision division)
{
    switch (division)
    {
        case NoteDivision::Quarter:          return 1.0f;
        case NoteDivision::Eighth:           return 0.5f;
        case NoteDivision::Sixteenth:        return 0.25f;
        case NoteDivision::ThirtySecond:     return 0.125f;
        case NoteDivision::DottedQuarter:    return 1.5f;
        case NoteDivision::DottedEighth:     return 0.75f;
        case NoteDivision::DottedSixteenth:  return 0.375f;
        case NoteDivision::TripletQuarter:   return 2.0f / 3.0f;
        case NoteDivision::TripletEighth:    return 1.0f / 3.0f;
        case NoteDivision::TripletSixteenth: return 1.0f / 6.0f;
        default:                             return 0.5f;
    }
}

void StereoDelay::updateDelayTimes()
{
    if (currentSampleRate <= 0.0)
        return;

    delayTimeSamplesLeft = calculateDelayTimeSamples(true);
    delayTimeSamplesRight = calculateDelayTimeSamples(false);

    // Ensure we don't exceed buffer size
    if (delayBufferSize > 0)
    {
        delayTimeSamplesLeft = juce::jlimit(1, delayBufferSize - 1, delayTimeSamplesLeft);
        delayTimeSamplesRight = juce::jlimit(1, delayBufferSize - 1, delayTimeSamplesRight);
    }
}

void StereoDelay::updateFilterCoeff()
{
    if (currentSampleRate <= 0.0)
        return;

    // Simple one-pole low-pass filter coefficient
    // This gives a gentle roll-off that simulates analog delay feedback character
    float omega = 2.0f * juce::MathConstants<float>::pi * filterCutoffHz / static_cast<float>(currentSampleRate);
    filterCoeff = omega / (omega + 1.0f);
}

juce::String StereoDelay::getNoteDivisionName(NoteDivision division)
{
    switch (division)
    {
        case NoteDivision::Quarter:          return "1/4";
        case NoteDivision::Eighth:           return "1/8";
        case NoteDivision::Sixteenth:        return "1/16";
        case NoteDivision::ThirtySecond:     return "1/32";
        case NoteDivision::DottedQuarter:    return "1/4D";
        case NoteDivision::DottedEighth:     return "1/8D";
        case NoteDivision::DottedSixteenth:  return "1/16D";
        case NoteDivision::TripletQuarter:   return "1/4T";
        case NoteDivision::TripletEighth:    return "1/8T";
        case NoteDivision::TripletSixteenth: return "1/16T";
        default:                             return "1/8";
    }
}

std::unique_ptr<juce::XmlElement> StereoDelay::createXml() const
{
    auto xml = Effect::createXml();
    xml->setAttribute("type", "StereoDelay");
    xml->setAttribute("tempoSyncEnabled", tempoSyncEnabled);
    xml->setAttribute("bpm", bpm);
    xml->setAttribute("delayTimeLeftMs", delayTimeLeftMs);
    xml->setAttribute("delayTimeRightMs", delayTimeRightMs);
    xml->setAttribute("noteDivisionLeft", static_cast<int>(noteDivisionLeft));
    xml->setAttribute("noteDivisionRight", static_cast<int>(noteDivisionRight));
    xml->setAttribute("feedback", feedback);
    xml->setAttribute("mix", mix);
    xml->setAttribute("filterCutoff", filterCutoffHz);
    xml->setAttribute("pingPong", pingPongEnabled);
    return xml;
}

void StereoDelay::loadFromXml(const juce::XmlElement& xml)
{
    Effect::loadFromXml(xml);
    tempoSyncEnabled = xml.getBoolAttribute("tempoSyncEnabled", false);
    bpm = xml.getDoubleAttribute("bpm", 120.0);
    delayTimeLeftMs = static_cast<float>(xml.getDoubleAttribute("delayTimeLeftMs", 250.0));
    delayTimeRightMs = static_cast<float>(xml.getDoubleAttribute("delayTimeRightMs", 250.0));
    noteDivisionLeft = static_cast<NoteDivision>(xml.getIntAttribute("noteDivisionLeft",
                                                  static_cast<int>(NoteDivision::Eighth)));
    noteDivisionRight = static_cast<NoteDivision>(xml.getIntAttribute("noteDivisionRight",
                                                   static_cast<int>(NoteDivision::Eighth)));
    feedback = static_cast<float>(xml.getDoubleAttribute("feedback", 0.4));
    mix = static_cast<float>(xml.getDoubleAttribute("mix", 0.5));
    filterCutoffHz = static_cast<float>(xml.getDoubleAttribute("filterCutoff", 8000.0));
    pingPongEnabled = xml.getBoolAttribute("pingPong", false);

    updateFilterCoeff();
    updateDelayTimes();
}
