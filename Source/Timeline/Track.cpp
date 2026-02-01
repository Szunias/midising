#include "Track.h"

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
      colour(trackColours[colourIndex++ % 8])
{
}
