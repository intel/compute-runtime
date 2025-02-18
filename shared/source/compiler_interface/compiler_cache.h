/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/os_handle.h"
#include "shared/source/utilities/arrayref.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace NEO {
struct HardwareInfo;

struct CompilerCacheConfig {
    bool enabled = false;
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

    MOCKABLE_VIRTUAL bool cacheBinary(const std::string &kernelFileHash, const char *pBinary, size_t binarySize);
    MOCKABLE_VIRTUAL std::unique_ptr<char[]> loadCachedBinary(const std::string &kernelFileHash, size_t &cachedBinarySize);

  protected:
    MOCKABLE_VIRTUAL bool evictCache(uint64_t &bytesEvicted);
    MOCKABLE_VIRTUAL bool renameTempFileBinaryToProperName(const std::string &oldName, const std::string &kernelFileHash);
    MOCKABLE_VIRTUAL bool createUniqueTempFileAndWriteData(char *tmpFilePathTemplate, const char *pBinary, size_t binarySize);
    MOCKABLE_VIRTUAL void lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &fd, size_t &directorySize);

    static std::mutex cacheAccessMtx;
    CompilerCacheConfig config;
};

static_assert(NEO::NonCopyableAndNonMovable<CompilerCache>);

} // namespace NEO
