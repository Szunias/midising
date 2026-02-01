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

private:
    std::atomic<TransportState> state { TransportState::Stopped };
    std::atomic<int64_t> playheadPosition { 0 };
    std::atomic<bool> looping { false };
    std::atomic<int64_t> loopStart { 0 };
    std::atomic<int64_t> loopEnd { 0 };
};
