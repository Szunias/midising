#pragma once

#include "TimeConversionTest.h"
#include "TimelineTest.h"
#include "VST3WorkflowTest.h"
#include <juce_core/juce_core.h>

/**
 * Test runner that executes all unit tests.
 */
class TestRunner
{
public:
    static bool runAllTests()
    {
        juce::Logger::writeToLog("=== MidiSing Unit Tests ===");
        juce::Logger::writeToLog("");

        bool allPassed = true;
        int totalTests = 0;
        int passedTests = 0;

        // TimeConversion tests
        juce::Logger::writeToLog("--- TimeConversion Tests ---");
        if (TimeConversionTest::runAllTests())
        {
            passedTests += 5;
        }
        totalTests += 5;
        juce::Logger::writeToLog("");

        // Timeline tests
        juce::Logger::writeToLog("--- Timeline Tests ---");
        if (TimelineTest::runAllTests())
        {
            passedTests += 4;
        }
        totalTests += 4;
        juce::Logger::writeToLog("");

        // VST3 Workflow tests (end-to-end verification)
        juce::Logger::writeToLog("--- VST3 Workflow Tests ---");
        if (VST3WorkflowTest::runAllTests())
        {
            passedTests += 7;
        }
        totalTests += 7;
        juce::Logger::writeToLog("");

        // Summary
        juce::Logger::writeToLog("=== Test Summary ===");
        juce::Logger::writeToLog(juce::String("Passed: ") + juce::String(passedTests) +
                                  "/" + juce::String(totalTests));

        if (passedTests == totalTests)
        {
            juce::Logger::writeToLog("ALL TESTS PASSED!");
            allPassed = true;
        }
        else
        {
            juce::Logger::writeToLog("SOME TESTS FAILED!");
            allPassed = false;
        }

        return allPassed;
    }
};
