#include "AudioExporter.h"

AudioExporter::AudioExporter()
{
    formatManager.registerBasicFormats();
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

    // Create WAV writer
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream =
        std::make_unique<juce::FileOutputStream>(file);

    if (outputStream->failedToOpen())
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outputStream.get(),
                                   sampleRate,
                                   2,           // stereo
                                   bitDepth,
                                   {},          // metadata
                                   0));         // quality

    if (writer == nullptr)
        return false;

    // Transfer ownership
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

    while (samplesRendered < lengthSamples)
    {
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
