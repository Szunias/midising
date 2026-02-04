#pragma once

#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <vector>

/**
 * FileBrowserItem represents a single file/folder item in the file list.
 * Supports drag-and-drop to timeline for audio/MIDI files.
 */
class FileBrowserItem : public juce::Component,
                        public juce::DragAndDropContainer
{
public:
    FileBrowserItem(const juce::File& file, bool isDirectory);
    ~FileBrowserItem() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    const juce::File& getFile() const { return file; }
    bool isDirectory() const { return isDir; }

    // Callbacks
    std::function<void(const juce::File&)> onFileClicked;
    std::function<void(const juce::File&)> onFileDoubleClicked;
    std::function<void(const juce::File&)> onDirectorySelected;

private:
    juce::File file;
    bool isDir;
    bool isHovered = false;
    bool isSelected = false;

    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileBrowserItem)
};

/**
 * AudioPreviewComponent handles audio file preview playback.
 * Shows waveform and allows click-to-preview functionality.
 * Features:
 * - Waveform visualization with playhead
 * - Volume control for preview
 * - Auto-play on file selection (optional)
 * - Loop mode for sample previewing
 * - Click-to-seek in waveform
 */
class AudioPreviewComponent : public juce::Component,
                               public juce::Timer,
                               public juce::ChangeListener
{
public:
    AudioPreviewComponent();
    ~AudioPreviewComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void setFile(const juce::File& file);
    void play();
    void stop();
    bool isPlaying() const { return transportSource.isPlaying(); }

    // Set the audio device manager for audio preview
    void setDeviceManager(juce::AudioDeviceManager* dm);

    // Volume control (0.0 to 1.0)
    void setVolume(float newVolume);
    float getVolume() const { return volume; }

    // Auto-play when file is selected
    void setAutoPlay(bool shouldAutoPlay) { autoPlay = shouldAutoPlay; }
    bool getAutoPlay() const { return autoPlay; }

    // Loop mode
    void setLooping(bool shouldLoop);
    bool getLooping() const { return looping; }

    // Get file info
    double getDurationSeconds() const;
    double getSampleRate() const { return currentSampleRate; }
    int getNumChannels() const { return currentNumChannels; }
    int getBitsPerSample() const { return currentBitsPerSample; }
    juce::String getFileInfo() const;

private:
    void loadFile(const juce::File& file);
    void drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds);
    void updateTransportState();
    void prepareAudioPlayer();

    juce::AudioDeviceManager* deviceManager = nullptr;
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioSourcePlayer audioSourcePlayer;
    bool audioPlayerPrepared = false;

    juce::File currentFile;
    juce::AudioBuffer<float> waveformBuffer;
    bool fileLoaded = false;

    // Audio file properties
    double currentSampleRate = 44100.0;
    int currentNumChannels = 2;
    int currentBitsPerSample = 16;
    int64_t currentLengthInSamples = 0;

    // Playback settings
    float volume = 0.8f;
    bool autoPlay = true;
    bool looping = false;

    // UI Components
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton loopButton;
    juce::Slider volumeSlider;
    juce::Label fileNameLabel;
    juce::Label durationLabel;
    juce::Label infoLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPreviewComponent)
};

/**
 * LocationButton represents a quick-access location button (Home, Desktop, etc.)
 */
class LocationButton : public juce::TextButton
{
public:
    LocationButton(const juce::String& name, const juce::File& location);

    const juce::File& getLocation() const { return locationPath; }

private:
    juce::File locationPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LocationButton)
};

/**
 * FileBrowser provides a comprehensive file browser for audio samples and plugins.
 * Features:
 * - Folder navigation with back/forward history
 * - Quick-access location buttons
 * - File filtering for audio/MIDI files
 * - Audio preview on click
 * - Drag-and-drop to timeline for creating tracks
 */
class FileBrowser : public juce::Component,
                    public juce::FileBrowserListener,
                    public juce::DragAndDropContainer
{
public:
    FileBrowser();
    ~FileBrowser() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // FileBrowserListener implementation
    void selectionChanged() override;
    void fileClicked(const juce::File& file, const juce::MouseEvent& e) override;
    void fileDoubleClicked(const juce::File& file) override;
    void browserRootChanged(const juce::File& newRoot) override;

    // Set the audio device manager for preview
    void setDeviceManager(juce::AudioDeviceManager* dm);

    // Navigation
    void navigateTo(const juce::File& directory);
    void navigateBack();
    void navigateForward();
    void navigateUp();
    void refresh();

    // Favorites management
    void addToFavorites(const juce::File& file);
    void removeFromFavorites(const juce::File& file);
    bool isFavorite(const juce::File& file) const;

    // Callbacks
    std::function<void(const juce::File&)> onFileSelected;
    std::function<void(const juce::File&)> onFileDoubleClicked;
    std::function<void(const juce::File&)> onFileDraggedToTimeline;

    // Get currently selected file
    juce::File getSelectedFile() const;

private:
    void setupLocationButtons();
    void setupFileBrowser();
    void updateNavigationButtons();
    void loadFavorites();
    void saveFavorites();
    juce::String getFileTypeIcon(const juce::File& file) const;
    bool isAudioFile(const juce::File& file) const;
    bool isMidiFile(const juce::File& file) const;
    bool isSupportedFile(const juce::File& file) const;

    // UI Components
    std::unique_ptr<juce::FileBrowserComponent> fileBrowser;
    std::unique_ptr<juce::WildcardFileFilter> fileFilter;
    std::unique_ptr<AudioPreviewComponent> previewComponent;

    // Navigation
    juce::TextButton backButton;
    juce::TextButton forwardButton;
    juce::TextButton upButton;
    juce::TextButton refreshButton;
    juce::TextButton homeButton;

    // Location buttons
    std::vector<std::unique_ptr<LocationButton>> locationButtons;

    // Navigation history
    std::vector<juce::File> historyBack;
    std::vector<juce::File> historyForward;
    juce::File currentDirectory;

    // Favorites
    std::vector<juce::File> favoriteLocations;

    // Search
    juce::TextEditor searchBox;
    juce::Label searchLabel;

    // Status
    juce::Label statusLabel;

    // Audio device manager reference
    juce::AudioDeviceManager* deviceManager = nullptr;

    // Layout constants
    static constexpr int TOOLBAR_HEIGHT = 30;
    static constexpr int LOCATION_BAR_HEIGHT = 28;
    static constexpr int PREVIEW_HEIGHT = 100;  // Increased for waveform + info + controls
    static constexpr int SEARCH_HEIGHT = 26;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileBrowser)
};
