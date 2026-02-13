#pragma once

#include "../Effects/Effect.h"
#include "../Effects/EffectParameter.h"
#include "../Effects/GainEffect.h"
#include "../Effects/SimpleEQEffect.h"
#include "../Effects/ReverbEffect.h"
#include "../Effects/ParametricEQ.h"
#include "../Effects/VCACompressor.h"
#include "../Effects/StereoDelay.h"
#include "../Effects/NoiseGate.h"
#include "../Effects/Limiter.h"
#include "../Effects/ChorusFlanger.h"
#include "../Effects/Saturation.h"
#include "../Effects/Phaser.h"
#include "../Effects/MultibandCompressor.h"
#include "../Effects/DeEsser.h"
#include <juce_core/juce_core.h>
#include <cmath>

/**
 * Tests for EffectParameter descriptor system.
 * Verifies getParameters/setParameter/getParameter round-trip for all effects.
 */
class EffectParamTest
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;
        int passed = 0;
        int total = 0;

        auto check = [&](bool result, const char* name)
        {
            total++;
            if (result) { passed++; }
            else { allPassed = false; juce::Logger::writeToLog(juce::String("  FAIL: ") + name); }
        };

        // Test each effect has non-empty parameters
        check(testEffectHasParams<GainEffect>("GainEffect"), "GainEffect has params");
        check(testEffectHasParams<SimpleEQEffect>("SimpleEQ"), "SimpleEQ has params");
        check(testEffectHasParams<ReverbEffect>("Reverb"), "Reverb has params");
        check(testEffectHasParams<ParametricEQ>("ParametricEQ"), "ParametricEQ has params");
        check(testEffectHasParams<VCACompressor>("VCACompressor"), "VCACompressor has params");
        check(testEffectHasParams<StereoDelay>("StereoDelay"), "StereoDelay has params");
        check(testEffectHasParams<NoiseGate>("NoiseGate"), "NoiseGate has params");
        check(testEffectHasParams<Limiter>("Limiter"), "Limiter has params");
        check(testEffectHasParams<ChorusFlanger>("ChorusFlanger"), "ChorusFlanger has params");
        check(testEffectHasParams<Saturation>("Saturation"), "Saturation has params");
        check(testEffectHasParams<Phaser>("Phaser"), "Phaser has params");
        check(testEffectHasParams<MultibandCompressor>("MultibandCompressor"), "MultibandCompressor has params");
        check(testEffectHasParams<DeEsser>("DeEsser"), "DeEsser has params");

        // Test setParameter/getParameter round-trip
        check(testRoundTrip<GainEffect>(), "GainEffect round-trip");
        check(testRoundTrip<VCACompressor>(), "VCACompressor round-trip");
        check(testRoundTrip<NoiseGate>(), "NoiseGate round-trip");
        check(testRoundTrip<Limiter>(), "Limiter round-trip");
        check(testRoundTrip<DeEsser>(), "DeEsser round-trip");

        // Test normalized conversion
        check(testNormalization(), "EffectParameter normalization");

        // Test sidechain support
        check(testSidechainSupport(), "VCACompressor sidechain support");

        juce::Logger::writeToLog(juce::String("  Effect Param Tests: ") + juce::String(passed) + "/" + juce::String(total));
        return allPassed;
    }

private:
    template <typename EffectType>
    static bool testEffectHasParams(const char* /*name*/)
    {
        EffectType effect;
        auto params = effect.getParameters();
        return !params.empty();
    }

    template <typename EffectType>
    static bool testRoundTrip()
    {
        EffectType effect;
        effect.prepareToPlay(44100.0, 512);
        auto params = effect.getParameters();

        for (const auto& param : params)
        {
            // Set to a value within range (midpoint)
            float midValue = (param.minValue + param.maxValue) * 0.5f;
            effect.setParameter(param.id, midValue);

            float retrieved = effect.getParameter(param.id);

            // Allow small tolerance for floating point
            float tolerance = (param.step > 0.0f) ? param.step : 0.01f;
            if (std::abs(retrieved - midValue) > tolerance * 2.0f)
                return false;
        }
        return true;
    }

    static bool testNormalization()
    {
        EffectParameter param;
        param.minValue = 0.0f;
        param.maxValue = 100.0f;
        param.skew = 1.0f;

        // Test midpoint
        param.currentValue = 50.0f;
        float norm = param.getNormalized();
        if (std::abs(norm - 0.5f) > 0.001f)
            return false;

        // Test setFromNormalized round-trip
        param.setFromNormalized(0.75f);
        if (std::abs(param.currentValue - 75.0f) > 0.1f)
            return false;

        // Test edges
        param.setFromNormalized(0.0f);
        if (std::abs(param.currentValue - 0.0f) > 0.1f)
            return false;

        param.setFromNormalized(1.0f);
        if (std::abs(param.currentValue - 100.0f) > 0.1f)
            return false;

        return true;
    }

    static bool testSidechainSupport()
    {
        VCACompressor comp;
        if (!comp.supportsSidechain())
            return false;

        GainEffect gain;
        if (gain.supportsSidechain())
            return false;

        return true;
    }
};
