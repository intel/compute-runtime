/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <algorithm>
#include <immintrin.h>
#include <intrin.h>
#include <vector>

namespace NEO {

namespace {
constexpr BYTE lastLevelCacheLevel = 3u;
} // namespace

void cpuidWindowsWrapper(int *cpuInfo, int functionId) {
    __cpuid(cpuInfo, functionId);
}

void cpuidexWindowsWrapper(int *cpuInfo, int functionId, int subfunctionId) {
    __cpuidex(cpuInfo, functionId, subfunctionId);
}

uint64_t xgetbvWindowsWrapper(uint32_t index) {
    return _xgetbv(index);
}

void getCpuFlagsWindows(std::string &cpuFlags) {}

size_t getLastLevelCacheSizeWindows() {
    DWORD bufferSize = 0;
    if (SysCalls::getLogicalProcessorInformationEx(RelationCache, nullptr, &bufferSize) ||
        SysCalls::getLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return 0u;
    }

    std::vector<uint8_t> buffer(bufferSize);
    if (!SysCalls::getLogicalProcessorInformationEx(RelationCache,
                                                    reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buffer.data()),
                                                    &bufferSize)) {
        return 0u;
    }

    size_t largestCacheSize = 0u;
    size_t lastLevelCacheSize = 0u;
    for (DWORD offset = 0; offset < bufferSize;) {
        auto *entry = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buffer.data() + offset);
        if (entry->Size == 0) {
            break;
        }
        if (entry->Relationship == RelationCache) {
            const auto cacheSize = static_cast<size_t>(entry->Cache.CacheSize);
            largestCacheSize = std::max(largestCacheSize, cacheSize);
            if (entry->Cache.Level >= lastLevelCacheLevel) {
                lastLevelCacheSize = std::max(lastLevelCacheSize, cacheSize);
            }
        }
        offset += entry->Size;
    }

    return lastLevelCacheSize != 0u ? lastLevelCacheSize : largestCacheSize;
}

void (*CpuInfo::cpuidexFunc)(int *, int, int) = cpuidexWindowsWrapper;
void (*CpuInfo::cpuidFunc)(int *, int) = cpuidWindowsWrapper;
void (*CpuInfo::getCpuFlagsFunc)(std::string &) = getCpuFlagsWindows;
size_t (*CpuInfo::getLastLevelCacheSizeFunc)() = getLastLevelCacheSizeWindows;
uint64_t (*CpuInfo::xgetbvFunc)(uint32_t) = xgetbvWindowsWrapper;

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
