#pragma once

#include <juce_core/juce_core.h>

/**
 * Identifies the target of an automation lane.
 * Can be a built-in parameter (Volume, Pan) or an effect/plugin parameter.
 */
struct AutomationTarget
{
    enum class Type
    {
        Volume,
        Pan,
        Mute,
        Send,
        EffectParam,
        PluginParam
    };

    Type targetType = Type::Volume;
    int effectIndex = -1;          // Slot index (0-7) for EffectParam
    juce::String parameterId;      // Effect parameter id (e.g. "threshold")
    juce::String parameterName;    // Display name (e.g. "Compressor: Threshold")

    /** Unique key for lane lookup. */
    juce::String getKey() const
    {
        switch (targetType)
        {
            case Type::Volume:      return "Volume";
            case Type::Pan:         return "Pan";
            case Type::Mute:        return "Mute";
            case Type::Send:        return "Send";
            case Type::EffectParam: return "FX" + juce::String(effectIndex) + ":" + parameterId;
            case Type::PluginParam: return "Plugin:" + parameterId;
            default:                return "Unknown";
        }
    }

    bool isEffectParam() const { return targetType == Type::EffectParam; }
    bool isPluginParam() const { return targetType == Type::PluginParam; }

    bool operator==(const AutomationTarget& other) const
    {
        return getKey() == other.getKey();
    }
};
