#include "AudioRecorder.h"

AudioRecorder::AudioRecorder()
{
    formatManager.registerBasicFormats();
    writerThread.startThread();
}

AudioRecorder::~AudioRecorder()
{
    stopRecording();
    writerThread.stopThread(1000);
}

bool AudioRecorder::startRecording(const juce::File& file, double sampleRate, int numChannels)
{
    stopRecording();

    if (file.exists())
        file.deleteFile();

    currentSampleRate = sampleRate;
    samplesRecorded = 0;

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

    // Create threaded writer
    {
        juce::ScopedLock sl(writerLock);
        threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
            writer, writerThread, 32768
        );
    }

    recording = true;
    return true;
}

void AudioRecorder::stopRecording()
{
    recording = false;

    juce::ScopedLock sl(writerLock);
    threadedWriter.reset();
}

void AudioRecorder::writeAudioBlock(const juce::AudioBuffer<float>& buffer)
{
    if (recording.load())
    {
        juce::ScopedLock sl(writerLock);
        if (threadedWriter != nullptr && buffer.getNumChannels() > 0)
        {
            const float* const* channels = buffer.getArrayOfReadPointers();
            threadedWriter->write(channels, buffer.getNumSamples());
            samplesRecorded += buffer.getNumSamples();
        }
    }
}

double AudioRecorder::getRecordedDuration() const
{
    return static_cast<double>(samplesRecorded.load()) / currentSampleRate;
}
