#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

enum class MarkerType
{
    Standard,
    Loop
};

struct Marker
{
    juce::String name;
    int64_t position = 0;       // Position in samples
    juce::Colour colour = juce::Colours::orange;
    MarkerType type = MarkerType::Standard;

    bool operator<(const Marker& other) const { return position < other.position; }
};
