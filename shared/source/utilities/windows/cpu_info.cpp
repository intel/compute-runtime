/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include <intrin.h>

namespace NEO {

void cpuidWindowsWrapper(int *cpuInfo, int functionId) {
    __cpuid(cpuInfo, functionId);
}

void cpuidexWindowsWrapper(int *cpuInfo, int functionId, int subfunctionId) {
    __cpuidex(cpuInfo, functionId, subfunctionId);
}

void getCpuFlagsWindows(std::string &cpuFlags) {}

void (*CpuInfo::cpuidexFunc)(int *, int, int) = cpuidexWindowsWrapper;
void (*CpuInfo::cpuidFunc)(int *, int) = cpuidWindowsWrapper;
void (*CpuInfo::getCpuFlagsFunc)(std::string &) = getCpuFlagsWindows;

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
