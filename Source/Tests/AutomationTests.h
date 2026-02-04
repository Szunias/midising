#pragma once

#include "../Timeline/AutomationLane.h"
#include <juce_core/juce_core.h>
#include <cmath>

/**
 * Unit tests for automation interpolation.
 *
 * Test Coverage:
 * 1. AutomationLane creation and configuration
 * 2. Point management (add, remove, move, clear)
 * 3. Linear interpolation between points
 * 4. Exponential curve interpolation
 * 5. Logarithmic curve interpolation
 * 6. Step interpolation (instant changes)
 * 7. S-Curve (smoothstep) interpolation
 * 8. Edge cases (before/after all points)
 * 9. Block value retrieval for audio processing
 * 10. Bypass functionality
 * 11. Point finding utilities
 */
class AutomationTests
{
public:
    static bool runAllTests()
    {
        bool allPassed = true;

        allPassed &= testAutomationLaneCreation();
        allPassed &= testPointManagement();
        allPassed &= testLinearInterpolation();
        allPassed &= testExponentialInterpolation();
        allPassed &= testLogarithmicInterpolation();
        allPassed &= testStepInterpolation();
        allPassed &= testSCurveInterpolation();
        allPassed &= testEdgeCases();
        allPassed &= testBlockValueRetrieval();
        allPassed &= testBypassFunctionality();
        allPassed &= testPointFinding();
        allPassed &= testCurveUtilityFunctions();

        return allPassed;
    }

private:
    //==========================================================================
    // Helper: Check float equality with tolerance
    //==========================================================================
    static bool floatEquals(float a, float b, float tolerance = 0.001f)
    {
        return std::abs(a - b) < tolerance;
    }

    //==========================================================================
    // Test 1: Automation Lane Creation and Configuration
    //==========================================================================
    static bool testAutomationLaneCreation()
    {
        bool passed = true;

        // Test volume automation lane (default)
        AutomationLane volumeLane(AutomationParameterType::Volume, "Volume");
        passed &= (volumeLane.getParameterType() == AutomationParameterType::Volume);
        passed &= (volumeLane.getParameterName() == "Volume");
        passed &= floatEquals(volumeLane.getMinValue(), 0.0f);
        passed &= floatEquals(volumeLane.getMaxValue(), 2.0f);
        passed &= floatEquals(volumeLane.getDefaultValue(), 1.0f);

        // Test pan automation lane
        AutomationLane panLane(AutomationParameterType::Pan, "Pan");
        passed &= (panLane.getParameterType() == AutomationParameterType::Pan);
        passed &= floatEquals(panLane.getMinValue(), -1.0f);
        passed &= floatEquals(panLane.getMaxValue(), 1.0f);
        passed &= floatEquals(panLane.getDefaultValue(), 0.0f);

        // Test mute automation lane
        AutomationLane muteLane(AutomationParameterType::Mute, "Mute");
        passed &= floatEquals(muteLane.getMinValue(), 0.0f);
        passed &= floatEquals(muteLane.getMaxValue(), 1.0f);
        passed &= floatEquals(muteLane.getDefaultValue(), 0.0f);

        // Test custom value range
        AutomationLane customLane(AutomationParameterType::Custom, "Custom");
        customLane.setValueRange(-10.0f, 10.0f);
        customLane.setDefaultValue(5.0f);
        passed &= floatEquals(customLane.getMinValue(), -10.0f);
        passed &= floatEquals(customLane.getMaxValue(), 10.0f);
        passed &= floatEquals(customLane.getDefaultValue(), 5.0f);

        // Test visibility and height
        volumeLane.setVisible(false);
        passed &= (volumeLane.isVisible() == false);
        volumeLane.setVisible(true);
        passed &= (volumeLane.isVisible() == true);

        volumeLane.setHeight(100);
        passed &= (volumeLane.getHeight() == 100);

        // Test minimum height constraint
        volumeLane.setHeight(10);
        passed &= (volumeLane.getHeight() >= 30);

        juce::Logger::writeToLog(passed ? "PASS: testAutomationLaneCreation" : "FAIL: testAutomationLaneCreation");
        return passed;
    }

    //==========================================================================
    // Test 2: Point Management
    //==========================================================================
    static bool testPointManagement()
    {
        bool passed = true;

        AutomationLane lane(AutomationParameterType::Volume, "Volume");

        // Verify initially empty
        passed &= (lane.getNumPoints() == 0);
        passed &= (lane.hasAutomation() == false);

        // Add points
        lane.addPoint(0, 0.0f, AutomationCurveType::Linear);
        lane.addPoint(1000, 1.0f, AutomationCurveType::Linear);
        lane.addPoint(500, 0.5f, AutomationCurveType::Linear);  // Add out of order

        passed &= (lane.getNumPoints() == 3);
        passed &= (lane.hasAutomation() == true);

        // Verify points are sorted by position
        passed &= (lane.getPoint(0).position == 0);
        passed &= (lane.getPoint(1).position == 500);
        passed &= (lane.getPoint(2).position == 1000);

        // Verify values
        passed &= floatEquals(lane.getPoint(0).value, 0.0f);
        passed &= floatEquals(lane.getPoint(1).value, 0.5f);
        passed &= floatEquals(lane.getPoint(2).value, 1.0f);

        // Update existing point (same position)
        lane.addPoint(500, 0.7f, AutomationCurveType::Exponential);
        passed &= (lane.getNumPoints() == 3);  // Should not add new point
        passed &= floatEquals(lane.getPoint(1).value, 0.7f);  // Should update value
        passed &= (lane.getPoint(1).curve == AutomationCurveType::Exponential);

        // Move point
        lane.movePoint(1, 600, 0.6f);
        passed &= (lane.getPoint(1).position == 600);
        passed &= floatEquals(lane.getPoint(1).value, 0.6f);

        // Set curve type
        lane.setPointCurve(1, AutomationCurveType::SCurve);
        passed &= (lane.getPoint(1).curve == AutomationCurveType::SCurve);

        // Remove point
        lane.removePoint(1);
        passed &= (lane.getNumPoints() == 2);

        // Remove points in range
        lane.addPoint(200, 0.3f);
        lane.addPoint(400, 0.4f);
        lane.addPoint(600, 0.5f);
        passed &= (lane.getNumPoints() == 5);

        lane.removePointsInRange(150, 450);
        passed &= (lane.getNumPoints() == 3);

        // Clear all
        lane.clearAllPoints();
        passed &= (lane.getNumPoints() == 0);
        passed &= (lane.hasAutomation() == false);

        juce::Logger::writeToLog(passed ? "PASS: testPointManagement" : "FAIL: testPointManagement");
        return passed;
    }

    //==========================================================================
    // Test 3: Linear Interpolation
    //==========================================================================
    static bool testLinearInterpolation()
    {
        bool passed = true;

        AutomationLane lane(AutomationParameterType::Volume, "Volume");
        lane.addPoint(0, 0.0f, AutomationCurveType::Linear);
        lane.addPoint(1000, 1.0f, AutomationCurveType::Linear);

        // Test start point
        passed &= floatEquals(lane.getValueAtPosition(0), 0.0f);

        // Test end point
        passed &= floatEquals(lane.getValueAtPosition(1000), 1.0f);

        // Test midpoint (linear should be exactly 0.5)
        passed &= floatEquals(lane.getValueAtPosition(500), 0.5f);

        // Test 25%
        passed &= floatEquals(lane.getValueAtPosition(250), 0.25f);

        // Test 75%
        passed &= floatEquals(lane.getValueAtPosition(750), 0.75f);

        // Test with decreasing values
        lane.clearAllPoints();
        lane.addPoint(0, 1.0f, AutomationCurveType::Linear);
        lane.addPoint(1000, 0.0f, AutomationCurveType::Linear);

        passed &= floatEquals(lane.getValueAtPosition(500), 0.5f);
        passed &= floatEquals(lane.getValueAtPosition(250), 0.75f);

        juce::Logger::writeToLog(passed ? "PASS: testLinearInterpolation" : "FAIL: testLinearInterpolation");
        return passed;
    }

    //==========================================================================
    // Test 4: Exponential Interpolation
    //==========================================================================
    static bool testExponentialInterpolation()
    {
        bool passed = true;

        AutomationLane lane(AutomationParameterType::Volume, "Volume");
        lane.addPoint(0, 0.0f, AutomationCurveType::Exponential);
        lane.addPoint(1000, 1.0f, AutomationCurveType::Linear);

        // Exponential: t^2, so at t=0.5, result should be 0.25
        float midValue = lane.getValueAtPosition(500);
        passed &= floatEquals(midValue, 0.25f, 0.01f);

        // At t=0.25, exponential gives 0.0625
        float quarterValue = lane.getValueAtPosition(250);
        passed &= floatEquals(quarterValue, 0.0625f, 0.01f);

        // At t=0.75, exponential gives 0.5625
        float threeQuarterValue = lane.getValueAtPosition(750);
        passed &= floatEquals(threeQuarterValue, 0.5625f, 0.01f);

        // Verify exponential is always below linear (for increasing values)
        for (int i = 100; i < 900; i += 100)
        {
            float expValue = lane.getValueAtPosition(i);
            float linearExpected = static_cast<float>(i) / 1000.0f;
            passed &= (expValue <= linearExpected);
        }

        juce::Logger::writeToLog(passed ? "PASS: testExponentialInterpolation" : "FAIL: testExponentialInterpolation");
        return passed;
    }

    //==========================================================================
    // Test 5: Logarithmic Interpolation
    //==========================================================================
    static bool testLogarithmicInterpolation()
    {
        bool passed = true;

        AutomationLane lane(AutomationParameterType::Volume, "Volume");
        lane.addPoint(0, 0.0f, AutomationCurveType::Logarithmic);
        lane.addPoint(1000, 1.0f, AutomationCurveType::Linear);

        // Logarithmic: sqrt(t), so at t=0.5, result should be ~0.707
        float midValue = lane.getValueAtPosition(500);
        float expectedMid = std::sqrt(0.5f);
        passed &= floatEquals(midValue, expectedMid, 0.01f);

        // At t=0.25, logarithmic gives 0.5
        float quarterValue = lane.getValueAtPosition(250);
        passed &= floatEquals(quarterValue, 0.5f, 0.01f);

        // Verify logarithmic is always above linear (for increasing values)
        for (int i = 100; i < 900; i += 100)
        {
            float logValue = lane.getValueAtPosition(i);
            float linearExpected = static_cast<float>(i) / 1000.0f;
            passed &= (logValue >= linearExpected);
        }

        juce::Logger::writeToLog(passed ? "PASS: testLogarithmicInterpolation" : "FAIL: testLogarithmicInterpolation");
        return passed;
    }

    //==========================================================================
    // Test 6: Step Interpolation
    //==========================================================================
    static bool testStepInterpolation()
    {
        bool passed = true;

        AutomationLane lane(AutomationParameterType::Mute, "Mute");
        lane.addPoint(0, 0.0f, AutomationCurveType::Step);
        lane.addPoint(1000, 1.0f, AutomationCurveType::Step);

        // Step: should hold start value until reaching end
        passed &= floatEquals(lane.getValueAtPosition(0), 0.0f);
        passed &= floatEquals(lane.getValueAtPosition(100), 0.0f);
        passed &= floatEquals(lane.getValueAtPosition(500), 0.0f);
        passed &= floatEquals(lane.getValueAtPosition(999), 0.0f);

        // At exact end position, should be end value
        passed &= floatEquals(lane.getValueAtPosition(1000), 1.0f);

        // Test multiple step points
        lane.clearAllPoints();
        lane.addPoint(0, 0.0f, AutomationCurveType::Step);
        lane.addPoint(500, 1.0f, AutomationCurveType::Step);
        lane.addPoint(1000, 0.5f, AutomationCurveType::Step);

        passed &= floatEquals(lane.getValueAtPosition(250), 0.0f);  // Before first step
        passed &= floatEquals(lane.getValueAtPosition(500), 1.0f);  // At first step
        passed &= floatEquals(lane.getValueAtPosition(750), 1.0f);  // After first step
        passed &= floatEquals(lane.getValueAtPosition(1000), 0.5f); // At second step

        juce::Logger::writeToLog(passed ? "PASS: testStepInterpolation" : "FAIL: testStepInterpolation");
        return passed;
    }

    //==========================================================================
    // Test 7: S-Curve (Smoothstep) Interpolation
    //==========================================================================
    static bool testSCurveInterpolation()
    {
        bool passed = true;

        AutomationLane lane(AutomationParameterType::Volume, "Volume");
        lane.addPoint(0, 0.0f, AutomationCurveType::SCurve);
        lane.addPoint(1000, 1.0f, AutomationCurveType::Linear);

        // S-Curve: 3t^2 - 2t^3
        // At t=0.5: 3*(0.25) - 2*(0.125) = 0.75 - 0.25 = 0.5
        float midValue = lane.getValueAtPosition(500);
        passed &= floatEquals(midValue, 0.5f, 0.01f);

        // At t=0.25: 3*(0.0625) - 2*(0.015625) = 0.1875 - 0.03125 = 0.15625
        float quarterValue = lane.getValueAtPosition(250);
        float expectedQuarter = 0.25f * 0.25f * (3.0f - 2.0f * 0.25f);
        passed &= floatEquals(quarterValue, expectedQuarter, 0.01f);

        // Verify S-curve characteristics:
        // - Starts slow (below linear at start)
        // - Equals linear at midpoint
        // - Ends slow (below linear at end)
        float val10 = lane.getValueAtPosition(100);
        float val90 = lane.getValueAtPosition(900);

        // At 10%, S-curve should be below linear
        passed &= (val10 < 0.1f);

        // At 90%, S-curve should be above linear
        passed &= (val90 > 0.9f);

        juce::Logger::writeToLog(passed ? "PASS: testSCurveInterpolation" : "FAIL: testSCurveInterpolation");
        return passed;
    }

    //==========================================================================
    // Test 8: Edge Cases
    //==========================================================================
    static bool testEdgeCases()
    {
        bool passed = true;

        // Test empty lane returns default value
        AutomationLane emptyLane(AutomationParameterType::Volume, "Volume");
        passed &= floatEquals(emptyLane.getValueAtPosition(500), 1.0f);  // Default volume

        // Test single point lane
        AutomationLane singlePoint(AutomationParameterType::Volume, "Volume");
        singlePoint.addPoint(500, 0.7f);
        passed &= floatEquals(singlePoint.getValueAtPosition(0), 0.7f);     // Before point
        passed &= floatEquals(singlePoint.getValueAtPosition(500), 0.7f);   // At point
        passed &= floatEquals(singlePoint.getValueAtPosition(1000), 0.7f);  // After point

        // Test position before first point
        AutomationLane lane(AutomationParameterType::Volume, "Volume");
        lane.addPoint(500, 0.3f);
        lane.addPoint(1000, 0.8f);
        passed &= floatEquals(lane.getValueAtPosition(0), 0.3f);    // Should be first point's value
        passed &= floatEquals(lane.getValueAtPosition(250), 0.3f);  // Still first point's value

        // Test position after last point
        passed &= floatEquals(lane.getValueAtPosition(1500), 0.8f);  // Should be last point's value
        passed &= floatEquals(lane.getValueAtPosition(2000), 0.8f);  // Still last point's value

        // Test scaled value
        AutomationLane scaledLane(AutomationParameterType::Volume, "Volume");
        scaledLane.addPoint(0, 0.5f);  // 0.5 normalized
        // With min=0, max=2, 0.5 normalized should give 1.0 scaled
        float scaledVal = scaledLane.getScaledValueAtPosition(0);
        passed &= floatEquals(scaledVal, 1.0f, 0.01f);

        juce::Logger::writeToLog(passed ? "PASS: testEdgeCases" : "FAIL: testEdgeCases");
        return passed;
    }

    //==========================================================================
    // Test 9: Block Value Retrieval
    //==========================================================================
    static bool testBlockValueRetrieval()
    {
        bool passed = true;

        const int blockSize = 512;
        float buffer[blockSize];

        // Test empty lane (should fill with default)
        AutomationLane emptyLane(AutomationParameterType::Volume, "Volume");
        emptyLane.getValuesForBlock(0, blockSize, buffer);

        for (int i = 0; i < blockSize; ++i)
        {
            if (!floatEquals(buffer[i], 1.0f))  // Default volume
            {
                passed = false;
                break;
            }
        }

        // Test block entirely before first point
        AutomationLane lane(AutomationParameterType::Volume, "Volume");
        lane.addPoint(1000, 0.5f);
        lane.addPoint(2000, 0.8f);
        lane.getValuesForBlock(0, blockSize, buffer);

        for (int i = 0; i < blockSize; ++i)
        {
            if (!floatEquals(buffer[i], 0.5f))  // First point's value
            {
                passed = false;
                break;
            }
        }

        // Test block entirely after last point
        lane.getValuesForBlock(3000, blockSize, buffer);

        for (int i = 0; i < blockSize; ++i)
        {
            if (!floatEquals(buffer[i], 0.8f))  // Last point's value
            {
                passed = false;
                break;
            }
        }

        // Test block spanning automation points
        lane.clearAllPoints();
        lane.addPoint(0, 0.0f, AutomationCurveType::Linear);
        lane.addPoint(512, 1.0f, AutomationCurveType::Linear);
        lane.getValuesForBlock(0, blockSize, buffer);

        // Verify linear ramp
        for (int i = 0; i < blockSize; ++i)
        {
            float expected = static_cast<float>(i) / 512.0f;
            if (!floatEquals(buffer[i], expected, 0.01f))
            {
                passed = false;
                break;
            }
        }

        // Test null buffer handling (should not crash)
        lane.getValuesForBlock(0, blockSize, nullptr);

        // Test zero samples (should not crash)
        lane.getValuesForBlock(0, 0, buffer);

        juce::Logger::writeToLog(passed ? "PASS: testBlockValueRetrieval" : "FAIL: testBlockValueRetrieval");
        return passed;
    }

    //==========================================================================
    // Test 10: Bypass Functionality
    //==========================================================================
    static bool testBypassFunctionality()
    {
        bool passed = true;

        AutomationLane lane(AutomationParameterType::Volume, "Volume");
        lane.addPoint(0, 0.0f, AutomationCurveType::Linear);
        lane.addPoint(1000, 1.0f, AutomationCurveType::Linear);

        // Verify normal operation
        passed &= (lane.isBypassed() == false);
        passed &= floatEquals(lane.getValueAtPosition(500), 0.5f);

        // Enable bypass
        lane.setBypassed(true);
        passed &= (lane.isBypassed() == true);

        // Should return default value when bypassed
        passed &= floatEquals(lane.getValueAtPosition(500), 1.0f);  // Default volume
        passed &= floatEquals(lane.getValueAtPosition(0), 1.0f);
        passed &= floatEquals(lane.getValueAtPosition(1000), 1.0f);

        // Test block retrieval when bypassed
        const int blockSize = 256;
        float buffer[blockSize];
        lane.getValuesForBlock(0, blockSize, buffer);

        for (int i = 0; i < blockSize; ++i)
        {
            if (!floatEquals(buffer[i], 1.0f))  // Should all be default
            {
                passed = false;
                break;
            }
        }

        // Disable bypass, should return to normal
        lane.setBypassed(false);
        passed &= (lane.isBypassed() == false);
        passed &= floatEquals(lane.getValueAtPosition(500), 0.5f);

        juce::Logger::writeToLog(passed ? "PASS: testBypassFunctionality" : "FAIL: testBypassFunctionality");
        return passed;
    }

    //==========================================================================
    // Test 11: Point Finding
    //==========================================================================
    static bool testPointFinding()
    {
        bool passed = true;

        AutomationLane lane(AutomationParameterType::Volume, "Volume");
        lane.addPoint(100, 0.1f);
        lane.addPoint(500, 0.5f);
        lane.addPoint(900, 0.9f);

        // Find point at exact position
        passed &= (lane.findPointAtPosition(100) == 0);
        passed &= (lane.findPointAtPosition(500) == 1);
        passed &= (lane.findPointAtPosition(900) == 2);

        // Find point with tolerance
        passed &= (lane.findPointAtPosition(105, 10) == 0);  // Within tolerance
        passed &= (lane.findPointAtPosition(115, 10) == -1); // Outside tolerance

        // Find point at non-existent position
        passed &= (lane.findPointAtPosition(300) == -1);

        // Find nearest point before
        passed &= (lane.findNearestPointBefore(0) == -1);    // No point before 0
        passed &= (lane.findNearestPointBefore(100) == 0);   // At first point
        passed &= (lane.findNearestPointBefore(300) == 0);   // Between first and second
        passed &= (lane.findNearestPointBefore(500) == 1);   // At second point
        passed &= (lane.findNearestPointBefore(700) == 1);   // Between second and third
        passed &= (lane.findNearestPointBefore(1000) == 2);  // After last point

        // Find nearest point after
        passed &= (lane.findNearestPointAfter(0) == 0);      // Before first point
        passed &= (lane.findNearestPointAfter(100) == 1);    // After first point
        passed &= (lane.findNearestPointAfter(300) == 1);    // Between first and second
        passed &= (lane.findNearestPointAfter(500) == 2);    // After second point
        passed &= (lane.findNearestPointAfter(900) == -1);   // No point after last
        passed &= (lane.findNearestPointAfter(1000) == -1);  // After last point

        juce::Logger::writeToLog(passed ? "PASS: testPointFinding" : "FAIL: testPointFinding");
        return passed;
    }

    //==========================================================================
    // Test 12: Curve Utility Functions
    //==========================================================================
    static bool testCurveUtilityFunctions()
    {
        bool passed = true;

        // Test direct interpolate function
        using namespace AutomationCurveUtils;

        // Linear interpolation
        passed &= floatEquals(interpolate(0.0f, 1.0f, 0.5f, AutomationCurveType::Linear), 0.5f);
        passed &= floatEquals(interpolate(0.0f, 1.0f, 0.0f, AutomationCurveType::Linear), 0.0f);
        passed &= floatEquals(interpolate(0.0f, 1.0f, 1.0f, AutomationCurveType::Linear), 1.0f);

        // Exponential: t^2
        passed &= floatEquals(interpolate(0.0f, 1.0f, 0.5f, AutomationCurveType::Exponential), 0.25f);

        // Logarithmic: sqrt(t)
        passed &= floatEquals(interpolate(0.0f, 1.0f, 0.25f, AutomationCurveType::Logarithmic), 0.5f);

        // Step
        passed &= floatEquals(interpolate(0.0f, 1.0f, 0.5f, AutomationCurveType::Step), 0.0f);
        passed &= floatEquals(interpolate(0.0f, 1.0f, 1.0f, AutomationCurveType::Step), 1.0f);

        // S-Curve at midpoint should be 0.5
        passed &= floatEquals(interpolate(0.0f, 1.0f, 0.5f, AutomationCurveType::SCurve), 0.5f);

        // Test clamping of t
        passed &= floatEquals(interpolate(0.0f, 1.0f, -0.5f, AutomationCurveType::Linear), 0.0f);
        passed &= floatEquals(interpolate(0.0f, 1.0f, 1.5f, AutomationCurveType::Linear), 1.0f);

        // Test dB conversion functions
        // Normalized 0.5 with range -60 to +6 should give -27 dB
        float db = normalizedToDecibels(0.5f, -60.0f, 6.0f);
        passed &= floatEquals(db, -27.0f);

        // At normalized 0, should return min dB
        passed &= floatEquals(normalizedToDecibels(0.0f, -60.0f, 6.0f), -60.0f);

        // At normalized 1, should return max dB
        passed &= floatEquals(normalizedToDecibels(1.0f, -60.0f, 6.0f), 6.0f);

        // Test dB to normalized
        float norm = decibelsToNormalized(-27.0f, -60.0f, 6.0f);
        passed &= floatEquals(norm, 0.5f);

        // Below min dB should return 0
        passed &= floatEquals(decibelsToNormalized(-70.0f, -60.0f, 6.0f), 0.0f);

        // Above max dB should return 1
        passed &= floatEquals(decibelsToNormalized(10.0f, -60.0f, 6.0f), 1.0f);

        juce::Logger::writeToLog(passed ? "PASS: testCurveUtilityFunctions" : "FAIL: testCurveUtilityFunctions");
        return passed;
    }
};
