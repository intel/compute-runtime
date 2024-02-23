/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/compiler_cache.h"

namespace NEO {
class CompilerCacheMock : public CompilerCache {
  public:
    using CompilerCache::config;

    CompilerCacheMock() : CompilerCache(CompilerCacheConfig{}) {
    }

    bool cacheBinary(const std::string &kernelFileHash, const char *pBinary, size_t binarySize) override {
        cacheInvoked++;
        hashToBinaryMap[kernelFileHash] = std::string(pBinary, binarySize);
        cacheBinaryKernelFileHashes.push_back(kernelFileHash);
        return cacheResult;
    }

    std::unique_ptr<char[]> loadCachedBinary(const std::string &kernelFileHash, size_t &cachedBinarySize) override {
        if (loadResult || numberOfLoadResult > 0) {
            numberOfLoadResult--;
            cachedBinarySize = sizeof(char);
            return std::unique_ptr<char[]>{new char[1]};
        }

        auto it = hashToBinaryMap.find(kernelFileHash);
        if (it != hashToBinaryMap.end()) {
            cachedBinarySize = it->second.size();
            auto binaryCopy = std::make_unique<char[]>(cachedBinarySize);
            std::copy(it->second.begin(), it->second.end(), binaryCopy.get());
            return binaryCopy;
        } else {
            cachedBinarySize = 0;
            return nullptr;
        }
    }

    std::vector<std::string> cacheBinaryKernelFileHashes{};
    bool cacheResult = false;
    uint32_t cacheInvoked = 0u;
    bool loadResult = false;
    uint32_t numberOfLoadResult = 0u;
    std::unordered_map<std::string, std::string> hashToBinaryMap;
};
} // namespace NEO
