#pragma once

#include "LookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>

/**
 * BrowserPanel provides a file browser for audio and MIDI files.
 * Allows browsing and previewing samples, presets, and project files.
 */
class BrowserPanel : public juce::Component,
                     public juce::FileBrowserListener
{
public:
    BrowserPanel()
    {
        // Setup file filter for audio and MIDI files
        fileFilter = std::make_unique<juce::WildcardFileFilter>(
            "*.wav;*.mp3;*.aif;*.aiff;*.mid;*.midi;*.midising",
            "*",
            "Audio & MIDI Files");

        // Create file browser with tree view
        fileBrowser = std::make_unique<juce::FileBrowserComponent>(
            juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::canSelectDirectories,
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            fileFilter.get(),
            nullptr);

        fileBrowser->addListener(this);
        addAndMakeVisible(fileBrowser.get());

        // Preview label
        previewLabel.setText("Select a file to preview", juce::dontSendNotification);
        previewLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
        previewLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(previewLabel);
    }

    ~BrowserPanel() override
    {
        if (fileBrowser != nullptr)
            fileBrowser->removeListener(this);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(MidiSingLookAndFeel::backgroundDark);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Preview area at bottom
        auto previewArea = bounds.removeFromBottom(30);
        previewLabel.setBounds(previewArea.reduced(4));

        // File browser fills the rest
        fileBrowser->setBounds(bounds);
    }

    // FileBrowserListener implementation
    void selectionChanged() override
    {
        auto selectedFile = fileBrowser->getSelectedFile(0);
        if (selectedFile.existsAsFile())
        {
            previewLabel.setText(selectedFile.getFileName(), juce::dontSendNotification);
            previewLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
        }
        else if (selectedFile.isDirectory())
        {
            previewLabel.setText(selectedFile.getFileName(), juce::dontSendNotification);
            previewLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
        }
        else
        {
            previewLabel.setText("Select a file to preview", juce::dontSendNotification);
            previewLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
        }
    }

    void fileClicked(const juce::File& file, const juce::MouseEvent& e) override
    {
        juce::ignoreUnused(e);
        if (file.existsAsFile() && onFileClicked)
        {
            onFileClicked(file);
        }
    }

    void fileDoubleClicked(const juce::File& file) override
    {
        if (file.existsAsFile() && onFileDoubleClicked)
        {
            onFileDoubleClicked(file);
        }
    }

    void browserRootChanged(const juce::File& newRoot) override
    {
        juce::ignoreUnused(newRoot);
    }

    // Callbacks for file interactions
    std::function<void(const juce::File&)> onFileClicked;
    std::function<void(const juce::File&)> onFileDoubleClicked;

    // Set the browser root directory
    void setRootDirectory(const juce::File& dir)
    {
        if (dir.isDirectory())
            fileBrowser->setRoot(dir);
    }

    juce::File getCurrentDirectory() const
    {
        return fileBrowser->getRoot();
    }

private:
    std::unique_ptr<juce::WildcardFileFilter> fileFilter;
    std::unique_ptr<juce::FileBrowserComponent> fileBrowser;
    juce::Label previewLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowserPanel)
};
