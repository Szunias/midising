#include "MainComponent.h"

//==============================================================================
class MidiSingApplication : public juce::JUCEApplication
{
public:
    MidiSingApplication() {}

    const juce::String getApplicationName() override       { return "MidiSing"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);
    }

    //==============================================================================
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);

            #if JUCE_IOS || JUCE_ANDROID
                setFullScreen(true);
            #else
                setResizable(true, true);
                centreWithSize(1200, 800);
            #endif

            setVisible(true);
        }

        void closeButtonPressed() override
        {
            auto* mainComponent = dynamic_cast<MainComponent*>(getContentComponent());

            if (mainComponent != nullptr && mainComponent->hasUnsavedChanges())
            {
                auto options = juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::QuestionIcon)
                    .withTitle("Unsaved Changes")
                    .withMessage("Do you want to save changes before closing?")
                    .withButton("Save")
                    .withButton("Don't Save")
                    .withButton("Cancel");

                juce::AlertWindow::showAsync(options, [this, mainComponent](int result)
                {
                    if (result == 1) // Save
                    {
                        mainComponent->saveProject();
                        JUCEApplication::getInstance()->systemRequestedQuit();
                    }
                    else if (result == 2) // Don't Save
                    {
                        JUCEApplication::getInstance()->systemRequestedQuit();
                    }
                    // result == 3 or 0 is Cancel - do nothing
                });
            }
            else
            {
                JUCEApplication::getInstance()->systemRequestedQuit();
            }
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION(MidiSingApplication)
