#include "MainComponent.h"
#include "Audio/AudioTrack.h"
#include "Tests/TestRunner.h"
#include "UI/CommandIDs.h"

//==============================================================================
MainComponent::MainComponent()
    : statusBar(deviceManager),
      spectrumDisplay(audioEngine.getSpectrumAnalyzer())
{
    // Set custom look and feel
    setLookAndFeel(&lookAndFeel);

    setSize(1200, 800);

    // Run unit tests in debug mode
#if JUCE_DEBUG
    juce::Logger::writeToLog("Running unit tests...");
    TestRunner::runAllTests();
#endif
    
    // Setup transport bar
    transportBar.setTransport(&audioEngine.getTransport());
    setupTransportCallbacks();
    addAndMakeVisible(transportBar);
    
    // Setup status bar
    addAndMakeVisible(statusBar);
    
    // Setup spectrum display
    addAndMakeVisible(spectrumDisplay);

    // Setup commands
    setupCommands();
    setupCommands();
    setupCommands();
    addKeyListener(commandManager.getKeyMappings());

    // Setup timeline view
    timelineView.setTimeline(&audioEngine.getTimeline());
    timelineView.setTransport(&audioEngine.getTransport());
    timelineView.setMidiEngine(&audioEngine.getMidiEngine());
    addAndMakeVisible(timelineView);

    // Create some demo tracks so the timeline isn't empty
    createDemoTracks();

    // Request audio permissions if needed (mobile)
    if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
        && !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
            [&](bool granted) { setAudioChannels(granted ? 2 : 0, 2); });
    }
    else
    {
        setAudioChannels(2, 2);
    }

    // Register global key listener
    getLookAndFeel().setUsingNativeAlertWindows(true);
    addKeyListener(this);
}

MainComponent::~MainComponent()
{
    removeKeyListener(this);
    setLookAndFeel(nullptr);
    shutdownAudio();
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    return commandManager.getKeyMappings()->keyPressed(key, originatingComponent);
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    audioEngine.prepareToPlay(samplesPerBlockExpected, sampleRate);
    timelineView.setSampleRate(sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    audioEngine.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
    audioEngine.releaseResources();
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // Transport bar at top
    transportBar.setBounds(bounds.removeFromTop(50));
    
    // Status bar at bottom
    statusBar.setBounds(bounds.removeFromBottom(24));
    
    // Spectrum Display (small strip above status bar)
    spectrumDisplay.setBounds(bounds.removeFromBottom(60));

    // Timeline view fills the rest
    timelineView.setBounds(bounds);
}

void MainComponent::setupTransportCallbacks()
{
    transportBar.onPlay = [this]()
    {
        audioEngine.getTransport().togglePlayPause();
    };

    transportBar.onStop = [this]()
    {
        audioEngine.getTransport().stop();
    };

    transportBar.onRecord = [this]()
    {
        if (audioEngine.isRecording())
            audioEngine.stopRecording();
        else
            audioEngine.startRecording();
    };
    
    transportBar.onSave = [this]() { saveProject(); };
    transportBar.onOpen = [this]() { openProject(); };

    transportBar.onBpmChange = [this](double bpm)
    {
        audioEngine.getTimeline().setBpm(bpm);
    };

    transportBar.onMetronomeToggle = [this](bool enabled)
    {
        audioEngine.getMetronome().setEnabled(enabled);
    };

    transportBar.onMetronomeVolumeChange = [this](double volume)
    {
        audioEngine.getMetronome().setVolume(volume);
    };
}

void MainComponent::createDemoTracks()
{
    auto& timeline = audioEngine.getTimeline();

    // Create an Audio track
    auto* audioTrack = new AudioTrack("Audio 1");
    audioTrack->setColour(juce::Colour(0xff3498db)); // Blue
    audioTrack->setArmed(true); // Armed for recording by default
    timeline.addTrack(audioTrack);

    // Create a second Audio track
    auto* audioTrack2 = new AudioTrack("Audio 2");
    audioTrack2->setColour(juce::Colour(0xff2ecc71)); // Green
    timeline.addTrack(audioTrack2);

    // Refresh timeline view - need resized() to create TrackHeaders
    timelineView.resized();
    timelineView.repaint();
    timelineView.resized();
    timelineView.repaint();
}

#include "Serialization/ProjectSerializer.h"

void MainComponent::saveProject()
{
    // Stop playback
    audioEngine.getTransport().stop();
    
    fileChooser = std::make_unique<juce::FileChooser>("Save Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.midising");

    auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File())
        {
            if (!file.hasFileExtension("midising"))
                file = file.withFileExtension("midising");
            
            ProjectSerializer::saveProject(audioEngine.getTimeline(), file);
        }
    });
}

void MainComponent::openProject()
{
    // Stop playback
    audioEngine.getTransport().stop();

    fileChooser = std::make_unique<juce::FileChooser>("Open Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.midising");

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File())
        {
            // Clear history 
            undoManager.clearUndoHistory(); 
            
            ProjectSerializer::loadProject(audioEngine.getTimeline(), 
                                          audioEngine.getTransport(), 
                                          &audioEngine.getMidiEngine(), 
                                          file);
                                      
            // Update views
            timelineView.resized();
            timelineView.repaint();
            transportBar.repaint();
        }
    });
}

void MainComponent::setupCommands()
{
    commandManager.registerAllCommandsForTarget(this);
}

void MainComponent::getAllCommands(juce::Array<juce::CommandID>& commands)
{
    commands.add(CommandIDs::playStop);
    commands.add(CommandIDs::record);
    commands.add(CommandIDs::save);
    commands.add(CommandIDs::open);
    commands.add(CommandIDs::undo);
    commands.add(CommandIDs::redo);
}

void MainComponent::getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    switch (commandID)
    {
    case CommandIDs::playStop:
        result.setInfo("Play/Stop", "Starts or stops playback", "Transport", 0);
        result.addDefaultKeypress(juce::KeyPress::spaceKey, 0);
        break;
    case CommandIDs::record:
        result.setInfo("Record", "Toggles recording", "Transport", 0);
        result.addDefaultKeypress('r', 0);
        break;
    case CommandIDs::save:
        result.setInfo("Save Project", "Saves the project", "Project", 0);
        result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier);
        break;
    case CommandIDs::open:
        result.setInfo("Open Project", "Opens a project", "Project", 0);
        result.addDefaultKeypress('o', juce::ModifierKeys::commandModifier);
        break;
    case CommandIDs::undo:
        result.setInfo("Undo", "Undo the last operation", "Edit", 0);
        result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier);
        result.setActive(undoManager.canUndo());
        break;
    case CommandIDs::redo:
        result.setInfo("Redo", "Redo the last operation", "Edit", 0);
        result.setActive(undoManager.canRedo());
#if JUCE_MAC
        result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
#else
        result.addDefaultKeypress('y', juce::ModifierKeys::commandModifier);
#endif
        break;
    default:
        break;
    }
}

bool MainComponent::perform(const InvocationInfo& info)
{
    switch (info.commandID)
    {
    case CommandIDs::playStop:
        audioEngine.getTransport().togglePlayPause();
        return true;
    case CommandIDs::record:
        if (audioEngine.isRecording())
            audioEngine.stopRecording();
        else
            audioEngine.startRecording();
        return true;
    case CommandIDs::save:
        saveProject();
        return true;
    case CommandIDs::open:
        openProject();
        return true;
    case CommandIDs::undo:
        undoManager.undo();
        return true;
    case CommandIDs::redo:
        undoManager.redo();
        return true;
    default:
        return false;
    }
}

