#include "FileBrowser.h"

//==============================================================================
// FavoritesManager Implementation
//==============================================================================

FavoritesManager::FavoritesManager()
{
    // Set up properties file options
    juce::PropertiesFile::Options options;
    options.applicationName = "MidiSing";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.folderName = "MidiSing";
    options.storageFormat = juce::PropertiesFile::storeAsXML;

    // Create the properties file
    propertiesFile = std::make_unique<juce::PropertiesFile>(options);

    // Load favorites from properties
    loadFromProperties();
}

FavoritesManager::~FavoritesManager()
{
    // Save favorites before destroying
    if (propertiesFile != nullptr)
    {
        saveToProperties();
        propertiesFile->saveIfNeeded();
    }
}

void FavoritesManager::addFavorite(const juce::File& folder)
{
    if (!folder.exists())
        return;

    // Check if already in favorites
    if (isFavorite(folder))
        return;

    // Check max limit
    if (favorites.size() >= maxNumberOfFavorites)
    {
        // Remove the oldest favorite to make room
        favorites.remove(0);
    }

    favorites.add(folder);
    saveToProperties();
    notifyListeners();
}

void FavoritesManager::removeFavorite(const juce::File& folder)
{
    int index = favorites.indexOf(folder);
    if (index >= 0)
    {
        favorites.remove(index);
        saveToProperties();
        notifyListeners();
    }
}

bool FavoritesManager::isFavorite(const juce::File& folder) const
{
    return favorites.contains(folder);
}

juce::Array<juce::File> FavoritesManager::getFavorites() const
{
    return favorites;
}

void FavoritesManager::clear()
{
    favorites.clear();
    saveToProperties();
    notifyListeners();
}

void FavoritesManager::addListener(Listener* listener)
{
    listeners.add(listener);
}

void FavoritesManager::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

void FavoritesManager::loadFromProperties()
{
    if (propertiesFile == nullptr)
        return;

    // Load favorites as a semicolon-separated string
    juce::String favoritesString = propertiesFile->getValue(favoritesKey);

    if (favoritesString.isNotEmpty())
    {
        juce::StringArray paths;
        paths.addTokens(favoritesString, ";", "");

        for (const auto& path : paths)
        {
            juce::File folder(path);
            if (folder.exists() && favorites.size() < maxNumberOfFavorites)
            {
                favorites.add(folder);
            }
        }
    }
}

void FavoritesManager::saveToProperties()
{
    if (propertiesFile == nullptr)
        return;

    // Save favorites as a semicolon-separated string
    juce::StringArray paths;
    for (const auto& folder : favorites)
    {
        paths.add(folder.getFullPathName());
    }

    propertiesFile->setValue(favoritesKey, paths.joinIntoString(";"));
    propertiesFile->saveIfNeeded();
}

void FavoritesManager::notifyListeners()
{
    listeners.call([](Listener& l) { l.favoritesChanged(); });
}

//==============================================================================
// FavoriteButton Implementation
//==============================================================================

FavoriteButton::FavoriteButton(const juce::File& f, FavoritesManager& manager)
    : folder(f), favoritesManager(manager)
{
    // Remove button (X)
    removeButton.setButtonText("x");
    removeButton.setTooltip("Remove from favorites");
    removeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    removeButton.setColour(juce::TextButton::textColourOffId, MidiSingLookAndFeel::textDimColour);
    removeButton.onClick = [this]() {
        favoritesManager.removeFavorite(folder);
    };
    addAndMakeVisible(removeButton);
}

void FavoriteButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    if (isHovered)
    {
        g.setColour(MidiSingLookAndFeel::backgroundLight);
        g.fillRoundedRectangle(bounds, 3.0f);
    }

    // Folder icon
    auto iconBounds = bounds.removeFromLeft(20.0f).reduced(4.0f);
    g.setColour(MidiSingLookAndFeel::accentColour);

    // Draw star icon for favorite
    juce::Path starPath;
    auto ib = iconBounds.reduced(1.0f);
    float cx = ib.getCentreX();
    float cy = ib.getCentreY();
    float r = ib.getWidth() * 0.5f;

    for (int i = 0; i < 5; ++i)
    {
        float angle = juce::MathConstants<float>::twoPi * i / 5.0f - juce::MathConstants<float>::halfPi;
        float x = cx + r * std::cos(angle);
        float y = cy + r * std::sin(angle);

        if (i == 0)
            starPath.startNewSubPath(x, y);
        else
            starPath.lineTo(x, y);

        // Inner point
        float innerAngle = angle + juce::MathConstants<float>::twoPi / 10.0f;
        float innerR = r * 0.4f;
        starPath.lineTo(cx + innerR * std::cos(innerAngle),
                        cy + innerR * std::sin(innerAngle));
    }
    starPath.closeSubPath();
    g.fillPath(starPath);

    // Folder name
    bounds.removeFromLeft(2.0f);
    bounds.removeFromRight(18.0f); // Space for remove button
    g.setColour(MidiSingLookAndFeel::textColour);
    g.setFont(12.0f);
    g.drawText(folder.getFileName(), bounds.toNearestInt(), juce::Justification::centredLeft, true);
}

void FavoriteButton::resized()
{
    auto bounds = getLocalBounds();
    removeButton.setBounds(bounds.removeFromRight(18).reduced(2));
}

void FavoriteButton::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    if (onFolderClicked)
        onFolderClicked(folder);
}

void FavoriteButton::mouseEnter(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isHovered = true;
    repaint();
}

void FavoriteButton::mouseExit(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isHovered = false;
    repaint();
}

//==============================================================================
// FavoritesPanel Implementation
//==============================================================================

FavoritesPanel::FavoritesPanel(FavoritesManager& manager)
    : favoritesManager(manager)
{
    favoritesManager.addListener(this);

    // Title label
    titleLabel.setText("Favorites", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    titleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    // Viewport for scrollable favorites
    viewport.setViewedComponent(&buttonsContainer, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    rebuildFavoriteButtons();
}

FavoritesPanel::~FavoritesPanel()
{
    favoritesManager.removeListener(this);
}

void FavoritesPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.setColour(MidiSingLookAndFeel::backgroundMid);
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    // Border
    g.setColour(MidiSingLookAndFeel::borderColour);
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 4.0f, 1.0f);
}

void FavoritesPanel::resized()
{
    auto bounds = getLocalBounds().reduced(4);

    // Title at top
    titleLabel.setBounds(bounds.removeFromTop(20));

    // Viewport for favorites list
    viewport.setBounds(bounds);

    // Update buttons container size
    int buttonHeight = 24;
    int totalHeight = static_cast<int>(favoriteButtons.size()) * buttonHeight;
    buttonsContainer.setSize(bounds.getWidth() - 10, juce::jmax(totalHeight, bounds.getHeight()));

    // Position buttons
    int y = 0;
    for (auto& btn : favoriteButtons)
    {
        btn->setBounds(0, y, buttonsContainer.getWidth(), buttonHeight);
        y += buttonHeight;
    }
}

void FavoritesPanel::favoritesChanged()
{
    rebuildFavoriteButtons();
}

void FavoritesPanel::rebuildFavoriteButtons()
{
    // Clear existing buttons
    favoriteButtons.clear();
    buttonsContainer.removeAllChildren();

    // Create buttons for each favorite
    auto favorites = favoritesManager.getFavorites();
    for (const auto& folder : favorites)
    {
        auto btn = std::make_unique<FavoriteButton>(folder, favoritesManager);
        btn->onFolderClicked = [this](const juce::File& f) {
            if (onFavoriteSelected)
                onFavoriteSelected(f);
        };
        buttonsContainer.addAndMakeVisible(btn.get());
        favoriteButtons.push_back(std::move(btn));
    }

    resized();
    repaint();
}

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

    // Listen to transport changes
    transportSource.addChangeListener(this);

    // Play button
    playButton.setButtonText(">");
    playButton.setTooltip("Play");
    playButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::playColour.withAlpha(0.8f));
    playButton.onClick = [this]() {
        if (isPlaying())
            stop();
        else
            play();
    };
    addAndMakeVisible(playButton);

    // Stop button
    stopButton.setButtonText("[]");
    stopButton.setTooltip("Stop");
    stopButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::recordColour.withAlpha(0.8f));
    stopButton.onClick = [this]() {
        stop();
        transportSource.setPosition(0);
    };
    addAndMakeVisible(stopButton);

    // Loop button
    loopButton.setButtonText("L");
    loopButton.setTooltip("Loop");
    loopButton.setClickingTogglesState(true);
    loopButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::backgroundMid);
    loopButton.setColour(juce::TextButton::buttonOnColourId, MidiSingLookAndFeel::accentColour);
    loopButton.onClick = [this]() {
        setLooping(loopButton.getToggleState());
    };
    addAndMakeVisible(loopButton);

    // Volume slider
    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(volume, juce::dontSendNotification);
    volumeSlider.setTooltip("Preview Volume");
    volumeSlider.onValueChange = [this]() {
        setVolume(static_cast<float>(volumeSlider.getValue()));
    };
    addAndMakeVisible(volumeSlider);

    // File name label
    fileNameLabel.setText("No file selected", juce::dontSendNotification);
    fileNameLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textColour);
    fileNameLabel.setJustificationType(juce::Justification::centredLeft);
    fileNameLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(fileNameLabel);

    // Duration label
    durationLabel.setText("--:--", juce::dontSendNotification);
    durationLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
    durationLabel.setJustificationType(juce::Justification::centredRight);
    durationLabel.setFont(juce::Font(11.0f));
    addAndMakeVisible(durationLabel);

    // Info label (sample rate, channels, bit depth)
    infoLabel.setText("", juce::dontSendNotification);
    infoLabel.setColour(juce::Label::textColourId, MidiSingLookAndFeel::textDimColour);
    infoLabel.setJustificationType(juce::Justification::centredLeft);
    infoLabel.setFont(juce::Font(10.0f));
    addAndMakeVisible(infoLabel);

    // Start timer for updating playback position
    startTimerHz(30);
}

AudioPreviewComponent::~AudioPreviewComponent()
{
    stopTimer();
    transportSource.removeChangeListener(this);
    stop();

    if (deviceManager != nullptr)
    {
        deviceManager->removeAudioCallback(&audioSourcePlayer);
    }

    audioSourcePlayer.setSource(nullptr);
    transportSource.setSource(nullptr);
    readerSource.reset();
}

void AudioPreviewComponent::setDeviceManager(juce::AudioDeviceManager* dm)
{
    if (deviceManager != dm)
    {
        // Remove from old device manager
        if (deviceManager != nullptr)
        {
            deviceManager->removeAudioCallback(&audioSourcePlayer);
        }

        deviceManager = dm;
        audioPlayerPrepared = false;

        // Prepare with new device manager
        if (deviceManager != nullptr)
        {
            prepareAudioPlayer();
        }
    }
}

void AudioPreviewComponent::prepareAudioPlayer()
{
    if (deviceManager != nullptr && !audioPlayerPrepared)
    {
        auto* currentDevice = deviceManager->getCurrentAudioDevice();
        if (currentDevice != nullptr)
        {
            double sampleRate = currentDevice->getCurrentSampleRate();
            int blockSize = currentDevice->getCurrentBufferSizeSamples();

            if (sampleRate > 0 && blockSize > 0)
            {
                transportSource.prepareToPlay(blockSize, sampleRate);
                audioPlayerPrepared = true;
            }
        }
    }
}

void AudioPreviewComponent::setVolume(float newVolume)
{
    volume = juce::jlimit(0.0f, 1.0f, newVolume);
    audioSourcePlayer.setGain(volume);
}

void AudioPreviewComponent::setLooping(bool shouldLoop)
{
    looping = shouldLoop;
    if (readerSource != nullptr)
    {
        readerSource->setLooping(looping);
    }
}

void AudioPreviewComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &transportSource)
    {
        updateTransportState();
    }
}

void AudioPreviewComponent::updateTransportState()
{
    if (transportSource.isPlaying())
    {
        playButton.setButtonText("||");
        playButton.setTooltip("Pause");
    }
    else
    {
        playButton.setButtonText(">");
        playButton.setTooltip("Play");
    }
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

    // Waveform area (accounting for new layout)
    auto waveformBounds = bounds.reduced(4);
    waveformBounds.removeFromTop(34);  // Space for filename + info
    waveformBounds.removeFromBottom(26);  // Space for buttons

    if (fileLoaded && waveformBuffer.getNumSamples() > 0)
    {
        drawWaveform(g, waveformBounds);

        // Draw playhead position
        double lengthInSeconds = transportSource.getLengthInSeconds();
        if (lengthInSeconds > 0)
        {
            double position = transportSource.getCurrentPosition() / lengthInSeconds;
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
    auto topRow = bounds.removeFromTop(18);
    fileNameLabel.setBounds(topRow.removeFromLeft(topRow.getWidth() - 50));
    durationLabel.setBounds(topRow);

    // Info row (sample rate, channels, etc.)
    auto infoRow = bounds.removeFromTop(14);
    infoLabel.setBounds(infoRow);

    // Buttons at bottom
    auto bottomRow = bounds.removeFromBottom(24);
    int btnWidth = 28;
    int spacing = 3;

    playButton.setBounds(bottomRow.removeFromLeft(btnWidth));
    bottomRow.removeFromLeft(spacing);
    stopButton.setBounds(bottomRow.removeFromLeft(btnWidth));
    bottomRow.removeFromLeft(spacing);
    loopButton.setBounds(bottomRow.removeFromLeft(btnWidth));
    bottomRow.removeFromLeft(spacing + 4);

    // Volume slider takes remaining space
    volumeSlider.setBounds(bottomRow);
}

void AudioPreviewComponent::mouseDown(const juce::MouseEvent& e)
{
    // Click on waveform to seek
    if (fileLoaded && transportSource.getLengthInSeconds() > 0)
    {
        auto bounds = getLocalBounds().reduced(4);
        bounds.removeFromTop(34);  // Match paint() layout
        bounds.removeFromBottom(26);

        if (bounds.contains(e.getPosition()))
        {
            double clickPosition = static_cast<double>(e.x - bounds.getX()) / bounds.getWidth();
            clickPosition = juce::jlimit(0.0, 1.0, clickPosition);
            transportSource.setPosition(clickPosition * transportSource.getLengthInSeconds());

            // Start playing if not already
            if (!isPlaying())
            {
                play();
            }
        }
    }
}

void AudioPreviewComponent::timerCallback()
{
    if (transportSource.isPlaying())
    {
        // Update duration label to show current position / total
        double currentPos = transportSource.getCurrentPosition();
        double totalLength = transportSource.getLengthInSeconds();

        if (totalLength > 0)
        {
            int curMins = static_cast<int>(currentPos) / 60;
            int curSecs = static_cast<int>(currentPos) % 60;
            int totMins = static_cast<int>(totalLength) / 60;
            int totSecs = static_cast<int>(totalLength) % 60;

            durationLabel.setText(juce::String::formatted("%d:%02d / %d:%02d",
                curMins, curSecs, totMins, totSecs), juce::dontSendNotification);
        }

        repaint();
    }
    else if (fileLoaded)
    {
        // Reset to show total duration when stopped
        double totalLength = transportSource.getLengthInSeconds();
        if (totalLength > 0)
        {
            int mins = static_cast<int>(totalLength) / 60;
            int secs = static_cast<int>(totalLength) % 60;
            int millis = static_cast<int>((totalLength - static_cast<int>(totalLength)) * 10);
            durationLabel.setText(juce::String::formatted("%d:%02d.%d", mins, secs, millis),
                juce::dontSendNotification);
        }
    }
}

void AudioPreviewComponent::setFile(const juce::File& file)
{
    if (file == currentFile && fileLoaded)
        return;

    // Stop previous playback
    stop();
    currentFile = file;

    if (file.existsAsFile())
    {
        loadFile(file);
        fileNameLabel.setText(file.getFileName(), juce::dontSendNotification);

        // Auto-play if enabled
        if (fileLoaded && autoPlay)
        {
            play();
        }
    }
    else
    {
        fileLoaded = false;
        currentSampleRate = 44100.0;
        currentNumChannels = 2;
        currentBitsPerSample = 16;
        currentLengthInSamples = 0;

        fileNameLabel.setText("No file selected", juce::dontSendNotification);
        durationLabel.setText("--:--", juce::dontSendNotification);
        infoLabel.setText("", juce::dontSendNotification);
        waveformBuffer.clear();
    }

    repaint();
}

double AudioPreviewComponent::getDurationSeconds() const
{
    if (currentSampleRate > 0 && currentLengthInSamples > 0)
    {
        return static_cast<double>(currentLengthInSamples) / currentSampleRate;
    }
    return 0.0;
}

juce::String AudioPreviewComponent::getFileInfo() const
{
    if (!fileLoaded)
        return "";

    juce::String info;
    info << juce::String(currentSampleRate / 1000.0, 1) << " kHz | ";
    info << currentNumChannels << " ch | ";
    info << currentBitsPerSample << " bit";
    return info;
}

void AudioPreviewComponent::loadFile(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor(file);

    if (reader != nullptr)
    {
        // Store file properties
        currentSampleRate = reader->sampleRate;
        currentNumChannels = static_cast<int>(reader->numChannels);
        currentBitsPerSample = static_cast<int>(reader->bitsPerSample);
        currentLengthInSamples = reader->lengthInSamples;

        // Create waveform preview buffer (downsampled)
        auto numSamples = static_cast<int>(reader->lengthInSamples);
        int previewSamples = juce::jmin(512, numSamples);

        if (previewSamples > 0)
        {
            waveformBuffer.setSize(static_cast<int>(reader->numChannels), previewSamples);

            // Downsample for waveform display
            juce::AudioBuffer<float> tempBuffer(static_cast<int>(reader->numChannels), numSamples);
            reader->read(&tempBuffer, 0, numSamples, 0, true, true);

            int samplesPerPixel = juce::jmax(1, numSamples / previewSamples);
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
        }

        // Setup transport for playback
        readerSource = std::make_unique<juce::AudioFormatReaderSource>(
            formatManager.createReaderFor(file), true);

        if (readerSource != nullptr)
        {
            readerSource->setLooping(looping);
            transportSource.setSource(readerSource.get(), 0, nullptr, reader->sampleRate);

            // Prepare the audio player if we have a device manager
            if (deviceManager != nullptr && !audioPlayerPrepared)
            {
                prepareAudioPlayer();
            }
        }

        // Update duration label
        double durationSecs = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
        int mins = static_cast<int>(durationSecs) / 60;
        int secs = static_cast<int>(durationSecs) % 60;
        int millis = static_cast<int>((durationSecs - static_cast<int>(durationSecs)) * 10);
        durationLabel.setText(juce::String::formatted("%d:%02d.%d", mins, secs, millis), juce::dontSendNotification);

        // Update info label
        infoLabel.setText(getFileInfo(), juce::dontSendNotification);

        fileLoaded = true;

        delete reader;
    }
    else
    {
        fileLoaded = false;
        currentSampleRate = 44100.0;
        currentNumChannels = 2;
        currentBitsPerSample = 16;
        currentLengthInSamples = 0;

        durationLabel.setText("--:--", juce::dontSendNotification);
        infoLabel.setText("", juce::dontSendNotification);
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
        // Ensure the audio player is prepared
        if (!audioPlayerPrepared)
        {
            prepareAudioPlayer();
        }

        // Set up the audio chain
        audioSourcePlayer.setSource(&transportSource);
        audioSourcePlayer.setGain(volume);

        // Add callback if not already added
        deviceManager->addAudioCallback(&audioSourcePlayer);

        // If at the end, restart from beginning
        if (transportSource.getCurrentPosition() >= transportSource.getLengthInSeconds() - 0.1)
        {
            transportSource.setPosition(0);
        }

        transportSource.start();
        updateTransportState();
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
    updateTransportState();
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
    // Setup favorites manager first (needs to be available for listener registration)
    favoritesManager = std::make_unique<FavoritesManager>();
    favoritesManager->addListener(this);

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

    // Setup favorite toggle button (star)
    favoriteButton.setButtonText("*");
    favoriteButton.setTooltip("Add to favorites");
    favoriteButton.setClickingTogglesState(true);
    favoriteButton.setColour(juce::TextButton::buttonColourId, MidiSingLookAndFeel::backgroundMid);
    favoriteButton.setColour(juce::TextButton::buttonOnColourId, MidiSingLookAndFeel::accentColour);
    favoriteButton.onClick = [this]() {
        toggleCurrentDirectoryFavorite();
    };
    addAndMakeVisible(favoriteButton);

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

    // Setup favorites panel
    setupFavoritesPanel();

    // Setup preview component
    previewComponent = std::make_unique<AudioPreviewComponent>();
    addAndMakeVisible(previewComponent.get());

    // Update navigation state
    updateNavigationButtons();
    updateFavoriteButton();
}

FileBrowser::~FileBrowser()
{
    if (favoritesManager != nullptr)
        favoritesManager->removeListener(this);

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

void FileBrowser::setupFavoritesPanel()
{
    favoritesPanel = std::make_unique<FavoritesPanel>(*favoritesManager);
    favoritesPanel->onFavoriteSelected = [this](const juce::File& folder) {
        navigateTo(folder);
    };
    addAndMakeVisible(favoritesPanel.get());
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
    favoriteButton.setBounds(toolbar.removeFromLeft(btnWidth).reduced(btnMargin));

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

    // Favorites panel on the left (if visible and there are favorites)
    if (favoritesPanel != nullptr && favoritesPanelVisible && favoritesManager->getFavorites().size() > 0)
    {
        favoritesPanel->setVisible(true);
        favoritesPanel->setBounds(bounds.removeFromLeft(FAVORITES_PANEL_WIDTH).reduced(2));
    }
    else if (favoritesPanel != nullptr)
    {
        favoritesPanel->setVisible(false);
    }

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

        // If it's an audio file, load it in preview (but don't auto-play on selection change)
        if (isAudioFile(selectedFile))
        {
            // Temporarily disable auto-play for selection changes
            bool wasAutoPlay = previewComponent->getAutoPlay();
            previewComponent->setAutoPlay(false);
            previewComponent->setFile(selectedFile);
            previewComponent->setAutoPlay(wasAutoPlay);
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
        // Preview audio file on single click (with auto-play enabled)
        if (isAudioFile(file))
        {
            previewComponent->setAutoPlay(true);
            previewComponent->setFile(file);
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
    updateFavoriteButton();

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
    if (favoritesManager != nullptr)
    {
        favoritesManager->addFavorite(file);
    }
}

void FileBrowser::removeFromFavorites(const juce::File& file)
{
    if (favoritesManager != nullptr)
    {
        favoritesManager->removeFavorite(file);
    }
}

bool FileBrowser::isFavorite(const juce::File& file) const
{
    if (favoritesManager != nullptr)
    {
        return favoritesManager->isFavorite(file);
    }
    return false;
}

void FileBrowser::toggleCurrentDirectoryFavorite()
{
    if (!currentDirectory.exists())
        return;

    if (isFavorite(currentDirectory))
    {
        removeFromFavorites(currentDirectory);
    }
    else
    {
        addToFavorites(currentDirectory);
    }
    updateFavoriteButton();
}

juce::Array<juce::File> FileBrowser::getFavorites() const
{
    if (favoritesManager != nullptr)
    {
        return favoritesManager->getFavorites();
    }
    return {};
}

void FileBrowser::setFavoritesPanelVisible(bool shouldBeVisible)
{
    if (favoritesPanelVisible != shouldBeVisible)
    {
        favoritesPanelVisible = shouldBeVisible;
        resized();
    }
}

void FileBrowser::updateFavoriteButton()
{
    bool isFav = isFavorite(currentDirectory);
    favoriteButton.setToggleState(isFav, juce::dontSendNotification);
    favoriteButton.setTooltip(isFav ? "Remove from favorites" : "Add to favorites");
}

void FileBrowser::favoritesChanged()
{
    // Update the favorite button state
    updateFavoriteButton();

    // Trigger layout update in case favorites panel needs to show/hide
    resized();
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
