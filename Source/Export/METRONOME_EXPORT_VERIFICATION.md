# Metronome Export Exclusion - Verification Plan

## Status: PREPARED FOR FUTURE IMPLEMENTATION

The Metronome class has been enhanced with export mode functionality to ensure it can be excluded from audio exports when AudioExporter is implemented.

## Changes Made

### 1. Metronome.h
- Added `setExportMode(bool)` method to enable/disable export mode
- Added `isInExportMode()` query method
- Added `std::atomic<bool> exportMode` member variable

### 2. Metronome.cpp
- Modified `processBlock()` to skip all processing when `exportMode` is true
- Early return ensures zero overhead during export

### 3. Documentation
- Created `EXPORT_GUIDELINES.md` with implementation patterns for future AudioExporter
- Documented RAII guard pattern for exception-safe export mode handling

## Current Behavior

**Recording (AudioRecorder):**
- Already excludes metronome correctly ✓
- Records input buffer before metronome processing
- No changes needed

**Live Playback (AudioEngine):**
- Metronome mixes clicks into output buffer ✓
- Works as expected for monitoring

**Export (AudioExporter):**
- Not yet implemented
- When implemented, must use `setExportMode(true)` before rendering

## Manual Verification Steps

When AudioExporter is implemented, verify with these steps:

### Test 1: Basic Export Exclusion
1. Create a simple project with 1-2 bars of audio content
2. Enable metronome (toggle button in transport bar)
3. Play project and verify you hear clicks
4. Export the project to WAV file
5. Open exported file in audio editor (Audacity, etc.)
6. **VERIFY:** No click sounds in waveform or audio playback
7. **VERIFY:** Only musical content is present

### Test 2: Export Mode Toggle
1. Enable metronome and start playback
2. In code/debugger, verify `exportMode` is false during playback
3. Trigger export operation
4. Verify `exportMode` is set to true during export
5. Verify `exportMode` is reset to false after export completes
6. Resume playback and verify metronome clicks return

### Test 3: Export Mode Exception Safety
1. Modify export code to throw exception mid-export
2. Verify `exportMode` is still reset to false
3. Verify metronome works correctly after failed export

### Test 4: Different Metronome States
Export with these metronome configurations and verify all exclude clicks:
- Metronome enabled, volume 100%
- Metronome enabled, volume 50%
- Metronome enabled with count-in active
- Metronome with accent beats
- Different BPMs: 60, 120, 180

### Test 5: Export During Count-in
1. Enable count-in (1 or 2 bars)
2. Press record to start count-in
3. During count-in, trigger an export
4. Verify export does not contain count-in clicks

## Expected Results

All exported audio files should:
- ✓ Contain ONLY musical content (tracks, instruments, vocals)
- ✓ Contain NO click sounds from metronome
- ✓ Have identical content whether metronome was enabled or disabled
- ✓ Match expected length and sample rate

## Implementation Checklist for AudioExporter

When creating AudioExporter, ensure:

- [ ] Call `audioEngine.getMetronome().setExportMode(true)` before export
- [ ] Call `audioEngine.getMetronome().setExportMode(false)` after export
- [ ] Use RAII guard or try-finally pattern for exception safety
- [ ] Test all verification scenarios above
- [ ] Document export behavior in AudioExporter class
- [ ] Add export test to integration test suite

## Technical Details

### Export Mode Mechanism

```cpp
// In Metronome::processBlock()
if (exportMode.load())
{
    clickPlaybackPosition = -1;
    return; // Skip all metronome processing
}
```

This ensures:
- **Zero CPU overhead** during export (early return)
- **Thread-safe** (std::atomic<bool>)
- **No clicks mixed** into any output buffers
- **Playback state preserved** (only affects output, not internal state)

### Thread Safety

The `exportMode` flag is thread-safe because:
- Uses `std::atomic<bool>`
- Can be safely toggled from UI thread or export thread
- Audio thread reads atomically in `processBlock()`
- No race conditions or locks needed

## Related Files

- `Source/Audio/Metronome.h` - Export mode interface
- `Source/Audio/Metronome.cpp` - Export mode implementation
- `Source/Audio/AudioEngine.cpp` - Integration point for export rendering
- `Source/Export/EXPORT_GUIDELINES.md` - Implementation guidelines

## Notes

- Recording already excludes metronome (captures before metronome processes)
- Export mode is independent of enabled/disabled state
- Visual beat indicator should continue working during export (UI only)
- Metronome state (enabled/volume) is preserved during export
