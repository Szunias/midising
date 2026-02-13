#include "AudioExporter.h"

AudioExporter::AudioExporter()
{
    formatManager.registerBasicFormats();
}

juce::String AudioExporter::getFormatName(ExportFormat fmt)
{
    switch (fmt)
    {
        case ExportFormat::WAV:  return "WAV";
        case ExportFormat::FLAC: return "FLAC";
        case ExportFormat::OGG:  return "OGG Vorbis";
        default: return "WAV";
    }
}

juce::String AudioExporter::getFileExtension(ExportFormat fmt)
{
    switch (fmt)
    {
        case ExportFormat::WAV:  return "wav";
        case ExportFormat::FLAC: return "flac";
        case ExportFormat::OGG:  return "ogg";
        default: return "wav";
    }
}

bool AudioExporter::exportToFile(AudioEngine& audioEngine,
                                  const juce::File& file,
                                  int64_t startSample,
                                  int64_t lengthSamples,
                                  std::function<void(float)> progressCallback)
{
    // Delete existing file
    if (file.exists())
        file.deleteFile();

    // Create appropriate format writer based on selected format
    std::unique_ptr<juce::AudioFormat> audioFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream =
        std::make_unique<juce::FileOutputStream>(file);

    if (outputStream->failedToOpen())
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer;

    switch (format)
    {
        case ExportFormat::FLAC:
        {
            juce::FlacAudioFormat flacFormat;
            writer.reset(flacFormat.createWriterFor(
                outputStream.get(), sampleRate, 2, bitDepth, {}, 0));
            break;
        }
        case ExportFormat::OGG:
        {
            juce::OggVorbisAudioFormat oggFormat;
            // OGG uses quality parameter (0-10 scale mapped from 0.0-1.0)
            int oggQuality = static_cast<int>(quality * 10.0f);
            writer.reset(oggFormat.createWriterFor(
                outputStream.get(), sampleRate, 2, 16, {}, oggQuality));
            break;
        }
        case ExportFormat::WAV:
        default:
        {
            juce::WavAudioFormat wavFormat;
            writer.reset(wavFormat.createWriterFor(
                outputStream.get(), sampleRate, 2, bitDepth, {}, 0));
            break;
        }
    }

    if (writer == nullptr)
        return false;

    // Transfer ownership of the stream to the writer
    outputStream.release();

    // Prepare audio engine
    audioEngine.prepareToPlay(blockSize, sampleRate);

    // Set playhead to start position
    audioEngine.getTransport().setPlayheadPosition(startSample);
    audioEngine.getTransport().play();

    // Create buffer for rendering
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::AudioSourceChannelInfo channelInfo(&buffer, 0, blockSize);

    int64_t samplesRendered = 0;
    shouldCancel.store(false);

    while (samplesRendered < lengthSamples)
    {
        // Check for cancellation
        if (shouldCancel.load())
        {
            audioEngine.getTransport().stop();
            audioEngine.releaseResources();
            return false;
        }

        int samplesToRender = static_cast<int>(
            juce::jmin(static_cast<int64_t>(blockSize), lengthSamples - samplesRendered)
        );

        channelInfo.numSamples = samplesToRender;

        // Render block
        audioEngine.getNextAudioBlock(channelInfo);

        // Write to file
        writer->writeFromAudioSampleBuffer(buffer, 0, samplesToRender);

        samplesRendered += samplesToRender;

        // Report progress
        if (progressCallback)
        {
            float progress = static_cast<float>(samplesRendered) / static_cast<float>(lengthSamples);
            progressCallback(progress);
        }
    }

    // Stop transport
    audioEngine.getTransport().stop();
    audioEngine.releaseResources();

    return true;
}
