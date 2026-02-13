#include "SettingsPanel.h"
#include "../Utils/AppSettings.h"
#include "../Plugins/PluginManager.h"

//==============================================================================
// Simple component with resized callback
class LayoutPanel : public juce::Component
{
public:
    std::function<void()> onResized;
    void resized() override { if (onResized) onResized(); }
};

//==============================================================================
// Simple list model for plugin paths
class PluginPathsListModel : public juce::ListBoxModel
{
public:
    PluginPathsListModel(juce::StringArray& paths) : pathsRef(paths) {}

    int getNumRows() override { return pathsRef.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll(juce::Colours::white.withAlpha(0.15f));

        g.setColour(MidiSingLookAndFeel::textColour);
        g.drawText(pathsRef[rowNumber], 5, 0, width - 10, height, juce::Justification::centredLeft);
    }

private:
    juce::StringArray& pathsRef;
};

//==============================================================================

SettingsPanel::SettingsPanel(juce::AudioDeviceManager& deviceManager,
                             juce::MidiInputCallback* midiCallback)
    : audioDeviceManager(deviceManager),
      midiInputCallback(midiCallback)
{
    // Create device selector with input and output enabled
    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
        audioDeviceManager,
        0, 2,       // min/max input channels
        0, 2,       // min/max output channels
        true,       // show MIDI inputs
        true,       // show MIDI outputs
        true,       // treat channels as stereo pairs
        true        // show advanced options (buffer size, sample rate)
    );

    audioDeviceManager.addChangeListener(this);

    // Initialize MIDI callbacks for any already-enabled devices
    syncMidiInputCallbacks();

    // Setup tabs
    tabs = std::make_unique<juce::TabbedComponent>(juce::TabbedButtonBar::TabsAtTop);
    tabs->setTabBarDepth(30);
    tabs->setOutline(0);

    // Audio/MIDI tab
    tabs->addTab("Audio / MIDI", MidiSingLookAndFeel::backgroundDark, deviceSelector.get(), false);

    // General settings tab
    setupGeneralTab();

    // Plugin paths tab
    setupPluginPathsTab();

    addAndMakeVisible(tabs.get());

    setSize(600, 500);
}

SettingsPanel::~SettingsPanel()
{
    audioDeviceManager.removeChangeListener(this);
}

void SettingsPanel::setupGeneralTab()
{
    auto layoutPanel = std::make_unique<LayoutPanel>();

    auto& settings = AppSettings::getInstance();

    // Recording path
    auto* recLabel = new juce::Label({}, "Recording Folder:");
    recLabel->setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    layoutPanel->addAndMakeVisible(recLabel);

    auto* recPathLabel = new juce::Label({}, settings.getRecordingPath());
    recPathLabel->setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour.darker());
    recPathLabel->setColour(juce::Label::backgroundColourId, MidiSingLookAndFeel::backgroundDark.brighter(0.05f));
    layoutPanel->addAndMakeVisible(recPathLabel);

    auto* recBrowseBtn = new juce::TextButton("Browse...");
    recBrowseBtn->onClick = [recPathLabel]()
    {
        auto chooser = std::make_shared<juce::FileChooser>("Select Recording Folder");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [recPathLabel, chooser](const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder != juce::File() && folder.isDirectory())
                {
                    AppSettings::getInstance().setRecordingPath(folder.getFullPathName());
                    recPathLabel->setText(folder.getFullPathName(), juce::dontSendNotification);
                }
            });
    };
    layoutPanel->addAndMakeVisible(recBrowseBtn);

    // Auto-save interval
    auto* autoSaveLabel = new juce::Label({}, "Auto-save interval (minutes):");
    autoSaveLabel->setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    layoutPanel->addAndMakeVisible(autoSaveLabel);

    auto* autoSaveSlider = new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    autoSaveSlider->setRange(1, 30, 1);
    autoSaveSlider->setValue(settings.getAutoSaveIntervalMinutes());
    autoSaveSlider->onValueChange = [autoSaveSlider]()
    {
        AppSettings::getInstance().setAutoSaveIntervalMinutes(static_cast<int>(autoSaveSlider->getValue()));
    };
    layoutPanel->addAndMakeVisible(autoSaveSlider);

    // Metronome pre-count bars
    auto* preCountLabel = new juce::Label({}, "Metronome pre-count bars:");
    preCountLabel->setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    layoutPanel->addAndMakeVisible(preCountLabel);

    auto* preCountSlider = new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    preCountSlider->setRange(0, 4, 1);
    preCountSlider->setValue(settings.getMetronomePreCountBars());
    preCountSlider->onValueChange = [preCountSlider]()
    {
        AppSettings::getInstance().setMetronomePreCountBars(static_cast<int>(preCountSlider->getValue()));
    };
    layoutPanel->addAndMakeVisible(preCountSlider);

    // Layout using resized callback
    auto* panelPtr = layoutPanel.get();
    layoutPanel->onResized = [panelPtr, recLabel, recBrowseBtn, recPathLabel, autoSaveLabel, autoSaveSlider, preCountLabel, preCountSlider]()
    {
        auto bounds = panelPtr->getLocalBounds().reduced(15);
        int rowH = 30;
        int gap = 10;

        auto row1 = bounds.removeFromTop(rowH); bounds.removeFromTop(gap);
        recLabel->setBounds(row1.removeFromLeft(140));
        recBrowseBtn->setBounds(row1.removeFromRight(80));
        recPathLabel->setBounds(row1.reduced(5, 0));

        auto row2 = bounds.removeFromTop(rowH); bounds.removeFromTop(gap);
        autoSaveLabel->setBounds(row2.removeFromLeft(220));
        autoSaveSlider->setBounds(row2);

        auto row3 = bounds.removeFromTop(rowH); bounds.removeFromTop(gap);
        preCountLabel->setBounds(row3.removeFromLeft(220));
        preCountSlider->setBounds(row3);
    };

    generalPanel = std::move(layoutPanel);
    tabs->addTab("General", MidiSingLookAndFeel::backgroundDark, generalPanel.get(), false);
}

void SettingsPanel::setupPluginPathsTab()
{
    pluginPathsPanel = std::make_unique<juce::Component>();
    pluginPathsArray = std::make_unique<juce::StringArray>();

    // Load paths from AppSettings
    *pluginPathsArray = AppSettings::getInstance().getPluginSearchPaths();

    // If no custom paths, add defaults
    if (pluginPathsArray->isEmpty())
    {
        auto defaults = PluginManager::getDefaultPluginPaths();
        for (const auto& path : defaults)
            pluginPathsArray->add(path);
    }

    pluginPathsListBox = std::make_unique<juce::ListBox>();
    pluginPathsListBox->setModel(new PluginPathsListModel(*pluginPathsArray));
    pluginPathsListBox->setColour(juce::ListBox::backgroundColourId, MidiSingLookAndFeel::backgroundDark.brighter(0.05f));
    pluginPathsPanel->addAndMakeVisible(pluginPathsListBox.get());

    addPathButton = std::make_unique<juce::TextButton>("Add Folder...");
    addPathButton->onClick = [this]()
    {
        auto chooser = std::make_shared<juce::FileChooser>("Select Plugin Folder");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder != juce::File() && folder.isDirectory())
                {
                    auto path = folder.getFullPathName();
                    if (!pluginPathsArray->contains(path))
                    {
                        pluginPathsArray->add(path);
                        AppSettings::getInstance().setPluginSearchPaths(*pluginPathsArray);
                        refreshPluginPathList();
                    }
                }
            });
    };
    pluginPathsPanel->addAndMakeVisible(addPathButton.get());

    removePathButton = std::make_unique<juce::TextButton>("Remove");
    removePathButton->onClick = [this]()
    {
        int selected = pluginPathsListBox->getSelectedRow();
        if (selected >= 0 && selected < pluginPathsArray->size())
        {
            pluginPathsArray->remove(selected);
            AppSettings::getInstance().setPluginSearchPaths(*pluginPathsArray);
            refreshPluginPathList();
        }
    };
    pluginPathsPanel->addAndMakeVisible(removePathButton.get());

    rescanButton = std::make_unique<juce::TextButton>("Rescan Plugins");
    rescanButton->onClick = [this]()
    {
        AppSettings::getInstance().setPluginSearchPaths(*pluginPathsArray);
        PluginManager::getInstance().startScan(*pluginPathsArray);
    };
    pluginPathsPanel->addAndMakeVisible(rescanButton.get());

    // Layout the plugin paths panel
    pluginPathsPanel->setSize(600, 400);

    tabs->addTab("Plugins", MidiSingLookAndFeel::backgroundDark, pluginPathsPanel.get(), false);
}

void SettingsPanel::refreshPluginPathList()
{
    if (pluginPathsListBox != nullptr)
    {
        // Recreate model with updated paths
        pluginPathsListBox->setModel(new PluginPathsListModel(*pluginPathsArray));
        pluginPathsListBox->updateContent();
        pluginPathsListBox->repaint();
    }
}

void SettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark);
}

void SettingsPanel::resized()
{
    tabs->setBounds(getLocalBounds().reduced(5));

    // Layout plugin paths panel
    if (pluginPathsPanel != nullptr)
    {
        auto bounds = pluginPathsPanel->getLocalBounds().reduced(10);
        auto buttonRow = bounds.removeFromBottom(35);

        if (pluginPathsListBox != nullptr)
            pluginPathsListBox->setBounds(bounds.reduced(0, 5));

        if (addPathButton != nullptr)
            addPathButton->setBounds(buttonRow.removeFromLeft(100).reduced(2));
        if (removePathButton != nullptr)
            removePathButton->setBounds(buttonRow.removeFromLeft(80).reduced(2));

        buttonRow.removeFromLeft(20); // spacer
        if (rescanButton != nullptr)
            rescanButton->setBounds(buttonRow.removeFromLeft(120).reduced(2));
    }
}

void SettingsPanel::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused(source);
    syncMidiInputCallbacks();
    saveDeviceSettings();
}

juce::File SettingsPanel::getSettingsFile() const
{
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appDataDir.getChildFile("MidiSing").getChildFile("audio_device_settings.xml");
}

void SettingsPanel::saveDeviceSettings()
{
    auto settingsFile = getSettingsFile();
    settingsFile.getParentDirectory().createDirectory();

    auto xml = audioDeviceManager.createStateXml();
    if (xml != nullptr)
    {
        settingsFile.replaceWithText(xml->toString());
    }
}

void SettingsPanel::loadDeviceSettings()
{
    auto settingsFile = getSettingsFile();
    if (!settingsFile.existsAsFile())
        return;

    auto xml = juce::XmlDocument::parse(settingsFile);
    if (xml != nullptr)
    {
        audioDeviceManager.initialise(2, 2, xml.get(), true);
    }
}

void SettingsPanel::syncMidiInputCallbacks()
{
    if (midiInputCallback == nullptr)
        return;

    auto midiInputs = juce::MidiInput::getAvailableDevices();

    for (const auto& device : midiInputs)
    {
        bool isEnabled = audioDeviceManager.isMidiInputDeviceEnabled(device.identifier);

        audioDeviceManager.removeMidiInputDeviceCallback(device.identifier, midiInputCallback);

        if (isEnabled)
        {
            audioDeviceManager.addMidiInputDeviceCallback(device.identifier, midiInputCallback);
        }
    }
}
