#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "Audio/AudioEngine.h"
#include "UI/LookAndFeel.h"
#include "UI/TransportBar.h"
#include "UI/ToolBar.h"
#include "UI/TimelineView.h"
#include "UI/StatusBar.h"
#include "UI/SpectrumDisplay.h"
#include "UI/CollapsiblePanel.h"
#include "UI/BrowserPanel.h"
#include "UI/FileBrowser.h"
#include "UI/MixerPanel.h"
#include "UI/PianoRoll.h"
#include "UI/ResizableSplitter.h"
#include "Utils/UndoManager.h"
#include "Utils/RecentFilesManager.h"

//==============================================================================
/**
 * MainComponent is the root component of the DAW.
 * Implements robust audio device management with crash protection
 * for switching between ASIO, DirectSound, and WASAPI drivers.
 * Includes auto-save functionality with crash recovery support.
 */
class MainComponent : public juce::AudioAppComponent,
                      public juce::KeyListener,
                      public juce::ApplicationCommandTarget,
                      public juce::MenuBarModel,
                      public juce::ChangeListener,
                      public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Keyboard handling
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

    // ApplicationCommandTarget overrides
    ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands(juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;
    bool perform(const InvocationInfo& info) override;

    // MenuBarModel overrides
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    // ChangeListener override - handles audio device changes
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Timer override - handles auto-save
    void timerCallback() override;

    // Access to audio engine
    AudioEngine& getAudioEngine() { return audioEngine; }
    DAWUndoManager& getUndoManager() { return undoManager; }
    juce::ApplicationCommandManager& getCommandManager() { return commandManager; }

    // Unsaved changes tracking
    bool hasUnsavedChanges() const { return hasUnsavedChanges_; }
    void setHasUnsavedChanges(bool dirty);

    // Project file tracking
    void setCurrentProjectFile(const juce::File& file);
    juce::File getCurrentProjectFile() const { return currentProjectFile; }

    // Project management
    void newProject();
    void saveProject();
    void saveProjectAs();
    void openProject();
    void exportAudio();
    void collectAllAndSave();

private:
    void updateWindowTitle();
    void setupTransportCallbacks();
    void createDemoTracks();
    void setupCommands();
    void createNewProject();
    void showAudioSettings();
    void showAboutDialog();
    void showKeyboardShortcuts();
    void exportMidi();

    // Audio device management with crash protection
    void setupAudioDeviceManager();
    void saveDeviceState();
    void restoreDeviceState();
    void handleDeviceChange();
    void handleDeviceError(const juce::String& errorMessage);

    // Panel visibility management
    void setupCollapsiblePanels();
    void toggleBrowserPanel();
    void toggleMixerPanel();
    void togglePianoRollPanel();
    void showPianoRollWithRegion(MidiRegion* region);
    void updatePanelLayout();

    // Auto-save and recovery system
    void setupAutoSave();
    void performAutoSave();
    void checkForRecoveryFile();
    void deleteRecoveryFile();
    juce::File getRecoveryFile() const;
    juce::File getAutoSaveFolder() const;

    juce::ApplicationCommandManager commandManager;
    juce::MenuBarComponent menuBar;

    MidiSingLookAndFeel lookAndFeel;
    AudioEngine audioEngine;
    TransportBar transportBar;
    ToolBar toolBar;
    TimelineView timelineView;
    StatusBar statusBar;
    SpectrumDisplay spectrumDisplay;
    DAWUndoManager undoManager;
    RecentFilesManager recentFilesManager;
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool hasUnsavedChanges_ = false;
    juce::File currentProjectFile;

    // Audio device state management for crash protection
    std::unique_ptr<juce::XmlElement> lastKnownGoodDeviceState;
    std::atomic<bool> isChangingDevice { false };
    double lastSampleRate = 44100.0;
    int lastBlockSize = 512;

    // Collapsible panels
    std::unique_ptr<CollapsiblePanel> browserPanel;
    std::unique_ptr<CollapsiblePanel> mixerPanel;
    std::unique_ptr<CollapsiblePanel> pianoRollPanel;

    // Panel content components
    std::unique_ptr<FileBrowser> browserContent;
    std::unique_ptr<MixerPanel> mixerContent;
    std::unique_ptr<PianoRoll> pianoRollContent;

    // Panel visibility state
    bool browserVisible = false;
    bool mixerVisible = false;
    bool pianoRollVisible = false;

    // Panel sizes (for restoration after collapse)
    int browserPanelWidth = 250;
    int mixerPanelHeight = 200;
    int pianoRollPanelHeight = 250;

    // Resizable splitters between panels
    std::unique_ptr<ResizableSplitter> browserSplitter;   // Vertical splitter for browser
    std::unique_ptr<ResizableSplitter> bottomSplitter;    // Horizontal splitter for mixer/piano roll

    // Splitter setup
    void setupSplitters();
    void updateSplitterPositions();

    // Auto-save settings
    static constexpr int autoSaveIntervalMs = 5 * 60 * 1000;  // 5 minutes in milliseconds
    std::atomic<bool> autoSaveEnabled { true };
    juce::Time lastAutoSaveTime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
