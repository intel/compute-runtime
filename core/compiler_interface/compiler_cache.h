/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/utilities/arrayref.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

namespace NEO {
struct HardwareInfo;

struct CompilerCacheConfig {
    bool enabled = true;
    std::string cacheFileExtension;
    std::string cacheDir;
};

class CompilerCache {
  public:
    static const std::string getCachedFileName(const HardwareInfo &hwInfo, ArrayRef<const char> input,
                                               ArrayRef<const char> options, ArrayRef<const char> internalOptions);

    CompilerCache(const CompilerCacheConfig &config);
    virtual ~CompilerCache() = default;

    CompilerCache(const CompilerCache &) = delete;
    CompilerCache(CompilerCache &&) = delete;
    CompilerCache &operator=(const CompilerCache &) = delete;
    CompilerCache &operator=(CompilerCache &&) = delete;

    MOCKABLE_VIRTUAL bool cacheBinary(const std::string kernelFileHash, const char *pBinary, uint32_t binarySize);
    MOCKABLE_VIRTUAL std::unique_ptr<char[]> loadCachedBinary(const std::string kernelFileHash, size_t &cachedBinarySize);

  protected:
    static std::mutex cacheAccessMtx;
    CompilerCacheConfig config;
};
} // namespace NEO
