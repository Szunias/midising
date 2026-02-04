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

//==============================================================================
// Disk Caching Implementation
//==============================================================================

bool WaveformCache::setCacheDirectory(const juce::File& directory)
{
    if (!directory.exists())
    {
        auto result = directory.createDirectory();
        if (result.failed())
        {
            DBG("WaveformCache: Failed to create cache directory: " + result.getErrorMessage());
            diskCacheEnabled = false;
            return false;
        }
    }

    if (!directory.isDirectory())
    {
        DBG("WaveformCache: Specified cache path is not a directory");
        diskCacheEnabled = false;
        return false;
    }

    cacheDirectory = directory;
    diskCacheEnabled = true;
    DBG("WaveformCache: Cache directory set to: " + directory.getFullPathName());
    return true;
}

juce::File WaveformCache::getCacheDirectory() const
{
    return cacheDirectory;
}

bool WaveformCache::isDiskCacheEnabled() const
{
    return diskCacheEnabled && cacheDirectory.isDirectory();
}

juce::String WaveformCache::generateCacheKey(const juce::File& sourceFile) const
{
    // Generate a unique cache key based on file path and modification time
    juce::String pathHash = juce::String(sourceFile.getFullPathName().hashCode64());
    juce::int64 modTime = sourceFile.getLastModificationTime().toMilliseconds();

    return pathHash + "_" + juce::String(modTime);
}

juce::File WaveformCache::getCacheFilePath(const juce::File& sourceFile) const
{
    if (!isDiskCacheEnabled())
        return {};

    juce::String cacheKey = generateCacheKey(sourceFile);
    return cacheDirectory.getChildFile(cacheKey + CACHE_EXTENSION);
}

bool WaveformCache::isCacheValid(const juce::File& cacheFile, const juce::File& sourceFile) const
{
    if (!cacheFile.existsAsFile())
        return false;

    if (!sourceFile.existsAsFile())
        return false;

    // Cache file name includes modification time hash, so if the source file changed,
    // the cache key would be different. Just verify the cache file exists and is readable.
    if (cacheFile.getSize() < 16)  // Minimum valid cache file size
        return false;

    return true;
}

bool WaveformCache::saveMipmapToDisk(int64_t id, const juce::File& sourceFile)
{
    if (!isDiskCacheEnabled())
        return false;

    if (!sourceFile.existsAsFile())
    {
        DBG("WaveformCache: Source file does not exist: " + sourceFile.getFullPathName());
        return false;
    }

    const WaveformMipmapData* mipmap = nullptr;
    {
        juce::ScopedLock lock(mipmapLock);
        auto it = mipmaps.find(id);
        if (it == mipmaps.end())
        {
            DBG("WaveformCache: Mipmap not found for id: " + juce::String(id));
            return false;
        }
        mipmap = it->second.get();
    }

    if (mipmap == nullptr)
        return false;

    juce::File cacheFile = getCacheFilePath(sourceFile);
    if (cacheFile == juce::File())
        return false;

    // Create a temporary file first, then move to final location (atomic)
    juce::File tempFile = cacheFile.getSiblingFile(cacheFile.getFileName() + ".tmp");

    {
        std::unique_ptr<juce::FileOutputStream> stream(tempFile.createOutputStream());
        if (stream == nullptr)
        {
            DBG("WaveformCache: Failed to create output stream for cache file");
            return false;
        }

        // Write source file info for validation
        stream->writeInt64(sourceFile.getSize());
        stream->writeInt64(sourceFile.getLastModificationTime().toMilliseconds());

        // Write mipmap data
        if (!mipmap->writeToBinaryStream(*stream))
        {
            DBG("WaveformCache: Failed to write mipmap data");
            tempFile.deleteFile();
            return false;
        }
    }

    // Move temp file to final location
    if (!tempFile.moveFileTo(cacheFile))
    {
        DBG("WaveformCache: Failed to finalize cache file");
        tempFile.deleteFile();
        return false;
    }

    DBG("WaveformCache: Saved cache for: " + sourceFile.getFileName());
    return true;
}

int64_t WaveformCache::loadMipmapFromDisk(const juce::File& sourceFile)
{
    if (!isDiskCacheEnabled())
        return -1;

    if (!sourceFile.existsAsFile())
        return -1;

    juce::File cacheFile = getCacheFilePath(sourceFile);
    if (!isCacheValid(cacheFile, sourceFile))
        return -1;

    std::unique_ptr<juce::FileInputStream> stream(cacheFile.createInputStream());
    if (stream == nullptr)
    {
        DBG("WaveformCache: Failed to open cache file for reading");
        return -1;
    }

    // Validate source file info
    juce::int64 cachedSize = stream->readInt64();
    juce::int64 cachedModTime = stream->readInt64();

    if (cachedSize != sourceFile.getSize() ||
        cachedModTime != sourceFile.getLastModificationTime().toMilliseconds())
    {
        DBG("WaveformCache: Cache validation failed - source file changed");
        cacheFile.deleteFile();  // Remove stale cache
        return -1;
    }

    // Load mipmap data
    auto mipmap = std::make_unique<WaveformMipmapData>();
    if (!mipmap->readFromBinaryStream(*stream))
    {
        DBG("WaveformCache: Failed to load mipmap from cache");
        cacheFile.deleteFile();  // Remove corrupt cache
        return -1;
    }

    // Generate a unique ID for this loaded mipmap
    static int64_t nextId = 1000000;  // Start from high number to avoid collision with addAudioBuffer IDs
    int64_t id = nextId++;

    // Store in memory cache
    {
        juce::ScopedLock lock(mipmapLock);
        mipmaps[id] = std::move(mipmap);
    }

    DBG("WaveformCache: Loaded cache for: " + sourceFile.getFileName() + " (id=" + juce::String(id) + ")");
    return id;
}

bool WaveformCache::hasDiskCache(const juce::File& sourceFile) const
{
    if (!isDiskCacheEnabled())
        return false;

    juce::File cacheFile = getCacheFilePath(sourceFile);
    return isCacheValid(cacheFile, sourceFile);
}

bool WaveformCache::removeDiskCache(const juce::File& sourceFile)
{
    if (!isDiskCacheEnabled())
        return false;

    juce::File cacheFile = getCacheFilePath(sourceFile);
    if (cacheFile.existsAsFile())
        return cacheFile.deleteFile();

    return false;
}

int WaveformCache::clearDiskCache()
{
    if (!isDiskCacheEnabled())
        return 0;

    int filesRemoved = 0;
    juce::Array<juce::File> cacheFiles;

    cacheDirectory.findChildFiles(cacheFiles, juce::File::findFiles, false, "*" + juce::String(CACHE_EXTENSION));

    for (const auto& file : cacheFiles)
    {
        if (file.deleteFile())
            ++filesRemoved;
    }

    DBG("WaveformCache: Cleared " + juce::String(filesRemoved) + " cache files");
    return filesRemoved;
}

int64_t WaveformCache::getDiskCacheSizeBytes() const
{
    if (!isDiskCacheEnabled())
        return 0;

    int64_t totalSize = 0;
    juce::Array<juce::File> cacheFiles;

    cacheDirectory.findChildFiles(cacheFiles, juce::File::findFiles, false, "*" + juce::String(CACHE_EXTENSION));

    for (const auto& file : cacheFiles)
    {
        totalSize += file.getSize();
    }

    return totalSize;
}
