/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/utilities/io_functions.h"

#include "os_inc.h"

namespace NEO {
bool CompilerCache::evictCache() {
    return true;
}

void CompilerCache::lockConfigFileAndReadSize(const std::string &configFilePath, int &fd, size_t &directorySize) {}

bool CompilerCache::createUniqueTempFileAndWriteData(char *tmpFilePath, const char *pBinary, size_t binarySize) {
    return true;
}
bool CompilerCache::renameTempFileBinaryToProperName(const std::string &oldName, const std::string &kernelFileHash) {
    return true;
}

bool CompilerCache::cacheBinary(const std::string &kernelFileHash, const char *pBinary, size_t binarySize) {
    if (pBinary == nullptr || binarySize == 0) {
        return false;
    }
    std::string filePath = config.cacheDir + PATH_SEPARATOR + kernelFileHash + config.cacheFileExtension;
    std::lock_guard<std::mutex> lock(cacheAccessMtx);
    return 0 != writeDataToFile(filePath.c_str(), pBinary, binarySize);
}

std::unique_ptr<char[]> CompilerCache::loadCachedBinary(const std::string &kernelFileHash, size_t &cachedBinarySize) {
    std::string filePath = config.cacheDir + PATH_SEPARATOR + kernelFileHash + config.cacheFileExtension;

    std::lock_guard<std::mutex> lock(cacheAccessMtx);
    return loadDataFromFile(filePath.c_str(), cachedBinarySize);
}
} // namespace NEO