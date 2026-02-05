#include "PluginWindow.h"
#include "../Plugins/PluginInstance.h"
#include "../Presets/PresetManager.h"
#include "LookAndFeel.h"

//==============================================================================
// PresetToolbar Implementation
//==============================================================================

PluginWindow::PresetToolbar::PresetToolbar(PluginWindow& owner)
    : ownerWindow(owner)
{
    // Setup preset combo box
    presetComboBox.setTextWhenNothingSelected("Select Preset...");
    presetComboBox.setTextWhenNoChoicesAvailable("No presets available");
    presetComboBox.onChange = [this]() { onPresetSelected(); };
    addAndMakeVisible(presetComboBox);

    // Setup buttons
    saveButton.onClick = [this]() { onSaveClicked(); };
    saveButton.setTooltip("Save current settings as a preset");
    addAndMakeVisible(saveButton);

    loadButton.onClick = [this]() { onLoadClicked(); };
    loadButton.setTooltip("Load a preset from file");
    addAndMakeVisible(loadButton);

    abButton.onClick = [this]() { onABClicked(); };
    abButton.setTooltip("Toggle between A and B states");
    abButton.setClickingTogglesState(false);
    addAndMakeVisible(abButton);

    copyToBButton.onClick = [this]() { onCopyToBClicked(); };
    copyToBButton.setTooltip("Copy current settings to B slot");
    addAndMakeVisible(copyToBButton);

    // Setup label for current preset name
    currentPresetLabel.setJustificationType(juce::Justification::centredLeft);
    currentPresetLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
    addAndMakeVisible(currentPresetLabel);

    // Initial state update
    updateABState();
}

void PluginWindow::PresetToolbar::resized()
{
    auto bounds = getLocalBounds().reduced(4, 2);

    // Layout: [ComboBox][Save][Load] ... [Copy to B][A/B]
    const int buttonWidth = 60;
    const int abButtonWidth = 50;
    const int copyButtonWidth = 80;
    const int spacing = 4;

    // Preset dropdown - takes ~40% of available width
    int comboWidth = juce::jmax(150, (bounds.getWidth() - buttonWidth * 2 - abButtonWidth - copyButtonWidth - spacing * 5) / 2);
    presetComboBox.setBounds(bounds.removeFromLeft(comboWidth));
    bounds.removeFromLeft(spacing);

    // Save and Load buttons
    saveButton.setBounds(bounds.removeFromLeft(buttonWidth));
    bounds.removeFromLeft(spacing);
    loadButton.setBounds(bounds.removeFromLeft(buttonWidth));
    bounds.removeFromLeft(spacing);

    // Right side - A/B controls
    abButton.setBounds(bounds.removeFromRight(abButtonWidth));
    bounds.removeFromRight(spacing);
    copyToBButton.setBounds(bounds.removeFromRight(copyButtonWidth));
    bounds.removeFromRight(spacing);

    // Rest is for the current preset label (optional display)
    if (bounds.getWidth() > 50)
    {
        currentPresetLabel.setBounds(bounds);
        currentPresetLabel.setVisible(true);
    }
    else
    {
        currentPresetLabel.setVisible(false);
    }
}

void PluginWindow::PresetToolbar::paint(juce::Graphics& g)
{
    // Draw toolbar background
    g.setColour(MidiSingLookAndFeel::backgroundMid);
    g.fillRect(getLocalBounds());

    // Draw bottom border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
}

void PluginWindow::PresetToolbar::refreshPresetList()
{
    presetComboBox.clear(juce::dontSendNotification);
    availablePresets.clear();

    auto* pm = ownerWindow.getPresetManager();
    auto* plugin = ownerWindow.getPluginInstance();

    if (pm == nullptr || plugin == nullptr)
        return;

    // Get presets for this plugin from the default directory
    auto presetDir = pm->getDefaultPresetDirectory();
    auto presetsForPlugin = pm->getPresetsInDirectory(presetDir, true);

    int itemId = 1;
    for (const auto& presetFile : presetsForPlugin)
    {
        // Load preset to check compatibility
        auto preset = pm->loadPreset(presetFile);
        if (preset.isValid() && pm->isPresetCompatible(preset, *plugin))
        {
            availablePresets.add(presetFile);
            presetComboBox.addItem(preset.metadata.name.isNotEmpty() ?
                                   preset.metadata.name :
                                   presetFile.getFileNameWithoutExtension(),
                                   itemId++);
        }
    }

    // Add recent presets section if available
    const auto& recentPresets = pm->getRecentPresets();
    if (!recentPresets.isEmpty())
    {
        presetComboBox.addSeparator();
        presetComboBox.addSectionHeading("Recent");

        for (const auto& recentFile : recentPresets)
        {
            if (recentFile.existsAsFile())
            {
                auto preset = pm->loadPreset(recentFile);
                if (preset.isValid() && pm->isPresetCompatible(preset, *plugin))
                {
                    // Check if not already in the list
                    bool alreadyListed = false;
                    for (const auto& existing : availablePresets)
                    {
                        if (existing == recentFile)
                        {
                            alreadyListed = true;
                            break;
                        }
                    }

                    if (!alreadyListed)
                    {
                        availablePresets.add(recentFile);
                        presetComboBox.addItem(preset.metadata.name.isNotEmpty() ?
                                               preset.metadata.name :
                                               recentFile.getFileNameWithoutExtension(),
                                               itemId++);
                    }
                }
            }
        }
    }
}

void PluginWindow::PresetToolbar::updateABState()
{
    auto* plugin = ownerWindow.getPluginInstance();
    if (plugin == nullptr)
        return;

    // Update A/B button text to show current state
    auto currentSlot = plugin->getCurrentSlot();
    if (currentSlot == PluginInstance::StateSlot::A)
    {
        abButton.setButtonText("A");
        abButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::accentColour);
    }
    else
    {
        abButton.setButtonText("B");
        abButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::warningColour);
    }

    // Enable Copy to B only if we have something to copy
    copyToBButton.setEnabled(true);
}

void PluginWindow::PresetToolbar::onPresetSelected()
{
    int selectedIndex = presetComboBox.getSelectedItemIndex();
    if (selectedIndex < 0 || selectedIndex >= availablePresets.size())
        return;

    auto& presetFile = availablePresets[selectedIndex];
    auto* pm = ownerWindow.getPresetManager();
    auto* plugin = ownerWindow.getPluginInstance();

    if (pm == nullptr || plugin == nullptr)
        return;

    // Load and apply the preset
    if (pm->loadAndApplyPreset(presetFile, *plugin))
    {
        pm->addToRecentPresets(presetFile);

        // Update current preset label
        auto preset = pm->loadPreset(presetFile);
        currentPresetLabel.setText(preset.metadata.name.isNotEmpty() ?
                                   preset.metadata.name :
                                   presetFile.getFileNameWithoutExtension(),
                                   juce::dontSendNotification);

        // Notify callback
        if (ownerWindow.onPresetLoaded)
            ownerWindow.onPresetLoaded(presetFile);
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Preset Load Failed",
            "Could not load the selected preset.");
    }
}

void PluginWindow::PresetToolbar::onSaveClicked()
{
    auto* pm = ownerWindow.getPresetManager();
    auto* plugin = ownerWindow.getPluginInstance();

    if (pm == nullptr || plugin == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Save Failed",
            "No preset manager available.");
        return;
    }

    // Show file save dialog
    auto presetDir = pm->getDefaultPresetDirectory();

    // Create subdirectory for this plugin
    auto pluginPresetDir = presetDir.getChildFile(plugin->getName().replaceCharacters("/<>:\"|?*\\", "_________"));
    pluginPresetDir.createDirectory();

    auto fileChooser = std::make_shared<juce::FileChooser>(
        "Save Preset",
        pluginPresetDir,
        juce::String("*.") + PresetManager::PRESET_EXTENSION);

    fileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, pm, plugin, fileChooser](const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file == juce::File())
                return;

            // Ensure proper extension
            if (!file.hasFileExtension(PresetManager::PRESET_EXTENSION))
                file = file.withFileExtension(PresetManager::PRESET_EXTENSION);

            // Create metadata
            PresetManager::PresetMetadata metadata;
            metadata.name = file.getFileNameWithoutExtension();
            metadata.author = juce::SystemStats::getFullUserName();

            // Save the preset
            if (pm->savePluginPreset(*plugin, metadata, file))
            {
                pm->addToRecentPresets(file);
                refreshPresetList();

                currentPresetLabel.setText(metadata.name, juce::dontSendNotification);

                // Notify callback
                if (ownerWindow.onPresetSaved)
                    ownerWindow.onPresetSaved(file);
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Save Failed",
                    "Could not save the preset to " + file.getFullPathName());
            }
        });
}

void PluginWindow::PresetToolbar::onLoadClicked()
{
    auto* pm = ownerWindow.getPresetManager();
    auto* plugin = ownerWindow.getPluginInstance();

    if (pm == nullptr || plugin == nullptr)
        return;

    auto presetDir = pm->getDefaultPresetDirectory();

    // Build wildcard for supported formats
    juce::String wildcards = juce::String("*.") + PresetManager::PRESET_EXTENSION;
    wildcards += ";*.fxp;*.fxb";  // Also support FXP/FXB import

    auto fileChooser = std::make_shared<juce::FileChooser>(
        "Load Preset",
        presetDir,
        wildcards);

    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, pm, plugin, fileChooser](const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file == juce::File())
                return;

            bool success = false;
            juce::String presetName;

            if (file.hasFileExtension(PresetManager::PRESET_EXTENSION))
            {
                // Load native preset format
                success = pm->loadAndApplyPreset(file, *plugin);
                if (success)
                {
                    auto preset = pm->loadPreset(file);
                    presetName = preset.metadata.name.isNotEmpty() ?
                                 preset.metadata.name :
                                 file.getFileNameWithoutExtension();
                }
            }
            else if (file.hasFileExtension("fxp") || file.hasFileExtension("fxb"))
            {
                // Import FXP/FXB format
                juce::String errorMessage;
                success = pm->importFxpFxb(file, *plugin, errorMessage);
                if (success)
                {
                    presetName = PresetManager::getPresetNameFromFxp(file);
                    if (presetName.isEmpty())
                        presetName = file.getFileNameWithoutExtension();
                }
                else if (errorMessage.isNotEmpty())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Import Failed",
                        errorMessage);
                    return;
                }
            }

            if (success)
            {
                pm->addToRecentPresets(file);
                refreshPresetList();
                currentPresetLabel.setText(presetName, juce::dontSendNotification);

                if (ownerWindow.onPresetLoaded)
                    ownerWindow.onPresetLoaded(file);
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Load Failed",
                    "Could not load the preset from " + file.getFullPathName());
            }
        });
}

void PluginWindow::PresetToolbar::onABClicked()
{
    auto* plugin = ownerWindow.getPluginInstance();
    if (plugin == nullptr)
        return;

    // Swap between A and B states
    plugin->swapAB();
    updateABState();
}

void PluginWindow::PresetToolbar::onCopyToBClicked()
{
    auto* plugin = ownerWindow.getPluginInstance();
    if (plugin == nullptr)
        return;

    // Copy current state to B slot
    plugin->copyToB();
    updateABState();

    // Show brief confirmation
    currentPresetLabel.setText("Copied to B", juce::dontSendNotification);
}

//==============================================================================
// Static member initialization
//==============================================================================

juce::ApplicationProperties* PluginWindow::appProperties = nullptr;

//==============================================================================
// Construction/Destruction
//==============================================================================

PluginWindow::PluginWindow(PluginInstance* plugin,
                           CloseAction action,
                           bool useGenericIfNoEditor)
    : PluginWindow(plugin, juce::String(), action, useGenericIfNoEditor)
{
}

PluginWindow::PluginWindow(PluginInstance* plugin,
                           const juce::String& title,
                           CloseAction action,
                           bool useGenericIfNoEditor)
    : DocumentWindow(title.isEmpty() && plugin != nullptr ? plugin->getName() : title,
                     juce::Desktop::getInstance().getDefaultLookAndFeel()
                         .findColour(juce::ResizableWindow::backgroundColourId),
                     DocumentWindow::closeButton | DocumentWindow::minimiseButton)
    , pluginInstance(plugin)
    , closeAction(action)
    , useGenericEditorFallback(useGenericIfNoEditor)
{
    jassert(pluginInstance != nullptr);

    setUsingNativeTitleBar(true);
    setResizable(true, false);

    // Initialize the editor
    initializeEditor();

    // Try to restore saved window position
    if (!restoreWindowPosition())
    {
        // Use default bounds if no saved position
        auto bounds = getDefaultBounds();
        setBounds(bounds);
    }

    // Ensure window is visible on a valid display
    auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getBounds());
    if (display == nullptr)
    {
        // Window is off-screen, center on primary display
        centreWithSize(getWidth(), getHeight());
    }
}

PluginWindow::~PluginWindow()
{
    // Save position before destroying
    saveWindowPosition();

    // Notify listener if set
    if (onWindowClosed)
        onWindowClosed(this);

    // Clear the content before destruction
    clearContentComponent();
    editor.reset();
}

//==============================================================================
// DocumentWindow Overrides
//==============================================================================

void PluginWindow::closeButtonPressed()
{
    // Check if close is allowed via callback
    if (onCloseRequested)
    {
        if (!onCloseRequested(this))
            return; // Close blocked by callback
    }

    // Save position
    saveWindowPosition();

    switch (closeAction)
    {
        case CloseAction::Hide:
            setVisible(false);
            break;

        case CloseAction::DeleteWindow:
            // The owner should delete this window
            // We set visibility to false and let the owner handle deletion
            setVisible(false);

            // Use async deletion to avoid issues if called from within a callback
            juce::MessageManager::callAsync([this]()
            {
                // Note: The owner (PluginManager) should be tracking this window
                // and will delete it when it detects it's no longer visible
                // or through the onWindowClosed callback
            });
            break;
    }
}

void PluginWindow::moved()
{
    DocumentWindow::moved();

    // Notify listener
    if (onBoundsChanged)
        onBoundsChanged(this, getBounds());
}

void PluginWindow::resized()
{
    DocumentWindow::resized();

    // Notify listener
    if (onBoundsChanged)
        onBoundsChanged(this, getBounds());
}

//==============================================================================
// Plugin Access
//==============================================================================

juce::String PluginWindow::getPluginIdentifier() const
{
    if (pluginInstance != nullptr)
        return pluginInstance->getIdentifierString();

    return {};
}

//==============================================================================
// Editor Management
//==============================================================================

bool PluginWindow::recreateEditor()
{
    // Clear existing editor
    clearContentComponent();
    editor.reset();

    // Create new editor
    editor = createPluginEditor();

    if (editor != nullptr)
    {
        // Get editor constraints
        auto constraints = getEditorConstraints();
        int width = juce::jmax(constraints.getWidth(), MIN_WIDTH);
        int height = juce::jmax(constraints.getHeight(), MIN_HEIGHT);

        // Setup with toolbar if visible
        if (presetToolbarVisible && presetToolbar != nullptr)
        {
            setupContentWithToolbar();
            height += TOOLBAR_HEIGHT;
        }
        else
        {
            setContentNonOwned(editor.get(), true);
        }

        setResizeLimits(MIN_WIDTH, MIN_HEIGHT, 4096, 4096);

        // Only resize if necessary (with native title bar, getTitleBarHeight() returns 0)
        if (getWidth() < width || getHeight() < height)
        {
            setSize(width, height);
        }

        updateTitle();

        // Refresh preset list if toolbar exists
        if (presetToolbar != nullptr)
            presetToolbar->refreshPresetList();

        return true;
    }

    return false;
}

juce::Rectangle<int> PluginWindow::getEditorConstraints() const
{
    if (editor != nullptr)
    {
        auto* constrainer = editor->getConstrainer();
        if (constrainer != nullptr)
        {
            int minWidth = constrainer->getMinimumWidth();
            int minHeight = constrainer->getMinimumHeight();

            if (minWidth > 0 && minHeight > 0)
                return { 0, 0, minWidth, minHeight };
        }

        // Use editor's current size
        return { 0, 0, editor->getWidth(), editor->getHeight() };
    }

    return { 0, 0, DEFAULT_WIDTH, DEFAULT_HEIGHT };
}

//==============================================================================
// Position Persistence
//==============================================================================

void PluginWindow::saveWindowPosition()
{
    auto key = getPositionPropertyKey();
    if (key.isEmpty())
        return;

    auto& props = getProperties();
    if (auto* userSettings = props.getUserSettings())
    {
        auto bounds = getBounds();
        juce::String boundsStr = juce::String(bounds.getX()) + "," +
                                  juce::String(bounds.getY()) + "," +
                                  juce::String(bounds.getWidth()) + "," +
                                  juce::String(bounds.getHeight());

        userSettings->setValue(key, boundsStr);
        userSettings->saveIfNeeded();
    }
}

juce::Rectangle<int> PluginWindow::getCurrentBounds() const
{
    return getBounds();
}

void PluginWindow::setWindowBounds(const juce::Rectangle<int>& bounds)
{
    // Validate bounds before applying
    if (bounds.getWidth() >= MIN_WIDTH && bounds.getHeight() >= MIN_HEIGHT)
    {
        setBounds(bounds);

        // Ensure window is visible on a valid display
        auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getBounds());
        if (display == nullptr)
        {
            // Window is off-screen, center on primary display
            centreWithSize(getWidth(), getHeight());
        }
    }
}

std::unique_ptr<juce::XmlElement> PluginWindow::createPositionXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("PLUGIN_WINDOW");

    auto identifier = getPluginIdentifier();
    if (identifier.isEmpty())
        return nullptr;

    xml->setAttribute("pluginId", identifier);

    auto bounds = getBounds();
    xml->setAttribute("x", bounds.getX());
    xml->setAttribute("y", bounds.getY());
    xml->setAttribute("width", bounds.getWidth());
    xml->setAttribute("height", bounds.getHeight());
    xml->setAttribute("visible", isVisible());

    return xml;
}

bool PluginWindow::restorePositionFromXml(const juce::XmlElement& xml)
{
    if (!xml.hasTagName("PLUGIN_WINDOW"))
        return false;

    // Verify this XML is for the correct plugin
    juce::String xmlPluginId = xml.getStringAttribute("pluginId");
    auto currentPluginId = getPluginIdentifier();

    if (xmlPluginId.isEmpty() || xmlPluginId != currentPluginId)
        return false;

    // Restore bounds
    int x = xml.getIntAttribute("x", 100);
    int y = xml.getIntAttribute("y", 100);
    int width = xml.getIntAttribute("width", DEFAULT_WIDTH);
    int height = xml.getIntAttribute("height", DEFAULT_HEIGHT);

    // Validate bounds
    if (width >= MIN_WIDTH && height >= MIN_HEIGHT)
    {
        setBounds(x, y, width, height);

        // Ensure window is visible on a valid display
        auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getBounds());
        if (display == nullptr)
        {
            // Window is off-screen, center on primary display
            centreWithSize(getWidth(), getHeight());
        }

        return true;
    }

    return false;
}

juce::Rectangle<int> PluginWindow::getBoundsFromXml(const juce::XmlElement& xml)
{
    if (!xml.hasTagName("PLUGIN_WINDOW"))
        return {};

    int x = xml.getIntAttribute("x", 100);
    int y = xml.getIntAttribute("y", 100);
    int width = xml.getIntAttribute("width", DEFAULT_WIDTH);
    int height = xml.getIntAttribute("height", DEFAULT_HEIGHT);

    return { x, y, width, height };
}

juce::String PluginWindow::getPluginIdFromXml(const juce::XmlElement& xml)
{
    if (!xml.hasTagName("PLUGIN_WINDOW"))
        return {};

    return xml.getStringAttribute("pluginId");
}

bool PluginWindow::restoreWindowPosition()
{
    auto key = getPositionPropertyKey();
    if (key.isEmpty())
        return false;

    auto& props = getProperties();
    if (auto* userSettings = props.getUserSettings())
    {
        juce::String boundsStr = userSettings->getValue(key);

        if (boundsStr.isNotEmpty())
        {
            auto tokens = juce::StringArray::fromTokens(boundsStr, ",", "");

            if (tokens.size() == 4)
            {
                int x = tokens[0].getIntValue();
                int y = tokens[1].getIntValue();
                int w = tokens[2].getIntValue();
                int h = tokens[3].getIntValue();

                // Validate bounds
                if (w >= MIN_WIDTH && h >= MIN_HEIGHT)
                {
                    setBounds(x, y, w, h);
                    return true;
                }
            }
        }
    }

    return false;
}

void PluginWindow::clearSavedPosition()
{
    auto key = getPositionPropertyKey();
    if (key.isEmpty())
        return;

    auto& props = getProperties();
    if (auto* userSettings = props.getUserSettings())
    {
        userSettings->removeValue(key);
        userSettings->saveIfNeeded();
    }
}

void PluginWindow::setApplicationProperties(juce::ApplicationProperties* properties)
{
    appProperties = properties;
}

juce::String PluginWindow::getPositionPropertyKey() const
{
    auto identifier = getPluginIdentifier();
    if (identifier.isEmpty())
        return {};

    // Create a valid property key from the plugin identifier
    // Replace characters that might be problematic in property keys
    auto safeKey = identifier.replaceCharacters("/<>:\"|?*\\", "_________");
    return juce::String(POSITION_KEY_PREFIX) + safeKey;
}

//==============================================================================
// Private Methods
//==============================================================================

void PluginWindow::initializeEditor()
{
    editor = createPluginEditor();

    if (editor != nullptr)
    {
        // Apply editor constraints
        auto constraints = getEditorConstraints();
        int width = juce::jmax(constraints.getWidth(), MIN_WIDTH);
        int height = juce::jmax(constraints.getHeight(), MIN_HEIGHT);

        setResizeLimits(MIN_WIDTH, MIN_HEIGHT, 4096, 4096);

        // Setup with preset toolbar visible by default
        presetToolbarVisible = true;
        presetToolbar = std::make_unique<PresetToolbar>(*this);
        setupContentWithToolbar();

        // Initial size based on editor + toolbar
        setSize(width, height + TOOLBAR_HEIGHT);
    }
    else
    {
        // No editor available - show placeholder
        setSize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    }

    updateTitle();
}

std::unique_ptr<juce::AudioProcessorEditor> PluginWindow::createPluginEditor()
{
    if (pluginInstance == nullptr)
        return nullptr;

    auto* processor = pluginInstance->getProcessor();
    if (processor == nullptr)
        return nullptr;

    // Try to create the plugin's custom editor
    // Note: We use createEditor() instead of createEditorIfNeeded() because
    // createEditorIfNeeded() caches the editor pointer internally and expects
    // the processor to manage editor lifetime. We manage it ourselves via unique_ptr.
    if (processor->hasEditor())
    {
        auto* customEditor = processor->createEditor();
        if (customEditor != nullptr)
            return std::unique_ptr<juce::AudioProcessorEditor>(customEditor);
    }

    // Fall back to generic editor if allowed
    if (useGenericEditorFallback)
    {
        return std::make_unique<juce::GenericAudioProcessorEditor>(*processor);
    }

    return nullptr;
}

void PluginWindow::updateTitle()
{
    if (pluginInstance != nullptr)
    {
        juce::String title = pluginInstance->getName();

        // Add manufacturer if available
        auto manufacturer = pluginInstance->getManufacturer();
        if (manufacturer.isNotEmpty())
            title += " - " + manufacturer;

        // Indicate if bypassed
        if (pluginInstance->isBypassed())
            title += " [Bypassed]";

        setName(title);
    }
}

juce::ApplicationProperties& PluginWindow::getProperties()
{
    // Use provided properties if available
    if (appProperties != nullptr)
        return *appProperties;

    // Create default properties instance
    static std::unique_ptr<juce::ApplicationProperties> defaultProperties;

    if (defaultProperties == nullptr)
    {
        defaultProperties = std::make_unique<juce::ApplicationProperties>();

        juce::PropertiesFile::Options options;
        options.applicationName = "MidiSing";
        options.folderName = "MidiSing";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";

        defaultProperties->setStorageParameters(options);
    }

    return *defaultProperties;
}

juce::Rectangle<int> PluginWindow::getDefaultBounds() const
{
    // Get editor size (with native title bar, content size = window size)
    auto constraints = getEditorConstraints();
    int width = juce::jmax(constraints.getWidth(), DEFAULT_WIDTH);
    int height = juce::jmax(constraints.getHeight(), DEFAULT_HEIGHT);

    // Add toolbar height if visible
    if (presetToolbarVisible)
        height += TOOLBAR_HEIGHT;

    // Center on screen
    auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    if (display != nullptr)
    {
        auto area = display->userArea;
        int x = area.getCentreX() - width / 2;
        int y = area.getCentreY() - height / 2;
        return { x, y, width, height };
    }

    return { 100, 100, width, height };
}

//==============================================================================
// Preset Toolbar
//==============================================================================

void PluginWindow::setPresetToolbarVisible(bool shouldShow)
{
    if (presetToolbarVisible == shouldShow)
        return;

    presetToolbarVisible = shouldShow;

    if (shouldShow)
    {
        // Create toolbar if needed
        if (presetToolbar == nullptr)
        {
            presetToolbar = std::make_unique<PresetToolbar>(*this);
            presetToolbar->refreshPresetList();
        }

        setupContentWithToolbar();
    }
    else
    {
        // Just show the editor directly
        if (editor != nullptr)
        {
            clearContentComponent();
            setContentNonOwned(editor.get(), true);
        }

        presetToolbar.reset();
    }

    updateContentLayout();
}

void PluginWindow::refreshPresetList()
{
    if (presetToolbar != nullptr)
        presetToolbar->refreshPresetList();
}

void PluginWindow::updateContentLayout()
{
    if (editor == nullptr)
        return;

    // Calculate required size
    auto editorConstraints = getEditorConstraints();
    int editorWidth = juce::jmax(editorConstraints.getWidth(), MIN_WIDTH);
    int editorHeight = juce::jmax(editorConstraints.getHeight(), MIN_HEIGHT);

    int totalHeight = editorHeight;
    if (presetToolbarVisible && presetToolbar != nullptr)
        totalHeight += TOOLBAR_HEIGHT;

    // Update window size if needed
    int currentWidth = getWidth();
    int currentHeight = getHeight();

    if (currentWidth < editorWidth)
        currentWidth = editorWidth;
    if (currentHeight < totalHeight)
        currentHeight = totalHeight;

    if (currentWidth != getWidth() || currentHeight != getHeight())
        setSize(currentWidth, currentHeight);
}

void PluginWindow::setupContentWithToolbar()
{
    if (editor == nullptr)
        return;

    // Create a wrapper component that contains both toolbar and editor
    class ContentWrapper : public juce::Component
    {
    public:
        ContentWrapper(PluginWindow& owner) : ownerWindow(owner) {}

        void resized() override
        {
            auto bounds = getLocalBounds();

            // Place toolbar at top
            if (ownerWindow.presetToolbar != nullptr && ownerWindow.presetToolbarVisible)
            {
                ownerWindow.presetToolbar->setBounds(bounds.removeFromTop(PluginWindow::TOOLBAR_HEIGHT));
            }

            // Editor takes rest
            if (ownerWindow.editor != nullptr)
            {
                ownerWindow.editor->setBounds(bounds);
            }
        }

        void childBoundsChanged(juce::Component*) override
        {
            resized();
        }

    private:
        PluginWindow& ownerWindow;
    };

    // Remove current content
    clearContentComponent();

    // Create wrapper
    auto* wrapper = new ContentWrapper(*this);
    wrapper->addAndMakeVisible(presetToolbar.get());
    wrapper->addAndMakeVisible(editor.get());

    // Calculate size
    auto editorConstraints = getEditorConstraints();
    int width = juce::jmax(editorConstraints.getWidth(), MIN_WIDTH);
    int height = juce::jmax(editorConstraints.getHeight(), MIN_HEIGHT) + TOOLBAR_HEIGHT;

    wrapper->setSize(width, height);

    // Set as content (window owns it now)
    setContentOwned(wrapper, true);
}
