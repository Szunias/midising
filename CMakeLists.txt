cmake_minimum_required(VERSION 3.22)

project(MidiSing VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE from GitHub
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

# Create the GUI application
juce_add_gui_app(MidiSing
    PRODUCT_NAME "MidiSing"
    COMPANY_NAME "MidiSing"
    BUNDLE_ID "com.midising.app"
    VERSION "1.0.0"
)

# Source files
target_sources(MidiSing
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/MainComponent.h
        # Utils
        Source/Utils/TimeConversion.h
        Source/Utils/RecentFilesManager.h
        Source/Utils/RecentFilesManager.cpp
        Source/Utils/AppSettings.h
        Source/Utils/AppSettings.cpp
        # Timeline
        Source/Timeline/Track.h
        Source/Timeline/Track.cpp
        Source/Timeline/Region.h
        Source/Timeline/Region.cpp
        Source/Timeline/Timeline.h
        Source/Timeline/Timeline.cpp
        Source/Timeline/AutomationLane.h
        Source/Timeline/AutomationLane.cpp
        Source/Timeline/TempoTrack.h
        # Audio
        Source/Audio/Transport.h
        Source/Audio/Mixer.h
        Source/Audio/SendReturn.h
        Source/Audio/SendReturn.cpp
        Source/Audio/GroupBus.h
        Source/Audio/GroupBus.cpp
        Source/Audio/MixGroup.h
        Source/Audio/MixGroup.cpp
        Source/Audio/AudioEngine.h
        Source/Audio/AudioEngine.cpp
        Source/Audio/AudioTrack.h
        Source/Audio/AudioTrack.cpp
        Source/Audio/Metronome.h
        Source/Audio/Metronome.cpp
        Source/Audio/TrackFreezer.h
        Source/Audio/TrackFreezer.cpp
        # MIDI
        Source/MIDI/SimpleSynthSound.h
        Source/MIDI/SimpleSynthVoice.h
        Source/MIDI/MidiEngine.h
        Source/MIDI/MidiTrack.h
        Source/MIDI/MidiTrack.cpp
        Source/MIDI/MidiImporter.h
        Source/MIDI/MidiImporter.cpp
        Source/MIDI/PluginHost.h
        Source/MIDI/PluginHost.cpp
        # Plugins (VST3 hosting)
        Source/Plugins/PluginInstance.h
        Source/Plugins/PluginInstance.cpp
        Source/Plugins/PluginManager.h
        Source/Plugins/PluginManager.cpp
        # Presets
        Source/Presets/PresetManager.h
        Source/Presets/PresetManager.cpp
        # UI
        Source/UI/LookAndFeel.h
        Source/UI/CommandIDs.h
        Source/UI/TransportBar.h
        Source/UI/TransportBar.cpp
        Source/UI/ToolBar.h
        Source/UI/ToolBar.cpp
        Source/UI/Cursors.h
        Source/UI/TrackHeader.h
        Source/UI/TrackHeader.cpp
        Source/UI/TimelineView.h
        Source/UI/TimelineView.cpp
        Source/UI/ChannelStrip.h
        Source/UI/ChannelStrip.cpp
        Source/UI/MixerPanel.h
        Source/UI/MixerPanel.cpp
        Source/UI/PianoRoll.h
        Source/UI/PianoRoll.cpp
        Source/UI/SettingsPanel.h
        Source/UI/SettingsPanel.cpp
        Source/UI/StatusBar.h
        Source/UI/StatusBar.cpp
        Source/UI/SpectrumDisplay.h
        Source/UI/SpectrumDisplay.cpp
        Source/UI/AutomationLaneView.h
        Source/UI/AutomationLaneView.cpp
        Source/Audio/SpectrumAnalyzer.h
        Source/Audio/SpectrumAnalyzer.cpp
        Source/Audio/TimeStretch.h
        Source/Audio/TimeStretch.cpp
        Source/Audio/PitchShift.h
        Source/Audio/PitchShift.cpp
        # Audio Recording
        Source/Audio/AudioRecorder.h
        Source/Audio/AudioRecorder.cpp
        Source/Audio/AudioImporter.h
        Source/Audio/AudioImporter.cpp
        # Export
        Source/Export/MidiExporter.h
        Source/Export/MidiExporter.cpp
        Source/Export/AudioExporter.h
        Source/Export/AudioExporter.cpp
        Source/Audio/WaveformCache.h
        Source/Audio/WaveformCache.cpp
        # Serialization
        Source/Serialization/ProjectSerializer.h
        Source/Serialization/ProjectSerializer.cpp
        # Undo/Redo
        Source/Utils/UndoManager.h
        Source/Actions/TrackActions.h
        Source/Actions/MixerActions.h
        Source/Actions/PianoRollActions.h
        # Effects
        Source/Effects/Effect.h
        Source/Effects/EffectChain.h
        Source/Effects/EffectChain.cpp
        Source/Effects/GainEffect.h
        Source/Effects/SimpleEQEffect.h
        Source/Effects/SimpleEQEffect.cpp
        Source/Effects/ReverbEffect.h
        Source/Effects/ReverbEffect.cpp
        Source/Effects/ParametricEQ.h
        Source/Effects/ParametricEQ.cpp
        Source/Effects/VCACompressor.h
        Source/Effects/VCACompressor.cpp
        Source/Effects/StereoDelay.h
        Source/Effects/StereoDelay.cpp
        Source/UI/EffectRackUI.h
        Source/UI/EffectRackUI.cpp
        Source/UI/PluginSlot.h
        Source/UI/PluginSlot.cpp
        Source/UI/PluginWindow.h
        Source/UI/PluginWindow.cpp
        Source/UI/CollapsiblePanel.h
        Source/UI/BrowserPanel.h
        Source/UI/FileBrowser.h
        Source/UI/FileBrowser.cpp
        Source/UI/ResizableSplitter.h
        # Tests (headers only - included for IDE navigation)
        Source/Tests/TimeConversionTest.h
        Source/Tests/TimelineTest.h
        Source/Tests/TestRunner.h
)

# JUCE compile definitions
target_compile_definitions(MidiSing
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MidiSing,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MidiSing,JUCE_VERSION>"
)

target_link_libraries(MidiSing
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

#==============================================================================
# Standalone Test Runner
#==============================================================================

juce_add_console_app(MidiSingTests
    PRODUCT_NAME "MidiSing Tests"
    VERSION "1.0.0"
)

target_sources(MidiSingTests
    PRIVATE
        Source/Tests/TestMain.cpp
        # All source files needed by tests (same as main app minus Main.cpp and UI)
        Source/Utils/TimeConversion.h
        Source/Utils/RecentFilesManager.h
        Source/Utils/RecentFilesManager.cpp
        Source/Utils/AppSettings.h
        Source/Utils/AppSettings.cpp
        Source/Timeline/Track.h
        Source/Timeline/Track.cpp
        Source/Timeline/Region.h
        Source/Timeline/Region.cpp
        Source/Timeline/Timeline.h
        Source/Timeline/Timeline.cpp
        Source/Timeline/AutomationLane.h
        Source/Timeline/AutomationLane.cpp
        Source/Timeline/TempoTrack.h
        Source/Audio/Transport.h
        Source/Audio/Mixer.h
        Source/Audio/SendReturn.h
        Source/Audio/SendReturn.cpp
        Source/Audio/GroupBus.h
        Source/Audio/GroupBus.cpp
        Source/Audio/MixGroup.h
        Source/Audio/MixGroup.cpp
        Source/Audio/AudioEngine.h
        Source/Audio/AudioEngine.cpp
        Source/Audio/AudioTrack.h
        Source/Audio/AudioTrack.cpp
        Source/Audio/Metronome.h
        Source/Audio/Metronome.cpp
        Source/Audio/TrackFreezer.h
        Source/Audio/TrackFreezer.cpp
        Source/MIDI/SimpleSynthSound.h
        Source/MIDI/SimpleSynthVoice.h
        Source/MIDI/MidiEngine.h
        Source/MIDI/MidiTrack.h
        Source/MIDI/MidiTrack.cpp
        Source/MIDI/MidiImporter.h
        Source/MIDI/MidiImporter.cpp
        Source/MIDI/PluginHost.h
        Source/MIDI/PluginHost.cpp
        Source/Plugins/PluginInstance.h
        Source/Plugins/PluginInstance.cpp
        Source/Plugins/PluginManager.h
        Source/Plugins/PluginManager.cpp
        Source/Presets/PresetManager.h
        Source/Presets/PresetManager.cpp
        Source/Export/MidiExporter.h
        Source/Export/MidiExporter.cpp
        Source/Export/AudioExporter.h
        Source/Export/AudioExporter.cpp
        Source/Audio/WaveformCache.h
        Source/Audio/WaveformCache.cpp
        Source/Audio/SpectrumAnalyzer.h
        Source/Audio/SpectrumAnalyzer.cpp
        Source/Audio/TimeStretch.h
        Source/Audio/TimeStretch.cpp
        Source/Audio/PitchShift.h
        Source/Audio/PitchShift.cpp
        Source/Audio/AudioRecorder.h
        Source/Audio/AudioRecorder.cpp
        Source/Audio/AudioImporter.h
        Source/Audio/AudioImporter.cpp
        Source/Serialization/ProjectSerializer.h
        Source/Serialization/ProjectSerializer.cpp
        Source/Utils/UndoManager.h
        Source/Actions/TrackActions.h
        Source/Actions/MixerActions.h
        Source/Actions/PianoRollActions.h
        Source/Effects/Effect.h
        Source/Effects/EffectChain.h
        Source/Effects/EffectChain.cpp
        Source/Effects/GainEffect.h
        Source/Effects/SimpleEQEffect.h
        Source/Effects/SimpleEQEffect.cpp
        Source/Effects/ReverbEffect.h
        Source/Effects/ReverbEffect.cpp
        Source/Effects/ParametricEQ.h
        Source/Effects/ParametricEQ.cpp
        Source/Effects/VCACompressor.h
        Source/Effects/VCACompressor.cpp
        Source/Effects/StereoDelay.h
        Source/Effects/StereoDelay.cpp
        # UI files needed by tests
        Source/UI/PianoRoll.h
        Source/UI/PianoRoll.cpp
        Source/UI/PluginWindow.h
        Source/UI/PluginWindow.cpp
        Source/UI/LookAndFeel.h
        Source/Actions/PianoRollActions.h
        # Test files
        Source/Tests/TestRunner.h
        Source/Tests/TimeConversionTest.h
        Source/Tests/TimelineTest.h
        Source/Tests/VST3WorkflowTest.h
        Source/Tests/MixerRoutingTest.h
        Source/Tests/ProjectSaveLoadTest.h
        Source/Tests/AudioEngineTest.h
        Source/Tests/UndoTests.h
        Source/Tests/AutomationTests.h
        Source/Tests/MidiTests.h
        Source/Tests/TimeStretchTests.h
        Source/Tests/AutomationE2ETest.h
        Source/Tests/MidiCCE2ETest.h
        Source/Tests/UndoRedoE2ETest.h
        Source/Tests/TimeStretchE2ETest.h
        Source/Tests/PerformanceVerificationTest.h
)

target_compile_definitions(MidiSingTests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_APPLICATION_NAME_STRING="MidiSing Tests"
        JUCE_APPLICATION_VERSION_STRING="1.0.0"
)

target_link_libraries(MidiSingTests
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

#==============================================================================
# CPack / NSIS Installer Configuration (Windows)
#==============================================================================

set(CPACK_PACKAGE_NAME "MidiSing")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "MidiSing")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MidiSing - Digital Audio Workstation")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "MidiSing")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "MidiSing v${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME "MidiSing")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/Resources/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/Resources/icon.ico")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\MidiSing.lnk' '$INSTDIR\\\\bin\\\\MidiSing.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\MidiSing.lnk'"
    )

    # Include MSVC runtime redistributable
    include(InstallRequiredSystemLibraries)
endif()

install(TARGETS MidiSing
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

include(CPack)
