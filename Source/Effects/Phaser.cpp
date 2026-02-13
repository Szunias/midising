#include "Phaser.h"

Phaser::Phaser() : Effect("Phaser")
{
}

void Phaser::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    lfoIncrement = rateHz / static_cast<float>(sampleRate);
    lfoPhase = 0.0f;

    for (int i = 0; i < MAX_STAGES; ++i)
    {
        allPassL[i].reset();
        allPassR[i].reset();
    }

    feedbackSampleL = 0.0f;
    feedbackSampleR = 0.0f;
}

void Phaser::releaseResources()
{
    for (int i = 0; i < MAX_STAGES; ++i)
    {
        allPassL[i].reset();
        allPassR[i].reset();
    }
}

void Phaser::processBlock(juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    float* leftChannel = numChannels > 0 ? buffer.getWritePointer(0) : nullptr;
    float* rightChannel = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    float minFreq = centerFreq / 4.0f;
    float maxFreq = centerFreq * 2.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        // LFO generates a sine wave modulation
        float lfoValue = std::sin(lfoPhase * 2.0f * juce::MathConstants<float>::pi);
        lfoPhase += lfoIncrement;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

        // Map LFO to frequency range
        float modFreq = minFreq + (maxFreq - minFreq) * (0.5f + 0.5f * lfoValue * depth);

        // Calculate all-pass coefficient from frequency
        float w = modFreq / static_cast<float>(currentSampleRate);
        float coeff = (1.0f - w) / (1.0f + w);

        // Set coefficient for all stages
        for (int i = 0; i < numStages; ++i)
        {
            allPassL[i].a1 = coeff;
            allPassR[i].a1 = coeff;
        }

        // Process left channel
        if (leftChannel != nullptr)
        {
            float input = leftChannel[s] + feedbackSampleL * feedback;
            float filtered = input;
            for (int i = 0; i < numStages; ++i)
                filtered = allPassL[i].process(filtered);

            feedbackSampleL = filtered;
            leftChannel[s] = leftChannel[s] * (1.0f - mix) + filtered * mix;
        }

        // Process right channel
        if (rightChannel != nullptr)
        {
            float input = rightChannel[s] + feedbackSampleR * feedback;
            float filtered = input;
            for (int i = 0; i < numStages; ++i)
                filtered = allPassR[i].process(filtered);

            feedbackSampleR = filtered;
            rightChannel[s] = rightChannel[s] * (1.0f - mix) + filtered * mix;
        }
    }
}

void Phaser::setRate(float rate)
{
    rateHz = juce::jlimit(0.1f, 5.0f, rate);
    lfoIncrement = rateHz / static_cast<float>(currentSampleRate);
}

void Phaser::setDepth(float d) { depth = juce::jlimit(0.0f, 1.0f, d); }
void Phaser::setFeedback(float fb) { feedback = juce::jlimit(0.0f, 0.95f, fb); }
void Phaser::setMix(float m) { mix = juce::jlimit(0.0f, 1.0f, m); }

void Phaser::setNumStages(int stages)
{
    numStages = juce::jlimit(2, MAX_STAGES, stages);
    // Ensure it's even
    if (numStages % 2 != 0) numStages--;
}

void Phaser::setCenterFreq(float freq) { centerFreq = juce::jlimit(200.0f, 5000.0f, freq); }

std::unique_ptr<juce::XmlElement> Phaser::createXml() const
{
    auto xml = Effect::createXml();
    xml->setAttribute("type", "Phaser");
    xml->setAttribute("rate", static_cast<double>(rateHz));
    xml->setAttribute("depth", static_cast<double>(depth));
    xml->setAttribute("feedback", static_cast<double>(feedback));
    xml->setAttribute("mix", static_cast<double>(mix));
    xml->setAttribute("numStages", numStages);
    xml->setAttribute("centerFreq", static_cast<double>(centerFreq));
    return xml;
}

void Phaser::loadFromXml(const juce::XmlElement& xml)
{
    Effect::loadFromXml(xml);
    rateHz = static_cast<float>(xml.getDoubleAttribute("rate", 0.5));
    depth = static_cast<float>(xml.getDoubleAttribute("depth", 0.7));
    feedback = static_cast<float>(xml.getDoubleAttribute("feedback", 0.3));
    mix = static_cast<float>(xml.getDoubleAttribute("mix", 0.5));
    numStages = xml.getIntAttribute("numStages", 6);
    centerFreq = static_cast<float>(xml.getDoubleAttribute("centerFreq", 1000.0));
}
