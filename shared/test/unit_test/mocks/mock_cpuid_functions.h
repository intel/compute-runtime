/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/cpu_info.h"

using namespace NEO;

struct MockCpuInfo : public NEO::CpuInfo {
    using CpuInfo::cpuFlags;
    using CpuInfo::features;
    using CpuInfo::featuresDetected;
    using CpuInfo::lastLevelCacheSize;
    using CpuInfo::virtualAddressSize;
};

inline constexpr size_t mockLastLevelCacheSize = static_cast<size_t>(8u * MemoryConstants::megaByte);

inline MockCpuInfo *getMockCpuInfo(const NEO::CpuInfo &cpuInfo) {
    return static_cast<MockCpuInfo *>(const_cast<NEO::CpuInfo *>(&CpuInfo::getInstance()));
}

void mockCpuidEnableAll(int *cpuInfo, int functionId);

void mockCpuidFunctionAvailableDisableAll(int *cpuInfo, int functionId);

void mockCpuidFunctionNotAvailableDisableAll(int *cpuInfo, int functionId);

void mockCpuidReport36BitVirtualAddressSize(int *cpuInfo, int functionId);

void mockCpuidEnableAllExceptAvx512FoundationBit(int *cpuInfo, int functionId);

void mockCpuidEnableAllExceptAvx2Bit(int *cpuInfo, int functionId);

size_t mockGetLastLevelCacheSize();

size_t mockGetLastLevelCacheSizeUnavailable();

uint64_t mockXgetbvEnableAll(uint32_t index);

uint64_t mockXgetbvDisableAll(uint32_t index);
