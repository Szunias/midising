#pragma once

#include <atomic>
#include <cstdint>
#include <cmath>

// Forward declaration
class TempoTrack;

/**
 * Transport states for playback control.
 */
enum class TransportState
{
    Stopped,
    Playing,
    Recording
};

/**
 * Information about a loop boundary crossing within an audio buffer.
 * Used for sample-accurate loop handling during audio processing.
 */
struct LoopCrossingInfo
{
    bool didLoop = false;           // True if loop boundary was crossed
    int samplesBeforeLoop = 0;      // Samples to process before reaching loop end
    int samplesAfterLoop = 0;       // Samples to process after wrapping to loop start
    int64_t wrapPosition = 0;       // Position in buffer where wrap occurred
    int wrapCount = 0;              // Number of times the loop wrapped (usually 0 or 1)
};

/**
 * Transport controls playback state and playhead position.
 * Thread-safe for access from audio thread.
 */
class Transport
{
public:
    Transport() = default;
    ~Transport() = default;

    // State control
    void play()
    {
        if (state.load() == TransportState::Stopped)
            state.store(TransportState::Playing);
    }

    void pause()
    {
        if (state.load() == TransportState::Playing)
            state.store(TransportState::Stopped);
    }

    void stop()
    {
        state.store(TransportState::Stopped);
        playheadPosition.store(0);
    }

    void record()
    {
        state.store(TransportState::Recording);
    }

    void togglePlayPause()
    {
        if (state.load() == TransportState::Playing)
            pause();
        else
            play();
    }

    // State queries
    TransportState getState() const { return state.load(); }
    bool isPlaying() const { return state.load() == TransportState::Playing; }
    bool isRecording() const { return state.load() == TransportState::Recording; }
    bool isStopped() const { return state.load() == TransportState::Stopped; }

    // Playhead position (in samples)
    int64_t getPlayheadPosition() const { return playheadPosition.load(); }
    void setPlayheadPosition(int64_t position) { playheadPosition.store(position); }
    
    // Advance playhead by given number of samples (called from audio thread)
    void advancePlayhead(int numSamples)
    {
        if (state.load() != TransportState::Stopped)
        {
            playheadPosition.fetch_add(numSamples);
        }
    }

    // Loop settings
    bool isLooping() const { return looping.load(); }
    void setLooping(bool shouldLoop) { looping.store(shouldLoop); }
    int64_t getLoopStart() const { return loopStart.load(); }
    int64_t getLoopEnd() const { return loopEnd.load(); }
    void setLoopRange(int64_t start, int64_t end)
    {
        loopStart.store(start);
        loopEnd.store(end);
    }

    // Check and handle loop wrapping
    void handleLoopWrap()
    {
        if (looping.load())
        {
            int64_t pos = playheadPosition.load();
            int64_t end = loopEnd.load();
            if (pos >= end)
            {
                playheadPosition.store(loopStart.load());
            }
        }
    }

    /**
     * Sample-accurate loop handling for audio buffer processing.
     * Calculates where within a buffer the loop boundary is crossed.
     *
     * @param numSamples Number of samples in the buffer being processed
     * @return LoopCrossingInfo with details about loop boundary crossing
     */
    LoopCrossingInfo getLoopCrossingInfo(int numSamples) const
    {
        LoopCrossingInfo info;

        if (!looping.load() || state.load() == TransportState::Stopped)
        {
            info.samplesBeforeLoop = numSamples;
            return info;
        }

        int64_t pos = playheadPosition.load();
        int64_t start = loopStart.load();
        int64_t end = loopEnd.load();
        int64_t loopLength = end - start;

        // Validate loop range
        if (loopLength <= 0 || end <= start)
        {
            info.samplesBeforeLoop = numSamples;
            return info;
        }

        int64_t endPosition = pos + numSamples;

        // Check if we cross the loop boundary
        if (pos < end && endPosition >= end)
        {
            info.didLoop = true;
            info.samplesBeforeLoop = static_cast<int>(end - pos);
            info.samplesAfterLoop = numSamples - info.samplesBeforeLoop;
            info.wrapPosition = end;
            info.wrapCount = 1;

            // Handle case where loop length is shorter than remaining samples
            // (multiple wraps within single buffer)
            if (info.samplesAfterLoop >= loopLength && loopLength > 0)
            {
                info.wrapCount += static_cast<int>(info.samplesAfterLoop / loopLength);
                info.samplesAfterLoop = static_cast<int>(info.samplesAfterLoop % loopLength);
            }
        }
        else
        {
            info.samplesBeforeLoop = numSamples;
        }

        return info;
    }

    /**
     * Advance playhead with sample-accurate loop handling.
     * Returns loop crossing information for audio buffer processing.
     *
     * @param numSamples Number of samples to advance
     * @return LoopCrossingInfo with details about any loop boundary crossings
     */
    LoopCrossingInfo advancePlayheadWithLoop(int numSamples)
    {
        LoopCrossingInfo info = getLoopCrossingInfo(numSamples);

        if (state.load() == TransportState::Stopped)
        {
            return info;
        }

        if (!looping.load())
        {
            playheadPosition.fetch_add(numSamples);
            return info;
        }

        int64_t start = loopStart.load();
        int64_t end = loopEnd.load();
        int64_t loopLength = end - start;

        if (loopLength <= 0)
        {
            playheadPosition.fetch_add(numSamples);
            return info;
        }

        if (info.didLoop)
        {
            // Calculate new position after wrapping
            // New position = loopStart + (remaining samples after all wraps)
            int64_t newPos = start + info.samplesAfterLoop;
            playheadPosition.store(newPos);
        }
        else
        {
            playheadPosition.fetch_add(numSamples);
        }

        return info;
    }

    /**
     * Get the loop length in samples.
     * Returns 0 if loop range is invalid.
     */
    int64_t getLoopLength() const
    {
        int64_t start = loopStart.load();
        int64_t end = loopEnd.load();
        return (end > start) ? (end - start) : 0;
    }

    /**
     * Check if a position is within the loop range.
     */
    bool isPositionInLoop(int64_t position) const
    {
        return position >= loopStart.load() && position < loopEnd.load();
    }

    /**
     * Snap position to loop start if looping and position is past loop end.
     * Returns the snapped position without modifying playhead.
     */
    int64_t snapToLoopRange(int64_t position) const
    {
        if (!looping.load())
        {
            return position;
        }

        int64_t start = loopStart.load();
        int64_t end = loopEnd.load();
        int64_t loopLength = end - start;

        if (loopLength <= 0 || position < end)
        {
            return position;
        }

        // Calculate wrapped position
        int64_t offset = (position - start) % loopLength;
        return start + offset;
    }

    // Punch-in/out recording
    bool isPunchInEnabled() const { return punchInEnabled.load(); }
    void setPunchInEnabled(bool enabled) { punchInEnabled.store(enabled); }
    bool isPunchOutEnabled() const { return punchOutEnabled.load(); }
    void setPunchOutEnabled(bool enabled) { punchOutEnabled.store(enabled); }
    int64_t getPunchInPosition() const { return punchInPosition.load(); }
    int64_t getPunchOutPosition() const { return punchOutPosition.load(); }
    void setPunchRange(int64_t start, int64_t end)
    {
        punchInPosition.store(start);
        punchOutPosition.store(end);
    }
    void clearPunchRange()
    {
        punchInEnabled.store(false);
        punchOutEnabled.store(false);
        punchInPosition.store(0);
        punchOutPosition.store(0);
    }
    bool shouldRecordAtPosition(int64_t pos) const
    {
        if (state.load() != TransportState::Recording)
            return false;
        bool inOk = !punchInEnabled.load() || pos >= punchInPosition.load();
        bool outOk = !punchOutEnabled.load() || pos < punchOutPosition.load();
        return inOk && outOk;
    }
    int getPreRollBeats() const { return preRollBeats.load(); }
    void setPreRollBeats(int beats) { preRollBeats.store(beats > 0 ? beats : 0); }

    // Tempo
    void setBPM(double newBpm) { bpm.store(newBpm); }
    double getBPM() const { return bpm.load(); }

    //==========================================================================
    // Tempo Ramp Integration
    //==========================================================================

    /**
     * Set the tempo track for tempo ramp support.
     * When set, the transport will query tempo at specific positions from the track.
     * @param track Pointer to the tempo track (can be nullptr to disable)
     */
    void setTempoTrack(const TempoTrack* track) { tempoTrack = track; }

    /**
     * Get the currently set tempo track.
     * @return Pointer to the tempo track, or nullptr if not set
     */
    const TempoTrack* getTempoTrack() const { return tempoTrack; }

    /**
     * Set the sample rate for time conversions.
     * @param rate Sample rate in Hz
     */
    void setSampleRate(double rate) { sampleRate.store(rate); }

    /**
     * Get the current sample rate.
     * @return Sample rate in Hz
     */
    double getSampleRate() const { return sampleRate.load(); }

    /**
     * Get the effective BPM at the current playhead position.
     * If a tempo track is set with tempo changes, returns interpolated tempo.
     * Otherwise returns the base BPM.
     * @return BPM at the current position
     */
    double getEffectiveBPM() const
    {
        return getEffectiveBPMAtPosition(playheadPosition.load());
    }

    /**
     * Get the effective BPM at a specific sample position.
     * If a tempo track is set with tempo changes, returns interpolated tempo.
     * Otherwise returns the base BPM.
     * @param position Position in samples
     * @return BPM at the position
     */
    double getEffectiveBPMAtPosition(int64_t position) const;

    /**
     * Get the number of samples per beat at the current playhead position.
     * Accounts for tempo ramps if a tempo track is set.
     * @return Samples per beat at the current position
     */
    double getSamplesPerBeat() const
    {
        return getSamplesPerBeatAtPosition(playheadPosition.load());
    }

    /**
     * Get the number of samples per beat at a specific sample position.
     * Accounts for tempo ramps if a tempo track is set.
     * @param position Position in samples
     * @return Samples per beat at the position
     */
    double getSamplesPerBeatAtPosition(int64_t position) const
    {
        double effectiveBpm = getEffectiveBPMAtPosition(position);
        double rate = sampleRate.load();
        if (effectiveBpm <= 0.0 || rate <= 0.0)
            return rate * 0.5; // Default to 120 BPM
        return (60.0 / effectiveBpm) * rate;
    }

    /**
     * Calculate the effective tempo for an audio buffer, considering tempo ramps.
     * Returns the average BPM across the buffer for smooth parameter changes.
     * @param numSamples Size of the buffer in samples
     * @return Average BPM over the buffer
     */
    double getAverageBPMForBuffer(int numSamples) const
    {
        int64_t startPos = playheadPosition.load();
        int64_t endPos = startPos + numSamples;

        double startBpm = getEffectiveBPMAtPosition(startPos);
        double endBpm = getEffectiveBPMAtPosition(endPos);

        // Return average (simple linear assumption within buffer)
        return (startBpm + endBpm) * 0.5;
    }

    /**
     * Get tempo values at the start and end of a buffer for ramping.
     * Useful for smooth tempo-dependent parameter transitions.
     * @param numSamples Size of the buffer in samples
     * @param startBpm Output: BPM at buffer start
     * @param endBpm Output: BPM at buffer end
     */
    void getTempoRampForBuffer(int numSamples, double& startBpm, double& endBpm) const
    {
        int64_t startPos = playheadPosition.load();
        int64_t endPos = startPos + numSamples;

        startBpm = getEffectiveBPMAtPosition(startPos);
        endBpm = getEffectiveBPMAtPosition(endPos);
    }

    /**
     * Check if there are tempo changes (ramps) in effect.
     * @return true if tempo track is set and has tempo changes
     */
    bool hasTempoRamps() const;

    /**
     * Convert a beat position to samples using the current tempo state.
     * Accounts for tempo ramps if a tempo track is set.
     * @param beats Beat position
     * @return Sample position
     */
    int64_t beatsToSamples(double beats) const;

    /**
     * Convert a sample position to beats using the current tempo state.
     * Accounts for tempo ramps if a tempo track is set.
     * @param samples Sample position
     * @return Beat position
     */
    double samplesToBeats(int64_t samples) const;

private:
    std::atomic<TransportState> state { TransportState::Stopped };
    std::atomic<int64_t> playheadPosition { 0 };
    std::atomic<bool> looping { false };
    std::atomic<int64_t> loopStart { 0 };
    std::atomic<int64_t> loopEnd { 0 };
    std::atomic<double> bpm { 120.0 };
    std::atomic<double> sampleRate { 44100.0 };
    const TempoTrack* tempoTrack { nullptr };

    // Punch-in/out state
    std::atomic<bool> punchInEnabled { false };
    std::atomic<bool> punchOutEnabled { false };
    std::atomic<int64_t> punchInPosition { 0 };
    std::atomic<int64_t> punchOutPosition { 0 };
    std::atomic<int> preRollBeats { 0 };
};

//==============================================================================
// Inline implementations that depend on TempoTrack
// These are defined outside the class to allow deferred resolution
//==============================================================================

#include "../Timeline/TempoTrack.h"

inline double Transport::getEffectiveBPMAtPosition(int64_t position) const
{
    if (tempoTrack != nullptr && tempoTrack->isEnabled() && tempoTrack->hasTempoChanges())
    {
        return tempoTrack->getBpmAtPosition(position);
    }
    return bpm.load();
}

inline bool Transport::hasTempoRamps() const
{
    return tempoTrack != nullptr && tempoTrack->isEnabled() && tempoTrack->hasTempoChanges();
}

inline int64_t Transport::beatsToSamples(double beats) const
{
    if (tempoTrack != nullptr && tempoTrack->isEnabled() && tempoTrack->hasTempoChanges())
    {
        return tempoTrack->beatsToSamples(beats);
    }
    // Simple conversion with constant tempo
    double effectiveBpm = bpm.load();
    double rate = sampleRate.load();
    if (effectiveBpm <= 0.0 || rate <= 0.0)
        return static_cast<int64_t>(beats * rate * 0.5); // Default 120 BPM
    double samplesPerBeat = (60.0 / effectiveBpm) * rate;
    return static_cast<int64_t>(beats * samplesPerBeat);
}

inline double Transport::samplesToBeats(int64_t samples) const
{
    if (tempoTrack != nullptr && tempoTrack->isEnabled() && tempoTrack->hasTempoChanges())
    {
        return tempoTrack->samplesToBeats(samples);
    }
    // Simple conversion with constant tempo
    double effectiveBpm = bpm.load();
    double rate = sampleRate.load();
    if (effectiveBpm <= 0.0 || rate <= 0.0)
        return static_cast<double>(samples) / (rate * 0.5); // Default 120 BPM
    double samplesPerBeat = (60.0 / effectiveBpm) * rate;
    return static_cast<double>(samples) / samplesPerBeat;
}
