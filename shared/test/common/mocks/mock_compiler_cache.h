/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/compiler_cache.h"

namespace NEO {
class CompilerCacheMock : public CompilerCache {
  public:
    CompilerCacheMock() : CompilerCache(CompilerCacheConfig{}) {
    }

    bool cacheBinary(const std::string kernelFileHash, const char *pBinary, uint32_t binarySize) override {
        cacheInvoked++;
        return cacheResult;
    }

    std::unique_ptr<char[]> loadCachedBinary(const std::string kernelFileHash, size_t &cachedBinarySize) override {
        if (loadResult || numberOfLoadResult > 0) {
            numberOfLoadResult--;
            cachedBinarySize = sizeof(char);
            return std::unique_ptr<char[]>{new char[1]};
        } else
            return nullptr;
    }

    bool cacheResult = false;
    uint32_t cacheInvoked = 0u;
    bool loadResult = false;
    uint32_t numberOfLoadResult = 0u;
};
} // namespace NEO
