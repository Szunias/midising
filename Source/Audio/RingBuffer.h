#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cstring>

/**
 * Lock-free Single-Producer Single-Consumer (SPSC) ring buffer for audio.
 * Designed for real-time audio recording where the audio thread writes
 * and a background thread reads for disk writing.
 *
 * Thread Safety:
 * =============
 * - Only ONE thread may call push() methods (audio thread / producer)
 * - Only ONE thread may call pop() methods (writer thread / consumer)
 * - getAvailableRead() and getAvailableWrite() are safe from any thread
 * - No locks or mutexes used - completely lock-free
 *
 * Memory Ordering:
 * ===============
 * - Uses acquire/release semantics for proper synchronization
 * - Producer: release on writePos update (after writing data)
 * - Consumer: acquire on readPos (before reading data)
 * - This ensures data written is visible to consumer before position update
 *
 * Usage:
 * ======
 * Audio Thread (Producer):
 *   ringBuffer.push(audioBuffer);  // Lock-free, never blocks
 *
 * Writer Thread (Consumer):
 *   while (ringBuffer.getAvailableRead() >= blockSize)
 *       ringBuffer.pop(tempBuffer, blockSize);
 *       writer->write(tempBuffer);
 */
template <typename SampleType = float>
class RingBuffer
{
public:
    /**
     * Construct a ring buffer with specified capacity.
     * Capacity is rounded up to power of 2 for efficient modulo.
     *
     * @param numChannels Number of audio channels
     * @param capacitySamples Number of samples per channel (will be rounded to power of 2)
     */
    RingBuffer(int numChannels = 2, int capacitySamples = 65536)
    {
        // Round capacity up to next power of 2
        int capacity = 1;
        while (capacity < capacitySamples)
            capacity <<= 1;

        bufferCapacity = capacity;
        capacityMask = capacity - 1;
        channels = numChannels;

        // Allocate storage for all channels
        buffer.setSize(numChannels, capacity);
        buffer.clear();

        writePos.store(0, std::memory_order_relaxed);
        readPos.store(0, std::memory_order_relaxed);
    }

    ~RingBuffer() = default;

    /**
     * Reset the buffer to empty state.
     * NOT thread-safe - only call when no other threads are accessing.
     */
    void reset()
    {
        writePos.store(0, std::memory_order_relaxed);
        readPos.store(0, std::memory_order_relaxed);
        buffer.clear();
    }

    /**
     * Resize the buffer.
     * NOT thread-safe - only call when no other threads are accessing.
     */
    void resize(int numChannels, int capacitySamples)
    {
        // Round capacity up to next power of 2
        int capacity = 1;
        while (capacity < capacitySamples)
            capacity <<= 1;

        bufferCapacity = capacity;
        capacityMask = capacity - 1;
        channels = numChannels;

        buffer.setSize(numChannels, capacity);
        buffer.clear();

        writePos.store(0, std::memory_order_relaxed);
        readPos.store(0, std::memory_order_relaxed);
    }

    /**
     * Get number of samples available to read (consumer).
     * Thread-safe, can be called from any thread.
     */
    int getAvailableRead() const
    {
        int write = writePos.load(std::memory_order_acquire);
        int read = readPos.load(std::memory_order_relaxed);
        return (write - read + bufferCapacity) & capacityMask;
    }

    /**
     * Get number of samples that can be written (producer).
     * Thread-safe, can be called from any thread.
     */
    int getAvailableWrite() const
    {
        int write = writePos.load(std::memory_order_relaxed);
        int read = readPos.load(std::memory_order_acquire);
        // Reserve one slot to distinguish full from empty
        return ((read - write - 1) + bufferCapacity) & capacityMask;
    }

    /**
     * Get total buffer capacity in samples.
     */
    int getCapacity() const { return bufferCapacity; }

    /**
     * Get number of channels.
     */
    int getNumChannels() const { return channels; }

    /**
     * Push audio samples from an AudioBuffer (producer only).
     * Lock-free, never blocks. If buffer is full, samples are dropped.
     *
     * @param source Audio buffer to copy from
     * @return Number of samples actually written (may be less than requested if full)
     */
    int push(const juce::AudioBuffer<SampleType>& source)
    {
        return push(source, source.getNumSamples());
    }

    /**
     * Push audio samples from an AudioBuffer (producer only).
     * Lock-free, never blocks. If buffer is full, samples are dropped.
     *
     * @param source Audio buffer to copy from
     * @param numSamples Number of samples to push
     * @return Number of samples actually written (may be less than requested if full)
     */
    int push(const juce::AudioBuffer<SampleType>& source, int numSamples)
    {
        const int available = getAvailableWrite();
        const int toWrite = juce::jmin(numSamples, available);

        if (toWrite == 0)
            return 0;

        const int write = writePos.load(std::memory_order_relaxed);
        const int channelsToCopy = juce::jmin(source.getNumChannels(), channels);

        // Copy data for each channel
        for (int ch = 0; ch < channelsToCopy; ++ch)
        {
            const SampleType* src = source.getReadPointer(ch);
            SampleType* dest = buffer.getWritePointer(ch);

            const int firstPart = juce::jmin(toWrite, bufferCapacity - write);
            const int secondPart = toWrite - firstPart;

            // Copy first contiguous block
            std::memcpy(dest + write, src, static_cast<size_t>(firstPart) * sizeof(SampleType));

            // Copy wrapped portion if needed
            if (secondPart > 0)
            {
                std::memcpy(dest, src + firstPart, static_cast<size_t>(secondPart) * sizeof(SampleType));
            }
        }

        // Fill remaining channels with zeros if source has fewer channels
        for (int ch = channelsToCopy; ch < channels; ++ch)
        {
            SampleType* dest = buffer.getWritePointer(ch);

            const int firstPart = juce::jmin(toWrite, bufferCapacity - write);
            const int secondPart = toWrite - firstPart;

            std::memset(dest + write, 0, static_cast<size_t>(firstPart) * sizeof(SampleType));
            if (secondPart > 0)
            {
                std::memset(dest, 0, static_cast<size_t>(secondPart) * sizeof(SampleType));
            }
        }

        // Update write position with release semantics
        // This ensures all writes are visible before position update
        const int newWrite = (write + toWrite) & capacityMask;
        writePos.store(newWrite, std::memory_order_release);

        return toWrite;
    }

    /**
     * Push raw sample data for all channels (producer only).
     * Lock-free, never blocks. If buffer is full, samples are dropped.
     *
     * @param channelData Array of pointers to channel data
     * @param numChannelsToCopy Number of channels in channelData
     * @param numSamples Number of samples per channel to push
     * @return Number of samples actually written (may be less than requested if full)
     */
    int push(const SampleType* const* channelData, int numChannelsToCopy, int numSamples)
    {
        const int available = getAvailableWrite();
        const int toWrite = juce::jmin(numSamples, available);

        if (toWrite == 0)
            return 0;

        const int write = writePos.load(std::memory_order_relaxed);
        const int channelsToUse = juce::jmin(numChannelsToCopy, channels);

        // Copy data for each channel
        for (int ch = 0; ch < channelsToUse; ++ch)
        {
            const SampleType* src = channelData[ch];
            SampleType* dest = buffer.getWritePointer(ch);

            const int firstPart = juce::jmin(toWrite, bufferCapacity - write);
            const int secondPart = toWrite - firstPart;

            std::memcpy(dest + write, src, static_cast<size_t>(firstPart) * sizeof(SampleType));
            if (secondPart > 0)
            {
                std::memcpy(dest, src + firstPart, static_cast<size_t>(secondPart) * sizeof(SampleType));
            }
        }

        // Fill remaining channels with zeros
        for (int ch = channelsToUse; ch < channels; ++ch)
        {
            SampleType* dest = buffer.getWritePointer(ch);

            const int firstPart = juce::jmin(toWrite, bufferCapacity - write);
            const int secondPart = toWrite - firstPart;

            std::memset(dest + write, 0, static_cast<size_t>(firstPart) * sizeof(SampleType));
            if (secondPart > 0)
            {
                std::memset(dest, 0, static_cast<size_t>(secondPart) * sizeof(SampleType));
            }
        }

        const int newWrite = (write + toWrite) & capacityMask;
        writePos.store(newWrite, std::memory_order_release);

        return toWrite;
    }

    /**
     * Pop audio samples into an AudioBuffer (consumer only).
     * Lock-free, never blocks. If not enough data, returns less than requested.
     *
     * @param dest Destination buffer to copy into
     * @param numSamples Number of samples to pop
     * @return Number of samples actually read (may be less than requested if not enough data)
     */
    int pop(juce::AudioBuffer<SampleType>& dest, int numSamples)
    {
        const int available = getAvailableRead();
        const int toRead = juce::jmin(numSamples, available);

        if (toRead == 0)
            return 0;

        const int read = readPos.load(std::memory_order_relaxed);
        const int channelsToCopy = juce::jmin(dest.getNumChannels(), channels);

        // Copy data for each channel
        for (int ch = 0; ch < channelsToCopy; ++ch)
        {
            const SampleType* src = buffer.getReadPointer(ch);
            SampleType* destPtr = dest.getWritePointer(ch);

            const int firstPart = juce::jmin(toRead, bufferCapacity - read);
            const int secondPart = toRead - firstPart;

            std::memcpy(destPtr, src + read, static_cast<size_t>(firstPart) * sizeof(SampleType));
            if (secondPart > 0)
            {
                std::memcpy(destPtr + firstPart, src, static_cast<size_t>(secondPart) * sizeof(SampleType));
            }
        }

        // Update read position with release semantics
        const int newRead = (read + toRead) & capacityMask;
        readPos.store(newRead, std::memory_order_release);

        return toRead;
    }

    /**
     * Pop raw sample data for all channels (consumer only).
     * Lock-free, never blocks. If not enough data, returns less than requested.
     *
     * @param channelData Array of pointers to destination channel buffers
     * @param numChannelsToCopy Number of channels in channelData
     * @param numSamples Number of samples per channel to pop
     * @return Number of samples actually read
     */
    int pop(SampleType* const* channelData, int numChannelsToCopy, int numSamples)
    {
        const int available = getAvailableRead();
        const int toRead = juce::jmin(numSamples, available);

        if (toRead == 0)
            return 0;

        const int read = readPos.load(std::memory_order_relaxed);
        const int channelsToUse = juce::jmin(numChannelsToCopy, channels);

        for (int ch = 0; ch < channelsToUse; ++ch)
        {
            const SampleType* src = buffer.getReadPointer(ch);
            SampleType* destPtr = channelData[ch];

            const int firstPart = juce::jmin(toRead, bufferCapacity - read);
            const int secondPart = toRead - firstPart;

            std::memcpy(destPtr, src + read, static_cast<size_t>(firstPart) * sizeof(SampleType));
            if (secondPart > 0)
            {
                std::memcpy(destPtr + firstPart, src, static_cast<size_t>(secondPart) * sizeof(SampleType));
            }
        }

        const int newRead = (read + toRead) & capacityMask;
        readPos.store(newRead, std::memory_order_release);

        return toRead;
    }

    /**
     * Peek at samples without removing them (consumer only).
     * Lock-free.
     *
     * @param dest Destination buffer to copy into
     * @param numSamples Number of samples to peek
     * @return Number of samples actually peeked
     */
    int peek(juce::AudioBuffer<SampleType>& dest, int numSamples) const
    {
        const int available = getAvailableRead();
        const int toPeek = juce::jmin(numSamples, available);

        if (toPeek == 0)
            return 0;

        const int read = readPos.load(std::memory_order_acquire);
        const int channelsToCopy = juce::jmin(dest.getNumChannels(), channels);

        for (int ch = 0; ch < channelsToCopy; ++ch)
        {
            const SampleType* src = buffer.getReadPointer(ch);
            SampleType* destPtr = dest.getWritePointer(ch);

            const int firstPart = juce::jmin(toPeek, bufferCapacity - read);
            const int secondPart = toPeek - firstPart;

            std::memcpy(destPtr, src + read, static_cast<size_t>(firstPart) * sizeof(SampleType));
            if (secondPart > 0)
            {
                std::memcpy(destPtr + firstPart, src, static_cast<size_t>(secondPart) * sizeof(SampleType));
            }
        }

        return toPeek;
    }

    /**
     * Skip/discard samples without copying (consumer only).
     * Useful for dropping data when behind.
     *
     * @param numSamples Number of samples to skip
     * @return Number of samples actually skipped
     */
    int skip(int numSamples)
    {
        const int available = getAvailableRead();
        const int toSkip = juce::jmin(numSamples, available);

        if (toSkip == 0)
            return 0;

        const int read = readPos.load(std::memory_order_relaxed);
        const int newRead = (read + toSkip) & capacityMask;
        readPos.store(newRead, std::memory_order_release);

        return toSkip;
    }

    /**
     * Check if buffer is empty.
     */
    bool isEmpty() const
    {
        return getAvailableRead() == 0;
    }

    /**
     * Check if buffer is full.
     */
    bool isFull() const
    {
        return getAvailableWrite() == 0;
    }

private:
    juce::AudioBuffer<SampleType> buffer;

    int bufferCapacity = 0;
    int capacityMask = 0;     // For efficient modulo (capacity - 1)
    int channels = 0;

    // Atomic positions for lock-free SPSC operation
    // Use alignas to prevent false sharing between cache lines
    alignas(64) std::atomic<int> writePos { 0 };
    alignas(64) std::atomic<int> readPos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RingBuffer)
};
