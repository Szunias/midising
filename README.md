# MidiSing

A professional Digital Audio Workstation (DAW) built with the JUCE framework for Windows. MidiSing provides a complete set of tools for audio and MIDI production, including multi-track recording, editing, mixing, VST3 plugin hosting, and export.

## Features

### Audio & Recording
- Multi-track audio and MIDI recording and playback
- Audio engine with dropout detection and monitoring
- Audio recording with input monitoring and latency compensation
- Time stretch and pitch shift processing
- Track freeze for CPU optimization

### Mixing
- Per-track faders, pan controls, mute/solo, and arm buttons
- Send/return routing with aux tracks
- Group busses for submixing
- Mix groups for linked faders
- Built-in effects: Parametric EQ, Reverb, VCA Compressor, Stereo Delay

### VST3 Plugins
- VST3 plugin hosting with crash-protected scanning
- Custom plugin search paths with rescan
- Plugin selector UI per track

### MIDI
- Piano roll editor with Draw, Erase, Select, Split, and Mute tools
- Quantization with snap-to-grid (quarter, eighth, sixteenth, triplets)
- Swing quantization
- MIDI transforms: transpose, velocity scaling, humanize, legato
- CC lane editing (Mod Wheel, Sustain, Pitch Bend, Expression)
- MIDI input from external devices with activity indicator
- Undo/redo integration for piano roll edits

### Automation
- Automation system with Read/Write/Touch/Latch modes
- Volume and pan automation lanes per track

### Export
- Export to WAV, FLAC, and OGG Vorbis formats
- Configurable sample rate (44.1/48/96 kHz) and bit depth (16/24/32-bit)
- Export cancellation support with progress dialog
- MIDI file export (.mid)

### Project Management
- Project save/load with "Collect All and Save" bundle support
- Auto-save with crash recovery
- Full undo/redo system with unlimited history

### UI
- Collapsible panels: File Browser, Mixer, Piano Roll
- Resizable splitters between panels
- Spectrum analyzer display
- Metronome with volume control
- Status bar with CPU, memory, dropouts, MIDI activity, auto-save indicator
- Custom dark theme
- Keyboard shortcuts for all major operations

## System Requirements

- Windows 10 or Windows 11
- Audio interface or built-in sound card (ASIO, WASAPI, or DirectSound)
- 4 GB RAM minimum (8 GB recommended)

## Build Requirements

- Visual Studio 2022 (C++17 support required)
- CMake 3.22 or later
- Internet connection for initial build (JUCE is fetched automatically via CMake FetchContent)

## Building

```bash
git clone <repository-url>
cd midising
cmake -B build -S .
cmake --build build --config Release
```

The built executable will be at `build/MidiSing_artefacts/Release/MidiSing.exe`.

### Building the Installer

```bash
cd build
cpack -C Release
```

This creates an NSIS installer for Windows distribution.

### Running Tests

```bash
cmake --build build --config Release --target MidiSingTests
build/MidiSingTests_artefacts/Release/MidiSingTests.exe
```

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| Ctrl+N | New Project |
| Ctrl+O | Open Project |
| Ctrl+S | Save Project |
| Ctrl+Shift+S | Save Project As |
| Ctrl+Z | Undo |
| Ctrl+Shift+Z | Redo |
| Space | Play/Stop |
| R | Record |
| Home | Return to Start |
| Ctrl+T | Add Audio Track |
| Ctrl+Shift+T | Add MIDI Track |
| B | Toggle File Browser |
| M | Toggle Mixer |
| P | Toggle Piano Roll |

### Piano Roll Shortcuts

| Shortcut | Action |
|---|---|
| D / 1 | Draw tool |
| E / 2 | Erase tool |
| S / 3 | Select tool |
| P / 4 | Split tool |
| M / 5 | Mute tool |
| Q | Quantize selected notes |
| G | Toggle snap-to-grid |
| Up/Down | Transpose semitone |
| Shift+Up/Down | Transpose octave |
| H | Humanize selected notes |
| L | Legato selected notes |
| Delete | Delete selected notes |
| Ctrl+A | Select all notes |

## License

MIT License - see [LICENSE](LICENSE) for details.
