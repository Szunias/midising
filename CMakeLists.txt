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
        # Timeline
        Source/Timeline/Track.h
        Source/Timeline/Track.cpp
        Source/Timeline/Region.h
        Source/Timeline/Region.cpp
        Source/Timeline/Timeline.h
        Source/Timeline/Timeline.cpp
        Source/Timeline/AutomationLane.h
        Source/Timeline/AutomationLane.cpp
        # Audio
        Source/Audio/Transport.h
        Source/Audio/Mixer.h
        Source/Audio/SendReturn.h
        Source/Audio/SendReturn.cpp
        Source/Audio/GroupBus.h
        Source/Audio/GroupBus.cpp
        Source/Audio/AudioEngine.h
        Source/Audio/AudioEngine.cpp
        Source/Audio/AudioTrack.h
        Source/Audio/AudioTrack.cpp
        Source/Audio/Metronome.h
        Source/Audio/Metronome.cpp
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
        # UI
        Source/UI/LookAndFeel.h
        Source/UI/TransportBar.h
        Source/UI/TransportBar.cpp
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
        Source/Audio/SpectrumAnalyzer.h
        Source/Audio/SpectrumAnalyzer.cpp
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
        Source/UI/CollapsiblePanel.h
        Source/UI/BrowserPanel.h
        Source/UI/FileBrowser.h
        Source/UI/FileBrowser.cpp
        Source/UI/ResizableSplitter.h
        # Tests
        Source/Tests/TimeConversionTest.h
        Source/Tests/TimelineTest.h
        Source/Tests/TestRunner.h
)

# JUCE modules
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
