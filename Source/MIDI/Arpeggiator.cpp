#include "Arpeggiator.h"
#include <cmath>

Arpeggiator::Arpeggiator()
    : MidiEffect("Arpeggiator")
{
}

void Arpeggiator::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::ignoreUnused(samplesPerBlock);
    reset();
}

void Arpeggiator::reset()
{
    heldNotes.clear();
    samplePosition = 0.0;
    currentNoteIndex = -1;
    currentPlayingNote = -1;
    goingUp = true;
}

void Arpeggiator::processMidi(juce::MidiBuffer& buffer, int numSamples,
                               double sampleRate, double bpm)
{
    if (bypass || bpm <= 0.0)
        return;

    currentSampleRate = sampleRate;

    // Collect incoming note-on/off events and remove them from the buffer
    juce::MidiBuffer outputBuffer;
    std::vector<std::pair<int, juce::MidiMessage>> nonNoteEvents;

    for (const auto metadata : buffer)
    {
        auto msg = metadata.getMessage();
        int samplePos = metadata.samplePosition;

        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            // Add to held notes if not already present
            if (std::find(heldNotes.begin(), heldNotes.end(), note) == heldNotes.end())
            {
                heldNotes.push_back(note);
            }
            currentPlayingChannel = msg.getChannel();
        }
        else if (msg.isNoteOff())
        {
            int note = msg.getNoteNumber();
            heldNotes.erase(
                std::remove(heldNotes.begin(), heldNotes.end(), note),
                heldNotes.end());

            // If no notes held, send note-off for any currently playing note
            if (heldNotes.empty() && currentPlayingNote >= 0)
            {
                outputBuffer.addEvent(
                    juce::MidiMessage::noteOff(currentPlayingChannel, currentPlayingNote, 0.0f),
                    samplePos);
                currentPlayingNote = -1;
                currentNoteIndex = -1;
                samplePosition = 0.0;
            }
        }
        else
        {
            // Pass through non-note events (CC, pitch bend, etc.)
            nonNoteEvents.push_back({ samplePos, msg });
        }
    }

    // If no notes are held, output non-note events only
    if (heldNotes.empty())
    {
        buffer.clear();
        for (auto& [pos, msg] : nonNoteEvents)
            outputBuffer.addEvent(msg, pos);
        buffer.swapWith(outputBuffer);
        return;
    }

    // Build the arpeggiated note sequence
    auto noteSequence = buildNoteSequence();

    if (noteSequence.empty())
    {
        buffer.clear();
        for (auto& [pos, msg] : nonNoteEvents)
            outputBuffer.addEvent(msg, pos);
        buffer.swapWith(outputBuffer);
        return;
    }

    // Calculate step length in samples
    double beatsPerStep = getRateDivisor();
    double samplesPerBeat = (sampleRate * 60.0) / bpm;
    double samplesPerStep = samplesPerBeat * beatsPerStep;
    double gateOnSamples = samplesPerStep * gateLength;

    // Process each sample in the block
    for (int i = 0; i < numSamples; ++i)
    {
        double positionInStep = std::fmod(samplePosition, samplesPerStep);

        // Step boundary: trigger new note
        if (positionInStep < 1.0)
        {
            // Turn off previous note
            if (currentPlayingNote >= 0)
            {
                outputBuffer.addEvent(
                    juce::MidiMessage::noteOff(currentPlayingChannel, currentPlayingNote, 0.0f), i);
                currentPlayingNote = -1;
            }

            // Advance to next note in sequence
            int seqSize = static_cast<int>(noteSequence.size());
            currentNoteIndex = getNextNoteIndex(currentNoteIndex, seqSize);

            if (currentNoteIndex >= 0 && currentNoteIndex < seqSize)
            {
                int noteToPlay = noteSequence[static_cast<size_t>(currentNoteIndex)];
                // Clamp to valid MIDI range
                noteToPlay = juce::jlimit(0, 127, noteToPlay);

                outputBuffer.addEvent(
                    juce::MidiMessage::noteOn(currentPlayingChannel, noteToPlay, 0.8f), i);
                currentPlayingNote = noteToPlay;
            }
        }
        // Gate off: turn off note before end of step
        else if (positionInStep >= gateOnSamples && positionInStep < gateOnSamples + 1.0)
        {
            if (currentPlayingNote >= 0)
            {
                outputBuffer.addEvent(
                    juce::MidiMessage::noteOff(currentPlayingChannel, currentPlayingNote, 0.0f), i);
                currentPlayingNote = -1;
            }
        }

        samplePosition += 1.0;
    }

    // Add non-note events back
    for (auto& [pos, msg] : nonNoteEvents)
        outputBuffer.addEvent(msg, pos);

    buffer.clear();
    buffer.swapWith(outputBuffer);
}

void Arpeggiator::setPattern(ArpPattern newPattern) { pattern = newPattern; }
ArpPattern Arpeggiator::getPattern() const { return pattern; }

void Arpeggiator::setRate(ArpRate newRate) { rate = newRate; }
ArpRate Arpeggiator::getRate() const { return rate; }

void Arpeggiator::setOctaveRange(int octaves)
{
    octaveRange = juce::jlimit(1, 4, octaves);
}

int Arpeggiator::getOctaveRange() const { return octaveRange; }

void Arpeggiator::setGateLength(float length)
{
    gateLength = juce::jlimit(0.1f, 1.0f, length);
}

float Arpeggiator::getGateLength() const { return gateLength; }

double Arpeggiator::getRateDivisor() const
{
    switch (rate)
    {
        case ArpRate::Quarter:          return 1.0;
        case ArpRate::Eighth:           return 0.5;
        case ArpRate::Sixteenth:        return 0.25;
        case ArpRate::DottedEighth:     return 0.75;
        case ArpRate::DottedSixteenth:  return 0.375;
        default:                        return 0.5;
    }
}

std::vector<int> Arpeggiator::buildNoteSequence() const
{
    if (heldNotes.empty())
        return {};

    // Start with a sorted copy of held notes for Up/Down/UpDown patterns
    std::vector<int> baseNotes = heldNotes;

    if (pattern != ArpPattern::AsPlayed && pattern != ArpPattern::Random)
    {
        std::sort(baseNotes.begin(), baseNotes.end());
    }

    // Expand across octave range
    std::vector<int> expanded;
    for (int octave = 0; octave < octaveRange; ++octave)
    {
        for (int note : baseNotes)
        {
            int transposed = note + (octave * 12);
            if (transposed <= 127)
                expanded.push_back(transposed);
        }
    }

    if (expanded.empty())
        return {};

    // Apply pattern ordering
    switch (pattern)
    {
        case ArpPattern::Up:
        case ArpPattern::AsPlayed:
        case ArpPattern::Random:
            // Already in correct order (sorted for Up, original for AsPlayed/Random)
            break;

        case ArpPattern::Down:
            std::reverse(expanded.begin(), expanded.end());
            break;

        case ArpPattern::UpDown:
            // Up then down (excluding duplicate endpoints)
            if (expanded.size() > 1)
            {
                std::vector<int> upDown = expanded;
                for (int i = static_cast<int>(expanded.size()) - 2; i >= 1; --i)
                    upDown.push_back(expanded[static_cast<size_t>(i)]);
                return upDown;
            }
            break;
    }

    return expanded;
}

int Arpeggiator::getNextNoteIndex(int currentIndex, int sequenceSize)
{
    if (sequenceSize <= 0)
        return -1;

    if (pattern == ArpPattern::Random)
    {
        return randomGenerator.nextInt(sequenceSize);
    }

    // For UpDown, the sequence already contains the full up-then-down path
    // so simple increment works
    int nextIndex = currentIndex + 1;
    if (nextIndex >= sequenceSize)
        nextIndex = 0;

    return nextIndex;
}

std::unique_ptr<juce::XmlElement> Arpeggiator::createXml() const
{
    auto xml = MidiEffect::createXml();
    xml->setAttribute("pattern", static_cast<int>(pattern));
    xml->setAttribute("rate", static_cast<int>(rate));
    xml->setAttribute("octaveRange", octaveRange);
    xml->setAttribute("gateLength", static_cast<double>(gateLength));
    return xml;
}

void Arpeggiator::loadFromXml(const juce::XmlElement& xml)
{
    MidiEffect::loadFromXml(xml);
    pattern = static_cast<ArpPattern>(xml.getIntAttribute("pattern", static_cast<int>(ArpPattern::Up)));
    rate = static_cast<ArpRate>(xml.getIntAttribute("rate", static_cast<int>(ArpRate::Eighth)));
    octaveRange = juce::jlimit(1, 4, xml.getIntAttribute("octaveRange", 1));
    gateLength = juce::jlimit(0.1f, 1.0f,
        static_cast<float>(xml.getDoubleAttribute("gateLength", 0.8)));
}
