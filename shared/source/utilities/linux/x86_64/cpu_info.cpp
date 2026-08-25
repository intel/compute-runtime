/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/linux/file_descriptor.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cpuid.h>
#include <cstdlib>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>

namespace NEO {

namespace {
constexpr uint32_t lastLevelCacheLevel = 3u;
constexpr size_t cacheAttributeBufferSize = MemoryConstants::pageSize;
constexpr size_t cpuInfoFileBufferSize = 64u * MemoryConstants::kiloByte;
constexpr std::string_view cacheIndexPrefix = "index";
constexpr std::string_view processorPrefix = "cpu";

struct CacheSizes {
    size_t largest = 0u;
    size_t lastLevel = 0u;
};

bool readSysFsFile(const std::string &path, size_t bufferSize, std::string &content) {
    FileDescriptor fd(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return false;
    }
    content.assign(bufferSize, '\0');
    const auto bytesRead = SysCalls::pread(fd, content.data(), content.size(), 0);
    if (bytesRead <= 0) {
        return false;
    }
    content.resize(bytesRead);
    return true;
}

std::optional<std::string> readCacheAttribute(const std::string &path) {
    std::string value;
    if (!readSysFsFile(path, cacheAttributeBufferSize, value)) {
        return std::nullopt;
    }
    return value.substr(0, value.find('\n'));
}

void markSharedProcessorsVisited(const std::string &sharedProcessorList, std::set<uint32_t> &visitedProcessors) {
    auto parseProcessorId = [](const std::string &value) {
        return static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
    };

    std::istringstream stream(sharedProcessorList);

    for (std::string token; std::getline(stream, token, ',');) {
        const auto rangeSeparator = token.find('-');

        const auto firstProcessorId = parseProcessorId(token.substr(0, rangeSeparator));
        const auto lastProcessorId = (rangeSeparator == std::string::npos)
                                         ? firstProcessorId
                                         : parseProcessorId(token.substr(rangeSeparator + 1));

        for (uint32_t processorId = firstProcessorId; processorId <= lastProcessorId; ++processorId) {
            visitedProcessors.insert(processorId);
        }
    }
}

size_t parseCacheSize(const std::string &value) {
    char *sizeSuffix = nullptr;
    const auto sizeParsed = std::strtoull(value.c_str(), &sizeSuffix, 10);
    if (sizeParsed == 0) {
        return 0u;
    }

    switch (*sizeSuffix) {
    case 'K':
        return static_cast<size_t>(sizeParsed) * MemoryConstants::kiloByte;
    case 'M':
        return static_cast<size_t>(sizeParsed) * MemoryConstants::megaByte;
    case 'G':
        return static_cast<size_t>(sizeParsed) * MemoryConstants::gigaByte;
    default:
        return 0u;
    }
}

CacheSizes readProcessorCacheSizes(const std::string &processorPath, std::set<uint32_t> &visitedProcessors) {
    CacheSizes cacheSizes{};

    const std::string cacheDirectoryPath = processorPath + "/cache";
    auto *cacheDirectory = SysCalls::opendir(cacheDirectoryPath.c_str());
    if (cacheDirectory == nullptr) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "Opening directory %s failed! errno: %d\n", cacheDirectoryPath.c_str(), errno);
        return cacheSizes;
    }

    while (auto *cacheEntry = SysCalls::readdir(cacheDirectory)) {
        if (!std::string_view(cacheEntry->d_name).starts_with(cacheIndexPrefix)) {
            continue;
        }

        const std::string cachePath = cacheDirectoryPath + "/" + cacheEntry->d_name;
        const auto cacheLevelString = readCacheAttribute(cachePath + "/level");
        const auto cacheSizeString = readCacheAttribute(cachePath + "/size");
        if (!cacheLevelString.has_value() || !cacheSizeString.has_value()) {
            continue;
        }

        const size_t cacheSizeValue = parseCacheSize(*cacheSizeString);
        const size_t cacheLevelValue = static_cast<uint32_t>(std::strtoul(cacheLevelString->c_str(), nullptr, 10));

        cacheSizes.largest = std::max(cacheSizes.largest, cacheSizeValue);
        if (cacheLevelValue >= lastLevelCacheLevel) {
            cacheSizes.lastLevel = std::max(cacheSizes.lastLevel, cacheSizeValue);

            if (const auto sharedProcessorList = readCacheAttribute(cachePath + "/shared_cpu_list")) {
                markSharedProcessorsVisited(*sharedProcessorList, visitedProcessors);
            }
        }
    }
    SysCalls::closedir(cacheDirectory);

    return cacheSizes;
}
} // namespace

void cpuidLinuxWrapper(int cpuInfo[4], int functionId) {
    __cpuid_count(functionId, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
}

void cpuidexLinuxWrapper(int *cpuInfo, int functionId, int subfunctionId) {
    __cpuid_count(functionId, subfunctionId, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
}

uint64_t xgetbvLinuxWrapper(uint32_t index) {
    uint32_t eax, edx;
    __asm__ __volatile__("xgetbv"
                         : "=a"(eax), "=d"(edx)
                         : "c"(index));
    return (static_cast<uint64_t>(edx) << 32) | eax;
}

void getCpuFlagsLinux(std::string &cpuFlags) {
    std::string content;
    if (!readSysFsFile(std::string(Os::sysFsProcPathPrefix) + "/cpuinfo", cpuInfoFileBufferSize, content)) {
        return;
    }
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.substr(0, 5) == "flags") {
            cpuFlags = line;
            break;
        }
    }
}

size_t getLastLevelCacheSizeLinux() {
    auto *topologyDirectory = SysCalls::opendir(Os::sysFsSystemCpuPathPrefix);
    if (topologyDirectory == nullptr) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "Opening directory %s failed! errno: %d\n", Os::sysFsSystemCpuPathPrefix, errno);
        return 0u;
    }

    CacheSizes cacheSizes{};
    std::set<uint32_t> visitedProcessors;
    while (auto *processorEntry = SysCalls::readdir(topologyDirectory)) {
        if (!std::string_view(processorEntry->d_name).starts_with(processorPrefix)) {
            continue;
        }

        const char *processorIdString = processorEntry->d_name + processorPrefix.size();
        if (!std::isdigit(static_cast<unsigned char>(*processorIdString))) {
            continue;
        }

        const auto processorId = static_cast<uint32_t>(std::strtoul(processorIdString, nullptr, 10));
        if (visitedProcessors.count(processorId) != 0) {
            continue;
        }
        visitedProcessors.insert(processorId);

        const std::string processorPath = std::string(Os::sysFsSystemCpuPathPrefix) + "/" + processorEntry->d_name;
        const auto processorCacheSizes = readProcessorCacheSizes(processorPath, visitedProcessors);
        cacheSizes.largest = std::max(cacheSizes.largest, processorCacheSizes.largest);
        cacheSizes.lastLevel = std::max(cacheSizes.lastLevel, processorCacheSizes.lastLevel);
    }
    SysCalls::closedir(topologyDirectory);

    return cacheSizes.lastLevel != 0u ? cacheSizes.lastLevel : cacheSizes.largest;
}

void (*CpuInfo::cpuidexFunc)(int *, int, int) = cpuidexLinuxWrapper;
void (*CpuInfo::cpuidFunc)(int[4], int) = cpuidLinuxWrapper;
void (*CpuInfo::getCpuFlagsFunc)(std::string &) = getCpuFlagsLinux;
size_t (*CpuInfo::getLastLevelCacheSizeFunc)() = getLastLevelCacheSizeLinux;
uint64_t (*CpuInfo::xgetbvFunc)(uint32_t) = xgetbvLinuxWrapper;

const CpuInfo CpuInfo::instance;

void CpuInfo::cpuid(
    uint32_t cpuInfo[4],
    uint32_t functionId) const {
    cpuidFunc(reinterpret_cast<int *>(cpuInfo), functionId);
}

void CpuInfo::cpuidex(
    uint32_t cpuInfo[4],
    uint32_t functionId,
    uint32_t subfunctionId) const {
    cpuidexFunc(reinterpret_cast<int *>(cpuInfo), functionId, subfunctionId);
}

} // namespace NEO
