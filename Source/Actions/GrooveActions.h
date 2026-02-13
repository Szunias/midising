#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Timeline/Region.h"
#include "../MIDI/GrooveTemplate.h"
#include <vector>
#include <cmath>

/**
 * ApplyGrooveAction - undoable action for applying a groove template to a MIDI region.
 *
 * Stores the original timestamps and velocities for all note-on events (and their
 * corresponding note-offs) so the groove application can be fully reversed.
 */
class ApplyGrooveAction : public juce::UndoableAction
{
public:
    /**
     * @param region The MIDI region to apply the groove to
     * @param grooveTemplate The groove template to apply
     * @param strength Groove application strength (0.0 - 1.0)
     * @param bpm Current tempo
     * @param sampleRate Current sample rate
     */
    ApplyGrooveAction(MidiRegion& region, const GrooveTemplate& grooveTemplate,
                      float strength, double bpm, double sampleRate)
        : regionRef(region), groove(grooveTemplate),
          grooveStrength(strength), currentBpm(bpm), currentSampleRate(sampleRate)
    {
        // Capture the original state of all note events before applying the groove
        auto& seq = region.getMidiSequence();

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* event = seq.getEventPointer(i);
            if (event == nullptr)
                continue;

            const auto& msg = event->message;

            if (msg.isNoteOn())
            {
                NoteState state;
                state.noteNumber = msg.getNoteNumber();
                state.channel = msg.getChannel();
                state.originalStartTicks = msg.getTimeStamp();
                state.originalVelocity = msg.getFloatVelocity();

                if (event->noteOffObject != nullptr)
                {
                    state.originalEndTicks = event->noteOffObject->message.getTimeStamp();
                }
                else
                {
                    state.originalEndTicks = state.originalStartTicks + 480.0;
                }

                // These will be filled in after the groove is applied
                state.groovedStartTicks = state.originalStartTicks;
                state.groovedEndTicks = state.originalEndTicks;
                state.groovedVelocity = state.originalVelocity;

                originalStates.push_back(state);
            }
        }
    }

    bool perform() override
    {
        if (!wasPerformed && !originalStates.empty())
        {
            // Apply the groove template
            groove.applyToRegion(regionRef, grooveStrength, currentBpm, currentSampleRate);

            // Capture the grooved positions for later undo/redo
            captureCurrentState();

            wasPerformed = true;
        }
        else if (wasPerformed)
        {
            // Re-apply from stored grooved state (for redo)
            applyState(true);
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && !originalStates.empty())
        {
            applyState(false);
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override
    {
        return static_cast<int>(10 + originalStates.size() * 4);
    }

private:
    struct NoteState
    {
        int noteNumber;
        int channel;
        double originalStartTicks;
        double originalEndTicks;
        float originalVelocity;
        double groovedStartTicks;
        double groovedEndTicks;
        float groovedVelocity;
    };

    /**
     * Capture the current state of notes after groove application.
     */
    void captureCurrentState()
    {
        auto& seq = regionRef.getMidiSequence();

        for (auto& state : originalStates)
        {
            // Find the note in the (now grooved) sequence
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* event = seq.getEventPointer(i);
                if (event == nullptr)
                    continue;

                const auto& msg = event->message;

                if (msg.isNoteOn() &&
                    msg.getNoteNumber() == state.noteNumber &&
                    msg.getChannel() == state.channel)
                {
                    // Match by proximity - the grooved position should be
                    // close to the original position
                    double timeDiff = std::abs(msg.getTimeStamp() - state.originalStartTicks);
                    double gridSize = groove.getGridSize();
                    double threshold = (gridSize > 0.0) ? gridSize * 0.5 : 480.0;

                    if (timeDiff < threshold)
                    {
                        state.groovedStartTicks = msg.getTimeStamp();
                        state.groovedVelocity = msg.getFloatVelocity();

                        if (event->noteOffObject != nullptr)
                        {
                            state.groovedEndTicks = event->noteOffObject->message.getTimeStamp();
                        }
                        else
                        {
                            double duration = state.originalEndTicks - state.originalStartTicks;
                            state.groovedEndTicks = state.groovedStartTicks + duration;
                        }
                        break;
                    }
                }
            }
        }
    }

    /**
     * Apply either the original or grooved state to the sequence.
     * @param applyGrooved If true, apply grooved state; if false, restore originals
     */
    void applyState(bool applyGrooved)
    {
        auto& seq = regionRef.getMidiSequence();

        for (const auto& state : originalStates)
        {
            double searchStart = applyGrooved ? state.originalStartTicks : state.groovedStartTicks;
            double targetStart = applyGrooved ? state.groovedStartTicks : state.originalStartTicks;
            double targetEnd = applyGrooved ? state.groovedEndTicks : state.originalEndTicks;
            float targetVelocity = applyGrooved ? state.groovedVelocity : state.originalVelocity;

            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* event = seq.getEventPointer(i);
                if (event == nullptr)
                    continue;

                auto& msg = event->message;

                if (msg.isNoteOn() &&
                    msg.getNoteNumber() == state.noteNumber &&
                    msg.getChannel() == state.channel &&
                    std::abs(msg.getTimeStamp() - searchStart) < 2.0)
                {
                    msg.setTimeStamp(targetStart);
                    msg.setVelocity(targetVelocity);

                    if (event->noteOffObject != nullptr)
                    {
                        event->noteOffObject->message.setTimeStamp(targetEnd);
                    }
                    break;
                }
            }
        }

        seq.sort();
        seq.updateMatchedPairs();
    }

    MidiRegion& regionRef;
    GrooveTemplate groove;
    float grooveStrength;
    double currentBpm;
    double currentSampleRate;
    std::vector<NoteState> originalStates;
    bool wasPerformed = false;
};
