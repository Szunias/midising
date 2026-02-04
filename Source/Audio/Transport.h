#pragma once

#include <atomic>
#include <cstdint>

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

    // Tempo
    void setBPM(double newBpm) { bpm.store(newBpm); }
    double getBPM() const { return bpm.load(); }

private:
    std::atomic<TransportState> state { TransportState::Stopped };
    std::atomic<int64_t> playheadPosition { 0 };
    std::atomic<bool> looping { false };
    std::atomic<int64_t> loopStart { 0 };
    std::atomic<int64_t> loopEnd { 0 };
    std::atomic<double> bpm { 120.0 };
};
