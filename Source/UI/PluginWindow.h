#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

// Forward declarations
class PluginInstance;
class PresetManager;

/**
 * Window for hosting plugin GUI (AudioProcessorEditor).
 *
 * Features:
 *   - Inherits from juce::DocumentWindow for native window behavior
 *   - Hosts the plugin's custom editor or GenericAudioProcessorEditor
 *   - Handles window close button (hides or destroys window)
 *   - Persists window position and size across sessions
 *   - Supports window resizing with plugin editor constraints
 *   - Shows plugin name and status in title bar
 *
 * Position Persistence:
 *   Window bounds are saved to ApplicationProperties keyed by plugin identifier.
 *   On creation, if saved bounds exist, window opens at that position.
 *
 * Usage:
 *   auto window = std::make_unique<PluginWindow>(pluginInstance, ...);
 *   window->setVisible(true);
 */
class PluginWindow : public juce::DocumentWindow
{
public:
    /**
     * Behavior when close button is pressed.
     */
    enum class CloseAction
    {
        Hide,           ///< Hide the window (can be shown again)
        DeleteWindow    ///< Delete the window entirely
    };

    //==========================================================================
    // Construction/Destruction
    //==========================================================================

    /**
     * Create a plugin window for the given plugin instance.
     *
     * @param plugin The plugin instance whose editor to host (must not be null)
     * @param closeAction What to do when close button is pressed
     * @param useGenericIfNoEditor If plugin has no custom editor, use GenericAudioProcessorEditor
     */
    PluginWindow(PluginInstance* plugin,
                 CloseAction closeAction = CloseAction::Hide,
                 bool useGenericIfNoEditor = true);

    /**
     * Create a plugin window with explicit title.
     *
     * @param plugin The plugin instance whose editor to host (must not be null)
     * @param title Window title (if empty, uses plugin name)
     * @param closeAction What to do when close button is pressed
     * @param useGenericIfNoEditor If plugin has no custom editor, use GenericAudioProcessorEditor
     */
    PluginWindow(PluginInstance* plugin,
                 const juce::String& title,
                 CloseAction closeAction = CloseAction::Hide,
                 bool useGenericIfNoEditor = true);

    /**
     * Destructor - saves window position before destroying.
     */
    ~PluginWindow() override;

    //==========================================================================
    // DocumentWindow Overrides
    //==========================================================================

    /**
     * Called when the close button is pressed.
     * Depending on CloseAction, either hides the window or triggers deletion.
     */
    void closeButtonPressed() override;

    /**
     * Called when the window is moved.
     * Saves the new position for persistence.
     */
    void moved() override;

    /**
     * Called when the window is resized.
     * Saves the new bounds for persistence.
     */
    void resized() override;

    //==========================================================================
    // Plugin Access
    //==========================================================================

    /**
     * Get the plugin instance this window is hosting.
     * @return Pointer to the plugin instance
     */
    PluginInstance* getPluginInstance() const { return pluginInstance; }

    /**
     * Get the plugin's unique identifier string.
     * Used as key for position persistence.
     * @return Plugin identifier string
     */
    juce::String getPluginIdentifier() const;

    //==========================================================================
    // Editor Management
    //==========================================================================

    /**
     * Get the hosted editor component.
     * @return Pointer to the AudioProcessorEditor, or nullptr if none
     */
    juce::AudioProcessorEditor* getEditor() const { return editor.get(); }

    /**
     * Check if the window is hosting a valid editor.
     * @return true if an editor is being hosted
     */
    bool hasEditor() const { return editor != nullptr; }

    /**
     * Recreate the editor (useful if plugin editor was deleted externally).
     * @return true if editor was successfully created
     */
    bool recreateEditor();

    /**
     * Get the preferred/minimum size of the plugin editor.
     * @return Rectangle with width/height (position is 0,0)
     */
    juce::Rectangle<int> getEditorConstraints() const;

    //==========================================================================
    // Position Persistence
    //==========================================================================

    /**
     * Save the current window bounds to ApplicationProperties.
     * Called automatically on move/resize and destruction.
     */
    void saveWindowPosition();

    /**
     * Load and apply saved window position from ApplicationProperties.
     * Called automatically during construction.
     * @return true if position was restored, false if using default position
     */
    bool restoreWindowPosition();

    /**
     * Clear the saved position for this plugin.
     * Next time the window opens, it will use default positioning.
     */
    void clearSavedPosition();

    /**
     * Set the ApplicationProperties instance to use for persistence.
     * If not set, uses a default property file location.
     * @param properties The ApplicationProperties to use
     */
    static void setApplicationProperties(juce::ApplicationProperties* properties);

    /**
     * Get the property key used for saving this plugin's window bounds.
     * @return Property key string
     */
    juce::String getPositionPropertyKey() const;

    //==========================================================================
    // Project-Level Position Persistence
    //==========================================================================

    /**
     * Get the current window bounds.
     * Used for project-level serialization.
     * @return Current window bounds rectangle
     */
    juce::Rectangle<int> getCurrentBounds() const;

    /**
     * Set window bounds directly.
     * Used when restoring from project file.
     * Validates bounds and ensures window is visible on screen.
     * @param bounds The bounds to set
     */
    void setWindowBounds(const juce::Rectangle<int>& bounds);

    /**
     * Create XML element containing window position data.
     * Used by ProjectSerializer for project-level persistence.
     * @return XML element with position data, or nullptr if plugin has no identifier
     */
    std::unique_ptr<juce::XmlElement> createPositionXml() const;

    /**
     * Restore window position from XML element.
     * Used by ProjectSerializer when loading projects.
     * @param xml The XML element containing position data
     * @return true if position was restored successfully
     */
    bool restorePositionFromXml(const juce::XmlElement& xml);

    /**
     * Extract bounds from XML element without applying them.
     * Useful for pre-loading positions before windows are created.
     * @param xml The XML element containing position data
     * @return Bounds rectangle, or empty rectangle if invalid
     */
    static juce::Rectangle<int> getBoundsFromXml(const juce::XmlElement& xml);

    /**
     * Extract plugin identifier from XML element.
     * Useful for matching saved positions to plugins.
     * @param xml The XML element containing position data
     * @return Plugin identifier string, or empty if invalid
     */
    static juce::String getPluginIdFromXml(const juce::XmlElement& xml);

    //==========================================================================
    // Close Action
    //==========================================================================

    /**
     * Set the close action behavior.
     * @param action The action to take when close button is pressed
     */
    void setCloseAction(CloseAction action) { closeAction = action; }

    /**
     * Get the current close action behavior.
     * @return Current close action
     */
    CloseAction getCloseAction() const { return closeAction; }

    //==========================================================================
    // Preset Toolbar
    //==========================================================================

    /**
     * Show or hide the preset toolbar.
     * The toolbar contains preset dropdown, save/load buttons, and A/B toggle.
     * @param shouldShow true to show the toolbar, false to hide
     */
    void setPresetToolbarVisible(bool shouldShow);

    /**
     * Check if the preset toolbar is visible.
     * @return true if the toolbar is visible
     */
    bool isPresetToolbarVisible() const { return presetToolbarVisible; }

    /**
     * Refresh the preset list in the toolbar dropdown.
     * Call this when new presets are available or after saving a preset.
     */
    void refreshPresetList();

    /**
     * Set the PresetManager instance to use for preset operations.
     * @param manager The PresetManager to use
     */
    void setPresetManager(PresetManager* manager) { presetManager = manager; }

    /**
     * Get the current PresetManager.
     * @return Pointer to the PresetManager, or nullptr if not set
     */
    PresetManager* getPresetManager() const { return presetManager; }

    //==========================================================================
    // Callbacks
    //==========================================================================

    /**
     * Callback invoked when the close button is pressed (before action is taken).
     * Can be used to perform cleanup or block the close.
     * Return false to prevent the default close action.
     */
    std::function<bool(PluginWindow*)> onCloseRequested;

    /**
     * Callback invoked when the window is about to be deleted.
     */
    std::function<void(PluginWindow*)> onWindowClosed;

    /**
     * Callback invoked when the window bounds change (move or resize).
     */
    std::function<void(PluginWindow*, const juce::Rectangle<int>&)> onBoundsChanged;

    /**
     * Callback invoked when a preset is loaded from the toolbar.
     * @param presetFile The file that was loaded
     */
    std::function<void(const juce::File&)> onPresetLoaded;

    /**
     * Callback invoked when a preset is saved from the toolbar.
     * @param presetFile The file that was saved
     */
    std::function<void(const juce::File&)> onPresetSaved;

private:
    //==========================================================================
    // Preset Toolbar Component
    //==========================================================================

    /**
     * Internal toolbar component for preset management.
     */
    class PresetToolbar : public juce::Component
    {
    public:
        PresetToolbar(PluginWindow& owner);
        ~PresetToolbar() override = default;

        void resized() override;
        void paint(juce::Graphics& g) override;

        void refreshPresetList();
        void updateABState();

    private:
        PluginWindow& ownerWindow;

        juce::ComboBox presetComboBox;
        juce::TextButton saveButton { "Save" };
        juce::TextButton loadButton { "Load" };
        juce::TextButton abButton { "A/B" };
        juce::TextButton copyToBButton { "Copy to B" };
        juce::Label currentPresetLabel;

        juce::Array<juce::File> availablePresets;

        void onPresetSelected();
        void onSaveClicked();
        void onLoadClicked();
        void onABClicked();
        void onCopyToBClicked();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetToolbar)
    };

    //==========================================================================
    // Member Variables
    //==========================================================================

    // The plugin instance being hosted
    PluginInstance* pluginInstance = nullptr;

    // The editor component
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    // Behavior when close button is pressed
    CloseAction closeAction = CloseAction::Hide;

    // Whether to use generic editor as fallback
    bool useGenericEditorFallback = true;

    // Static properties instance for position persistence
    static juce::ApplicationProperties* appProperties;

    // Preset management
    PresetManager* presetManager = nullptr;
    std::unique_ptr<PresetToolbar> presetToolbar;
    bool presetToolbarVisible = false;

    //==========================================================================
    // Private Methods
    //==========================================================================

    /**
     * Initialize the window with the plugin editor.
     */
    void initializeEditor();

    /**
     * Create the appropriate editor for the plugin.
     * @return The created editor component, or nullptr on failure
     */
    std::unique_ptr<juce::AudioProcessorEditor> createPluginEditor();

    /**
     * Update the window title based on plugin state.
     */
    void updateTitle();

    /**
     * Get the default properties instance (creates if needed).
     * @return ApplicationProperties reference
     */
    static juce::ApplicationProperties& getProperties();

    /**
     * Calculate default window position for a new plugin window.
     * Centers on screen or cascades from other open windows.
     * @return Default bounds rectangle
     */
    juce::Rectangle<int> getDefaultBounds() const;

    /**
     * Update the content layout when toolbar visibility changes.
     */
    void updateContentLayout();

    /**
     * Create a content component wrapper that includes the toolbar.
     */
    void setupContentWithToolbar();

    //==========================================================================
    // Constants
    //==========================================================================

    /** Property key prefix for window bounds */
    static constexpr const char* POSITION_KEY_PREFIX = "PluginWindowBounds_";

    /** Default window width if no editor constraints */
    static constexpr int DEFAULT_WIDTH = 600;

    /** Default window height if no editor constraints */
    static constexpr int DEFAULT_HEIGHT = 400;

    /** Minimum window width */
    static constexpr int MIN_WIDTH = 200;

    /** Minimum window height */
    static constexpr int MIN_HEIGHT = 100;

    /** Height of the preset toolbar */
    static constexpr int TOOLBAR_HEIGHT = 32;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWindow)
};
