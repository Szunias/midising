#include "AudioImporter.h"

AudioImporter::AudioImporter()
{
    formatManager.registerBasicFormats();
}

std::unique_ptr<juce::AudioBuffer<float>> AudioImporter::loadFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return nullptr;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    
    if (reader == nullptr)
        return nullptr;

    int numSamples = static_cast<int>(reader->lengthInSamples);
    int numChannels = static_cast<int>(reader->numChannels);
    
    auto buffer = std::make_unique<juce::AudioBuffer<float>>(numChannels, numSamples);
    reader->read(buffer.get(), 0, numSamples, 0, true, true);
    
    return buffer;
}

std::unique_ptr<juce::AudioBuffer<float>> AudioImporter::loadFile(const juce::File& file, double targetSampleRate)
{
    if (!file.existsAsFile())
        return nullptr;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    
    if (reader == nullptr)
        return nullptr;

    // If sample rates match, just load normally
    if (std::abs(reader->sampleRate - targetSampleRate) < 0.1)
    {
        return loadFile(file);
    }

    // Need to resample
    int origNumSamples = static_cast<int>(reader->lengthInSamples);
    int numChannels = static_cast<int>(reader->numChannels);
    
    // Load original
    juce::AudioBuffer<float> origBuffer(numChannels, origNumSamples);
    reader->read(&origBuffer, 0, origNumSamples, 0, true, true);
    
    // Calculate resampled length
    double ratio = targetSampleRate / reader->sampleRate;
    int newNumSamples = static_cast<int>(origNumSamples * ratio);
    
    auto resampledBuffer = std::make_unique<juce::AudioBuffer<float>>(numChannels, newNumSamples);
    
    // Simple linear interpolation resampling
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = origBuffer.getReadPointer(ch);
        float* dst = resampledBuffer->getWritePointer(ch);
        
        for (int i = 0; i < newNumSamples; ++i)
        {
            double srcPos = i / ratio;
            int srcIndex = static_cast<int>(srcPos);
            double frac = srcPos - srcIndex;
            
            if (srcIndex + 1 < origNumSamples)
            {
                dst[i] = static_cast<float>(src[srcIndex] * (1.0 - frac) + src[srcIndex + 1] * frac);
            }
            else if (srcIndex < origNumSamples)
            {
                dst[i] = src[srcIndex];
            }
        }
    }
    
    return resampledBuffer;
}

AudioImporter::AudioFileInfo AudioImporter::getFileInfo(const juce::File& file)
{
    AudioFileInfo info;
    
    if (!file.existsAsFile())
        return info;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    
    if (reader == nullptr)
        return info;

    info.sampleRate = reader->sampleRate;
    info.lengthInSamples = reader->lengthInSamples;
    info.numChannels = static_cast<int>(reader->numChannels);
    info.lengthInSeconds = reader->lengthInSamples / reader->sampleRate;
    info.format = reader->getFormatName();
    
    return info;
}

juce::StringArray AudioImporter::getSupportedExtensions() const
{
    return juce::StringArray { ".wav", ".mp3", ".aiff", ".aif", ".flac", ".ogg" };
}

bool AudioImporter::isSupported(const juce::File& file) const
{
    juce::String ext = file.getFileExtension().toLowerCase();
    return getSupportedExtensions().contains(ext);
}
