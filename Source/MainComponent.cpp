#include "MainComponent.h"
#include "Audio/AudioTrack.h"
#include "Audio/AudioImporter.h"
#include <cmath>
#include "MIDI/MidiTrack.h"
#include "MIDI/MidiImporter.h"
#include "Actions/TrackActions.h"
#include "Tests/TestRunner.h"
#include "UI/CommandIDs.h"
#include "UI/SettingsPanel.h"
#include "Export/AudioExporter.h"

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

    // Setup tool bar (below transport bar)
    addAndMakeVisible(toolBar);

    // Setup status bar
    statusBar.setAudioEngine(&audioEngine);
    addAndMakeVisible(statusBar);
    
    // Setup spectrum display
    addAndMakeVisible(spectrumDisplay);

    // Setup commands
    setupCommands();
    addKeyListener(commandManager.getKeyMappings());

    // Setup menu bar
    menuBar.setModel(this);
    addAndMakeVisible(menuBar);

    // Setup timeline view
    timelineView.setTimeline(&audioEngine.getTimeline());
    timelineView.setTransport(&audioEngine.getTransport());
    timelineView.setMidiEngine(&audioEngine.getMidiEngine());
    addAndMakeVisible(timelineView);

    // Setup collapsible panels (Browser, Mixer, PianoRoll)
    setupCollapsiblePanels();

    // Wire up timeline view callback for double-click on MIDI regions
    timelineView.onMidiRegionDoubleClicked = [this](MidiRegion* region)
    {
        showPianoRollWithRegion(region);
    };

    // Setup resizable splitters between panels
    setupSplitters();

    // Create some demo tracks so the timeline isn't empty
    createDemoTracks();

    // Setup audio device manager with crash protection
    setupAudioDeviceManager();

    // Register global key listener
    getLookAndFeel().setUsingNativeAlertWindows(true);
    addKeyListener(this);

    // Setup auto-save system and check for recovery files
    setupAutoSave();
}

MainComponent::~MainComponent()
{
    // Stop auto-save timer
    stopTimer();

    // Remove device change listener before shutdown
    deviceManager.removeChangeListener(this);

    removeKeyListener(this);
    setLookAndFeel(nullptr);
    shutdownAudio();

    // Clean up recovery file on normal exit if no unsaved changes
    if (!hasUnsavedChanges_)
    {
        deleteRecoveryFile();
    }
}

void MainComponent::updateWindowTitle()
{
    // Find the parent DocumentWindow
    auto* window = findParentComponentOfClass<juce::DocumentWindow>();
    if (window != nullptr)
    {
        juce::String title = "MidiSing";

        // Add filename if we have a project open
        if (currentProjectFile != juce::File())
        {
            title << " - " << currentProjectFile.getFileNameWithoutExtension();
        }

        // Add asterisk if there are unsaved changes
        if (hasUnsavedChanges_)
        {
            title << " *";
        }

        window->setName(title);
    }
}

void MainComponent::setHasUnsavedChanges(bool dirty)
{
    if (hasUnsavedChanges_ != dirty)
    {
        hasUnsavedChanges_ = dirty;
        updateWindowTitle();
    }
}

void MainComponent::setCurrentProjectFile(const juce::File& file)
{
    currentProjectFile = file;
    updateWindowTitle();
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    return commandManager.getKeyMappings()->keyPressed(key, originatingComponent);
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    // Configure audio device for stereo input
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputChannels.setRange(0, 2, true);  // Enable stereo input (channels 0 and 1)
    deviceManager.setAudioDeviceSetup(setup, true);

    audioEngine.prepareToPlay(samplesPerBlockExpected, sampleRate);
    timelineView.setSampleRate(sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Capture input buffer for recording BEFORE calling audioEngine.getNextAudioBlock()
    // This is necessary because the audio engine clears the buffer before processing,
    // which would lose the raw input signal needed for recording
    if (audioEngine.isRecording())
    {
        audioEngine.recordInputBlock(*bufferToFill.buffer);
    }

    // Calculate and update input levels for metering (before buffer is cleared)
    if (bufferToFill.buffer->getNumChannels() > 0)
    {
        float leftLevel = bufferToFill.buffer->getMagnitude(0, 0, bufferToFill.numSamples);
        float rightLevel = bufferToFill.buffer->getNumChannels() > 1
            ? bufferToFill.buffer->getMagnitude(1, 0, bufferToFill.numSamples)
            : leftLevel;

        // Update status bar on message thread
        juce::MessageManager::callAsync([this, leftLevel, rightLevel]()
        {
            statusBar.setInputLevel(leftLevel, rightLevel);
        });
    }

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

    // Menu bar at top
    menuBar.setBounds(bounds.removeFromTop(juce::LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));

    // Transport bar below menu bar
    transportBar.setBounds(bounds.removeFromTop(50));

    // Tool bar below transport bar
    toolBar.setBounds(bounds.removeFromTop(40));

    // Status bar at bottom
    statusBar.setBounds(bounds.removeFromBottom(24));

    // Spectrum Display (small strip above status bar)
    spectrumDisplay.setBounds(bounds.removeFromBottom(60));

    // Browser panel on the left (if visible) with splitter
    if (browserVisible && browserPanel != nullptr)
    {
        browserPanel->setBounds(bounds.removeFromLeft(browserPanelWidth));

        // Position browser splitter
        if (browserSplitter != nullptr)
        {
            browserSplitter->setVisible(true);
            browserSplitter->setBounds(bounds.removeFromLeft(browserSplitter->getThickness()));
        }
    }
    else if (browserSplitter != nullptr)
    {
        browserSplitter->setVisible(false);
    }

    // Bottom panel splitter and panels
    bool hasBottomPanel = (pianoRollVisible && pianoRollPanel != nullptr) ||
                          (mixerVisible && mixerPanel != nullptr);

    if (hasBottomPanel && bottomSplitter != nullptr)
    {
        // Piano roll panel at bottom (if visible) - takes priority over mixer
        if (pianoRollVisible && pianoRollPanel != nullptr)
        {
            // Remove space for panel and splitter from bottom
            auto panelBounds = bounds.removeFromBottom(pianoRollPanelHeight);
            auto splitterBounds = bounds.removeFromBottom(bottomSplitter->getThickness());

            pianoRollPanel->setBounds(panelBounds);
            bottomSplitter->setBounds(splitterBounds);
            bottomSplitter->setVisible(true);
        }
        // Mixer panel at bottom (if visible and piano roll is not)
        else if (mixerVisible && mixerPanel != nullptr)
        {
            // Remove space for panel and splitter from bottom
            auto panelBounds = bounds.removeFromBottom(mixerPanelHeight);
            auto splitterBounds = bounds.removeFromBottom(bottomSplitter->getThickness());

            mixerPanel->setBounds(panelBounds);
            bottomSplitter->setBounds(splitterBounds);
            bottomSplitter->setVisible(true);
        }
    }
    else if (bottomSplitter != nullptr)
    {
        bottomSplitter->setVisible(false);
    }

    // Timeline view fills the rest
    timelineView.setBounds(bounds);

    // Update splitter limits based on current layout
    updateSplitterPositions();
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
        setHasUnsavedChanges(true);
    };

    transportBar.onMetronomeToggle = [this](bool enabled)
    {
        audioEngine.getMetronome().setEnabled(enabled);
    };

    transportBar.onMetronomeVolumeChange = [this](double volume)
    {
        audioEngine.getMetronome().setVolume(volume);
    };
    
    // Initialize metronome UI state
    transportBar.setMetronomeEnabled(audioEngine.getMetronome().isEnabled());
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

void MainComponent::newProject()
{
    // Check for unsaved changes first
    if (hasUnsavedChanges_)
    {
        auto options = juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Unsaved Changes")
            .withMessage("Do you want to save changes before creating a new project?")
            .withButton("Save")
            .withButton("Don't Save")
            .withButton("Cancel");

        juce::AlertWindow::showAsync(options, [this](int result)
        {
            if (result == 0)  // Cancel
                return;

            if (result == 1)  // Save
            {
                // Save the current project first
                // Note: We need to defer the new project creation until after save completes
                // For simplicity, we'll just save and let the user create new project again
                saveProject();
                return;
            }

            // result == 2: Don't Save - proceed with new project
            createNewProject();
        });
    }
    else
    {
        // No unsaved changes, create new project directly
        createNewProject();
    }
}

void MainComponent::createNewProject()
{
    // Stop playback and recording
    audioEngine.getTransport().stop();
    if (audioEngine.isRecording())
        audioEngine.stopRecording();

    // Clear the timeline
    auto& timeline = audioEngine.getTimeline();
    timeline.clearTracks();

    // Reset timeline properties to defaults
    timeline.setBpm(120.0);
    timeline.setBeatsPerBar(4);

    // Clear undo history
    undoManager.clearUndoHistory();

    // Reset current project file
    setCurrentProjectFile(juce::File());

    // Clear unsaved changes flag
    setHasUnsavedChanges(false);

    // Create default empty tracks
    createDemoTracks();

    // Update views
    timelineView.resized();
    timelineView.repaint();
    transportBar.repaint();
}

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

            ProjectSerializer::saveProject(audioEngine.getTimeline(), audioEngine.getTransport(), file);

            // Update current file and clear dirty flag after successful save
            setCurrentProjectFile(file);
            setHasUnsavedChanges(false);

            // Add to recent files after successful save
            recentFilesManager.addFile(file);

            // Delete recovery file since we have a successful manual save
            deleteRecoveryFile();
        }
    });
}

void MainComponent::saveProjectAs()
{
    // Stop playback
    audioEngine.getTransport().stop();

    // Start with current file location if we have one, otherwise use Documents
    auto initialLocation = currentProjectFile != juce::File()
        ? currentProjectFile.getParentDirectory()
        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    fileChooser = std::make_unique<juce::FileChooser>("Save Project As",
        initialLocation,
        "*.midising");

    auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File())
        {
            if (!file.hasFileExtension("midising"))
                file = file.withFileExtension("midising");

            ProjectSerializer::saveProject(audioEngine.getTimeline(), audioEngine.getTransport(), file);

            // Update current file and clear dirty flag after successful save
            setCurrentProjectFile(file);
            setHasUnsavedChanges(false);

            // Add to recent files after successful save
            recentFilesManager.addFile(file);

            // Delete recovery file since we have a successful manual save
            deleteRecoveryFile();
        }
    });
}

void MainComponent::collectAllAndSave()
{
    // Stop playback
    audioEngine.getTransport().stop();

    // Start with current file location if we have one, otherwise use Documents
    auto initialLocation = currentProjectFile != juce::File()
        ? currentProjectFile.getParentDirectory()
        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    // Use folder browser to select project bundle location
    fileChooser = std::make_unique<juce::FileChooser>("Choose Project Folder",
        initialLocation,
        "",  // No file filter for folder selection
        true);  // Use native dialog

    auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto folder = fc.getResult();
        if (folder != juce::File())
        {
            // Ensure folder has a valid name
            if (folder.getFileName().isEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                       "Invalid Folder",
                                                       "Please choose a valid folder name for the project.");
                return;
            }

            // Create the project folder if it doesn't exist
            if (!folder.exists())
            {
                auto result = folder.createDirectory();
                if (result.failed())
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                           "Error",
                                                           "Could not create project folder: " + result.getErrorMessage());
                    return;
                }
            }

            // Collect all external audio files into the project bundle
            int filesCollected = ProjectSerializer::collectAllFiles(audioEngine.getTimeline(), folder);

            if (filesCollected < 0)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                       "Error",
                                                       "Failed to collect audio files into the project folder.");
                return;
            }

            // Save the project bundle
            bool success = ProjectSerializer::saveProjectBundle(audioEngine.getTimeline(),
                                                                audioEngine.getTransport(),
                                                                folder);

            if (success)
            {
                // Update current file to point to the project file within the bundle
                auto projectFile = folder.getChildFile(ProjectSerializer::PROJECT_FILE_NAME);
                setCurrentProjectFile(projectFile);
                setHasUnsavedChanges(false);

                // Add to recent files after successful save
                recentFilesManager.addFile(projectFile);

                // Delete recovery file since we have a successful manual save
                deleteRecoveryFile();

                // Show success message
                juce::String message = "Project saved successfully.";
                if (filesCollected > 0)
                {
                    message += "\n\n" + juce::String(filesCollected) + " external audio file"
                             + (filesCollected == 1 ? " was" : "s were") + " copied into the project folder.";
                }
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                                       "Collect All and Save",
                                                       message);
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                       "Error",
                                                       "Failed to save project bundle.");
            }
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

            // Update current file and clear dirty flag after successful load
            setCurrentProjectFile(file);
            setHasUnsavedChanges(false);

            // Add to recent files after successful load
            recentFilesManager.addFile(file);
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
    commands.add(CommandIDs::projectNew);
    commands.add(CommandIDs::save);
    commands.add(CommandIDs::saveAs);
    commands.add(CommandIDs::collectAllAndSave);
    commands.add(CommandIDs::open);
    commands.add(CommandIDs::exportAudio);
    commands.add(CommandIDs::undo);
    commands.add(CommandIDs::redo);
    commands.add(CommandIDs::audioSettings);
    commands.add(CommandIDs::addAudioTrack);
    commands.add(CommandIDs::addMidiTrack);
    commands.add(CommandIDs::toggleBrowser);
    commands.add(CommandIDs::toggleMixer);
    commands.add(CommandIDs::togglePianoRoll);

    // Tool mode commands
    commands.add(CommandIDs::toolSelect);
    commands.add(CommandIDs::toolDraw);
    commands.add(CommandIDs::toolSplit);
    commands.add(CommandIDs::toolErase);
    commands.add(CommandIDs::toolMute);

    // Edit commands
    commands.add(CommandIDs::selectAll);
    commands.add(CommandIDs::deleteSelection);
    commands.add(CommandIDs::splitAtPlayhead);
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
    case CommandIDs::projectNew:
        result.setInfo("New Project", "Creates a new project", "Project", 0);
        result.addDefaultKeypress('n', juce::ModifierKeys::commandModifier);
        break;
    case CommandIDs::save:
        result.setInfo("Save Project", "Saves the project", "Project", 0);
        result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier);
        break;
    case CommandIDs::saveAs:
        result.setInfo("Save Project As", "Saves the project with a new name", "Project", 0);
        result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
        break;
    case CommandIDs::collectAllAndSave:
        result.setInfo("Collect All and Save", "Copies all external audio files into the project folder and saves", "Project", 0);
        result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier);
        break;
    case CommandIDs::open:
        result.setInfo("Open Project", "Opens a project", "Project", 0);
        result.addDefaultKeypress('o', juce::ModifierKeys::commandModifier);
        break;
    case CommandIDs::exportAudio:
        result.setInfo("Export Audio...", "Exports the project to a WAV file", "Project", 0);
        result.addDefaultKeypress('e', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
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
    case CommandIDs::audioSettings:
        result.setInfo("Audio Settings...", "Opens the audio device settings dialog", "Settings", 0);
        result.addDefaultKeypress(',', juce::ModifierKeys::commandModifier);
        break;
    case CommandIDs::addAudioTrack:
        result.setInfo("Add Audio Track", "Adds a new audio track to the timeline", "Track", 0);
        result.addDefaultKeypress('t', juce::ModifierKeys::commandModifier);
        break;
    case CommandIDs::addMidiTrack:
        result.setInfo("Add MIDI Track", "Adds a new MIDI track to the timeline", "Track", 0);
        result.addDefaultKeypress('t', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
        break;
    case CommandIDs::toggleBrowser:
        result.setInfo("Toggle Browser", "Shows or hides the file browser panel", "View", 0);
        result.addDefaultKeypress('b', juce::ModifierKeys::commandModifier);
        result.setTicked(browserVisible);
        break;
    case CommandIDs::toggleMixer:
        result.setInfo("Toggle Mixer", "Shows or hides the mixer panel", "View", 0);
        result.addDefaultKeypress('m', juce::ModifierKeys::commandModifier);
        result.setTicked(mixerVisible);
        break;
    case CommandIDs::togglePianoRoll:
        result.setInfo("Toggle Piano Roll", "Shows or hides the piano roll editor", "View", 0);
        result.addDefaultKeypress('p', juce::ModifierKeys::commandModifier);
        result.setTicked(pianoRollVisible);
        break;

    // Tool mode shortcuts
    case CommandIDs::toolSelect:
        result.setInfo("Select Tool", "Switch to selection tool", "Tools", 0);
        result.addDefaultKeypress('v', 0);
        break;
    case CommandIDs::toolDraw:
        result.setInfo("Draw Tool", "Switch to draw tool for creating regions", "Tools", 0);
        result.addDefaultKeypress('d', 0);
        break;
    case CommandIDs::toolSplit:
        result.setInfo("Split Tool", "Switch to split tool for cutting regions", "Tools", 0);
        result.addDefaultKeypress('s', 0);
        break;
    case CommandIDs::toolErase:
        result.setInfo("Erase Tool", "Switch to erase tool for deleting regions", "Tools", 0);
        result.addDefaultKeypress('e', 0);
        break;
    case CommandIDs::toolMute:
        result.setInfo("Mute Tool", "Switch to mute tool for muting regions", "Tools", 0);
        result.addDefaultKeypress('m', 0);
        break;

    // Edit commands
    case CommandIDs::selectAll:
        result.setInfo("Select All", "Select all regions", "Edit", 0);
        result.addDefaultKeypress('a', juce::ModifierKeys::commandModifier);
        break;
    case CommandIDs::deleteSelection:
        result.setInfo("Delete", "Delete selected region", "Edit", 0);
        result.addDefaultKeypress(juce::KeyPress::deleteKey, 0);
        result.addDefaultKeypress(juce::KeyPress::backspaceKey, 0);
        break;
    case CommandIDs::splitAtPlayhead:
        result.setInfo("Split at Playhead", "Split selected region at playhead position", "Edit", 0);
        result.addDefaultKeypress('b', juce::ModifierKeys::commandModifier);
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
    case CommandIDs::projectNew:
        newProject();
        return true;
    case CommandIDs::save:
        saveProject();
        return true;
    case CommandIDs::saveAs:
        saveProjectAs();
        return true;
    case CommandIDs::collectAllAndSave:
        collectAllAndSave();
        return true;
    case CommandIDs::open:
        openProject();
        return true;
    case CommandIDs::exportAudio:
        exportAudio();
        return true;
    case CommandIDs::undo:
        if (undoManager.undo())
            setHasUnsavedChanges(true);
        return true;
    case CommandIDs::redo:
        if (undoManager.redo())
            setHasUnsavedChanges(true);
        return true;
    case CommandIDs::audioSettings:
        showAudioSettings();
        return true;
    case CommandIDs::addAudioTrack:
        {
            auto& timeline = audioEngine.getTimeline();
            int trackNum = timeline.getNumTracks() + 1;
            auto* newTrack = new AudioTrack("Audio " + juce::String(trackNum));
            newTrack->setColour(juce::Colour::fromHSV(juce::Random::getSystemRandom().nextFloat(), 0.6f, 0.8f, 1.0f));
            undoManager.perform(new AddTrackAction(timeline, newTrack));
            timelineView.resized();
            timelineView.repaint();
            setHasUnsavedChanges(true);
        }
        return true;
    case CommandIDs::addMidiTrack:
        {
            auto& timeline = audioEngine.getTimeline();
            int trackNum = timeline.getNumTracks() + 1;
            auto* newTrack = new MidiTrack("MIDI " + juce::String(trackNum), &audioEngine.getMidiEngine());
            newTrack->setColour(juce::Colour::fromHSV(juce::Random::getSystemRandom().nextFloat(), 0.6f, 0.8f, 1.0f));
            undoManager.perform(new AddTrackAction(timeline, newTrack));
            timelineView.resized();
            timelineView.repaint();
            setHasUnsavedChanges(true);
        }
        return true;
    case CommandIDs::toggleBrowser:
        toggleBrowserPanel();
        return true;
    case CommandIDs::toggleMixer:
        toggleMixerPanel();
        return true;
    case CommandIDs::togglePianoRoll:
        togglePianoRollPanel();
        return true;

    // Tool mode shortcuts
    case CommandIDs::toolSelect:
        toolBar.setToolMode(ToolMode::Select);
        timelineView.setToolMode(ToolMode::Select);
        return true;
    case CommandIDs::toolDraw:
        toolBar.setToolMode(ToolMode::Draw);
        timelineView.setToolMode(ToolMode::Draw);
        return true;
    case CommandIDs::toolSplit:
        toolBar.setToolMode(ToolMode::Split);
        timelineView.setToolMode(ToolMode::Split);
        return true;
    case CommandIDs::toolErase:
        toolBar.setToolMode(ToolMode::Erase);
        timelineView.setToolMode(ToolMode::Erase);
        return true;
    case CommandIDs::toolMute:
        // Mute tool - no dedicated ToolMode, this could toggle region muting
        // For now, just return true to acknowledge the command
        return true;

    // Edit commands
    case CommandIDs::selectAll:
        timelineView.selectFirstRegion();
        return true;
    case CommandIDs::deleteSelection:
        timelineView.deleteSelection();
        setHasUnsavedChanges(true);
        return true;
    case CommandIDs::splitAtPlayhead:
        {
            // Get the current playhead position from the transport
            int64_t playheadPosition = audioEngine.getTransport().getPlayheadPosition();
            timelineView.splitSelectionAtPosition(playheadPosition);
            setHasUnsavedChanges(true);
        }
        return true;

    default:
        return false;
    }
}

//==============================================================================
// MenuBarModel implementation
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "View", "Track" };
}

juce::PopupMenu MainComponent::getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName)
{
    juce::ignoreUnused(menuName);
    juce::PopupMenu menu;

    if (topLevelMenuIndex == 0)  // File menu
    {
        menu.addCommandItem(&commandManager, CommandIDs::projectNew);
        menu.addCommandItem(&commandManager, CommandIDs::open);

        // Open Recent submenu
        juce::PopupMenu recentFilesMenu;
        auto recentFiles = recentFilesManager.getRecentFiles();

        if (recentFiles.size() > 0)
        {
            for (int i = 0; i < recentFiles.size(); ++i)
            {
                auto file = recentFiles[i];
                recentFilesMenu.addItem(1000 + i, file.getFileNameWithoutExtension());
            }
        }
        else
        {
            recentFilesMenu.addItem(-1, "No recent files", false);
        }

        menu.addSubMenu("Open Recent", recentFilesMenu);

        menu.addCommandItem(&commandManager, CommandIDs::save);
        menu.addCommandItem(&commandManager, CommandIDs::saveAs);
        menu.addCommandItem(&commandManager, CommandIDs::collectAllAndSave);
        menu.addSeparator();
        menu.addCommandItem(&commandManager, CommandIDs::exportAudio);
        menu.addSeparator();
        menu.addCommandItem(&commandManager, CommandIDs::audioSettings);
        menu.addSeparator();
        menu.addCommandItem(&commandManager, juce::StandardApplicationCommandIDs::quit);
    }
    else if (topLevelMenuIndex == 1)  // Edit menu
    {
        menu.addCommandItem(&commandManager, CommandIDs::undo);
        menu.addCommandItem(&commandManager, CommandIDs::redo);
        menu.addSeparator();
        menu.addCommandItem(&commandManager, CommandIDs::selectAll);
        menu.addCommandItem(&commandManager, CommandIDs::deleteSelection);
        menu.addSeparator();
        menu.addCommandItem(&commandManager, CommandIDs::splitAtPlayhead);
    }
    else if (topLevelMenuIndex == 2)  // View menu
    {
        menu.addCommandItem(&commandManager, CommandIDs::toggleBrowser);
        menu.addCommandItem(&commandManager, CommandIDs::toggleMixer);
        menu.addCommandItem(&commandManager, CommandIDs::togglePianoRoll);
    }
    else if (topLevelMenuIndex == 3)  // Track menu
    {
        menu.addCommandItem(&commandManager, CommandIDs::addAudioTrack);
        menu.addCommandItem(&commandManager, CommandIDs::addMidiTrack);
    }

    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int topLevelMenuIndex)
{
    // Handle recent files menu items
    if (menuItemID >= 1000 && menuItemID < 2000)
    {
        auto recentFiles = recentFilesManager.getRecentFiles();
        int recentIndex = menuItemID - 1000;

        if (recentIndex < recentFiles.size())
        {
            auto file = recentFiles[recentIndex];

            if (file.existsAsFile())
            {
                // Stop playback
                audioEngine.getTransport().stop();

                // Clear history
                undoManager.clearUndoHistory();

                // Load the project
                ProjectSerializer::loadProject(audioEngine.getTimeline(),
                                              audioEngine.getTransport(),
                                              &audioEngine.getMidiEngine(),
                                              file);

                // Update views
                timelineView.resized();
                timelineView.repaint();
                transportBar.repaint();

                // Update current file and clear dirty flag after successful load
                setCurrentProjectFile(file);
                setHasUnsavedChanges(false);
            }
            else
            {
                // File no longer exists
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                       "File Not Found",
                                                       "The file '" + file.getFileName() + "' could not be found.");
            }
        }
    }
}

void MainComponent::showAudioSettings()
{
    // Save current device state before opening settings
    // This allows us to restore if the new device fails
    saveDeviceState();

    // Pass the MidiEngine as the MIDI input callback so that MIDI devices
    // selected in the settings panel will send events to the MidiEngine
    auto settingsPanel = std::make_unique<SettingsPanel>(deviceManager, &audioEngine.getMidiEngine());

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Audio & MIDI Settings";
    options.dialogBackgroundColour = MidiSingLookAndFeel::backgroundDark;
    options.content.setOwned(settingsPanel.release());
    options.componentToCentreAround = this;
    options.useNativeTitleBar = true;
    options.resizable = true;

    options.launchAsync();
}

//==============================================================================
// Audio Device Management with Crash Protection
//==============================================================================

void MainComponent::setupAudioDeviceManager()
{
    // Request audio permissions if needed (mobile)
    if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
        && !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
            [this](bool granted) {
                try
                {
                    setAudioChannels(granted ? 2 : 0, 2);
                    // Register for device change notifications
                    deviceManager.addChangeListener(this);
                    // Save initial working state
                    saveDeviceState();
                }
                catch (...)
                {
                    handleDeviceError("Failed to initialize audio device");
                }
            });
    }
    else
    {
        try
        {
            setAudioChannels(2, 2);
            // Register for device change notifications
            deviceManager.addChangeListener(this);
            // Save initial working state
            saveDeviceState();
        }
        catch (...)
        {
            handleDeviceError("Failed to initialize audio device");
        }
    }
}

void MainComponent::saveDeviceState()
{
    // Store the current device configuration as XML
    // This allows us to restore if device switching fails
    lastKnownGoodDeviceState = deviceManager.createStateXml();

    // Also store audio parameters
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        lastSampleRate = device->getCurrentSampleRate();
        lastBlockSize = device->getCurrentBufferSizeSamples();
    }
}

void MainComponent::restoreDeviceState()
{
    if (lastKnownGoodDeviceState != nullptr)
    {
        // Attempt to restore the previous device configuration
        juce::String error = deviceManager.initialise(
            2,      // numInputChannels
            2,      // numOutputChannels
            lastKnownGoodDeviceState.get(),
            true    // selectDefaultDeviceOnFailure
        );

        if (error.isNotEmpty())
        {
            // If restore also fails, try to use default device
            error = deviceManager.initialise(2, 2, nullptr, true);

            if (error.isNotEmpty())
            {
                handleDeviceError("Could not restore audio device: " + error);
            }
        }
    }
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager)
    {
        handleDeviceChange();
    }
}

void MainComponent::handleDeviceChange()
{
    // Prevent re-entrant calls during device switching
    if (isChangingDevice.exchange(true))
        return;

    // Stop transport to prevent audio callbacks during device switch
    // This is CRITICAL for ASIO driver stability
    bool wasPlaying = audioEngine.getTransport().isPlaying();
    bool wasRecording = audioEngine.isRecording();

    audioEngine.getTransport().stop();
    if (wasRecording)
        audioEngine.stopRecording();

    // Give the audio system time to fully stop
    juce::Thread::sleep(50);

    auto* currentDevice = deviceManager.getCurrentAudioDevice();

    if (currentDevice != nullptr)
    {
        try
        {
            double newSampleRate = currentDevice->getCurrentSampleRate();
            int newBlockSize = currentDevice->getCurrentBufferSizeSamples();

            // Check if sample rate or block size changed
            bool sampleRateChanged = (std::abs(newSampleRate - lastSampleRate) > 0.1);
            bool blockSizeChanged = (newBlockSize != lastBlockSize);

            if (sampleRateChanged || blockSizeChanged)
            {
                // Re-prepare the audio engine with new settings
                audioEngine.releaseResources();
                audioEngine.prepareToPlay(newBlockSize, newSampleRate);
                timelineView.setSampleRate(newSampleRate);

                lastSampleRate = newSampleRate;
                lastBlockSize = newBlockSize;
            }

            // Device switch successful - save this as known good state
            saveDeviceState();

            // Resume playback if it was playing before
            if (wasPlaying)
            {
                audioEngine.getTransport().play();
            }
        }
        catch (const std::exception& e)
        {
            handleDeviceError(juce::String("Device error: ") + e.what());
            restoreDeviceState();
        }
        catch (...)
        {
            handleDeviceError("Unknown error during device switch");
            restoreDeviceState();
        }
    }
    else
    {
        // No device available - try to restore previous state
        handleDeviceError("Audio device disconnected");
        restoreDeviceState();
    }

    isChangingDevice.store(false);
}

void MainComponent::handleDeviceError(const juce::String& errorMessage)
{
    // Log the error
    juce::Logger::writeToLog("Audio Device Error: " + errorMessage);

    // Show error message to user on the message thread
    juce::MessageManager::callAsync([errorMessage]()
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Audio Device Error",
            errorMessage + "\n\nThe previous device settings will be restored if possible.",
            "OK"
        );
    });
}

//==============================================================================
// Collapsible Panel System
//==============================================================================

void MainComponent::setupCollapsiblePanels()
{
    // Create Browser panel with FileBrowser
    browserContent = std::make_unique<FileBrowser>();
    browserContent->setDeviceManager(&deviceManager);

    // Set up callbacks for file interactions
    browserContent->onFileDoubleClicked = [this](const juce::File& file)
    {
        // Import file to timeline when double-clicked
        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg")
        {
            // Create new audio track and import file
            auto& timeline = audioEngine.getTimeline();
            int trackNum = timeline.getNumTracks() + 1;
            auto* newTrack = new AudioTrack("Audio " + juce::String(trackNum));
            newTrack->setColour(juce::Colour::fromHSV(juce::Random::getSystemRandom().nextFloat(), 0.6f, 0.8f, 1.0f));
            timeline.addTrack(newTrack);

            // Import audio to the new track at position 0
            AudioImporter importer;
            auto buffer = importer.loadFile(file, 44100.0);
            if (buffer != nullptr && buffer->getNumSamples() > 0)
            {
                auto region = std::make_unique<AudioRegion>(0, buffer->getNumSamples());
                region->setAudioBuffer(*buffer);
                region->setName(file.getFileNameWithoutExtension());
                region->setFilePath(file.getFullPathName());
                newTrack->addRegion(std::move(region));
            }

            timelineView.resized();
            timelineView.repaint();
            setHasUnsavedChanges(true);
        }
        else if (ext == ".mid" || ext == ".midi")
        {
            // Create new MIDI track and import file
            auto& timeline = audioEngine.getTimeline();
            int trackNum = timeline.getNumTracks() + 1;
            auto* newTrack = new MidiTrack("MIDI " + juce::String(trackNum), &audioEngine.getMidiEngine());
            newTrack->setColour(juce::Colour::fromHSV(juce::Random::getSystemRandom().nextFloat(), 0.6f, 0.8f, 1.0f));
            timeline.addTrack(newTrack);

            // Import MIDI to the new track (track 0 from MIDI file)
            MidiImporter midiImporter;
            auto midiSequence = midiImporter.importTrack(file, 0, 44100.0);
            if (midiSequence.getNumEvents() > 0)
            {
                // Calculate length from last event timestamp
                double lastTime = midiSequence.getEndTime();
                int64_t lengthSamples = static_cast<int64_t>(lastTime * 44100.0) + 44100; // +1 second buffer
                
                auto region = std::make_unique<MidiRegion>(0, lengthSamples);
                region->setMidiSequence(midiSequence);
                region->setName(file.getFileNameWithoutExtension());
                newTrack->addRegion(std::move(region));
            }

            timelineView.resized();
            timelineView.repaint();
            setHasUnsavedChanges(true);
        }
    };

    browserPanel = std::make_unique<CollapsiblePanel>("File Browser", CollapsiblePanel::Position::Left);
    browserPanel->setContent(browserContent.get());
    browserPanel->onCollapse = [this]() { toggleBrowserPanel(); };
    // Start hidden - user can toggle via View menu
    browserPanel->setVisible(false);

    // Create Mixer panel
    mixerContent = std::make_unique<MixerPanel>();
    mixerContent->setTimeline(&audioEngine.getTimeline());
    mixerContent->setMixer(&audioEngine.getMixer());
    mixerPanel = std::make_unique<CollapsiblePanel>("Mixer", CollapsiblePanel::Position::Bottom);
    mixerPanel->setContent(mixerContent.get());
    mixerPanel->onCollapse = [this]() { toggleMixerPanel(); };
    // Start hidden
    mixerPanel->setVisible(false);

    // Create Piano Roll panel
    pianoRollContent = std::make_unique<PianoRoll>();
    pianoRollPanel = std::make_unique<CollapsiblePanel>("Piano Roll", CollapsiblePanel::Position::Bottom);
    pianoRollPanel->setContent(pianoRollContent.get());
    pianoRollPanel->onCollapse = [this]() { togglePianoRollPanel(); };
    // Start hidden
    pianoRollPanel->setVisible(false);

    // Add panels to component (order matters for z-order)
    addChildComponent(browserPanel.get());
    addChildComponent(mixerPanel.get());
    addChildComponent(pianoRollPanel.get());
}

void MainComponent::toggleBrowserPanel()
{
    browserVisible = !browserVisible;

    if (browserPanel != nullptr)
    {
        browserPanel->setVisible(browserVisible);
    }

    // Update layout
    resized();

    // Update menu checkmarks
    commandManager.commandStatusChanged();
}

void MainComponent::toggleMixerPanel()
{
    mixerVisible = !mixerVisible;

    if (mixerPanel != nullptr)
    {
        mixerPanel->setVisible(mixerVisible);
    }

    // If showing mixer and piano roll is visible, hide piano roll
    // (they share the same bottom space)
    if (mixerVisible && pianoRollVisible)
    {
        pianoRollVisible = false;
        if (pianoRollPanel != nullptr)
            pianoRollPanel->setVisible(false);
    }

    // Update layout
    resized();

    // Update menu checkmarks
    commandManager.commandStatusChanged();
}

void MainComponent::togglePianoRollPanel()
{
    pianoRollVisible = !pianoRollVisible;

    if (pianoRollPanel != nullptr)
    {
        pianoRollPanel->setVisible(pianoRollVisible);
    }

    // If showing piano roll and mixer is visible, hide mixer
    // (they share the same bottom space)
    if (pianoRollVisible && mixerVisible)
    {
        mixerVisible = false;
        if (mixerPanel != nullptr)
            mixerPanel->setVisible(false);
    }

    // Update layout
    resized();

    // Update menu checkmarks
    commandManager.commandStatusChanged();
}

void MainComponent::showPianoRollWithRegion(MidiRegion* region)
{
    // Ensure piano roll panel is visible
    if (!pianoRollVisible)
    {
        pianoRollVisible = true;

        if (pianoRollPanel != nullptr)
        {
            pianoRollPanel->setVisible(true);
        }

        // If showing piano roll and mixer is visible, hide mixer
        // (they share the same bottom space)
        if (mixerVisible)
        {
            mixerVisible = false;
            if (mixerPanel != nullptr)
                mixerPanel->setVisible(false);
        }

        // Update layout
        resized();

        // Update menu checkmarks
        commandManager.commandStatusChanged();
    }

    // Set the MIDI region on the piano roll
    if (pianoRollContent != nullptr && region != nullptr)
    {
        pianoRollContent->setMidiRegion(region);
    }
}

void MainComponent::updatePanelLayout()
{
    // Force layout update
    resized();
}

//==============================================================================
// Resizable Splitter System
//==============================================================================

void MainComponent::setupSplitters()
{
    // Create browser splitter (vertical - splits left from right)
    browserSplitter = std::make_unique<ResizableSplitter>(ResizableSplitter::Orientation::Vertical);
    browserSplitter->setCurrentPosition(browserPanelWidth);
    browserSplitter->setLimits(150, 500);  // Min 150px, max 500px

    browserSplitter->onDrag = [this](int newPosition)
    {
        browserPanelWidth = newPosition;
        resized();
    };

    browserSplitter->onDragEnd = [this]()
    {
        // Could save panel sizes to user preferences here
    };

    addChildComponent(browserSplitter.get());

    // Create bottom splitter (horizontal - splits top from bottom)
    bottomSplitter = std::make_unique<ResizableSplitter>(ResizableSplitter::Orientation::Horizontal);
    bottomSplitter->setLimits(100, 500);  // Min 100px, max 500px

    bottomSplitter->onDrag = [this](int newPosition)
    {
        // Update the appropriate panel height based on which is visible
        if (pianoRollVisible)
        {
            pianoRollPanelHeight = newPosition;
        }
        else if (mixerVisible)
        {
            mixerPanelHeight = newPosition;
        }
        resized();
    };

    bottomSplitter->onDragEnd = [this]()
    {
        // Could save panel sizes to user preferences here
    };

    addChildComponent(bottomSplitter.get());
}

void MainComponent::updateSplitterPositions()
{
    // Update browser splitter limits based on available width
    if (browserSplitter != nullptr)
    {
        int maxBrowserWidth = getWidth() - 400;  // Leave at least 400px for timeline
        browserSplitter->setLimits(150, juce::jmax(150, maxBrowserWidth));
        browserSplitter->setCurrentPosition(browserPanelWidth);
    }

    // Update bottom splitter limits based on available height
    if (bottomSplitter != nullptr)
    {
        // Calculate available height for bottom panel
        int menuHeight = juce::LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight();
        int transportHeight = 50;
        int toolBarHeight = 40;
        int statusHeight = 24;
        int spectrumHeight = 60;
        int minTimelineHeight = 200;

        int availableHeight = getHeight() - menuHeight - transportHeight - toolBarHeight - statusHeight - spectrumHeight;
        int maxBottomHeight = availableHeight - minTimelineHeight;

        bottomSplitter->setLimits(100, juce::jmax(100, maxBottomHeight));

        // Set current position based on which panel is visible
        if (pianoRollVisible)
        {
            bottomSplitter->setCurrentPosition(pianoRollPanelHeight);
        }
        else if (mixerVisible)
        {
            bottomSplitter->setCurrentPosition(mixerPanelHeight);
        }
    }
}

//==============================================================================
// Export progress dialog with cancel support
class ExportProgressTask : public juce::ThreadWithProgressWindow
{
public:
    ExportProgressTask(AudioEngine& engine, const juce::File& outputFile, int64_t lengthInSamples)
        : juce::ThreadWithProgressWindow("Exporting Audio...", true, true),
          audioEngine(engine),
          file(outputFile),
          lengthSamples(lengthInSamples)
    {
        setStatusMessage("Preparing to export...");
    }

    void run() override
    {
        AudioExporter exporter;
        exporter.setSampleRate(44100.0);
        exporter.setBitDepth(16);

        setStatusMessage("Rendering audio...");

        exportSuccess = exporter.exportToFile(audioEngine, file, 0, lengthSamples,
            [this](float progress)
            {
                // Update progress bar (0.0 to 1.0)
                setProgress(static_cast<double>(progress));

                // Update status message with percentage
                int percent = static_cast<int>(progress * 100.0f);
                setStatusMessage("Exporting... " + juce::String(percent) + "%");

                // Check for user cancellation
                if (threadShouldExit())
                {
                    wasCancelled = true;
                    // Note: We can't actually stop the export mid-way since AudioExporter
                    // doesn't support cancellation. We'll delete the file after completion.
                }
            });
    }

    bool wasSuccessful() const { return exportSuccess && !wasCancelled; }
    bool wasCancelledByUser() const { return wasCancelled; }
    juce::File getOutputFile() const { return file; }

private:
    AudioEngine& audioEngine;
    juce::File file;
    int64_t lengthSamples;
    bool exportSuccess = false;
    bool wasCancelled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportProgressTask)
};

void MainComponent::exportAudio()
{
    // Stop playback
    audioEngine.getTransport().stop();

    fileChooser = std::make_unique<juce::FileChooser>("Export Audio",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.wav");

    auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File())
        {
            if (!file.hasFileExtension("wav"))
                file = file.withFileExtension("wav");

            // Get timeline length in samples
            auto& timeline = audioEngine.getTimeline();
            int64_t endSample = timeline.getEndSample();

            if (endSample <= 0)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                       "Export Failed",
                                                       "The project is empty. Add some audio or MIDI clips before exporting.");
                return;
            }

            // Create and run export task with progress dialog
            ExportProgressTask exportTask(audioEngine, file, endSample);

            // runThread() shows modal dialog and returns true when thread finishes
            if (exportTask.runThread())
            {
                if (exportTask.wasSuccessful())
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                                           "Export Complete",
                                                           "Audio exported successfully to:\n" + file.getFullPathName());
                }
                else if (exportTask.wasCancelledByUser())
                {
                    // Delete the partial/complete file since user cancelled
                    file.deleteFile();
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                                           "Export Cancelled",
                                                           "The export was cancelled.");
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                           "Export Failed",
                                                           "Failed to export audio. Please check the file path and try again.");
                }
            }
            else
            {
                // runThread() returned false - user closed the dialog early or error occurred
                file.deleteFile();
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                                       "Export Cancelled",
                                                       "The export was cancelled.");
            }
        }
    });
}

//==============================================================================
// Auto-Save and Recovery System
//==============================================================================

juce::File MainComponent::getAutoSaveFolder() const
{
    // Use the application data directory for auto-save files
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appDataDir.getChildFile("MidiSing").getChildFile("AutoSave");
}

juce::File MainComponent::getRecoveryFile() const
{
    return getAutoSaveFolder().getChildFile("recovery.midising");
}

void MainComponent::setupAutoSave()
{
    // Create auto-save folder if it doesn't exist
    auto autoSaveFolder = getAutoSaveFolder();
    if (!autoSaveFolder.exists())
    {
        autoSaveFolder.createDirectory();
    }

    // Check for existing recovery file (from previous crash)
    checkForRecoveryFile();

    // Start the auto-save timer
    lastAutoSaveTime = juce::Time::getCurrentTime();
    startTimer(autoSaveIntervalMs);

    juce::Logger::writeToLog("Auto-save system initialized (interval: 5 minutes)");
}

void MainComponent::timerCallback()
{
    // Only auto-save if there are unsaved changes
    if (autoSaveEnabled.load() && hasUnsavedChanges_)
    {
        performAutoSave();
    }
}

void MainComponent::performAutoSave()
{
    auto recoveryFile = getRecoveryFile();

    // Create a temporary file first, then move to avoid corruption
    auto tempFile = recoveryFile.getSiblingFile("recovery_temp.midising");

    try
    {
        // Save the project to the temp file
        ProjectSerializer::saveProject(audioEngine.getTimeline(), audioEngine.getTransport(), tempFile);

        // If we have a current project file, store its path in a companion file
        // so we know where to suggest saving on recovery
        if (currentProjectFile != juce::File())
        {
            auto infoFile = getAutoSaveFolder().getChildFile("recovery_info.txt");
            infoFile.replaceWithText(currentProjectFile.getFullPathName());
        }

        // Move temp file to recovery file (atomic on most file systems)
        if (tempFile.existsAsFile())
        {
            if (recoveryFile.existsAsFile())
            {
                recoveryFile.deleteFile();
            }
            tempFile.moveFileTo(recoveryFile);
        }

        lastAutoSaveTime = juce::Time::getCurrentTime();

        juce::Logger::writeToLog("Auto-save completed: " + recoveryFile.getFullPathName());
    }
    catch (...)
    {
        juce::Logger::writeToLog("Auto-save failed");
        // Clean up temp file if it exists
        if (tempFile.existsAsFile())
        {
            tempFile.deleteFile();
        }
    }
}

void MainComponent::checkForRecoveryFile()
{
    auto recoveryFile = getRecoveryFile();

    if (!recoveryFile.existsAsFile())
        return;

    // Get the timestamp of the recovery file
    auto recoveryTime = recoveryFile.getLastModificationTime();
    auto formattedTime = recoveryTime.formatted("%Y-%m-%d %H:%M:%S");

    // Check if there's info about the original project
    juce::String originalProjectInfo;
    auto infoFile = getAutoSaveFolder().getChildFile("recovery_info.txt");
    if (infoFile.existsAsFile())
    {
        juce::String originalPath = infoFile.loadFileAsString().trim();
        if (originalPath.isNotEmpty())
        {
            juce::File originalFile(originalPath);
            originalProjectInfo = "\n\nOriginal project: " + originalFile.getFileName();
        }
    }

    // Show recovery prompt
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Recover Unsaved Work")
        .withMessage("MidiSing found auto-saved work from a previous session.\n\n"
                     "Last auto-save: " + formattedTime +
                     originalProjectInfo +
                     "\n\nWould you like to recover this work?")
        .withButton("Recover")
        .withButton("Discard");

    juce::AlertWindow::showAsync(options, [this, recoveryFile](int result)
    {
        if (result == 1)  // Recover
        {
            // Load the recovery file
            audioEngine.getTransport().stop();
            undoManager.clearUndoHistory();

            ProjectSerializer::loadProject(audioEngine.getTimeline(),
                                          audioEngine.getTransport(),
                                          &audioEngine.getMidiEngine(),
                                          recoveryFile);

            // Update views
            timelineView.resized();
            timelineView.repaint();
            transportBar.repaint();

            // Mark as having unsaved changes since this is recovered work
            setHasUnsavedChanges(true);

            // Check if we have info about the original project file
            auto infoFile = getAutoSaveFolder().getChildFile("recovery_info.txt");
            if (infoFile.existsAsFile())
            {
                juce::String originalPath = infoFile.loadFileAsString().trim();
                if (originalPath.isNotEmpty())
                {
                    juce::File originalFile(originalPath);
                    // Set the project file so user can easily save to same location
                    setCurrentProjectFile(originalFile);
                }
            }

            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                                   "Recovery Complete",
                                                   "Your work has been recovered. Please save your project to preserve these changes.");
        }
        else  // Discard
        {
            deleteRecoveryFile();
        }
    });
}

void MainComponent::deleteRecoveryFile()
{
    auto recoveryFile = getRecoveryFile();
    if (recoveryFile.existsAsFile())
    {
        recoveryFile.deleteFile();
    }

    // Also delete the info file
    auto infoFile = getAutoSaveFolder().getChildFile("recovery_info.txt");
    if (infoFile.existsAsFile())
    {
        infoFile.deleteFile();
    }
}

