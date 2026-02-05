#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

// Forward declarations
class PluginInstance;

/**
 * Manages plugin presets in the .midising-preset format.
 *
 * Features:
 * - Save/load presets in custom .midising-preset XML format
 * - Import FXP/FXB preset files from VST2
 * - Export presets for sharing
 * - Recent presets list
 * - Preset metadata (name, author, description, tags)
 *
 * File Format (.midising-preset):
 * ```xml
 * <MIDISING_PRESET version="1.0">
 *   <METADATA>
 *     <name>My Preset</name>
 *     <author>User</author>
 *     <description>A great preset</description>
 *     <tags>bass,synth,warm</tags>
 *     <created>2024-01-15T10:30:00</created>
 *     <modified>2024-01-15T12:00:00</modified>
 *   </METADATA>
 *   <PLUGIN>
 *     <pluginId>VST3-Manufacturer-PluginName-UID</pluginId>
 *     <pluginName>Plugin Name</pluginName>
 *     <manufacturer>Manufacturer</manufacturer>
 *     <pluginFormat>VST3</pluginFormat>
 *     <isInstrument>false</isInstrument>
 *   </PLUGIN>
 *   <STATE encoding="base64">Base64EncodedPluginState</STATE>
 * </MIDISING_PRESET>
 * ```
 */
class PresetManager
{
public:
    //==========================================================================
    // Constants
    //==========================================================================

    /** File extension for MidiSing presets (without dot) */
    static constexpr const char* PRESET_EXTENSION = "midising-preset";

    /** File extension with dot for convenience */
    static constexpr const char* PRESET_EXTENSION_WITH_DOT = ".midising-preset";

    /** Current preset format version */
    static constexpr const char* PRESET_FORMAT_VERSION = "1.0";

    /** Root XML tag for preset files */
    static constexpr const char* PRESET_ROOT_TAG = "MIDISING_PRESET";

    /** FXP/FXB format constants */
    static constexpr juce::uint32 FXP_MAGIC = 0x4B6E6343;  // "CcnK" in big-endian
    static constexpr juce::uint32 FXP_CHUNK_MAGIC = 0x68435046;  // "FPCh" - FXP opaque chunk
    static constexpr juce::uint32 FXP_PARAMS_MAGIC = 0x6B437846;  // "FxCk" - FXP parameters
    static constexpr juce::uint32 FXB_CHUNK_MAGIC = 0x68434246;  // "FBCh" - FXB opaque chunk
    static constexpr juce::uint32 FXB_PARAMS_MAGIC = 0x6B427846;  // "FxBk" - FXB parameters

    //==========================================================================
    // Preset Metadata
    //==========================================================================

    /**
     * Structure holding preset metadata.
     */
    struct PresetMetadata
    {
        juce::String name;              ///< Display name of the preset
        juce::String author;            ///< Author/creator name
        juce::String description;       ///< Optional description
        juce::StringArray tags;         ///< Tags for categorization
        juce::Time created;             ///< Creation timestamp
        juce::Time modified;            ///< Last modification timestamp

        /** Default constructor with current time */
        PresetMetadata()
            : created(juce::Time::getCurrentTime())
            , modified(juce::Time::getCurrentTime())
        {}

        /** Check if metadata is valid (has at least a name) */
        bool isValid() const { return name.isNotEmpty(); }
    };

    /**
     * Structure representing a full preset including state data.
     */
    struct Preset
    {
        PresetMetadata metadata;                ///< Preset metadata
        juce::String pluginId;                  ///< Plugin identifier string
        juce::String pluginName;                ///< Plugin display name
        juce::String manufacturer;              ///< Plugin manufacturer
        juce::String pluginFormat;              ///< Plugin format (VST3, AU, etc.)
        bool isInstrument = false;              ///< Whether plugin is an instrument
        juce::MemoryBlock state;                ///< Plugin state as binary data
        juce::File sourceFile;                  ///< Source file (if loaded from file)

        /** Check if preset is valid */
        bool isValid() const
        {
            return metadata.isValid() &&
                   pluginId.isNotEmpty() &&
                   state.getSize() > 0;
        }
    };

    //==========================================================================
    // Construction
    //==========================================================================

    PresetManager();
    ~PresetManager();

    // Non-copyable
    PresetManager(const PresetManager&) = delete;
    PresetManager& operator=(const PresetManager&) = delete;

    //==========================================================================
    // Preset Save/Load
    //==========================================================================

    /**
     * Save a preset to a .midising-preset file.
     * @param preset The preset to save
     * @param file The target file
     * @return true if successful
     */
    bool savePreset(const Preset& preset, const juce::File& file);

    /**
     * Save a PluginInstance's current state as a preset file.
     * @param plugin The plugin to save state from
     * @param metadata The preset metadata
     * @param file The target file
     * @return true if successful
     */
    bool savePluginPreset(const PluginInstance& plugin,
                          const PresetMetadata& metadata,
                          const juce::File& file);

    /**
     * Load a preset from a .midising-preset file.
     * @param file The preset file to load
     * @return The loaded preset, or empty preset on failure
     */
    Preset loadPreset(const juce::File& file);

    /**
     * Apply a preset to a PluginInstance.
     * Note: The preset must be compatible with the plugin.
     * @param preset The preset to apply
     * @param plugin The plugin to apply the preset to
     * @return true if successful
     */
    bool applyPresetToPlugin(const Preset& preset, PluginInstance& plugin);

    /**
     * Load and apply a preset file to a plugin in one operation.
     * @param file The preset file
     * @param plugin The plugin to apply the preset to
     * @return true if successful
     */
    bool loadAndApplyPreset(const juce::File& file, PluginInstance& plugin);

    //==========================================================================
    // FXP/FXB Import
    //==========================================================================

    /**
     * Import a VST2 FXP (single preset) or FXB (bank) file.
     * Creates a .midising-preset file with the imported state.
     *
     * @param fxpFile The FXP/FXB file to import
     * @param plugin The plugin to import to (must be loaded)
     * @param errorMessage Output parameter for error messages
     * @return true if successful
     */
    bool importFxpFxb(const juce::File& fxpFile,
                      PluginInstance& plugin,
                      juce::String& errorMessage);

    /**
     * Check if a file is an FXP preset file.
     * @param file The file to check
     * @return true if it appears to be an FXP file
     */
    static bool isFxpFile(const juce::File& file);

    /**
     * Check if a file is an FXB bank file.
     * @param file The file to check
     * @return true if it appears to be an FXB file
     */
    static bool isFxbFile(const juce::File& file);

    /**
     * Validate FXP/FXB file format by checking magic bytes.
     * @param file The file to validate
     * @param errorMessage Output parameter for validation error
     * @return true if file has valid FXP/FXB format
     */
    static bool validateFxpFxbFormat(const juce::File& file, juce::String& errorMessage);

    /**
     * Extract preset name from FXP file header.
     * @param fxpFile The FXP file to read
     * @return The preset name, or empty string if not found
     */
    static juce::String getPresetNameFromFxp(const juce::File& fxpFile);

    /**
     * Import FXP/FXB and save as .midising-preset file.
     * Loads the preset into the plugin, captures the state, and saves to internal format.
     *
     * @param fxpFile The FXP/FXB file to import
     * @param plugin The plugin to import to (must be loaded)
     * @param outputFile The output .midising-preset file
     * @param errorMessage Output parameter for error messages
     * @return true if successful
     */
    bool importFxpFxbAndSaveAsPreset(const juce::File& fxpFile,
                                      PluginInstance& plugin,
                                      const juce::File& outputFile,
                                      juce::String& errorMessage);

    /**
     * Import FXP/FXB and save to default presets directory.
     * Uses filename and extracted preset name for metadata.
     *
     * @param fxpFile The FXP/FXB file to import
     * @param plugin The plugin to import to (must be loaded)
     * @param errorMessage Output parameter for error messages
     * @return The created preset file, or invalid File on failure
     */
    juce::File importFxpFxbToDefaultDirectory(const juce::File& fxpFile,
                                               PluginInstance& plugin,
                                               juce::String& errorMessage);

    /**
     * Get the FXP/FXB file type.
     * @param file The file to check
     * @return "FXP" for single preset, "FXB" for bank, empty string if invalid
     */
    static juce::String getFxpFxbType(const juce::File& file);

    //==========================================================================
    // Preset Validation
    //==========================================================================

    /**
     * Check if a preset is compatible with a plugin.
     * Compares plugin identifiers to ensure compatibility.
     * @param preset The preset to check
     * @param plugin The plugin to check against
     * @return true if compatible
     */
    bool isPresetCompatible(const Preset& preset, const PluginInstance& plugin) const;

    /**
     * Validate a preset file without loading it.
     * @param file The preset file to validate
     * @return true if the file is a valid .midising-preset
     */
    bool validatePresetFile(const juce::File& file) const;

    //==========================================================================
    // Preset Directory Management
    //==========================================================================

    /**
     * Get the default preset directory for the application.
     * Creates the directory if it doesn't exist.
     * @return The default preset directory
     */
    juce::File getDefaultPresetDirectory() const;

    /**
     * Get all preset files in a directory.
     * @param directory The directory to search
     * @param searchSubdirectories Whether to search subdirectories
     * @return Array of preset files found
     */
    juce::Array<juce::File> getPresetsInDirectory(const juce::File& directory,
                                                   bool searchSubdirectories = true) const;

    /**
     * Get presets organized by plugin.
     * @param directory The directory to search
     * @return Map of plugin name to preset files
     */
    std::map<juce::String, juce::Array<juce::File>> getPresetsByPlugin(
        const juce::File& directory) const;

    //==========================================================================
    // Recent Presets
    //==========================================================================

    /**
     * Add a preset file to the recent list.
     * @param file The preset file to add
     */
    void addToRecentPresets(const juce::File& file);

    /**
     * Get the list of recent preset files.
     * @return Array of recent preset files
     */
    const juce::Array<juce::File>& getRecentPresets() const { return recentPresets; }

    /**
     * Clear the recent presets list.
     */
    void clearRecentPresets() { recentPresets.clear(); }

    /**
     * Set the maximum number of recent presets to track.
     * @param maxCount Maximum number of recent presets
     */
    void setMaxRecentPresets(int maxCount) { maxRecentPresets = maxCount; }

    //==========================================================================
    // XML Utilities
    //==========================================================================

    /**
     * Create an XML element from a preset.
     * @param preset The preset to serialize
     * @return XML element containing the preset data
     */
    std::unique_ptr<juce::XmlElement> createPresetXml(const Preset& preset) const;

    /**
     * Parse a preset from an XML element.
     * @param xml The XML element to parse
     * @return The parsed preset
     */
    Preset parsePresetXml(const juce::XmlElement& xml) const;

    /**
     * Create a Preset from a PluginInstance.
     * @param plugin The plugin to create preset from
     * @param metadata The preset metadata
     * @return The created preset
     */
    Preset createPresetFromPlugin(const PluginInstance& plugin,
                                  const PresetMetadata& metadata) const;

private:
    //==========================================================================
    // Member Variables
    //==========================================================================

    /** List of recently used preset files */
    juce::Array<juce::File> recentPresets;

    /** Maximum number of recent presets to track */
    int maxRecentPresets = 10;

    //==========================================================================
    // Internal Methods
    //==========================================================================

    /**
     * Create XML element for preset metadata.
     */
    std::unique_ptr<juce::XmlElement> createMetadataXml(const PresetMetadata& metadata) const;

    /**
     * Parse preset metadata from XML element.
     */
    PresetMetadata parseMetadataXml(const juce::XmlElement& xml) const;

    /**
     * Create XML element for plugin info.
     */
    std::unique_ptr<juce::XmlElement> createPluginInfoXml(const Preset& preset) const;

    /**
     * Parse plugin info from XML element.
     */
    void parsePluginInfoXml(const juce::XmlElement& xml, Preset& preset) const;

    JUCE_LEAK_DETECTOR(PresetManager)
};
