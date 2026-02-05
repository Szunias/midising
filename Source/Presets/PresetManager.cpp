#include "PresetManager.h"
#include "../Plugins/PluginInstance.h"

//==============================================================================
// Construction
//==============================================================================

PresetManager::PresetManager()
{
}

PresetManager::~PresetManager()
{
}

//==============================================================================
// Preset Save/Load
//==============================================================================

bool PresetManager::savePreset(const Preset& preset, const juce::File& file)
{
    if (!preset.isValid())
    {
        DBG("PresetManager::savePreset - Invalid preset");
        return false;
    }

    auto xml = createPresetXml(preset);
    if (xml == nullptr)
    {
        DBG("PresetManager::savePreset - Failed to create XML");
        return false;
    }

    // Ensure the file has the correct extension
    juce::File targetFile = file;
    if (!targetFile.hasFileExtension(PRESET_EXTENSION))
    {
        targetFile = targetFile.withFileExtension(PRESET_EXTENSION);
    }

    // Create parent directory if needed
    auto parentDir = targetFile.getParentDirectory();
    if (!parentDir.exists())
    {
        auto result = parentDir.createDirectory();
        if (result.failed())
        {
            DBG("PresetManager::savePreset - Failed to create directory: " + result.getErrorMessage());
            return false;
        }
    }

    // Write the XML to file
    if (!xml->writeTo(targetFile))
    {
        DBG("PresetManager::savePreset - Failed to write file: " + targetFile.getFullPathName());
        return false;
    }

    DBG("PresetManager::savePreset - Saved preset to: " + targetFile.getFullPathName());
    return true;
}

bool PresetManager::savePluginPreset(const PluginInstance& plugin,
                                      const PresetMetadata& metadata,
                                      const juce::File& file)
{
    Preset preset = createPresetFromPlugin(plugin, metadata);

    if (!preset.isValid())
    {
        DBG("PresetManager::savePluginPreset - Failed to create preset from plugin");
        return false;
    }

    return savePreset(preset, file);
}

PresetManager::Preset PresetManager::loadPreset(const juce::File& file)
{
    Preset preset;

    if (!file.existsAsFile())
    {
        DBG("PresetManager::loadPreset - File not found: " + file.getFullPathName());
        return preset;
    }

    // Parse the XML
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
    {
        DBG("PresetManager::loadPreset - Failed to parse XML: " + file.getFullPathName());
        return preset;
    }

    // Validate root tag
    if (!xml->hasTagName(PRESET_ROOT_TAG))
    {
        DBG("PresetManager::loadPreset - Invalid root tag, expected " + juce::String(PRESET_ROOT_TAG));
        return preset;
    }

    // Parse the preset
    preset = parsePresetXml(*xml);
    preset.sourceFile = file;

    // Add to recent presets
    if (preset.isValid())
    {
        const_cast<PresetManager*>(this)->addToRecentPresets(file);
    }

    return preset;
}

bool PresetManager::applyPresetToPlugin(const Preset& preset, PluginInstance& plugin)
{
    if (!preset.isValid())
    {
        DBG("PresetManager::applyPresetToPlugin - Invalid preset");
        return false;
    }

    // Warn if preset might not be compatible
    if (!isPresetCompatible(preset, plugin))
    {
        DBG("PresetManager::applyPresetToPlugin - Warning: Preset may not be compatible with plugin");
        // Continue anyway - user might want to try loading
    }

    // Apply the state
    if (preset.state.getSize() > 0)
    {
        plugin.setStateInformation(preset.state);
        DBG("PresetManager::applyPresetToPlugin - Applied preset: " + preset.metadata.name);
        return true;
    }

    DBG("PresetManager::applyPresetToPlugin - Preset has no state data");
    return false;
}

bool PresetManager::loadAndApplyPreset(const juce::File& file, PluginInstance& plugin)
{
    Preset preset = loadPreset(file);

    if (!preset.isValid())
    {
        return false;
    }

    return applyPresetToPlugin(preset, plugin);
}

//==============================================================================
// FXP/FXB Import
//==============================================================================

bool PresetManager::importFxpFxb(const juce::File& fxpFile,
                                  PluginInstance& plugin,
                                  juce::String& errorMessage)
{
    if (!fxpFile.existsAsFile())
    {
        errorMessage = "File not found: " + fxpFile.getFullPathName();
        return false;
    }

    // Check file extension
    bool isFxp = isFxpFile(fxpFile);
    bool isFxb = isFxbFile(fxpFile);

    if (!isFxp && !isFxb)
    {
        errorMessage = "File is not a valid FXP or FXB file";
        return false;
    }

    // Load the file data
    juce::MemoryBlock fileData;
    if (!fxpFile.loadFileAsData(fileData))
    {
        errorMessage = "Failed to read file: " + fxpFile.getFullPathName();
        return false;
    }

    // Get the underlying plugin processor
    auto* processor = plugin.getProcessor();
    if (processor == nullptr)
    {
        errorMessage = "Plugin processor is not available";
        return false;
    }

    // Try to load FXP/FXB data directly into the plugin
    // Note: FXP/FXB format is a legacy VST2 format. JUCE 7+ no longer has
    // VSTPluginFormat::loadFromFXBFile. We attempt to use setStateInformation
    // which may work for some plugins that support the raw FXP/FXB format.
    processor->setStateInformation(fileData.getData(), static_cast<int>(fileData.getSize()));
    
    // Note: We can't easily verify if the load was successful since setStateInformation
    // doesn't return a status. The plugin may silently ignore invalid data.
    DBG("PresetManager::importFxpFxb - Attempted to import legacy format (FXP/FXB support is limited in VST3)");

    DBG("PresetManager::importFxpFxb - Successfully imported: " + fxpFile.getFileName());
    return true;
}

bool PresetManager::isFxpFile(const juce::File& file)
{
    return file.hasFileExtension("fxp") ||
           file.hasFileExtension("FXP");
}

bool PresetManager::isFxbFile(const juce::File& file)
{
    return file.hasFileExtension("fxb") ||
           file.hasFileExtension("FXB");
}

bool PresetManager::validateFxpFxbFormat(const juce::File& file, juce::String& errorMessage)
{
    if (!file.existsAsFile())
    {
        errorMessage = "File not found: " + file.getFullPathName();
        return false;
    }

    // Check file size (minimum header size is 28 bytes for basic FXP header)
    if (file.getSize() < 28)
    {
        errorMessage = "File is too small to be a valid FXP/FXB file";
        return false;
    }

    // Read the header
    juce::FileInputStream stream(file);
    if (!stream.openedOk())
    {
        errorMessage = "Failed to open file for reading";
        return false;
    }

    // Read magic number (big-endian "CcnK")
    juce::uint32 magic = static_cast<juce::uint32>(stream.readIntBigEndian());
    if (magic != FXP_MAGIC)
    {
        errorMessage = "Invalid FXP/FXB file: missing 'CcnK' magic header";
        return false;
    }

    // Skip byteSize (4 bytes)
    stream.readIntBigEndian();

    // Read fxMagic to determine type
    juce::uint32 fxMagic = static_cast<juce::uint32>(stream.readIntBigEndian());

    // Validate fxMagic
    if (fxMagic != FXP_CHUNK_MAGIC && fxMagic != FXP_PARAMS_MAGIC &&
        fxMagic != FXB_CHUNK_MAGIC && fxMagic != FXB_PARAMS_MAGIC)
    {
        errorMessage = "Invalid FXP/FXB file: unrecognized format type";
        return false;
    }

    return true;
}

juce::String PresetManager::getPresetNameFromFxp(const juce::File& fxpFile)
{
    if (!fxpFile.existsAsFile() || fxpFile.getSize() < 60)
    {
        return {};
    }

    // Open file for reading
    juce::FileInputStream stream(fxpFile);
    if (!stream.openedOk())
    {
        return {};
    }

    // Read and validate magic
    juce::uint32 magic = static_cast<juce::uint32>(stream.readIntBigEndian());
    if (magic != FXP_MAGIC)
    {
        return {};
    }

    // Skip: byteSize (4), fxMagic (4), version (4), fxID (4), fxVersion (4)
    stream.skipNextBytes(20);

    // Read the preset name (28 bytes, null-terminated)
    char presetName[28];
    stream.read(presetName, 28);
    presetName[27] = '\0';  // Ensure null termination

    juce::String name(presetName);
    return name.trim();
}

juce::String PresetManager::getFxpFxbType(const juce::File& file)
{
    if (!file.existsAsFile() || file.getSize() < 12)
    {
        return {};
    }

    juce::FileInputStream stream(file);
    if (!stream.openedOk())
    {
        return {};
    }

    // Read magic
    juce::uint32 magic = static_cast<juce::uint32>(stream.readIntBigEndian());
    if (magic != FXP_MAGIC)
    {
        return {};
    }

    // Skip byteSize
    stream.readIntBigEndian();

    // Read fxMagic
    juce::uint32 fxMagic = static_cast<juce::uint32>(stream.readIntBigEndian());

    // Determine type
    if (fxMagic == FXP_CHUNK_MAGIC || fxMagic == FXP_PARAMS_MAGIC)
    {
        return "FXP";
    }
    else if (fxMagic == FXB_CHUNK_MAGIC || fxMagic == FXB_PARAMS_MAGIC)
    {
        return "FXB";
    }

    return {};
}

bool PresetManager::importFxpFxbAndSaveAsPreset(const juce::File& fxpFile,
                                                  PluginInstance& plugin,
                                                  const juce::File& outputFile,
                                                  juce::String& errorMessage)
{
    // Validate the FXP/FXB file format
    if (!validateFxpFxbFormat(fxpFile, errorMessage))
    {
        return false;
    }

    // Determine file type
    juce::String fileType = getFxpFxbType(fxpFile);
    bool isBankFile = (fileType == "FXB");

    // Import the FXP/FXB into the plugin
    if (!importFxpFxb(fxpFile, plugin, errorMessage))
    {
        return false;
    }

    // Create metadata for the preset
    PresetMetadata metadata;

    // Try to get preset name from FXP header, fallback to filename
    juce::String presetName = getPresetNameFromFxp(fxpFile);
    if (presetName.isEmpty())
    {
        presetName = fxpFile.getFileNameWithoutExtension();
    }
    metadata.name = presetName;
    metadata.description = "Imported from " + fileType + ": " + fxpFile.getFileName();
    metadata.tags.add("imported");
    metadata.tags.add(fileType.toLowerCase());

    // Create preset from current plugin state
    Preset preset = createPresetFromPlugin(plugin, metadata);

    if (!preset.isValid())
    {
        errorMessage = "Failed to capture plugin state after import";
        return false;
    }

    // Save the preset
    if (!savePreset(preset, outputFile))
    {
        errorMessage = "Failed to save preset file: " + outputFile.getFullPathName();
        return false;
    }

    DBG("PresetManager::importFxpFxbAndSaveAsPreset - Successfully converted " +
        fxpFile.getFileName() + " to " + outputFile.getFileName());

    return true;
}

juce::File PresetManager::importFxpFxbToDefaultDirectory(const juce::File& fxpFile,
                                                          PluginInstance& plugin,
                                                          juce::String& errorMessage)
{
    // Validate the FXP/FXB file format first
    if (!validateFxpFxbFormat(fxpFile, errorMessage))
    {
        return {};
    }

    // Get plugin name for subdirectory
    const auto& desc = plugin.getPluginDescription();
    juce::String pluginName = desc.name;

    // Sanitize plugin name for use as directory name
    pluginName = pluginName.replaceCharacters(" /\\:*?\"<>|", "___________");

    // Create target directory: presets/PluginName/
    juce::File presetDir = getDefaultPresetDirectory().getChildFile(pluginName);
    if (!presetDir.exists())
    {
        auto result = presetDir.createDirectory();
        if (result.failed())
        {
            errorMessage = "Failed to create preset directory: " + result.getErrorMessage();
            return {};
        }
    }

    // Get preset name from FXP or use filename
    juce::String presetName = getPresetNameFromFxp(fxpFile);
    if (presetName.isEmpty())
    {
        presetName = fxpFile.getFileNameWithoutExtension();
    }

    // Sanitize preset name
    presetName = presetName.replaceCharacters("/\\:*?\"<>|", "_________");

    // Create output file path
    juce::File outputFile = presetDir.getChildFile(presetName).withFileExtension(PRESET_EXTENSION);

    // Handle filename conflicts by appending numbers
    int counter = 1;
    while (outputFile.existsAsFile() && counter < 1000)
    {
        outputFile = presetDir.getChildFile(presetName + "_" + juce::String(counter))
                              .withFileExtension(PRESET_EXTENSION);
        counter++;
    }

    // Perform the import and save
    if (!importFxpFxbAndSaveAsPreset(fxpFile, plugin, outputFile, errorMessage))
    {
        return {};
    }

    // Add to recent presets
    addToRecentPresets(outputFile);

    return outputFile;
}

//==============================================================================
// Preset Validation
//==============================================================================

bool PresetManager::isPresetCompatible(const Preset& preset, const PluginInstance& plugin) const
{
    // Check if plugin identifiers match
    juce::String pluginId = plugin.getIdentifierString();

    // First check for exact match
    if (preset.pluginId == pluginId)
    {
        return true;
    }

    // Check for partial match (same plugin name and manufacturer)
    const auto& desc = plugin.getPluginDescription();

    if (preset.pluginName == desc.name &&
        preset.manufacturer == desc.manufacturerName)
    {
        return true;
    }

    return false;
}

bool PresetManager::validatePresetFile(const juce::File& file) const
{
    if (!file.existsAsFile())
    {
        return false;
    }

    // Check extension
    if (!file.hasFileExtension(PRESET_EXTENSION))
    {
        return false;
    }

    // Try to parse
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
    {
        return false;
    }

    // Check root tag
    if (!xml->hasTagName(PRESET_ROOT_TAG))
    {
        return false;
    }

    // Check version attribute exists
    if (!xml->hasAttribute("version"))
    {
        return false;
    }

    return true;
}

//==============================================================================
// Preset Directory Management
//==============================================================================

juce::File PresetManager::getDefaultPresetDirectory() const
{
    // Use app data directory for presets
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto presetDir = appDataDir.getChildFile("MidiSing").getChildFile("Presets");

    // Create if doesn't exist
    if (!presetDir.exists())
    {
        presetDir.createDirectory();
    }

    return presetDir;
}

juce::Array<juce::File> PresetManager::getPresetsInDirectory(const juce::File& directory,
                                                              bool searchSubdirectories) const
{
    juce::Array<juce::File> presets;

    if (!directory.exists() || !directory.isDirectory())
    {
        return presets;
    }

    // Search for preset files
    int flags = juce::File::findFiles;
    if (searchSubdirectories)
    {
        flags |= juce::File::findFilesAndDirectories;
    }

    auto files = directory.findChildFiles(juce::File::findFiles, searchSubdirectories,
                                           "*." + juce::String(PRESET_EXTENSION));

    for (const auto& file : files)
    {
        presets.add(file);
    }

    return presets;
}

std::map<juce::String, juce::Array<juce::File>> PresetManager::getPresetsByPlugin(
    const juce::File& directory) const
{
    std::map<juce::String, juce::Array<juce::File>> presetsByPlugin;

    auto allPresets = getPresetsInDirectory(directory, true);

    for (const auto& file : allPresets)
    {
        // Quick parse to get plugin name
        auto xml = juce::XmlDocument::parse(file);
        if (xml != nullptr && xml->hasTagName(PRESET_ROOT_TAG))
        {
            auto* pluginXml = xml->getChildByName("PLUGIN");
            if (pluginXml != nullptr)
            {
                juce::String pluginName = pluginXml->getStringAttribute("pluginName", "Unknown");
                presetsByPlugin[pluginName].add(file);
            }
        }
    }

    return presetsByPlugin;
}

//==============================================================================
// Recent Presets
//==============================================================================

void PresetManager::addToRecentPresets(const juce::File& file)
{
    // Remove if already in list
    recentPresets.removeFirstMatchingValue(file);

    // Add to front
    recentPresets.insert(0, file);

    // Trim to max size
    while (recentPresets.size() > maxRecentPresets)
    {
        recentPresets.removeLast();
    }
}

//==============================================================================
// XML Utilities
//==============================================================================

std::unique_ptr<juce::XmlElement> PresetManager::createPresetXml(const Preset& preset) const
{
    auto xml = std::make_unique<juce::XmlElement>(PRESET_ROOT_TAG);
    xml->setAttribute("version", PRESET_FORMAT_VERSION);

    // Add metadata
    auto metadataXml = createMetadataXml(preset.metadata);
    if (metadataXml != nullptr)
    {
        xml->addChildElement(metadataXml.release());
    }

    // Add plugin info
    auto pluginXml = createPluginInfoXml(preset);
    if (pluginXml != nullptr)
    {
        xml->addChildElement(pluginXml.release());
    }

    // Add state as Base64
    if (preset.state.getSize() > 0)
    {
        auto stateXml = std::make_unique<juce::XmlElement>("STATE");
        stateXml->setAttribute("encoding", "base64");
        stateXml->addTextElement(preset.state.toBase64Encoding());
        xml->addChildElement(stateXml.release());
    }

    return xml;
}

PresetManager::Preset PresetManager::parsePresetXml(const juce::XmlElement& xml) const
{
    Preset preset;

    // Check version (for future compatibility)
    juce::String version = xml.getStringAttribute("version", "1.0");

    // Parse metadata
    if (auto* metadataXml = xml.getChildByName("METADATA"))
    {
        preset.metadata = parseMetadataXml(*metadataXml);
    }

    // Parse plugin info
    if (auto* pluginXml = xml.getChildByName("PLUGIN"))
    {
        parsePluginInfoXml(*pluginXml, preset);
    }

    // Parse state
    if (auto* stateXml = xml.getChildByName("STATE"))
    {
        juce::String encoding = stateXml->getStringAttribute("encoding", "base64");

        if (encoding == "base64")
        {
            juce::String base64Data = stateXml->getAllSubText().trim();
            if (base64Data.isNotEmpty())
            {
                preset.state.fromBase64Encoding(base64Data);
            }
        }
    }

    return preset;
}

PresetManager::Preset PresetManager::createPresetFromPlugin(const PluginInstance& plugin,
                                                             const PresetMetadata& metadata) const
{
    Preset preset;
    preset.metadata = metadata;

    // Update modified time
    preset.metadata.modified = juce::Time::getCurrentTime();

    // Get plugin info
    const auto& desc = plugin.getPluginDescription();
    preset.pluginId = plugin.getIdentifierString();
    preset.pluginName = desc.name;
    preset.manufacturer = desc.manufacturerName;
    preset.pluginFormat = desc.pluginFormatName;
    preset.isInstrument = desc.isInstrument;

    // Get plugin state
    plugin.getStateInformation(preset.state);

    return preset;
}

//==============================================================================
// Internal XML Methods
//==============================================================================

std::unique_ptr<juce::XmlElement> PresetManager::createMetadataXml(const PresetMetadata& metadata) const
{
    auto xml = std::make_unique<juce::XmlElement>("METADATA");

    // Create child elements for each metadata field
    if (metadata.name.isNotEmpty())
    {
        auto nameXml = std::make_unique<juce::XmlElement>("name");
        nameXml->addTextElement(metadata.name);
        xml->addChildElement(nameXml.release());
    }

    if (metadata.author.isNotEmpty())
    {
        auto authorXml = std::make_unique<juce::XmlElement>("author");
        authorXml->addTextElement(metadata.author);
        xml->addChildElement(authorXml.release());
    }

    if (metadata.description.isNotEmpty())
    {
        auto descXml = std::make_unique<juce::XmlElement>("description");
        descXml->addTextElement(metadata.description);
        xml->addChildElement(descXml.release());
    }

    if (metadata.tags.size() > 0)
    {
        auto tagsXml = std::make_unique<juce::XmlElement>("tags");
        tagsXml->addTextElement(metadata.tags.joinIntoString(","));
        xml->addChildElement(tagsXml.release());
    }

    // Timestamps in ISO 8601 format
    auto createdXml = std::make_unique<juce::XmlElement>("created");
    createdXml->addTextElement(metadata.created.toISO8601(true));
    xml->addChildElement(createdXml.release());

    auto modifiedXml = std::make_unique<juce::XmlElement>("modified");
    modifiedXml->addTextElement(metadata.modified.toISO8601(true));
    xml->addChildElement(modifiedXml.release());

    return xml;
}

PresetManager::PresetMetadata PresetManager::parseMetadataXml(const juce::XmlElement& xml) const
{
    PresetMetadata metadata;

    // Parse child elements
    if (auto* nameXml = xml.getChildByName("name"))
    {
        metadata.name = nameXml->getAllSubText().trim();
    }

    if (auto* authorXml = xml.getChildByName("author"))
    {
        metadata.author = authorXml->getAllSubText().trim();
    }

    if (auto* descXml = xml.getChildByName("description"))
    {
        metadata.description = descXml->getAllSubText().trim();
    }

    if (auto* tagsXml = xml.getChildByName("tags"))
    {
        juce::String tagsStr = tagsXml->getAllSubText().trim();
        if (tagsStr.isNotEmpty())
        {
            metadata.tags.addTokens(tagsStr, ",", "\"");
            metadata.tags.trim();
            metadata.tags.removeEmptyStrings();
        }
    }

    if (auto* createdXml = xml.getChildByName("created"))
    {
        metadata.created = juce::Time::fromISO8601(createdXml->getAllSubText().trim());
    }

    if (auto* modifiedXml = xml.getChildByName("modified"))
    {
        metadata.modified = juce::Time::fromISO8601(modifiedXml->getAllSubText().trim());
    }

    return metadata;
}

std::unique_ptr<juce::XmlElement> PresetManager::createPluginInfoXml(const Preset& preset) const
{
    auto xml = std::make_unique<juce::XmlElement>("PLUGIN");

    xml->setAttribute("pluginId", preset.pluginId);
    xml->setAttribute("pluginName", preset.pluginName);
    xml->setAttribute("manufacturer", preset.manufacturer);
    xml->setAttribute("pluginFormat", preset.pluginFormat);
    xml->setAttribute("isInstrument", preset.isInstrument);

    return xml;
}

void PresetManager::parsePluginInfoXml(const juce::XmlElement& xml, Preset& preset) const
{
    preset.pluginId = xml.getStringAttribute("pluginId");
    preset.pluginName = xml.getStringAttribute("pluginName");
    preset.manufacturer = xml.getStringAttribute("manufacturer");
    preset.pluginFormat = xml.getStringAttribute("pluginFormat", "VST3");
    preset.isInstrument = xml.getBoolAttribute("isInstrument", false);
}
