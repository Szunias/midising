#include "GroupBus.h"

//==============================================================================
// GroupBus Implementation
//==============================================================================

GroupBus::GroupBus(const juce::String& name)
    : Track(name, TrackType::Audio)  // Group busses are audio-type for signal flow
{
}

void GroupBus::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    // Pre-allocate stereo input buffer
    inputBuffer.setSize(2, samplesPerBlock);
    inputBuffer.clear();

    // Prepare effect chain
    effectChain.prepareToPlay(sampleRate, samplesPerBlock);
}

void GroupBus::processBlock(juce::AudioBuffer<float>& buffer, int /*startSample*/, int numSamples)
{
    // GroupBus doesn't use regions - it processes accumulated input
    // This method processes the effect chain on the input buffer
    // and copies to the output buffer
    processAccumulatedInput(buffer, numSamples);
}

void GroupBus::releaseResources()
{
    effectChain.releaseResources();
    inputBuffer.setSize(0, 0);
}

void GroupBus::clearInputBuffer()
{
    inputBuffer.clear();
}

void GroupBus::addToInputBuffer(const juce::AudioBuffer<float>& sourceBuffer,
                                float leftGain, float rightGain, int numSamples)
{
    // Accumulate audio from track outputs into the input buffer
    // The gains already include volume and pan from the source track
    int samplesToProcess = juce::jmin(numSamples, inputBuffer.getNumSamples(), sourceBuffer.getNumSamples());

    // Add left channel
    if (inputBuffer.getNumChannels() >= 1 && sourceBuffer.getNumChannels() >= 1)
    {
        inputBuffer.addFrom(0, 0, sourceBuffer, 0, 0, samplesToProcess, leftGain);
    }

    // Add right channel
    if (inputBuffer.getNumChannels() >= 2)
    {
        if (sourceBuffer.getNumChannels() >= 2)
        {
            inputBuffer.addFrom(1, 0, sourceBuffer, 1, 0, samplesToProcess, rightGain);
        }
        else if (sourceBuffer.getNumChannels() >= 1)
        {
            // Mono source: use right gain on the mono channel for right output
            inputBuffer.addFrom(1, 0, sourceBuffer, 0, 0, samplesToProcess, rightGain);
        }
    }
}

void GroupBus::processAccumulatedInput(juce::AudioBuffer<float>& outputBuffer, int numSamples)
{
    // Copy input buffer to output for processing
    int channelsToProcess = juce::jmin(outputBuffer.getNumChannels(), inputBuffer.getNumChannels(), 2);
    int samplesToProcess = juce::jmin(numSamples, outputBuffer.getNumSamples(), inputBuffer.getNumSamples());

    outputBuffer.clear();

    for (int ch = 0; ch < channelsToProcess; ++ch)
    {
        outputBuffer.copyFrom(ch, 0, inputBuffer, ch, 0, samplesToProcess);
    }

    // Process through effect chain
    effectChain.processBlock(outputBuffer);

    // Update peak metering
    if (outputBuffer.getNumChannels() >= 1)
    {
        float peak = outputBuffer.getMagnitude(0, 0, samplesToProcess);
        peakLevelLeft.store(peak);
    }
    if (outputBuffer.getNumChannels() >= 2)
    {
        float peak = outputBuffer.getMagnitude(1, 0, samplesToProcess);
        peakLevelRight.store(peak);
    }
}

int GroupBus::addInsert(std::unique_ptr<Effect> effect)
{
    if (effectChain.getNumEffects() >= MAX_INSERT_SLOTS)
        return -1;

    int slotIndex = effectChain.getNumEffects();
    effectChain.addEffect(std::move(effect));
    return slotIndex;
}

bool GroupBus::addInsertAtSlot(int slotIndex, std::unique_ptr<Effect> effect)
{
    if (slotIndex < 0 || slotIndex >= MAX_INSERT_SLOTS)
        return false;

    if (effectChain.getNumEffects() >= MAX_INSERT_SLOTS)
        return false;

    // For simplicity, add at end (slot management would need more complex implementation)
    effectChain.addEffect(std::move(effect));
    return true;
}

void GroupBus::removeInsert(int slotIndex)
{
    if (slotIndex >= 0 && slotIndex < effectChain.getNumEffects())
    {
        effectChain.removeEffect(slotIndex);
    }
}

void GroupBus::moveInsert(int fromSlot, int toSlot)
{
    effectChain.moveEffect(fromSlot, toSlot);
}

Effect* GroupBus::getInsert(int slotIndex)
{
    return effectChain.getEffect(slotIndex);
}

const Effect* GroupBus::getInsert(int slotIndex) const
{
    return const_cast<EffectChain&>(effectChain).getEffect(slotIndex);
}

bool GroupBus::isInsertSlotEmpty(int slotIndex) const
{
    return slotIndex >= effectChain.getNumEffects();
}

void GroupBus::clearInserts()
{
    effectChain.clear();
}

void GroupBus::setInsertBypassed(int slotIndex, bool bypassed)
{
    auto* effect = effectChain.getEffect(slotIndex);
    if (effect != nullptr)
    {
        effect->setBypass(bypassed);
    }
}

bool GroupBus::isInsertBypassed(int slotIndex) const
{
    auto* effect = const_cast<EffectChain&>(effectChain).getEffect(slotIndex);
    return effect != nullptr ? effect->isBypassed() : false;
}

std::unique_ptr<juce::XmlElement> GroupBus::createXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("GROUPBUS");
    xml->setAttribute("name", getName());
    xml->setAttribute("volume", static_cast<double>(getVolume()));
    xml->setAttribute("pan", static_cast<double>(getPan()));
    xml->setAttribute("muted", isMuted());
    xml->setAttribute("soloed", isSoloed());

    // Save colour
    xml->setAttribute("colour", getColour().toString());

    // Save effect chain
    if (effectChain.getNumEffects() > 0)
    {
        auto effectsXml = effectChain.createXml();
        if (effectsXml != nullptr)
            xml->addChildElement(effectsXml.release());
    }

    return xml;
}

void GroupBus::loadFromXml(const juce::XmlElement& xml)
{
    setName(xml.getStringAttribute("name", "Group"));
    setVolume(static_cast<float>(xml.getDoubleAttribute("volume", 1.0)));
    setPan(static_cast<float>(xml.getDoubleAttribute("pan", 0.0)));
    setMuted(xml.getBoolAttribute("muted", false));
    setSoloed(xml.getBoolAttribute("soloed", false));

    // Load colour
    auto colourStr = xml.getStringAttribute("colour", "");
    if (colourStr.isNotEmpty())
        setColour(juce::Colour::fromString(colourStr));

    // Load effect chain
    auto* effectsXml = xml.getChildByName("EFFECTCHAIN");
    if (effectsXml != nullptr)
    {
        effectChain.loadFromXml(*effectsXml);
    }
}
