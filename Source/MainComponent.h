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
#include "UI/TimelineView.h"
#include "UI/StatusBar.h"
#include "UI/SpectrumDisplay.h"
#include "Utils/UndoManager.h"
#include "Utils/RecentFilesManager.h"

//==============================================================================
class MainComponent : public juce::AudioAppComponent,
                      public juce::KeyListener,
                      public juce::ApplicationCommandTarget,
                      public juce::MenuBarModel
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

private:
    void updateWindowTitle();
    void setupTransportCallbacks();
    void createDemoTracks();
    void setupCommands();
    void createNewProject();
    void showAudioSettings();

    juce::ApplicationCommandManager commandManager;
    juce::MenuBarComponent menuBar;

    MidiSingLookAndFeel lookAndFeel;
    AudioEngine audioEngine;
    TransportBar transportBar;
    TimelineView timelineView;
    StatusBar statusBar;
    SpectrumDisplay spectrumDisplay;
    DAWUndoManager undoManager;
    RecentFilesManager recentFilesManager;
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool hasUnsavedChanges_ = false;
    juce::File currentProjectFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
