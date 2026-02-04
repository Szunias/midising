#pragma once

#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_data_structures/juce_data_structures.h>
#include <memory>
#include <vector>

/**
 * FavoritesManager manages the list of bookmarked/favorite folders.
 * Uses PropertiesFile for persistent storage.
 */
class FavoritesManager
{
public:
    FavoritesManager();
    ~FavoritesManager();

    /**
     * Adds a folder to favorites.
     * The folder will be added if not already present.
     */
    void addFavorite(const juce::File& folder);

    /**
     * Removes a folder from favorites.
     */
    void removeFavorite(const juce::File& folder);

    /**
     * Checks if a folder is in favorites.
     */
    bool isFavorite(const juce::File& folder) const;

    /**
     * Returns the list of favorite folders.
     */
    juce::Array<juce::File> getFavorites() const;

    /**
     * Clears all favorites.
     */
    void clear();

    /**
     * Returns the maximum number of favorites to track.
     */
    int getMaxNumberOfFavorites() const { return maxNumberOfFavorites; }

    /**
     * Listener interface for favorites changes.
     */
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void favoritesChanged() = 0;
    };

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    void loadFromProperties();
    void saveToProperties();
    void notifyListeners();

    std::unique_ptr<juce::PropertiesFile> propertiesFile;
    juce::Array<juce::File> favorites;
    juce::ListenerList<Listener> listeners;

    static constexpr int maxNumberOfFavorites = 20;
    static constexpr const char* favoritesKey = "fileBrowserFavorites";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FavoritesManager)
};

/**
 * FavoriteButton represents a single favorite folder in the favorites panel.
 */
class FavoriteButton : public juce::Component
{
public:
    FavoriteButton(const juce::File& folder, FavoritesManager& manager);
    ~FavoriteButton() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    const juce::File& getFolder() const { return folder; }

    std::function<void(const juce::File&)> onFolderClicked;

private:
    juce::File folder;
    FavoritesManager& favoritesManager;
    juce::TextButton removeButton;
    bool isHovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FavoriteButton)
};

/**
 * FavoritesPanel displays and manages the list of favorite folders.
 */
class FavoritesPanel : public juce::Component,
                       public FavoritesManager::Listener
{
public:
    FavoritesPanel(FavoritesManager& manager);
    ~FavoritesPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // FavoritesManager::Listener
    void favoritesChanged() override;

    std::function<void(const juce::File&)> onFavoriteSelected;

private:
    void rebuildFavoriteButtons();

    FavoritesManager& favoritesManager;
    juce::Label titleLabel;
    std::vector<std::unique_ptr<FavoriteButton>> favoriteButtons;
    juce::Viewport viewport;
    juce::Component buttonsContainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FavoritesPanel)
};

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
 * - Favorites/bookmarks for frequently accessed folders
 */
class FileBrowser : public juce::Component,
                    public juce::FileBrowserListener,
                    public juce::DragAndDropContainer,
                    public FavoritesManager::Listener
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

    // FavoritesManager::Listener implementation
    void favoritesChanged() override;

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
    void toggleCurrentDirectoryFavorite();
    juce::Array<juce::File> getFavorites() const;

    // Show/hide favorites panel
    void setFavoritesPanelVisible(bool shouldBeVisible);
    bool isFavoritesPanelVisible() const { return favoritesPanelVisible; }

    // Callbacks
    std::function<void(const juce::File&)> onFileSelected;
    std::function<void(const juce::File&)> onFileDoubleClicked;
    std::function<void(const juce::File&)> onFileDraggedToTimeline;

    // Get currently selected file
    juce::File getSelectedFile() const;

private:
    void setupLocationButtons();
    void setupFileBrowser();
    void setupFavoritesPanel();
    void updateNavigationButtons();
    void updateFavoriteButton();
    juce::String getFileTypeIcon(const juce::File& file) const;
    bool isAudioFile(const juce::File& file) const;
    bool isMidiFile(const juce::File& file) const;
    bool isSupportedFile(const juce::File& file) const;

    // UI Components
    std::unique_ptr<juce::FileBrowserComponent> fileBrowser;
    std::unique_ptr<juce::WildcardFileFilter> fileFilter;
    std::unique_ptr<AudioPreviewComponent> previewComponent;

    // Navigation buttons
    juce::TextButton backButton;
    juce::TextButton forwardButton;
    juce::TextButton upButton;
    juce::TextButton refreshButton;
    juce::TextButton homeButton;
    juce::TextButton favoriteButton;  // Star button to toggle favorites

    // Location buttons
    std::vector<std::unique_ptr<LocationButton>> locationButtons;

    // Navigation history
    std::vector<juce::File> historyBack;
    std::vector<juce::File> historyForward;
    juce::File currentDirectory;

    // Favorites management
    std::unique_ptr<FavoritesManager> favoritesManager;
    std::unique_ptr<FavoritesPanel> favoritesPanel;
    bool favoritesPanelVisible = true;

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
    static constexpr int FAVORITES_PANEL_WIDTH = 140;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileBrowser)
};
