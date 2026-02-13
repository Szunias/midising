#include "TestRunner.h"
#include <juce_core/juce_core.h>

/**
 * Standalone test runner executable.
 * Runs all MidiSing unit tests outside of the main GUI application.
 */
int main()
{
    // Initialize JUCE for console mode
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::Logger::writeToLog("MidiSing Test Runner");
    juce::Logger::writeToLog("====================\n");

    bool allPassed = TestRunner::runAllTests();

    return allPassed ? 0 : 1;
}
