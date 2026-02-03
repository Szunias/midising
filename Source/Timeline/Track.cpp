#include "Track.h"
#include "AutomationLane.h"
#include "../Audio/SendReturn.h"

// Predefined track colors for visual variety
static const juce::Colour trackColours[] = {
    juce::Colour(0xff5a9fd4),  // Blue
    juce::Colour(0xff6bba75),  // Green
    juce::Colour(0xffd4605a),  // Red
    juce::Colour(0xffd4a85a),  // Orange
    juce::Colour(0xffa85ad4),  // Purple
    juce::Colour(0xff5ad4cf),  // Cyan
    juce::Colour(0xffd45a9f),  // Pink
    juce::Colour(0xffd4cf5a),  // Yellow
};

static int colourIndex = 0;

Track::Track(const juce::String& trackName, TrackType trackType)
    : name(trackName),
      type(trackType),
      colour(trackColours[colourIndex++ % 8]),
      sendManager(std::make_unique<SendManager>())
{
}

Track::~Track() = default;

AutomationLane* Track::addAutomationLane(const juce::String& parameterName)
{
    // Determine parameter type based on name
    AutomationParameterType paramType = AutomationParameterType::Custom;

    if (parameterName.equalsIgnoreCase("Volume"))
        paramType = AutomationParameterType::Volume;
    else if (parameterName.equalsIgnoreCase("Pan"))
        paramType = AutomationParameterType::Pan;
    else if (parameterName.equalsIgnoreCase("Mute"))
        paramType = AutomationParameterType::Mute;
    else if (parameterName.containsIgnoreCase("Send"))
        paramType = AutomationParameterType::Send;

    auto lane = std::make_unique<AutomationLane>(paramType, parameterName);
    AutomationLane* lanePtr = lane.get();
    automationLanes.push_back(std::move(lane));
    return lanePtr;
}

void Track::removeAutomationLane(size_t index)
{
    if (index < automationLanes.size())
    {
        automationLanes.erase(automationLanes.begin() + static_cast<long>(index));
    }
}

AutomationLane* Track::getAutomationLane(size_t index)
{
    if (index < automationLanes.size())
        return automationLanes[index].get();
    return nullptr;
}

const AutomationLane* Track::getAutomationLane(size_t index) const
{
    if (index < automationLanes.size())
        return automationLanes[index].get();
    return nullptr;
}

AutomationLane* Track::findAutomationLane(const juce::String& parameterName)
{
    for (auto& lane : automationLanes)
    {
        if (lane->getParameterName().equalsIgnoreCase(parameterName))
            return lane.get();
    }
    return nullptr;
}

const AutomationLane* Track::findAutomationLane(const juce::String& parameterName) const
{
    for (const auto& lane : automationLanes)
    {
        if (lane->getParameterName().equalsIgnoreCase(parameterName))
            return lane.get();
    }
    return nullptr;
}

float Track::getAutomatedVolume(int64_t position) const
{
    const auto* volumeLane = findAutomationLane("Volume");
    if (volumeLane && volumeLane->hasAutomation() && !volumeLane->isBypassed())
    {
        return volumeLane->getScaledValueAtPosition(position);
    }
    return volume.load();
}

float Track::getAutomatedPan(int64_t position) const
{
    const auto* panLane = findAutomationLane("Pan");
    if (panLane && panLane->hasAutomation() && !panLane->isBypassed())
    {
        return panLane->getScaledValueAtPosition(position);
    }
    return pan.load();
}

//==============================================================================
// Send routing
//==============================================================================

SendManager& Track::getSendManager()
{
    return *sendManager;
}

const SendManager& Track::getSendManager() const
{
    return *sendManager;
}

bool Track::hasActiveSends() const
{
    if (sendManager == nullptr)
        return false;

    for (int i = 0; i < sendManager->getNumSends(); ++i)
    {
        const auto* send = sendManager->getSend(i);
        if (send != nullptr && send->enabled.load() && send->destAuxIndex >= 0)
            return true;
    }
    return false;
}
