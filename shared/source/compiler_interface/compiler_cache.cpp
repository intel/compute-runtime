/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/casts.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/utilities/debug_settings_reader.h"
#include "shared/source/utilities/io_functions.h"

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
    hash.update(r_pod_cast<const char *>(&hwInfo.platform), sizeof(hwInfo.platform));
    hash.update("----", 4);
    hash.update(r_pod_cast<const char *>(std::to_string(hwInfo.featureTable.asHash()).c_str()), std::to_string(hwInfo.featureTable.asHash()).length());
    hash.update("----", 4);
    hash.update(r_pod_cast<const char *>(std::to_string(hwInfo.workaroundTable.asHash()).c_str()), std::to_string(hwInfo.workaroundTable.asHash()).length());

    auto res = hash.finish();
    std::stringstream stream;
    stream << std::setfill('0')
           << std::setw(sizeof(res) * 2)
           << std::hex
           << res;

    if (DebugManager.flags.BinaryCacheTrace.get()) {
        std::string traceFilePath = config.cacheDir + PATH_SEPARATOR + stream.str() + ".trace";
        std::string inputFilePath = config.cacheDir + PATH_SEPARATOR + stream.str() + ".input";
        std::lock_guard<std::mutex> lock(cacheAccessMtx);
        auto fp = NEO::IoFunctions::fopenPtr(traceFilePath.c_str(), "w");
        if (fp) {
            NEO::IoFunctions::fprintf(fp, "---- input ----\n");
            NEO::IoFunctions::fprintf(fp, "<%s>\n", inputFilePath.c_str());
            NEO::IoFunctions::fprintf(fp, "---- options ----\n");
            NEO::IoFunctions::fprintf(fp, "%s\n", &*options.begin());
            NEO::IoFunctions::fprintf(fp, "---- internal options ----\n");
            NEO::IoFunctions::fprintf(fp, "%s\n", &*internalOptions.begin());

            NEO::IoFunctions::fprintf(fp, "---- platform ----\n");
            NEO::IoFunctions::fprintf(fp, "  eProductFamily=%d\n", hwInfo.platform.eProductFamily);
            NEO::IoFunctions::fprintf(fp, "  ePCHProductFamily=%d\n", hwInfo.platform.ePCHProductFamily);
            NEO::IoFunctions::fprintf(fp, "  eDisplayCoreFamily=%d\n", hwInfo.platform.eDisplayCoreFamily);
            NEO::IoFunctions::fprintf(fp, "  eRenderCoreFamily=%d\n", hwInfo.platform.eRenderCoreFamily);
            NEO::IoFunctions::fprintf(fp, "  ePlatformType=%d\n", hwInfo.platform.ePlatformType);
            NEO::IoFunctions::fprintf(fp, "  usDeviceID=%d\n", hwInfo.platform.usDeviceID);
            NEO::IoFunctions::fprintf(fp, "  usRevId=%d\n", hwInfo.platform.usRevId);
            NEO::IoFunctions::fprintf(fp, "  usDeviceID_PCH=%d\n", hwInfo.platform.usDeviceID_PCH);
            NEO::IoFunctions::fprintf(fp, "  usRevId_PCH=%d\n", hwInfo.platform.usRevId_PCH);
            NEO::IoFunctions::fprintf(fp, "  eGTType=%d\n", hwInfo.platform.eGTType);

            NEO::IoFunctions::fprintf(fp, "---- feature table ----\n");
            auto featureTable = r_pod_cast<const char *>(&hwInfo.featureTable.packed);
            for (size_t idx = 0; idx < sizeof(hwInfo.featureTable.packed); idx++) {
                NEO::IoFunctions::fprintf(fp, "%02x.", (uint8_t)(featureTable[idx]));
            }
            NEO::IoFunctions::fprintf(fp, "\n");

            NEO::IoFunctions::fprintf(fp, "---- workaround table ----\n");
            auto workaroundTable = reinterpret_cast<const char *>(&hwInfo.workaroundTable);
            for (size_t idx = 0; idx < sizeof(hwInfo.workaroundTable); idx++) {
                NEO::IoFunctions::fprintf(fp, "%02x.", (uint8_t)(workaroundTable[idx]));
            }
            NEO::IoFunctions::fprintf(fp, "\n");

            NEO::IoFunctions::fclosePtr(fp);
        }
        fp = NEO::IoFunctions::fopenPtr(inputFilePath.c_str(), "w");
        if (fp) {
            NEO::IoFunctions::fwritePtr(&*input.begin(), input.size(), 1, fp);
            NEO::IoFunctions::fclosePtr(fp);
        }
    }

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
