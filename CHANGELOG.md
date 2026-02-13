# Changelog

## v1.0.0

### Core Features
- Multi-track audio and MIDI recording and playback
- VST3 plugin hosting with crash-protected scanning
- Built-in effects: Parametric EQ, Reverb, VCA Compressor, Stereo Delay
- Automation system with Read/Write/Touch/Latch modes
- Full undo/redo system with unlimited history
- Project save/load with "Collect All and Save" bundle support
- Auto-save with crash recovery
- Metronome with volume control

### Audio
- Audio engine with dropout detection and monitoring
- Audio recording with input monitoring and latency compensation
- Time stretch and pitch shift processing
- Send/return routing with aux tracks
- Group busses for submixing
- Mix groups for linked faders
- Track freeze for CPU optimization

### Export
- Export to WAV, FLAC, and OGG Vorbis formats
- Configurable sample rate (44.1/48/96 kHz) and bit depth (16/24/32-bit)
- Export cancellation support with progress dialog
- MIDI file export (.mid)

### MIDI
- Piano roll editor with Draw, Erase, Select, Split, and Mute tools
- Quantization with snap-to-grid (quarter, eighth, sixteenth, triplets)
- Swing quantization
- MIDI transforms: transpose, velocity scaling, humanize, legato
- CC lane editing (Mod Wheel, Sustain, Pitch Bend, Expression)
- MIDI input from external devices with activity indicator
- Undo/redo integration for piano roll edits

### UI
- Collapsible panels: File Browser, Mixer, Piano Roll
- Resizable splitters between panels
- Spectrum analyzer display
- Status bar with CPU, memory, dropouts, MIDI activity, auto-save indicator
- Custom dark theme look and feel
- Keyboard shortcuts for all major operations
- Help menu with About dialog and keyboard shortcuts reference

### Settings
- Audio device settings with buffer size and sample rate selection
- Persistent audio device settings across restarts
- MIDI input device selection
- Custom VST plugin search paths with rescan
- Configurable recording folder
- Configurable auto-save interval
- Metronome pre-count bars setting

### Distribution
- Windows installer via NSIS
- Standalone test runner executable
