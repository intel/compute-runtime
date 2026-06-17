/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include <immintrin.h>
#include <intrin.h>

namespace NEO {

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

void (*CpuInfo::cpuidexFunc)(int *, int, int) = cpuidexWindowsWrapper;
void (*CpuInfo::cpuidFunc)(int *, int) = cpuidWindowsWrapper;
void (*CpuInfo::getCpuFlagsFunc)(std::string &) = getCpuFlagsWindows;
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
