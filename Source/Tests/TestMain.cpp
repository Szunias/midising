#include "TestRunner.h"
#include <juce_core/juce_core.h>
#include <iostream>

/** Logger that writes to stdout for console test runner. */
class StdoutLogger : public juce::Logger
{
    void logMessage(const juce::String& message) override
    {
        std::cout << message.toStdString() << std::endl;
    }
};

/**
 * Standalone test runner executable.
 * Runs all MidiSing unit tests outside of the main GUI application.
 */
int main()
{
    // Initialize JUCE for console mode
    juce::ScopedJuceInitialiser_GUI juceInit;

    StdoutLogger stdoutLogger;
    juce::Logger::setCurrentLogger(&stdoutLogger);

    juce::Logger::writeToLog("MidiSing Test Runner");
    juce::Logger::writeToLog("====================\n");

    bool allPassed = TestRunner::runAllTests();

    juce::Logger::setCurrentLogger(nullptr);
    return allPassed ? 0 : 1;
}
