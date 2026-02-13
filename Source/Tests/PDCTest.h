#pragma once

#include "../Audio/DelayCompensation.h"
#include <juce_core/juce_core.h>
#include <cmath>

/**
 * Tests for Plugin Delay Compensation system.
 * Verifies CompensationDelay and DelayCompensationManager.
 */
class PDCTest
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

        check(testCompensationDelayBasic(), "CompensationDelay basic delay");
        check(testCompensationDelayZero(), "CompensationDelay zero delay passthrough");
        check(testManagerRecalculate(), "Manager recalculate compensation");
        check(testManagerEqualLatency(), "Manager equal latency = zero compensation");
        check(testManagerSingleHighLatency(), "Manager single high-latency track");

        juce::Logger::writeToLog(juce::String("  PDC Tests: ") + juce::String(passed) + "/" + juce::String(total));
        return allPassed;
    }

private:
    static bool testCompensationDelayBasic()
    {
        CompensationDelay delay;
        delay.prepare(1024, 2);
        delay.setDelaySamples(10);

        // Create a buffer with a known impulse
        juce::AudioBuffer<float> buffer(2, 64);
        buffer.clear();
        buffer.setSample(0, 0, 1.0f); // Impulse at sample 0
        buffer.setSample(1, 0, 1.0f);

        delay.processSamples(buffer);

        // The impulse should now be at position 10 (delayed by 10 samples)
        // Sample 0 should be 0 (the old buffer was empty)
        if (std::abs(buffer.getSample(0, 0)) > 0.001f)
            return false;

        // We need to process more blocks to see the delayed output
        // Since we just wrote the impulse, it should come out 10 samples later
        // But the first block already processed... let's check the delayed position
        // After processing 64 samples with the impulse at input[0],
        // output[0..9] should be 0 and output[10] should be 1.0
        if (std::abs(buffer.getSample(0, 10) - 1.0f) > 0.001f)
            return false;

        return true;
    }

    static bool testCompensationDelayZero()
    {
        CompensationDelay delay;
        delay.prepare(1024, 2);
        delay.setDelaySamples(0); // No delay

        juce::AudioBuffer<float> buffer(2, 32);
        buffer.clear();
        buffer.setSample(0, 5, 0.75f);

        delay.processSamples(buffer);

        // With zero delay, signal should pass through unchanged
        if (std::abs(buffer.getSample(0, 5) - 0.75f) > 0.001f)
            return false;

        return true;
    }

    static bool testManagerRecalculate()
    {
        DelayCompensationManager manager;
        manager.prepare(4096, 3);

        // Track 0: 100 samples latency, Track 1: 300 samples, Track 2: 200 samples
        manager.setTrackLatency(0, 100);
        manager.setTrackLatency(1, 300);
        manager.setTrackLatency(2, 200);
        manager.recalculate();

        // Max latency is 300
        if (manager.getTotalCompensation() != 300)
            return false;

        // Track 0 compensation = 300 - 100 = 200
        if (manager.getTrackCompensation(0) != 200)
            return false;

        // Track 1 compensation = 300 - 300 = 0
        if (manager.getTrackCompensation(1) != 0)
            return false;

        // Track 2 compensation = 300 - 200 = 100
        if (manager.getTrackCompensation(2) != 100)
            return false;

        return true;
    }

    static bool testManagerEqualLatency()
    {
        DelayCompensationManager manager;
        manager.prepare(4096, 2);

        manager.setTrackLatency(0, 256);
        manager.setTrackLatency(1, 256);
        manager.recalculate();

        // Both equal -> compensation should be 0 for both
        if (manager.getTrackCompensation(0) != 0)
            return false;
        if (manager.getTrackCompensation(1) != 0)
            return false;

        return true;
    }

    static bool testManagerSingleHighLatency()
    {
        DelayCompensationManager manager;
        manager.prepare(4096, 3);

        manager.setTrackLatency(0, 0);
        manager.setTrackLatency(1, 512);
        manager.setTrackLatency(2, 0);
        manager.recalculate();

        // Track 1 has highest latency, others need compensation
        if (manager.getTotalCompensation() != 512)
            return false;
        if (manager.getTrackCompensation(0) != 512)
            return false;
        if (manager.getTrackCompensation(1) != 0)
            return false;
        if (manager.getTrackCompensation(2) != 512)
            return false;

        return true;
    }
};
