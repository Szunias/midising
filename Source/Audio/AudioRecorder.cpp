#include "AudioRecorder.h"

AudioRecorder::AudioRecorder()
{
    formatManager.registerBasicFormats();

    // Create ring buffer with default settings (will be resized on startRecording)
    ringBuffer = std::make_unique<RingBuffer<float>>(2, RING_BUFFER_SIZE);

    // Pre-allocate drain buffer
    drainBuffer.setSize(2, DRAIN_BLOCK_SIZE);

    // Start the writer thread
    writerThread.startThread();
}

AudioRecorder::~AudioRecorder()
{
    stopRecording();

    // Remove ourselves from the time slice thread before stopping it
    writerThread.removeTimeSliceClient(this);
    writerThread.stopThread(1000);
}

bool AudioRecorder::startRecording(const juce::File& file, double sampleRate, int numChannels,
                                   int64_t startPositionSamples)
{
    stopRecording();

    if (file.exists())
        file.deleteFile();

    currentSampleRate = sampleRate;
    currentNumChannels = numChannels;
    samplesRecorded = 0;
    samplesDropped = 0;
    recordingStartPosition = startPositionSamples;

    // Resize ring buffer for the correct number of channels
    ringBuffer->resize(numChannels, RING_BUFFER_SIZE);

    // Resize drain buffer to match channels
    drainBuffer.setSize(numChannels, DRAIN_BLOCK_SIZE);

    // Create WAV format writer
    juce::WavAudioFormat wavFormat;

    auto fileStream = std::make_unique<juce::FileOutputStream>(file);
    if (fileStream->failedToOpen())
    {
        return false;
    }

    // Create writer
    auto* writer = wavFormat.createWriterFor(
        fileStream.release(),  // Transfer ownership
        sampleRate,
        static_cast<unsigned int>(numChannels),
        16,     // bits per sample
        {},     // metadata
        0       // quality option
    );

    if (writer == nullptr)
    {
        return false;
    }

    // Create threaded writer with a larger internal FIFO
    // The threaded writer handles its own buffering to disk
    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer, writerThread, 65536  // Internal FIFO size
    );

    // Start recording - set flag before adding to thread
    recording = true;

    // Add ourselves to the time slice thread to drain the ring buffer
    writerThread.addTimeSliceClient(this);

    return true;
}

void AudioRecorder::stopRecording()
{
    // Stop accepting new samples first
    recording = false;

    // Remove from time slice thread
    writerThread.removeTimeSliceClient(this);

    // Drain any remaining samples in the ring buffer
    if (threadedWriter != nullptr && ringBuffer != nullptr)
    {
        drainRingBuffer();
    }

    // Flush and close the writer
    threadedWriter.reset();
}

void AudioRecorder::writeAudioBlock(const juce::AudioBuffer<float>& buffer)
{
    // Lock-free: just push to ring buffer
    // This is safe to call from the audio thread
    if (recording.load(std::memory_order_acquire))
    {
        if (ringBuffer != nullptr && buffer.getNumChannels() > 0)
        {
            const int numSamples = buffer.getNumSamples();
            const int written = ringBuffer->push(buffer, numSamples);

            // Track dropped samples (buffer overflow)
            if (written < numSamples)
            {
                samplesDropped.fetch_add(numSamples - written, std::memory_order_relaxed);
            }
        }
    }
}

int AudioRecorder::useTimeSlice()
{
    // Called by the background thread to drain ring buffer to file
    if (!recording.load(std::memory_order_acquire))
    {
        // Not recording, sleep longer
        return 50;
    }

    drainRingBuffer();

    // Return quickly if there's data to write, otherwise sleep a bit
    if (ringBuffer != nullptr && ringBuffer->getAvailableRead() >= DRAIN_BLOCK_SIZE)
    {
        return 0;  // Call again immediately
    }

    return 1;  // Short sleep (1ms) to keep latency low
}

void AudioRecorder::drainRingBuffer()
{
    if (ringBuffer == nullptr || threadedWriter == nullptr)
        return;

    // Drain all available data from ring buffer
    while (ringBuffer->getAvailableRead() >= DRAIN_BLOCK_SIZE)
    {
        const int popped = ringBuffer->pop(drainBuffer, DRAIN_BLOCK_SIZE);
        if (popped > 0)
        {
            // Write to file via threaded writer
            const float* const* channels = drainBuffer.getArrayOfReadPointers();
            threadedWriter->write(channels, popped);

            // Update samples recorded count
            samplesRecorded.fetch_add(popped, std::memory_order_relaxed);
        }
        else
        {
            break;
        }
    }

    // Handle partial blocks (less than DRAIN_BLOCK_SIZE but some data available)
    const int remaining = ringBuffer->getAvailableRead();
    if (remaining > 0)
    {
        const int popped = ringBuffer->pop(drainBuffer, remaining);
        if (popped > 0)
        {
            const float* const* channels = drainBuffer.getArrayOfReadPointers();
            threadedWriter->write(channels, popped);
            samplesRecorded.fetch_add(popped, std::memory_order_relaxed);
        }
    }
}

double AudioRecorder::getRecordedDuration() const
{
    return static_cast<double>(samplesRecorded.load()) / currentSampleRate;
}

int64_t AudioRecorder::getCompensatedStartPosition() const
{
    // Apply latency compensation - shift position backward by the latency amount
    int64_t compensated = recordingStartPosition - latencyCompensationSamples;

    // Don't allow negative positions
    if (compensated < 0)
    {
        compensated = 0;
    }

    return compensated;
}

float AudioRecorder::getRingBufferFillLevel() const
{
    if (ringBuffer == nullptr)
        return 0.0f;

    const int capacity = ringBuffer->getCapacity();
    if (capacity == 0)
        return 0.0f;

    return static_cast<float>(ringBuffer->getAvailableRead()) / static_cast<float>(capacity);
}
