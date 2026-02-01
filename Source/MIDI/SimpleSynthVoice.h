#pragma once

#include "SimpleSynthSound.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * SimpleSynthVoice generates audio for a single note.
 * Uses sine wave with ADSR envelope.
 */
class SimpleSynthVoice : public juce::SynthesiserVoice
{
public:
    SimpleSynthVoice() = default;

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SimpleSynthSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound* sound,
                   int currentPitchWheelPosition) override
    {
        juce::ignoreUnused(sound, currentPitchWheelPosition);

        currentAngle = 0.0;
        level = velocity * 0.3; // Scale velocity
        tailOff = 0.0;

        // Calculate frequency from MIDI note
        double frequencyHz = 440.0 * std::pow(2.0, (midiNoteNumber - 69) / 12.0);
        angleDelta = frequencyHz * 2.0 * juce::MathConstants<double>::pi / getSampleRate();

        // Start envelope
        adsr.noteOn();
    }

    void stopNote(float velocity, bool allowTailOff) override
    {
        juce::ignoreUnused(velocity);

        if (allowTailOff)
        {
            adsr.noteOff();
        }
        else
        {
            clearCurrentNote();
            angleDelta = 0.0;
            adsr.reset();
        }
    }

    void pitchWheelMoved(int newValue) override
    {
        juce::ignoreUnused(newValue);
    }

    void controllerMoved(int controllerNumber, int newValue) override
    {
        juce::ignoreUnused(controllerNumber, newValue);
    }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample, int numSamples) override
    {
        if (angleDelta == 0.0)
            return;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Generate sine wave
            float currentSample = static_cast<float>(std::sin(currentAngle) * level);
            
            // Apply envelope
            currentSample *= adsr.getNextSample();

            // Add to all channels
            for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            {
                outputBuffer.addSample(channel, startSample + sample, currentSample);
            }

            // Advance phase
            currentAngle += angleDelta;
            if (currentAngle >= 2.0 * juce::MathConstants<double>::pi)
                currentAngle -= 2.0 * juce::MathConstants<double>::pi;
        }

        // Check if note has finished
        if (!adsr.isActive())
        {
            clearCurrentNote();
            angleDelta = 0.0;
        }
    }

    void setCurrentPlaybackSampleRate(double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate(newRate);
        
        if (newRate > 0)
        {
            // Configure ADSR
            juce::ADSR::Parameters params;
            params.attack = 0.01f;   // 10ms attack
            params.decay = 0.1f;     // 100ms decay
            params.sustain = 0.7f;   // 70% sustain
            params.release = 0.3f;   // 300ms release
            adsr.setParameters(params);
            adsr.setSampleRate(newRate);
        }
    }

private:
    double currentAngle = 0.0;
    double angleDelta = 0.0;
    double level = 0.0;
    double tailOff = 0.0;
    juce::ADSR adsr;
};
