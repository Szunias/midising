#include "FileBrowser.h"

//==============================================================================
// FileBrowserItem Implementation
//==============================================================================

FileBrowserItem::FileBrowserItem(const juce::File& f, bool isDirectory)
    : file(f), isDir(isDirectory)
{
}

void FileBrowserItem::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    if (isSelected)
    {
        g.setColour(MidiSingLookAndFeel::accentColour.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds, 3.0f);
    }
    else if (isHovered)
    {
        g.setColour(MidiSingLookAndFeel::backgroundLight);
        g.fillRoundedRectangle(bounds, 3.0f);
    }

    // Icon
    auto iconBounds = bounds.removeFromLeft(24.0f).reduced(4.0f);
    g.setColour(isDir ? MidiSingLookAndFeel::accentColour : MidiSingLookAndFeel::textColour);

    if (isDir)
    {
        // Draw folder icon
        juce::Path folderPath;
        auto ib = iconBounds.reduced(2.0f);
        folderPath.addRoundedRectangle(ib.getX(), ib.getY() + 4.0f,
                                        ib.getWidth(), ib.getHeight() - 4.0f, 2.0f);
        folderPath.addRoundedRectangle(ib.getX(), ib.getY(),
                                        ib.getWidth() * 0.4f, 4.0f, 1.0f);
        g.fillPath(folderPath);
    }
    else
    {
        // Draw file icon based on type
        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".aif" || ext == ".aiff" || ext == ".flac")
        {
            g.setColour(MidiSingLookAndFeel::waveformColour);
        }
        else if (ext == ".mid" || ext == ".midi")
        {
            g.setColour(MidiSingLookAndFeel::midiNoteColour);
        }

        // Draw simple file icon
        juce::Path filePath;
        auto fb = iconBounds.reduced(2.0f);
        filePath.addRoundedRectangle(fb.getX(), fb.getY(), fb.getWidth(), fb.getHeight(), 2.0f);
        g.fillPath(filePath);
    }

    // File name
    bounds.removeFromLeft(4.0f);
    g.setColour(MidiSingLookAndFeel::textColour);
    g.setFont(13.0f);
    g.drawText(file.getFileName(), bounds.toNearestInt(), juce::Justification::centredLeft, true);
}

void FileBrowserItem::mouseEnter(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isHovered = true;
    repaint();
}

void FileBrowserItem::mouseExit(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isHovered = false;
    repaint();
}

void FileBrowserItem::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isSelected = true;
    repaint();

    if (onFileClicked)
        onFileClicked(file);
}

void FileBrowserItem::mouseDoubleClick(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    if (isDir && onDirectorySelected)
    {
        onDirectorySelected(file);
    }
    else if (onFileDoubleClicked)
    {
        onFileDoubleClicked(file);
    }
}

void FileBrowserItem::mouseDrag(const juce::MouseEvent& e)
{
    // Start drag-and-drop for files (not directories)
    if (!isDir && e.getDistanceFromDragStart() > 5)
    {
        // Create drag description with file path
        juce::var dragData;
        dragData = file.getFullPathName();

        // Perform external drag for dropping to timeline
        juce::StringArray files;
        files.add(file.getFullPathName());
        performExternalDragDropOfFiles(files, false);
    }
}

//==============================================================================
// AudioPreviewComponent Implementation
//==============================================================================

AudioPreviewComponent::AudioPreviewComponent()
{
    formatManager.registerBasicFormats();

    // Play button
    playButton.setButtonText("Play");
    playButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::playColour.withAlpha(0.8f));
    playButton.onClick = [this]() { play(); };
    addAndMakeVisible(playButton);

    // Stop button
    stopButton.setButtonText("Stop");
    stopButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::recordColour.withAlpha(0.8f));
    stopButton.onClick = [this]() { stop(); };
    addAndMakeVisible(stopButton);

    // File name label
    fileNameLabel.setText("No file selected", juce::dontSendNotification);
    fileNameLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    fileNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(fileNameLabel);

    // Duration label
    durationLabel.setText("--:--", juce::dontSendNotification);
    durationLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
    durationLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(durationLabel);

    // Start timer for updating playback position
    startTimerHz(30);
}

AudioPreviewComponent::~AudioPreviewComponent()
{
    stopTimer();
    stop();

    if (deviceManager != nullptr)
    {
        deviceManager->removeAudioCallback(&audioSourcePlayer);
    }

    audioSourcePlayer.setSource(nullptr);
    transportSource.setSource(nullptr);
    readerSource.reset();
}

void AudioPreviewComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.setColour(MidiSingLookAndFeel::backgroundMid);
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    // Border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 4.0f, 1.0f);

    // Waveform area
    auto waveformBounds = bounds.reduced(4);
    waveformBounds.removeFromTop(22);  // Space for filename
    waveformBounds.removeFromBottom(26);  // Space for buttons

    if (fileLoaded && waveformBuffer.getNumSamples() > 0)
    {
        drawWaveform(g, waveformBounds);

        // Draw playhead position
        if (transportSource.isPlaying())
        {
            double position = transportSource.getCurrentPosition() / transportSource.getLengthInSeconds();
            int playheadX = waveformBounds.getX() + static_cast<int>(position * waveformBounds.getWidth());

            g.setColour(MidiSingLookAndFeel::accentColour);
            g.drawLine(static_cast<float>(playheadX), static_cast<float>(waveformBounds.getY()),
                       static_cast<float>(playheadX), static_cast<float>(waveformBounds.getBottom()), 2.0f);
        }
    }
    else
    {
        g.setColour(MidiSingLookAndFeel::textDimColour);
        g.setFont(12.0f);
        g.drawText("Click a file to preview", waveformBounds, juce::Justification::centred, true);
    }
}

void AudioPreviewComponent::resized()
{
    auto bounds = getLocalBounds().reduced(4);

    // File name at top
    auto topRow = bounds.removeFromTop(20);
    fileNameLabel.setBounds(topRow.removeFromLeft(topRow.getWidth() - 50));
    durationLabel.setBounds(topRow);

    // Buttons at bottom
    auto bottomRow = bounds.removeFromBottom(24);
    playButton.setBounds(bottomRow.removeFromLeft(50));
    bottomRow.removeFromLeft(4);
    stopButton.setBounds(bottomRow.removeFromLeft(50));
}

void AudioPreviewComponent::mouseDown(const juce::MouseEvent& e)
{
    // Click on waveform to seek
    if (fileLoaded && transportSource.getLengthInSeconds() > 0)
    {
        auto bounds = getLocalBounds().reduced(4);
        bounds.removeFromTop(22);
        bounds.removeFromBottom(26);

        if (bounds.contains(e.getPosition()))
        {
            double clickPosition = static_cast<double>(e.x - bounds.getX()) / bounds.getWidth();
            clickPosition = juce::jlimit(0.0, 1.0, clickPosition);
            transportSource.setPosition(clickPosition * transportSource.getLengthInSeconds());
        }
    }
}

void AudioPreviewComponent::timerCallback()
{
    if (transportSource.isPlaying())
    {
        repaint();
    }
}

void AudioPreviewComponent::setFile(const juce::File& file)
{
    if (file == currentFile)
        return;

    stop();
    currentFile = file;

    if (file.existsAsFile())
    {
        loadFile(file);
        fileNameLabel.setText(file.getFileName(), juce::dontSendNotification);
    }
    else
    {
        fileLoaded = false;
        fileNameLabel.setText("No file selected", juce::dontSendNotification);
        durationLabel.setText("--:--", juce::dontSendNotification);
        waveformBuffer.clear();
    }

    repaint();
}

void AudioPreviewComponent::loadFile(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor(file);

    if (reader != nullptr)
    {
        // Create waveform preview buffer (downsampled)
        auto numSamples = static_cast<int>(reader->lengthInSamples);
        int previewSamples = juce::jmin(512, numSamples);
        waveformBuffer.setSize(reader->numChannels, previewSamples);

        // Downsample for waveform display
        juce::AudioBuffer<float> tempBuffer(reader->numChannels, numSamples);
        reader->read(&tempBuffer, 0, numSamples, 0, true, true);

        int samplesPerPixel = numSamples / previewSamples;
        for (int ch = 0; ch < static_cast<int>(reader->numChannels); ++ch)
        {
            auto* src = tempBuffer.getReadPointer(ch);
            auto* dest = waveformBuffer.getWritePointer(ch);

            for (int i = 0; i < previewSamples; ++i)
            {
                float maxVal = 0.0f;
                int start = i * samplesPerPixel;
                int end = juce::jmin(start + samplesPerPixel, numSamples);

                for (int j = start; j < end; ++j)
                {
                    maxVal = juce::jmax(maxVal, std::abs(src[j]));
                }

                dest[i] = maxVal;
            }
        }

        // Setup transport for playback
        readerSource = std::make_unique<juce::AudioFormatReaderSource>(
            formatManager.createReaderFor(file), true);

        transportSource.setSource(readerSource.get(), 0, nullptr, reader->sampleRate);

        // Update duration label
        double durationSecs = reader->lengthInSamples / reader->sampleRate;
        int mins = static_cast<int>(durationSecs) / 60;
        int secs = static_cast<int>(durationSecs) % 60;
        durationLabel.setText(juce::String::formatted("%d:%02d", mins, secs), juce::dontSendNotification);

        fileLoaded = true;

        delete reader;
    }
    else
    {
        fileLoaded = false;
        durationLabel.setText("--:--", juce::dontSendNotification);
    }
}

void AudioPreviewComponent::drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (waveformBuffer.getNumSamples() == 0)
        return;

    g.setColour(MidiSingLookAndFeel::backgroundDark);
    g.fillRect(bounds);

    g.setColour(MidiSingLookAndFeel::waveformColour.withAlpha(0.7f));

    float width = static_cast<float>(bounds.getWidth());
    float height = static_cast<float>(bounds.getHeight());
    float centreY = bounds.getCentreY();

    int numSamples = waveformBuffer.getNumSamples();
    auto* samples = waveformBuffer.getReadPointer(0);

    juce::Path waveformPath;
    waveformPath.startNewSubPath(static_cast<float>(bounds.getX()), centreY);

    for (int i = 0; i < numSamples; ++i)
    {
        float x = bounds.getX() + (static_cast<float>(i) / numSamples) * width;
        float y = centreY - samples[i] * height * 0.45f;
        waveformPath.lineTo(x, y);
    }

    // Mirror for bottom half
    for (int i = numSamples - 1; i >= 0; --i)
    {
        float x = bounds.getX() + (static_cast<float>(i) / numSamples) * width;
        float y = centreY + samples[i] * height * 0.45f;
        waveformPath.lineTo(x, y);
    }

    waveformPath.closeSubPath();
    g.fillPath(waveformPath);
}

void AudioPreviewComponent::play()
{
    if (fileLoaded && deviceManager != nullptr)
    {
        audioSourcePlayer.setSource(&transportSource);
        deviceManager->addAudioCallback(&audioSourcePlayer);
        transportSource.setPosition(0);
        transportSource.start();
    }
}

void AudioPreviewComponent::stop()
{
    transportSource.stop();

    if (deviceManager != nullptr)
    {
        deviceManager->removeAudioCallback(&audioSourcePlayer);
    }

    audioSourcePlayer.setSource(nullptr);
}

//==============================================================================
// LocationButton Implementation
//==============================================================================

LocationButton::LocationButton(const juce::String& name, const juce::File& location)
    : juce::TextButton(name), locationPath(location)
{
    setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::backgroundMid);
    setColour(juce::TextButton::textColourOffId, MidiSingLookAndFeel::textColour);
}

//==============================================================================
// FileBrowser Implementation
//==============================================================================

FileBrowser::FileBrowser()
{
    // Setup navigation buttons
    backButton.setButtonText("<");
    backButton.setTooltip("Back");
    backButton.onClick = [this]() { navigateBack(); };
    addAndMakeVisible(backButton);

    forwardButton.setButtonText(">");
    forwardButton.setTooltip("Forward");
    forwardButton.onClick = [this]() { navigateForward(); };
    addAndMakeVisible(forwardButton);

    upButton.setButtonText("^");
    upButton.setTooltip("Up one level");
    upButton.onClick = [this]() { navigateUp(); };
    addAndMakeVisible(upButton);

    refreshButton.setButtonText("R");
    refreshButton.setTooltip("Refresh");
    refreshButton.onClick = [this]() { refresh(); };
    addAndMakeVisible(refreshButton);

    homeButton.setButtonText("H");
    homeButton.setTooltip("Home");
    homeButton.onClick = [this]() {
        navigateTo(juce::File::getSpecialLocation(juce::File::userHomeDirectory));
    };
    addAndMakeVisible(homeButton);

    // Setup search box
    searchBox.setTextToShowWhenEmpty("Search...", MidiSingLookAndFeel::textDimColour);
    searchBox.onTextChange = [this]() {
        // Filter would be applied here
        repaint();
    };
    addAndMakeVisible(searchBox);

    // Setup status label
    statusLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    // Setup location buttons
    setupLocationButtons();

    // Setup file browser
    setupFileBrowser();

    // Setup preview component
    previewComponent = std::make_unique<AudioPreviewComponent>();
    addAndMakeVisible(previewComponent.get());

    // Load favorites
    loadFavorites();

    // Update navigation state
    updateNavigationButtons();
}

FileBrowser::~FileBrowser()
{
    saveFavorites();

    if (fileBrowser != nullptr)
        fileBrowser->removeListener(this);
}

void FileBrowser::setupLocationButtons()
{
    // Home
    auto homeBtn = std::make_unique<LocationButton>("Home",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory));
    homeBtn->onClick = [this, loc = homeBtn->getLocation()]() { navigateTo(loc); };
    addAndMakeVisible(homeBtn.get());
    locationButtons.push_back(std::move(homeBtn));

    // Desktop
    auto desktopBtn = std::make_unique<LocationButton>("Desktop",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory));
    desktopBtn->onClick = [this, loc = desktopBtn->getLocation()]() { navigateTo(loc); };
    addAndMakeVisible(desktopBtn.get());
    locationButtons.push_back(std::move(desktopBtn));

    // Documents
    auto docsBtn = std::make_unique<LocationButton>("Documents",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
    docsBtn->onClick = [this, loc = docsBtn->getLocation()]() { navigateTo(loc); };
    addAndMakeVisible(docsBtn.get());
    locationButtons.push_back(std::move(docsBtn));

    // Music
    auto musicBtn = std::make_unique<LocationButton>("Music",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory));
    musicBtn->onClick = [this, loc = musicBtn->getLocation()]() { navigateTo(loc); };
    addAndMakeVisible(musicBtn.get());
    locationButtons.push_back(std::move(musicBtn));
}

void FileBrowser::setupFileBrowser()
{
    // Setup file filter for audio and MIDI files
    fileFilter = std::make_unique<juce::WildcardFileFilter>(
        "*.wav;*.mp3;*.aif;*.aiff;*.flac;*.ogg;*.mid;*.midi",
        "*",
        "Audio & MIDI Files");

    // Create file browser
    fileBrowser = std::make_unique<juce::FileBrowserComponent>(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles |
        juce::FileBrowserComponent::canSelectDirectories,
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        fileFilter.get(),
        nullptr);

    fileBrowser->addListener(this);
    fileBrowser->setColour(juce::FileBrowserComponent::currentPathBoxBackgroundColourId,
                           MidiSingLookAndFeel::buttonBackground);
    fileBrowser->setColour(juce::FileBrowserComponent::currentPathBoxTextColourId,
                           MidiSingLookAndFeel::textColour);
    addAndMakeVisible(fileBrowser.get());

    currentDirectory = fileBrowser->getRoot();
}

void FileBrowser::paint(juce::Graphics& g)
{
    g.fillAll(MidiSingLookAndFeel::backgroundDark);
}

void FileBrowser::resized()
{
    auto bounds = getLocalBounds();

    // Navigation toolbar at top
    auto toolbar = bounds.removeFromTop(TOOLBAR_HEIGHT);
    int btnWidth = 28;
    int btnMargin = 2;

    backButton.setBounds(toolbar.removeFromLeft(btnWidth).reduced(btnMargin));
    forwardButton.setBounds(toolbar.removeFromLeft(btnWidth).reduced(btnMargin));
    upButton.setBounds(toolbar.removeFromLeft(btnWidth).reduced(btnMargin));
    refreshButton.setBounds(toolbar.removeFromLeft(btnWidth).reduced(btnMargin));
    homeButton.setBounds(toolbar.removeFromLeft(btnWidth).reduced(btnMargin));

    toolbar.removeFromLeft(8);
    searchBox.setBounds(toolbar.reduced(btnMargin));

    // Location buttons bar
    auto locationBar = bounds.removeFromTop(LOCATION_BAR_HEIGHT);
    int locBtnWidth = locationBar.getWidth() / static_cast<int>(locationButtons.size());
    for (auto& btn : locationButtons)
    {
        btn->setBounds(locationBar.removeFromLeft(locBtnWidth).reduced(2));
    }

    // Preview component at bottom
    if (previewComponent != nullptr)
    {
        previewComponent->setBounds(bounds.removeFromBottom(PREVIEW_HEIGHT).reduced(2));
    }

    // Status bar
    statusLabel.setBounds(bounds.removeFromBottom(20).reduced(4, 0));

    // File browser fills the rest
    if (fileBrowser != nullptr)
    {
        fileBrowser->setBounds(bounds.reduced(2));
    }
}

void FileBrowser::selectionChanged()
{
    auto selectedFile = fileBrowser->getSelectedFile(0);

    if (selectedFile.existsAsFile())
    {
        statusLabel.setText(selectedFile.getFileName() + " - " +
                            juce::File::descriptionOfSizeInBytes(selectedFile.getSize()),
                            juce::dontSendNotification);

        // If it's an audio file, load it in preview
        if (isAudioFile(selectedFile))
        {
            previewComponent->setFile(selectedFile);
        }

        if (onFileSelected)
            onFileSelected(selectedFile);
    }
    else if (selectedFile.isDirectory())
    {
        int numItems = selectedFile.getNumberOfChildFiles(
            juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles);
        statusLabel.setText(juce::String(numItems) + " items", juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText("", juce::dontSendNotification);
    }
}

void FileBrowser::fileClicked(const juce::File& file, const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    if (file.existsAsFile())
    {
        // Preview audio file on single click
        if (isAudioFile(file))
        {
            previewComponent->setFile(file);
            previewComponent->play();
        }

        if (onFileSelected)
            onFileSelected(file);
    }
}

void FileBrowser::fileDoubleClicked(const juce::File& file)
{
    if (file.isDirectory())
    {
        navigateTo(file);
    }
    else if (file.existsAsFile())
    {
        if (onFileDoubleClicked)
            onFileDoubleClicked(file);
    }
}

void FileBrowser::browserRootChanged(const juce::File& newRoot)
{
    // Add to history
    if (currentDirectory != newRoot && currentDirectory.exists())
    {
        historyBack.push_back(currentDirectory);
        historyForward.clear();
    }

    currentDirectory = newRoot;
    updateNavigationButtons();

    // Update status
    int numItems = newRoot.getNumberOfChildFiles(
        juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles);
    statusLabel.setText(newRoot.getFileName() + " - " + juce::String(numItems) + " items",
                        juce::dontSendNotification);
}

void FileBrowser::setDeviceManager(juce::AudioDeviceManager* dm)
{
    deviceManager = dm;
    if (previewComponent != nullptr)
    {
        previewComponent->setDeviceManager(dm);
    }
}

void FileBrowser::navigateTo(const juce::File& directory)
{
    if (directory.isDirectory())
    {
        fileBrowser->setRoot(directory);
    }
}

void FileBrowser::navigateBack()
{
    if (!historyBack.empty())
    {
        historyForward.push_back(currentDirectory);
        auto dest = historyBack.back();
        historyBack.pop_back();

        // Temporarily disable history recording
        currentDirectory = dest;
        fileBrowser->setRoot(dest);
        updateNavigationButtons();
    }
}

void FileBrowser::navigateForward()
{
    if (!historyForward.empty())
    {
        historyBack.push_back(currentDirectory);
        auto dest = historyForward.back();
        historyForward.pop_back();

        currentDirectory = dest;
        fileBrowser->setRoot(dest);
        updateNavigationButtons();
    }
}

void FileBrowser::navigateUp()
{
    auto parent = currentDirectory.getParentDirectory();
    if (parent.exists() && parent != currentDirectory)
    {
        navigateTo(parent);
    }
}

void FileBrowser::refresh()
{
    fileBrowser->refresh();
}

void FileBrowser::updateNavigationButtons()
{
    backButton.setEnabled(!historyBack.empty());
    forwardButton.setEnabled(!historyForward.empty());
    upButton.setEnabled(currentDirectory.getParentDirectory() != currentDirectory);
}

void FileBrowser::addToFavorites(const juce::File& file)
{
    if (!isFavorite(file))
    {
        favoriteLocations.push_back(file);
        saveFavorites();
    }
}

void FileBrowser::removeFromFavorites(const juce::File& file)
{
    favoriteLocations.erase(
        std::remove(favoriteLocations.begin(), favoriteLocations.end(), file),
        favoriteLocations.end());
    saveFavorites();
}

bool FileBrowser::isFavorite(const juce::File& file) const
{
    return std::find(favoriteLocations.begin(), favoriteLocations.end(), file)
           != favoriteLocations.end();
}

void FileBrowser::loadFavorites()
{
    auto favFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("MidiSing")
                       .getChildFile("favorites.txt");

    if (favFile.existsAsFile())
    {
        juce::StringArray lines;
        favFile.readLines(lines);

        for (const auto& line : lines)
        {
            juce::File f(line);
            if (f.exists())
            {
                favoriteLocations.push_back(f);
            }
        }
    }
}

void FileBrowser::saveFavorites()
{
    auto favFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("MidiSing")
                       .getChildFile("favorites.txt");

    favFile.getParentDirectory().createDirectory();

    juce::String content;
    for (const auto& f : favoriteLocations)
    {
        content += f.getFullPathName() + "\n";
    }

    favFile.replaceWithText(content);
}

juce::File FileBrowser::getSelectedFile() const
{
    return fileBrowser->getSelectedFile(0);
}

juce::String FileBrowser::getFileTypeIcon(const juce::File& file) const
{
    if (file.isDirectory())
        return "folder";

    auto ext = file.getFileExtension().toLowerCase();

    if (ext == ".wav" || ext == ".mp3" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg")
        return "audio";
    if (ext == ".mid" || ext == ".midi")
        return "midi";

    return "file";
}

bool FileBrowser::isAudioFile(const juce::File& file) const
{
    auto ext = file.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".mp3" || ext == ".aif" || ext == ".aiff"
        || ext == ".flac" || ext == ".ogg";
}

bool FileBrowser::isMidiFile(const juce::File& file) const
{
    auto ext = file.getFileExtension().toLowerCase();
    return ext == ".mid" || ext == ".midi";
}

bool FileBrowser::isSupportedFile(const juce::File& file) const
{
    return isAudioFile(file) || isMidiFile(file);
}
