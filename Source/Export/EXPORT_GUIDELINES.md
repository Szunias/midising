# Audio Export Guidelines

## Metronome Exclusion from Exports

The metronome is designed for monitoring and practice purposes only and should **never** be included in exported audio files.

### Implementation Approach

When implementing AudioExporter, use one of these approaches to exclude the metronome:

#### Option 1: Enable Export Mode (Recommended)

The Metronome class provides an `exportMode` flag that completely disables metronome output:

```cpp
// Before starting export
audioEngine.getMetronome().setExportMode(true);

// Perform export rendering
// ...

// After export completes (or fails)
audioEngine.getMetronome().setExportMode(false);
```

**Benefits:**
- Simple and explicit
- Works with any AudioEngine-based rendering
- Thread-safe (uses std::atomic)
- Automatically skips all metronome processing

#### Option 2: Render Timeline Directly

Alternatively, AudioExporter can bypass the metronome by rendering the timeline and mixer directly without going through AudioEngine's full pipeline:

```cpp
// Pseudo-code example:
Timeline& timeline = audioEngine.getTimeline();
Mixer& mixer = audioEngine.getMixer();

// Render directly without metronome
mixer.processBlock(timeline, outputBuffer, playheadPos, numSamples);
// Skip: metronome.processBlock()
```

### Recording vs Export

**Note:** Recording (AudioRecorder) already correctly excludes the metronome because it captures the input buffer *before* the metronome processes, so no special handling is needed for recording.

### Testing Export Exclusion

To verify metronome exclusion:

1. Enable metronome during playback (verify clicks are audible)
2. Export a section of audio with metronome enabled
3. Open exported file in audio editor or DAW
4. Verify no click sounds appear in waveform or playback
5. Compare with live playback to confirm only musical content was exported

### Thread Safety

The `exportMode` flag uses `std::atomic<bool>` for thread-safe access, so it's safe to toggle from any thread (UI thread, export thread, etc.).

### Error Handling

Always ensure exportMode is reset, even if export fails:

```cpp
try {
    audioEngine.getMetronome().setExportMode(true);
    // Export code...
} catch (...) {
    audioEngine.getMetronome().setExportMode(false);
    throw;
}
```

Or use RAII pattern:

```cpp
class ExportModeGuard {
public:
    explicit ExportModeGuard(Metronome& m) : metronome(m) {
        metronome.setExportMode(true);
    }
    ~ExportModeGuard() {
        metronome.setExportMode(false);
    }
private:
    Metronome& metronome;
};

// Usage:
ExportModeGuard guard(audioEngine.getMetronome());
// Export code... metronome automatically re-enabled when guard goes out of scope
```
