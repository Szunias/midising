#include "ChorusFlanger.h"

ChorusFlanger::ChorusFlanger()
    : Effect("Chorus/Flanger")
{
}

void ChorusFlanger::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Allocate delay buffers large enough for max delay
    delayBufferSize = static_cast<int>(sampleRate * maxDelayMs * 0.001) + 2;
    delayLineL.assign(static_cast<size_t>(delayBufferSize), 0.0f);
    delayLineR.assign(static_cast<size_t>(delayBufferSize), 0.0f);
    writePos = 0;

    // Reset LFO
    lfoPhase = 0.0f;
    lfoIncrement = rateHz / static_cast<float>(sampleRate);

    feedbackL = 0.0f;
    feedbackR = 0.0f;
}

void ChorusFlanger::releaseResources()
{
    delayLineL.clear();
    delayLineR.clear();
}

float ChorusFlanger::readDelayLine(const std::vector<float>& buffer, float delaySamples) const
{
    // Linear interpolation read from circular buffer
    float readPosF = static_cast<float>(writePos) - delaySamples;
    if (readPosF < 0.0f)
        readPosF += static_cast<float>(delayBufferSize);

    int readIdx = static_cast<int>(readPosF);
    float frac = readPosF - static_cast<float>(readIdx);

    int idx0 = readIdx % delayBufferSize;
    int idx1 = (readIdx + 1) % delayBufferSize;

    return buffer[static_cast<size_t>(idx0)] * (1.0f - frac) +
           buffer[static_cast<size_t>(idx1)] * frac;
}

void ChorusFlanger::processBlock(juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    if (delayLineL.empty())
        return;

    // Determine delay range based on mode
    float baseDelayMs, modDepthMs;
    if (mode == Mode::Chorus)
    {
        baseDelayMs = delayOffsetMs;  // 10-30ms typical
        modDepthMs = depth * 5.0f;    // Up to 5ms modulation
    }
    else // Flanger
    {
        baseDelayMs = juce::jmin(delayOffsetMs, 10.0f);  // 1-10ms
        modDepthMs = depth * baseDelayMs * 0.8f;          // Modulates most of the delay
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // LFO (sine wave)
        float lfoValue = std::sin(2.0f * juce::MathConstants<float>::pi * lfoPhase);
        lfoPhase += lfoIncrement;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        // Calculate modulated delay time in samples
        float delayMs = baseDelayMs + lfoValue * modDepthMs;
        delayMs = juce::jmax(0.1f, delayMs);
        float delaySamples = delayMs * static_cast<float>(currentSampleRate) * 0.001f;
        delaySamples = juce::jlimit(0.5f, static_cast<float>(delayBufferSize - 1), delaySamples);

        // Read from delay lines
        float delayedL = readDelayLine(delayLineL, delaySamples);
        float delayedR = numChannels >= 2 ? readDelayLine(delayLineR, delaySamples) : delayedL;

        // Get dry input
        float dryL = numChannels >= 1 ? buffer.getSample(0, sample) : 0.0f;
        float dryR = numChannels >= 2 ? buffer.getSample(1, sample) : dryL;

        // Write to delay line with feedback
        delayLineL[static_cast<size_t>(writePos)] = dryL + feedbackL * feedback;
        if (numChannels >= 2)
            delayLineR[static_cast<size_t>(writePos)] = dryR + feedbackR * feedback;

        // Store feedback
        feedbackL = juce::jlimit(-1.0f, 1.0f, delayedL);
        feedbackR = juce::jlimit(-1.0f, 1.0f, delayedR);

        // Advance write position
        writePos = (writePos + 1) % delayBufferSize;

        // Mix dry/wet
        float outL = dryL * (1.0f - mix) + delayedL * mix;
        float outR = dryR * (1.0f - mix) + delayedR * mix;

        if (numChannels >= 1)
            buffer.setSample(0, sample, outL);
        if (numChannels >= 2)
            buffer.setSample(1, sample, outR);
    }
}

void ChorusFlanger::setMode(Mode m)
{
    mode = m;
    // Adjust default delay offset for mode
    if (mode == Mode::Chorus && delayOffsetMs < 10.0f)
        delayOffsetMs = 20.0f;
    else if (mode == Mode::Flanger && delayOffsetMs > 10.0f)
        delayOffsetMs = 5.0f;
}

void ChorusFlanger::setRate(float hz)
{
    rateHz = juce::jlimit(0.1f, 10.0f, hz);
    if (currentSampleRate > 0.0)
        lfoIncrement = rateHz / static_cast<float>(currentSampleRate);
}

void ChorusFlanger::setDepth(float d)
{
    depth = juce::jlimit(0.0f, 1.0f, d);
}

void ChorusFlanger::setFeedback(float fb)
{
    feedback = juce::jlimit(0.0f, 0.95f, fb);
}

void ChorusFlanger::setMix(float m)
{
    mix = juce::jlimit(0.0f, 1.0f, m);
}

void ChorusFlanger::setDelayOffset(float ms)
{
    delayOffsetMs = juce::jlimit(0.5f, 40.0f, ms);
}

std::unique_ptr<juce::XmlElement> ChorusFlanger::createXml() const
{
    auto xml = Effect::createXml();
    xml->setAttribute("type", "ChorusFlanger");
    xml->setAttribute("mode", static_cast<int>(mode));
    xml->setAttribute("rate", rateHz);
    xml->setAttribute("depth", depth);
    xml->setAttribute("feedback", feedback);
    xml->setAttribute("mix", mix);
    xml->setAttribute("delayOffset", delayOffsetMs);
    return xml;
}

void ChorusFlanger::loadFromXml(const juce::XmlElement& xml)
{
    Effect::loadFromXml(xml);
    mode = static_cast<Mode>(xml.getIntAttribute("mode", 0));
    rateHz = static_cast<float>(xml.getDoubleAttribute("rate", 0.5));
    depth = static_cast<float>(xml.getDoubleAttribute("depth", 0.5));
    feedback = static_cast<float>(xml.getDoubleAttribute("feedback", 0.0));
    mix = static_cast<float>(xml.getDoubleAttribute("mix", 0.5));
    delayOffsetMs = static_cast<float>(xml.getDoubleAttribute("delayOffset", 20.0));
}
