# MidiSing

A professional Digital Audio Workstation (DAW) built with JUCE framework for Windows. MidiSing provides essential tools for audio and MIDI production, including multi-track recording, editing, mixing, and VST3 plugin support.

## Features

- **Multi-track Timeline** - Audio and MIDI tracks with drag-and-drop region editing
- **Professional Mixer** - Per-track faders, pan controls, mute/solo, and send effects
- **VST3 Plugin Support** - Load and use third-party VST3 instruments and effects
- **Recording** - Multi-channel audio input with input monitoring and latency compensation
- **Editing Tools** - Select, Draw, Split, Erase, Pan, Zoom, Automation, and Range tools
- **MIDI Editing** - Piano roll editor with velocity editing and note manipulation
- **Effects** - Built-in EQ, Compressor, Delay, and Reverb effects
- **Project Management** - Save/load projects with bundled audio files and auto-save
- **Audio Export** - Render projects to WAV format with progress indication

## Requirements

- Windows 10/11
- Visual Studio 2022
- CMake 3.21+

## Building

```bash
cd midising
mkdir build
cmake -B build -S .
cmake --build build --config Release
```

## Usage

The built executable will be located at `build/MidiSing_artefacts/Release/MidiSing.exe`.

## License

This project is for educational and personal use.
