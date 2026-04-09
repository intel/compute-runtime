/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/os_handle.h"
#include "shared/source/utilities/arrayref.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace NEO {
struct HardwareInfo;
struct ElementsStruct;

struct CacheStats {
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t version = 0;
};

struct CompilerCacheConfig {
    bool enabled = false;
    bool statsEnabled = false;
    std::string cacheFileExtension;
    std::string cacheDir;
    size_t cacheSize = 0;
};

class CompilerCache : NEO::NonCopyableAndNonMovableClass {
  public:
    CompilerCache(const CompilerCacheConfig &config);
    virtual ~CompilerCache() = default;

    const CompilerCacheConfig &getConfig() {
        return config;
    }

    const std::string getCachedFileName(const HardwareInfo &hwInfo, ArrayRef<const char> input,
                                        ArrayRef<const char> options, ArrayRef<const char> internalOptions,
                                        ArrayRef<const char> specIds, ArrayRef<const char> specValues,
                                        ArrayRef<const char> igcRevision, size_t igcLibSize, time_t igcLibMTime);
    bool showStats(bool verbose, std::string &output);

    MOCKABLE_VIRTUAL bool cacheBinary(const std::string &kernelFileHash, const char *pBinary, size_t binarySize);
    MOCKABLE_VIRTUAL std::unique_ptr<char[]> loadCachedBinary(const std::string &kernelFileHash, size_t &cachedBinarySize);

    constexpr static uint64_t cacheVersion = 1;

    enum class ReadStatsResult {
        success = 0,
        notFound = 1,
        error = -1
    };

  protected:
    MOCKABLE_VIRTUAL ReadStatsResult readStatsFromFile(const std::string &statsPath, CacheStats &cacheStats);
    MOCKABLE_VIRTUAL bool evictCache(uint64_t &bytesEvicted);
    MOCKABLE_VIRTUAL bool renameTempFileBinaryToProperName(const std::string &oldName, const std::string &kernelFileHash);
    MOCKABLE_VIRTUAL bool createUniqueTempFileAndWriteData(char *tmpFilePathTemplate, const char *pBinary, size_t binarySize);
    MOCKABLE_VIRTUAL void lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &fd, size_t &directorySize);
    MOCKABLE_VIRTUAL bool createStats(const std::string &statsPath);
    MOCKABLE_VIRTUAL bool updateStats(const std::string &statsPath, bool hit);
    MOCKABLE_VIRTUAL bool updateAllStats(const std::string &cacheFile, bool hit);
    MOCKABLE_VIRTUAL bool getFiles(const std::string &startPath, const std::function<bool(const std::string_view &)> &filter, std::vector<ElementsStruct> &foundFiles);
    MOCKABLE_VIRTUAL std::string getCachedFilePath(const std::string &cacheFile);
    MOCKABLE_VIRTUAL bool createCacheDirectories(const std::string &cacheFile);
    MOCKABLE_VIRTUAL bool compareByLastAccessTime(const ElementsStruct &a, const ElementsStruct &b);
    MOCKABLE_VIRTUAL bool getCachedFiles(std::vector<ElementsStruct> &cacheFiles);

    constexpr static int maxCacheDepth = 2;
    static std::mutex cacheAccessMtx;
    CompilerCacheConfig config;
};

static_assert(NEO::NonCopyableAndNonMovable<CompilerCache>);

} // namespace NEO
