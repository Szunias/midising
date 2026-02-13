#include "Saturation.h"

Saturation::Saturation()
    : Effect("Saturation")
{
}

void Saturation::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    driveLinear = juce::Decibels::decibelsToGain(driveDb);
    outputLinear = juce::Decibels::decibelsToGain(outputLevelDb);

    // Initialize 2x oversampling (no filtering for minimal latency, IIR for quality)
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2,  // numChannels
        1,  // oversampling order (2^1 = 2x)
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        false  // isMaximumQuality
    );
    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));
    oversamplingReady = true;
}

void Saturation::releaseResources()
{
    if (oversampling != nullptr)
        oversampling->reset();
    oversamplingReady = false;
}

float Saturation::waveshape(float input) const
{
    switch (satType)
    {
        case Type::Soft:
            // Soft saturation using tanh
            return std::tanh(input);

        case Type::Hard:
            // Hard clipping
            return juce::jlimit(-1.0f, 1.0f, input);

        case Type::Tape:
        {
            // Asymmetric tape-like saturation
            if (input >= 0.0f)
                return std::tanh(input);
            else
                return std::tanh(input * 0.8f) * 1.1f;  // Softer negative, slight boost
        }

        case Type::Tube:
        {
            // Asymmetric tube-like saturation (even harmonics)
            float sign = input >= 0.0f ? 1.0f : -1.0f;
            float absInput = std::abs(input);
            float shaped = 1.0f - std::exp(-absInput);
            // Add slight even-harmonic character
            float even = 0.1f * (1.0f - std::exp(-absInput * absInput));
            return (shaped * sign) + even;
        }

        default:
            return std::tanh(input);
    }
}

void Saturation::processBlock(juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    if (!oversamplingReady || oversampling == nullptr)
    {
        // Fallback: process without oversampling
        for (int sample = 0; sample < numSamples; ++sample)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float dry = buffer.getSample(ch, sample);
                float driven = dry * driveLinear;
                float wet = waveshape(driven) * outputLinear;
                buffer.setSample(ch, sample, dry * (1.0f - mix) + wet * mix);
            }
        }
        return;
    }

    // Create a dsp::AudioBlock from the buffer
    juce::dsp::AudioBlock<float> block(buffer);

    // Upsample
    auto oversampledBlock = oversampling->processSamplesUp(block);

    int oversampledNumSamples = static_cast<int>(oversampledBlock.getNumSamples());
    int oversampledNumChannels = static_cast<int>(oversampledBlock.getNumChannels());

    // Process with waveshaping at oversampled rate
    for (int sample = 0; sample < oversampledNumSamples; ++sample)
    {
        for (int ch = 0; ch < oversampledNumChannels; ++ch)
        {
            float* channelData = oversampledBlock.getChannelPointer(static_cast<size_t>(ch));
            float dry = channelData[sample];
            float driven = dry * driveLinear;
            float wet = waveshape(driven) * outputLinear;
            channelData[sample] = dry * (1.0f - mix) + wet * mix;
        }
    }

    // Downsample back
    oversampling->processSamplesDown(block);
}

void Saturation::setSaturationType(Type type)
{
    satType = type;
}

void Saturation::setDrive(float db)
{
    driveDb = juce::jlimit(0.0f, 36.0f, db);
    driveLinear = juce::Decibels::decibelsToGain(driveDb);
}

void Saturation::setMix(float m)
{
    mix = juce::jlimit(0.0f, 1.0f, m);
}

void Saturation::setOutputLevel(float db)
{
    outputLevelDb = juce::jlimit(-24.0f, 0.0f, db);
    outputLinear = juce::Decibels::decibelsToGain(outputLevelDb);
}

std::unique_ptr<juce::XmlElement> Saturation::createXml() const
{
    auto xml = Effect::createXml();
    xml->setAttribute("type", "Saturation");
    xml->setAttribute("satType", static_cast<int>(satType));
    xml->setAttribute("drive", driveDb);
    xml->setAttribute("mix", mix);
    xml->setAttribute("outputLevel", outputLevelDb);
    return xml;
}

void Saturation::loadFromXml(const juce::XmlElement& xml)
{
    Effect::loadFromXml(xml);
    satType = static_cast<Type>(xml.getIntAttribute("satType", 0));
    driveDb = static_cast<float>(xml.getDoubleAttribute("drive", 12.0));
    mix = static_cast<float>(xml.getDoubleAttribute("mix", 1.0));
    outputLevelDb = static_cast<float>(xml.getDoubleAttribute("outputLevel", 0.0));
    driveLinear = juce::Decibels::decibelsToGain(driveDb);
    outputLinear = juce::Decibels::decibelsToGain(outputLevelDb);
}
