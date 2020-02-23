/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/utilities/debug_settings_reader.h"

#include "config.h"
#include "os_inc.h"

#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

namespace NEO {
std::mutex CompilerCache::cacheAccessMtx;
const std::string CompilerCache::getCachedFileName(const HardwareInfo &hwInfo, const ArrayRef<const char> input,
                                                   const ArrayRef<const char> options, const ArrayRef<const char> internalOptions) {
    Hash hash;

    hash.update("----", 4);
    hash.update(&*input.begin(), input.size());
    hash.update("----", 4);
    hash.update(&*options.begin(), options.size());
    hash.update("----", 4);
    hash.update(&*internalOptions.begin(), internalOptions.size());

    hash.update("----", 4);
    hash.update(reinterpret_cast<const char *>(&hwInfo.platform), sizeof(hwInfo.platform));
    hash.update("----", 4);
    hash.update(reinterpret_cast<const char *>(&hwInfo.featureTable), sizeof(hwInfo.featureTable));
    hash.update("----", 4);
    hash.update(reinterpret_cast<const char *>(&hwInfo.workaroundTable), sizeof(hwInfo.workaroundTable));

    auto res = hash.finish();
    std::stringstream stream;
    stream << std::setfill('0')
           << std::setw(sizeof(res) * 2)
           << std::hex
           << res;
    return stream.str();
}

CompilerCache::CompilerCache(const CompilerCacheConfig &cacheConfig)
    : config(cacheConfig){};

bool CompilerCache::cacheBinary(const std::string kernelFileHash, const char *pBinary, uint32_t binarySize) {
    if (pBinary == nullptr || binarySize == 0) {
        return false;
    }
    std::string filePath = config.cacheDir + PATH_SEPARATOR + kernelFileHash + config.cacheFileExtension;
    std::lock_guard<std::mutex> lock(cacheAccessMtx);
    return 0 != writeDataToFile(filePath.c_str(), pBinary, binarySize);
}

std::unique_ptr<char[]> CompilerCache::loadCachedBinary(const std::string kernelFileHash, size_t &cachedBinarySize) {
    std::string filePath = config.cacheDir + PATH_SEPARATOR + kernelFileHash + config.cacheFileExtension;

    std::lock_guard<std::mutex> lock(cacheAccessMtx);
    return loadDataFromFile(filePath.c_str(), cachedBinarySize);
}

} // namespace NEO
