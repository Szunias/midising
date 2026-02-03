#include "SendReturn.h"

//==============================================================================
// AuxTrack Implementation
//==============================================================================

AuxTrack::AuxTrack(const juce::String& name)
    : Track(name, TrackType::Audio)  // Aux tracks are audio-type for signal flow
{
}

void AuxTrack::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    // Pre-allocate stereo input buffer
    inputBuffer.setSize(2, samplesPerBlock);
    inputBuffer.clear();

    // Prepare effect chain
    effectChain.prepareToPlay(sampleRate, samplesPerBlock);
}

void AuxTrack::processBlock(juce::AudioBuffer<float>& buffer, int /*startSample*/, int numSamples)
{
    // AuxTrack doesn't use regions - it processes accumulated input
    // This method processes the effect chain on the input buffer
    // and copies to the output buffer

    processAccumulatedInput(buffer, numSamples);
}

void AuxTrack::releaseResources()
{
    effectChain.releaseResources();
    inputBuffer.setSize(0, 0);
}

void AuxTrack::clearInputBuffer()
{
    inputBuffer.clear();
}

void AuxTrack::addToInputBuffer(const juce::AudioBuffer<float>& sourceBuffer, float gain, int numSamples)
{
    // Accumulate audio from sends into the input buffer
    int channelsToProcess = juce::jmin(inputBuffer.getNumChannels(), sourceBuffer.getNumChannels(), 2);
    int samplesToProcess = juce::jmin(numSamples, inputBuffer.getNumSamples(), sourceBuffer.getNumSamples());

    for (int ch = 0; ch < channelsToProcess; ++ch)
    {
        inputBuffer.addFrom(ch, 0, sourceBuffer, ch, 0, samplesToProcess, gain);
    }

    // Handle mono source to stereo aux
    if (sourceBuffer.getNumChannels() == 1 && inputBuffer.getNumChannels() >= 2)
    {
        inputBuffer.addFrom(1, 0, sourceBuffer, 0, 0, samplesToProcess, gain);
    }
}

void AuxTrack::processAccumulatedInput(juce::AudioBuffer<float>& outputBuffer, int numSamples)
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

int AuxTrack::addInsert(std::unique_ptr<Effect> effect)
{
    if (effectChain.getNumEffects() >= MAX_INSERT_SLOTS)
        return -1;

    int slotIndex = effectChain.getNumEffects();
    effectChain.addEffect(std::move(effect));
    return slotIndex;
}

bool AuxTrack::addInsertAtSlot(int slotIndex, std::unique_ptr<Effect> effect)
{
    if (slotIndex < 0 || slotIndex >= MAX_INSERT_SLOTS)
        return false;

    if (effectChain.getNumEffects() >= MAX_INSERT_SLOTS)
        return false;

    // For simplicity, add at end (slot management would need more complex implementation)
    effectChain.addEffect(std::move(effect));
    return true;
}

void AuxTrack::removeInsert(int slotIndex)
{
    if (slotIndex >= 0 && slotIndex < effectChain.getNumEffects())
    {
        effectChain.removeEffect(slotIndex);
    }
}

void AuxTrack::moveInsert(int fromSlot, int toSlot)
{
    effectChain.moveEffect(fromSlot, toSlot);
}

Effect* AuxTrack::getInsert(int slotIndex)
{
    return effectChain.getEffect(slotIndex);
}

const Effect* AuxTrack::getInsert(int slotIndex) const
{
    return const_cast<EffectChain&>(effectChain).getEffect(slotIndex);
}

bool AuxTrack::isInsertSlotEmpty(int slotIndex) const
{
    return slotIndex >= effectChain.getNumEffects();
}

void AuxTrack::clearInserts()
{
    effectChain.clear();
}

void AuxTrack::setInsertBypassed(int slotIndex, bool bypassed)
{
    auto* effect = effectChain.getEffect(slotIndex);
    if (effect != nullptr)
    {
        effect->setBypass(bypassed);
    }
}

bool AuxTrack::isInsertBypassed(int slotIndex) const
{
    auto* effect = const_cast<EffectChain&>(effectChain).getEffect(slotIndex);
    return effect != nullptr ? effect->isBypassed() : false;
}

std::unique_ptr<juce::XmlElement> AuxTrack::createXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("AUXTRACK");
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

void AuxTrack::loadFromXml(const juce::XmlElement& xml)
{
    setName(xml.getStringAttribute("name", "Aux"));
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

//==============================================================================
// SendManager Implementation
//==============================================================================

SendManager::SendManager()
{
    sends.reserve(MAX_SENDS);
}

int SendManager::addSend(int destAuxIndex, float level, bool preFader)
{
    if (static_cast<int>(sends.size()) >= MAX_SENDS)
        return -1;

    sends.emplace_back(destAuxIndex, level, preFader);
    return static_cast<int>(sends.size()) - 1;
}

void SendManager::removeSend(int sendIndex)
{
    if (sendIndex >= 0 && sendIndex < static_cast<int>(sends.size()))
    {
        sends.erase(sends.begin() + sendIndex);
    }
}

Send* SendManager::getSend(int index)
{
    if (index >= 0 && index < static_cast<int>(sends.size()))
        return &sends[static_cast<size_t>(index)];
    return nullptr;
}

const Send* SendManager::getSend(int index) const
{
    if (index >= 0 && index < static_cast<int>(sends.size()))
        return &sends[static_cast<size_t>(index)];
    return nullptr;
}

void SendManager::setSendLevel(int sendIndex, float level)
{
    if (auto* send = getSend(sendIndex))
    {
        send->sendLevel.store(juce::jlimit(0.0f, 1.0f, level));
    }
}

float SendManager::getSendLevel(int sendIndex) const
{
    if (auto* send = getSend(sendIndex))
    {
        return send->sendLevel.load();
    }
    return 0.0f;
}

void SendManager::setSendEnabled(int sendIndex, bool enabled)
{
    if (auto* send = getSend(sendIndex))
    {
        send->enabled.store(enabled);
    }
}

bool SendManager::isSendEnabled(int sendIndex) const
{
    if (auto* send = getSend(sendIndex))
    {
        return send->enabled.load();
    }
    return false;
}

void SendManager::setSendPreFader(int sendIndex, bool preFader)
{
    if (auto* send = getSend(sendIndex))
    {
        send->preFader.store(preFader);
    }
}

bool SendManager::isSendPreFader(int sendIndex) const
{
    if (auto* send = getSend(sendIndex))
    {
        return send->preFader.load();
    }
    return false;
}

void SendManager::clearSends()
{
    sends.clear();
}

int SendManager::findSendToAux(int auxIndex) const
{
    for (size_t i = 0; i < sends.size(); ++i)
    {
        if (sends[i].destAuxIndex == auxIndex)
            return static_cast<int>(i);
    }
    return -1;
}

std::unique_ptr<juce::XmlElement> SendManager::createXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("SENDS");

    for (const auto& send : sends)
    {
        xml->addChildElement(send.createXml().release());
    }

    return xml;
}

void SendManager::loadFromXml(const juce::XmlElement& xml)
{
    sends.clear();

    for (auto* sendXml : xml.getChildWithTagNameIterator("SEND"))
    {
        Send send;
        send.loadFromXml(*sendXml);
        sends.push_back(std::move(send));
    }
}
