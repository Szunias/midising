#include "WaveformCache.h"

// Helper class to wrap AudioBuffer as look-alike InputSource for Thumbnail?
// Actually, AudioThumbnail::reset(numChannels, sampleRate, totalSamples) and addBlock() works for raw data.

WaveformCache::WaveformCache()
{
    formatManager.registerBasicFormats();
}

WaveformCache::~WaveformCache()
{
}

void WaveformCache::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
}

int64_t WaveformCache::addAudioBuffer(const juce::AudioBuffer<float>& buffer)
{
    // Generate a simple hash based on pointer and size (simplistic, assumes buffer doesn't change OR we use ID)
    // For better robustness, regions should probably hold a unique ID.
    // Let's assume the calling code provides a unique ID or we generate one.
    // For now, let's just generate a simple random ID if we are storing it.
    
    // Actually, storing thumbnails requires an InputSource usually if we want efficient file seeking.
    // But our regions hold full buffers in memory (AudioRegion).
    // So we can feed data directly.
    
    static int64_t nextId = 1;
    int64_t id = nextId++;
    
    auto thumb = std::make_unique<juce::AudioThumbnail>(512, formatManager, thumbnailCache);
    
    // Feed data
    thumb->reset(buffer.getNumChannels(), currentSampleRate, buffer.getNumSamples());
    thumb->addBlock(0, buffer, 0, buffer.getNumSamples());
    
    thumbnails[id] = std::move(thumb);
    return id;
}

juce::AudioThumbnail& WaveformCache::getThumbnail(int64_t hash)
{
    auto it = thumbnails.find(hash);
    if (it != thumbnails.end())
        return *it->second;
        
    // Fallback: return a dummy or create new (shouldn't happen if usage is correct)
    // Just create a temporary empty one to return reference
    static juce::AudioThumbnail dummy(512, formatManager, thumbnailCache);
    return dummy;
}

void WaveformCache::refreshBuffer(int64_t hash, const juce::AudioBuffer<float>& buffer)
{
    auto it = thumbnails.find(hash);
    if (it != thumbnails.end())
    {
        it->second->reset(buffer.getNumChannels(), currentSampleRate, buffer.getNumSamples());
        it->second->addBlock(0, buffer, 0, buffer.getNumSamples());
    }
}
