#include "MainComponent.h"
#include "UI/SplashScreen.h"
#include "UI/LookAndFeel.h"

//==============================================================================
class MidiSingApplication : public juce::JUCEApplication,
                            public juce::Timer
{
public:
    MidiSingApplication() {}

    const juce::String getApplicationName() override       { return "MidiSing"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);

        // Set up look and feel
        lookAndFeel = std::make_unique<MidiSingLookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel(lookAndFeel.get());

        // Show splash screen
        splashWindow = std::make_unique<SplashScreenWindow>();

        // Set up callback for when splash is ready to close
        splashWindow->getSplashScreen()->setOnReadyCallback([this]()
        {
            closeSplashAndShowMain();
        });

        // Start initialization sequence
        initializationStage = 0;
        startTimer(50); // Start staged initialization
    }

    void shutdown() override
    {
        stopTimer();
        mainWindow = nullptr;
        splashWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        lookAndFeel = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);
    }

    void timerCallback() override
    {
        if (splashWindow == nullptr || splashWindow->getSplashScreen() == nullptr)
            return;

        auto* splash = splashWindow->getSplashScreen();

        // Staged initialization with progress updates
        switch (initializationStage)
        {
            case 0:
                splash->setProgress(0.1, "Loading audio engine...");
                initializationStage++;
                break;

            case 1:
                splash->setProgress(0.25, "Initializing MIDI system...");
                initializationStage++;
                break;

            case 2:
                splash->setProgress(0.4, "Loading plugins...");
                initializationStage++;
                break;

            case 3:
                splash->setProgress(0.55, "Setting up timeline...");
                initializationStage++;
                break;

            case 4:
                splash->setProgress(0.7, "Preparing user interface...");
                initializationStage++;
                break;

            case 5:
                // Create main window (this may take some time)
                createMainWindow();
                splash->setProgress(0.9, "Finalizing...");
                initializationStage++;
                break;

            case 6:
                splash->setProgress(1.0, "Ready!");
                stopTimer();
                // Splash screen will call closeSplashAndShowMain via callback
                break;

            default:
                break;
        }
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
    void createMainWindow()
    {
        if (mainWindow == nullptr)
        {
            mainWindow = std::make_unique<MainWindow>(getApplicationName());
            mainWindow->setVisible(false); // Keep hidden until splash closes
        }
    }

    void closeSplashAndShowMain()
    {
        // Hide and destroy splash
        if (splashWindow != nullptr)
        {
            splashWindow->setVisible(false);
            splashWindow = nullptr;
        }

        // Show main window
        if (mainWindow != nullptr)
        {
            mainWindow->setVisible(true);
            mainWindow->toFront(true);
        }
    }

    std::unique_ptr<MidiSingLookAndFeel> lookAndFeel;
    std::unique_ptr<SplashScreenWindow> splashWindow;
    std::unique_ptr<MainWindow> mainWindow;
    int initializationStage = 0;
};

//==============================================================================
START_JUCE_APPLICATION(MidiSingApplication)
