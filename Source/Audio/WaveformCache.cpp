#include "WaveformCache.h"

// Static member definition for constexpr array (needed for older C++ standards)
constexpr int WaveformMipmapData::kMipmapFactors[WaveformMipmapData::kNumLevels];

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
    // Generate a unique ID for this buffer
    static int64_t nextId = 1;
    int64_t id = nextId++;

    // Create legacy thumbnail (for backward compatibility)
    auto thumb = std::make_unique<juce::AudioThumbnail>(512, formatManager, thumbnailCache);
    thumb->reset(buffer.getNumChannels(), currentSampleRate, buffer.getNumSamples());
    thumb->addBlock(0, buffer, 0, buffer.getNumSamples());
    thumbnails[id] = std::move(thumb);

    // Create mipmap data for efficient multi-resolution display
    {
        juce::ScopedLock lock(mipmapLock);
        auto mipmap = std::make_unique<WaveformMipmapData>();
        mipmap->generateFromBuffer(buffer);
        mipmaps[id] = std::move(mipmap);
    }

    return id;
}

juce::AudioThumbnail& WaveformCache::getThumbnail(int64_t hash)
{
    auto it = thumbnails.find(hash);
    if (it != thumbnails.end())
        return *it->second;

    // Fallback: return a dummy (shouldn't happen if usage is correct)
    static juce::AudioThumbnail dummy(512, formatManager, thumbnailCache);
    return dummy;
}

void WaveformCache::refreshBuffer(int64_t hash, const juce::AudioBuffer<float>& buffer)
{
    // Update legacy thumbnail
    auto it = thumbnails.find(hash);
    if (it != thumbnails.end())
    {
        it->second->reset(buffer.getNumChannels(), currentSampleRate, buffer.getNumSamples());
        it->second->addBlock(0, buffer, 0, buffer.getNumSamples());
    }

    // Update mipmap data
    {
        juce::ScopedLock lock(mipmapLock);
        auto mit = mipmaps.find(hash);
        if (mit != mipmaps.end())
        {
            mit->second->generateFromBuffer(buffer);
        }
        else
        {
            // Create new mipmap if it doesn't exist
            auto mipmap = std::make_unique<WaveformMipmapData>();
            mipmap->generateFromBuffer(buffer);
            mipmaps[hash] = std::move(mipmap);
        }
    }
}

bool WaveformCache::hasMipmap(int64_t id) const
{
    juce::ScopedLock lock(mipmapLock);
    return mipmaps.find(id) != mipmaps.end();
}

const WaveformMipmapData* WaveformCache::getMipmap(int64_t id) const
{
    juce::ScopedLock lock(mipmapLock);
    auto it = mipmaps.find(id);
    if (it != mipmaps.end())
        return it->second.get();
    return nullptr;
}

bool WaveformCache::getPeaksForRange(int64_t id, int64_t startSample, int64_t endSample, int channel,
                                     float& outMin, float& outMax, float& outRms) const
{
    juce::ScopedLock lock(mipmapLock);

    auto it = mipmaps.find(id);
    if (it == mipmaps.end())
    {
        outMin = outMax = outRms = 0.0f;
        return false;
    }

    it->second->getPeaksForRange(startSample, endSample, channel, outMin, outMax, outRms);
    return true;
}

int WaveformCache::getOptimalMipmapLevel(int64_t id, double samplesPerPixel) const
{
    juce::ScopedLock lock(mipmapLock);

    auto it = mipmaps.find(id);
    if (it == mipmaps.end())
        return 0;

    return it->second->getOptimalLevelIndex(samplesPerPixel);
}

void WaveformCache::removeBuffer(int64_t id)
{
    // Remove legacy thumbnail
    thumbnails.erase(id);

    // Remove mipmap data
    {
        juce::ScopedLock lock(mipmapLock);
        mipmaps.erase(id);
    }
}

void WaveformCache::clearAll()
{
    thumbnails.clear();

    {
        juce::ScopedLock lock(mipmapLock);
        mipmaps.clear();
    }
}

size_t WaveformCache::getMemoryUsageBytes() const
{
    juce::ScopedLock lock(mipmapLock);

    size_t totalBytes = 0;

    for (const auto& pair : mipmaps)
    {
        const auto* mipmap = pair.second.get();
        if (mipmap == nullptr)
            continue;

        // Calculate memory for all mipmap levels
        for (int level = 0; level < WaveformMipmapData::kNumLevels; ++level)
        {
            const auto& levelData = mipmap->getLevel(level);
            // Each level stores: minValues, maxValues, rmsValues (3 vectors of floats)
            size_t levelSize = static_cast<size_t>(levelData.numPoints * levelData.numChannels);
            totalBytes += levelSize * sizeof(float) * 3;  // min, max, rms
        }
    }

    // Add overhead for map entries and unique_ptr
    totalBytes += mipmaps.size() * (sizeof(std::unique_ptr<WaveformMipmapData>) + sizeof(int64_t) + 64);

    return totalBytes;
}
