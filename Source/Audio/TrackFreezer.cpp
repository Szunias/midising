#include "TrackFreezer.h"
#include "../Timeline/Timeline.h"

bool TrackFreezer::freezeTrack(Track& track,
                                Timeline& timeline,
                                const juce::File& outputFile,
                                int64_t lengthSamples,
                                double sampleRate,
                                std::function<void(float)> progressCallback)
{
    // Delete existing file
    if (outputFile.exists())
        outputFile.deleteFile();

    // Create WAV writer
    juce::WavAudioFormat wavFormat;
    auto outputStream = std::make_unique<juce::FileOutputStream>(outputFile);

    if (outputStream->failedToOpen())
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outputStream.get(), sampleRate, 2, 24, {}, 0));

    if (writer == nullptr)
        return false;

    outputStream.release();

    // Prepare track for offline rendering
    track.prepareToPlay(sampleRate, blockSize);

    // Create buffer for rendering
    juce::AudioBuffer<float> buffer(2, blockSize);

    int64_t samplesRendered = 0;

    while (samplesRendered < lengthSamples)
    {
        int samplesToRender = static_cast<int>(
            juce::jmin(static_cast<int64_t>(blockSize), lengthSamples - samplesRendered));

        buffer.clear();

        // Process the track
        track.processBlock(buffer, static_cast<int>(samplesRendered), samplesToRender);

        // Write to file
        writer->writeFromAudioSampleBuffer(buffer, 0, samplesToRender);

        samplesRendered += samplesToRender;

        if (progressCallback)
        {
            float progress = static_cast<float>(samplesRendered) / static_cast<float>(lengthSamples);
            progressCallback(progress);
        }
    }

    track.releaseResources();
    return true;
}

juce::File TrackFreezer::getFreezeFolder()
{
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto folder = appDataDir.getChildFile("MidiSing").getChildFile("FrozenTracks");
    folder.createDirectory();
    return folder;
}

juce::File TrackFreezer::getFreezeFile(const juce::String& trackName)
{
    auto folder = getFreezeFolder();
    auto safeName = trackName.replaceCharacters(" /\\:*?\"<>|", "__________");
    auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    return folder.getChildFile(safeName + "_" + timestamp + ".wav");
}
